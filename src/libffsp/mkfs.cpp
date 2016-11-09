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

#include <vector>

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

namespace ffsp
{

static uint32_t get_eraseblk_cnt(int fd, uint32_t eb_size)
{
    off_t size = lseek(fd, 0, SEEK_END);
    if (size == -1)
    {
        perror("lseek() on file system failed\n");
        exit(EXIT_FAILURE);
    }
    return static_cast<uint32_t>(size / eb_size);
}

static uint32_t get_inode_cnt(uint32_t eb_size, uint32_t cl_size, uint32_t eb_cnt)
{
    // Note that the first inode number is always invalid.
    return (eb_size                             // Only look at the first erase block
            - cl_size                           // super block aligned to clustersize
            - (eb_cnt * sizeof(eraseblock))) // eb usage
           / sizeof(uint32_t);                  // inodes are 4 bytes in size

    // TODO: FFSP_RESERVED_INODE_ID is not taken care of.
    //  But it is highly unlikely that the file system is created with
    //  as many inodes that FFSP_RESERVED_INODE_ID could be tried
    //  to be used as a valid inode_no.
}

static bool create_super_eb(int fd, const ffsp_mkfs_options& options)
{
    std::vector<char> eb_buf;
    eb_buf.reserve(options.erasesize);
    size_t eb_buf_written = 0;

    const uint16_t max_writeops = options.erasesize / options.clustersize;
    const uint32_t eb_cnt = get_eraseblk_cnt(fd, options.erasesize);
    const uint32_t ino_cnt = get_inode_cnt(options.erasesize, options.clustersize, eb_cnt);

    superblock sb = {};
    sb.s_fsid = put_be32(FFSP_FILE_SYSTEM_ID);
    sb.s_flags = put_be32(0);
    sb.s_neraseblocks = put_be32(eb_cnt);
    sb.s_nino = put_be32(ino_cnt);
    sb.s_blocksize = put_be32(options.clustersize);
    sb.s_clustersize = put_be32(options.clustersize);
    sb.s_erasesize = put_be32(options.erasesize);
    sb.s_ninoopen = put_be32(options.ninoopen);
    sb.s_neraseopen = put_be32(options.neraseopen);
    sb.s_nerasereserve = put_be32(options.nerasereserve);
    sb.s_nerasewrites = put_be32(options.nerasewrites);
    memcpy(eb_buf.data(), &sb, sizeof(sb));
    eb_buf_written = sizeof(sb);

    // align eb_usage + ino_map to clustersize
    memset(eb_buf.data() + eb_buf_written, 0, options.clustersize - eb_buf_written);
    eb_buf_written += (options.clustersize - eb_buf_written);

    // The first EB is for the superblock, erase block usage and inodes ids
    eraseblock eb1 = {};
    eb1.e_type = FFSP_EB_SUPER;
    eb1.e_lastwrite = put_be16(0);
    eb1.e_cvalid = put_be16(0);
    eb1.e_writeops = put_be16(0);
    memcpy(eb_buf.data() + eb_buf_written, &eb1, sizeof(eb1));
    eb_buf_written += sizeof(eb1);

    // The second EB is for directory entries
    eraseblock eb2 = {};
    eb2.e_type = FFSP_EB_DENTRY_INODE;
    eb2.e_lastwrite = put_be16(0);
    eb2.e_cvalid = put_be16(1);              // Only the root directory exists...
    eb2.e_writeops = put_be16(max_writeops); // ...but the eb is closed.
    memcpy(eb_buf.data() + eb_buf_written, &eb2, sizeof(eb2));
    eb_buf_written += sizeof(eb2);

    for (uint32_t i = 2; i < eb_cnt; ++i)
    {
        // The remaining erase blocks are empty
        eraseblock eb = {};
        eb.e_type = FFSP_EB_EMPTY;
        eb.e_lastwrite = put_be16(0);
        eb.e_cvalid = put_be16(0);
        eb.e_writeops = put_be16(0);
        memcpy(eb_buf.data() + eb_buf_written, &eb, sizeof(eb));
        eb_buf_written += sizeof(eb);
    }

    // inode id 0 is defined to be invalid
    be32_t cl_0 = put_be32(0xffffffff); // Value does not matter
    memcpy(eb_buf.data() + eb_buf_written, &cl_0, sizeof(cl_0));
    eb_buf_written += sizeof(cl_0);

    // inode id 1 points to the root inode
    be32_t cl_1 = put_be32(options.erasesize / options.clustersize);
    memcpy(eb_buf.data() + eb_buf_written, &cl_1, sizeof(cl_1));
    eb_buf_written += sizeof(cl_1);

    // The rest of the cluster id array will be set to 0 -> no indes.
    memset(eb_buf.data() + eb_buf_written, 0, options.erasesize - eb_buf_written);

    // Write the first erase block into the file.
    uint64_t written_bytes = 0;
    if (!ffsp_write_raw(fd, eb_buf.data(), options.erasesize, 0, written_bytes))
    {
        perror("create_super_eb");
        return false;
    }
    return true;
}

static bool create_inode_eb(int fd, const ffsp_mkfs_options& options)
{
    std::vector<char> eb_buf;
    eb_buf.reserve(options.erasesize);
    size_t eb_buf_written = 0;

    inode root = {};
    root.i_size = put_be64(sizeof(dentry) * 2);
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
    ffsp_update_time(root.i_ctime);
    memcpy(eb_buf.data(), &root, sizeof(root));
    eb_buf_written = sizeof(root);

    // Fill the embedded data with file and/or directory entries
    dentry dot = {}; // "."
    dot.ino = put_be32(1);
    dot.len = static_cast<uint8_t>(strlen("."));
    strcpy(dot.name, ".");
    memcpy(eb_buf.data() + eb_buf_written, &dot, sizeof(dot));
    eb_buf_written += sizeof(dot);

    dentry dotdot = {}; // ".."
    dotdot.ino = put_be32(1);
    dotdot.len = static_cast<uint8_t>(strlen(".."));
    strcpy(dotdot.name, "..");
    memcpy(eb_buf.data() + eb_buf_written, &dotdot, sizeof(dotdot));
    eb_buf_written += sizeof(dotdot);

    uint64_t written_bytes = 0;
    if (!ffsp_write_raw(fd, eb_buf.data(), eb_buf_written, options.erasesize, written_bytes))
    {
        perror("create_inode_eb");
        return false;
    }
    return true;
}

bool ffsp_mkfs(const char* path, const ffsp_mkfs_options& options)
{
#ifdef _WIN32
    int fd = open(path, O_WRONLY);
#else
    int fd = open(path, O_WRONLY | O_SYNC);
#endif
    if (fd == -1)
    {
        perror("ffsp_mkfs open file");
        return false;
    }

    if (!ffsp_fmkfs(fd, options))
    {
        return false;
    }

    if (close(fd) == -1)
    {
        perror("ffsp_mkfs close file");
        return false;
    }
    return true;
}

bool ffsp_fmkfs(int fd, const ffsp_mkfs_options& options)
{
    // Setup the first eraseblock with super, usage and inodemap
    if (!create_super_eb(fd, options))
    {
        return false;
    }

    // Create the inode for the root directory
    if (!create_inode_eb(fd, options))
    {
        return false;
    }
    return true;
}

} // namespace ffsp
