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

#include "gc.hpp"
#include "bitops.hpp"
#include "debug.hpp"
#include "eraseblk.hpp"
#include "inode.hpp"
#include "inode_group.hpp"
#include "io_raw.hpp"
#include "log.hpp"
#include "summary.hpp"

#include <cstdlib>
#include <cstring>

namespace ffsp
{

struct ffsp_gcinfo
{
    ffsp_eraseblk_type eb_type;
    unsigned int write_time;
    unsigned int write_cnt;
};

ffsp_gcinfo *ffsp_gcinfo_init(const ffsp_fs& fs)
{
    auto* info = new ffsp_gcinfo[fs.neraseopen - 1];

    // FIXME: Too much hard coded crap in here!

    // TODO: Set write_time correctly!

    if (fs.neraseopen == 3)
    {
        info[0].eb_type = FFSP_EB_DENTRY_INODE;
        info[0].write_time = 0;
        info[0].write_cnt = 0;

        info[1].eb_type = FFSP_EB_DENTRY_CLIN;
        info[1].write_time = 0;
        info[1].write_cnt = 0;
    }
    else if (fs.neraseopen == 4)
    {
        info[0].eb_type = FFSP_EB_DENTRY_INODE;
        info[0].write_time = 0;
        info[0].write_cnt = 0;

        info[1].eb_type = FFSP_EB_FILE_INODE;
        info[1].write_time = 0;
        info[1].write_cnt = 0;

        info[2].eb_type = FFSP_EB_DENTRY_CLIN;
        info[2].write_time = 0;
        info[2].write_cnt = 0;
    }
    else if (fs.neraseopen >= 5)
    {
        info[0].eb_type = FFSP_EB_DENTRY_INODE;
        info[0].write_time = 0;
        info[0].write_cnt = 0;

        info[1].eb_type = FFSP_EB_FILE_INODE;
        info[1].write_time = 0;
        info[1].write_cnt = 0;

        info[2].eb_type = FFSP_EB_DENTRY_CLIN;
        info[2].write_time = 0;
        info[2].write_cnt = 0;

        info[3].eb_type = FFSP_EB_FILE_CLIN;
        info[3].write_time = 0;
        info[3].write_cnt = 0;
    }

    return info;
}

void ffsp_gcinfo_uninit(ffsp_gcinfo* info)
{
    delete [] info;
}

/*
 * Checks if garbage collection can be performed on the given erase block type.
 */
static bool is_eb_type_collectable(ffsp_eraseblk_type type)
{
    switch (type)
    {
        case FFSP_EB_DENTRY_INODE:
        case FFSP_EB_DENTRY_CLIN:
        case FFSP_EB_FILE_INODE:
        case FFSP_EB_FILE_CLIN:
            return true;
        default:
            return false;
    }
}

static bool is_eb_collectable(const ffsp_fs& fs, unsigned int eb_id)
{
    int cvalid = ffsp_eb_get_cvalid(fs, eb_id);
    int writeops = get_be16(fs.eb_usage[eb_id].e_writeops);

    int max_writeops = fs.erasesize / fs.clustersize;
    int max_cvalid = max_writeops;

    if (ffsp_summary_required(fs, eb_id))
    {
        /* erase block summary does not count as a valid cluster */
        max_cvalid--;
    }

    /*
     * An erase block is candidate to garbage collection if it:
     *  1) contains valid inodes or valid indirect clusters
     *  2) was closed (write operations set to max)
     *  3) does not consist of valid clusters only
     */
    if (cvalid && (writeops == max_writeops) && (cvalid < max_cvalid))
        return true;
    return false;
}

#if 0 // see ffsp_gc()
/*
 * Checks if a given inode (specified by its inode number) still contains
 * an indirect cluster pointer to the given cluster id.
 */
static bool is_clin_valid(ffsp_fs& fs, unsigned int cl_id, unsigned int ino_no)
{
    if (!ino_no || (ino_no >= fs.nino))
        return false;

    ffsp_inode* ino;
    int rc = ffsp_lookup_no(&fs, &ino, ino_no);
    if (rc < 0)
        return false;

    unsigned int flags = get_be32(ino->i_flags);
    uint64_t size = get_be64(ino->i_size);

    if (!(flags & FFSP_DATA_CLIN) || !size)
        return false;

    be32_t* ind_ptr = (be32_t*)ffsp_inode_data(ino);
    int ind_last = (size - 1) / fs.clustersize;

    for (int i = 0; i <= ind_last; i++)
        if (get_be32(ind_ptr[i]) == cl_id)
            return true;
    return false;
}

static void swap_cluster_id(ffsp_fs& fs, unsigned int ino_no,
                            unsigned int old_cl_id, unsigned int new_cl_id)
{
    ffsp_inode* ino;
    ffsp_lookup_no(&fs, &ino, ino_no);

    be32_t* ind_ptr = (be32_t*)ffsp_inode_data(ino);
    int ind_last = (get_be64(ino->i_size) - 1) / fs.clustersize;

    for (int i = 0; i <= ind_last; i++)
    {
        if (get_be32(ind_ptr[i]) != old_cl_id)
            continue;

        ind_ptr[i] = put_be32(new_cl_id);
        ffsp_mark_dirty(&fs, ino);
        break;
    }
}
#endif

/*
 * Searches inside the erase block usage map for erase blocks that contain
 * no valid data clusters and sets them to "free".
 */
static void collect_empty_eraseblks(ffsp_fs& fs)
{
    /* erase block id "0" is always reserved */
    for (unsigned int eb_id = 1; eb_id < fs.neraseblocks; eb_id++)
    {
        if (!is_eb_type_collectable(fs.eb_usage[eb_id].e_type))
            continue;

        /*
         * The erase block contains inodes or indirect pointers.
         * Set it to "free" if it does not contain any valid clusters.
         */
        if (!ffsp_eb_get_cvalid(fs, eb_id))
        {
            fs.eb_usage[eb_id].e_type = FFSP_EB_EMPTY;
            fs.eb_usage[eb_id].e_lastwrite = put_be16(0);
            fs.eb_usage[eb_id].e_writeops = put_be16(0);
        }
    }
}

static unsigned int find_empty_eraseblk(const ffsp_fs& fs)
{
    /* erase block id "0" is always reserved */
    for (unsigned int eb_id = 1; eb_id < fs.neraseblocks; eb_id++)
        if (fs.eb_usage[eb_id].e_type == FFSP_EB_EMPTY)
            return eb_id;
    return FFSP_INVALID_EB_ID;
}

/* get a pointer to the gcinfo structure of a specific erase block type */
static ffsp_gcinfo* get_gcinfo(const ffsp_fs& fs, int eb_type)
{
    for (unsigned int i = 0; i < (fs.neraseopen - 1); i++)
        if (fs.gcinfo[i].eb_type == eb_type)
            return &fs.gcinfo[i];
    return nullptr;
}

/*
 * Search to gc_info structure to find an erase block type that needs cleaning.
 * This currently only checks for the number of written erase blocks of every
 * erase block type.
 *
 * TODO: Introduce some sort of garbage collection policies to also take
 * other criteria into account (like last write time).
 */
static ffsp_eraseblk_type find_collectable_eb_type(const ffsp_fs& fs /*, GC_POLICY*/)
{
    for (unsigned int i = 0; i < (fs.neraseopen - 1); i++)
        if (fs.gcinfo[i].write_cnt >= fs.nerasewrites)
            return fs.gcinfo[i].eb_type;
    return FFSP_EB_INVALID;
}

/*
 * Finds the erase block of a given type that contains the least amount
 * of valid clusters.
 */
static unsigned int find_collectable_eraseblk(ffsp_fs& fs, ffsp_eraseblk_type eb_type)
{
    int least_cvalid = fs.erasesize / fs.clustersize;
    unsigned int least_cvalid_id = FFSP_INVALID_EB_ID;

    /* erase block id "0" is always reserved */
    for (unsigned int eb_id = 1; eb_id < fs.neraseblocks; eb_id++)
    {
        ffsp_eraseblk_type cur_type = fs.eb_usage[eb_id].e_type;
        int cur_valid = ffsp_eb_get_cvalid(fs, eb_id);

        /*
         * Consider the current erase block for cleaning if it:
         *  - has the type we are searching for
         *  - meets the "collectable" requirements
         *  - contains the least amount of valid clusters
         */
        if ((cur_type == eb_type) && is_eb_collectable(fs, eb_id) && (cur_valid < least_cvalid))
        {
            least_cvalid = cur_valid;
            least_cvalid_id = eb_id;
        }
    }
    return least_cvalid_id;
}

/*
 * Appends valid inode clusters from the source erase block to the destination
 * ease block. Updates erase block usage and inode map accordingly.
 * The function stops if either the source erase block does not contain
 * any more valid inodes, or if the destination erase block is full.
 * Returns the number of valid inode clusters inside the destination
 * erase block.
 */
static int move_inodes(ffsp_fs& fs, unsigned int src_eb_id,
                       unsigned int dest_eb_id, int dest_moved)
{
    /* TODO: Error handling missing! */

    /* How many inodes may fit into one cluster? */
    ffsp_inode** inodes = (ffsp_inode**)malloc((fs.clustersize / sizeof(ffsp_inode)) * sizeof(ffsp_inode*));
    if (!inodes)
    {
        ffsp_log().critical("malloc(valid inode pointers) failed");
        abort();
    }

    int max_cvalid = fs.erasesize / fs.clustersize;
    for (int i = 0; i < max_cvalid; i++)
    {
        uint64_t eb_off = src_eb_id * fs.erasesize;
        uint64_t cl_off = eb_off + (i * fs.clustersize);
        unsigned int cl_id = cl_off / fs.clustersize;

        int ino_cnt = ffsp_read_inode_group(fs, cl_id, inodes);
        if (!ino_cnt)
            continue;

        uint64_t read_bytes = 0;
        ffsp_read_raw(fs.fd, fs.buf, fs.clustersize, cl_off, read_bytes);
        ffsp_debug_update(fs, FFSP_DEBUG_READ_RAW, read_bytes);

        eb_off = dest_eb_id * fs.erasesize;
        cl_off = eb_off + (dest_moved * fs.clustersize);
        uint64_t written_bytes = 0;
        ffsp_write_raw(fs.fd, fs.buf, fs.clustersize, cl_off, written_bytes);
        ffsp_debug_update(fs, FFSP_DEBUG_WRITE_RAW, written_bytes);
        ffsp_debug_update(fs, FFSP_DEBUG_GC_WRITE, fs.clustersize);

        cl_id = cl_off / fs.clustersize;
        for (int j = 0; j < ino_cnt; j++)
        {
            fs.ino_map[get_be32(inodes[j]->i_no)] = put_be32(cl_id);
            ffsp_delete_inode(inodes[j]);
        }

        ffsp_eb_inc_cvalid(fs, dest_eb_id);
        ffsp_eb_dec_cvalid(fs, src_eb_id);

        /* check if the "new" erase block is full already */
        if (++dest_moved == max_cvalid)
            break;
    }
    free(inodes);
    return dest_moved;
}

/*
 * Collects one inode erase block.
 */
static void collect_inodes(ffsp_fs& fs, ffsp_eraseblk_type eb_type)
{
    /* TODO: Error handling missing! */

    int max_writeops = fs.erasesize / fs.clustersize;
    int max_cvalid = max_writeops;

    int moved_cl_cnt = 0;
    unsigned int free_eb_id = find_empty_eraseblk(fs);

    do
    {
        unsigned int eb_id = find_collectable_eraseblk(fs, eb_type);
        if (eb_id == FFSP_INVALID_EB_ID)
            break;

        moved_cl_cnt = move_inodes(fs, eb_id, free_eb_id, moved_cl_cnt);
    } while (moved_cl_cnt != max_cvalid);

    /* still "0" if no collectable erase block was found */
    if (moved_cl_cnt)
    {
        /* tell gcinfo that we wrote an eb of a specific type */
        unsigned int write_time = ffsp_gcinfo_update_writetime(fs, eb_type);

        fs.eb_usage[free_eb_id].e_type = eb_type;
        fs.eb_usage[free_eb_id].e_lastwrite = put_be16(write_time);
        fs.eb_usage[free_eb_id].e_writeops = put_be16(max_writeops);
    }
}

#if 0 // see ffsp_gc()
static int move_clin(ffsp_fs& fs, unsigned int src_eb_id,
                     unsigned int dest_eb_id, int dest_moved, be32_t* dest_eb_summary)
{
    /* TODO: Error handling missing! */

    int max_cvalid = (fs.erasesize / fs.clustersize) - 1;
    be32_t* src_eb_summary = ffsp_alloc_summary(fs);

    /* We need the summary of the source erase block to check which
     * cluster is still valid. */
    ffsp_read_summary(fs, src_eb_id, src_eb_summary);

    for (int i = 0; i < max_cvalid; i++)
    {
        unsigned int ino_no = get_be32(src_eb_summary[i]);
        unsigned int src_cl_id = src_eb_id * fs.erasesize / fs.clustersize + i;
        unsigned int dest_cl_id = dest_eb_id * fs.erasesize / fs.clustersize + dest_moved;

        if (!is_clin_valid(fs, src_cl_id, ino_no))
            continue;

        uint64_t cl_off = src_cl_id * fs.clustersize;
        uint64_t read_bytes = 0;
        ffsp_read_raw(fs.fd, fs.buf, fs.clustersize, cl_off, read_bytes);
        ffsp_debug_update(fs, FFSP_DEBUG_READ_RAW, read_bytes);

        cl_off = dest_cl_id * fs.clustersize;
        uint64_t written_bytes = 0;
        ffsp_write_raw(fs.fd, fs.buf, fs.clustersize, cl_off, written_bytes);
        ffsp_debug_update(fs, FFSP_DEBUG_WRITE_RAW, written_bytes);
        ffsp_debug_update(fs, FFSP_DEBUG_GC_WRITE, fs.clustersize);

        swap_cluster_id(fs, ino_no, src_cl_id, dest_cl_id);
        ffsp_add_summary_ref(dest_eb_summary, ino_no, dest_moved);
        ffsp_eb_inc_cvalid(fs, dest_eb_id);
        ffsp_eb_dec_cvalid(fs, src_eb_id);

        /* check if the "free" erase block is full already */
        if (++dest_moved == max_cvalid)
            break;
    }
    ffsp_delete_summary(src_eb_summary);
    return dest_moved;
}

static void collect_clin(ffsp_fs& fs, ffsp_eraseblk_type eb_type)
{
    /* TODO: Error handling missing! */

    int max_writeops = fs.erasesize / fs.clustersize;
    int max_cvalid = max_writeops - 1;

    int moved_cl_cnt = 0;
    be32_t* eb_summary = ffsp_alloc_summary(fs);
    unsigned int free_eb_id = find_empty_eraseblk(fs);

    unsigned int eb_id;
    do
    {
        eb_id = find_collectable_eraseblk(fs, eb_type);
        if (eb_id == FFSP_INVALID_EB_ID)
            break;

        moved_cl_cnt = move_clin(fs, eb_id, free_eb_id, moved_cl_cnt, eb_summary);
    } while (moved_cl_cnt != max_cvalid);

    /* still "0" if no collectable erase block was found */
    if (moved_cl_cnt)
    {
        ffsp_write_summary(fs, free_eb_id, eb_summary);
        ffsp_debug_update(fs, FFSP_DEBUG_GC_WRITE, fs.clustersize);

        /* tell gcinfo that we wrote an eb of a specific type */
        unsigned int write_time = ffsp_gcinfo_update_writetime(fs, eb_type);

        fs.eb_usage[free_eb_id].e_type = eb_type;
        fs.eb_usage[free_eb_id].e_lastwrite = put_be16(write_time);
        fs.eb_usage[free_eb_id].e_writeops = put_be16(max_writeops);
    }
    ffsp_delete_summary(eb_summary);
}
#endif

unsigned int ffsp_gcinfo_update_writetime(ffsp_fs& fs, ffsp_eraseblk_type eb_type)
{
    ffsp_gcinfo* info = get_gcinfo(fs, eb_type);
    return ++info->write_time;
}

unsigned int ffsp_gcinfo_inc_writecnt(ffsp_fs& fs, ffsp_eraseblk_type eb_type)
{
    ffsp_gcinfo* info = get_gcinfo(fs, eb_type);
    return ++info->write_cnt;
}

void ffsp_gc(ffsp_fs& fs)
{
    ffsp_log().debug("ffsp_gc()");

    if (ffsp_emtpy_eraseblk_count(fs) < fs.nerasereserve)
    {
        ffsp_log().debug("ffsp_gc(): too few free erase blocks present.");
        return;
    }

    // FIXME: ebin data is never freed!

    ffsp_eraseblk_type eb_type;
    while ((eb_type = find_collectable_eb_type(fs)) != FFSP_EB_INVALID)
    {
        if (eb_type == FFSP_EB_DENTRY_INODE ||
            eb_type == FFSP_EB_FILE_INODE )
        {
            ffsp_log().debug("ffsp_gc(): collecting eb_type {}", eb_type);
            collect_inodes(fs, eb_type);
        }

#if 0
        else if (ffsp_summary_required0(eb_type))
        {
            /* FIXME: Enable GC of cluster indirect data!
             *  The amount of valid clusters for cluster indirect
             *  erase blocks is not tracked correctly. This can
             *  lead to a deadlock inside collect_clin(). This
             *  function will be disabled until the bug is fixed.
             */
            collect_clin(fs, eb_type);
        }
#endif

        /* TODO: How to handle this correctly? */
        ffsp_gcinfo* info = get_gcinfo(fs, eb_type);
        info->write_cnt = 0;
    }
    collect_empty_eraseblks(fs);
}

} // namespace ffsp
