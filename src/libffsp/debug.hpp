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

enum class debug_metric
{
    read_raw,
    write_raw,
    fuse_read,
    fuse_write,
    gc_read,
    gc_write,
};

void debug_update(const fs_context& fs, debug_metric type, unsigned long val);

bool is_debug_path(fs_context& fs, const char* path);
bool debug_getattr(fs_context& fs, const char* path, struct ::stat& stbuf);
bool debug_readdir(fs_context& fs, const char* path, std::vector<std::string>& dirs);
bool debug_open(fs_context& fs, const char* path);
bool debug_release(fs_context& fs, const char* path);
bool debug_read(fs_context& fs, const char* path, char* buf, uint64_t count, uint64_t offset, uint64_t& read);

} // namespace ffsp

#endif /* DEBUG_HPP */
