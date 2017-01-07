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

#ifndef INODE_HPP
#define INODE_HPP

#include "ffsp.hpp"

#include <vector>

#include <sys/types.h>

#ifdef _WIN32
#include <fuse_win.h>
#endif

namespace ffsp
{

inode* allocate_inode(const fs_context& fs);
void delete_inode(inode* ino);
void* inode_data(const inode& ino);
uint64_t get_inode_size(const fs_context& fs, const inode& ino);
bool is_inode_valid(const fs_context& fs, cl_id_t cl_id, const inode& ino);
//bool is_inode_data_type(const fs_context& fs, const inode* ino);

int lookup_no(fs_context& fs, inode** ino, ino_t ino_no);
int lookup(fs_context& fs, inode** ino, const char* path);
int flush_inodes(fs_context& fs, bool force);
int release_inodes(fs_context& fs);

int create(fs_context& fs, const char* path, mode_t mode, uid_t uid, gid_t gid, dev_t device);
int symlink(fs_context& fs, const char* oldpath, const char* newpath, uid_t uid, gid_t gid);
int readlink(fs_context& fs, const char* path, char* buf, size_t len);
int link(fs_context& fs, const char* oldpath, const char* newpath);
int unlink(fs_context& fs, const char* path);
int rmdir(fs_context& fs, const char* path);
int rename(fs_context& fs, const char* oldpath, const char* newpath);

void mark_dirty(fs_context& fs, const inode& ino);
void reset_dirty(fs_context& fs, const inode& ino);

//int cache_dir(fs_context& fs, inode* ino, dentry** dent_buf, int* dentry_cnt);
int read_dir(fs_context& fs, const inode& ino, std::vector<dentry>& dentries);

void invalidate_ind_ptr(fs_context& fs, const be32_t* ind_ptr, int cnt, inode_data_type ind_type);

} // namespace ffsp

#endif /* INODE_HPP */
