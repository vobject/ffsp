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

//be32_t* ffsp_summary_list_add(const ffsp_fs& fs, ffsp_summary_list_node& head, ffsp_eraseblk_type eb_type)
//{
//    ffsp_summary_list_node* node = new ffsp_summary_list_node;
//    node->eb_type = eb_type;
//    node->summary = new be32_t[fs.clustersize];
//    memset(node->summary, 0, fs.clustersize);
//    node->next = head.next;
//    head.next = node;
//    return node->summary;
//}

//void ffsp_summary_list_del(ffsp_summary_list_node& head, ffsp_eraseblk_type eb_type)
//{
//    ffsp_summary_list_node* node = &head;

//    while (node->next && (node->next->eb_type != eb_type))
//        node = node->next;

//    if (node->next)
//    {
//        ffsp_summary_list_node* tmp = node->next;
//        node->next = node->next->next;

//        delete [] tmp->summary; // delete the summary
//        delete tmp; // delete the node
//    }
//}

//be32_t* ffsp_summary_list_find(ffsp_summary_list_node& head, ffsp_eraseblk_type eb_type)
//{
//    ffsp_summary_list_node* node = &head;

//    while (node->next && (node->next->eb_type != eb_type))
//        node = node->next;

//    return node->next ? node->next->summary : nullptr;
//}

//bool ffsp_summary_required0(ffsp_eraseblk_type eb_type)
//{
//    /*
//     * Erase blocks containing cluster indirect data always
//     * have an erase block summary section that cannot be used for
//     * data at the end. Its size is always one cluster.
//     */
//    switch (eb_type)
//    {
//        case FFSP_EB_DENTRY_INODE:
//        case FFSP_EB_FILE_INODE:
//            return false;
//        case FFSP_EB_DENTRY_CLIN:
//        case FFSP_EB_FILE_CLIN:
//            return true;
//    }
//    ffsp_log().error("ffsp_has_summary(): Invalid erase block type {}", eb_type);
//    return false;
//}

//bool ffsp_summary_write0(ffsp_fs& fs, uint32_t eb_id, be32_t* summary)
//{
//    uint64_t eb_off = eb_id * fs.erasesize;
//    uint64_t summary_off = eb_off + (fs.erasesize - fs.clustersize);

//    uint64_t written_bytes = 0;
//    if (!ffsp_write_raw(fs.fd, summary, fs.clustersize, summary_off, written_bytes))
//    {
//        ffsp_log().error("failed to write erase block summary");
//        return false;
//    }
//    ffsp_debug_update(fs, FFSP_DEBUG_WRITE_RAW, written_bytes);
//    return true;
//}

//void ffsp_summary_add_ref0(be32_t* summary, unsigned int ino_no, int writeops)
//{
//    summary[writeops] = put_be32(ino_no);
//}


struct ffsp_summary
{
    explicit ffsp_summary(size_t size) : buf_{size, put_be32(0)} {}
    ffsp_summary* open() { if (open_) { return nullptr; } open_ = true; return this; }
    ffsp_summary* get() { if (!open_) { return nullptr; } return this; }
    void close() { std::fill(buf_.begin(), buf_.end(), put_be32(0)); open_ = false; }
    const void* data() const { return buf_.data(); }
    std::vector<be32_t> buf_;
    bool open_ = false;
};

struct ffsp_summary_cache
{
    explicit ffsp_summary_cache(size_t size) : dentry_clin{size}, inode_clin{size} {}

    /* no other erase block types require a summary */
    ffsp_summary dentry_clin;
    ffsp_summary inode_clin;
};

ffsp_summary_cache* ffsp_summary_cache_init(const ffsp_fs& fs)
{
    return new ffsp_summary_cache{fs.clustersize};
}

void ffsp_summary_cache_uninit(ffsp_summary_cache* cache)
{
    delete cache;
}

ffsp_summary* ffsp_summary_open(ffsp_summary_cache& cache, ffsp_eraseblk_type eb_type)
{
    if (eb_type == FFSP_EB_DENTRY_CLIN)
        return cache.dentry_clin.open();
    else if (eb_type == FFSP_EB_FILE_CLIN)
        return cache.inode_clin.open();
    else
        return nullptr;
}

ffsp_summary* ffsp_summary_get(ffsp_summary_cache& cache, ffsp_eraseblk_type eb_type)
{
    if (eb_type == FFSP_EB_DENTRY_CLIN)
        return cache.dentry_clin.get();
    else if (eb_type == FFSP_EB_FILE_CLIN)
        return cache.inode_clin.get();
    else
        return nullptr;
}

void ffsp_summary_close(ffsp_summary_cache& cache, ffsp_summary* summary)
{
    if (summary == &cache.dentry_clin)
        cache.dentry_clin.close();
    else if (summary == &cache.inode_clin)
        cache.inode_clin.close();
}

bool ffsp_summary_required(const ffsp_fs& fs, uint32_t eb_id)
{
    /*
     * Erase blocks containing cluster indirect data always
     * have an erase block summary section that cannot be used for
     * data at the end. Its size is always one cluster.
     */
    const ffsp_eraseblk_type eb_type = fs.eb_usage[eb_id].e_type;
    return (eb_type == FFSP_EB_DENTRY_CLIN) ||
           (eb_type == FFSP_EB_FILE_CLIN);
}

bool ffsp_summary_write(const ffsp_fs& fs, ffsp_summary* summary, uint32_t eb_id)
{
    uint64_t eb_off = eb_id * fs.erasesize;
    uint64_t summary_off = eb_off + (fs.erasesize - fs.clustersize);

    uint64_t written_bytes = 0;
    if (!ffsp_write_raw(fs.fd, summary->data(), fs.clustersize, summary_off, written_bytes))
    {
        ffsp_log().error("ffsp_summary_write(): failed to write erase block summary");
        return false;
    }
    ffsp_debug_update(fs, FFSP_DEBUG_WRITE_RAW, written_bytes);
    return true;

}

void ffsp_summary_add_ref(ffsp_summary* summary, uint16_t cl_idx, uint32_t ino_no)
{
    summary->buf_[cl_idx] = put_be32(ino_no);
}
