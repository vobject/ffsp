/*
 * Copyright (C) 2011-2012 IBM Corporation
 *
 * Author: Volker Schneider <volker.schneider@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "io.hpp"
#include "debug.hpp"
#include "eraseblk.hpp"
#include "ffsp.hpp"
#include "gc.hpp"
#include "inode.hpp"
#include "io_raw.hpp"
#include "log.hpp"
#include "utils.hpp"

#include <algorithm>

#include <cerrno>
#include <cstdlib>
#include <cstring>

struct write_context
{
    const char* buf{nullptr};
    size_t bytes_left{0};
    uint64_t offset{0};

    ffsp_inode* ino{nullptr};
    be32_t* ind_ptr{nullptr};
    uint64_t old_size{0};
    uint64_t new_size{0};
    unsigned int old_ind_size{0};
    unsigned int new_ind_size{0};
    int old_type{0};
    int new_type{0};
};

static uint32_t max_emb_size(const ffsp_fs& fs)
{
    return fs.clustersize - sizeof(ffsp_inode);
}

static uint64_t max_clin_size(const ffsp_fs& fs)
{
    // Number of possible pointers to indirect clusters times
    //  size of an indirect cluster.
    return (fs.clustersize - sizeof(ffsp_inode)) /
           sizeof(be32_t) * fs.clustersize;
}

static uint64_t max_ebin_size(const ffsp_fs& fs)
{
    // Number of possible pointers to indirect erase blocks times
    //  size of an indirect erase block.
    return (fs.clustersize - sizeof(ffsp_inode)) /
           sizeof(be32_t) * fs.erasesize;
}

static bool is_buf_empty(const char* buf, size_t size)
{
    for (size_t i = 0; i < size; ++i)
        if (buf[i])
            return false;
    return true;
}

static int ind_from_offset(uint64_t offset, uint32_t ind_size)
{
    //	unsigned int cluster = cl_from_offset(fs, offset);
    //	return cluster / fs.eb_per_cl;
    return offset / ind_size;
}

static int ind_size_from_size(ffsp_fs& fs, uint64_t size)
{
    if (size > max_clin_size(fs))
        return fs.erasesize;
    else if (size > max_emb_size(fs))
        return fs.clustersize;
    else
        return 0; // No indirect data for this file size
}

static int data_type_from_size(ffsp_fs& fs, uint32_t size)
{
    if (size > max_clin_size(fs))
        return FFSP_DATA_EBIN;
    else if (size > max_emb_size(fs))
        return FFSP_DATA_CLIN;
    else
        return FFSP_DATA_EMB;
}

static int write_ind(ffsp_fs& fs, write_context& ctx, const char* buf, be32_t* ind_id)
{
    if (is_buf_empty(buf, ctx.new_ind_size))
    {
        // Create a file hole because the current indirect chunk
        //  consists of zeros only.
        *ind_id = put_be32(0);
        return 0;
    }
    uint32_t mode = get_be32(ctx.ino->i_mode);
    ffsp_eraseblk_type eb_type = ffsp_get_eraseblk_type(fs, ctx.new_type, mode);

    // Search for a cluster id or an erase block id to write to.
    unsigned int eb_id;
    unsigned int cl_id;
    int rc = ffsp_find_writable_cluster(fs, eb_type, eb_id, cl_id);
    if (rc < 0)
    {
        ffsp_log().debug("Failed to find writable cluster or erase block");
        return rc;
    }
    uint64_t cl_off = cl_id * ctx.new_ind_size;

    uint64_t written_bytes = 0;
    if (!ffsp_write_raw(fs.fd, buf, ctx.new_ind_size, cl_off, written_bytes))
        return -errno;
    ffsp_debug_update(fs, FFSP_DEBUG_WRITE_RAW, written_bytes);

    // This operation may internally finalize erase blocks by
    //  writing their erase block summary.
    ffsp_commit_write_operation(fs, eb_type, eb_id, ctx.ino->i_no);
    *ind_id = put_be32(cl_id);
    return written_bytes;
}

static int read_emb(ffsp_fs& fs, ffsp_inode* ino,
                    char* buf, size_t size, uint64_t offset)
{
    (void)fs;
    char* emb_data = (char*)ffsp_inode_data(ino);
    const uint64_t i_size = get_be64(ino->i_size);

    if ((offset + size) > i_size)
        size = i_size - offset;

    memcpy(buf, emb_data + offset, size);
    return size;
}

static int read_ind(ffsp_fs& fs, ffsp_inode* ino, char* buf,
                    size_t count, uint64_t offset, uint32_t ind_size)
{
    /* indirect cluster ids containing data */
    be32_t* ind_ptr = (be32_t*)ffsp_inode_data(ino);

    /* current cluster id from the embedded data */
    int ind_index = offset / ind_size;

    /* offset inside the first cluster to read from */
    int ind_offset = offset % ind_size;

    /* never try to read more than there is available */
    count = MIN(count, get_be64(ino->i_size) - offset);
    uint64_t bytes_left = count;

    while (bytes_left)
    {
        /* number of bytes to be read from the current indirect cluster */
        int ind_left = MIN(bytes_left, ind_size - ind_offset);

        if (!get_be32(ind_ptr[ind_index]))
        {
            /* we got a file hole */
            memset(buf, 0, ind_left);
        }
        else
        {
            uint64_t cl_off = get_be32(ind_ptr[ind_index]) * ind_size + ind_offset;

            uint64_t read_bytes = 0;
            if (ffsp_read_raw(fs.fd, buf, ind_left, cl_off, read_bytes))
                ffsp_debug_update(fs, FFSP_DEBUG_READ_RAW, read_bytes);
        }

        buf += ind_left;
        bytes_left -= ind_left;
        ind_offset = 0;
        ++ind_index;
    }
    return count - bytes_left;
}

