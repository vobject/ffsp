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

extern "C" {
#include "libffsp/ffsp.h"
#include "libffsp/log.h"
#include "libffsp/debug.h"
#include "libffsp/mount.h"
#include "libffsp/eraseblk.h"
#include "libffsp/inode.h"
#include "libffsp/io.h"
#include "libffsp/utils.h"
}

#include "spdlog/spdlog.h"

#include <fuse.h>

#include <atomic>
#include <string>

#include <sys/stat.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#ifdef _WIN32
#include <io.h>
#define S_ISDIR(m)	(((m)&S_IFDIR)==S_IFDIR)
#else
#include <unistd.h>
#define FUSE_OFF_T off_t
#endif

struct ffsp_params {
	std::string device;
} ffsp_params;

// Convert from fuse_file_info->fh to ffsp_inode...
static struct ffsp_inode *get_inode(const struct fuse_file_info *fi)
{
	return (struct ffsp_inode *)(size_t)fi->fh;
}
// ... and back to fuse_file_info->fh.
static void set_inode(struct fuse_file_info *fi, const struct ffsp_inode *ino)
{
	fi->fh = (size_t)ino;
}

static void *fuse_ffsp_init(struct fuse_conn_info *conn)
{
	ffsp *fs = new ffsp;

	if (ffsp_mount(fs, ffsp_params.device.c_str()) < 0) {
		FFSP_ERROR("ffsp_mount() failed. exiting...");
		exit(EXIT_FAILURE);
	}

#ifndef _WIN32
	if (conn->capable & FUSE_CAP_ATOMIC_O_TRUNC) {
		conn->want |= FUSE_CAP_ATOMIC_O_TRUNC;
	}

	if (conn->capable & FUSE_CAP_BIG_WRITES) {
		conn->want |= FUSE_CAP_BIG_WRITES;
		conn->max_write = fs->clustersize;
		FFSP_DEBUG("Setting max_write to %u", conn->max_write);
	}
#else
	conn->max_write = fs->clustersize;
	FFSP_DEBUG("Setting max_write to %u", conn->max_write);
#endif

	// TODO: Would it be ok to read all existing inode + dentry structs
	//  into memory at mount time?
	//  -> probably a good idea to be set via console arg to
	//      measure max memory usage.

	return fs;
}

static void fuse_ffsp_destroy(void *user)
{
	ffsp *fs = static_cast<ffsp *>(user);
	ffsp_unmount(fs);
	delete fs;
}

