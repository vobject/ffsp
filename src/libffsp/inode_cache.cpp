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

#include "log.hpp"
#include "inode_cache.hpp"

#include <cstdlib>
#include <cstring>

void ffsp_inode_cache_init(const struct ffsp* fs,
                           struct ffsp_inode_cache** cache)
{
    int buf_size;
    void* buf;

    *cache = (struct ffsp_inode_cache*)malloc(sizeof(struct ffsp_inode_cache));
    if (!*cache)
    {
        FFSP_ERROR("malloc(ffsp_inode_cache) failed");
        abort();
    }

    buf_size = fs->nino * sizeof(struct ffsp_inode*);
    buf = malloc(buf_size);
    if (!buf)
    {
        FFSP_ERROR("malloc(ffsp_inode_cache buffer) failed");
        abort();
    }
    memset(buf, FFSP_INVALID_INO_NO, buf_size);

    (*cache)->count = fs->nino;
    (*cache)->valid = 0;
    (*cache)->buf = (struct ffsp_inode**)buf;
}

void ffsp_inode_cache_uninit(struct ffsp_inode_cache** cache)
{
    free((*cache)->buf);
    free(*cache);
    *cache = NULL;
}

void ffsp_inode_cache_insert(struct ffsp_inode_cache* cache,
                             struct ffsp_inode* ino)
{
    cache->buf[get_be32(ino->i_no)] = ino;
    cache->valid++;
}

int ffsp_inode_cache_entry_count(struct ffsp_inode_cache* cache)
{
    return cache->valid;
}

void ffsp_inode_cache_remove(struct ffsp_inode_cache* cache,
                             struct ffsp_inode* ino)
{
    cache->buf[get_be32(ino->i_no)] = FFSP_INVALID_INO_NO;
    cache->valid--;
}

struct ffsp_inode* ffsp_inode_cache_find(struct ffsp_inode_cache* cache,
                                         be32_t ino_no)
{
    return cache->buf[get_be32(ino_no)];
}

void ffsp_inode_cache_init_status(struct ffsp_inode_cache_status* status)
{
    status->last_valid_index = 0;
}

struct ffsp_inode* ffsp_inode_cache_next(struct ffsp_inode_cache* cache,
                                         struct ffsp_inode_cache_status* status)
{
    int next_index;

    next_index = status->last_valid_index;
    if (next_index)
        next_index++;

    for (; next_index < cache->count; next_index++)
    {
        if (cache->buf[next_index] != FFSP_INVALID_INO_NO)
        {
            status->last_valid_index = next_index;
            return cache->buf[next_index];
        }
    }
    return NULL;
}
