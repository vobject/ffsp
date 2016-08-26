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
#include "io_raw.hpp"
#include "log.hpp"

#include <cstdlib>
#include <cstring>

be32_t* ffsp_alloc_summary(const struct ffsp* fs)
{
    be32_t* summary;

    summary = (be32_t*)malloc(fs->clustersize);
    if (!summary)
    {
        FFSP_ERROR("malloc(summary) failed");
        abort();
    }
    memset(summary, 0, fs->clustersize);
    return summary;
}

void ffsp_delete_summary(be32_t* summary)
{
    free(summary);
}

void ffsp_summary_list_add(struct ffsp_summary_list_node* head,
                           be32_t* summary, int eb_type)
{
    struct ffsp_summary_list_node* node;

    node = (struct ffsp_summary_list_node*)malloc(sizeof(struct ffsp_summary_list_node));
    if (!node)
    {
        FFSP_ERROR("malloc(summary list node) failed");
        abort();
    }
    node->eb_type = eb_type;
    node->summary = summary;
    node->next = head->next;
    head->next = node;
}

void ffsp_summary_list_del(struct ffsp_summary_list_node* head,
                           int eb_type)
{
    struct ffsp_summary_list_node* tmp;
    struct ffsp_summary_list_node* node = head;

    while (node->next && (node->next->eb_type != eb_type))
        node = node->next;

    if (node->next)
    {
        tmp = node->next;
        node->next = node->next->next;
        free(tmp);
    }
}

be32_t* ffsp_summary_list_find(struct ffsp_summary_list_node* head,
                               int eb_type)
{
    struct ffsp_summary_list_node* node = head;

    while (node->next && (node->next->eb_type != eb_type))
        node = node->next;

    return node->next ? node->next->summary : NULL;
}

bool ffsp_has_summary(int eb_type)
{
    /*
	 * Erase blocks containing cluster indirect data always
	 * have an erase block summary section that cannot be used for
	 * data at the end. Its size is always one cluster.
	 */
    switch (eb_type)
    {
        case FFSP_EB_DENTRY_INODE:
        case FFSP_EB_FILE_INODE:
            return false;
        case FFSP_EB_DENTRY_CLIN:
        case FFSP_EB_FILE_CLIN:
            return true;
    }
    FFSP_ERROR("ffsp_has_summary(): Invalid erase block type.");
    return false;
}

int ffsp_read_summary(const struct ffsp* fs, uint32_t eb_id, be32_t* summary)
{
    int rc;
    uint64_t eb_off;
    uint64_t summary_off;

    eb_off = eb_id * fs->erasesize;
    summary_off = eb_off + fs->erasesize - fs->clustersize;

    rc = ffsp_read_raw(fs->fd, summary, fs->clustersize, summary_off);
    if (rc < 0)
        FFSP_ERROR("failed to read erase block summary");
    return rc;
}

int ffsp_write_summary(const struct ffsp* fs, uint32_t eb_id, be32_t* summary)
{
    int rc;
    uint64_t eb_off;
    uint64_t summary_off;

    eb_off = eb_id * fs->erasesize;
    summary_off = eb_off + (fs->erasesize - fs->clustersize);

    rc = ffsp_write_raw(fs->fd, summary, fs->clustersize, summary_off);
    if (rc < 0)
        FFSP_ERROR("Failed to write summary for erase block.");
    return rc;
}

void ffsp_add_summary_ref(be32_t* summary, unsigned int ino_no, int writeops)
{
    summary[writeops] = put_be32(ino_no);
}
