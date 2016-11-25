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

#include "inode_group.hpp"
#include "debug.hpp"
#include "eraseblk.hpp"
#include "ffsp.hpp"
#include "inode.hpp"
#include "inode_cache.hpp"
#include "io_raw.hpp"
#include "log.hpp"

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

/* copy grouped elements into the cluster buffer. */
static void group_inodes(const fs_context& fs, inode** group,
                         int group_elem_cnt, char* cl_buf)
{
    unsigned int cl_filling = 0;
    for (int i = 0; i < group_elem_cnt; i++)
    {
        unsigned int ino_size = get_inode_size(fs, group[i]);
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
static int get_inode_group(const fs_context& fs, inode** inodes,
                           unsigned int ino_cnt, inode** group)
{
    unsigned int group_size = 0;
    int group_elem_cnt = 0;

    for (unsigned int i = 0; i < ino_cnt; i++)
    {
        if (!inodes[i])
            continue;

        unsigned int free_bytes = fs.clustersize - group_size;
        unsigned int inode_size = get_inode_size(fs, inodes[i]);

        if (inode_size > free_bytes)
        {
            /* no more free space inside the cluster for
             * additional inodes */
            break;
        }
        /* move the current inode into the inode group */
        group[group_elem_cnt++] = inodes[i];
        inodes[i] = nullptr;
        group_size += inode_size;
    }
    return group_elem_cnt;
}

/* Read all valid inodes from the specified cluster. */
int read_inode_group(fs_context& fs, unsigned int cl_id, inode** inodes)
{
    uint64_t cl_offset = cl_id * fs.clustersize;

    ssize_t rc = read_raw(*fs.io_ctx, fs.buf, fs.clustersize, cl_offset);
    if (rc < 0)
        return static_cast<int>(rc);
    debug_update(fs, debug_metric::read_raw, static_cast<uint64_t>(rc));

    int ino_cnt = 0;
    char* ino_buf = fs.buf;

    while ((ino_buf - fs.buf) < (ptrdiff_t)fs.clustersize)
    {
        inode* ino = (inode*)ino_buf;
        unsigned int ino_size = get_inode_size(fs, ino);

        if (is_inode_valid(fs, cl_id, ino))
        {
            ino = allocate_inode(fs);
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
int write_inodes(fs_context& fs, inode** inodes, unsigned int ino_cnt)
{
    if (!ino_cnt)
        return 0;

    /* Needed to get the correct erase block type.
     * There might be different types for dentries and files */
    bool for_dentry = S_ISDIR(get_be32(inodes[0]->i_mode));

    /* an inode group can have a max size of ino_cnt elements */
    inode** group = (inode**)malloc(ino_cnt * sizeof(inode*));
    if (!group)
    {
        log().critical("malloc(inode group) failed");
        abort();
    }

    int group_elem_cnt;
    while ((group_elem_cnt = get_inode_group(fs, inodes, ino_cnt, group)))
    {
        eraseblock_type eb_type = get_eraseblk_type(fs, inode_data_type::emb, for_dentry);

        /* search for a cluster id to write the inode(s) to */
        unsigned int eb_id;
        unsigned int cl_id;
        int rc = find_writable_cluster(fs, eb_type, eb_id, cl_id);
        if (rc < 0)
        {
            log().debug("Failed to find writable cluster or erase block");
            free(group);
            return -ENOSPC;
        }
        uint64_t offset = cl_id * fs.clustersize;

        group_inodes(fs, group, group_elem_cnt, fs.buf);
        ssize_t write_rc = write_raw(*fs.io_ctx, fs.buf, fs.clustersize, offset);
        if (write_rc < 0)
        {
            free(group);
            return static_cast<int>(write_rc);
        }
        debug_update(fs, debug_metric::write_raw, static_cast<uint64_t>(write_rc));

        /* ignore the last parameter - it is only needed if we wrote
         * into an erase block with a summary block at its end. but
         * inode erase blocks do not have a summary block. */
        commit_write_operation(fs, eb_type, eb_id, put_be32(0));

        /* assign the new cluster id to all inode map entries, update
         * information about how many inodes reside inside the written
         * cluster, and unmark the written inodes */
        for (int i = 0; i < group_elem_cnt; i++)
        {
            fs.ino_map[get_be32(group[i]->i_no)] = put_be32(cl_id);
            fs.cl_occupancy[cl_id]++;
            reset_dirty(fs, group[i]);
        }
    }
    free(group);
    return 0;
}

} // namespace ffsp