static int trunc_emb2ind(ffsp_fs& fs, write_context& ctx, const char* ind_buf)
{
    int rc = write_ind(fs, ctx, ind_buf, &ctx.ind_ptr[0]);
    if (rc < 0)
        return rc;

    // The indirect chunk index where the writing starts
    int ind_last = ind_from_offset(ctx.new_size - 1, ctx.new_ind_size);

    // Additional indirect blocks to be reserved for the rest of
    //  "ctx.new_size". Start with index 1 because indirect block
    //  0 already contains the old embedded data.
    for (int i = 1; i < ind_last; ++i)
        ctx.ind_ptr[i] = put_be32(0);

    ctx.ino->i_flags = put_be32(get_be32(ctx.ino->i_flags) & ~ctx.old_type);
    ctx.ino->i_flags = put_be32(get_be32(ctx.ino->i_flags) | ctx.new_type);
    return 0;
}

static int trunc_ind2emb(ffsp_fs& fs, write_context& ctx)
{
    int rc = read_ind(fs, ctx.ino, fs.buf, ctx.new_size, 0, ctx.old_ind_size);
    if (rc < 0)
        return rc;

    int ind_last = ind_from_offset(ctx.old_size - 1, ctx.old_ind_size);

    // The file will be shrunk to fit into an inode's embedded
    //  data store. Therefore all indirect pointers will
    //  be invalidated (and later freed by the GC).
    // The appearance of the inode id does not have to be
    //  removed from the erase blocks summary because the caller
    //  would know that it is invalid when he tries to look it up.
    ffsp_invalidate_ind_ptr(fs, ctx.ind_ptr, ind_last + 1, ctx.old_type);

    // Move the previously indirect data into the inode.
    memcpy(ctx.ind_ptr, fs.buf, ctx.new_size);

    ctx.ino->i_flags = put_be32(get_be32(ctx.ino->i_flags) & ~ctx.old_type);
    ctx.ino->i_flags = put_be32(get_be32(ctx.ino->i_flags) | ctx.new_type);
    return 0;
}

