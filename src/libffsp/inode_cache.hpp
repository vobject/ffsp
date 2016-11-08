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

#ifndef INODE_CACHE_HPP
#define INODE_CACHE_HPP

#include "ffsp.hpp"

#include <functional>
#include <vector>

struct ffsp_inode_cache;

ffsp_inode_cache* ffsp_inode_cache_init(const ffsp_fs& fs);
void ffsp_inode_cache_uninit(ffsp_inode_cache* cache);

void ffsp_inode_cache_insert(ffsp_inode_cache& cache, ffsp_inode* ino);
void ffsp_inode_cache_remove(ffsp_inode_cache& cache, ffsp_inode* ino);
ffsp_inode* ffsp_inode_cache_find(const ffsp_inode_cache& cache, be32_t ino_no);
std::vector<ffsp_inode*> ffsp_inode_cache_get(const ffsp_inode_cache& cache);
std::vector<ffsp_inode*> ffsp_inode_cache_get_if(const ffsp_inode_cache& cache,
                                                 const std::function<bool(const ffsp_inode&)>& p);

#endif /* INODE_CACHE_HPP */
