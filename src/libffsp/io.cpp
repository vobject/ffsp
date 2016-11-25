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
#include <sys/stat.h>

#ifdef _WIN32
#ifndef S_ISDIR
#include <io.h>
#define S_ISDIR(mode) (((mode)&S_IFMT) == S_IFDIR)
#endif
#endif

namespace ffsp
{

struct write_context
{
    const char* buf{ nullptr };
    size_t bytes_left{ 0 };
    uint64_t offset{ 0 };

    inode* ino{ nullptr };
    be32_t* ind_ptr{ nullptr };
    uint64_t old_size{ 0 };
    uint64_t new_size{ 0 };
    uint64_t old_ind_size{ 0 };
    uint64_t new_ind_size{ 0 };
    inode_data_type old_type{ inode_data_type::emb };
    inode_data_type new_type{ inode_data_type::emb };
};

static bool is_buf_empty(const char* buf, size_t size)
{
    for (size_t i = 0; i < size; ++i)
        if (buf[i])
            return false;
    return true;
}

static uint64_t max_emb_size(const fs_context& fs)
{
    return fs.clustersize - sizeof(inode);
}

static uint64_t max_clin_size(const fs_context& fs)
{
    // Number of possible pointers to indirect clusters times
    //  size of an indirect cluster.
    return (fs.clustersize - sizeof(inode)) / sizeof(be32_t) * fs.clustersize;
}

static uint64_t max_ebin_size(const fs_context& fs)
{
    // Number of possible pointers to indirect erase blocks times
    //  size of an indirect erase block.
    return (fs.clustersize - sizeof(inode)) / sizeof(be32_t) * fs.erasesize;
}

static uint32_t ind_from_offset(uint64_t offset, uint64_t ind_size)
{
    //	unsigned int cluster = cl_from_offset(fs, offset);
    //	return cluster / fs.eb_per_cl;
    return static_cast<uint32_t>(offset / ind_size);
}

static uint64_t ind_size_from_size(fs_context& fs, uint64_t size)
{
    if (size > max_clin_size(fs))
        return fs.erasesize;
    else if (size > max_emb_size(fs))
        return fs.clustersize;
    else
        return 0; // No indirect data for this file size
}

static inode_data_type data_type_from_size(fs_context& fs, uint64_t size)
{
    if (size > max_clin_size(fs))
        return inode_data_type::ebin;
    else if (size > max_emb_size(fs))
        return inode_data_type::clin;
    else
        return inode_data_type::emb;
}

static ssize_t write_ind(fs_context& fs, write_context& ctx, const char* buf, be32_t* ind_id)
{
    if (is_buf_empty(buf, ctx.new_ind_size))
    {
        // Create a file hole because the current indirect chunk consists of zeros only.
        *ind_id = put_be32(0);
        return 0;
    }
    bool for_dentry = S_ISDIR(get_be32(ctx.ino->i_mode));
    eraseblock_type eb_type = get_eraseblk_type(fs, ctx.new_type, for_dentry);

    // Search for a cluster id or an erase block id to write to.
    uint32_t eb_id;
    uint32_t cl_id;
    int rc = find_writable_cluster(fs, eb_type, eb_id, cl_id);
    if (rc < 0)
    {
        log().debug("Failed to find writable cluster or erase block");
        return -ENOSPC;
    }
    uint64_t cl_off = cl_id * ctx.new_ind_size;

    ssize_t write_rc = write_raw(*fs.io_ctx, buf, ctx.new_ind_size, cl_off);
    if (write_rc < 0)
        return write_rc;
    debug_update(fs, debug_metric::write_raw, static_cast<uint64_t>(write_rc));

    // This operation may internally finalize erase blocks by
    //  writing their erase block summary.
    commit_write_operation(fs, eb_type, eb_id, ctx.ino->i_no);
    *ind_id = put_be32(cl_id);
    return write_rc;
}

static ssize_t read_emb(fs_context& fs, inode* ino, char* buf, uint64_t nbyte, uint64_t offset)
{
    (void)fs;
    char* emb_data = (char*)inode_data(ino);
    const uint64_t i_size = get_be64(ino->i_size);

    if ((offset + nbyte) > i_size)
        nbyte = i_size - offset;

    memcpy(buf, emb_data + offset, nbyte);
    return static_cast<ssize_t>(nbyte);
}

static ssize_t read_ind(fs_context& fs, inode* ino, char* buf,
                        uint64_t nbyte, uint64_t offset, uint64_t ind_size)
{
    /* indirect cluster ids containing data */
    be32_t* ind_ptr = (be32_t*)inode_data(ino);

    /* current cluster id from the embedded data */
    uint32_t ind_index = static_cast<uint32_t>(offset / ind_size);

    /* offset inside the first cluster to read from */
    uint64_t ind_offset = offset % ind_size;

    /* never try to read more than there is available */
    nbyte = std::min(nbyte, get_be64(ino->i_size) - offset);
    uint64_t bytes_left = nbyte;

    while (bytes_left)
    {
        /* number of bytes to be read from the current indirect cluster */
        uint64_t ind_left = std::min(bytes_left, ind_size - ind_offset);

        if (!get_be32(ind_ptr[ind_index]))
        {
            /* we got a file hole */
            memset(buf, 0, ind_left);
        }
        else
        {
            uint64_t cl_off = get_be32(ind_ptr[ind_index]) * ind_size + ind_offset;

            ssize_t rc = read_raw(*fs.io_ctx, buf, ind_left, cl_off);
            if (!(rc < 0))
                debug_update(fs, debug_metric::read_raw, static_cast<uint64_t>(rc));
        }

        buf += ind_left;
        bytes_left -= ind_left;
        ind_offset = 0;
        ++ind_index;
    }
    return static_cast<ssize_t>(nbyte - bytes_left);
}

static ssize_t trunc_emb2ind(fs_context& fs, write_context& ctx, const char* ind_buf)
{
    ssize_t rc = write_ind(fs, ctx, ind_buf, &ctx.ind_ptr[0]);
    if (rc < 0)
        return rc;

    // The indirect chunk index where the writing starts
    uint32_t ind_last = ind_from_offset(ctx.new_size - 1, ctx.new_ind_size);

    // Additional indirect blocks to be reserved for the rest of
    //  "ctx.new_size". Start with index 1 because indirect block
    //  0 already contains the old embedded data.
    for (uint32_t i = 1; i < ind_last; ++i)
        ctx.ind_ptr[i] = put_be32(0);

    // clear old data type flag and set the new data type flag
    uint32_t flags = get_be32(ctx.ino->i_flags);
    flags = flags & ~static_cast<uint8_t>(ctx.old_type);
    flags = flags | static_cast<uint8_t>(ctx.new_type);
    ctx.ino->i_flags = put_be32(flags);
    return 0;
}

static ssize_t trunc_ind2emb(fs_context& fs, write_context& ctx)
{
    ssize_t rc = read_ind(fs, ctx.ino, fs.buf, ctx.new_size, 0, ctx.old_ind_size);
    if (rc < 0)
        return rc;

    uint32_t ind_last = ind_from_offset(ctx.old_size - 1, ctx.old_ind_size);

    // The file will be shrunk to fit into an inode's embedded
    //  data store. Therefore all indirect pointers will
    //  be invalidated (and later freed by the GC).
    // The appearance of the inode id does not have to be
    //  removed from the erase blocks summary because the caller
    //  would know that it is invalid when he tries to look it up.
    invalidate_ind_ptr(fs, ctx.ind_ptr, ind_last + 1, ctx.old_type);

    // Move the previously indirect data into the inode.
    memcpy(ctx.ind_ptr, fs.buf, ctx.new_size);

    // clear old data type flag and set the new data type flag
    uint32_t flags = get_be32(ctx.ino->i_flags);
    flags = flags & ~static_cast<uint8_t>(ctx.old_type);
    flags = flags | static_cast<uint8_t>(ctx.new_type);
    ctx.ino->i_flags = put_be32(flags);
    return 0;
}

static ssize_t trunc_clin2ebin(fs_context& fs, write_context& ctx)
{
    // Restore this backup on error.
    be32_t* old_ptr = (be32_t*)malloc(max_emb_size(fs));
    if (!old_ptr)
    {
        log().critical("malloc(max_emb_size) failed!");
        abort();
    }
    memcpy(old_ptr, ctx.ind_ptr, max_emb_size(fs));
    uint32_t old_ptr_cnt = ind_from_offset(ctx.old_size - 1, fs.clustersize) + 1;

    uint64_t written = 0;
    while (written < ctx.old_size)
    {
        ssize_t rc = read_ind(fs, ctx.ino, fs.buf, fs.erasesize, written, fs.clustersize);
        if (rc < 0)
        {
            free(old_ptr);
            return rc;
        }

        // We did not read full erase block. Zero out the rest.
        if (static_cast<uint64_t>(rc) < fs.erasesize)
            memset(fs.buf + rc, 0, fs.erasesize - static_cast<uint64_t>(rc));

        rc = write_ind(fs, ctx, fs.buf, &ctx.ind_ptr[written / fs.erasesize]);
        if (rc < 0)
        {
            // Reset newly allocated erase block to empty
            uint32_t ind_ptr_cnt = ind_from_offset(written - 1, fs.erasesize) + 1;
            invalidate_ind_ptr(fs, ctx.ind_ptr, ind_ptr_cnt, ctx.new_type);
            // Reset the inode's old indirect cluster pointers
            memcpy(ctx.ind_ptr, old_ptr, max_emb_size(fs));

            free(old_ptr);
            return rc;
        }

        written += static_cast<uint64_t>(rc);
    }
    invalidate_ind_ptr(fs, old_ptr, old_ptr_cnt, ctx.old_type);

    uint32_t ind_first = ind_from_offset(written - 1, fs.erasesize);
    uint32_t ind_last = ind_from_offset(ctx.new_size - 1, fs.erasesize);

    for (uint32_t i = ind_first + 1; i <= ind_last; ++i)
        ctx.ind_ptr[i] = put_be32(0);

    free(old_ptr);

    // clear old data type flag and set the new data type flag
    uint32_t flags = get_be32(ctx.ino->i_flags);
    flags = flags & ~static_cast<uint8_t>(inode_data_type::clin);
    flags = flags | static_cast<uint8_t>(inode_data_type::ebin);
    ctx.ino->i_flags = put_be32(flags);
    return 0;
}

static ssize_t trunc_ind(fs_context& fs, write_context& ctx)
{
    if (ctx.new_size < ctx.old_size)
    {
        // Handle file reduction
        uint32_t ind_first = ind_from_offset(ctx.new_size - 1, ctx.new_ind_size);
        uint32_t ind_last = ind_from_offset(ctx.old_size - 1, ctx.new_ind_size);
        uint32_t ind_cnt = ind_last - ind_first;

        invalidate_ind_ptr(fs, ctx.ind_ptr + ind_first + 1, ind_cnt, ctx.old_type);
    }
    else
    {
        // Handle file extension

        // FIXME: Check if the current cluster is not entirely full
        //  and make sure that the rest of it is zeroed and rewritten.
        // This method will probably have to be rewritten to match
        //  this requirement. Because the calling function might also
        //  have something to write into the affected cluster.

        uint32_t ind_first = ind_from_offset(ctx.old_size - 1, ctx.new_ind_size);
        uint32_t ind_last = ind_from_offset(ctx.new_size - 1, ctx.new_ind_size);

        for (uint32_t i = ind_first + 1; i <= ind_last; ++i)
            ctx.ind_ptr[i] = put_be32(0);
    }
    return 0;
}

static ssize_t trunc_clin(fs_context& fs, write_context& ctx)
{
    if (ctx.new_type == inode_data_type::ebin)
        return trunc_clin2ebin(fs, ctx);
    else if (ctx.new_type == inode_data_type::emb)
        return trunc_ind2emb(fs, ctx);
    else
        return trunc_ind(fs, ctx);
}

static ssize_t trunc_ebin(fs_context& fs, write_context& ctx)
{
    if (ctx.new_type == inode_data_type::emb)
        return trunc_ind2emb(fs, ctx);
    else
        return trunc_ind(fs, ctx);
}

static ssize_t write_emb(fs_context& fs, write_context& ctx)
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
        return static_cast<ssize_t>(ctx.bytes_left);
    }
    size_t nbyte = ctx.bytes_left;

    // Move all the inode embedded data into a temporary buffer because
    //  it will be moved into an indirect cluster or erase block later.
    memcpy(fs.buf, ctx.ind_ptr, ctx.old_size);
    memset(fs.buf + ctx.old_size, 0, ctx.new_ind_size - ctx.old_size);

    // Calculate in which indirect cluster or erase block the write request
    //  starts (ind_index) and at which offset therein (ind_offset).
    uint32_t ind_index = static_cast<uint32_t>(ctx.offset / ctx.new_ind_size);
    uint64_t ind_offset = ctx.offset % ctx.new_ind_size;

    // Check if the current write request already starts inside the
    //  embedded data offset. If so, perform it. The modified data will
    //  be moved into an indirect cluster or even erase block afterwards.
    if (ind_index == 0)
    {
        // Bytes to be written into the current indirect block.
        uint64_t ind_left = std::min(ctx.bytes_left, ctx.new_ind_size - ind_offset);
        memcpy(fs.buf + ind_offset, ctx.buf, ind_left);
        ctx.buf += ind_left;
        ctx.bytes_left -= ind_left;
        ind_offset = 0;
        ++ind_index;
    }

    // Move the data inside the inode (embedded data) into an indirect
    //  cluster or even erase block (based on how big it is going to get
    //  during the whole write request).
    ssize_t rc = trunc_emb2ind(fs, ctx, fs.buf);
    if (rc < 0)
        return rc;

    memset(fs.buf, 0, ind_offset);
    while (ctx.bytes_left)
    {
        // Bytes to be written into the current indirect block.
        uint64_t ind_left = std::min(ctx.bytes_left, ctx.new_ind_size - ind_offset);
        memcpy(fs.buf + ind_offset, ctx.buf, ind_left);

        rc = write_ind(fs, ctx, fs.buf, &ctx.ind_ptr[ind_index]);
        if (rc < 0)
            return rc;

        ++ind_index;
        ctx.buf += ind_left;
        ctx.bytes_left -= ind_left;
        ind_offset = 0;
    }
    return static_cast<ssize_t>(nbyte - ctx.bytes_left);
}

