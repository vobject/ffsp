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

#ifndef DEBUG_HPP
#define DEBUG_HPP

#include "ffsp.hpp"

#include <string>
#include <vector>

struct stat;

namespace ffsp
{

#define FFSP_DEBUG_READ_RAW 1
#define FFSP_DEBUG_WRITE_RAW 2
#define FFSP_DEBUG_FUSE_READ 3
#define FFSP_DEBUG_FUSE_WRITE 4
#define FFSP_DEBUG_GC_READ 5
#define FFSP_DEBUG_GC_WRITE 6

void ffsp_debug_update(const ffsp_fs& fs, int type, unsigned long val);

bool ffsp_debug_is_debug_path(ffsp_fs& fs, const char* path);
bool ffsp_debug_getattr(ffsp_fs& fs, const char* path, struct stat& stbuf);
bool ffsp_debug_readdir(ffsp_fs& fs, const char* path, std::vector<std::string>& dirs);
bool ffsp_debug_open(ffsp_fs& fs, const char* path);
bool ffsp_debug_release(ffsp_fs& fs, const char* path);
bool ffsp_debug_read(ffsp_fs& fs, const char* path, char* buf, uint64_t count, uint64_t offset, uint64_t& read);

} // namespace ffsp

#endif /* DEBUG_HPP */
