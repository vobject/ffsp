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
#include "ffsp.hpp"
#include "inode.hpp"
#include "inode_cache.hpp"
#include "io_raw.hpp"
#include "log.hpp"
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

static void read_super(struct ffsp* fs)
{
    int rc;
    struct ffsp_super sb;

    rc = ffsp_read_raw(fs->fd, &sb, sizeof(struct ffsp_super), 0);
    if (rc < 0)
    {
        ffsp_log().critical("reading super block failed");
        abort();
    }

    // The super block is only read once and its content is saved
    //  inside the ffsp structure.
    fs->fsid = get_be32(sb.s_fsid);
    fs->flags = get_be32(sb.s_flags);
    fs->neraseblocks = get_be32(sb.s_neraseblocks);
    fs->nino = get_be32(sb.s_nino);
    fs->blocksize = get_be32(sb.s_blocksize);
    fs->clustersize = get_be32(sb.s_clustersize);
    fs->erasesize = get_be32(sb.s_erasesize);
    fs->ninoopen = get_be32(sb.s_ninoopen);
    fs->neraseopen = get_be32(sb.s_neraseopen);
    fs->nerasereserve = get_be32(sb.s_nerasereserve);
    fs->nerasewrites = get_be32(sb.s_nerasewrites);
}

static void read_eb_usage(struct ffsp* fs)
{
    int size;
    uint64_t offset;

    // Size of the array that holds the erase block meta information
    size = fs->neraseblocks * sizeof(struct ffsp_eraseblk);

    fs->eb_usage = (struct ffsp_eraseblk*)malloc(size);
    if (!fs->eb_usage)
    {
        ffsp_log().critical("malloc(erase blocks - size=%d) failed", size);
        abort();
    }

    offset = fs->clustersize;
    if ((ffsp_read_raw(fs->fd, fs->eb_usage, size, offset)) < 0)
    {
        ffsp_log().critical("reading erase block info failed");
        free(fs->eb_usage);
        abort();
    }
}

static void read_ino_map(struct ffsp* fs)
{
    int size;
    uint64_t offset;

    // Size of the array in bytes holding the cluster ids.
    size = fs->nino * sizeof(uint32_t);

    fs->ino_map = (be32_t*)malloc(size);
    if (!fs->ino_map)
    {
        ffsp_log().critical("malloc(inode ids - size=%d) failed", size);
        abort();
    }

    offset = fs->erasesize - size; // read the invalid inode, too
    if ((ffsp_read_raw(fs->fd, fs->ino_map, size, offset)) < 0)
    {
        ffsp_log().critical("reading cluster ids failed");
        free(fs->ino_map);
        abort();
    }
}

static void read_cl_occupancy(struct ffsp* fs)
{
    off_t size;
    int cl_occ_size;
    unsigned int cl_id;

    size = lseek(fs->fd, 0, SEEK_END);
    if (size == -1)
    {
        ffsp_log().critical("lseek() on device failed");
        exit(EXIT_FAILURE);
    }

    cl_occ_size = (size / fs->clustersize) * sizeof(int);
    fs->cl_occupancy = (int*)malloc(cl_occ_size);
    if (!fs->cl_occupancy)
    {
        ffsp_log().critical("malloc(cluster occupancy array) failed");
        abort();
    }
    memset(fs->cl_occupancy, 0, cl_occ_size);

    /* Initialize the cluster occupancy array. Check how many inodes
	 * are valid in each cluster. */
    for (unsigned int i = 1; i < fs->nino; i++)
    {
        cl_id = get_be32(fs->ino_map[i]);
        if (cl_id)
            fs->cl_occupancy[cl_id]++;
    }
}