static ssize_t write_clin(fs_context& fs, write_context& ctx)
{
    size_t nbyte = ctx.bytes_left;

    // In which indirect cluster does the writing start?
    uint32_t ind_index = static_cast<uint32_t>(ctx.offset / ctx.new_ind_size);

    // The write-offset inside a cluster
    uint64_t ind_offset = ctx.offset % ctx.new_ind_size;

    while (ctx.bytes_left)
    {
        // Number of bytes to write into the current indirect cluster
        uint64_t ind_left = std::min(ctx.bytes_left, ctx.new_ind_size - ind_offset);

        // We start or finish writing inside an existing cluster.
        //  In this case do not decrease the amount of free clusters in this
        //  erase block because the already existing cluster will be
        //  invalidated and therefore be "free" again.
        uint64_t cl_off = 0;
        bool overwrite = false;
        if ((ind_left < ctx.new_ind_size) && get_be32(ctx.ind_ptr[ind_index]))
        {
            cl_off = get_be32(ctx.ind_ptr[ind_index]) * ctx.new_ind_size;
            overwrite = true;

            ssize_t rc = read_raw(*fs.io_ctx, fs.buf, ctx.new_ind_size, cl_off);
            if (rc < 0)
                return rc;
            debug_update(fs, debug_metric::read_raw, static_cast<uint64_t>(rc));
        }
        else
        {
            memset(fs.buf, 0, ind_offset);
            overwrite = false;
        }
        memcpy(fs.buf + ind_offset, ctx.buf, ind_left);

        ssize_t rc = write_ind(fs, ctx, fs.buf, &ctx.ind_ptr[ind_index]);
        if (rc < 0)
            return rc;

        if (overwrite)
        {
            // The last write operation replaced an existing
            //  cluster. Invalidate the overwritten cluster.
            eb_dec_cvalid(fs, static_cast<uint32_t>(cl_off / fs.erasesize));
        }
        ++ind_index;
        ctx.buf += ind_left;
        ctx.bytes_left -= ind_left;
        ind_offset = 0;
    }
    return static_cast<ssize_t>(nbyte - ctx.bytes_left);
}

