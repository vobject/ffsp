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

#include "log.hpp"
#include "ffsp.hpp"
#include "io_raw.hpp"
#include "eraseblk.hpp"
#include "inode.hpp"
#include "inode_cache.hpp"
#include "inode_group.hpp"

#include <cstdlib>
#include <cstring>

/* copy grouped elements into the cluster buffer. */
static void group_inodes(const struct ffsp* fs, struct ffsp_inode** group,
                         int group_elem_cnt, void* cl_buf)
{
    unsigned int ino_size;
    unsigned int cl_filling = 0;
    char* buf_ptr = (char*)cl_buf;

    for (int i = 0; i < group_elem_cnt; i++)
    {
        ino_size = ffsp_get_inode_size(fs, group[i]);
        memcpy(buf_ptr + cl_filling, group[i], ino_size);
        cl_filling += ino_size;
    }
    memset(buf_ptr + cl_filling, 0, fs->clustersize - cl_filling);
}

/* Read all valid inodes from the specified cluster. */
int ffsp_read_inode_group(struct ffsp* fs, unsigned int cl_id,
                          struct ffsp_inode** inodes)
{
    int rc;
    uint64_t cl_offset;
    int ino_cnt;
    char* ino_buf;
    struct ffsp_inode* ino;
    unsigned int ino_size;

    cl_offset = cl_id * fs->clustersize;
    rc = ffsp_read_raw(fs->fd, fs->buf, fs->clustersize, cl_offset);
    if (rc < 0)
        return rc;

    ino_cnt = 0;
    ino_buf = fs->buf;

    while ((ino_buf - fs->buf) < (ptrdiff_t)fs->clustersize)
    {
        ino = (struct ffsp_inode*)ino_buf;
        ino_size = ffsp_get_inode_size(fs, ino);

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
 * Search for inodes that would fit into one cluster and save a pointer to
 * those in 'group'. The grouped pointer are invalidated in 'inodes'.
 * Return the number of inodes grouped together.
 */
int ffsp_get_inode_group(const struct ffsp* fs, struct ffsp_inode** inodes,
                         int ino_cnt, struct ffsp_inode** group)
{
    unsigned int free_bytes;
    unsigned int inode_size;
    unsigned int group_size;
    int group_elem_cnt;

    group_size = 0;
    group_elem_cnt = 0;

    for (int i = 0; i < ino_cnt; i++)
    {
        if (!inodes[i])
            continue;

        free_bytes = fs->clustersize - group_size;
        inode_size = ffsp_get_inode_size(fs, inodes[i]);

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

/*
 * Group as many inodes as possible into one cluster, write the cluster
 * to disk and update all meta data.
 */
int ffsp_write_inodes(struct ffsp* fs, struct ffsp_inode** inodes, int ino_cnt)
{
    int rc;
    uint32_t mode;
    int eb_type;
    struct ffsp_inode** group;
    int group_elem_cnt;
    unsigned int eb_id;
    unsigned int cl_id;
    uint64_t offset;

    if (!ino_cnt)
        return 0;

    /* Needed to get the correct erase block type.
	 * There might be different types for dentries and files */
    mode = get_be32(inodes[0]->i_mode);

    /* an inode group can have a max size of ino_cnt elements */
    group = (struct ffsp_inode**)malloc(ino_cnt * sizeof(struct ffsp_inode*));
    if (!group)
    {
        FFSP_ERROR("malloc(inode group) failed");
        abort();
    }

    while ((group_elem_cnt = ffsp_get_inode_group(fs, inodes, ino_cnt, group)))
    {
        eb_type = ffsp_get_eraseblk_type(fs, FFSP_DATA_EMB, mode);

        /* search for a cluster id to write the inode(s) to */
        rc = ffsp_find_writable_cluster(fs, eb_type, &eb_id, &cl_id);
        if (rc < 0)
        {
            FFSP_DEBUG("Failed to find writable cluster or erase block");
            free(group);
            return rc;
        }
        offset = cl_id * fs->clustersize;

        group_inodes(fs, group, group_elem_cnt, fs->buf);
        rc = ffsp_write_raw(fs->fd, fs->buf, fs->clustersize, offset);
        if (rc < 0)
        {
            free(group);
            return rc;
        }

        /* ignore the last parameter - it is only needed if we wrote
		 * into an erase block with a summary block at its end. but
		 * inode erase blocks do not have a summary block. */
        ffsp_commit_write_operation(fs, eb_type, eb_id, put_be32(0));

        /* assign the new cluster id to all inode map entries, update
		 * information about how many inodes reside inside the written
		 * cluster, and unmark the written inodes */
        for (int i = 0; i < group_elem_cnt; i++)
        {
            fs->ino_map[get_be32(group[i]->i_no)] = put_be32(cl_id);
            fs->cl_occupancy[cl_id]++;
            ffsp_reset_dirty(fs, group[i]);
        }
    }
    free(group);
    return 0;
}
