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

#define FUSE_USE_VERSION 26

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

static struct ffsp_params {
	char *device;
} ffsp_params;

static struct ffsp fs; // Global access point into the file system.

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
#ifndef _WIN32
	if (conn->capable & FUSE_CAP_ATOMIC_O_TRUNC)
		conn->want |= FUSE_CAP_ATOMIC_O_TRUNC;
#endif

	if (ffsp_mount(&fs, ffsp_params.device) < 0) {
		FFSP_ERROR("ffsp_mount() failed. exiting...");
		exit(EXIT_FAILURE);
	}

#ifndef _WIN32
	if (conn->capable & FUSE_CAP_BIG_WRITES) {
		conn->want |= FUSE_CAP_BIG_WRITES;
		conn->max_write = fs.clustersize;
		FFSP_DEBUG("Setting max_write to %u", conn->max_write);
	}
#else
	conn->max_write = fs.clustersize;
	FFSP_DEBUG("Setting max_write to %u", conn->max_write);
#endif

	// TODO: Would it be ok to read all existing inode + dentry structs
	//  into memory at mount time?
	//  -> probably a good idea to be set via console arg to
	//      measure max memory usage.

	return NULL;
}

static void fuse_ffsp_destroy(void *user)
{
	(void) user;

	ffsp_unmount(&fs);
}

#ifdef _WIN32
static int fuse_ffsp_getattr(const char *path, struct FUSE_STAT *stbuf)
#else
static int fuse_ffsp_getattr(const char *path, struct stat *stbuf)
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