static ssize_t write_ebin(fs_context& fs, write_context& ctx)
{
    /*
     * TODO: Split this function into smaller functions.
     */

    size_t nbyte = ctx.bytes_left;

    /* indirect erase block index that is to be written */
    uint32_t eb_index = static_cast<uint32_t>(ctx.offset / ctx.new_ind_size);

    /* write-offset inside the erase block */
    uint64_t eb_offset = ctx.offset % ctx.new_ind_size;

    while (ctx.bytes_left)
    {
        /* number of bytes left to be written into the current erase block */
        uint64_t eb_left = std::min(ctx.bytes_left, ctx.new_ind_size - eb_offset);

        /* indirect erase block id that is to be written */
        uint32_t eb_id = get_be32(ctx.ind_ptr[eb_index]);

        if ((eb_left < ctx.new_ind_size) && eb_id)
        {
            /* The erase block we want to write into already exists.
             * Do not allocate a fresh erase blocks but write
             * directly into the existing one. But do it in
             * cluster-sized chunks. */

            uint64_t cl_count = eb_left;
            uint32_t cl_index = static_cast<uint32_t>(eb_offset / fs.clustersize);
            uint64_t cl_offset = eb_offset % fs.clustersize;

            while (cl_count)
            {
                uint64_t cl_left = std::min(cl_count, fs.clustersize - cl_offset);
                uint64_t offset = eb_id * ctx.new_ind_size + cl_index * fs.clustersize;
                /* offset = ctx.offset / fs.clustersize; */

                if (cl_left < fs.clustersize)
                {
                    /* the write request is not cluster aligned.
                     * read the content of the to-be-written-into
                     * cluster to initiate a cluster aligned
                     * write later. */
                    ssize_t rc = read_raw(*fs.io_ctx, fs.buf, fs.clustersize, offset);
                    if (rc < 0)
                        return rc;
                    debug_update(fs, debug_metric::read_raw, static_cast<uint64_t>(rc));
                }
                else
                {
                    memset(fs.buf, 0, cl_offset);
                }
                memcpy(fs.buf + cl_offset, ctx.buf, cl_left);

                ssize_t rc = write_raw(*fs.io_ctx, fs.buf, fs.clustersize, offset);
                if (rc < 0)
                    return rc;
                debug_update(fs, debug_metric::write_raw, static_cast<uint64_t>(rc));

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
            ssize_t rc = write_ind(fs, ctx, fs.buf, &ctx.ind_ptr[eb_index]);
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
    return static_cast<ssize_t>(nbyte - ctx.bytes_left);
}

int truncate(fs_context& fs, inode* ino, uint64_t length)
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
    ctx.ind_ptr = (be32_t*)inode_data(ino);
    ctx.old_size = get_be64(ino->i_size);
    ctx.new_size = length;
    ctx.old_ind_size = ind_size_from_size(fs, ctx.old_size);
    ctx.new_ind_size = ind_size_from_size(fs, ctx.new_size);
    ctx.new_type = data_type_from_size(fs, ctx.new_size);

    inode_data_type data_type = static_cast<inode_data_type>(get_be32(ino->i_flags) & 0xff);

    ssize_t rc;
    if (data_type == inode_data_type::emb)
    {
        ctx.old_type = inode_data_type::emb;
        rc = write_emb(fs, ctx);
    }
    else if (data_type == inode_data_type::clin)
    {
        ctx.old_type = inode_data_type::clin;
        rc = trunc_clin(fs, ctx);
    }
    else if (data_type == inode_data_type::ebin)
    {
        ctx.old_type = inode_data_type::ebin;
        rc = trunc_ebin(fs, ctx);
    }
    else
    {
        log().error("ffsp::truncate(): unknown inode type");
        return -EPERM;
    }

    if (!(rc < 0))
    {
        ino->i_size = put_be64(length);
        update_time(ino->i_ctime);
        update_time(ino->i_mtime);
        mark_dirty(fs, ino);
        flush_inodes(fs, false);

        // The recent call to mark the current inode dirty might have
        //  triggered flushing all dirty inodes to disk. Therefore we should
        //  check if inode erase blocks need cleaning.
        gc(fs);
    }
    return static_cast<int>(rc);
}

ssize_t read(fs_context& fs, inode* ino, char* buf, uint64_t nbyte, uint64_t offset)
{
    if (nbyte == 0)
        return 0;

    if (offset >= get_be64(ino->i_size))
    {
        log().debug("ffsp::read(offset={}): too big", offset);
        return 0;
    }
    inode_data_type data_type = static_cast<inode_data_type>(get_be32(ino->i_flags) & 0xff);

    ssize_t rc;
    if (data_type == inode_data_type::emb)
        rc = read_emb(fs, ino, buf, nbyte, offset);
    else if (data_type == inode_data_type::clin)
        rc = read_ind(fs, ino, buf, nbyte, offset, fs.clustersize);
    else if (data_type == inode_data_type::ebin)
        rc = read_ind(fs, ino, buf, nbyte, offset, fs.erasesize);
    else
    {
        log().error("ffsp::read(): unknown inode type");
        return -EPERM;
    }

    //    TODO: Decide what to do with this.
    //    if (!(fs.flags & FFSP_SUPER_NOATIME))
    //        ffsp_update_atime(cl->ino);

    return rc;
}

ssize_t write(fs_context& fs, inode* ino, const char* buf, uint64_t nbyte, uint64_t offset)
{
    if (nbyte == 0)
        return 0;

    write_context ctx;
    ctx.buf = buf;
    ctx.bytes_left = nbyte;
    ctx.offset = offset;
    ctx.ino = ino;
    ctx.ind_ptr = (be32_t*)inode_data(ino);
    ctx.old_size = get_be64(ino->i_size);
    ctx.new_size = MAX(get_be64(ino->i_size), offset + nbyte);
    ctx.old_ind_size = ind_size_from_size(fs, ctx.old_size);
    ctx.new_ind_size = ind_size_from_size(fs, ctx.new_size);
    ctx.new_type = data_type_from_size(fs, ctx.new_size);

    if (ctx.new_size > max_ebin_size(fs))
        return -EFBIG;

    inode_data_type data_type = static_cast<inode_data_type>(get_be32(ino->i_flags) & 0xff);

    ssize_t rc;
    if (data_type == inode_data_type::emb)
    {
        ctx.old_type = inode_data_type::emb;
        rc = write_emb(fs, ctx);
    }
    else if (data_type == inode_data_type::clin)
    {
        ctx.old_type = inode_data_type::clin;

        if (ctx.new_type == inode_data_type::ebin)
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
    else if (data_type == inode_data_type::ebin)
    {
        ctx.old_type = inode_data_type::ebin;

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
        log().error("ffsp::write(): unknown inode type");
        return -EPERM;
    }

    if (!(rc < 0))
    {
        ino->i_size = put_be64(ctx.new_size);
        update_time(ino->i_mtime);
        mark_dirty(fs, ino);
        flush_inodes(fs, false);

        // The recent call to mark the current inode dirty might have
        //  triggered flushing all dirty inodes to disk. Therefore we should
        //  check if inode erase blocks need cleaning.
        gc(fs);
    }
    return rc;
}

} // namespace ffsp