static int trunc_clin2ebin(ffsp_fs& fs, write_context& ctx)
{
    // Restore this backup on error.
    be32_t* old_ptr = (be32_t*)malloc(max_emb_size(fs));
    if (!old_ptr)
    {
        ffsp_log().critical("malloc(max_emb_size) failed!");
        abort();
    }
    memcpy(old_ptr, ctx.ind_ptr, max_emb_size(fs));
    int old_ptr_cnt = ind_from_offset(ctx.old_size - 1, fs.clustersize) + 1;

    uint64_t written = 0;
    while (written < ctx.old_size)
    {
        int rc = read_ind(fs, ctx.ino, fs.buf, fs.erasesize, written, fs.clustersize);
        if (rc < 0)
        {
            free(old_ptr);
            return rc;
        }

        // We did not read full erase block. Zero out the rest.
        if ((uint32_t)rc < fs.erasesize)
            memset(fs.buf + rc, 0, fs.erasesize - rc);

        rc = write_ind(fs, ctx, fs.buf, &ctx.ind_ptr[written / fs.erasesize]);
        if (rc < 0)
        {
            // Reset newly allocated erase block to empty
            int ind_ptr_cnt = ind_from_offset(written - 1, fs.erasesize) + 1;
            ffsp_invalidate_ind_ptr(fs, ctx.ind_ptr, ind_ptr_cnt, ctx.new_type);
            // Reset the inode's old indirect cluster pointers
            memcpy(ctx.ind_ptr, old_ptr, max_emb_size(fs));

            free(old_ptr);
            return rc;
        }

        written += rc;
    }
    ffsp_invalidate_ind_ptr(fs, old_ptr, old_ptr_cnt, ctx.old_type);

    int ind_first = ind_from_offset(written - 1, fs.erasesize);
    int ind_last = ind_from_offset(ctx.new_size - 1, fs.erasesize);

    for (int i = ind_first + 1; i <= ind_last; ++i)
        ctx.ind_ptr[i] = put_be32(0);

    free(old_ptr);

    ctx.ino->i_flags = put_be32(get_be32(ctx.ino->i_flags) & ~FFSP_DATA_CLIN);
    ctx.ino->i_flags = put_be32(get_be32(ctx.ino->i_flags) | FFSP_DATA_EBIN);
    return 0;
}

static int trunc_ind(ffsp_fs& fs, write_context& ctx)
{
    if (ctx.new_size < ctx.old_size)
    {
        // Handle file reduction
        int ind_first = ind_from_offset(ctx.new_size - 1, ctx.new_ind_size);
        int ind_last = ind_from_offset(ctx.old_size - 1, ctx.new_ind_size);
        int ind_cnt = ind_last - ind_first;

        ffsp_invalidate_ind_ptr(fs, ctx.ind_ptr + ind_first + 1, ind_cnt, ctx.old_type);
    }
    else
    {
        // Handle file extension

        // FIXME: Check if the current cluster is not entirely full
        //  and make sure that the rest of it is zeroed and rewritten.
        // This method will probably have to be rewritten to match
        //  this requirement. Because the calling function might also
        //  have something to write into the affected cluster.

        int ind_first = ind_from_offset(ctx.old_size - 1, ctx.new_ind_size);
        int ind_last = ind_from_offset(ctx.new_size - 1, ctx.new_ind_size);

        for (int i = ind_first + 1; i <= ind_last; ++i)
            ctx.ind_ptr[i] = put_be32(0);
    }
    return 0;
}

static int trunc_clin(ffsp_fs& fs, write_context& ctx)
{
    if (ctx.new_type == FFSP_DATA_EBIN)
        return trunc_clin2ebin(fs, ctx);
    else if (ctx.new_type == FFSP_DATA_EMB)
        return trunc_ind2emb(fs, ctx);
    else
        return trunc_ind(fs, ctx);
}

static int trunc_ebin(ffsp_fs& fs, write_context& ctx)
{
    if (ctx.new_type == FFSP_DATA_EMB)
        return trunc_ind2emb(fs, ctx);
    else
        return trunc_ind(fs, ctx);
}

