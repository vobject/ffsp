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

#include "ffsp_test_utils.hpp"

#include "libffsp/ffsp.hpp"
#include "libffsp/mkfs.hpp"
#include "libffsp/mount.hpp"
#include "libffsp/io_raw.hpp"

#include "fuse_ffsp.hpp"

#include "fuse.h"

#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <unistd.h>

fuse_context* fuse_get_context()
{
    static fuse_context dummy_ctx = {};
    return &dummy_ctx;
}

namespace ffsp
{

namespace test
{

bool create_file(const char* file_path, uint64_t file_size)
{
    FILE* fp = fopen(file_path, "w");
    return fp && (ftruncate(fileno(fp), static_cast<off_t>(file_size)) == 0) && (fclose(fp) == 0);
}

bool remove_file(const char* file_path)
{
    return (remove(file_path) == 0);
}

bool make_fs(const char* file_path, const mkfs_options& opts)
{
    auto* io_ctx = ffsp::io_context_init(file_path);
    return io_ctx && mkfs(*io_ctx, opts) && (ffsp::io_context_uninit(io_ctx), true);
}

bool mount_fs(fs_context** fs, const char* file_path)
{
    *fs = mount(file_path);
    return *fs;
}

bool unmount_fs(fs_context* fs)
{
    unmount(fs);
    return true;
}

bool mkfs_ffsp(const char* program,
               uint32_t clustersize, uint32_t erasesize, uint32_t ninoopen,
               uint32_t neraseopen, uint32_t nerasereserve, uint32_t nerasewrites,
               const char* device)
{
    const std::string command = std::string(program)
                              + " -c " + std::to_string(clustersize)
                              + " -e " + std::to_string(erasesize)
                              + " -i " + std::to_string(ninoopen)
                              + " -o " + std::to_string(neraseopen)
                              + " -r " + std::to_string(nerasereserve)
                              + " -w " + std::to_string(nerasewrites)
                              + " " + device;
    return std::system(command.c_str()) == 0;
}

bool mount_ffsp(const char* program, const char* device, const char* mountpoint)
{
    const std::string command = std::string(program)
                              + " " + device
                              + " " + mountpoint;
    return std::system(command.c_str()) == 0;
}

bool unmount_ffsp(const char* program, const char* mountpoint)
{
    const std::string command = std::string(program)
                              + " " + mountpoint;
    return std::system(command.c_str()) == 0;
}

bool default_create_file()
{
    return create_file(default_fs_path, default_fs_size);
}

bool default_remove_file()
{
    return remove_file(default_fs_path);
}

bool default_make_fs()
{
    return make_fs(default_fs_path, default_mkfs_options);
}

bool default_mount_fs(fs_context** fs)
{
    return mount_fs(fs, default_fs_path);
}

bool default_unmount_fs(fs_context* fs)
{
    return unmount_fs(fs);
}

bool default_mkfs_ffsp()
{
    return mkfs_ffsp(default_bin_mkfs,
                     default_mkfs_options.clustersize,
                     default_mkfs_options.erasesize,
                     default_mkfs_options.ninoopen,
                     default_mkfs_options.neraseopen,
                     default_mkfs_options.nerasereserve,
                     default_mkfs_options.nerasewrites,
                     default_fs_path);
}

bool default_mount_ffsp()
{
    return mount_ffsp(default_bin_mount, default_fs_path, default_dir_mountpoint);
}

bool default_unmount_ffsp()
{
    return unmount_ffsp(default_bin_unmount, default_dir_mountpoint);
}

} // namespace test

} // namespace ffsp