static int fuse_ffsp_readdir(const char *path, void *buf,
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

static int fuse_ffsp_open(const char *path, struct fuse_file_info *fi)
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

static int fuse_ffsp_release(const char *path, struct fuse_file_info *fi)
{
	(void) path;

	set_inode(fi, NULL);
	return 0;
}

static int fuse_ffsp_truncate(const char *path, FUSE_OFF_T length)
{
	int rc;
	struct ffsp_inode *ino;

	rc = ffsp_lookup(&fs, &ino, path);
	if (rc < 0)
		return rc;

	ffsp_truncate(&fs, ino, length);
	return 0;
}

static int fuse_ffsp_read(const char *path, char *buf, size_t count,
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

static int fuse_ffsp_write(const char *path, const char *buf, size_t count,
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

static int fuse_ffsp_mknod(const char *path, mode_t mode, dev_t device)
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

static int fuse_ffsp_link(const char *oldpath, const char *newpath)
{
	if (strncmp(newpath, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return -EIO;

	return ffsp_link(&fs, oldpath, newpath);
}

static int fuse_ffsp_symlink(const char *oldpath, const char *newpath)
{
	uid_t uid;
	gid_t gid;

	if (strncmp(newpath, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return -EIO;

	uid = fuse_get_context()->uid;
	gid = fuse_get_context()->gid;

	return ffsp_symlink(&fs, oldpath, newpath, uid, gid);
}

static int fuse_ffsp_readlink(const char *path, char *buf, size_t len)
{
	return ffsp_readlink(&fs, path, buf, len);
}

static int fuse_ffsp_mkdir(const char *path, mode_t mode)
{
	uid_t uid;
	gid_t gid;

	//FFSP_DEBUG("fuse_ffsp_mkdir(path=%s, mode=0x%x)", path, mode);

	if (strncmp(path, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return -EIO;

	uid = fuse_get_context()->uid;
	gid = fuse_get_context()->gid;

	return ffsp_create(&fs, path, mode | S_IFDIR, uid, gid, 0);
}

static int fuse_ffsp_rmdir(const char *path)
{
	return ffsp_rmdir(&fs, path);
}

static int fuse_ffsp_unlink(const char *path)
{
	return ffsp_unlink(&fs, path);
}

static int fuse_ffsp_rename(const char *oldpath, const char *newpath)
{
	if (strncmp(newpath, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return -EIO;

	return ffsp_rename(&fs, oldpath, newpath);
}

static int fuse_ffsp_utimens(const char *path, const struct timespec tv[2])
{
	int rc;
	struct ffsp_inode *ino;

	rc = ffsp_lookup(&fs, &ino, path);
	if (rc < 0)
		return rc;

	ffsp_utimens(&fs, ino, tv);
	return 0;
}

static int fuse_ffsp_chmod(const char *path, mode_t mode)
{
	int rc;
	struct ffsp_inode *ino;

	//FFSP_DEBUG("fuse_ffsp_chmod(path=%s, mode=0x%x)", path, mode);

	rc = ffsp_lookup(&fs, &ino, path);
	if (rc < 0)
		return rc;

	ino->i_mode = put_be32(mode);
	ffsp_mark_dirty(&fs, ino);
	ffsp_flush_inodes(&fs, false);
	return 0;
}

static int fuse_ffsp_chown(const char *path, uid_t uid, gid_t gid)
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

static int fuse_ffsp_statfs(const char *path, struct statvfs *sfs)
{
	(void) path;

	ffsp_statfs(&fs, sfs);
	return 0;
}

static int fuse_ffsp_flush(const char *path, struct fuse_file_info *fi)
{
	(void) fi;

	if (strncmp(path, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return 0;

	// TODO: Implement Me!

	// Will write back the inode map and erase block usage data.

	return 0;
}

static int fuse_ffsp_fsync(const char *path, int xxx, struct fuse_file_info *fi)
{
	(void) xxx;
	(void) fi;

	if (strncmp(path, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return 0;

	// TODO: Implement Me!

	// Will write back the file's dirty data.

	return 0;
}

struct fuse_operations_logging
{
	fuse_operations_logging()
		: ops_()
	{
		ops_.getattr = [](const char *path, struct stat *stbuf)
		{
			logger_->info("> getattr(path={})", path);
			int rc = fuse_ffsp_getattr(path, stbuf);
			logger_->info("< getattr(rc={})", rc);
			return rc;
		};

		ops_.readlink = [](const char *path, char *buf, size_t len)
		{
			logger_->info("> readlink(path={})", path);
			int rc = fuse_ffsp_readlink(path, buf, len);
			logger_->info("< readlink(rc={})", rc);
			return rc;
		};

		ops_.mknod = [](const char *path, mode_t mode, dev_t device)
		{
			logger_->info("> mknod(path={})", path);
			int rc = fuse_ffsp_mknod(path, mode, device);
			logger_->info("< mknod(rc={})", rc);
			return rc;
		};

		ops_.mkdir = [](const char *path, mode_t mode)
		{
			logger_->info("> mkdir(path={})", path);
			int rc = fuse_ffsp_mkdir(path, mode);
			logger_->info("< mkdir(rc={})", rc);
			return rc;
		};

		ops_.unlink = [](const char *path)
		{
			logger_->info("> unlink(path={})", path);
			int rc = fuse_ffsp_unlink(path);
			logger_->info("< unlink(rc={})", rc);
			return rc;
		};

		ops_.rmdir = [](const char *path)
		{
			logger_->info("> rmdir(path={})", path);
			int rc = fuse_ffsp_rmdir(path);
			logger_->info("< rmdir(rc={})", rc);
			return rc;
		};

		ops_.symlink = [](const char *oldpath, const char *newpath)
		{
			logger_->info("> symlink(oldpath={}, newpath={})", oldpath, newpath);
			int rc = fuse_ffsp_symlink(oldpath, newpath);
			logger_->info("< symlink(rc={})", rc);
			return rc;
		};

		ops_.rename = [](const char *oldpath, const char *newpath)
		{
			logger_->info("> rename(oldpath={}, newpath={})", oldpath, newpath);
			int rc = fuse_ffsp_rename(oldpath, newpath);
			logger_->info("< rename(rc={})", rc);
			return rc;
		};

		ops_.link = [](const char *oldpath, const char *newpath)
		{
			logger_->info("> link(oldpath={}, newpath={})", oldpath, newpath);
			int rc = fuse_ffsp_link(oldpath, newpath);
			logger_->info("< link(rc={})", rc);
			return rc;
		};

		ops_.chmod = [](const char *path, mode_t mode)
		{
			logger_->info("> chmod(path={})", path);
			int rc = fuse_ffsp_chmod(path, mode);
			logger_->info("< chmod(rc={})", rc);
			return rc;
		};

		ops_.chown = [](const char *path, uid_t uid, gid_t gid)
		{
			logger_->info("> chown(path={})", path);
			int rc = fuse_ffsp_chown(path, uid, gid);
			logger_->info("< chown(rc={})", rc);
			return rc;
		};

		ops_.truncate = [](const char *path, FUSE_OFF_T length)
		{
			logger_->info("> truncate(path={})", path);
			int rc = fuse_ffsp_truncate(path, length);
			logger_->info("< truncate(rc={})", rc);
			return rc;
		};

		ops_.open = [](const char *path, struct fuse_file_info *fi)
		{
			logger_->info("> open(path={})", path);
			int rc = fuse_ffsp_open(path, fi);
			logger_->info("< open(rc={})", rc);
			return rc;
		};

		ops_.read = [](const char *path, char *buf, size_t count,
			FUSE_OFF_T offset, struct fuse_file_info *fi)
		{
			logger_->info("> read(path={})", path);
			int rc = fuse_ffsp_read(path, buf, count, offset, fi);
			logger_->info("< read(rc={})", rc);
			return rc;
		};

		ops_.write = [](const char *path, const char *buf, size_t count,
			FUSE_OFF_T offset, struct fuse_file_info *fi)
		{
			logger_->info("> write(path={})", path);
			int rc = fuse_ffsp_write(path, buf, count, offset, fi);
			logger_->info("< write(rc={})", rc);
			return rc;
		};

		ops_.statfs = [](const char *path, struct statvfs *sfs)
		{
			logger_->info("> statfs(path={})", path);
			int rc = fuse_ffsp_statfs(path, sfs);
			logger_->info("< statfs(rc={})", rc);
			return rc;
		};

		ops_.flush = [](const char *path, struct fuse_file_info *fi)
		{
			logger_->info("> flush(path={})", path);
			int rc = fuse_ffsp_flush(path, fi);
			logger_->info("< flush(rc={})", rc);
			return rc;
		};

		ops_.release = [](const char *path, struct fuse_file_info *fi)
		{
			logger_->info("> release(path={})", path);
			int rc = fuse_ffsp_release(path, fi);
			logger_->info("< release(rc={})", rc);
			return rc;
		};

		ops_.fsync = [](const char *path, int xxx, struct fuse_file_info *fi)
		{
			logger_->info("> fsync(path={})", path);
			int rc = fuse_ffsp_fsync(path, xxx, fi);
			logger_->info("< fsync(rc={})", rc);
			return rc;
		};

		ops_.readdir = [](const char *path, void *buf,
				fuse_fill_dir_t filler, FUSE_OFF_T offset,
				struct fuse_file_info *fi)
		{
			logger_->info("> readdir(path={})", path);
			int rc = fuse_ffsp_readdir(path, buf, filler, offset, fi);
			logger_->info("< readdir(rc={})", rc);
			return rc;
		};

		ops_.init = [](struct fuse_conn_info *conn)
		{
			logger_->info("> init()");
			fuse_ffsp_init(conn);
			logger_->info("< init()");
			return (void*)NULL;
		};

		ops_.destroy = [](void *user)
		{
			logger_->info("> destroy()");
			fuse_ffsp_destroy(user);
			logger_->info("< destroy()");
		};

		ops_.utimens = [](const char *path, const struct timespec tv[2])
		{
			logger_->info("> utimens(path={})", path);
			int rc = fuse_ffsp_utimens(path, tv);
			logger_->info("< utimens(rc={})", rc);
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

#if FUSE_VERSION >= 28
		ops_.ioctl = nullptr;
		ops_.poll = nullptr;
#endif

#if FUSE_VERSION >= 29
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

		ops_.flag_nullpath_ok = 0;
		ops_.flag_nopath = 0;
		ops_.flag_utime_omit_ok = 0;
	}

	fuse_operations ops_;
	static std::shared_ptr<spdlog::logger> logger_;
};
std::shared_ptr<spdlog::logger> fuse_operations_logging::logger_ = spdlog::stdout_logger_mt("console");

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
			if (!ffsp_params.device) {
				ffsp_params.device = strdup(arg);
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
	fuse_operations_logging ffsp_oper;

	memset(&ffsp_params, 0, sizeof(ffsp_params));

	if (fuse_opt_parse(&args, NULL, ffsp_opt, fuse_ffsp_opt_proc) == -1) {
		printf("fuse_opt_parse() failed!\n");
		return EXIT_FAILURE;
	}

	if (!ffsp_params.device) {
		printf("device argument missing\n");
		return EXIT_FAILURE;
	}

	if (fuse_opt_add_arg(&args, "-odefault_permissions") == -1) {
		printf("fuse_opt_add_arg() failed!\n");
		free(ffsp_params.device);
		return EXIT_FAILURE;
	}

	int rc = fuse_main(args.argc, args.argv, &ffsp_oper.ops_, NULL);

	free(ffsp_params.device);
	fuse_opt_free_args(&args);
	return rc;
}
