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

#ifndef DEBUG_H
#define DEBUG_H

#include "ffsp.hpp"

#include <cstddef>

struct stat;

#define FFSP_DEBUG_FILE "/.FFSP"

#define FFSP_DEBUG_READ_RAW 1
#define FFSP_DEBUG_WRITE_RAW 2
#define FFSP_DEBUG_FUSE_READ 3
#define FFSP_DEBUG_FUSE_WRITE 4
#define FFSP_DEBUG_GC_READ 5
#define FFSP_DEBUG_GC_WRITE 6
#define FFSP_DEBUG_LOG_ERROR 7

void ffsp_debug_fuse_stat(struct stat* stbuf);
int ffsp_debug_get_info(char* buf, size_t count);
void ffsp_debug_update(int type, unsigned long val);

/* TODO: Introduce some kind of 'debuginfo context':
 * 	ffsp_debug_set_context(FFSP_DEBUG_CTX_GC);
 * 	ffsp_write(); <--- will log to write_raw AND gc_write.
 * 	ffsp_debug_unset_context(FFSP_DEBUG_CTX_GC);
 */

#endif /* DEBUG_H */
