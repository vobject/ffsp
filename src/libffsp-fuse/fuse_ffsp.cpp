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
#include "fuse_ffsp_log.hpp"
#include "fuse_ffsp_utils.hpp"

#include "libffsp/debug.hpp"
#include "libffsp/eraseblk.hpp"
#include "libffsp/ffsp.hpp"
#include "libffsp/inode.hpp"
#include "libffsp/io.hpp"
#include "libffsp/io_backend.hpp"
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
#include <time.h>
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
    size_t memsize{ 0 };
} mnt_opts;

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

void set_options(const char* device)
{
    mnt_opts.device.reset(new std::string{ device });
    mnt_opts.mkfs_opts.reset();
    mnt_opts.memsize = 0;
}

void set_options(const char* device, const mkfs_options& options)
{
    mnt_opts.device.reset(new std::string{ device });
    mnt_opts.mkfs_opts.reset(new mkfs_options{ options });
    mnt_opts.memsize = 0;
}

void set_options(size_t memsize, const mkfs_options& options)
{
    mnt_opts.device.reset();
    mnt_opts.mkfs_opts.reset(new mkfs_options{ options });
    mnt_opts.memsize = memsize;
}

void* init(fuse_conn_info* conn)
{
    log().debug("init(conn={})", log_ptr(conn));

    io_backend* io_ctx = mnt_opts.device
                             ? ffsp::io_backend_init(mnt_opts.device->c_str())
                             : ffsp::io_backend_init(mnt_opts.memsize);

    if (!io_ctx)
    {
        log().error("ffsp::init(): init I/O context failed");
        exit(EXIT_FAILURE);
    }

    if (mnt_opts.mkfs_opts && !mkfs(*io_ctx, *mnt_opts.mkfs_opts))
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

    if (conn)
    {
#ifdef _WIN32
        conn->max_write = fs->clustersize;
        log().info("Setting max_write to {}", conn->max_write);
#else
        if (conn->capable & FUSE_CAP_ATOMIC_O_TRUNC)
        {
            conn->want |= FUSE_CAP_ATOMIC_O_TRUNC;
        }

        if (conn->capable & FUSE_CAP_BIG_WRITES)
        {
            conn->want |= FUSE_CAP_BIG_WRITES;
            conn->max_write = fs->clustersize;
            log().info("Setting max_write to {}", conn->max_write);
        }
#endif
    }

    // TODO: Would it be ok to read all existing inode + dentry structs
    //  into memory at mount time?
    //  -> probably a good idea to be set via console arg to
    //      measure max memory usage.

    return fs;
}

void destroy(void* user)
{
    log().debug("destroy(user={})", user);

    io_backend* io_ctx = ffsp::unmount(static_cast<fs_context*>(user));
    ffsp::io_backend_uninit(io_ctx);
}

#ifdef _WIN32
int getattr(fs_context& fs, const char* path, struct FUSE_STAT* stbuf)
#else
int getattr(fs_context& fs, const char* path, struct ::stat* stbuf)
#endif
{
    log().debug("getattr(path={}, stbuf={})", path, static_cast<void*>(stbuf));

    if (ffsp::is_debug_path(fs, path))
        return ffsp::debug_getattr(fs, path, *stbuf) ? 0 : -EIO;

    inode* ino;
    int rc = ffsp::lookup(fs, &ino, path);
    if (rc < 0)
        return rc;

#ifdef _WIN32
    struct ::stat stbuf_tmp;
    ffsp::fuse::stat(fs, *ino, stbuf_tmp);

    memset(stbuf, 0, sizeof(*stbuf));
    stbuf->st_dev = stbuf_tmp.st_dev;     /* dev_t <- _dev_t */
    stbuf->st_ino = stbuf_tmp.st_ino;     /* uint64_t <- _ino_t */
    stbuf->st_mode = stbuf_tmp.st_mode;   /* mode_t <- unsigned short */
    stbuf->st_nlink = stbuf_tmp.st_nlink; /* nlink_t <- short */
    stbuf->st_uid = stbuf_tmp.st_uid;     /* uid_t <- short */
    stbuf->st_gid = stbuf_tmp.st_gid;     /* gid_t <- short */
    stbuf->st_rdev = stbuf_tmp.st_rdev;   /* dev_t <- _dev_t */
    stbuf->st_size = stbuf_tmp.st_size;   /* FUSE_OFF_T <- _off_t */

    timestruc_t atim;
    atim.tv_sec = stbuf_tmp.st_atime;
    atim.tv_nsec = 0;
    stbuf->st_atim = atim; /* timestruc_t <- time_t */

    timestruc_t mtim;
    mtim.tv_sec = stbuf_tmp.st_mtime;
    mtim.tv_nsec = 0;
    stbuf->st_mtim = mtim; /* timestruc_t <- time_t */

    timestruc_t ctim;
    ctim.tv_sec = stbuf_tmp.st_ctime;
    ctim.tv_nsec = 0;
    stbuf->st_ctim = ctim; /* timestruc_t <- time_t */
#else
    ffsp::fuse::stat(fs, *ino, *stbuf);
#endif

    return 0;
}