static int write_emb(ffsp_fs& fs, write_context& ctx)
{
    if (!ctx.new_ind_size)
    {
        // There is no indirect data size. That means that the write
        //  request is going to take place inside the inode's
        //  embedded data only.
        char* emb_data = (char*)ctx.ind_ptr;

        // Handle file growth (truncation) inside emb_size boundary
        if (ctx.new_size > ctx.old_size)
        {
            memset(emb_data + ctx.old_size, 0,
                   ctx.new_size - ctx.old_size);
        }
        memcpy(emb_data + ctx.offset, ctx.buf, ctx.bytes_left);
        return ctx.bytes_left;
    }
    size_t count = ctx.bytes_left;

    // Move all the inode embedded data into a temporary buffer because
    //  it will be moved into an indirect cluster or erase block later.
    memcpy(fs.buf, ctx.ind_ptr, ctx.old_size);
    memset(fs.buf + ctx.old_size, 0, ctx.new_ind_size - ctx.old_size);

    // Calculate in which indirect cluster or erase block the write request
    //  starts (ind_index) and at which offset therein (ind_offset).
    int ind_index = ctx.offset / ctx.new_ind_size;
    int ind_offset = ctx.offset % ctx.new_ind_size;

    // Check if the current write request already starts inside the
    //  embedded data offset. If so, perform it. The modified data will
    //  be moved into an indirect cluster or even erase block afterwards.
    if (ind_index == 0)
    {
        // Bytes to be written into the current indirect block.
        unsigned int ind_left = MIN(ctx.bytes_left, ctx.new_ind_size - ind_offset);
        memcpy(fs.buf + ind_offset, ctx.buf, ind_left);
        ctx.buf += ind_left;
        ctx.bytes_left -= ind_left;
        ind_offset = 0;
        ++ind_index;
    }

    // Move the data inside the inode (embedded data) into an indirect
    //  cluster or even erase block (based on how big it is going to get
    //  during the whole write request).
    int rc = trunc_emb2ind(fs, ctx, fs.buf);
    if (rc < 0)
        return rc;

    memset(fs.buf, 0, ind_offset);
    while (ctx.bytes_left)
    {
        // Bytes to be written into the current indirect block.
        unsigned int ind_left = MIN(ctx.bytes_left, ctx.new_ind_size - ind_offset);
        memcpy(fs.buf + ind_offset, ctx.buf, ind_left);

        rc = write_ind(fs, ctx, fs.buf, &ctx.ind_ptr[ind_index]);
        if (rc < 0)
            return rc;

        ++ind_index;
        ctx.buf += ind_left;
        ctx.bytes_left -= ind_left;
        ind_offset = 0;
    }
    return count - ctx.bytes_left;
}

static int write_clin(ffsp_fs& fs, write_context& ctx)
{
    size_t count = ctx.bytes_left;

    // In which indirect cluster does the writing start?
    int ind_index = ctx.offset / ctx.new_ind_size;

    // The write-offset inside a cluster
    int ind_offset = ctx.offset % ctx.new_ind_size;

    while (ctx.bytes_left)
    {
        // Number of bytes to write into the current indirect cluster
        unsigned int ind_left = MIN(ctx.bytes_left, ctx.new_ind_size - ind_offset);

        // We start or finish writing inside an existing cluster.
        //  In this case do not decrease the amount of free clusters in this
        //  erase block because the already existing cluster will be
        //  invalidated and therefore be "free" again.
        uint64_t cl_off;
        bool overwrite;
        if ((ind_left < ctx.new_ind_size) && get_be32(ctx.ind_ptr[ind_index]))
        {
            cl_off = get_be32(ctx.ind_ptr[ind_index]) * ctx.new_ind_size;

            uint64_t read_bytes = 0;
            if (!ffsp_read_raw(fs.fd, fs.buf, ctx.new_ind_size, cl_off, read_bytes))
                return -errno;
            ffsp_debug_update(fs, FFSP_DEBUG_READ_RAW, read_bytes);
            overwrite = true;
        }
        else
        {
            memset(fs.buf, 0, ind_offset);
            overwrite = false;
        }
        memcpy(fs.buf + ind_offset, ctx.buf, ind_left);

        int rc = write_ind(fs, ctx, fs.buf, &ctx.ind_ptr[ind_index]);
        if (rc < 0)
            return rc;

        if (overwrite)
        {
            // The last write operation replaced an existing
            //  cluster. Invalidate the overwritten cluster.
            ffsp_eb_dec_cvalid(fs, cl_off / fs.erasesize);
        }
        ++ind_index;
        ctx.buf += ind_left;
        ctx.bytes_left -= ind_left;
        ind_offset = 0;
    }
    return count - ctx.bytes_left;
}

