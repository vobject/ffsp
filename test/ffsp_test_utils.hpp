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

#ifndef FFSP_TEST_UTILS_HPP
#define FFSP_TEST_UTILS_HPP

#include "libffsp/mkfs.hpp"

namespace ffsp
{

namespace ffsp_testing
{

bool create_file(const char* file_path, uint64_t file_size);
bool remove_file(const char* file_path);

bool make_fs(const char* file_path, const ffsp_mkfs_options& opts);

bool mount_fs(ffsp_fs& fs, const char* file_path);
bool unmount_fs(ffsp_fs& fs);

bool mkfs_ffsp(const char* program,
               uint32_t clustersize, uint32_t erasesize, uint32_t ninoopen,
               uint32_t neraseopen, uint32_t nerasereserve, uint32_t nerasewrites,
               const char* device);
bool mount_ffsp(const char* program, const char* device, const char* mountpoint);
bool unmount_ffsp(const char* program, const char* mountpoint);

/*
    Max number of inodes:

    number of erase blocks = file system size / erase block size
    number of indes = ((erase block size)
                       - (size of cluster)
                       - (number of erase blocks * size of erase block struct)
                       - (size of (root) inode id))
                      / (size of inode id)

    Default file system parameters for small files:
    - file system size = 128 MiB
    - erase block size = 4 MiB
    - cluster size = 32 KiB
    - number of erase blocks = 32
    - number of inodes = (4194304 - 32768 - 32*8 - 4) / 4 = 1040319
*/
const constexpr char* const default_fs_path{ "/tmp/test.ffsp_fs" };
const constexpr uint64_t default_fs_size{ 1024 * 1024 * 128 };           // 128 MiB
const constexpr ffsp_mkfs_options default_mkfs_options{ 1024 * 32,       // cluster
                                                        1024 * 1024 * 4, // eraseblock
                                                        128,             // open inodes
                                                        5,               // open eraseblocks
                                                        3,               // reserved eraseblocks
                                                        5 };             // gc trigger

const constexpr char* const default_bin_mkfs{ "./mkfs.ffsp" };
const constexpr char* const default_bin_mount{ "./mount.ffsp" };
const constexpr char* const default_bin_unmount{ "fusermount -u" };
const constexpr char* const default_dir_mountpoint{ "mnt" };

bool default_create_file();
bool default_remove_file();

bool default_make_fs();

bool default_mount_fs(ffsp_fs& fs);
bool default_unmount_fs(ffsp_fs& fs);

bool default_mkfs_ffsp();
bool default_mount_ffsp();
bool default_unmount_ffsp();

} // namespace ffsp_testing

} // namespace ffsp

#endif // FFSP_TEST_UTILS_HPP
