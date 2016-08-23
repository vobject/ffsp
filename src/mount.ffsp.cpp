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
}

#include "fuse_ffsp.hpp"

#include "spdlog/spdlog.h"
#include "spdlog/fmt/bundled/ostream.h"

#include <atomic>
#include <string>

#ifdef _WIN32
std::ostream& operator<<(std::ostream& os, const struct FUSE_STAT& stat)
#else
std::ostream& operator<<(std::ostream& os, const struct stat& stat)
#endif
{
    return os << "{"
              << "dev=" << stat.st_dev
              << ", ino=" << stat.st_ino
              << ", nlink=" << stat.st_nlink
              << ", mode=0x" << std::hex << stat.st_mode << std::dec
              << ", uid=" << stat.st_uid
              << ", gid=" << stat.st_gid
              << ", rdev=" << stat.st_rdev
              << ", size=" << stat.st_size
              << ", blksize=" << stat.st_blksize
              << ", blocks=" << stat.st_blocks
#ifdef _WIN32
              << ", atime=" << stat.st_atim.tv_sec << "." << stat.st_atim.tv_nsec
              << ", mtime=" << stat.st_mtim.tv_sec << "." << stat.st_mtim.tv_nsec
              << ", ctime=" << stat.st_ctim.tv_sec << "." << stat.st_ctim.tv_nsec
              << ", birthtim=" << stat.st_birthtim.tv_sec << "." << stat.st_birthtim.tv_nsec
#else
              << ", atime=" << stat.st_atime
              << ", mtime=" << stat.st_mtime
              << ", ctime=" << stat.st_ctime
#endif
              << "}";
}

std::ostream& operator<<(std::ostream& os, const struct fuse_file_info& fi)
{
    return os << "{"
              << "flags=0x" << std::hex << fi.flags << std::dec
              << ", fh=0x" << std::hex << fi.fh << std::dec
              << ", fh_old=0x" << std::hex << fi.fh_old << std::dec
              << ", lock_owner=" << fi.lock_owner
              << ", writepage=" << fi.writepage
              << ", direct_io=" << fi.direct_io
              << ", keep_cache=" << fi.keep_cache
              << ", flush=" << fi.flush
#ifndef _WIN32
              << ", nonseekable=" << fi.nonseekable
              << ", flock_release=" << fi.flock_release
#endif
              << "}";
}

std::ostream& operator<<(std::ostream& os, const struct statvfs& sfs)
{
    return os << "{"
              << "bsize=" << sfs.f_bsize
              << ", frsize=" << sfs.f_frsize
              << ", blocks=" << sfs.f_blocks
              << ", bfree=" << sfs.f_bfree
              << ", bavail=" << sfs.f_bavail
              << ", files=" << sfs.f_files
              << ", ffree=" << sfs.f_ffree
              << ", favail=" << sfs.f_favail
              << ", fsid=" << sfs.f_fsid
              << ", flag=" << sfs.f_flag
              << ", namemax=" << sfs.f_namemax
              << "}";
}

std::ostream& operator<<(std::ostream& os, const struct fuse_conn_info& conn)
{
    return os << "{"
              << "protocol_version=" << conn.proto_major << "." << conn.proto_minor
              << ", async_read=" << conn.async_read
              << ", max_write=" << conn.max_write
              << ", max_readahead=" << conn.max_readahead
#ifndef _WIN32
              << ", capable=0x" << std::hex << conn.capable << std::dec
              << ", want=0x" << std::hex << conn.want << std::dec
              << ", max_background=" << conn.max_background
              << ", congestion_threshold=" << conn.congestion_threshold
#endif
              << "}";
}

std::ostream& operator<<(std::ostream& os, const struct timespec& tv)
{
    return os << "{sec=" << tv.tv_sec << " nsec=" << tv.tv_nsec << "}";
}

