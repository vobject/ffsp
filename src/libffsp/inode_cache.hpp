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

namespace ffsp
{

struct inode_cache;

inode_cache* ffsp_inode_cache_init(const fs_context& fs);
void ffsp_inode_cache_uninit(inode_cache* cache);

void ffsp_inode_cache_insert(inode_cache& cache, inode* ino);
void ffsp_inode_cache_remove(inode_cache& cache, inode* ino);
inode* ffsp_inode_cache_find(const inode_cache& cache, be32_t ino_no);
std::vector<inode*> ffsp_inode_cache_get(const inode_cache& cache);
std::vector<inode*> ffsp_inode_cache_get_if(const inode_cache& cache,
                                            const std::function<bool(const inode&)>& p);

} // namespace ffsp

#endif /* INODE_CACHE_HPP */
