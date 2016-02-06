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

#include "io_raw.h"
#include "log.h"
#include "debug.h"

#include <limits.h>
#include <errno.h>
#include <stdint.h>

#ifdef WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#define SSIZE_MAX MAXSSIZE_T
#else
#include <unistd.h>
#endif

int ffsp_read_raw(int fd, void *buf, size_t count, off_t offset)
{
	ssize_t rc;

	if (count > SSIZE_MAX) {
		FFSP_DEBUG("ffsp_read_internal(): count > SSIZE_MAX");
	}

//	rc = pread(fd, buf, count, offset);
	if (rc == -1) {
		rc = -errno;
		FFSP_ERROR("ffsp_read_raw(): pread() failed; errno=%d", errno);
	}

	ffsp_debug_update(FFSP_DEBUG_READ_RAW, count);

	// TODO: Handle EINTR.
	// TODO: Find out if interrupts can occur when the file system was
	//        not started with the "-o intr" flag.

	return rc;
}

int ffsp_write_raw(int fd, const void *buf, size_t count, off_t offset)
{
	ssize_t rc;

	if (count > SSIZE_MAX) {
		FFSP_DEBUG("ffsp_write_internal(): count > SSIZE_MAX");
	}

//	rc = pwrite(fd, buf, count, offset);
	if (rc == -1) {
		rc = -errno; // Return negative errno
		FFSP_ERROR("ffsp_write_raw(): pwrite() failed; errno=%d", errno);
	}

	ffsp_debug_update(FFSP_DEBUG_WRITE_RAW, count);

	// TODO: Handle EINTR.
	// TODO: Find out if interrupts can occur when the file system was
	//        not started with the "-o intr" flag.

	return rc;
}
