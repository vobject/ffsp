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

namespace fuse_api_logging
{
	auto console = spdlog::stdout_logger_mt("console");

	struct init
	{
		init(struct fuse_conn_info *conn)
		{
			console->info("> init()");
		}

		~init()
		{
			console->info("< init()");
		}
	};

	struct destroy
	{
		destroy(void *user)
		{
			console->info("> destroy(user={})", user);
		}

		~destroy()
		{
			console->info("< destroy()");
		}
	};

	struct getattr
	{
		getattr(const char *path, struct stat *stbuf)
		{
			console->info("> getattr(path={})", path);
		}

		~getattr()
		{
			console->info("< getattr()");
		}
	};

	struct readdir
	{
		readdir(const char *path, void *buf,
			fuse_fill_dir_t filler, FUSE_OFF_T offset,
			struct fuse_file_info *fi)
		{
			console->info("> readdir(path={})", path);
		}

		~readdir()
		{
			console->info("< readdir()");
		}
	};

	struct open
	{
		open(const char *path, struct fuse_file_info *fi)
		{
			console->info("> open(path={})", path);
		}

		~open()
		{
			console->info("< open()");
		}
	};

	struct release
	{
		release(const char *path, struct fuse_file_info *fi)
		{
			console->info("> release(path={})", path);
		}

		~release()
		{
			console->info("< release()");
		}
	};

	struct truncate
	{
		truncate(const char *path, FUSE_OFF_T length)
		{
			console->info("> truncate(path={})", path);
		}

		~truncate()
		{
			console->info("< truncate()");
		}
	};

	struct read
	{
		read(const char *path, char *buf, size_t count,
			FUSE_OFF_T offset, struct fuse_file_info *fi)
		{
			console->info("> read(path={})", path);
		}

		~read()
		{
			console->info("< read()");
		}
	};

	struct write
	{
		write(const char *path, const char *buf, size_t count,
			FUSE_OFF_T offset, struct fuse_file_info *fi)
		{
			console->info("> write(path={})", path);
		}

		~write()
		{
			console->info("< write()");
		}
	};

	struct mknod
	{
		mknod(const char *path, mode_t mode, dev_t device)
		{
			console->info("> mknod(path={})", path);
		}

		~mknod()
		{
			console->info("< mknod()");
		}
	};

	struct link
	{
		link(const char *oldpath, const char *newpath)
		{
			console->info("> link(oldpath={}, newpath={})", oldpath, newpath);
		}

		~link()
		{
			console->info("< link()");
		}
	};

	struct symlink
	{
		symlink(const char *oldpath, const char *newpath)
		{
			console->info("> symlink(oldpath={}, newpath={})", oldpath, newpath);
		}

		~symlink()
		{
			console->info("< symlink()");
		}
	};

	struct readlink
	{
		readlink(const char *path, char *buf, size_t len)
		{
			console->info("> readlink(path={})", path);
		}

		~readlink()
		{
			console->info("< readlink()");
		}
	};

	struct mkdir
	{
		mkdir(const char *path, mode_t mode)
		{
			console->info("> mkdir(path={})", path);
		}

		~mkdir()
		{
			console->info("< mkdir()");
		}
	};

	struct rmdir
	{
		rmdir(const char *path)
		{
			console->info("> rmdir(path={})", path);
		}

		~rmdir()
		{
			console->info("< rmdir()");
		}
	};

	struct unlink
	{
		unlink(const char *path)
		{
			console->info("> unlink(path={})", path);
		}

		~unlink()
		{
			console->info("< unlink()");
		}
	};

	struct rename
	{
		rename(const char *oldpath, const char *newpath)
		{
			console->info("> rename(oldpath={}, newpath={})", oldpath, newpath);
		}

		~rename()
		{
			console->info("< rename()");
		}
	};

	struct utimens
	{
		utimens(const char *path, const struct timespec tv[2])
		{
			console->info("> utimens(path={})", path);
		}

		~utimens()
		{
			console->info("< utimens()");
		}
	};

	struct chmod
	{
		chmod(const char *path, mode_t mode)
		{
			console->info("> chmod(path={})", path);
		}

		~chmod()
		{
			console->info("< chmod()");
		}
	};

	struct chown
	{
		chown(const char *path, uid_t uid, gid_t gid)
		{
			console->info("> chown(path={})", path);
		}

		~chown()
		{
			console->info("< chown()");
		}
	};

	struct statfs
	{
		statfs(const char *path, struct statvfs *sfs)
		{
			console->info("> statfs(path={})", path);
		}

		~statfs()
		{
			console->info("< statfs()");
		}
	};

	struct flush
	{
		flush(const char *path, struct fuse_file_info *fi)
		{
			console->info("> flush(path={})", path);
		}

