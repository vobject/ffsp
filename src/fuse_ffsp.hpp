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

#ifndef FUSE_FFSP_HPP
#define FUSE_FFSP_HPP

#include "fuse.h"

#include <string>

#ifndef _WIN32
#define FUSE_OFF_T off_t
#endif

namespace ffsp
{

struct fs_context;

namespace fuse
{

void set_params(const std::string& device);

void* init(fuse_conn_info* conn);

void destroy(void* user);

#ifdef _WIN32
int getattr(fs_context& fs, const char* path, struct FUSE_STAT* stbuf);
#else
int getattr(fs_context& fs, const char* path, struct ::stat* stbuf);
#endif

int readdir(fs_context& fs, const char* path, void* buf, fuse_fill_dir_t filler,
            FUSE_OFF_T offset, fuse_file_info* fi);

int open(fs_context& fs, const char* path, fuse_file_info* fi);

int release(fs_context& fs, const char* path, fuse_file_info* fi);

int truncate(fs_context& fs, const char* path, FUSE_OFF_T length);

int read(fs_context& fs, const char* path, char* buf, size_t count,
         FUSE_OFF_T offset, fuse_file_info* fi);

int write(fs_context& fs, const char* path, const char* buf, size_t count,
          FUSE_OFF_T offset, fuse_file_info* fi);

int mknod(fs_context& fs, const char* path, mode_t mode, dev_t device);

int link(fs_context& fs, const char* oldpath, const char* newpath);

int symlink(fs_context& fs, const char* oldpath, const char* newpath);

int readlink(fs_context& fs, const char* path, char* buf, size_t bufsize);

int mkdir(fs_context& fs, const char* path, mode_t mode);

int rmdir(fs_context& fs, const char* path);

int unlink(fs_context& fs, const char* path);

int rename(fs_context& fs, const char* oldpath, const char* newpath);

int utimens(fs_context& fs, const char* path, const struct ::timespec tv[2]);

int chmod(fs_context& fs, const char* path, mode_t mode);

int chown(fs_context& fs, const char* path, uid_t uid, gid_t gid);

int statfs(fs_context& fs, const char* path, struct ::statvfs* sfs);

int flush(fs_context& fs, const char* path, fuse_file_info* fi);

int fsync(fs_context& fs, const char* path, int datasync, fuse_file_info* fi);

} // namespace fuse

} // namespace ffsp

#endif // FUSE_FFSP_HPP
