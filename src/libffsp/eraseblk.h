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

#ifndef ERASEBLK_H
#define ERASEBLK_H

#include "ffsp.h"

#include <stdint.h>

int ffsp_eb_get_cvalid(const struct ffsp *fs, unsigned int eb_id);
void ffsp_eb_inc_cvalid(struct ffsp *fs, unsigned int eb_id);
void ffsp_eb_dec_cvalid(struct ffsp *fs, unsigned int eb_id);

unsigned int ffsp_emtpy_eraseblk_count(const struct ffsp *fs);
int ffsp_get_eraseblk_type(const struct ffsp *fs, int data_type,
		uint32_t mode);

int ffsp_find_writable_cluster(struct ffsp *fs, int eb_type,
				uint32_t *eb_id, uint32_t *cl_id);
void ffsp_commit_write_operation(struct ffsp *fs, int eb_type,
				uint32_t eb_id, be32_t ino_no);
void ffsp_close_eraseblks(struct ffsp *fs);
int ffsp_write_meta_data(const struct ffsp *fs);

#endif /* ERASEBLK_H */
