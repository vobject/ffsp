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

namespace ffsp
{

struct summary;
struct summary_cache;

summary_cache* summary_cache_init(const fs_context& fs);
void summary_cache_uninit(summary_cache* cache);

summary* summary_open(summary_cache& cache, eraseblock_type eb_type);
summary* summary_get(summary_cache& cache, eraseblock_type eb_type);
void summary_close(summary_cache& cache, summary* summary);

bool summary_required(const fs_context& fs, uint32_t eb_id);
bool summary_write(fs_context& fs, summary* summary, uint32_t eb_id);
void summary_add_ref(summary* summary, uint16_t cl_idx, uint32_t ino_no);

} // namespace ffsp

#endif /* SUMMARY_HPP */
