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
static void group_inodes(const fs_context& fs, std::vector<inode*> group, char* cl_buf)
{
    unsigned int cl_filling = 0;
    for (const auto& inode : group)
    {
        unsigned int ino_size = get_inode_size(fs, inode);
        memcpy(cl_buf + cl_filling, inode, ino_size);
        cl_filling += ino_size;
    }
    memset(cl_buf + cl_filling, 0, fs.clustersize - cl_filling);
}

/*
 * Search for inodes that would fit into one cluster and save a pointer to
 * those in 'group'. The grouped pointer are invalidated in 'inodes'.
 * Return the size of the inode group in bytes.
 */
static uint64_t get_inode_group(const fs_context& fs, std::vector<inode*>& inodes, std::vector<inode*>& group)
{
    uint64_t free_bytes = fs.clustersize;

    for (auto& inode : inodes)
    {
        if (!inode)
            continue;

        //unsigned int free_bytes = fs.clustersize - group_size;
        uint64_t ino_size = get_inode_size(fs, inode);

        if (ino_size > free_bytes)
        {
            /* no more free space inside the cluster for additional inodes */
            break;
        }
        /* move the current inode into the inode group */
        group.push_back(inode);
        inode = nullptr;
        free_bytes -= ino_size;
    }
    return fs.clustersize - free_bytes;
}

int read_inode_group(fs_context& fs, cl_id_t cl_id, std::vector<inode*>& inodes)
{
    uint64_t cl_offset = cl_id * fs.clustersize;

    ssize_t rc = read_raw(*fs.io_ctx, fs.buf, fs.clustersize, cl_offset);
    if (rc < 0)
        return static_cast<int>(rc);
    debug_update(fs, debug_metric::read_raw, static_cast<uint64_t>(rc));

    inodes.clear();
    // Number of inodes that can fit into one cluster
    inodes.reserve(fs.clustersize / sizeof(inode));

    char* ino_buf = fs.buf;
    while ((ino_buf - fs.buf) < (ptrdiff_t)fs.clustersize)
    {
        inode* ino = (inode*)ino_buf;
        auto ino_size = get_inode_size(fs, ino);

        if (is_inode_valid(fs, cl_id, ino))
        {
            ino = allocate_inode(fs);
            memcpy(ino, ino_buf, ino_size);
            inodes.push_back(ino);
        }
        ino_buf += ino_size;
    }
    return 0;
}

int write_inodes(fs_context& fs, const std::vector<inode*>& inodes)
{
    if (inodes.empty())
        return 0;

    /* Needed to get the correct erase block type.
     * There might be different types for dentries and files */
    bool for_dentry = S_ISDIR(get_be32(inodes[0]->i_mode));

    std::vector<inode*> inodes_cpy{inodes};

    std::vector<inode*> group;
    group.reserve(inodes.size());

    while (true)
    {
        group.clear();
        auto group_size = get_inode_group(fs, inodes_cpy, group);
        if (group.empty())
            break;

        log().info("Group {} {} inodes taking up {} bytes", group.size(), for_dentry ? "dentry" : "file", group_size);

        /* search for a cluster id to write the inode(s) to */
        eraseblock_type eb_type = get_eraseblk_type(fs, inode_data_type::emb, for_dentry);
        eb_id_t eb_id;
        cl_id_t cl_id;
        if (!find_writable_cluster(fs, eb_type, eb_id, cl_id))
        {
            log().info("Failed to find writable cluster or erase block");
            return -ENOSPC;
        }
        uint64_t offset = cl_id * fs.clustersize;

        group_inodes(fs, group, fs.buf);
        ssize_t write_rc = write_raw(*fs.io_ctx, fs.buf, fs.clustersize, offset);
        if (write_rc < 0)
            return static_cast<int>(write_rc);
        debug_update(fs, debug_metric::write_raw, static_cast<uint64_t>(write_rc));

        /* ignore the last parameter - it is only needed if we wrote
         * into an erase block with a summary block at its end. but
         * inode erase blocks do not have a summary block. */
        commit_write_operation(fs, eb_type, eb_id, put_be32(0));

        /* assign the new cluster id to all inode map entries, update
         * information about how many inodes reside inside the written
         * cluster, and unmark the written inodes */
        for (const auto& inode : group)
        {
            fs.ino_map[get_be32(inode->i_no)] = put_be32(cl_id);
            fs.cl_occupancy[cl_id]++;
            reset_dirty(fs, *inode);
        }
    }
    return 0;
}

} // namespace ffsp
