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

#define _GNU_SOURCE

#include "debug.h"

#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static struct ffsp_debug_info {
	int errors;
	uint64_t read_raw;
	uint64_t write_raw;
	uint64_t fuse_read;
	uint64_t fuse_write;
	uint64_t gc_read;
	uint64_t gc_write;
} debug_info = { 0, 0, 0, 0, 0, 0, 0 };

void ffsp_debug_fuse_stat(struct stat *stbuf)
{
	char tmp[4096];

	memset(stbuf, 0, sizeof(*stbuf));
	stbuf->st_dev = 0;
	stbuf->st_ino = 0;
#ifdef _WIN32
	stbuf->st_mode = S_IFREG; // FIXME
#else
	stbuf->st_mode = S_IFREG | S_IRWXU | S_IRWXG | S_IRWXO;
#endif
	stbuf->st_nlink = 1;
	stbuf->st_uid = 0;
	stbuf->st_gid = 0;
	stbuf->st_rdev = 0;
	stbuf->st_size = ffsp_debug_get_info(tmp, sizeof(tmp) - 1);
	stbuf->st_atime = 0;
	stbuf->st_mtime = 0;
	stbuf->st_ctime = 0;
}

int ffsp_debug_get_info(char *buf, size_t count)
{
	return snprintf(buf, count, "ffsp_debug_info:\n"
					"\tLOG_ERROR=%d\n"
					"\tREAD_RAW=%" PRIu64 "\n"
					"\tWRITE_RAW=%" PRIu64 "\n"
					"\tFUSE_READ=%" PRIu64 "\n"
					"\tFUSE_WRITE=%" PRIu64 "\n"
					"\tGC_READ=%" PRIu64 "\n"
					"\tGC_WRITE=%" PRIu64 "\n",
					debug_info.errors,
					debug_info.read_raw,
					debug_info.write_raw,
					debug_info.fuse_read,
					debug_info.fuse_write,
					debug_info.gc_read,
					debug_info.gc_write);
}

void ffsp_debug_update(int type, int val)
{
	switch (type) {
		case FFSP_DEBUG_READ_RAW:
			debug_info.read_raw += val;
			break;
		case FFSP_DEBUG_WRITE_RAW:
			debug_info.write_raw += val;
			break;
		case FFSP_DEBUG_FUSE_READ:
			debug_info.fuse_read += val;
			break;
		case FFSP_DEBUG_FUSE_WRITE:
			debug_info.fuse_write += val;
			break;
		case FFSP_DEBUG_GC_READ:
			debug_info.gc_read += val;
			break;
		case FFSP_DEBUG_GC_WRITE:
			debug_info.gc_write += val;
			break;
		case FFSP_DEBUG_LOG_ERROR:
			debug_info.errors += val;
			break;
		default:
			break;
	}
}
