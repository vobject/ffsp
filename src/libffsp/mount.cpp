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
#include "eraseblk.hpp"
#include "debug.hpp"
#include "ffsp.hpp"
#include "gc.hpp"
#include "inode.hpp"
#include "inode_cache.hpp"
#include "io_raw.hpp"
#include "log.hpp"
#include "summary.hpp"
#include "utils.hpp"

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

static void read_super(fs_context& fs)
{
    superblock sb;
    uint64_t read_bytes = 0;
    if (!read_raw(*fs.io_ctx, &sb, sizeof(superblock), 0, read_bytes))
    {
        log().critical("reading super block failed");
        abort();
    }
    debug_update(fs, debug_metric::read_raw, read_bytes);

    // The super block is only read once and its content is saved
    //  inside the ffsp structure.
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
}

static void read_eb_usage(fs_context& fs)
{
    // Size of the array that holds the erase block meta information
    uint64_t size = fs.neraseblocks * sizeof(eraseblock);

    fs.eb_usage = (eraseblock*)malloc(size);
    if (!fs.eb_usage)
    {
        log().critical("malloc(erase blocks - size=%d) failed", size);
        abort();
    }

    uint64_t offset = fs.clustersize;
    uint64_t read_bytes = 0;
    if (!read_raw(*fs.io_ctx, fs.eb_usage, size, offset, read_bytes))
    {
        log().critical("reading erase block info failed");
        free(fs.eb_usage);
        abort();
    }
    debug_update(fs, debug_metric::read_raw, read_bytes);
}

static void read_ino_map(fs_context& fs)
{
    // Size of the array in bytes holding the cluster ids.
    uint64_t size = fs.nino * sizeof(uint32_t);

    fs.ino_map = (be32_t*)malloc(size);
    if (!fs.ino_map)
    {
        log().critical("malloc(inode ids - size=%d) failed", size);
        abort();
    }

    uint64_t offset = fs.erasesize - size; // read the invalid inode, too
    uint64_t read_bytes = 0;
    if (!read_raw(*fs.io_ctx, fs.ino_map, size, offset, read_bytes))
    {
        log().critical("reading cluster ids failed");
        free(fs.ino_map);
        abort();
    }
    debug_update(fs, debug_metric::read_raw, read_bytes);
}

static void read_cl_occupancy(fs_context& fs)
{
    off_t size = io_context_size(*fs.io_ctx);
    if (size == -1)
    {
        log().critical("retrieving file size from device failed");
        exit(EXIT_FAILURE);
    }

    int cl_occ_size = (size / fs.clustersize) * sizeof(int);
    fs.cl_occupancy = (int*)malloc(cl_occ_size);
    if (!fs.cl_occupancy)
    {
        log().critical("malloc(cluster occupancy array) failed");
        abort();
    }
    memset(fs.cl_occupancy, 0, cl_occ_size);

    /* Initialize the cluster occupancy array. Check how many inodes
     * are valid in each cluster. */
    for (unsigned int i = 1; i < fs.nino; i++)
    {
        unsigned int cl_id = get_be32(fs.ino_map[i]);
        if (cl_id)
            fs.cl_occupancy[cl_id]++;
    }
}

fs_context* mount(const char* path)
{
    auto* fs = new fs_context;

    fs->io_ctx = io_context_init(path);
    if (!fs->io_ctx)
    {
        log().error("ffsp::mount(): init I/O context failed (path={})", path);
        return nullptr;
    }

    read_super(*fs);
    read_eb_usage(*fs);
    read_ino_map(*fs);

    fs->summary_cache = summary_cache_init(*fs);

    fs->inode_cache = inode_cache_init(*fs);

    size_t ino_bitmask_size = fs->nino / sizeof(uint32_t) + 1;
    fs->ino_status_map = (uint32_t*)malloc(ino_bitmask_size);
    if (!fs->ino_status_map)
    {
        log().critical("malloc(dirty inodes mask) failed");
        goto error;
    }
    memset(fs->ino_status_map, 0, ino_bitmask_size);

    read_cl_occupancy(*fs);

    fs->dirty_ino_cnt = 0;

    fs->gcinfo = gcinfo_init(*fs);

    fs->buf = (char*)malloc(fs->erasesize);
    if (!fs->buf)
    {
        log().critical("ffsp::mount(): malloc(erasesize) failed");
        goto error;
    }
    return fs;

error:
    /* FIXME: will crash if one of the pointer was not yet allocated! */

    free(fs->eb_usage);
    free(fs->ino_map);
    free(fs->ino_status_map);
    free(fs->cl_occupancy);
    free(fs->gcinfo);
    free(fs->buf);
    return nullptr;
}

void unmount(fs_context* fs)
{
    release_inodes(*fs);
    close_eraseblks(*fs);
    write_meta_data(*fs);

    inode_cache_uninit(fs->inode_cache);
    summary_cache_uninit(fs->summary_cache);
    gcinfo_uninit(fs->gcinfo);
    io_context_uninit(fs->io_ctx);

    free(fs->eb_usage);
    free(fs->ino_map);
    free(fs->ino_status_map);
    free(fs->cl_occupancy);
    free(fs->buf);
    delete fs;
}

} // namespace ffsp