void init_gcinfo(struct ffsp* fs)
{
    // FIXME: Too much hard coded crap in here!

    // TODO: Set write_time correctly!

    if (fs->neraseopen == 3)
    {
        fs->gcinfo[0].eb_type = FFSP_EB_DENTRY_INODE;
        fs->gcinfo[0].write_time = 0;
        fs->gcinfo[0].write_cnt = 0;

        fs->gcinfo[1].eb_type = FFSP_EB_DENTRY_CLIN;
        fs->gcinfo[1].write_time = 0;
        fs->gcinfo[1].write_cnt = 0;
    }
    else if (fs->neraseopen == 4)
    {
        fs->gcinfo[0].eb_type = FFSP_EB_DENTRY_INODE;
        fs->gcinfo[0].write_time = 0;
        fs->gcinfo[0].write_cnt = 0;

        fs->gcinfo[1].eb_type = FFSP_EB_FILE_INODE;
        fs->gcinfo[1].write_time = 0;
        fs->gcinfo[1].write_cnt = 0;

        fs->gcinfo[2].eb_type = FFSP_EB_DENTRY_CLIN;
        fs->gcinfo[2].write_time = 0;
        fs->gcinfo[2].write_cnt = 0;
    }
    else if (fs->neraseopen >= 5)
    {
        fs->gcinfo[0].eb_type = FFSP_EB_DENTRY_INODE;
        fs->gcinfo[0].write_time = 0;
        fs->gcinfo[0].write_cnt = 0;

        fs->gcinfo[1].eb_type = FFSP_EB_FILE_INODE;
        fs->gcinfo[1].write_time = 0;
        fs->gcinfo[1].write_cnt = 0;

        fs->gcinfo[2].eb_type = FFSP_EB_DENTRY_CLIN;
        fs->gcinfo[2].write_time = 0;
        fs->gcinfo[2].write_cnt = 0;

        fs->gcinfo[3].eb_type = FFSP_EB_FILE_CLIN;
        fs->gcinfo[3].write_time = 0;
        fs->gcinfo[3].write_cnt = 0;
    }
}

int ffsp_mount(struct ffsp* fs, const char* path)
{
    int ino_bitmask_size;
    int gcinfo_size;

/*
	 * O_DIRECT could also be used if all pwrite() calls get a
	 * page-aligned write pointer. But to get that calls to malloc had
	 * to be replaced by posix_memalign with 4k alignment.
	 */
#ifdef _WIN32
    fs->fd = open(path, O_RDWR);
#else
    fs->fd = open(path, O_RDWR | O_SYNC);
#endif
    if (fs->fd == -1)
    {
        ffsp_log().error("ffsp_mount(): open(path={}) failed", path);
        return -1;
    }
    read_super(fs);
    read_eb_usage(fs);
    read_ino_map(fs);

    // Initialize erase block summary list.
    fs->summary_head.eb_type = 0;
    fs->summary_head.summary = NULL;
    fs->summary_head.next = NULL;

    ffsp_inode_cache_init(fs, &fs->ino_cache);

    ino_bitmask_size = fs->nino / sizeof(uint32_t) + 1;
    fs->ino_status_map = (uint32_t*)malloc(ino_bitmask_size);
    if (!fs->ino_status_map)
    {
        ffsp_log().critical("malloc(dirty inodes mask) failed");
        goto error;
    }
    memset(fs->ino_status_map, 0, ino_bitmask_size);

    read_cl_occupancy(fs);

    fs->dirty_ino_cnt = 0;

    gcinfo_size = (fs->neraseopen - 1) * sizeof(struct ffsp_gcinfo);
    fs->gcinfo = (struct ffsp_gcinfo*)malloc(gcinfo_size);
    if (!fs->gcinfo)
    {
        ffsp_log().critical("malloc(gcinfo) failed");
        goto error;
    }
    init_gcinfo(fs);

    fs->buf = (char*)malloc(fs->erasesize);
    if (!fs->buf)
    {
        ffsp_log().critical("ffsp_mount(): malloc(erasesize) failed");
        goto error;
    }
    return 0;

error:
    /* FIXME: will crash if one of the pointer was not yet allocated! */

    free(fs->eb_usage);
    free(fs->ino_map);
    free(fs->ino_status_map);
    free(fs->cl_occupancy);
    free(fs->gcinfo);
    free(fs->buf);
    return -1;
}

void ffsp_unmount(struct ffsp* fs)
{
    if (!fs)
        return;

    ffsp_release_inodes(fs);
    ffsp_close_eraseblks(fs);
    ffsp_write_meta_data(fs);

    if ((fs->fd != -1) && (close(fs->fd) == -1))
        ffsp_log().debug("ffsp_unmount(): close(fd) failed");

    ffsp_inode_cache_uninit(&fs->ino_cache);
    free(fs->eb_usage);
    free(fs->ino_map);
    free(fs->ino_status_map);
    free(fs->cl_occupancy);
    free(fs->gcinfo);
    free(fs->buf);
}