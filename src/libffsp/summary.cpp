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

#include "summary.hpp"
#include "debug.hpp"
#include "io_raw.hpp"
#include "log.hpp"

#include <algorithm>

#include <cstdlib>
#include <cstring>

namespace ffsp
{

struct summary
{
    explicit summary(size_t size)
        : buf_{ size, put_be32(0) }
    {
    }
    summary* open()
    {
        if (open_)
            return nullptr;
        open_ = true;
        return this;
    }
    summary* get()
    {
        if (!open_)
            return nullptr;
        return this;
    }
    void close()
    {
        std::fill(buf_.begin(), buf_.end(), put_be32(0));
        open_ = false;
    }
    const void* data() const { return buf_.data(); }
    std::vector<be32_t> buf_;
    bool open_ = false;
};

struct summary_cache
{
    explicit summary_cache(size_t size)
        : dentry_clin{ size }
        , inode_clin{ size }
    {
    }

    /* no other erase block types require a summary */
    summary dentry_clin;
    summary inode_clin;
};

summary_cache* summary_cache_init(const fs_context& fs)
{
    return new summary_cache{ fs.clustersize };
}

void summary_cache_uninit(summary_cache* cache)
{
    delete cache;
}

summary* summary_open(summary_cache& cache, eraseblock_type eb_type)
{
    if (eb_type == eraseblock_type::dentry_clin)
        return cache.dentry_clin.open();
    else if (eb_type == eraseblock_type::file_clin)
        return cache.inode_clin.open();
    else
        return nullptr;
}

summary* summary_get(summary_cache& cache, eraseblock_type eb_type)
{
    if (eb_type == eraseblock_type::dentry_clin)
        return cache.dentry_clin.get();
    else if (eb_type == eraseblock_type::file_clin)
        return cache.inode_clin.get();
    else
        return nullptr;
}

void summary_close(summary_cache& cache, summary* summary)
{
    if (summary == &cache.dentry_clin)
        cache.dentry_clin.close();
    else if (summary == &cache.inode_clin)
        cache.inode_clin.close();
}

bool summary_required(const fs_context& fs, eraseblock_type eb_type)
{
    (void)fs;

    // Erase blocks containing cluster indirect data have an erase block summary
    // section at the end that cannot be used for data. Its size is one cluster.
    return (eb_type == eraseblock_type::dentry_clin) ||
           (eb_type == eraseblock_type::file_clin);
}

bool summary_write(fs_context& fs, summary* summary, eb_id_t eb_id)
{
    uint64_t eb_off = eb_id * fs.erasesize;
    uint64_t summary_off = eb_off + (fs.erasesize - fs.clustersize);

    ssize_t rc = write_raw(*fs.io_ctx, summary->data(), fs.clustersize, summary_off);
    if (rc < 0)
    {
        log().error("ffsp::summary_write(): failed to write erase block summary with error={}", rc);
        return false;
    }
    debug_update(fs, debug_metric::write_raw, static_cast<uint64_t>(rc));
    return true;
}

void summary_add_ref(summary* summary, uint16_t cl_idx, ino_t ino_no)
{
    summary->buf_[cl_idx] = put_be32(ino_no);
}

} // namespace ffsp
