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

#include "fuse_ffsp.hpp"

#include "libffsp/debug.hpp"
#include "libffsp/eraseblk.hpp"
#include "libffsp/ffsp.hpp"
#include "libffsp/inode.hpp"
#include "libffsp/io.hpp"
#include "libffsp/io_raw.hpp"
#include "libffsp/log.hpp"
#include "libffsp/mkfs.hpp"
#include "libffsp/mount.hpp"
#include "libffsp/utils.hpp"

#include <memory>
#include <string>
#include <vector>

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#ifdef _WIN32
#ifndef S_ISDIR
#include <io.h>
#define S_ISDIR(mode) (((mode)&S_IFMT) == S_IFDIR)
#endif
#else
#include <unistd.h>
#endif

namespace ffsp
{

namespace fuse
{

static struct mount_options
{
    std::unique_ptr<std::string> device;
    std::unique_ptr<mkfs_options> mkfs_opts;
    size_t memsize{0};
} mount_opts;


// Convert from fuse_file_info->fh to ffsp_inode...
static inode* get_inode(const fuse_file_info* fi)
{
    return (inode*)(size_t)fi->fh;
}
// ... and back to fuse_file_info->fh.
static void set_inode(fuse_file_info* fi, const inode* ino)
{
    fi->fh = (size_t)ino;
}

void set_options(const std::string& device)
{
    mount_opts.device.reset(new std::string{device});
    mount_opts.mkfs_opts.reset();
    mount_opts.memsize = 0;
}

void set_options(const std::string& device, const mkfs_options& options)
{
    mount_opts.device.reset(new std::string{device});
    mount_opts.mkfs_opts.reset(new mkfs_options{options});
    mount_opts.memsize = 0;
}

void set_options(size_t memsize, const mkfs_options& options)
{
    mount_opts.device.reset();
    mount_opts.mkfs_opts.reset(new mkfs_options{options});
    mount_opts.memsize = memsize;
}

void* init(fuse_conn_info* conn)
{
    log_init("ffsp_api", spdlog::level::debug);

    io_context* io_ctx = mount_opts.device
        ? ffsp::io_context_init(mount_opts.device->c_str())
        : ffsp::io_context_init(mount_opts.memsize);

    if (!io_ctx)
    {
        log().error("ffsp::init(): init I/O context failed");
        exit(EXIT_FAILURE);
    }

    if (mount_opts.mkfs_opts && !mkfs(*io_ctx, *mount_opts.mkfs_opts))
    {
        log().error("fuse::init(): mkfs failed");
        exit(EXIT_FAILURE);
    }

    fs_context* fs = ffsp::mount(io_ctx);
    if (!fs)
    {
        log().error("fuse::init(): mounting failed");
        exit(EXIT_FAILURE);
    }

#ifndef _WIN32
    if (conn->capable & FUSE_CAP_ATOMIC_O_TRUNC)
    {
        conn->want |= FUSE_CAP_ATOMIC_O_TRUNC;
    }

    if (conn->capable & FUSE_CAP_BIG_WRITES)
    {
        conn->want |= FUSE_CAP_BIG_WRITES;
        conn->max_write = fs->clustersize;
        log().debug("Setting max_write to {}", conn->max_write);
    }
#else
    conn->max_write = fs->clustersize;
    ffsp_log().debug("Setting max_write to {}", conn->max_write);
#endif

    // TODO: Would it be ok to read all existing inode + dentry structs
    //  into memory at mount time?
    //  -> probably a good idea to be set via console arg to
    //      measure max memory usage.

    return fs;
}

void destroy(void* user)
{
    io_context* io_ctx = ffsp::unmount(static_cast<fs_context*>(user));
    ffsp::io_context_uninit(io_ctx);

    log_deinit();
}

#ifdef _WIN32
int getattr(fs_context& fs, const char* path, struct FUSE_STAT* stbuf)
#else
int getattr(fs_context& fs, const char* path, struct ::stat* stbuf)
#endif
{
    if (ffsp::is_debug_path(fs, path))
        return ffsp::debug_getattr(fs, path, *stbuf) ? 0 : -EIO;

    inode* ino;
    int rc = ffsp::lookup(fs, &ino, path);
    if (rc < 0)
        return rc;

#ifdef _WIN32
    struct ::stat stbuf_tmp;
    ffsp::stat(fs, ino, &stbuf_tmp);

    memset(stbuf, 0, sizeof(*stbuf));
    stbuf->st_dev = stbuf_tmp.st_dev;     /* dev_t <- _dev_t */
    stbuf->st_ino = stbuf_tmp.st_ino;     /* uint64_t <- _ino_t */
    stbuf->st_mode = stbuf_tmp.st_mode;   /* mode_t <- unsigned short */
    stbuf->st_nlink = stbuf_tmp.st_nlink; /* nlink_t <- short */
    stbuf->st_uid = stbuf_tmp.st_uid;     /* uid_t <- short */
    stbuf->st_gid = stbuf_tmp.st_gid;     /* gid_t <- short */
    stbuf->st_rdev = stbuf_tmp.st_rdev;   /* dev_t <- _dev_t */
    stbuf->st_size = stbuf_tmp.st_size;   /* FUSE_OFF_T <- _off_t */

    struct ffsp::timespec atim;
    atim.tv_sec = stbuf_tmp.st_atime;
    atim.tv_nsec = 0;
    stbuf->st_atim = atim; /* timestruc_t <- time_t */

    struct ffsp::timespec mtim;
    mtim.tv_sec = stbuf_tmp.st_mtime;
    mtim.tv_nsec = 0;
    stbuf->st_mtim = mtim; /* timestruc_t <- time_t */

    struct ffsp::timespec ctim;
    ctim.tv_sec = stbuf_tmp.st_ctime;
    ctim.tv_nsec = 0;
    stbuf->st_ctim = ctim; /* timestruc_t <- time_t */
#else
    ffsp::stat(fs, *ino, *stbuf);
#endif

    return 0;
}

int readdir(fs_context& fs, const char* path, void* buf, fuse_fill_dir_t filler,
            FUSE_OFF_T offset, fuse_file_info* fi)
{
    (void)offset;
    (void)fi;

    if (ffsp::is_debug_path(fs, path))
    {
        std::vector<std::string> dirs;
        if (!ffsp::debug_readdir(fs, path, dirs))
            return -EIO;

        for (const auto& dir : dirs)
        {
            if (filler(buf, dir.c_str(), nullptr, 0))
                log().debug("readdir({}): filler full!", path);
        }
        return 0;
    }

    inode* ino;
    int rc = ffsp::lookup(fs, &ino, path);
    if (rc < 0)
        return rc;

    if (!S_ISDIR(get_be32(ino->i_mode)))
        return -ENOTDIR;

    // Number of potential ffsp_dentry elements. The exact number is not
    //  tracked. Return value of < 0 indicates an error.
    dentry* dent_buf;
    int dent_cnt;
    rc = cache_dir(fs, ino, &dent_buf, &dent_cnt);
    if (rc < 0)
        return rc;

    for (int i = 0; i < dent_cnt; i++)
    {
        if (get_be32(dent_buf[i].ino) == FFSP_INVALID_INO_NO)
            continue; // Invalid ffsp_entry
        if (filler(buf, dent_buf[i].name, NULL, 0))
            log().debug("readdir({}): filler full!", path);
    }
    // TODO: Handle directory cache inside ffsp structure.
    free(dent_buf);
    return 0;
}

int open(fs_context& fs, const char* path, fuse_file_info* fi)
{
    if (ffsp::is_debug_path(fs, path))
        return ffsp::debug_open(fs, path) ? 0 : -EIO;

    inode* ino;
    int rc = ffsp::lookup(fs, &ino, path);
    if (rc < 0)
        return rc;

    // TODO: Comment on why we explicitly made open & trunc atomic
    if (fi->flags & O_TRUNC)
    {
        rc = ffsp::truncate(fs, ino, 0);
        if (rc < 0)
            return rc;
    }
    set_inode(fi, ino);
    return 0;
}

int release(fs_context& fs, const char* path, fuse_file_info* fi)
{
    if (ffsp::is_debug_path(fs, path))
        return ffsp::debug_release(fs, path) ? 0 : -EIO;

    set_inode(fi, NULL);
    return 0;
}

int truncate(fs_context& fs, const char* path, FUSE_OFF_T length)
{
    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    if (length < 0)
        return -EINVAL;

    inode* ino;
    int rc = ffsp::lookup(fs, &ino, path);
    if (rc < 0)
        return rc;

    ffsp::truncate(fs, ino, static_cast<uint64_t>(length));
    return 0;
}

int read(fs_context& fs, const char* path, char* buf, size_t count,
         FUSE_OFF_T offset, fuse_file_info* fi)
{
    if (ffsp::is_debug_path(fs, path))
    {
        uint64_t read = 0;
        if (ffsp::debug_read(fs, path, buf, count, static_cast<uint64_t>(offset), read))
            return static_cast<int>(read);
        else
            return -EIO;
    }

    if (offset < 0)
        return -EINVAL;

    inode* ino;
    if (fi)
    {
        ino = get_inode(fi);
    }
    else
    {
        int rc = ffsp::lookup(fs, &ino, path);
        if (rc < 0)
            return rc;
    }

    ffsp::debug_update(fs, debug_metric::fuse_read, count);
    return ffsp::read(fs, ino, buf, count, static_cast<uint64_t>(offset));
}

int write(fs_context& fs, const char* path, const char* buf, size_t count,
          FUSE_OFF_T offset, fuse_file_info* fi)
{
    if (offset < 0)
        return -EINVAL;

    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    inode* ino;
    if (fi)
    {
        ino = get_inode(fi);
    }
    else
    {
        int rc = ffsp::lookup(fs, &ino, path);
        if (rc < 0)
            return rc;
    }

    ffsp::debug_update(fs, debug_metric::fuse_write, count);
    return ffsp::write(fs, ino, buf, count, static_cast<uint64_t>(offset));
}

int mknod(fs_context& fs, const char* path, mode_t mode, dev_t device)
{
    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    uid_t uid = fuse_get_context()->uid;
    gid_t gid = fuse_get_context()->gid;

    // TODO: Make sure one and only one of the mode-flags is set.

    return ffsp::create(fs, path, mode, uid, gid, device);
}

int link(fs_context& fs, const char* oldpath, const char* newpath)
{
    if (ffsp::is_debug_path(fs, oldpath) || ffsp::is_debug_path(fs, newpath))
        return -EPERM;

    return ffsp::link(fs, oldpath, newpath);
}

int symlink(fs_context& fs, const char* oldpath, const char* newpath)
{
    if (ffsp::is_debug_path(fs, oldpath) || ffsp::is_debug_path(fs, newpath))
        return -EPERM;

    uid_t uid = fuse_get_context()->uid;
    gid_t gid = fuse_get_context()->gid;

    return ffsp::symlink(fs, oldpath, newpath, uid, gid);
}

int readlink(fs_context& fs, const char* path, char* buf, size_t bufsize)
{
    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    return ffsp::readlink(fs, path, buf, bufsize);
}

int mkdir(fs_context& fs, const char* path, mode_t mode)
{
    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    uid_t uid = fuse_get_context()->uid;
    gid_t gid = fuse_get_context()->gid;

    return ffsp::create(fs, path, mode | S_IFDIR, uid, gid, 0);
}

int unlink(fs_context& fs, const char* path)
{
    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    return ffsp::unlink(fs, path);
}

int rmdir(fs_context& fs, const char* path)
{
    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    return ffsp::rmdir(fs, path);
}

int rename(fs_context& fs, const char* oldpath, const char* newpath)
{
    if (ffsp::is_debug_path(fs, oldpath) || is_debug_path(fs, newpath))
        return -EPERM;

    return ffsp::rename(fs, oldpath, newpath);
}

int utimens(fs_context& fs, const char* path, const struct ::timespec tv[2])
{
    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    inode* ino;
    int rc = ffsp::lookup(fs, &ino, path);
    if (rc < 0)
        return rc;

    ffsp::utimens(fs, *ino, tv);
    return 0;
}

int chmod(fs_context& fs, const char* path, mode_t mode)
{
    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    inode* ino;
    int rc = ffsp::lookup(fs, &ino, path);
    if (rc < 0)
        return rc;

    ino->i_mode = put_be32(mode);
    mark_dirty(fs, ino);
    flush_inodes(fs, false);
    return 0;
}

int chown(fs_context& fs, const char* path, uid_t uid, gid_t gid)
{
    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    inode* ino;
    int rc = ffsp::lookup(fs, &ino, path);
    if (rc < 0)
        return rc;

    ino->i_uid = put_be32(uid);
    ino->i_gid = put_be32(gid);
    mark_dirty(fs, ino);
    flush_inodes(fs, false);
    return 0;
}

int statfs(fs_context& fs, const char* path, struct ::statvfs* sfs)
{
    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    ffsp::statfs(fs, *sfs);
    return 0;
}

int flush(fs_context& fs, const char* path, fuse_file_info* fi)
{
    (void)fi;

    if (ffsp::is_debug_path(fs, path))
        return 0;

    // TODO: Implement Me!

    // Will write back the inode map and erase block usage data.

    return 0;
}

int fsync(fs_context& fs, const char* path, int datasync, fuse_file_info* fi)
{
    (void)datasync;
    (void)fi;

    if (ffsp::is_debug_path(fs, path))
        return 0;

    // TODO: Implement Me!

    // Will write back the file's dirty data.

    return 0;
}

} // namespace fuse

} // namespace ffsp
