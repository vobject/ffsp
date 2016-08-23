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

extern "C" {
#include "libffsp/ffsp.h"
#include "libffsp/mkfs.h"
#include "libffsp/mount.h"
}

#include "fuse_ffsp.hpp"

#include "fuse.h"

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

fuse_context* fuse_get_context()
{
    static fuse_context dummy_ctx = {};
    return &dummy_ctx;
}

namespace ffsp_testing
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

bool make_fs(const char* file_path, const ffsp_mkfs_options& opts)
{
    return ffsp_mkfs(file_path, &opts) == 0;
}

bool mount_fs(ffsp& fs, const char* file_path)
{
    return ffsp_mount(&fs, file_path) == 0;
}

bool unmount_fs(ffsp& fs)
{
    ffsp_unmount(&fs);
    return true;
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

bool default_mount_fs(ffsp& fs)
{
    return mount_fs(fs, default_fs_path);
}

bool default_unmount_fs(ffsp& fs)
{
    return unmount_fs(fs);
}

} // namespace ffsp_testing
