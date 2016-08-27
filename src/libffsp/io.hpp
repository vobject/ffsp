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

#ifndef IO_H
#define IO_H

#include "ffsp.hpp"

#include <cstddef>

int ffsp_truncate(struct ffsp* fs, struct ffsp_inode* ino,
                  uint64_t length);
int ffsp_read(const struct ffsp* fs, struct ffsp_inode* ino, void* buf,
              size_t count, uint64_t offset);
int ffsp_write(struct ffsp* fs, struct ffsp_inode* ino,
               const void* buf, size_t count, uint64_t offset);

#endif /* IO_H */