#ifdef _WIN32
static int fuse_ffsp_getattr(ffsp& fs, const char *path, struct FUSE_STAT *stbuf)
#else
static int fuse_ffsp_getattr(ffsp& fs, const char *path, struct stat *stbuf)
#endif
{
	int rc;
	struct ffsp_inode *ino;

	if (strncmp(path, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0) {
		ffsp_debug_fuse_stat(stbuf);
//		FFSP_DEBUG("fuse_ffsp_getattr(path=%s) with inode=%lld mode=0x%x nlink=%d size=%lld", path, stbuf->st_ino, stbuf->st_mode, stbuf->st_nlink, stbuf->st_size);
		return 0;
	}

	rc = ffsp_lookup(&fs, &ino, path);
	if (rc < 0)
		return rc;

#ifdef _WIN32
	struct stat stbuf_tmp;
	ffsp_stat(&fs, ino, &stbuf_tmp);

	memset(stbuf, 0, sizeof(*stbuf));
	stbuf->st_dev = stbuf_tmp.st_dev; /* dev_t <- _dev_t */
	stbuf->st_ino = stbuf_tmp.st_ino; /* uint64_t <- _ino_t */
	stbuf->st_mode = stbuf_tmp.st_mode; /* mode_t <- unsigned short */
	stbuf->st_nlink = stbuf_tmp.st_nlink; /* nlink_t <- short */
	stbuf->st_uid = stbuf_tmp.st_uid; /* uid_t <- short */
	stbuf->st_gid = stbuf_tmp.st_gid; /* gid_t <- short */
	stbuf->st_rdev = stbuf_tmp.st_rdev; /* dev_t <- _dev_t */
	stbuf->st_size = stbuf_tmp.st_size; /* FUSE_OFF_T <- _off_t */

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

//	stbuf->st_dev = stbuf_tmp.???; /* blksize_t <-  */
//	stbuf->st_dev = stbuf_tmp.???; /* blkcnt_t <-  */
//	stbuf->st_dev = stbuf_tmp.???; /* timestruc_t <-  */
#else
	ffsp_stat(&fs, ino, stbuf);
#endif

//	FFSP_DEBUG("fuse_ffsp_getattr(path=%s) with inode=%lld mode=0x%x nlink=%d size=%lld", path, stbuf->st_ino, stbuf->st_mode, stbuf->st_nlink, stbuf->st_size);
	return 0;
}

static int fuse_ffsp_readdir(ffsp& fs, const char *path, void *buf,
		fuse_fill_dir_t filler, FUSE_OFF_T offset,
		struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	int rc;
	struct ffsp_inode *ino;
	struct ffsp_dentry *dent_buf;
	int dent_cnt;

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

	for (int i = 0; i < dent_cnt; i++) {
		if (get_be32(dent_buf[i].ino) == FFSP_INVALID_INO_NO)
			continue; // Invalid ffsp_entry
		if (filler(buf, dent_buf[i].name, NULL, 0))
			FFSP_DEBUG("fuse_ffsp_readdir(): filler full!");
//		else
//			FFSP_DEBUG("fuse_ffsp_readdir(path=%s, offset=%lld) add %s", path, offset, dent_buf[i].name);
	}
	// TODO: Handle directory cache inside ffsp structure.
	free(dent_buf);
	return 0;
}

static int fuse_ffsp_open(ffsp& fs, const char *path, struct fuse_file_info *fi)
{
	int rc;
	struct ffsp_inode *ino;

	//FFSP_DEBUG("fuse_ffsp_open(path=%s, trunc=%s)", path,
	//		(fi->flags & O_TRUNC) ? "true" : "false");

	if (strncmp(path, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return 0;

	rc = ffsp_lookup(&fs, &ino, path);
	if (rc < 0)
		return rc;

	// TODO: Comment on why we explicitly made open & trunc atomic
	if (fi->flags & O_TRUNC) {
		rc = ffsp_truncate(&fs, ino, 0);
		if (rc < 0)
			return rc;
	}
	set_inode(fi, ino);

//	FFSP_DEBUG("fuse_ffsp_open(path=%s, trunc=%s) with flags=0x%x fh=0x%llx", path, (fi->flags & O_TRUNC) ? "true" : "false", fi->flags, fi->fh);
	return 0;
}

static int fuse_ffsp_release(ffsp& fs, const char *path, struct fuse_file_info *fi)
{
	(void) fs;
	(void) path;

	set_inode(fi, NULL);
	return 0;
}

static int fuse_ffsp_truncate(ffsp& fs, const char *path, FUSE_OFF_T length)
{
	int rc;
	struct ffsp_inode *ino;

	rc = ffsp_lookup(&fs, &ino, path);
	if (rc < 0)
		return rc;

	ffsp_truncate(&fs, ino, length);
	return 0;
}

static int fuse_ffsp_read(ffsp& fs, const char *path, char *buf, size_t count,
	FUSE_OFF_T offset, struct fuse_file_info *fi)
{
	int rc;
	struct ffsp_inode *ino;

	if (strncmp(path, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return ffsp_debug_get_info(buf, count);

	if ( fi ) {
		ino = get_inode(fi);
	} else {
		rc = ffsp_lookup(&fs, &ino, path);
		if (rc < 0)
			return rc;
	}

	ffsp_debug_update(FFSP_DEBUG_FUSE_READ, count);
	return ffsp_read(&fs, ino, buf, count, offset);
}

static int fuse_ffsp_write(ffsp& fs, const char *path, const char *buf, size_t count,
	FUSE_OFF_T offset, struct fuse_file_info *fi)
{
	int rc;
	struct ffsp_inode *ino;

	if (strncmp(path, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return -EIO;

	if ( fi ) {
		ino = get_inode(fi);
	} else {
		rc = ffsp_lookup(&fs, &ino, path);
		if (rc < 0)
			return rc;
	}

	ffsp_debug_update(FFSP_DEBUG_FUSE_WRITE, count);
	return ffsp_write(&fs, ino, buf, count, offset);
}

static int fuse_ffsp_mknod(ffsp& fs, const char *path, mode_t mode, dev_t device)
{
	uid_t uid;
	gid_t gid;

	if (strncmp(path, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return -EEXIST;

	uid = fuse_get_context()->uid;
	gid = fuse_get_context()->gid;

	// TODO: Make sure one and only one of the mode-flags is set.

	return ffsp_create(&fs, path, mode, uid, gid, device);
}

static int fuse_ffsp_link(ffsp& fs, const char *oldpath, const char *newpath)
{
	if (strncmp(newpath, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return -EIO;

	return ffsp_link(&fs, oldpath, newpath);
}

static int fuse_ffsp_symlink(ffsp& fs, const char *oldpath, const char *newpath)
{
	uid_t uid;
	gid_t gid;

	if (strncmp(newpath, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return -EIO;

	uid = fuse_get_context()->uid;
	gid = fuse_get_context()->gid;

	return ffsp_symlink(&fs, oldpath, newpath, uid, gid);
}

static int fuse_ffsp_readlink(ffsp& fs, const char *path, char *buf, size_t bufsize)
{
	return ffsp_readlink(&fs, path, buf, bufsize);
}

static int fuse_ffsp_mkdir(ffsp& fs, const char *path, mode_t mode)
{
	uid_t uid;
	gid_t gid;

	if (strncmp(path, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return -EIO;

	uid = fuse_get_context()->uid;
	gid = fuse_get_context()->gid;

	return ffsp_create(&fs, path, mode | S_IFDIR, uid, gid, 0);
}

static int fuse_ffsp_rmdir(ffsp& fs, const char *path)
{
	return ffsp_rmdir(&fs, path);
}

static int fuse_ffsp_unlink(ffsp& fs, const char *path)
{
	return ffsp_unlink(&fs, path);
}

static int fuse_ffsp_rename(ffsp& fs, const char *oldpath, const char *newpath)
{
	if (strncmp(newpath, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return -EIO;

	return ffsp_rename(&fs, oldpath, newpath);
}

static int fuse_ffsp_utimens(ffsp& fs, const char *path, const struct timespec tv[2])
{
	int rc;
	struct ffsp_inode *ino;

	rc = ffsp_lookup(&fs, &ino, path);
	if (rc < 0)
		return rc;

	ffsp_utimens(&fs, ino, tv);
	return 0;
}

static int fuse_ffsp_chmod(ffsp& fs, const char *path, mode_t mode)
{
	int rc;
	struct ffsp_inode *ino;

	rc = ffsp_lookup(&fs, &ino, path);
	if (rc < 0)
		return rc;

	ino->i_mode = put_be32(mode);
	ffsp_mark_dirty(&fs, ino);
	ffsp_flush_inodes(&fs, false);
	return 0;
}

static int fuse_ffsp_chown(ffsp& fs, const char *path, uid_t uid, gid_t gid)
{
	int rc;
	struct ffsp_inode *ino;

	rc = ffsp_lookup(&fs, &ino, path);
	if (rc < 0)
		return rc;

	ino->i_uid = put_be32(uid);
	ino->i_gid = put_be32(gid);
	ffsp_mark_dirty(&fs, ino);
	ffsp_flush_inodes(&fs, false);
	return 0;
}

static int fuse_ffsp_statfs(ffsp& fs, const char *path, struct statvfs *sfs)
{
	(void) path;

	ffsp_statfs(&fs, sfs);
	return 0;
}

static int fuse_ffsp_flush(ffsp& fs, const char *path, struct fuse_file_info *fi)
{
	(void) fs;
	(void) fi;

	if (strncmp(path, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return 0;

	// TODO: Implement Me!

	// Will write back the inode map and erase block usage data.

	return 0;
}

static int fuse_ffsp_fsync(ffsp& fs, const char *path, int datasync, struct fuse_file_info *fi)
{
	(void) fs;
	(void) datasync;
	(void) fi;

	if (strncmp(path, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return 0;

	// TODO: Implement Me!

	// Will write back the file's dirty data.

	return 0;
}

struct fuse_operations_wrapper
{
	fuse_operations_wrapper()
		: ops_()
	{
		ops_.getattr = [](const char *path, struct stat *stbuf)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} getattr(path={}, stbuf={})", id, path, static_cast<void*>(stbuf));
			int rc = fuse_ffsp_getattr(*fs, path, stbuf);
			logger_->info("< {} getattr(rc={})", id, rc);
			return rc;
		};

		ops_.readlink = [](const char *path, char *buf, size_t bufsize)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} readlink(path={}, buf={}, bufsize={})", id, path, static_cast<void*>(buf), bufsize);
			int rc = fuse_ffsp_readlink(*fs, path, buf, bufsize);
			logger_->info("< {} readlink(rc={})", id, rc);
			return rc;
		};

		ops_.mknod = [](const char *path, mode_t mode, dev_t device)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} mknod(path={}, mode={:#o}, device={})", id, path, mode, device);
			int rc = fuse_ffsp_mknod(*fs, path, mode, device);
			logger_->info("< {} mknod(rc={})", id, rc);
			return rc;
		};

		ops_.mkdir = [](const char *path, mode_t mode)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} mkdir(path={}, mode={:#o})", id, path, mode);
			int rc = fuse_ffsp_mkdir(*fs, path, mode);
			logger_->info("< {} mkdir(rc={})", id, rc);
			return rc;
		};

		ops_.unlink = [](const char *path)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} unlink(path={})", id, path);
			int rc = fuse_ffsp_unlink(*fs, path);
			logger_->info("< {} unlink(rc={})", id, rc);
			return rc;
		};

		ops_.rmdir = [](const char *path)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} rmdir(path={})", id, path);
			int rc = fuse_ffsp_rmdir(*fs, path);
			logger_->info("< {} rmdir(rc={})", id, rc);
			return rc;
		};

		ops_.symlink = [](const char *oldpath, const char *newpath)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} symlink(oldpath={}, newpath={})", id, oldpath, newpath);
			int rc = fuse_ffsp_symlink(*fs, oldpath, newpath);
			logger_->info("< {} symlink(rc={})", id, rc);
			return rc;
		};

		ops_.rename = [](const char *oldpath, const char *newpath)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} rename(oldpath={}, newpath={})", id, oldpath, newpath);
			int rc = fuse_ffsp_rename(*fs, oldpath, newpath);
			logger_->info("< {} rename(rc={})", id, rc);
			return rc;
		};

		ops_.link = [](const char *oldpath, const char *newpath)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} link(oldpath={}, newpath={})", id, oldpath, newpath);
			int rc = fuse_ffsp_link(*fs, oldpath, newpath);
			logger_->info("< {} link(rc={})", id, rc);
			return rc;
		};

		ops_.chmod = [](const char *path, mode_t mode)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} chmod(path={}, mode={:#o})", id, path, mode);
			int rc = fuse_ffsp_chmod(*fs, path, mode);
			logger_->info("< {} chmod(rc={})", id, rc);
			return rc;
		};

		ops_.chown = [](const char *path, uid_t uid, gid_t gid)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} chown(path={}, uid={}, gid={})", id, path, uid, gid);
			int rc = fuse_ffsp_chown(*fs, path, uid, gid);
			logger_->info("< {} chown(rc={})", id, rc);
			return rc;
		};

		ops_.truncate = [](const char *path, FUSE_OFF_T length)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} truncate(path={}, length={})", id, path, length);
			int rc = fuse_ffsp_truncate(*fs, path, length);
			logger_->info("< {} truncate(rc={})", id, rc);
			return rc;
		};

		ops_.open = [](const char *path, struct fuse_file_info *fi)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} open(path={}, fi={})", id, path, static_cast<void*>(fi));
			int rc = fuse_ffsp_open(*fs, path, fi);
			logger_->info("< {} open(rc={})", id, rc);
			return rc;
		};

		ops_.read = [](const char *path, char *buf, size_t count,
			FUSE_OFF_T offset, struct fuse_file_info *fi)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} read(path={}, buf={}, count={}, offset={}, fi={})", id, path, static_cast<void*>(buf), count, offset, static_cast<void*>(fi));
			int rc = fuse_ffsp_read(*fs, path, buf, count, offset, fi);
			logger_->info("< {} read(rc={})", id, rc);
			return rc;
		};

		ops_.write = [](const char *path, const char *buf, size_t count,
			FUSE_OFF_T offset, struct fuse_file_info *fi)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} write(path={}, buf={}, count={}, offset={}, fi={})", id, path, static_cast<const void*>(buf), count, offset, static_cast<void*>(fi));
			int rc = fuse_ffsp_write(*fs, path, buf, count, offset, fi);
			logger_->info("< {} write(rc={})", id, rc);
			return rc;
		};

		ops_.statfs = [](const char *path, struct statvfs *sfs)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} statfs(path={}, sfs={})", id, path, static_cast<void*>(sfs));
			int rc = fuse_ffsp_statfs(*fs, path, sfs);
			logger_->info("< {} statfs(rc={})", id, rc);
			return rc;
		};

		ops_.flush = [](const char *path, struct fuse_file_info *fi)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} flush(path={}, fi={})", id, path, static_cast<void*>(fi));
			int rc = fuse_ffsp_flush(*fs, path, fi);
			logger_->info("< {} flush(rc={})", id, rc);
			return rc;
		};

		ops_.release = [](const char *path, struct fuse_file_info *fi)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} release(path={}, fi={})", id, path, static_cast<void*>(fi));
			int rc = fuse_ffsp_release(*fs, path, fi);
			logger_->info("< {} release(rc={})", id, rc);
			return rc;
		};

		ops_.fsync = [](const char *path, int datasync, struct fuse_file_info *fi)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} fsync(path={}, datasync={}, fi={})", id, path, datasync, static_cast<void*>(fi));
			int rc = fuse_ffsp_fsync(*fs, path, datasync, fi);
			logger_->info("< {} fsync(rc={})", id, rc);
			return rc;
		};

		ops_.readdir = [](const char *path, void *buf,
				fuse_fill_dir_t filler, FUSE_OFF_T offset,
				struct fuse_file_info *fi)
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} readdir(path={}, buf={}, filler={}, offset={}, fi={})", id, path, buf, (filler != nullptr), offset, static_cast<void*>(fi));
			int rc = fuse_ffsp_readdir(*fs, path, buf, filler, offset, fi);
			logger_->info("< {} readdir(rc={})", id, rc);
			return rc;
		};

		ops_.init = [](struct fuse_conn_info *conn)
		{
			auto id = ++op_id_;
			logger_->info("> {} init(conn={})", id, static_cast<void*>(conn));
			void *private_data = fuse_ffsp_init(conn);
			logger_->info("< {} init(private_data={})", id, private_data);
			return private_data;
		};

		ops_.destroy = [](void *user)
		{
			auto id = ++op_id_;
			logger_->info("> {} destroy(user={})", id, user);
			fuse_ffsp_destroy(user);
			logger_->info("< {} destroy()", id);
		};

		ops_.utimens = [](const char *path, const struct timespec tv[2])
		{
			auto id = ++op_id_;
			auto fs = get_fs(fuse_get_context());
			logger_->info("> {} utimens(path={}, access={{sec={} nsec={}}}, mod={{sec={} nsec={}}})", id, path, tv[0].tv_sec, tv[0].tv_nsec, tv[1].tv_sec, tv[1].tv_nsec);
			int rc = fuse_ffsp_utimens(*fs, path, tv);
			logger_->info("< {} utimens(rc={})", id, rc);
			return rc;
		};

		ops_.getdir = nullptr; // deprecated
		ops_.utime = nullptr; // deprecated

		ops_.setxattr = nullptr;
		ops_.getxattr = nullptr;
		ops_.listxattr = nullptr;
		ops_.removexattr = nullptr;

		ops_.opendir = nullptr;
		ops_.releasedir = nullptr;
		ops_.fsyncdir = nullptr;

		ops_.access = nullptr;
		ops_.create = nullptr;

		ops_.ftruncate = nullptr;
		ops_.fgetattr = nullptr;

		ops_.lock = nullptr;
		ops_.bmap = nullptr;

