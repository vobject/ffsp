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
#include "log.hpp"

#include <limits>
#include <new>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <BaseTsd.h>
#include <cstdio>
#include <io.h>
typedef SSIZE_T ssize_t;
#else
#include <unistd.h>
#endif

namespace ffsp
{

static ssize_t do_pread(int fd, void* buf, size_t nbyte, off_t offset);
static ssize_t do_pwrite(int fd, const void* buf, size_t nbyte, off_t offset);

struct io_context
{
    virtual ~io_context() {}

    virtual uint64_t size() const = 0;
    virtual ssize_t read(void* buf, size_t nbyte, off_t offset) = 0;
    virtual ssize_t write(const void* buf, size_t nbyte, off_t offset) = 0;
};

struct file_io_context : io_context
{
    explicit file_io_context(int fd)
        : fd_{ fd }
    {
    }

    virtual ~file_io_context()
    {
        if (::close(fd_) == -1)
            log().error("ffsp::io_context_uninit(): close(fd) failed");
    }

    uint64_t size() const override
    {
        off_t size = ::lseek(fd_, 0, SEEK_END);
        if (size == -1)
        {
            log().critical("ffsp::io_context: lseek() failed");
            abort();
        }
        return static_cast<uint64_t>(size);
    }

    ssize_t read(void* buf, size_t nbyte, off_t offset) override
    {
        return do_pread(fd_, buf, nbyte, offset);
    }

    ssize_t write(const void* buf, size_t nbyte, off_t offset) override
    {
        return do_pwrite(fd_, buf, nbyte, offset);
    }

    const int fd_;
};

struct buffer_io_context : io_context
{
    explicit buffer_io_context(char* buf, size_t size)
        : buf_{ buf }
        , size_{ size }
    {
    }

    virtual ~buffer_io_context()
    {
        delete[] buf_;
    }

    uint64_t size() const override
    {
        return size_;
    }

    ssize_t read(void* buf, size_t nbyte, off_t offset) override
    {
        memcpy(buf, buf_ + offset, nbyte);
        return static_cast<ssize_t>(nbyte);
    }

    ssize_t write(const void* buf, size_t nbyte, off_t offset) override
    {
        memcpy(buf_ + offset, buf, nbyte);
        return static_cast<ssize_t>(nbyte);
    }

    char* buf_;
    const size_t size_;
};

io_context* io_context_init(const char* path)
{
/*
     * O_DIRECT could also be used if all pwrite() calls get a
     * page-aligned write pointer. But to get that calls to malloc had
     * to be replaced by posix_memalign with 4k alignment.
     */
#ifdef _WIN32
    int fd = ::open(path, O_RDWR);
#else
    int fd = ::open(path, O_RDWR | O_SYNC);
#endif
    if (fd == -1)
        return nullptr;

    return new file_io_context{ fd };
}

io_context* io_context_init(size_t size)
{
    auto* buf = new (std::nothrow) char[size];
    if (!buf)
        return nullptr;

    return new buffer_io_context{ buf, size };
}

void io_context_uninit(io_context* ctx)
{
    delete ctx;
}

uint64_t io_context_size(const io_context& ctx)
{
    return ctx.size();
}

static ssize_t do_pread(int fd, void* buf, size_t nbyte, off_t offset)
{
#ifdef _WIN32
    off_t oldoff = ::lseek(fd, offset, SEEK_SET);
    if (oldoff == -1)
        return -1;

    ssize_t rc = ::read(fd, buf, nbyte);

    int olderrno = errno;
    ::lseek(fd, oldoff, SEEK_SET);
    errno = olderrno;

    return rc;
#else
    return ::pread(fd, buf, nbyte, offset);
#endif
}

static ssize_t do_pwrite(int fd, const void* buf, size_t nbyte, off_t offset)
{
#ifdef _WIN32
    off_t oldoff = ::lseek(fd, offset, SEEK_SET);
    if (oldoff == -1)
        return -1;

    ssize_t rc = ::write(fd, buf, nbyte);

    int olderrno = errno;
    ::lseek(fd, oldoff, SEEK_SET);
    errno = olderrno;

    return rc;
#else
    return ::pwrite(fd, buf, nbyte, offset);
#endif
}

ssize_t read_raw(io_context& ctx, void* buf, uint64_t nbyte, uint64_t offset)
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

    ssize_t rc = ctx.read(buf, nbyte, static_cast<off_t>(offset));
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

ssize_t write_raw(io_context& ctx, const void* buf, uint64_t nbyte, uint64_t offset)
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

    ssize_t rc = ctx.write(buf, nbyte, static_cast<off_t>(offset));
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
