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

#ifndef IO_HPP
#define IO_HPP

#include "ffsp.hpp"

#include <cstddef>

int ffsp_truncate(ffsp_fs& fs, ffsp_inode* ino, uint64_t length);
int ffsp_read(ffsp_fs& fs, ffsp_inode* ino, char* buf, size_t count, uint64_t offset);
int ffsp_write(ffsp_fs& fs, ffsp_inode* ino, const char* buf, size_t count, uint64_t offset);

#endif /* IO_HPP */
