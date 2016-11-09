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
#include <fcntl.h>
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
    if (!ffsp_read_raw(fs.fd, &sb, sizeof(superblock), 0, read_bytes))
    {
        ffsp_log().critical("reading super block failed");
        abort();
    }
    ffsp_debug_update(fs, FFSP_DEBUG_READ_RAW, read_bytes);

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
        ffsp_log().critical("malloc(erase blocks - size=%d) failed", size);
        abort();
    }

    uint64_t offset = fs.clustersize;
    uint64_t read_bytes = 0;
    if (!ffsp_read_raw(fs.fd, fs.eb_usage, size, offset, read_bytes))
    {
        ffsp_log().critical("reading erase block info failed");
        free(fs.eb_usage);
        abort();
    }
    ffsp_debug_update(fs, FFSP_DEBUG_READ_RAW, read_bytes);
}

static void read_ino_map(fs_context& fs)
{
    // Size of the array in bytes holding the cluster ids.
    uint64_t size = fs.nino * sizeof(uint32_t);

    fs.ino_map = (be32_t*)malloc(size);
    if (!fs.ino_map)
    {
        ffsp_log().critical("malloc(inode ids - size=%d) failed", size);
        abort();
    }

    uint64_t offset = fs.erasesize - size; // read the invalid inode, too
    uint64_t read_bytes = 0;
    if (!ffsp_read_raw(fs.fd, fs.ino_map, size, offset, read_bytes))
    {
        ffsp_log().critical("reading cluster ids failed");
        free(fs.ino_map);
        abort();
    }
    ffsp_debug_update(fs, FFSP_DEBUG_READ_RAW, read_bytes);
}

static void read_cl_occupancy(fs_context& fs)
{
    off_t size;
    int cl_occ_size;
    unsigned int cl_id;

    size = lseek(fs.fd, 0, SEEK_END);
    if (size == -1)
    {
        ffsp_log().critical("lseek() on device failed");
        exit(EXIT_FAILURE);
    }

    cl_occ_size = (size / fs.clustersize) * sizeof(int);
    fs.cl_occupancy = (int*)malloc(cl_occ_size);
    if (!fs.cl_occupancy)
    {
        ffsp_log().critical("malloc(cluster occupancy array) failed");
        abort();
    }
    memset(fs.cl_occupancy, 0, cl_occ_size);

    /* Initialize the cluster occupancy array. Check how many inodes
     * are valid in each cluster. */
    for (unsigned int i = 1; i < fs.nino; i++)
    {
        cl_id = get_be32(fs.ino_map[i]);
        if (cl_id)
            fs.cl_occupancy[cl_id]++;
    }
}

bool ffsp_mount(fs_context& fs, const char* path)
{
    /*
     * O_DIRECT could also be used if all pwrite() calls get a
     * page-aligned write pointer. But to get that calls to malloc had
     * to be replaced by posix_memalign with 4k alignment.
     */
#ifdef _WIN32
    fs.fd = open(path, O_RDWR);
#else
    fs.fd = open(path, O_RDWR | O_SYNC);
#endif
    if (fs.fd == -1)
    {
        ffsp_log().error("ffsp_mount(): open(path={}) failed", path);
        return false;
    }
    read_super(fs);
    read_eb_usage(fs);
    read_ino_map(fs);

    fs.summary_cache = ffsp_summary_cache_init(fs);

    fs.inode_cache = ffsp_inode_cache_init(fs);

    size_t ino_bitmask_size = fs.nino / sizeof(uint32_t) + 1;
    fs.ino_status_map = (uint32_t*)malloc(ino_bitmask_size);
    if (!fs.ino_status_map)
    {
        ffsp_log().critical("malloc(dirty inodes mask) failed");
        goto error;
    }
    memset(fs.ino_status_map, 0, ino_bitmask_size);

    read_cl_occupancy(fs);

    fs.dirty_ino_cnt = 0;

    fs.gcinfo = ffsp_gcinfo_init(fs);

    fs.buf = (char*)malloc(fs.erasesize);
    if (!fs.buf)
    {
        ffsp_log().critical("ffsp_mount(): malloc(erasesize) failed");
        goto error;
    }
    return true;

error:
    /* FIXME: will crash if one of the pointer was not yet allocated! */

    free(fs.eb_usage);
    free(fs.ino_map);
    free(fs.ino_status_map);
    free(fs.cl_occupancy);
    free(fs.gcinfo);
    free(fs.buf);
    return false;
}

void ffsp_unmount(fs_context& fs)
{
    ffsp_release_inodes(fs);
    ffsp_close_eraseblks(fs);
    ffsp_write_meta_data(fs);

    if ((fs.fd != -1) && (close(fs.fd) == -1))
        ffsp_log().error("ffsp_unmount(): close(fd) failed");

    ffsp_inode_cache_uninit(fs.inode_cache);
    ffsp_summary_cache_uninit(fs.summary_cache);
    ffsp_gcinfo_uninit(fs.gcinfo);
    free(fs.eb_usage);
    free(fs.ino_map);
    free(fs.ino_status_map);
    free(fs.cl_occupancy);
    free(fs.buf);
}

} // namespace ffsp
