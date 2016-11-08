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

#ifndef SUMMARY_HPP
#define SUMMARY_HPP

#include "ffsp.hpp"

struct ffsp_summary;

ffsp_summary_cache* ffsp_summary_cache_init(const ffsp_fs& fs);
void ffsp_summary_cache_uninit(ffsp_summary_cache* cache);

ffsp_summary* ffsp_summary_open(ffsp_summary_cache& cache, ffsp_eraseblk_type eb_type);
ffsp_summary* ffsp_summary_get(ffsp_summary_cache& cache, ffsp_eraseblk_type eb_type);
void ffsp_summary_close(ffsp_summary_cache& cache, ffsp_summary* summary);

bool ffsp_summary_required(const ffsp_fs& fs, uint32_t eb_id);
bool ffsp_summary_write(const ffsp_fs& fs, ffsp_summary* summary, uint32_t eb_id);
void ffsp_summary_add_ref(ffsp_summary* summary, uint16_t cl_idx, uint32_t ino_no);


//be32_t* ffsp_summary_list_add(const ffsp_fs& fs, ffsp_summary_list_node& head, ffsp_eraseblk_type eb_type);
//void ffsp_summary_list_del(ffsp_summary_list_node& head, ffsp_eraseblk_type eb_type);
//be32_t* ffsp_summary_list_find(ffsp_summary_list_node& head, ffsp_eraseblk_type eb_type);

//bool ffsp_summary_required0(ffsp_eraseblk_type eb_type);
//bool ffsp_summary_write0(ffsp_fs& fs, uint32_t eb_id, be32_t* summary);
//void ffsp_summary_add_ref0(be32_t* summary, unsigned int ino_no, int writeops);

#endif /* SUMMARY_HPP */
