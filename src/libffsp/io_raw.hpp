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

#ifndef IO_RAW_HPP
#define IO_RAW_HPP

#include <cstdint>

#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#include <sys/types.h>
#endif

namespace ffsp
{

struct io_context;

io_context* io_context_init(const char* path);
io_context* io_context_init(size_t size);
void io_context_uninit(io_context* ctx);
uint64_t io_context_size(const io_context& ctx);

ssize_t read_raw(io_context& ctx, void* buf, uint64_t nbyte, uint64_t offset);
ssize_t write_raw(io_context& ctx, const void* buf, uint64_t nbyte, uint64_t offset);

} // namespace ffsp

#endif /* IO_RAW_HPP */
