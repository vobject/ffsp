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
#include "libffsp/log.hpp"
#include "libffsp/mount.hpp"
#include "libffsp/utils.hpp"

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

namespace fuse_ffsp
{

static struct params
{
    std::string device;
} params;

// Convert from fuse_file_info->fh to ffsp_inode...
static ffsp_inode* get_inode(const fuse_file_info* fi)
{
    return (ffsp_inode*)(size_t)fi->fh;
}
// ... and back to fuse_file_info->fh.
static void set_inode(fuse_file_info* fi, const ffsp_inode* ino)
{
    fi->fh = (size_t)ino;
}

void set_params(const std::string& device)
{
    params.device = device;
}

void* init(fuse_conn_info* conn)
{
    ffsp_log_init("ffsp_api", spdlog::level::debug);

    auto* fs = new ffsp_fs;

    if (!ffsp_mount(*fs, params.device.c_str()))
    {
        ffsp_log().error("ffsp_mount() failed. exiting...");
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
        ffsp_log().debug("Setting max_write to {}", conn->max_write);
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
    ffsp_fs* fs = static_cast<ffsp_fs*>(user);
    ffsp_unmount(*fs);
    delete fs;

    ffsp_log_deinit();
}

#ifdef _WIN32
int getattr(ffsp& fs, const char* path, struct FUSE_STAT* stbuf)
#else
int getattr(ffsp_fs& fs, const char* path, struct stat* stbuf)
#endif
{
    int rc;
    ffsp_inode* ino;

    if (ffsp_debug_is_debug_path(fs, path))
        return ffsp_debug_getattr(fs, path, *stbuf) ? 0 : -EIO;

    rc = ffsp_lookup(&fs, &ino, path);
    if (rc < 0)
        return rc;

#ifdef _WIN32
    struct stat stbuf_tmp;
    ffsp_stat(&fs, ino, &stbuf_tmp);

    memset(stbuf, 0, sizeof(*stbuf));
    stbuf->st_dev = stbuf_tmp.st_dev;     /* dev_t <- _dev_t */
    stbuf->st_ino = stbuf_tmp.st_ino;     /* uint64_t <- _ino_t */
    stbuf->st_mode = stbuf_tmp.st_mode;   /* mode_t <- unsigned short */
    stbuf->st_nlink = stbuf_tmp.st_nlink; /* nlink_t <- short */
    stbuf->st_uid = stbuf_tmp.st_uid;     /* uid_t <- short */
    stbuf->st_gid = stbuf_tmp.st_gid;     /* gid_t <- short */
    stbuf->st_rdev = stbuf_tmp.st_rdev;   /* dev_t <- _dev_t */
    stbuf->st_size = stbuf_tmp.st_size;   /* FUSE_OFF_T <- _off_t */

    struct timespec atim;
    atim.tv_sec = stbuf_tmp.st_atime;
    atim.tv_nsec = 0;
    stbuf->st_atim = atim; /* timestruc_t <- time_t */

    struct timespec mtim;
    mtim.tv_sec = stbuf_tmp.st_mtime;
    mtim.tv_nsec = 0;
    stbuf->st_mtim = mtim; /* timestruc_t <- time_t */

    struct timespec ctim;
    ctim.tv_sec = stbuf_tmp.st_ctime;
    ctim.tv_nsec = 0;
    stbuf->st_ctim = ctim; /* timestruc_t <- time_t */
#else
    ffsp_stat(fs, *ino, *stbuf);
#endif

    return 0;
}

int readdir(ffsp_fs& fs, const char* path, void* buf,
            fuse_fill_dir_t filler, FUSE_OFF_T offset,
            fuse_file_info* fi)
{
    (void)offset;
    (void)fi;

    int rc;
    ffsp_inode* ino;
    ffsp_dentry* dent_buf;
    int dent_cnt;

    if (ffsp_debug_is_debug_path(fs, path))
    {
        std::vector<std::string> dirs;
        if (!ffsp_debug_readdir(fs, path, dirs))
            return -EIO;

        for (const auto& dir : dirs)
        {
            if (filler(buf, dir.c_str(), nullptr, 0))
                ffsp_log().debug("readdir({}): filler full!", path);
        }
        return 0;
    }

    rc = ffsp_lookup(&fs, &ino, path);
    if (rc < 0)
        return rc;

    if (!S_ISDIR(get_be32(ino->i_mode)))
        return -ENOTDIR;

    // Number of potential ffsp_dentry elements. The exact number is not
    //  tracked. Return value of < 0 indicates an error.
    rc = ffsp_cache_dir(&fs, ino, &dent_buf, &dent_cnt);
    if (rc < 0)
        return rc;

    for (int i = 0; i < dent_cnt; i++)
    {
        if (get_be32(dent_buf[i].ino) == FFSP_INVALID_INO_NO)
            continue; // Invalid ffsp_entry
        if (filler(buf, dent_buf[i].name, NULL, 0))
            ffsp_log().debug("readdir({}): filler full!", path);
    }
    // TODO: Handle directory cache inside ffsp structure.
    free(dent_buf);
    return 0;
}

int open(ffsp_fs& fs, const char* path, fuse_file_info* fi)
{
    int rc;
    ffsp_inode* ino;

    if (ffsp_debug_is_debug_path(fs, path))
        return ffsp_debug_open(fs, path) ? 0 : -EIO;

    rc = ffsp_lookup(&fs, &ino, path);
    if (rc < 0)
        return rc;

    // TODO: Comment on why we explicitly made open & trunc atomic
    if (fi->flags & O_TRUNC)
    {
        rc = ffsp_truncate(&fs, ino, 0);
        if (rc < 0)
            return rc;
    }
    set_inode(fi, ino);
    return 0;
}

int release(ffsp_fs& fs, const char* path, fuse_file_info* fi)
{
    if (ffsp_debug_is_debug_path(fs, path))
        return ffsp_debug_release(fs, path) ? 0 : -EIO;

    set_inode(fi, NULL);
    return 0;
}

int truncate(ffsp_fs& fs, const char* path, FUSE_OFF_T length)
{
    int rc;
    ffsp_inode* ino;

    if (ffsp_debug_is_debug_path(fs, path))
        return -EPERM;

    if (length < 0)
        return -EINVAL;

    rc = ffsp_lookup(&fs, &ino, path);
    if (rc < 0)
        return rc;

    ffsp_truncate(&fs, ino, static_cast<uint64_t>(length));
    return 0;
}

int read(ffsp_fs& fs, const char* path, char* buf, size_t count,
         FUSE_OFF_T offset, fuse_file_info* fi)
{
    int rc;
    ffsp_inode* ino;

    if (ffsp_debug_is_debug_path(fs, path))
    {
        uint64_t read = 0;
        if (ffsp_debug_read(fs, path, buf, count, static_cast<uint64_t>(offset), read))
            return static_cast<int>(read);
        else
            return -EIO;
    }

    if (offset < 0)
        return -EINVAL;

    if (fi)
    {
        ino = get_inode(fi);
    }
    else
    {
        rc = ffsp_lookup(&fs, &ino, path);
        if (rc < 0)
            return rc;
    }

    ffsp_debug_update(fs, FFSP_DEBUG_FUSE_READ, count);
    return ffsp_read(&fs, ino, buf, count, static_cast<uint64_t>(offset));
}

int write(ffsp_fs& fs, const char* path, const char* buf, size_t count,
          FUSE_OFF_T offset, fuse_file_info* fi)
{
    int rc;
    ffsp_inode* ino;

    if (offset < 0)
        return -EINVAL;

    if (ffsp_debug_is_debug_path(fs, path))
        return -EPERM;

    if (fi)
    {
        ino = get_inode(fi);
    }
    else
    {
        rc = ffsp_lookup(&fs, &ino, path);
        if (rc < 0)
            return rc;
    }

    ffsp_debug_update(fs, FFSP_DEBUG_FUSE_WRITE, count);
    return ffsp_write(&fs, ino, buf, count, static_cast<uint64_t>(offset));
}

int mknod(ffsp_fs& fs, const char* path, mode_t mode, dev_t device)
{
    uid_t uid;
    gid_t gid;

    if (ffsp_debug_is_debug_path(fs, path))
        return -EPERM;

    uid = fuse_get_context()->uid;
    gid = fuse_get_context()->gid;

    // TODO: Make sure one and only one of the mode-flags is set.

    return ffsp_create(&fs, path, mode, uid, gid, device);
}

int link(ffsp_fs& fs, const char* oldpath, const char* newpath)
{
    if (ffsp_debug_is_debug_path(fs, oldpath) || ffsp_debug_is_debug_path(fs, newpath))
        return -EPERM;

    return ffsp_link(&fs, oldpath, newpath);
}

int symlink(ffsp_fs& fs, const char* oldpath, const char* newpath)
{
    uid_t uid;
    gid_t gid;

    if (ffsp_debug_is_debug_path(fs, oldpath) || ffsp_debug_is_debug_path(fs, newpath))
        return -EPERM;

    uid = fuse_get_context()->uid;
    gid = fuse_get_context()->gid;

    return ffsp_symlink(&fs, oldpath, newpath, uid, gid);
}

int readlink(ffsp_fs& fs, const char* path, char* buf, size_t bufsize)
{
    if (ffsp_debug_is_debug_path(fs, path))
        return -EPERM;

    return ffsp_readlink(&fs, path, buf, bufsize);
}

int mkdir(ffsp_fs& fs, const char* path, mode_t mode)
{
    uid_t uid;
    gid_t gid;

    if (ffsp_debug_is_debug_path(fs, path))
        return -EPERM;

    uid = fuse_get_context()->uid;
    gid = fuse_get_context()->gid;

    return ffsp_create(&fs, path, mode | S_IFDIR, uid, gid, 0);
}

int rmdir(ffsp_fs& fs, const char* path)
{
    if (ffsp_debug_is_debug_path(fs, path))
        return -EPERM;

    return ffsp_rmdir(&fs, path);
}

int unlink(ffsp_fs& fs, const char* path)
{
    if (ffsp_debug_is_debug_path(fs, path))
        return -EPERM;

    return ffsp_unlink(&fs, path);
}

int rename(ffsp_fs& fs, const char* oldpath, const char* newpath)
{
    if (ffsp_debug_is_debug_path(fs, oldpath) || ffsp_debug_is_debug_path(fs, newpath))
        return -EPERM;

    return ffsp_rename(&fs, oldpath, newpath);
}

int utimens(ffsp_fs& fs, const char* path, const struct timespec tv[2])
{
    int rc;
    ffsp_inode* ino;

    if (ffsp_debug_is_debug_path(fs, path))
        return -EPERM;

    rc = ffsp_lookup(&fs, &ino, path);
    if (rc < 0)
        return rc;

    ffsp_utimens(fs, *ino, tv);
    return 0;
}

int chmod(ffsp_fs& fs, const char* path, mode_t mode)
{
    int rc;
    ffsp_inode* ino;

    if (ffsp_debug_is_debug_path(fs, path))
        return -EPERM;

    rc = ffsp_lookup(&fs, &ino, path);
    if (rc < 0)
        return rc;

    ino->i_mode = put_be32(mode);
    ffsp_mark_dirty(&fs, ino);
    ffsp_flush_inodes(&fs, false);
    return 0;
}

int chown(ffsp_fs& fs, const char* path, uid_t uid, gid_t gid)
{
    int rc;
    ffsp_inode* ino;

    if (ffsp_debug_is_debug_path(fs, path))
        return -EPERM;

    rc = ffsp_lookup(&fs, &ino, path);
    if (rc < 0)
        return rc;

    ino->i_uid = put_be32(uid);
    ino->i_gid = put_be32(gid);
    ffsp_mark_dirty(&fs, ino);
    ffsp_flush_inodes(&fs, false);
    return 0;
}

int statfs(ffsp_fs& fs, const char* path, struct statvfs* sfs)
{
    if (ffsp_debug_is_debug_path(fs, path))
        return -EPERM;

    ffsp_statfs(fs, *sfs);
    return 0;
}

int flush(ffsp_fs& fs, const char* path, fuse_file_info* fi)
{
    (void)fi;

    if (ffsp_debug_is_debug_path(fs, path))
        return 0;

    // TODO: Implement Me!

    // Will write back the inode map and erase block usage data.

    return 0;
}

int fsync(ffsp_fs& fs, const char* path, int datasync, fuse_file_info* fi)
{
    (void)datasync;
    (void)fi;

    if (ffsp_debug_is_debug_path(fs, path))
        return 0;

    // TODO: Implement Me!

    // Will write back the file's dirty data.

    return 0;
}

} // namespace fuse_ffsp
