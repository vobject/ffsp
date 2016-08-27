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

#include <cstdlib>
#include <cstring>

void ffsp_inode_cache_init(const ffsp* fs,
                           ffsp_inode_cache** cache)
{
    int buf_size;
    void* buf;

    *cache = (ffsp_inode_cache*)malloc(sizeof(ffsp_inode_cache));
    if (!*cache)
    {
        ffsp_log().critical("malloc(ffsp_inode_cache) failed");
        abort();
    }

    buf_size = fs->nino * sizeof(ffsp_inode*);
    buf = malloc(buf_size);
    if (!buf)
    {
        ffsp_log().critical("malloc(ffsp_inode_cache buffer) failed");
        abort();
    }
    memset(buf, FFSP_INVALID_INO_NO, buf_size);

    (*cache)->count = fs->nino;
    (*cache)->valid = 0;
    (*cache)->buf = (ffsp_inode**)buf;
}

void ffsp_inode_cache_uninit(ffsp_inode_cache** cache)
{
    free((*cache)->buf);
    free(*cache);
    *cache = NULL;
}

void ffsp_inode_cache_insert(ffsp_inode_cache* cache,
                             ffsp_inode* ino)
{
    cache->buf[get_be32(ino->i_no)] = ino;
    cache->valid++;
}

int ffsp_inode_cache_entry_count(ffsp_inode_cache* cache)
{
    return cache->valid;
}

void ffsp_inode_cache_remove(ffsp_inode_cache* cache,
                             ffsp_inode* ino)
{
    cache->buf[get_be32(ino->i_no)] = FFSP_INVALID_INO_NO;
    cache->valid--;
}

ffsp_inode* ffsp_inode_cache_find(ffsp_inode_cache* cache,
                                  be32_t ino_no)
{
    return cache->buf[get_be32(ino_no)];
}

void ffsp_inode_cache_init_status(ffsp_inode_cache_status* status)
{
    status->last_valid_index = 0;
}

ffsp_inode* ffsp_inode_cache_next(ffsp_inode_cache* cache,
                                  ffsp_inode_cache_status* status)
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