template <typename T>
struct ptr_wrapper
{
    explicit ptr_wrapper(T p)
        : ptr_{ p }
    {
    }
    T const ptr_;
};

std::ostream& operator<<(std::ostream& os, const ptr_wrapper<const char*>& wrapper)
{
    if (wrapper.ptr_)
    {
        os << wrapper.ptr_; // don't dereference const char*
    }
    else
    {
        os << static_cast<void*>(nullptr);
    }
    return os;
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const ptr_wrapper<T>& wrapper)
{
    if (wrapper.ptr_)
    {
        os << *wrapper.ptr_;
    }
    else
    {
        os << static_cast<void*>(nullptr);
    }
    return os;
}

struct fuse_ffsp_operations
{
    fuse_ffsp_operations()
        : ops_()
    {
        const std::vector<spdlog::sink_ptr> sinks{
            std::make_shared<spdlog::sinks::stdout_sink_mt>(),
            std::make_shared<spdlog::sinks::simple_file_sink_mt>("ffsp_api.log", true)
        };
        logger_ = std::make_shared<spdlog::logger>("ffsp_api", std::begin(sinks), std::end(sinks));
        spdlog::register_logger(logger_);

#ifdef _WIN32
        ops_.getattr = [](const char* path, struct FUSE_STAT* stbuf)
#else
        ops_.getattr = [](const char* path, struct stat* stbuf)
#endif
        {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} getattr(path={}, stbuf={})", id, deref(path), static_cast<void*>(stbuf));
            int rc = fuse_ffsp::getattr(*fs, path, stbuf);
            logger_->info("< {} getattr(rc={}, stbuf={})", id, rc, deref(stbuf));
            return rc;
        };

