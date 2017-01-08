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

#ifndef IO_BACKEND_HPP
#define IO_BACKEND_HPP

#include <cstdint>

#include <sys/types.h>

#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

namespace ffsp
{

struct io_backend;

io_backend* io_backend_init(const char* path);
io_backend* io_backend_init(size_t size);
void io_backend_uninit(io_backend* ctx);

uint64_t io_backend_size(const io_backend& ctx);
ssize_t io_backend_read(io_backend& ctx, void* buf, size_t nbyte, off_t offset);
ssize_t io_backend_write(io_backend& ctx, const void* buf, size_t nbyte, off_t offset);

} // namespace ffsp

#endif /* IO_BACKEND_HPP */
