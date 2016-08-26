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

#include "mkfs.hpp"
#include "io_raw.hpp"
#include "utils.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static int get_eraseblk_cnt(int fd, int eb_size)
{
    off_t size;

    size = lseek(fd, 0, SEEK_END);
    if (size == -1)
    {
        perror("lseek() on file system failed\n");
        exit(EXIT_FAILURE);
    }

    return size / eb_size;
}

static int get_inode_cnt(const int eb_size, int cl_size, int eb_cnt)
{
    // Note that the first inode number is always invalid.
    return (eb_size                                    // Only look at the first erase block
            - cl_size                                  // super block aligned to clustersize
            - (eb_cnt * sizeof(struct ffsp_eraseblk))) // eb usage
           / sizeof(uint32_t);                         // inodes are 4 bytes in size

    // TODO: FFSP_RESERVED_INODE_ID is not taken care of.
    //  But it is highly unlikely that the file system is created with
    //  as many inodes that FFSP_RESERVED_INODE_ID could be tried
    //  to be used as a valid inode_no.
}

int ffsp_mkfs(const char* path, const struct ffsp_mkfs_options* options)
{
    int rc;
    int fd;

#ifdef _WIN32
    fd = open(path, O_WRONLY);
#else
    fd = open(path, O_WRONLY | O_SYNC);
#endif
    if (fd == -1)
    {
        perror("open()\n");
        return -1;
    }

    rc = ffsp_fmkfs(fd, options);
    if (rc < 0)
    {
        return rc;
    }

    if (close(fd) == -1)
    {
        perror("closing file system failed\n");
        return -1;
    }
    return 0;
}

int ffsp_fmkfs(int fd, const struct ffsp_mkfs_options* options)
{
    int rc;
    int eb_cnt;
    int ino_cnt;
    char* eb_buf;
    unsigned int eb_buf_written;
    unsigned int max_writeops;
    struct ffsp_super sb;
    struct ffsp_eraseblk eb;

    memset(&sb, 0, sizeof(sb));
    memset(&eb, 0, sizeof(eb));

    // Setup the first eraseblock with super, usage and inodemap
    eb_buf = (char*)malloc(options->erasesize);
    if (!eb_buf)
    {
        perror("malloc(erasesize)\n");
        return -1;
    }
    max_writeops = options->erasesize / options->clustersize;
    eb_cnt = get_eraseblk_cnt(fd, options->erasesize);
    ino_cnt = get_inode_cnt(options->erasesize, options->clustersize, eb_cnt);

    sb.s_fsid = put_be32(FFSP_FILE_SYSTEM_ID);
    sb.s_flags = put_be32(0);
    sb.s_neraseblocks = put_be32(eb_cnt);
    sb.s_nino = put_be32(ino_cnt);
    sb.s_blocksize = put_be32(options->clustersize);
    sb.s_clustersize = put_be32(options->clustersize);
    sb.s_erasesize = put_be32(options->erasesize);
    sb.s_ninoopen = put_be32(options->ninoopen);
    sb.s_neraseopen = put_be32(options->neraseopen);
    sb.s_nerasereserve = put_be32(options->nerasereserve);
    sb.s_nerasewrites = put_be32(options->nerasewrites);
    memcpy(eb_buf, &sb, sizeof(sb));
    eb_buf_written = sizeof(sb);

    // align eb_usage + ino_map to clustersize
    memset(eb_buf + eb_buf_written, 0, options->clustersize - sizeof(sb));
    eb_buf_written += options->clustersize - sizeof(sb);

    // The first EB is for the superblock, erase block usage and inodes ids
    eb.e_type = FFSP_EB_SUPER;
    eb.e_lastwrite = put_be16(0);
    eb.e_cvalid = put_be16(0);
    eb.e_writeops = put_be16(0);
    memcpy(eb_buf + eb_buf_written, &eb, sizeof(eb));
    eb_buf_written += sizeof(eb);

    // The second EB is for directory entries
    eb.e_type = FFSP_EB_DENTRY_INODE;
    eb.e_lastwrite = put_be16(0);
    eb.e_cvalid = put_be16(1);              // Only the root directory exists...
    eb.e_writeops = put_be16(max_writeops); // ...but the eb is closed.
    memcpy(eb_buf + eb_buf_written, &eb, sizeof(eb));
    eb_buf_written += sizeof(eb);

    for (int i = 2; i < eb_cnt; ++i)
    {
        // The remaining erase blocks are empty
        eb.e_type = FFSP_EB_EMPTY;
        eb.e_lastwrite = put_be16(0);
        eb.e_cvalid = put_be16(0);
        eb.e_writeops = put_be16(0);
        memcpy(eb_buf + eb_buf_written, &eb, sizeof(eb));
        eb_buf_written += sizeof(eb);
    }

    // inode id 0 is defined to be invalid
    be32_t cl_id = put_be32(0xffffffff); // Value does not matter
    memcpy(eb_buf + eb_buf_written, &cl_id, sizeof(cl_id));
    eb_buf_written += sizeof(cl_id);

    // inode id 1 will point to the root inode
    cl_id = put_be32(options->erasesize / options->clustersize);
    memcpy(eb_buf + eb_buf_written, &cl_id, sizeof(cl_id));
    eb_buf_written += sizeof(cl_id);

    // The rest of the cluster id array will be set to 0 -> no indes.
    memset(eb_buf + eb_buf_written, 0, options->erasesize - eb_buf_written);

    // Write the first erase block into the file.
    rc = ffsp_write_raw(fd, eb_buf, options->erasesize, 0);
    if (rc < 0)
    {
        errno = -rc;
        perror("writing first eraseblock failed\n");
        free(eb_buf);
        return -1;
    }

    // Create the inode for the root directory
    struct ffsp_inode root;
    memset(&root, 0, sizeof(root));
    root.i_size = put_be64(sizeof(struct ffsp_dentry) * 2);
    root.i_flags = put_be32(FFSP_DATA_EMB);
    root.i_no = put_be32(1);
    root.i_nlink = put_be32(2);
#ifdef _WIN32
    root.i_uid = put_be32(0);
    root.i_gid = put_be32(0);
    root.i_mode = put_be32(S_IFDIR);
#else
    root.i_uid = put_be32(getuid());
    root.i_gid = put_be32(getgid());
    root.i_mode = put_be32(S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif
    ffsp_update_time(&root.i_ctime);
    memcpy(eb_buf, &root, sizeof(root));
    eb_buf_written = sizeof(root);

    // Fill the embedded data with file and/or directory entries
    struct ffsp_dentry dot; // "."
    memset(&dot, 0, sizeof(dot));
    dot.ino = put_be32(1);
    dot.len = (uint8_t)strlen(".");
    strcpy(dot.name, ".");
    memcpy(eb_buf + eb_buf_written, &dot, sizeof(dot));
    eb_buf_written += sizeof(dot);

    struct ffsp_dentry dotdot; // ".."
    memset(&dotdot, 0, sizeof(dotdot));
    dotdot.ino = put_be32(1);
    dotdot.len = (uint8_t)strlen("..");
    strcpy(dotdot.name, "..");
    memcpy(eb_buf + eb_buf_written, &dotdot, sizeof(dotdot));
    eb_buf_written += sizeof(dotdot);

    rc = ffsp_write_raw(fd, eb_buf, eb_buf_written, options->erasesize);
    if (rc < 0)
    {
        errno = -rc;
        perror("writing second eraseblock failed\n");
        free(eb_buf);
        return -1;
    }
    free(eb_buf);
    return 0;
}