static int write_ebin(ffsp_fs& fs, write_context& ctx)
{
    /*
     * TODO: Split this function into smaller functions.
     */

    size_t count = ctx.bytes_left;

    /* indirect erase block index that is to be written */
    int eb_index = ctx.offset / ctx.new_ind_size;

    /* write-offset inside the erase block */
    int eb_offset = ctx.offset % ctx.new_ind_size;

    while (ctx.bytes_left)
    {
        /* number of bytes left to be written into the current erase block */
        unsigned int eb_left = MIN(ctx.bytes_left, ctx.new_ind_size - eb_offset);

        /* indirect erase block id that is to be written */
        int eb_id = get_be32(ctx.ind_ptr[eb_index]);

        if ((eb_left < ctx.new_ind_size) && eb_id)
        {
            /* The erase block we want to write into already exists.
             * Do not allocate a fresh erase blocks but write
             * directly into the existing one. But do it in
             * cluster-sized chunks. */

            unsigned int cl_count = eb_left;
            int cl_index = eb_offset / fs.clustersize;
            int cl_offset = eb_offset % fs.clustersize;

            while (cl_count)
            {
                unsigned int cl_left = MIN(cl_count, fs.clustersize - cl_offset);
                uint64_t offset = eb_id * ctx.new_ind_size + cl_index * fs.clustersize;
                /* offset = ctx.offset / fs.clustersize; */

                if (cl_left < fs.clustersize)
                {
                    /* the write request is not cluster aligned.
                     * read the content of the to-be-written-into
                     * cluster to initiate a cluster aligned
                     * write later. */
                    uint64_t read_bytes = 0;
                    if (!ffsp_read_raw(fs.fd, fs.buf, fs.clustersize, offset, read_bytes))
                        return -errno;
                    ffsp_debug_update(fs, FFSP_DEBUG_READ_RAW, read_bytes);
                }
                else
                {
                    memset(fs.buf, 0, cl_offset);
                }
                memcpy(fs.buf + cl_offset, ctx.buf, cl_left);

                uint64_t written_bytes = 0;
                if (!ffsp_write_raw(fs.fd, fs.buf, fs.clustersize, offset, written_bytes))
                    return -errno;
                ffsp_debug_update(fs, FFSP_DEBUG_WRITE_RAW, written_bytes);

                ctx.buf += cl_left;
                cl_count -= cl_left;
                cl_index++;
                cl_offset = 0;
            }
        }
        else
        {
            /* the erase block that we want to write to is not yet
             * allocated or it is allocated but will be completely
             * overwritten. */
            memset(fs.buf, 0, eb_offset);
            memcpy(fs.buf + eb_offset, ctx.buf, eb_left);
            int rc = write_ind(fs, ctx, fs.buf, &ctx.ind_ptr[eb_index]);
            if (rc < 0)
                return rc;

            /* FIXME: the current erase block will not be set to
             * "free" in case it was completely overwritten. */

            ctx.buf += eb_left;
        }
        ctx.bytes_left -= eb_left;
        ++eb_index;
        eb_offset = 0;
    }
    return count - ctx.bytes_left;
}

int ffsp_truncate(ffsp_fs& fs, ffsp_inode* ino, uint64_t length)
{
    if (length > max_ebin_size(fs))
        return -EFBIG;

    if (length == get_be64(ino->i_size))
        return 0;

    write_context ctx;
    ctx.buf = nullptr;  // no applicable for truncation
    ctx.bytes_left = 0; // no applicable for truncation
    ctx.offset = length;
    ctx.ino = ino;
    ctx.ind_ptr = (be32_t*)ffsp_inode_data(ino);
    ctx.old_size = get_be64(ino->i_size);
    ctx.new_size = length;
    ctx.old_ind_size = ind_size_from_size(fs, ctx.old_size);
    ctx.new_ind_size = ind_size_from_size(fs, ctx.new_size);
    ctx.new_type = data_type_from_size(fs, ctx.new_size);

    uint32_t i_flags = get_be32(ino->i_flags);

    int rc;
    if (i_flags & FFSP_DATA_EMB)
    {
        ctx.old_type = FFSP_DATA_EMB;
        rc = write_emb(fs, ctx);
    }
    else if (i_flags & FFSP_DATA_CLIN)
    {
        ctx.old_type = FFSP_DATA_CLIN;
        rc = trunc_clin(fs, ctx);
    }
    else if (i_flags & FFSP_DATA_EBIN)
    {
        ctx.old_type = FFSP_DATA_EBIN;
        rc = trunc_ebin(fs, ctx);
    }
    else
    {
        ffsp_log().error("ffsp_truncate(): unknown inode type");
        return -1;
    }
    if (rc < 0)
        return -EIO;

    ino->i_size = put_be64(length);
    ffsp_update_time(ino->i_ctime);
    ffsp_update_time(ino->i_mtime);
    ffsp_mark_dirty(fs, ino);
    ffsp_flush_inodes(fs, false);

    // The recent call to mark the current inode dirty might have
    //  triggered flushing all dirty inodes to disk. Therefore we should
    //  check if inode erase blocks need cleaning.
    ffsp_gc(fs);
    return rc;
}

