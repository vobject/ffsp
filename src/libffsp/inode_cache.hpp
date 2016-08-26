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

#ifndef INODE_CACHE_H
#define INODE_CACHE_H

#include "ffsp.hpp"

struct ffsp_inode_cache_status
{
    int last_valid_index;
};

struct ffsp_inode_cache
{
    int count;
    int valid;
    struct ffsp_inode** buf;
};

void ffsp_inode_cache_init(const struct ffsp* fs, struct ffsp_inode_cache** cache);
void ffsp_inode_cache_uninit(struct ffsp_inode_cache** cache);

int ffsp_inode_cache_entry_count(struct ffsp_inode_cache* cache);

void ffsp_inode_cache_insert(struct ffsp_inode_cache* cache, struct ffsp_inode* ino);
void ffsp_inode_cache_remove(struct ffsp_inode_cache* cache, struct ffsp_inode* ino);
struct ffsp_inode* ffsp_inode_cache_find(struct ffsp_inode_cache* cache, be32_t ino_no);

void ffsp_inode_cache_init_status(struct ffsp_inode_cache_status* status);
struct ffsp_inode* ffsp_inode_cache_next(struct ffsp_inode_cache* cache,
                                         struct ffsp_inode_cache_status* status);

#endif /* INODE_CACHE_H */
