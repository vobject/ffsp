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

#include "inode.hpp"
#include "eraseblk.hpp"
#include "debug.hpp"
#include "ffsp.hpp"
#include "inode_cache.hpp"
#include "inode_group.hpp"
#include "io_raw.hpp"
#include "log.hpp"

#include <cstdlib>
#include <cstring>

/* copy grouped elements into the cluster buffer. */
static void group_inodes(const ffsp_fs& fs, ffsp_inode** group,
                         int group_elem_cnt, char* cl_buf)
{
    unsigned int cl_filling = 0;
    for (int i = 0; i < group_elem_cnt; i++)
    {
        unsigned int ino_size = ffsp_get_inode_size(fs, group[i]);
        memcpy(cl_buf + cl_filling, group[i], ino_size);
        cl_filling += ino_size;
    }
    memset(cl_buf + cl_filling, 0, fs.clustersize - cl_filling);
}

/*
 * Search for inodes that would fit into one cluster and save a pointer to
 * those in 'group'. The grouped pointer are invalidated in 'inodes'.
 * Return the number of inodes grouped together.
 */
static int get_inode_group(const ffsp_fs& fs, ffsp_inode** inodes,
                           unsigned int ino_cnt, ffsp_inode** group)
{
    unsigned int group_size = 0;
    int group_elem_cnt = 0;

    for (unsigned int i = 0; i < ino_cnt; i++)
    {
        if (!inodes[i])
            continue;

        unsigned int free_bytes = fs.clustersize - group_size;
        unsigned int inode_size = ffsp_get_inode_size(fs, inodes[i]);

        if (inode_size > free_bytes)
        {
            /* no more free space inside the cluster for
             * additional inodes */
            break;
        }
        /* move the current inode into the inode group */
        group[group_elem_cnt++] = inodes[i];
        inodes[i] = NULL;
        group_size += inode_size;
    }
    return group_elem_cnt;
}

/* Read all valid inodes from the specified cluster. */
int ffsp_read_inode_group(ffsp_fs& fs, unsigned int cl_id, ffsp_inode** inodes)
{
    uint64_t cl_offset = cl_id * fs.clustersize;

    uint64_t read_bytes = 0;
    if (!ffsp_read_raw(fs.fd, fs.buf, fs.clustersize, cl_offset, read_bytes))
        return -errno;
    ffsp_debug_update(fs, FFSP_DEBUG_READ_RAW, read_bytes);

    int ino_cnt = 0;
    char* ino_buf = fs.buf;

    while ((ino_buf - fs.buf) < (ptrdiff_t)fs.clustersize)
    {
        ffsp_inode* ino = (ffsp_inode*)ino_buf;
        unsigned int ino_size = ffsp_get_inode_size(fs, ino);

        if (ffsp_is_inode_valid(fs, cl_id, ino))
        {
            ino = ffsp_allocate_inode(fs);
            memcpy(ino, ino_buf, ino_size);
            inodes[ino_cnt] = ino;
            ino_cnt++;
        }
        ino_buf += ino_size;
    }
    return ino_cnt;
}

/*
 * Group as many inodes as possible into one cluster, write the cluster
 * to disk and update all meta data.
 */
int ffsp_write_inodes(ffsp_fs& fs, ffsp_inode** inodes, unsigned int ino_cnt)
{
    if (!ino_cnt)
        return 0;

    /* Needed to get the correct erase block type.
     * There might be different types for dentries and files */
    uint32_t mode = get_be32(inodes[0]->i_mode);

    /* an inode group can have a max size of ino_cnt elements */
    ffsp_inode** group = (ffsp_inode**)malloc(ino_cnt * sizeof(ffsp_inode*));
    if (!group)
    {
        ffsp_log().critical("malloc(inode group) failed");
        abort();
    }

    int group_elem_cnt;
    while ((group_elem_cnt = get_inode_group(fs, inodes, ino_cnt, group)))
    {
        ffsp_eraseblk_type eb_type = ffsp_get_eraseblk_type(fs, FFSP_DATA_EMB, mode);

        /* search for a cluster id to write the inode(s) to */
        unsigned int eb_id;
        unsigned int cl_id;
        int rc = ffsp_find_writable_cluster(fs, eb_type, eb_id, cl_id);
        if (rc < 0)
        {
            ffsp_log().debug("Failed to find writable cluster or erase block");
            free(group);
            return rc;
        }
        uint64_t offset = cl_id * fs.clustersize;

        group_inodes(fs, group, group_elem_cnt, fs.buf);
        uint64_t written_bytes = 0;
        if (!ffsp_write_raw(fs.fd, fs.buf, fs.clustersize, offset, written_bytes))
        {
            free(group);
            return -errno;
        }
        ffsp_debug_update(fs, FFSP_DEBUG_WRITE_RAW, written_bytes);

        /* ignore the last parameter - it is only needed if we wrote
         * into an erase block with a summary block at its end. but
         * inode erase blocks do not have a summary block. */
        ffsp_commit_write_operation(fs, eb_type, eb_id, put_be32(0));

        /* assign the new cluster id to all inode map entries, update
         * information about how many inodes reside inside the written
         * cluster, and unmark the written inodes */
        for (int i = 0; i < group_elem_cnt; i++)
        {
            fs.ino_map[get_be32(group[i]->i_no)] = put_be32(cl_id);
            fs.cl_occupancy[cl_id]++;
            ffsp_reset_dirty(fs, group[i]);
        }
    }
    free(group);
    return 0;
}