int readdir(fs_context& fs, const char* path, void* buf, fuse_fill_dir_t filler,
            FUSE_OFF_T offset, fuse_file_info* fi)
{
    log().debug("readdir(path={}, buf={}, filler_cb={}, offset={}, fi={})", path, buf, (filler != nullptr), offset, log_ptr(fi));

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
    std::vector<dentry> dentries;
    rc = read_dir(fs, *ino, dentries);
    if (rc < 0)
        return rc;

    for (const auto& dent : dentries)
    {
        if (get_be32(dent.ino) == FFSP_INVALID_INO_NO)
            continue; // Invalid ffsp_entry
        if (filler(buf, dent.name, nullptr, 0))
            log().debug("readdir({}): filler full!", path);
    }
    return 0;
}

int open(fs_context& fs, const char* path, fuse_file_info* fi)
{
    log().debug("open(path={}, fi={})", path, log_ptr(fi));

    if (ffsp::is_debug_path(fs, path))
        return ffsp::debug_open(fs, path) ? 0 : -EIO;

    inode* ino;
    int rc = ffsp::lookup(fs, &ino, path);
    if (rc < 0)
        return rc;

    // TODO: Comment on why we explicitly made open & trunc atomic
    if (fi->flags & O_TRUNC)
    {
        rc = ffsp::truncate(fs, *ino, 0);
        if (rc < 0)
            return rc;
    }
    set_inode(fi, ino);
    return 0;
}

int release(fs_context& fs, const char* path, fuse_file_info* fi)
{
    log().debug("release(path={}, fi={})", path, log_ptr(fi));

    if (ffsp::is_debug_path(fs, path))
        return ffsp::debug_release(fs, path) ? 0 : -EIO;

    set_inode(fi, nullptr);
    return 0;
}

int truncate(fs_context& fs, const char* path, FUSE_OFF_T length)
{
    log().debug("truncate(path={}, length={})", path, length);

    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    if (length < 0)
        return -EINVAL;

    inode* ino;
    int rc = ffsp::lookup(fs, &ino, path);
    if (rc < 0)
        return rc;

    ffsp::truncate(fs, *ino, static_cast<uint64_t>(length));
    return 0;
}

int read(fs_context& fs, const char* path, char* buf, size_t nbyte,
         FUSE_OFF_T offset, fuse_file_info* fi)
{
    log().debug("read(path={}, buf={}, nbyte={}, offset={}, fi={})", path, static_cast<void*>(buf), nbyte, offset, log_ptr(fi));

    if (ffsp::is_debug_path(fs, path))
        return static_cast<int>(ffsp::debug_read(fs, path, buf, nbyte, static_cast<uint64_t>(offset)));

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

    ffsp::debug_update(fs, debug_metric::fuse_read, nbyte);
    return static_cast<int>(ffsp::read(fs, *ino, buf, nbyte, static_cast<uint64_t>(offset)));
}