int ffsp_read(ffsp_fs& fs, ffsp_inode* ino, char* buf, size_t count, uint64_t offset)
{
    if (count == 0)
        return 0;

    if (offset >= get_be64(ino->i_size))
    {
        ffsp_log().debug("ffsp_read(off={}): too big", offset);
        return 0;
    }
    uint32_t i_flags = get_be32(ino->i_flags);

    int rc;
    if (i_flags & FFSP_DATA_EMB)
        rc = read_emb(fs, ino, buf, count, offset);
    else if (i_flags & FFSP_DATA_CLIN)
        rc = read_ind(fs, ino, buf, count, offset, fs.clustersize);
    else if (i_flags & FFSP_DATA_EBIN)
        rc = read_ind(fs, ino, buf, count, offset, fs.erasesize);
    else
    {
        ffsp_log().error("ffsp_read(): unknown inode type");
        return -1;
    }
    if (rc < 0)
        return -EIO;

    // TODO: Decide what to do with this.
    //	if (!(fs.flags & FFSP_SUPER_NOATIME))
    //		ffsp_update_atime(cl->ino);

    return rc;
}

int ffsp_write(ffsp_fs& fs, ffsp_inode* ino, const char* buf, size_t count, uint64_t offset)
{
    if (count == 0)
        return 0;

    write_context ctx;
    ctx.buf = buf;
    ctx.bytes_left = count;
    ctx.offset = offset;
    ctx.ino = ino;
    ctx.ind_ptr = (be32_t*)ffsp_inode_data(ino);
    ctx.old_size = get_be64(ino->i_size);
    ctx.new_size = MAX(get_be64(ino->i_size), offset + count);
    ctx.old_ind_size = ind_size_from_size(fs, ctx.old_size);
    ctx.new_ind_size = ind_size_from_size(fs, ctx.new_size);
    ctx.new_type = data_type_from_size(fs, ctx.new_size);

    if (ctx.new_size > max_ebin_size(fs))
        return -EFBIG;

    uint32_t i_flags = get_be32(ino->i_flags);

    int rc;
    if (i_flags & FFSP_DATA_EMB)
    {
        ctx.old_type = FFSP_DATA_EMB;
        rc = write_emb(fs, ctx);
    }
    else if (i_flags & FFSP_DATA_CLIN)
    {
        ctx.old_type = FFSP_DATA_CLIN;

        if (ctx.new_type == FFSP_DATA_EBIN)
        {
            // Handle file type growth while writing.

            // TODO: This is not optimal -
            //  clusters might be written twice
            rc = trunc_clin2ebin(fs, ctx);
            if (rc < 0)
                return rc;
            rc = write_ebin(fs, ctx);
        }
        else
        {
            // The file type will not increase.
            if (ctx.new_size > ctx.old_size)
                trunc_clin(fs, ctx);
            rc = write_clin(fs, ctx);
        }
    }
    else if (i_flags & FFSP_DATA_EBIN)
    {
        ctx.old_type = FFSP_DATA_EBIN;

        if (ctx.new_size > ctx.old_size)
        {
            rc = trunc_ind(fs, ctx);
            if (rc < 0)
                return rc;
        }
        rc = write_ebin(fs, ctx);
    }
    else
    {
        ffsp_log().error("ffsp_write(): unknown inode type");
        return -1;
    }
    if (rc < 0)
        return -EIO;

    ino->i_size = put_be64(ctx.new_size);
    ffsp_update_time(ino->i_mtime);
    ffsp_mark_dirty(fs, ino);
    ffsp_flush_inodes(fs, false);

    // The recent call to mark the current inode dirty might have
    //  triggered flushing all dirty inodes to disk. Therefore we should
    //  check if inode erase blocks need cleaning.
    ffsp_gc(fs);
    return rc;
}
