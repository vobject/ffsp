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

/*
 * Checks if garbage collection can be performed on the given erase block type.
 */
static bool is_eb_type_collectable(int type)
{
    switch (type)
    {
        case FFSP_EB_DENTRY_INODE:
        case FFSP_EB_DENTRY_CLIN:
        case FFSP_EB_FILE_INODE:
        case FFSP_EB_FILE_CLIN:
            return true;
    }
    return false;
}

static bool is_eb_collectable(const struct ffsp* fs, unsigned int eb_id)
{
    int type;
    int cvalid;
    int writeops;
    int max_cvalid;
    int max_writeops;

    type = fs->eb_usage[eb_id].e_type;
    cvalid = ffsp_eb_get_cvalid(fs, eb_id);
    writeops = get_be16(fs->eb_usage[eb_id].e_writeops);

    max_writeops = fs->erasesize / fs->clustersize;
    max_cvalid = max_writeops;

    if (ffsp_has_summary(type))
        /* erase block summary does not count as a valid cluster */
        max_cvalid--;

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

/*
 * Checks if a given inode (specified by its inode number) still contains
 * an indirect cluster pointer to the given cluster id.
 */
static bool is_clin_valid(struct ffsp* fs, unsigned int cl_id,
                          unsigned int ino_no)
{
    int rc;
    struct ffsp_inode* ino;
    unsigned int flags;
    uint64_t size;
    be32_t* ind_ptr;
    int ind_last;

    if (!ino_no || (ino_no >= fs->nino))
        return false;

    rc = ffsp_lookup_no(fs, &ino, ino_no);
    if (rc < 0)
        return false;

    flags = get_be32(ino->i_flags);
    size = get_be64(ino->i_size);

    if (!(flags & FFSP_DATA_CLIN) || !size)
        return false;

    ind_ptr = (be32_t*)ffsp_inode_data(ino);
    ind_last = (size - 1) / fs->clustersize;

    for (int i = 0; i <= ind_last; i++)
        if (get_be32(ind_ptr[i]) == cl_id)
            return true;
    return false;
}

static void swap_cluster_id(struct ffsp* fs, unsigned int ino_no,
                            unsigned int old_cl_id, unsigned int new_cl_id)
{
    struct ffsp_inode* ino;
    be32_t* ind_ptr;
    int ind_last;

    ffsp_lookup_no(fs, &ino, ino_no);

    ind_ptr = (be32_t*)ffsp_inode_data(ino);
    ind_last = (get_be64(ino->i_size) - 1) / fs->clustersize;

    for (int i = 0; i <= ind_last; i++)
    {
        if (get_be32(ind_ptr[i]) != old_cl_id)
            continue;

        ind_ptr[i] = put_be32(new_cl_id);
        ffsp_mark_dirty(fs, ino);
        break;
    }
}

/*
 * Searches inside the erase block usage map for erase blocks that contain
 * no valid data clusters and sets them to "free".
 */
static void collect_empty_eraseblks(struct ffsp* fs)
{
    /* erase block id "0" is always reserved */
    for (unsigned int eb_id = 1; eb_id < fs->neraseblocks; eb_id++)
    {
        if (!is_eb_type_collectable(fs->eb_usage[eb_id].e_type))
            continue;

        /*
		 * The erase block contains inodes or indirect pointers.
		 * Set it to "free" if it does not contain any valid clusters.
		 */
        if (!ffsp_eb_get_cvalid(fs, eb_id))
        {
            fs->eb_usage[eb_id].e_type = FFSP_EB_EMPTY;
            fs->eb_usage[eb_id].e_lastwrite = put_be16(0);
            fs->eb_usage[eb_id].e_writeops = put_be16(0);
        }
    }
}

static unsigned int find_empty_eraseblk(const struct ffsp* fs)
{
    /* erase block id "0" is always reserved */
    for (unsigned int eb_id = 1; eb_id < fs->neraseblocks; eb_id++)
        if (fs->eb_usage[eb_id].e_type == FFSP_EB_EMPTY)
            return eb_id;
    return FFSP_INVALID_EB_ID;
}

/* get a pointer to the gcinfo structure of a specific erase block type */
static struct ffsp_gcinfo* get_gcinfo(const struct ffsp* fs, int eb_type)
{
    for (unsigned int i = 0; i < (fs->neraseopen - 1); i++)
        if (fs->gcinfo[i].eb_type == eb_type)
            return &fs->gcinfo[i];
    return NULL;
}

/*
 * Search to gc_info structure to find an erase block type that needs cleaning.
 * This currently only checks for the number of written erase blocks of every
 * erase block type.
 *
 * TODO: Introduce some sort of garbage collection policies to also take
 * other criteria into account (like last write time).
 */
static int find_collectable_eb_type(const struct ffsp* fs /*, GC_POLICY*/)
{
    for (unsigned int i = 0; i < (fs->neraseopen - 1); i++)
        if (fs->gcinfo[i].write_cnt >= fs->nerasewrites)
            return fs->gcinfo[i].eb_type;
    return -1;
}

/*
 * Finds the erase block of a given type that contains the least amount
 * of valid clusters.
 */
static unsigned int find_collectable_eraseblk(struct ffsp* fs, int eb_type)
{
    int least_cvalid;
    unsigned int least_cvalid_id;
    int cur_type;
    int cur_valid;

    least_cvalid = fs->erasesize / fs->clustersize;
    least_cvalid_id = FFSP_INVALID_EB_ID;

    /* erase block id "0" is always reserved */
    for (unsigned int eb_id = 1; eb_id < fs->neraseblocks; eb_id++)
    {
        cur_type = fs->eb_usage[eb_id].e_type;
        cur_valid = ffsp_eb_get_cvalid(fs, eb_id);

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
static int move_inodes(struct ffsp* fs, unsigned int src_eb_id,
                       unsigned int dest_eb_id, int dest_moved)
{
    /* TODO: Error handling missing! */

    int max_cvalid;
    uint64_t eb_off;
    uint64_t cl_off;
    struct ffsp_inode** inodes;
    int ino_cnt;
    unsigned int cl_id;

    max_cvalid = fs->erasesize / fs->clustersize;

    /* How many inodes may fit into one cluster? */
    inodes = (struct ffsp_inode**)malloc((fs->clustersize / sizeof(struct ffsp_inode)) *
                                         sizeof(struct ffsp_inode*));
    if (!inodes)
    {
        ffsp_log().critical("malloc(valid inode pointers) failed");
        abort();
    }

    for (int i = 0; i < max_cvalid; i++)
    {
        eb_off = src_eb_id * fs->erasesize;
        cl_off = eb_off + (i * fs->clustersize);
        cl_id = cl_off / fs->clustersize;

        ino_cnt = ffsp_read_inode_group(fs, cl_id, inodes);
        if (!ino_cnt)
            continue;

        ffsp_read_raw(fs->fd, fs->buf, fs->clustersize, cl_off);
        eb_off = dest_eb_id * fs->erasesize;
        cl_off = eb_off + (dest_moved * fs->clustersize);
        ffsp_write_raw(fs->fd, fs->buf, fs->clustersize, cl_off);
        ffsp_debug_update(FFSP_DEBUG_GC_WRITE, fs->clustersize);

        cl_id = cl_off / fs->clustersize;
        for (int j = 0; j < ino_cnt; j++)
        {
            fs->ino_map[get_be32(inodes[j]->i_no)] = put_be32(cl_id);
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
static void collect_inodes(struct ffsp* fs, int eb_type)
{
    /* TODO: Error handling missing! */

    int max_writeops;
    int max_cvalid;
    int moved_cl_cnt;
    unsigned int free_eb_id;
    unsigned int eb_id;
    unsigned int write_time;

    max_writeops = fs->erasesize / fs->clustersize;
    max_cvalid = max_writeops;

    moved_cl_cnt = 0;
    free_eb_id = find_empty_eraseblk(fs);

    do
    {
        eb_id = find_collectable_eraseblk(fs, eb_type);
        if (eb_id == FFSP_INVALID_EB_ID)
            break;

        moved_cl_cnt = move_inodes(fs, eb_id, free_eb_id, moved_cl_cnt);
    } while (moved_cl_cnt != max_cvalid);

    /* still "0" if no collectable erase block was found */
    if (moved_cl_cnt)
    {
        /* tell gcinfo that we wrote an eb of a specific type */
        write_time = ffsp_gcinfo_update_writetime(fs, eb_type);

        fs->eb_usage[free_eb_id].e_type = eb_type;
        fs->eb_usage[free_eb_id].e_lastwrite = put_be16(write_time);
        fs->eb_usage[free_eb_id].e_writeops = put_be16(max_writeops);
    }
}

static int move_clin(struct ffsp* fs, unsigned int src_eb_id,
                     unsigned int dest_eb_id, int dest_moved, be32_t* dest_eb_summary)
{
    /* TODO: Error handling missing! */

    int max_cvalid;
    be32_t* src_eb_summary;
    uint64_t cl_off;
    unsigned int ino_no;
    unsigned int src_cl_id;
    unsigned int dest_cl_id;

    max_cvalid = (fs->erasesize / fs->clustersize) - 1;
    src_eb_summary = ffsp_alloc_summary(fs);

    /* We need the summary of the source erase block to check which
	 * cluster is still valid. */
    ffsp_read_summary(fs, src_eb_id, src_eb_summary);

    for (int i = 0; i < max_cvalid; i++)
    {
        ino_no = get_be32(src_eb_summary[i]);
        src_cl_id = src_eb_id * fs->erasesize / fs->clustersize + i;

        if (!is_clin_valid(fs, src_cl_id, ino_no))
            continue;

        cl_off = src_cl_id * fs->clustersize;
        ffsp_read_raw(fs->fd, fs->buf, fs->clustersize, cl_off);

        dest_cl_id = dest_eb_id * fs->erasesize / fs->clustersize + dest_moved;
        cl_off = dest_cl_id * fs->clustersize;
        ffsp_write_raw(fs->fd, fs->buf, fs->clustersize, cl_off);
        ffsp_debug_update(FFSP_DEBUG_GC_WRITE, fs->clustersize);

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

static void collect_clin(struct ffsp* fs, int eb_type)
{
    /* TODO: Error handling missing! */

    int max_writeops;
    int max_cvalid;
    int moved_cl_cnt;
    be32_t* eb_summary;
    unsigned int free_eb_id;
    unsigned int eb_id;
    unsigned int write_time;

    max_writeops = fs->erasesize / fs->clustersize;
    max_cvalid = max_writeops - 1;

    moved_cl_cnt = 0;
    eb_summary = ffsp_alloc_summary(fs);
    free_eb_id = find_empty_eraseblk(fs);

    do
    {
        eb_id = find_collectable_eraseblk(fs, eb_type);
        if (eb_id == FFSP_INVALID_EB_ID)
            break;

        moved_cl_cnt = move_clin(fs, eb_id, free_eb_id, moved_cl_cnt,
                                 eb_summary);
    } while (moved_cl_cnt != max_cvalid);

    /* still "0" if no collectable erase block was found */
    if (moved_cl_cnt)
    {
        ffsp_write_summary(fs, free_eb_id, eb_summary);
        ffsp_debug_update(FFSP_DEBUG_GC_WRITE, fs->clustersize);

        /* tell gcinfo that we wrote an eb of a specific type */
        write_time = ffsp_gcinfo_update_writetime(fs, eb_type);

        fs->eb_usage[free_eb_id].e_type = eb_type;
        fs->eb_usage[free_eb_id].e_lastwrite = put_be16(write_time);
        fs->eb_usage[free_eb_id].e_writeops = put_be16(max_writeops);
    }
    ffsp_delete_summary(eb_summary);
}

unsigned int ffsp_gcinfo_update_writetime(struct ffsp* fs, int eb_type)
{
    struct ffsp_gcinfo* gcinfo = get_gcinfo(fs, eb_type);
    return ++gcinfo->write_time;
}

unsigned int ffsp_gcinfo_inc_writecnt(struct ffsp* fs, int eb_type)
{
    struct ffsp_gcinfo* gcinfo = get_gcinfo(fs, eb_type);
    return ++gcinfo->write_cnt;
}

void ffsp_gc(struct ffsp* fs)
{
    int eb_type;
    struct ffsp_gcinfo* gcinfo;

    ffsp_log().debug("ffsp_gc()");

    if (ffsp_emtpy_eraseblk_count(fs) < fs->nerasereserve)
    {
        ffsp_log().debug("ffsp_gc(): too few free erase blocks present.");
        return;
    }

    // FIXME: ebin data is never freed!

    while ((eb_type = find_collectable_eb_type(fs)) != -1)
    {
        ffsp_log().debug("ffsp_gc(): collecting eb_type {}", eb_type);

        if (ffsp_has_summary(eb_type))
            /* FIXME: Enable GC of cluster indirect data!
			 *  The amount of valid clusters for cluster indirect
			 *  erase blocks is not tracked correctly. This can
			 *  lead to a deadlock inside collect_clin(). This
			 *  function will be disabled until the bug is fixed.
			 */
            //			collect_clin(fs, eb_type);
            ;
        else
            collect_inodes(fs, eb_type);

        /* TODO: How to handle this correctly? */
        gcinfo = get_gcinfo(fs, eb_type);
        gcinfo->write_cnt = 0;
    }
    collect_empty_eraseblks(fs);
}