int write(fs_context& fs, const char* path, const char* buf, size_t nbyte,
          FUSE_OFF_T offset, fuse_file_info* fi)
{
    log().debug("write(path={}, buf={}, nbyte={}, offset={}, fi={})", path, static_cast<const void*>(buf), nbyte, offset, log_ptr(fi));

    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

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

    ffsp::debug_update(fs, debug_metric::fuse_write, nbyte);
    return static_cast<int>(ffsp::write(fs, *ino, buf, nbyte, static_cast<uint64_t>(offset)));
}

int mknod(fs_context& fs, const char* path, mode_t mode, dev_t device)
{
    log().debug("mknod(path={}, mode={:#o}, device={})", path, mode, device);

    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    uid_t uid = fuse_get_context()->uid;
    gid_t gid = fuse_get_context()->gid;

    // TODO: Make sure one and only one of the mode-flags is set.

    return ffsp::create(fs, path, mode, uid, gid, device);
}

int link(fs_context& fs, const char* oldpath, const char* newpath)
{
    log().debug("link(oldpath={}, newpath={})", oldpath, newpath);

    if (ffsp::is_debug_path(fs, oldpath) || ffsp::is_debug_path(fs, newpath))
        return -EPERM;

    return ffsp::link(fs, oldpath, newpath);
}

int symlink(fs_context& fs, const char* oldpath, const char* newpath)
{
    log().debug("symlink(oldpath={}, newpath={})", oldpath, newpath);

    if (ffsp::is_debug_path(fs, oldpath) || ffsp::is_debug_path(fs, newpath))
        return -EPERM;

    uid_t uid = fuse_get_context()->uid;
    gid_t gid = fuse_get_context()->gid;

    return ffsp::symlink(fs, oldpath, newpath, uid, gid);
}

int readlink(fs_context& fs, const char* path, char* buf, size_t bufsize)
{
    log().debug("readlink(path={}, buf={}, bufsize={})", path, static_cast<void*>(buf), bufsize);

    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    return ffsp::readlink(fs, path, buf, bufsize);
}

int mkdir(fs_context& fs, const char* path, mode_t mode)
{
    log().debug("mkdir(path={}, mode={:#o})", path, mode);

    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    uid_t uid = fuse_get_context()->uid;
    gid_t gid = fuse_get_context()->gid;

    return ffsp::create(fs, path, mode | S_IFDIR, uid, gid, 0);
}

int unlink(fs_context& fs, const char* path)
{
    log().debug("unlink(path={})", path);

    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    return ffsp::unlink(fs, path);
}

int rmdir(fs_context& fs, const char* path)
{
    log().debug("rmdir(path={})", path);

    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    return ffsp::rmdir(fs, path);
}

int rename(fs_context& fs, const char* oldpath, const char* newpath)
{
    log().debug("rename(oldpath={}, newpath={})", oldpath, newpath);

    if (ffsp::is_debug_path(fs, oldpath) || is_debug_path(fs, newpath))
        return -EPERM;

    return ffsp::rename(fs, oldpath, newpath);
}

int utimens(fs_context& fs, const char* path, const struct ::timespec tv[2])
{
    log().debug("utimens(path={}, access={}, mod={})", path, tv[0], tv[1]);

    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    inode* ino;
    int rc = ffsp::lookup(fs, &ino, path);
    if (rc < 0)
        return rc;

    ffsp::fuse::utimens(fs, *ino, tv);
    return 0;
}

int chmod(fs_context& fs, const char* path, mode_t mode)
{
    log().debug("chmod(path={}, mode={:#o})", path, mode);

    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    inode* ino;
    int rc = ffsp::lookup(fs, &ino, path);
    if (rc < 0)
        return rc;

    ino->i_mode = put_be32(mode);
    mark_dirty(fs, *ino);
    flush_inodes(fs, false);
    return 0;
}

int chown(fs_context& fs, const char* path, uid_t uid, gid_t gid)
{
    log().debug("chown(path={}, uid={}, gid={})", path, uid, gid);

    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    inode* ino;
    int rc = ffsp::lookup(fs, &ino, path);
    if (rc < 0)
        return rc;

    ino->i_uid = put_be32(uid);
    ino->i_gid = put_be32(gid);
    mark_dirty(fs, *ino);
    flush_inodes(fs, false);
    return 0;
}

