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

#include "inode_cache.hpp"
#include "log.hpp"

#include <vector>

#include <cstddef>

struct ffsp_inode_cache
{
    explicit ffsp_inode_cache(size_t size) : buf{size, nullptr} {}
    std::vector<ffsp_inode*> buf;
};

ffsp_inode_cache* ffsp_inode_cache_init(const ffsp_fs& fs)
{
    return new ffsp_inode_cache(fs.nino);
}

void ffsp_inode_cache_uninit(ffsp_inode_cache* cache)
{
    delete cache;
}

void ffsp_inode_cache_insert(ffsp_inode_cache& cache, ffsp_inode* ino)
{
    cache.buf[get_be32(ino->i_no)] = ino;
}

void ffsp_inode_cache_remove(ffsp_inode_cache& cache, ffsp_inode* ino)
{
    cache.buf[get_be32(ino->i_no)] = nullptr;
}

ffsp_inode* ffsp_inode_cache_find(const ffsp_inode_cache& cache, be32_t ino_no)
{
    return cache.buf[get_be32(ino_no)];
}

std::vector<ffsp_inode*> ffsp_inode_cache_get(const ffsp_inode_cache& cache)
{
    std::vector<ffsp_inode*> ret;
    for (size_t i = 1; i < cache.buf.size(); i++)
        if (cache.buf[i])
            ret.push_back(cache.buf[i]);
    return ret;
}

std::vector<ffsp_inode*> ffsp_inode_cache_get_if(const ffsp_inode_cache& cache,
                                                 const std::function<bool(const ffsp_inode&)>& p)
{
    std::vector<ffsp_inode*> ret;
    for (size_t i = 1; i < cache.buf.size(); i++)
        if (cache.buf[i] && p(*cache.buf[i]))
            ret.push_back(cache.buf[i]);
    return ret;
}