        ops_.readlink = [](const char* path, char* buf, size_t bufsize) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} readlink(path={}, buf={}, bufsize={})", id, deref(path), static_cast<void*>(buf), bufsize);
            int rc = fuse_ffsp::readlink(*fs, path, buf, bufsize);
            logger_->info("< {} readlink(rc={}, buf={})", id, rc, deref(buf));
            return rc;
        };

        ops_.mknod = [](const char* path, mode_t mode, dev_t device) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} mknod(path={}, mode={:#o}, device={})", id, deref(path), mode, device);
            int rc = fuse_ffsp::mknod(*fs, path, mode, device);
            logger_->info("< {} mknod(rc={})", id, rc);
            return rc;
        };

        ops_.mkdir = [](const char* path, mode_t mode) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} mkdir(path={}, mode={:#o})", id, deref(path), mode);
            int rc = fuse_ffsp::mkdir(*fs, path, mode);
            logger_->info("< {} mkdir(rc={})", id, rc);
            return rc;
        };

        ops_.unlink = [](const char* path) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} unlink(path={})", id, deref(path));
            int rc = fuse_ffsp::unlink(*fs, path);
            logger_->info("< {} unlink(rc={})", id, rc);
            return rc;
        };

        ops_.rmdir = [](const char* path) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} rmdir(path={})", id, deref(path));
            int rc = fuse_ffsp::rmdir(*fs, path);
            logger_->info("< {} rmdir(rc={})", id, rc);
            return rc;
        };

        ops_.symlink = [](const char* oldpath, const char* newpath) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} symlink(oldpath={}, newpath={})", id, deref(oldpath), deref(newpath));
            int rc = fuse_ffsp::symlink(*fs, oldpath, newpath);
            logger_->info("< {} symlink(rc={})", id, rc);
            return rc;
        };

        ops_.rename = [](const char* oldpath, const char* newpath) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} rename(oldpath={}, newpath={})", id, deref(oldpath), deref(newpath));
            int rc = fuse_ffsp::rename(*fs, oldpath, newpath);
            logger_->info("< {} rename(rc={})", id, rc);
            return rc;
        };

        ops_.link = [](const char* oldpath, const char* newpath) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} link(oldpath={}, newpath={})", id, deref(oldpath), deref(newpath));
            int rc = fuse_ffsp::link(*fs, oldpath, newpath);
            logger_->info("< {} link(rc={})", id, rc);
            return rc;
        };

        ops_.chmod = [](const char* path, mode_t mode) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} chmod(path={}, mode={:#o})", id, deref(path), mode);
            int rc = fuse_ffsp::chmod(*fs, path, mode);
            logger_->info("< {} chmod(rc={})", id, rc);
            return rc;
        };

        ops_.chown = [](const char* path, uid_t uid, gid_t gid) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} chown(path={}, uid={}, gid={})", id, deref(path), uid, gid);
            int rc = fuse_ffsp::chown(*fs, path, uid, gid);
            logger_->info("< {} chown(rc={})", id, rc);
            return rc;
        };

        ops_.truncate = [](const char* path, FUSE_OFF_T length) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} truncate(path={}, length={})", id, deref(path), length);
            int rc = fuse_ffsp::truncate(*fs, path, length);
            logger_->info("< {} truncate(rc={})", id, rc);
            return rc;
        };

        ops_.open = [](const char* path, struct fuse_file_info* fi) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} open(path={}, fi={})", id, deref(path), deref(fi));
            int rc = fuse_ffsp::open(*fs, path, fi);
            logger_->info("< {} open(rc={})", id, rc);
            return rc;
        };

        ops_.read = [](const char* path, char* buf, size_t count,
                       FUSE_OFF_T offset, struct fuse_file_info* fi) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} read(path={}, buf={}, count={}, offset={}, fi={})", id, deref(path), static_cast<void*>(buf), count, offset, deref(fi));
            int rc = fuse_ffsp::read(*fs, path, buf, count, offset, fi);
            logger_->info("< {} read(rc={})", id, rc);
            return rc;
        };

        ops_.write = [](const char* path, const char* buf, size_t count,
                        FUSE_OFF_T offset, struct fuse_file_info* fi) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} write(path={}, buf={}, count={}, offset={}, fi={})", id, deref(path), static_cast<const void*>(buf), count, offset, deref(fi));
            int rc = fuse_ffsp::write(*fs, path, buf, count, offset, fi);
            logger_->info("< {} write(rc={})", id, rc);
            return rc;
        };

        ops_.statfs = [](const char* path, struct statvfs* sfs) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} statfs(path={}, sfs={})", id, deref(path), static_cast<void*>(sfs));
            int rc = fuse_ffsp::statfs(*fs, path, sfs);
            logger_->info("< {} statfs(rc={}, sfs={})", id, rc, deref(sfs));
            return rc;
        };

        ops_.flush = [](const char* path, struct fuse_file_info* fi) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} flush(path={}, fi={})", id, deref(path), deref(fi));
            int rc = fuse_ffsp::flush(*fs, path, fi);
            logger_->info("< {} flush(rc={})", id, rc);
            return rc;
        };

        ops_.release = [](const char* path, struct fuse_file_info* fi) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} release(path={}, fi={})", id, deref(path), deref(fi));
            int rc = fuse_ffsp::release(*fs, path, fi);
            logger_->info("< {} release(rc={})", id, rc);
            return rc;
        };

        ops_.fsync = [](const char* path, int datasync, struct fuse_file_info* fi) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} fsync(path={}, datasync={}, fi={})", id, deref(path), datasync, deref(fi));
            int rc = fuse_ffsp::fsync(*fs, path, datasync, fi);
            logger_->info("< {} fsync(rc={})", id, rc);
            return rc;
        };

        ops_.readdir = [](const char* path, void* buf,
                          fuse_fill_dir_t filler, FUSE_OFF_T offset,
                          struct fuse_file_info* fi) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} readdir(path={}, buf={}, filler={}, offset={}, fi={})", id, deref(path), buf, (filler != nullptr), offset, deref(fi));
            int rc = fuse_ffsp::readdir(*fs, path, buf, filler, offset, fi);
            logger_->info("< {} readdir(rc={})", id, rc);
            return rc;
        };

        ops_.init = [](struct fuse_conn_info* conn) {
            auto id = ++op_id_;
            logger_->info("> {} init(conn={})", id, deref(conn));
            void* private_data = fuse_ffsp::init(conn);
            logger_->info("< {} init(private_data={}, conn={})", id, private_data, deref(conn));
            return private_data;
        };

        ops_.destroy = [](void* user) {
            auto id = ++op_id_;
            logger_->info("> {} destroy(user={})", id, user);
            fuse_ffsp::destroy(user);
            logger_->info("< {} destroy()", id);
        };

        ops_.utimens = [](const char* path, const struct timespec tv[2]) {
            auto id = ++op_id_;
            auto fs = get_fs(fuse_get_context());
            logger_->info("> {} utimens(path={}, access={}, mod={})", id, path, tv[0], tv[1]);
            int rc = fuse_ffsp::utimens(*fs, path, tv);
            logger_->info("< {} utimens(rc={})", id, rc);
            return rc;
        };

        ops_.getdir = nullptr; // deprecated
        ops_.utime = nullptr;  // deprecated

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
        ops_.win_set_times = nullptr;      // TODO