int statfs(fs_context& fs, const char* path, struct ::statvfs* sfs)
{
    log().debug("statfs(path={}, sfs={})", path, static_cast<void*>(sfs));

    if (ffsp::is_debug_path(fs, path))
        return -EPERM;

    ffsp::fuse::statfs(fs, *sfs);
    return 0;
}

int flush(fs_context& fs, const char* path, fuse_file_info* fi)
{
    log().debug("flush(path={}, fi={})", path, log_ptr(fi));

    if (ffsp::is_debug_path(fs, path))
        return 0;

    // TODO: Implement Me!

    // Will write back the inode map and erase block usage data.

    return 0;
}

int fsync(fs_context& fs, const char* path, int datasync, fuse_file_info* fi)
{
    log().debug("fsync(path={}, datasync={}, fi={})", path, datasync, log_ptr(fi));

    if (ffsp::is_debug_path(fs, path))
        return 0;

    // TODO: Implement Me!

    // Will write back the file's dirty data.

    return 0;
}

} // namespace fuse

} // namespace ffsp


// ffsp fuse "C" wrapper interface
extern "C" {

// http://stackoverflow.com/questions/2164827/explicitly-exporting-shared-library-functions-in-linux
#if defined(_MSC_VER)
#define EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
#define EXPORT __attribute__((visibility("default")))
#else
#define EXPORT
#pragma warning Unknown dynamic link import/export semantics.
#endif

EXPORT void ffsp_fuse_set_options_dev(const char* device)
{
    ffsp::fuse::set_options(device);
}

EXPORT void ffsp_fuse_set_options_dev_opt(const char* device, uint32_t c, uint32_t e, uint32_t i, uint32_t o, uint32_t r, uint32_t w)
{
    ffsp::fuse::set_options(device, { c, e, i, o, r, w });
}

EXPORT void ffsp_fuse_set_options_buf_opt(size_t memsize, uint32_t c, uint32_t e, uint32_t i, uint32_t o, uint32_t r, uint32_t w)
{
    ffsp::fuse::set_options(memsize, { c, e, i, o, r, w });
}

EXPORT void* ffsp_fuse_init(fuse_conn_info* conn)
{
    return ffsp::fuse::init(conn);
}

EXPORT void ffsp_fuse_destroy(void* user)
{
    return ffsp::fuse::destroy(user);
}

#ifdef _WIN32
EXPORT int ffsp_fuse_getattr(void* fs, const char* path, struct FUSE_STAT* stbuf)
#else
EXPORT int ffsp_fuse_getattr(void* fs, const char* path, struct ::stat* stbuf)
#endif
{
    return ffsp::fuse::getattr(*reinterpret_cast<ffsp::fs_context*>(fs), path, stbuf);
}

EXPORT int ffsp_fuse_readdir(void* fs, const char* path, void* buf, fuse_fill_dir_t filler, FUSE_OFF_T offset, fuse_file_info* fi)
{
    return ffsp::fuse::readdir(*reinterpret_cast<ffsp::fs_context*>(fs), path, buf, filler, offset, fi);
}

EXPORT int ffsp_fuse_open(void* fs, const char* path, fuse_file_info* fi)
{
    return ffsp::fuse::open(*reinterpret_cast<ffsp::fs_context*>(fs), path, fi);
}

EXPORT int ffsp_fuse_release(void* fs, const char* path, fuse_file_info* fi)
{
    return ffsp::fuse::release(*reinterpret_cast<ffsp::fs_context*>(fs), path, fi);
}

EXPORT int ffsp_fuse_truncate(void* fs, const char* path, FUSE_OFF_T length)
{
    return ffsp::fuse::truncate(*reinterpret_cast<ffsp::fs_context*>(fs), path, length);
}

EXPORT int ffsp_fuse_read(void* fs, const char* path, char* buf, size_t count, FUSE_OFF_T offset, fuse_file_info* fi)
{
    return ffsp::fuse::read(*reinterpret_cast<ffsp::fs_context*>(fs), path, buf, count, offset, fi);
}

EXPORT int ffsp_fuse_write(void* fs, const char* path, const char* buf, size_t count, FUSE_OFF_T offset, fuse_file_info* fi)
{
    return ffsp::fuse::write(*reinterpret_cast<ffsp::fs_context*>(fs), path, buf, count, offset, fi);
}

EXPORT int ffsp_fuse_mknod(void* fs, const char* path, mode_t mode, dev_t device)
{
    return ffsp::fuse::mknod(*reinterpret_cast<ffsp::fs_context*>(fs), path, mode, device);
}

EXPORT int ffsp_fuse_link(void* fs, const char* oldpath, const char* newpath)
{
    return ffsp::fuse::link(*reinterpret_cast<ffsp::fs_context*>(fs), oldpath, newpath);
}

EXPORT int ffsp_fuse_symlink(void* fs, const char* oldpath, const char* newpath)
{
    return ffsp::fuse::symlink(*reinterpret_cast<ffsp::fs_context*>(fs), oldpath, newpath);
}

EXPORT int ffsp_fuse_readlink(void* fs, const char* path, char* buf, size_t bufsize)
{
    return ffsp::fuse::readlink(*reinterpret_cast<ffsp::fs_context*>(fs), path, buf, bufsize);
}

EXPORT int ffsp_fuse_mkdir(void* fs, const char* path, mode_t mode)
{
    return ffsp::fuse::mkdir(*reinterpret_cast<ffsp::fs_context*>(fs), path, mode);
}

EXPORT int ffsp_fuse_rmdir(void* fs, const char* path)
{
    return ffsp::fuse::rmdir(*reinterpret_cast<ffsp::fs_context*>(fs), path);
}

EXPORT int ffsp_fuse_unlink(void* fs, const char* path)
{
    return ffsp::fuse::unlink(*reinterpret_cast<ffsp::fs_context*>(fs), path);
}

EXPORT int ffsp_fuse_rename(void* fs, const char* oldpath, const char* newpath)
{
    return ffsp::fuse::rename(*reinterpret_cast<ffsp::fs_context*>(fs), oldpath, newpath);
}

EXPORT int ffsp_fuse_utimens(void* fs, const char* path, const struct ::timespec tv[2])
{
    return ffsp::fuse::utimens(*reinterpret_cast<ffsp::fs_context*>(fs), path, tv);
}

EXPORT int ffsp_fuse_chmod(void* fs, const char* path, mode_t mode)
{
    return ffsp::fuse::chmod(*reinterpret_cast<ffsp::fs_context*>(fs), path, mode);
}

EXPORT int ffsp_fuse_chown(void* fs, const char* path, uid_t uid, gid_t gid)
{
    return ffsp::fuse::chown(*reinterpret_cast<ffsp::fs_context*>(fs), path, uid, gid);
}

EXPORT int ffsp_fuse_statfs(void* fs, const char* path, struct ::statvfs* sfs)
{
    return ffsp::fuse::statfs(*reinterpret_cast<ffsp::fs_context*>(fs), path, sfs);
}

EXPORT int ffsp_fuse_flush(void* fs, const char* path, fuse_file_info* fi)
{
    return ffsp::fuse::flush(*reinterpret_cast<ffsp::fs_context*>(fs), path, fi);
}

EXPORT int ffsp_fuse_fsync(void* fs, const char* path, int datasync, fuse_file_info* fi)
{
    return ffsp::fuse::fsync(*reinterpret_cast<ffsp::fs_context*>(fs), path, datasync, fi);
}

EXPORT void ffsp_log_init(const char* logname, int level, const char* logfile)
{
    ffsp::log_init(logname, static_cast<spdlog::level::level_enum>(level), logfile ? logfile : "");
}

EXPORT void ffsp_log_uninit()
{
    ffsp::log_uninit();
}

} // extern "C"
