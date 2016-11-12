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

#include "eraseblk.hpp"
#include "debug.hpp"
#include "gc.hpp"
#include "io_raw.hpp"
#include "log.hpp"
#include "summary.hpp"

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

int eb_get_cvalid(const fs_context& fs, uint32_t eb_id)
{
    return get_be16(fs.eb_usage[eb_id].e_cvalid);
}

void eb_inc_cvalid(fs_context& fs, uint32_t eb_id)
{
    inc_be16(fs.eb_usage[eb_id].e_cvalid);
}

void eb_dec_cvalid(fs_context& fs, uint32_t eb_id)
{
    dec_be16(fs.eb_usage[eb_id].e_cvalid);
}

uint32_t emtpy_eraseblk_count(const fs_context& fs)
{
    unsigned int cnt = 0;

    // Erase block id "0" is always reserved.
    for (unsigned int eb_id = 1; eb_id < fs.neraseblocks; ++eb_id)
        if (fs.eb_usage[eb_id].e_type == FFSP_EB_EMPTY)
            ++cnt;
    return cnt;
}

static uint32_t find_empty_eraseblk(const fs_context& fs)
{
    if (emtpy_eraseblk_count(fs) <= fs.nerasereserve)
        return FFSP_INVALID_EB_ID;

    // Erase block id "0" is always reserved.
    for (unsigned int eb_id = 1; eb_id < fs.neraseblocks; ++eb_id)
    {
        if (fs.eb_usage[eb_id].e_type == FFSP_EB_EMPTY)
        {
            fs.eb_usage[eb_id].e_lastwrite = put_be16(0);
            fs.eb_usage[eb_id].e_cvalid = put_be16(0);
            fs.eb_usage[eb_id].e_writeops = put_be16(0);
            return eb_id;
        }
    }
    return FFSP_INVALID_EB_ID;
}

eraseblock_type get_eraseblk_type(const fs_context& fs, int data_type, uint32_t mode)
{
    // TODO: Check if it is ok to put all other types (blk, pipe, etc)
    //  apart from dentry into the same "file" erase blocks.

    if (fs.neraseopen == 3)
    {
        // 1. EB: super block, erase block usage, inode map
        // 2. EB: inodes (dentry and file)
        // 3. EB: cluster indirect data (dentry and file)

        if (data_type == FFSP_DATA_EMB)
            return FFSP_EB_DENTRY_INODE;
        else if (data_type == FFSP_DATA_CLIN)
            return FFSP_EB_DENTRY_CLIN;
    }
    else if (fs.neraseopen == 4)
    {
        // 1. EB: super block, erase block usage, inode map
        // 2. EB: dentry inodes
        // 3. EB: file inodes
        // 4. EB: cluster indirect data (dentry and file)

        if (data_type == FFSP_DATA_EMB && S_ISDIR(mode))
            return FFSP_EB_DENTRY_INODE;
        else if (data_type == FFSP_DATA_EMB && !S_ISDIR(mode))
            return FFSP_EB_FILE_INODE;
        else if (data_type == FFSP_DATA_CLIN)
            return FFSP_EB_DENTRY_CLIN;
    }
    else if (fs.neraseopen >= 5)
    {
        // 1. EB: super block, erase block usage, inode map
        // 2. EB: dentry inodes
        // 3. EB: file inodes
        // 4. EB: cluster indirect dentry data
        // 5. EB: cluster indirect file data

        if (S_ISDIR(mode))
        {
            if (data_type == FFSP_DATA_EMB)
                return FFSP_EB_DENTRY_INODE;
            else if (data_type == FFSP_DATA_CLIN)
                return FFSP_EB_DENTRY_CLIN;
        }
        else
        {
            if (data_type == FFSP_DATA_EMB)
                return FFSP_EB_FILE_INODE;
            else if (data_type == FFSP_DATA_CLIN)
                return FFSP_EB_FILE_CLIN;
        }
    }
    return FFSP_EB_EBIN;
}

int find_writable_cluster(fs_context& fs, eraseblock_type eb_type,
                          uint32_t& eb_id, uint32_t& cl_id)
{
    if (eb_type == FFSP_EB_EBIN)
    {
        cl_id = eb_id = find_empty_eraseblk(fs);
        return (eb_id == FFSP_INVALID_EB_ID) ? -1 : 0;
    }

    unsigned int max_writeops = fs.erasesize / fs.clustersize;

    // Try to find an open erase block that matches the type we are
    //  searching for.
    for (unsigned int eb = 1; eb < fs.neraseblocks; ++eb)
    {
        if (fs.eb_usage[eb].e_type != eb_type)
            continue;

        // We found the right erase block type.
        // But it has to be open to be usable.
        unsigned int cur_writeops = get_be16(fs.eb_usage[eb].e_writeops);
        if (cur_writeops < max_writeops)
        {
            // This erase block is exactly what we were
            //  looking for. It matches the type and
            //  it is not full yet.
            eb_id = eb;
            // cl_id is the cluster id of the erase block
            //  plus the amount of already written clusters
            cl_id = eb * fs.erasesize / fs.clustersize + cur_writeops;
            return 0;
        }
    }

    // We were unable to find the right open erase block.
    // Open a new erase block and return it for writing.
    eb_id = find_empty_eraseblk(fs);
    if (eb_id == FFSP_INVALID_EB_ID)
        return -1;

    // The beginning of a new erase block is a valid cluster id, too.
    cl_id = eb_id * fs.erasesize / fs.clustersize;
    return 0;
}

