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

#ifndef SUMMARY_H
#define SUMMARY_H

#include "ffsp.hpp"

be32_t* ffsp_alloc_summary(const ffsp& fs);
void ffsp_delete_summary(be32_t* summary);

void ffsp_summary_list_add(ffsp_summary_list_node& head, be32_t* summary, int eb_type);
void ffsp_summary_list_del(ffsp_summary_list_node& head, int eb_type);
be32_t* ffsp_summary_list_find(ffsp_summary_list_node& head, int eb_type);

bool ffsp_has_summary(int eb_type);
bool ffsp_read_summary(ffsp& fs, uint32_t eb_id, be32_t* summary);
bool ffsp_write_summary(ffsp& fs, uint32_t eb_id, be32_t* summary);
void ffsp_add_summary_ref(be32_t* summary, unsigned int ino_no, int writeops);

#endif /* SUMMARY_H */