#if FUSE_USE_VERSION >= 28
		ops_.ioctl = nullptr;
		ops_.poll = nullptr;
#endif

#if FUSE_USE_VERSION >= 29
		ops_.write_buf = nullptr;
		ops_.read_buf = nullptr;
		ops_.flock = nullptr;
		ops_.fallocate = nullptr;
#endif

#ifdef _WIN32
		ops_.win_get_attributes = nullptr; // TODO
		ops_.win_set_attributes = nullptr; // TODO
		ops_.win_set_times = nullptr; // TODO
#endif
	}

	static struct ffsp *get_fs(struct fuse_context *ctx)
	{
		return (ffsp *)ctx->private_data;
	}

	fuse_operations ops_;

	// every operation (aka FUSE API call) has a unique id
	static std::atomic_uint op_id_;

	static std::shared_ptr<spdlog::logger> logger_;
};
std::atomic_uint fuse_operations_wrapper::op_id_;
std::shared_ptr<spdlog::logger> fuse_operations_wrapper::logger_ = spdlog::stdout_logger_mt("console");

static void show_usage(const char *progname)
{
	printf("Usage:\n");
	printf("%s DEVICE MOUNTPOINT\n", progname);
	printf("%s -h, --help        display this help and exit\n", progname);
	printf("%s -V, -version      print version and exit\n", progname);
}

