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

struct ffsp;

namespace ffsp_testing
{

bool create_file(const char* file_path, uint64_t file_size);
bool remove_file(const char* file_path);
bool make_fs(const char* file_path, const ffsp_mkfs_options& opts);

bool mount_fs(ffsp& fs, const char* file_path);
bool unmount_fs(ffsp& fs);

const constexpr char* const default_fs_path{ "/tmp/test.ffsp_fs" };
const constexpr uint64_t default_fs_size{ 1024 * 1024 * 128 };           // 128 MiB
const constexpr ffsp_mkfs_options default_mkfs_options{ 1024 * 32,       // cluster
                                                        1024 * 1024 * 4, // eraseblock
                                                        128,             // open inodes
                                                        5,               // open eraseblocks
                                                        3,               // reserved eraseblocks
                                                        5 };             // gc trigger

bool default_create_file();
bool default_remove_file();
bool default_make_fs();

bool default_mount_fs(ffsp& fs);
bool default_unmount_fs(ffsp& fs);

} // namespace ffsp_testing

#endif // FFSP_TEST_UTILS_HPP
