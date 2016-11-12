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

#ifndef ERASEBLK_HPP
#define ERASEBLK_HPP

#include "ffsp.hpp"

#include <sys/types.h>

namespace ffsp
{

int eb_get_cvalid(const fs_context& fs, uint32_t eb_id);
void eb_inc_cvalid(fs_context& fs, uint32_t eb_id);
void eb_dec_cvalid(fs_context& fs, uint32_t eb_id);

unsigned int emtpy_eraseblk_count(const fs_context& fs);
eraseblock_type get_eraseblk_type(const fs_context& fs, int data_type, uint32_t mode);

int find_writable_cluster(fs_context& fs, eraseblock_type eb_type, uint32_t& eb_id, uint32_t& cl_id);
void commit_write_operation(fs_context& fs, eraseblock_type eb_type, uint32_t eb_id, be32_t ino_no);
void close_eraseblks(fs_context& fs);
ssize_t write_meta_data(fs_context& fs);

} // namespace ffsp

#endif /* ERASEBLK_HPP */