static void show_version(const char *progname)
{
	printf("FUSE %s version %d.%d.%d\n", progname, FFSP_VERSION_MAJOR,
			FFSP_VERSION_MINOR, FFSP_VERSION_PATCH);
}

enum {
	KEY_HELP,
	KEY_VERSION,
};

static struct fuse_opt ffsp_opt[] = {
	FUSE_OPT_KEY("-h", KEY_HELP),
	FUSE_OPT_KEY("--help", KEY_HELP),
	FUSE_OPT_KEY("-V", KEY_VERSION),
	FUSE_OPT_KEY("--version", KEY_VERSION),
	FUSE_OPT_END,
};

static int fuse_ffsp_opt_proc(void *data, const char *arg, int key,
		struct fuse_args *outargs)
{
	(void) data;

	switch (key) {
		case FUSE_OPT_KEY_NONOPT:
			if (ffsp_params.device.empty()) {
				ffsp_params.device = arg;
				return 0;
			}
			return 1;

		case KEY_HELP:
			show_usage(outargs->argv[0]);
			exit(EXIT_SUCCESS);

		case KEY_VERSION:
			show_version(outargs->argv[0]);
			exit(EXIT_SUCCESS);
	}
	return 1;
}

int main(int argc, char *argv[])
{
	fuse_args args = FUSE_ARGS_INIT(argc, argv);
	fuse_operations_wrapper ffsp_oper;

	if (fuse_opt_parse(&args, NULL, ffsp_opt, fuse_ffsp_opt_proc) == -1) {
		printf("fuse_opt_parse() failed!\n");
		return EXIT_FAILURE;
	}

	if (ffsp_params.device.empty()) {
		printf("device argument missing\n");
		return EXIT_FAILURE;
	}

	if (fuse_opt_add_arg(&args, "-odefault_permissions") == -1) {
		printf("fuse_opt_add_arg() failed!\n");
		return EXIT_FAILURE;
	}

	int rc = fuse_main(args.argc, args.argv, &ffsp_oper.ops_, nullptr);

	fuse_opt_free_args(&args);
	spdlog::drop_all();
	return rc;
}
