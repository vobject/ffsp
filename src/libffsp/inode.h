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

#ifndef INODE_H
#define INODE_H

#include "ffsp.h"

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#include <fuse_win.h>
#endif

struct ffsp_inode* ffsp_allocate_inode(const struct ffsp* fs);
void ffsp_delete_inode(struct ffsp_inode* ino);
void* ffsp_inode_data(struct ffsp_inode* ino);
int ffsp_get_inode_size(const struct ffsp* fs, const struct ffsp_inode* ino);
bool ffsp_is_inode_valid(const struct ffsp* fs, unsigned int cl_id,
                         const struct ffsp_inode* ino);

int ffsp_lookup_no(struct ffsp* fs, struct ffsp_inode** ino, uint32_t ino_no);
int ffsp_lookup(struct ffsp* fs, struct ffsp_inode** ino,
                const char* path);
int ffsp_flush_inodes(struct ffsp* fs, bool force);
int ffsp_release_inodes(struct ffsp* fs);

int ffsp_create(struct ffsp* fs, const char* path, mode_t mode,
                uid_t uid, gid_t gid, dev_t device);
int ffsp_symlink(struct ffsp* fs, const char* oldpath, const char* newpath,
                 uid_t uid, gid_t gid);
int ffsp_readlink(struct ffsp* fs, const char* path, char* buf, size_t len);
int ffsp_link(struct ffsp* fs, const char* oldpath, const char* newpath);
int ffsp_unlink(struct ffsp* fs, const char* path);
int ffsp_rmdir(struct ffsp* fs, const char* path);
int ffsp_rename(struct ffsp* fs, const char* oldpath, const char* newpath);

void ffsp_mark_dirty(struct ffsp* fs, struct ffsp_inode* ino);
void ffsp_reset_dirty(struct ffsp* fs, struct ffsp_inode* ino);

int ffsp_cache_dir(const struct ffsp* fs, struct ffsp_inode* ino,
                   struct ffsp_dentry** dent_buf, int* dentry_cnt);

void ffsp_invalidate_ind_ptr(struct ffsp* fs, const be32_t* ind_ptr,
                             int cnt, int ind_type);

#endif /* INODE_H */