void commit_write_operation(fs_context& fs, eraseblock_type eb_type,
                            uint32_t eb_id, be32_t ino_no)
{
    /* TODO: Error handling missing! */

    // This function cannot trigger garbage collection by itself because
    //  calling functions may have to perform further operations to bring
    //  the file system into a consistent state. One (and currently the
    //  only) example is:
    //   This function does not know whether a cluster was just written
    //   that replaces (invalidates) a cluster in the same or another
    //   erase block. In this case the calling function has to decrement
    //   the old erase blocks valid cluster count.
    // To help triggering gc the function will increment fs.eb_written
    //  every time an erase block was finalized.

    if (eb_type == FFSP_EB_EBIN)
    {
        // Erase block indirect data is easy to handle.
        // It can never be "open" because it is always completely
        //  written by a single write operation.
        fs.eb_usage[eb_id].e_type = eb_type;
        return;
    }

    /* tell gcinfo that we wrote an erase block of a specific type */
    unsigned int write_time = gcinfo_update_writetime(fs, eb_type);

    // Update the meta data of the erase block that was written to.
    fs.eb_usage[eb_id].e_type = eb_type;
    fs.eb_usage[eb_id].e_lastwrite = put_be16(write_time);
    eb_inc_cvalid(fs, eb_id);
    inc_be16(fs.eb_usage[eb_id].e_writeops);

    int max_writeops = fs.erasesize / fs.clustersize;
    uint16_t writeops = get_be16(fs.eb_usage[eb_id].e_writeops);

    if (!summary_required(fs, eb_id))
    {
        if (writeops == max_writeops)
        {
            // An erase block without summary is implicitly
            //  finalized when its maximum write operations count
            //  is reached.
            gcinfo_inc_writecnt(fs, eb_type);
        }
        return;
    }

    // The erase block still needs a summary if it was just opened.
    summary* eb_summary;
    if (writeops == 1)
    {
        // Create a new erase block summary buffer for the newly
        //  opened erase block and add it to the summary list.
        eb_summary = summary_open(*fs.summary_cache, eb_type);
    }
    else
    {
        // The summary for this erase block should already exist.
        eb_summary = summary_get(*fs.summary_cache, eb_type);
    }
    // The last cluster of a cluster indirect erase block contains the
    //  inode ids of all inodes that have data inside this erase block.
    summary_add_ref(eb_summary, writeops - 1, get_be32(ino_no));

    // The summary information of every open erase block is cached.
    //  It will be written to the end of the erase block when this is full.
    if (writeops == (max_writeops - 1))
    {
        // The last write operation filled the erase block.
        // Write its summary to finalize it.
        summary_write(fs, eb_summary, eb_id);
        summary_close(*fs.summary_cache, eb_summary);

        /* we just performed another write operation;
         * tell gcinfo and update the erase block's usage data */
        write_time = gcinfo_update_writetime(fs, eb_type);

        fs.eb_usage[eb_id].e_lastwrite = put_be16(write_time);
        inc_be16(fs.eb_usage[eb_id].e_writeops);
        gcinfo_inc_writecnt(fs, eb_type);
    }
}

void close_eraseblks(fs_context& fs)
{
    /* TODO: Error handling missing! */

    for (unsigned int eb_id = 1; eb_id < fs.neraseblocks; ++eb_id)
    {
        if (fs.eb_usage[eb_id].e_type & FFSP_EB_EBIN)
            continue; /* can never be "open" */
        if (fs.eb_usage[eb_id].e_type & FFSP_EB_EMPTY)
            continue; /* can never be "open" */

        eraseblock_type eb_type = fs.eb_usage[eb_id].e_type;
        unsigned int writeops = get_be16(fs.eb_usage[eb_id].e_writeops);
        unsigned int max_writeops = fs.erasesize / fs.clustersize;

        if (writeops == max_writeops)
            continue; /* erase block is already finalized/closed */

        fs.eb_usage[eb_id].e_writeops = put_be16(max_writeops);

        if (!summary_required(fs, eb_id))
            continue;

        summary* eb_summary = summary_get(*fs.summary_cache, eb_type);

        summary_write(fs, eb_summary, eb_id);
        summary_close(*fs.summary_cache, eb_summary);

        /* tell gcinfo an erase block of a specific type was written */
        unsigned int write_time = gcinfo_update_writetime(fs, eb_type);
        fs.eb_usage[eb_id].e_lastwrite = put_be16(write_time);
    }
}

ssize_t write_meta_data(fs_context& fs)
{
    /*
     * Copy erase block usage info and the content of the inode map
     * into one continuous buffer so that we can initiate one
     * cluster-aligned write request into the first erase block.
     */

    size_t eb_usage_size = fs.neraseblocks * sizeof(eraseblock);
    memcpy(fs.buf, fs.eb_usage, eb_usage_size);

    size_t ino_map_size = fs.nino * sizeof(uint32_t);
    memcpy(fs.buf + eb_usage_size, fs.ino_map, ino_map_size);

    size_t meta_data_size = eb_usage_size + ino_map_size;
    uint64_t offset = fs.clustersize;

    ssize_t rc = write_raw(*fs.io_ctx, fs.buf, meta_data_size, offset);
    if (rc < 0)
    {
        log().error("writing meta data to first erase block failed");
        return rc;
    }
    debug_update(fs, debug_metric::write_raw, static_cast<uint64_t>(rc));
    return rc;
}

} // namespace ffsp
