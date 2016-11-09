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

ffsp_inode* ffsp_allocate_inode(const ffsp_fs& fs);
void ffsp_delete_inode(ffsp_inode* ino);
void* ffsp_inode_data(ffsp_inode* ino);
unsigned int ffsp_get_inode_size(const ffsp_fs& fs, const ffsp_inode* ino);
bool ffsp_is_inode_valid(const ffsp_fs& fs, unsigned int cl_id, const ffsp_inode* ino);

int ffsp_lookup_no(ffsp_fs& fs, ffsp_inode** ino, uint32_t ino_no);
int ffsp_lookup(ffsp_fs& fs, ffsp_inode** ino, const char* path);
int ffsp_flush_inodes(ffsp_fs& fs, bool force);
int ffsp_release_inodes(ffsp_fs& fs);

int ffsp_create(ffsp_fs& fs, const char* path, mode_t mode, uid_t uid, gid_t gid, dev_t device);
int ffsp_symlink(ffsp_fs& fs, const char* oldpath, const char* newpath, uid_t uid, gid_t gid);
int ffsp_readlink(ffsp_fs& fs, const char* path, char* buf, size_t len);
int ffsp_link(ffsp_fs& fs, const char* oldpath, const char* newpath);
int ffsp_unlink(ffsp_fs& fs, const char* path);
int ffsp_rmdir(ffsp_fs& fs, const char* path);
int ffsp_rename(ffsp_fs& fs, const char* oldpath, const char* newpath);

void ffsp_mark_dirty(ffsp_fs& fs, ffsp_inode* ino);
void ffsp_reset_dirty(ffsp_fs& fs, ffsp_inode* ino);

int ffsp_cache_dir(ffsp_fs& fs, ffsp_inode* ino, ffsp_dentry** dent_buf, int* dentry_cnt);

void ffsp_invalidate_ind_ptr(ffsp_fs& fs, const be32_t* ind_ptr, int cnt, int ind_type);

} // namespace ffsp

#endif /* INODE_HPP */
