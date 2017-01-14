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

#include "mount.hpp"
#include "debug.hpp"
#include "eraseblk.hpp"
#include "ffsp.hpp"
#include "gc.hpp"
#include "inode.hpp"
#include "inode_cache.hpp"
#include "io_backend.hpp"
#include "io_raw.hpp"
#include "log.hpp"
#include "mkfs.hpp"
#include "summary.hpp"
#include "utils.hpp"

#include <algorithm>
#include <memory>

#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace ffsp
{

static bool read_super(fs_context& fs)
{
    superblock sb;
    ssize_t rc = read_raw(*fs.io_ctx, &sb, sizeof(superblock), 0);
    if (rc < 0)
    {
        log().critical("reading super block failed");
        return false;
    }
    debug_update(fs, debug_metric::read_raw, static_cast<uint64_t>(rc));

    // The super block is only read once and its content is saved
    // inside the ffsp structure.
    fs.fsid = get_be32(sb.s_fsid);
    fs.flags = get_be32(sb.s_flags);
    fs.neraseblocks = get_be32(sb.s_neraseblocks);
    fs.nino = get_be32(sb.s_nino);
    fs.blocksize = get_be32(sb.s_blocksize);
    fs.clustersize = get_be32(sb.s_clustersize);
    fs.erasesize = get_be32(sb.s_erasesize);
    fs.ninoopen = get_be32(sb.s_ninoopen);
    fs.neraseopen = get_be32(sb.s_neraseopen);
    fs.nerasereserve = get_be32(sb.s_nerasereserve);
    fs.nerasewrites = get_be32(sb.s_nerasewrites);
    return true;
}

static bool read_eb_usage(fs_context& fs)
{
    fs.eb_usage.resize(fs.neraseblocks);

    // size of all erase block meta information in bytes
    uint64_t size = fs.neraseblocks * sizeof(eraseblock);
    uint64_t offset = fs.clustersize;

    ssize_t rc = read_raw(*fs.io_ctx, fs.eb_usage.data(), size, offset);
    if (rc < 0)
    {
        log().critical("reading erase block info failed");
        return false;
    }
    debug_update(fs, debug_metric::read_raw, static_cast<uint64_t>(rc));
    return true;
}

static bool read_ino_map(fs_context& fs)
{
    fs.ino_map.resize(fs.nino);

    // size of the array holding the cluster ids in bytes
    uint64_t size = fs.nino * sizeof(uint32_t);
    uint64_t offset = fs.erasesize - size;

    ssize_t rc = read_raw(*fs.io_ctx, fs.ino_map.data(), size, offset);
    if (rc < 0)
    {
        log().critical("reading cluster ids failed");
        return false;
    }
    debug_update(fs, debug_metric::read_raw, static_cast<uint64_t>(rc));
    return true;
}

static bool read_cl_occupancy(fs_context& fs)
{
    off_t size = io_backend_size(*fs.io_ctx);
    if (size == -1)
    {
        log().critical("retrieving file size from device failed");
        return false;
    }

    fs.cl_occupancy.resize(size / fs.clustersize);
    std::fill(fs.cl_occupancy.begin(), fs.cl_occupancy.end(), 0);

    // Initialize the cluster occupancy array. Check how many inodes
    // are valid in each cluster.
    for (unsigned int i = 1; i < fs.nino; i++)
    {
        cl_id_t cl_id = get_be32(fs.ino_map[i]);
        if (cl_id)
            fs.cl_occupancy[cl_id]++;
    }
    return true;
}

fs_context* mount(io_backend* ctx)
{
    if (!ctx)
    {
        log().error("ffsp::mount(): invalid io backend");
        return nullptr;
    }

    auto fs = std::make_unique<fs_context>();
    fs->io_ctx = ctx;

    if (   !read_super(*fs)
        || !read_eb_usage(*fs)
        || !read_ino_map(*fs))
    {
        log().critical("ffsp::mount(): failed to read data from super erase block");
        return nullptr;
    }

    if (!read_cl_occupancy(*fs))
    {
        log().critical("ffsp::mount(): failed to read cluster occupancy data");
        return nullptr;
    }

    fs->summary_cache = summary_cache_init(*fs);
    fs->inode_cache = inode_cache_init(*fs);
    fs->gcinfo = gcinfo_init(*fs);

    size_t ino_bitmask_size = fs->nino / 8;
    fs->ino_status_map = new uint32_t[ino_bitmask_size / sizeof(uint32_t)];
    memset(fs->ino_status_map, 0, ino_bitmask_size);

    fs->buf = new char[fs->erasesize];

    return fs.release();
}

io_backend* unmount(fs_context* fs)
{
    release_inodes(*fs);
    close_eraseblks(*fs);
    write_meta_data(*fs);

    inode_cache_uninit(fs->inode_cache);
    summary_cache_uninit(fs->summary_cache);
    gcinfo_uninit(fs->gcinfo);

    delete[] fs->ino_status_map;
    delete[] fs->buf;

    io_backend* io_ctx = fs->io_ctx;
    delete fs;
    return io_ctx;
}

} // namespace ffsp
