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
#include "io_backend.hpp"
#include "log.hpp"

#include <limits>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <cstdio>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace ffsp
{

ssize_t read_raw(io_backend& ctx, void* buf, uint64_t nbyte, uint64_t offset)
{
    if (nbyte > std::numeric_limits<ssize_t>::max())
    {
        log().error("ffsp::read_raw(): nbyte > ssize_t max");
        return -EOVERFLOW; // implementation defined
    }

    if (offset > std::numeric_limits<off_t>::max())
    {
        log().error("ffsp::read_raw(): offset > off_t max");
        return -EOVERFLOW;
    }

    ssize_t rc = io_backend_read(ctx, buf, nbyte, static_cast<off_t>(offset));
    if (rc == -1)
    {
        rc = -errno;
        log().error("ffsp::read_raw(): pread() failed with errno={}", -rc);
        return rc;
    }

    // TODO: Handle EINTR.
    // TODO: Find out if interrupts can occur when the file system was
    //        not started with the "-o intr" flag.

    return rc;
}

ssize_t write_raw(io_backend& ctx, const void* buf, uint64_t nbyte, uint64_t offset)
{
    if (nbyte > std::numeric_limits<ssize_t>::max())
    {
        log().error("ffsp::write_raw(): nbyte > ssize_t max");
        return -EOVERFLOW; // implementation defined
    }

    if (offset > std::numeric_limits<off_t>::max())
    {
        log().error("ffsp::write_raw(): offset > off_t max");
        return -EOVERFLOW;
    }

    ssize_t rc = io_backend_write(ctx, buf, nbyte, static_cast<off_t>(offset));
    if (rc == -1)
    {
        rc = -errno;
        log().error("ffsp::write_raw(): pwrite() failed with errno={}", -rc);
        return rc;
    }

    // TODO: Handle EINTR.
    // TODO: Find out if interrupts can occur when the file system was
    //        not started with the "-o intr" flag.

    return rc;
}

} // namespace ffsp