		~flush()
		{
			console->info("< flush()");
		}
	};

	struct fsync
	{
		fsync(const char *path, int xxx, struct fuse_file_info *fi)
		{
			console->info("> fsync(path={})", path);
		}

		~fsync()
		{
			console->info("< fsync()");
		}
	};
}

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
	fuse_api_logging::init log(conn);

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
	fuse_api_logging::destroy log(user);

	ffsp_unmount(&fs);
}

#ifdef _WIN32
static int fuse_ffsp_getattr(const char *path, struct FUSE_STAT *stbuf)
#else
static int fuse_ffsp_getattr(const char *path, struct stat *stbuf)
#endif
{
	fuse_api_logging::getattr log(path, stbuf);

	int rc;
	struct ffsp_inode *ino;

	if (strncmp(path, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0) {
		ffsp_debug_fuse_stat(stbuf);
		FFSP_DEBUG("fuse_ffsp_getattr(path=%s) with inode=%lld mode=0x%x nlink=%d size=%lld", path, stbuf->st_ino, stbuf->st_mode, stbuf->st_nlink, stbuf->st_size);
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

	FFSP_DEBUG("fuse_ffsp_getattr(path=%s) with inode=%lld mode=0x%x nlink=%d size=%lld", path, stbuf->st_ino, stbuf->st_mode, stbuf->st_nlink, stbuf->st_size);
	return 0;
}

static int fuse_ffsp_readdir(const char *path, void *buf,
		fuse_fill_dir_t filler, FUSE_OFF_T offset,
		struct fuse_file_info *fi)
{
	fuse_api_logging::readdir log(path, buf, filler, offset, fi);

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
		else
			FFSP_DEBUG("fuse_ffsp_readdir(path=%s, offset=%lld) add %s", path, offset, dent_buf[i].name);
	}
	// TODO: Handle directory cache inside ffsp structure.
	free(dent_buf);
	return 0;
}

static int fuse_ffsp_open(const char *path, struct fuse_file_info *fi)
{
	fuse_api_logging::open log(path, fi);

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

	FFSP_DEBUG("fuse_ffsp_open(path=%s, trunc=%s) with flags=0x%x fh=0x%llx", path, (fi->flags & O_TRUNC) ? "true" : "false", fi->flags, fi->fh);
	return 0;
}

static int fuse_ffsp_release(const char *path, struct fuse_file_info *fi)
{
	fuse_api_logging::release log(path, fi);

	set_inode(fi, NULL);
	return 0;
}

static int fuse_ffsp_truncate(const char *path, FUSE_OFF_T length)
{
	fuse_api_logging::truncate log(path, length);

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
	fuse_api_logging::read log(path, buf, count, offset, fi);

	int rc;
	struct ffsp_inode *ino;

	if (strncmp(path, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return ffsp_debug_get_info(buf, count);

	rc = ffsp_lookup(&fs, &ino, path);
	if (rc < 0)
		return rc;

	ffsp_debug_update(FFSP_DEBUG_FUSE_READ, count);
	return ffsp_read(&fs, ino, buf, count, offset);
}

static int fuse_ffsp_write(const char *path, const char *buf, size_t count,
	FUSE_OFF_T offset, struct fuse_file_info *fi)
{
	fuse_api_logging::write log(path, buf, count, offset, fi);

	int rc;
	struct ffsp_inode *ino;

	if (strncmp(path, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return -EIO;

	rc = ffsp_lookup(&fs, &ino, path);
	if (rc < 0)
		return rc;

	ffsp_debug_update(FFSP_DEBUG_FUSE_WRITE, count);
	return ffsp_write(&fs, ino, buf, count, offset);
}

static int fuse_ffsp_mknod(const char *path, mode_t mode, dev_t device)
{
	fuse_api_logging::mknod log(path, mode, device);

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
	fuse_api_logging::link log(oldpath, newpath);

	if (strncmp(newpath, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return -EIO;

	return ffsp_link(&fs, oldpath, newpath);
}

static int fuse_ffsp_symlink(const char *oldpath, const char *newpath)
{
	fuse_api_logging::symlink log(oldpath, newpath);

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
	fuse_api_logging::readlink log(path, buf, len);

	return ffsp_readlink(&fs, path, buf, len);
}

static int fuse_ffsp_mkdir(const char *path, mode_t mode)
{
	fuse_api_logging::mkdir log(path, mode);

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
	fuse_api_logging::rmdir log(path);

	return ffsp_rmdir(&fs, path);
}

static int fuse_ffsp_unlink(const char *path)
{
	fuse_api_logging::unlink log(path);

	return ffsp_unlink(&fs, path);
}

static int fuse_ffsp_rename(const char *oldpath, const char *newpath)
{
	fuse_api_logging::rename log(oldpath, newpath);

	if (strncmp(newpath, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return -EIO;

	return ffsp_rename(&fs, oldpath, newpath);
}

static int fuse_ffsp_utimens(const char *path, const struct timespec tv[2])
{
	fuse_api_logging::utimens log(path, tv);

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
	fuse_api_logging::chmod log(path, mode);

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
	fuse_api_logging::chown log(path, uid, gid);

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
	fuse_api_logging::statfs log(path, sfs);

	ffsp_statfs(&fs, sfs);
	return 0;
}

static int fuse_ffsp_flush(const char *path, struct fuse_file_info *fi)
{
	fuse_api_logging::flush log(path, fi);

	if (strncmp(path, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return 0;

	// TODO: Implement Me!

	// Will write back the inode map and erase block usage data.

	return 0;
}

static int fuse_ffsp_fsync(const char *path, int xxx, struct fuse_file_info *fi)
{
	fuse_api_logging::fsync log(path, xxx, fi);

	if (strncmp(path, FFSP_DEBUG_FILE, FFSP_NAME_MAX) == 0)
		return 0;

	// TODO: Implement Me!

	// Will write back the file's dirty data.

	return 0;
}

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
	int rc;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_operations ffsp_oper;

	memset(&ffsp_params, 0, sizeof(ffsp_params));

	if (fuse_opt_parse(&args, NULL, ffsp_opt, fuse_ffsp_opt_proc) == -1) {
		printf("fuse_opt_parse() failed!\n");
		return EXIT_FAILURE;
	}

	if (!ffsp_params.device) {
		printf("device argument missing\n");
		return EXIT_FAILURE;
	}

//	if (fuse_opt_add_arg(&args, "-odefault_permissions") == -1) {
//		printf("fuse_opt_add_arg() failed!\n");
//		free(ffsp_params.device);
//		return EXIT_FAILURE;
//	}

	memset(&ffsp_oper, 0, sizeof(struct fuse_operations));
	ffsp_oper.getattr = fuse_ffsp_getattr; // Similar to stat()
	ffsp_oper.readlink = fuse_ffsp_readlink; // Read the target of a symbolic link
	ffsp_oper.getdir = NULL; // deprecated
	ffsp_oper.mknod = fuse_ffsp_mknod;
	ffsp_oper.mkdir = fuse_ffsp_mkdir;
	ffsp_oper.unlink = fuse_ffsp_unlink;
	ffsp_oper.rmdir = fuse_ffsp_rmdir;
	ffsp_oper.symlink = fuse_ffsp_symlink;
	ffsp_oper.rename = fuse_ffsp_rename;
	ffsp_oper.link = fuse_ffsp_link;
	ffsp_oper.chmod = fuse_ffsp_chmod;
	ffsp_oper.chown = fuse_ffsp_chown;
	ffsp_oper.truncate = fuse_ffsp_truncate;
	ffsp_oper.utime = NULL; // deprecated
	ffsp_oper.open = fuse_ffsp_open; // No O_CREAT, E_EXCL or O_TRUNC will be passed
	ffsp_oper.read = fuse_ffsp_read;
	ffsp_oper.write = fuse_ffsp_write;
	ffsp_oper.statfs = fuse_ffsp_statfs;
	ffsp_oper.flush = fuse_ffsp_flush;
	ffsp_oper.release = fuse_ffsp_release;
	ffsp_oper.fsync = fuse_ffsp_fsync;
//	ffsp_oper.setxattr = NULL;
//	ffsp_oper.getxattr = NULL;
//	ffsp_oper.listxattr = NULL;
//	ffsp_oper.removexattr = NULL;
	ffsp_oper.opendir = NULL;
	ffsp_oper.readdir = fuse_ffsp_readdir;
	ffsp_oper.releasedir = NULL;
	ffsp_oper.fsyncdir = NULL;
	ffsp_oper.init = fuse_ffsp_init;
	ffsp_oper.destroy = fuse_ffsp_destroy;
	ffsp_oper.access = NULL; // TODO
	ffsp_oper.create = NULL; // TODO
	ffsp_oper.ftruncate = NULL;
	ffsp_oper.fgetattr = NULL;
	ffsp_oper.lock = NULL;
	ffsp_oper.utimens = fuse_ffsp_utimens;
	ffsp_oper.bmap = NULL;
//	ffsp_oper.ioctl = NULL;
//	ffsp_oper.poll = NULL;
#ifdef _WIN32
	ffsp_oper.win_get_attributes = NULL; // TODO
	ffsp_oper.win_set_attributes = NULL; // TODO
	ffsp_oper.win_set_times = NULL; // TODO
#endif

	rc = fuse_main(args.argc, args.argv, &ffsp_oper, NULL);

	free(ffsp_params.device);
	fuse_opt_free_args(&args);
	return rc;
}
