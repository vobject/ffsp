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

#include "io_raw.hpp"
#include "debug.hpp"
#include "log.hpp"

#include <cerrno>
#include <climits>

#ifdef _WIN32
#include <BaseTsd.h>
#include <cstdio>
#include <io.h>
typedef SSIZE_T ssize_t;
#define SSIZE_MAX MAXSSIZE_T
#else
#include <unistd.h>
#endif

static ssize_t do_pread(int fd, void* buf, size_t count, off_t offset)
{
#ifdef _WIN32
    ssize_t rc;
    off_t oldoff;
    int olderrno;

    oldoff = lseek(fd, offset, SEEK_SET);
    if (oldoff < 0)
        return -1;

    rc = read(fd, buf, count);

    olderrno = errno;
    lseek(fd, oldoff, SEEK_SET);
    errno = olderrno;
    return rc;
#else
    return pread(fd, buf, count, offset);
#endif
}

static ssize_t do_pwrite(int fd, const void* buf, size_t count, off_t offset)
{
#ifdef _WIN32
    ssize_t rc;
    off_t oldoff;
    int olderrno;

    oldoff = lseek(fd, offset, SEEK_SET);
    if (oldoff < 0)
        return -1;

    rc = write(fd, buf, count);

    olderrno = errno;
    lseek(fd, oldoff, SEEK_SET);
    errno = olderrno;

    return rc;
#else
    return pwrite(fd, buf, count, offset);
#endif
}

int ffsp_read_raw(int fd, void* buf, size_t count, off_t offset)
{
    ssize_t rc;

    if (count > SSIZE_MAX)
    {
        ffsp_log().debug("ffsp_read_internal(): count > SSIZE_MAX");
    }

    rc = do_pread(fd, buf, count, offset);
    if (rc == -1)
    {
        rc = -errno;
        ffsp_log().error("ffsp_read_raw(): pread() failed; errno={}", errno);
    }

    ffsp_debug_update(FFSP_DEBUG_READ_RAW, count);

    // TODO: Handle EINTR.
    // TODO: Find out if interrupts can occur when the file system was
    //        not started with the "-o intr" flag.

    return rc;
}

int ffsp_write_raw(int fd, const void* buf, size_t count, off_t offset)
{
    ssize_t rc;

    if (count > SSIZE_MAX)
    {
        ffsp_log().debug("ffsp_write_internal(): count > SSIZE_MAX");
    }

    rc = do_pwrite(fd, buf, count, offset);
    if (rc == -1)
    {
        rc = -errno; // Return negative errno
        ffsp_log().error("ffsp_write_raw(): pwrite() failed; errno={}", errno);
    }

    ffsp_debug_update(FFSP_DEBUG_WRITE_RAW, count);

    // TODO: Handle EINTR.
    // TODO: Find out if interrupts can occur when the file system was
    //        not started with the "-o intr" flag.

    return rc;
}
