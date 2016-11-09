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

#include <sys/types.h>

#ifdef _WIN32
#include <fuse_win.h>
#endif

namespace ffsp
{

inode* ffsp_allocate_inode(const fs_context& fs);
void ffsp_delete_inode(inode* ino);
void* ffsp_inode_data(inode* ino);
unsigned int ffsp_get_inode_size(const fs_context& fs, const inode* ino);
bool ffsp_is_inode_valid(const fs_context& fs, unsigned int cl_id, const inode* ino);

int ffsp_lookup_no(fs_context& fs, inode** ino, uint32_t ino_no);
int ffsp_lookup(fs_context& fs, inode** ino, const char* path);
int ffsp_flush_inodes(fs_context& fs, bool force);
int ffsp_release_inodes(fs_context& fs);

int ffsp_create(fs_context& fs, const char* path, mode_t mode, uid_t uid, gid_t gid, dev_t device);
int ffsp_symlink(fs_context& fs, const char* oldpath, const char* newpath, uid_t uid, gid_t gid);
int ffsp_readlink(fs_context& fs, const char* path, char* buf, size_t len);
int ffsp_link(fs_context& fs, const char* oldpath, const char* newpath);
int ffsp_unlink(fs_context& fs, const char* path);
int ffsp_rmdir(fs_context& fs, const char* path);
int ffsp_rename(fs_context& fs, const char* oldpath, const char* newpath);

void ffsp_mark_dirty(fs_context& fs, inode* ino);
void ffsp_reset_dirty(fs_context& fs, inode* ino);

int ffsp_cache_dir(fs_context& fs, inode* ino, dentry** dent_buf, int* dentry_cnt);

void ffsp_invalidate_ind_ptr(fs_context& fs, const be32_t* ind_ptr, int cnt, int ind_type);

} // namespace ffsp

#endif /* INODE_HPP */