#endif
    }

    ~fuse_ffsp_operations()
    {
        spdlog::drop_all();
    }

    static struct ffsp* get_fs(struct fuse_context* ctx)
    {
        return static_cast<ffsp*>(ctx->private_data);
    }

    template <typename T>
    static ptr_wrapper<const T*> deref(const T* ptr)
    {
        return ptr_wrapper<const T*>{ ptr };
    }

    fuse_operations ops_;

    // every operation (aka FUSE API call) has a unique id
    static std::atomic_uint op_id_;

    static std::shared_ptr<spdlog::logger> logger_;
};
std::atomic_uint fuse_ffsp_operations::op_id_;
std::shared_ptr<spdlog::logger> fuse_ffsp_operations::logger_;

static void show_usage(const char* progname)
{
    printf("Usage:\n");
    printf("%s DEVICE MOUNTPOINT\n", progname);
    printf("%s -h, --help        display this help and exit\n", progname);
    printf("%s -V, -version      print version and exit\n", progname);
}

static void show_version(const char* progname)
{
    printf("FUSE %s version %d.%d.%d\n", progname, FFSP_VERSION_MAJOR,
           FFSP_VERSION_MINOR, FFSP_VERSION_PATCH);
}

enum
{
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

static struct ffsp_params
{
    std::string device;
} ffsp_params;

static int ffsp_opt_proc(void* data, const char* arg, int key, fuse_args* outargs)
{
    (void)data;

    switch (key)
    {
        case FUSE_OPT_KEY_NONOPT:
            if (ffsp_params.device.empty())
            {
                ffsp_params.device.assign(arg);
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

int main(int argc, char* argv[])
{
    fuse_args args = FUSE_ARGS_INIT(argc, argv);
    if (fuse_opt_parse(&args, nullptr, ffsp_opt, ffsp_opt_proc) == -1)
    {
        printf("fuse_opt_parse() failed!\n");
        return EXIT_FAILURE;
    }

    if (ffsp_params.device.empty())
    {
        printf("device argument missing\n");
        return EXIT_FAILURE;
    }
    fuse_ffsp::set_params(ffsp_params.device);

    if (fuse_opt_add_arg(&args, "-odefault_permissions") == -1)
    {
        printf("fuse_opt_add_arg() failed!\n");
        return EXIT_FAILURE;
    }

    fuse_ffsp_operations ffsp_oper;
    int rc = fuse_main(args.argc, args.argv, &ffsp_oper.ops_, nullptr);
    if (rc != 0)
    {
        printf("fuse_main() failed with code %d!\n", rc);
    }

    fuse_opt_free_args(&args);
    return rc;
}
