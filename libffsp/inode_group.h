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

#ifndef INODE_GROUP_H
#define INODE_GROUP_H

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

#include "ffsp.h"

int ffsp_read_inode_group(struct ffsp *fs, unsigned int cl_id,
				struct ffsp_inode **inodes);
int ffsp_get_inode_group(const struct ffsp *fs, struct ffsp_inode **inodes,
		int ino_cnt, struct ffsp_inode **group);
int ffsp_write_inodes(struct ffsp *fs, struct ffsp_inode **inodes, int ino_cnt);

#endif /* INODE_GROUP_H */
