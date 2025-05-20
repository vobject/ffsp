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

#include "libffsp/ffsp.hpp"
#include "libffsp/log.hpp"
#include "libffsp/mkfs.hpp"
#include "libffsp-fuse/fuse_ffsp.hpp"
#include "libffsp-fuse/fuse_ffsp_log.hpp"

#include <atomic>
#include <string>

#include <cstddef>

#ifdef _WIN32
#include <direct.h>
#endif

namespace ffsp
{

namespace fuse
{

struct fuse_ffsp_operations
{
    fuse_ffsp_operations(int verbosity, const char* logfile)
        : ops_()
    {
        auto v2l = [](int verbosity) {
            if (verbosity == 4)
                return spdlog::level::trace;
            if (verbosity == 3)
                return spdlog::level::debug;
            if (verbosity == 2)
                return spdlog::level::info;
            if (verbosity == 1)
                return spdlog::level::warn;
            return spdlog::level::err;
        };

        ffsp::log_init("ffsp", v2l(verbosity), logfile ? logfile : "");

#ifdef _WIN32
        ops_.getattr = [](const char* path, struct FUSE_STAT* stbuf)
#else
        ops_.getattr = [](const char* path, struct stat* stbuf)
#endif
        {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} getattr(path={}, stbuf={})", id, path, static_cast<void*>(stbuf));
            int rc = ffsp::fuse::getattr(fs, path, stbuf);
            log.trace("< {} getattr(rc={}, stbuf={})", id, rc, log_ptr(stbuf));
            return rc;
        };

        ops_.readlink = [](const char* path, char* buf, size_t bufsize) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} readlink(path={}, buf={}, bufsize={})", id, path, static_cast<void*>(buf), bufsize);
            int rc = ffsp::fuse::readlink(fs, path, buf, bufsize);
            log.trace("< {} readlink(rc={}, buf={})", id, rc, buf);
            return rc;
        };

        ops_.mknod = [](const char* path, mode_t mode, dev_t device) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} mknod(path={}, mode={:#o}, device={})", id, path, mode, device);
            int rc = ffsp::fuse::mknod(fs, path, mode, device);
            log.trace("< {} mknod(rc={})", id, rc);
            return rc;
        };

        ops_.mkdir = [](const char* path, mode_t mode) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} mkdir(path={}, mode={:#o})", id, path, mode);
            int rc = ffsp::fuse::mkdir(fs, path, mode);
            log.trace("< {} mkdir(rc={})", id, rc);
            return rc;
        };

        ops_.unlink = [](const char* path) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} unlink(path={})", id, path);
            int rc = ffsp::fuse::unlink(fs, path);
            log.trace("< {} unlink(rc={})", id, rc);
            return rc;
        };

        ops_.rmdir = [](const char* path) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} rmdir(path={})", id, path);
            int rc = ffsp::fuse::rmdir(fs, path);
            log.trace("< {} rmdir(rc={})", id, rc);
            return rc;
        };

        ops_.symlink = [](const char* oldpath, const char* newpath) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} symlink(oldpath={}, newpath={})", id, oldpath, newpath);
            int rc = ffsp::fuse::symlink(fs, oldpath, newpath);
            log.trace("< {} symlink(rc={})", id, rc);
            return rc;
        };

        ops_.rename = [](const char* oldpath, const char* newpath) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} rename(oldpath={}, newpath={})", id, oldpath, newpath);
            int rc = ffsp::fuse::rename(fs, oldpath, newpath);
            log.trace("< {} rename(rc={})", id, rc);
            return rc;
        };

        ops_.link = [](const char* oldpath, const char* newpath) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} link(oldpath={}, newpath={})", id, oldpath, newpath);
            int rc = ffsp::fuse::link(fs, oldpath, newpath);
            log.trace("< {} link(rc={})", id, rc);
            return rc;
        };

        ops_.chmod = [](const char* path, mode_t mode) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} chmod(path={}, mode={:#o})", id, path, mode);
            int rc = ffsp::fuse::chmod(fs, path, mode);
            log.trace("< {} chmod(rc={})", id, rc);
            return rc;
        };

        ops_.chown = [](const char* path, uid_t uid, gid_t gid) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} chown(path={}, uid={}, gid={})", id, path, uid, gid);
            int rc = ffsp::fuse::chown(fs, path, uid, gid);
            log.trace("< {} chown(rc={})", id, rc);
            return rc;
        };

        ops_.truncate = [](const char* path, FUSE_OFF_T length) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} truncate(path={}, length={})", id, path, length);
            int rc = ffsp::fuse::truncate(fs, path, length);
            log.trace("< {} truncate(rc={})", id, rc);
            return rc;
        };

        ops_.open = [](const char* path, fuse_file_info* fi) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} open(path={}, fi={})", id, path, log_ptr(fi));
            int rc = ffsp::fuse::open(fs, path, fi);
            log.trace("< {} open(rc={})", id, rc);
            return rc;
        };

        ops_.read = [](const char* path, char* buf, size_t nbyte,
                       FUSE_OFF_T offset, fuse_file_info* fi) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} read(path={}, buf={}, nbyte={}, offset={}, fi={})", id, path, static_cast<void*>(buf), nbyte, offset, log_ptr(fi));
            int rc = ffsp::fuse::read(fs, path, buf, nbyte, offset, fi);
            log.trace("< {} read(rc={})", id, rc);
            return rc;
        };

        ops_.write = [](const char* path, const char* buf, size_t nbyte,
                        FUSE_OFF_T offset, fuse_file_info* fi) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} write(path={}, buf={}, nbyte={}, offset={}, fi={})", id, path, static_cast<const void*>(buf), nbyte, offset, log_ptr(fi));
            int rc = ffsp::fuse::write(fs, path, buf, nbyte, offset, fi);
            log.trace("< {} write(rc={})", id, rc);
            return rc;
        };

        ops_.statfs = [](const char* path, struct statvfs* sfs) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} statfs(path={}, sfs={})", id, path, static_cast<void*>(sfs));
            int rc = ffsp::fuse::statfs(fs, path, sfs);
            log.trace("< {} statfs(rc={}, sfs={})", id, rc, log_ptr(sfs));
            return rc;
        };

        ops_.flush = [](const char* path, fuse_file_info* fi) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} flush(path={}, fi={})", id, path, log_ptr(fi));
            int rc = ffsp::fuse::flush(fs, path, fi);
            log.trace("< {} flush(rc={})", id, rc);
            return rc;
        };

        ops_.release = [](const char* path, fuse_file_info* fi) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} release(path={}, fi={})", id, path, log_ptr(fi));
            int rc = ffsp::fuse::release(fs, path, fi);
            log.trace("< {} release(rc={})", id, rc);
            return rc;
        };

        ops_.fsync = [](const char* path, int datasync, fuse_file_info* fi) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} fsync(path={}, datasync={}, fi={})", id, path, datasync, log_ptr(fi));
            int rc = ffsp::fuse::fsync(fs, path, datasync, fi);
            log.trace("< {} fsync(rc={})", id, rc);
            return rc;
        };

        ops_.readdir = [](const char* path, void* buf,
                          fuse_fill_dir_t filler, FUSE_OFF_T offset,
                          fuse_file_info* fi) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} readdir(path={}, buf={}, filler={}, offset={}, fi={})", id, path, buf, (filler != nullptr), offset, log_ptr(fi));
            int rc = ffsp::fuse::readdir(fs, path, buf, filler, offset, fi);
            log.trace("< {} readdir(rc={})", id, rc);
            return rc;
        };

        ops_.init = [](fuse_conn_info* conn) {
            auto id = ++op_id_;
            auto& log = get_log();
            log.trace("> {} init(conn={})", id, log_ptr(conn));
            void* private_data = ffsp::fuse::init(conn);
            log.trace("< {} init(private_data={}, conn={})", id, private_data, log_ptr(conn));
            return private_data;
        };

        ops_.destroy = [](void* user) {
            auto id = ++op_id_;
            auto& log = get_log();
            log.trace("> {} destroy(user={})", id, user);
            ffsp::fuse::destroy(user);
            log.trace("< {} destroy()", id);
        };

        ops_.utimens = [](const char* path, const struct ::timespec tv[2]) {
            auto id = ++op_id_;
            auto& log = get_log();
            auto& fs = get_fs(fuse_get_context());
            log.trace("> {} utimens(path={}, access={}, mod={})", id, path, tv[0], tv[1]);
            int rc = ffsp::fuse::utimens(fs, path, tv);
            log.trace("< {} utimens(rc={})", id, rc);
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
        ffsp::log_uninit();
    }

    static ffsp::fs_context& get_fs(fuse_context* ctx)
    {
        return *static_cast<ffsp::fs_context*>(ctx->private_data);
    }

    static spdlog::logger& get_log()
    {
        return ffsp::log();
    }

    fuse_operations ops_;

    // every operation (aka FUSE API call) has a unique id
    static std::atomic_uint op_id_;
};
std::atomic_uint fuse_ffsp_operations::op_id_;

} // namespace fuse

} // namespace ffsp

static void show_usage(const char* progname)
{
    printf("Usage: %s DEVICE MOUNTPOINT\n"
           "      --logfile=FILE    Log file\n"
           "\n"
           "      --memonly         Utilize memory buffer as device\n"
           "      --memsize         Size of the memory buffer in bytes\n"
           "\n"
           "      --format          Format device before mounting\n"
           "  -c, --clustersize=N   Use a clusterblock size of N bytes (default:4KiB)\n"
           "  -e, --erasesize=N     Use a eraseblock size of N bytes (default:4MiB)\n"
           "  -i, --open-ino=N      Support caching of N dirty inodes at a time (default:128)\n"
           "  -o, --open-eb=N       Support N open erase blocks at a time (default:5)\n"
           "  -r, --reserve-eb=N    Reserve N erase blocks for internal use (default:3)\n"
           "  -w, --write-eb=N      Perform garbage collection after N erase blocks have been written (default:5)\n"
           "\n"
           "  -v                    WARNING log level verbosity\n"
           "  -vv                   INFO log level verbosity\n"
           "  -vvv                  DEBUG log level verbosity\n"
           "  -vvvv                 TRACE log level verbosity\n"
           "\n"
           "  -h, --help            Display this help message and exit\n"
           "  -V, --version         Print version and exit\n",
           progname);
}

static void show_version(const char* progname)
{
    printf("FUSE %s version %d.%d.%d\n", progname, ffsp::FFSP_VERSION_MAJOR,
           ffsp::FFSP_VERSION_MINOR,
           ffsp::FFSP_VERSION_PATCH);
}

struct ffsp_mount_arguments
{
    int verbosity{ 0 };
    char* logfile{ nullptr };

    std::string device;

    bool in_memory{ false };
    size_t memsize{ 0 };

    bool format{ false };
    uint32_t clustersize{ 1024 * 32 };
    uint32_t erasesize{ 1024 * 1024 * 4 };
    uint32_t ninoopen{ 128 };
    uint32_t neraseopen{ 5 };
    uint32_t nerasereserve{ 3 };
    uint32_t nerasewrites{ 5 };

    ~ffsp_mount_arguments()
    {
#ifndef _WIN32
        free(logfile);
#endif
    }
};

enum
{
    KEY_HELP,
    KEY_VERSION,
};

#define FFSP_MOUNT_OPT(t, p, v)                 \
    {                                           \
        t, offsetof(ffsp_mount_arguments, p), v \
    }

static fuse_opt ffsp_opt[] = {
    FFSP_MOUNT_OPT("-v", verbosity, 1),
    FFSP_MOUNT_OPT("-vv", verbosity, 2),
    FFSP_MOUNT_OPT("-vvv", verbosity, 3),
    FFSP_MOUNT_OPT("-vvvv", verbosity, 4),
    FFSP_MOUNT_OPT("--logfile=%s", logfile, 0),

    FFSP_MOUNT_OPT("--memonly", in_memory, 1),
#ifdef _WIN32
    FFSP_MOUNT_OPT("--memsize=%Iu", memsize, 0),
#else
    FFSP_MOUNT_OPT("--memsize=%zd", memsize, 0),
#endif

    FFSP_MOUNT_OPT("--format", format, 1),
    FFSP_MOUNT_OPT("--clustersize=%u", clustersize, 0),
    FFSP_MOUNT_OPT("--erasesize=%u", erasesize, 0),
    FFSP_MOUNT_OPT("--open-ino=%u", ninoopen, 0),
    FFSP_MOUNT_OPT("--open-eb=%u", neraseopen, 0),
    FFSP_MOUNT_OPT("--reserve-eb=%u", nerasereserve, 0),
    FFSP_MOUNT_OPT("--write-eb=%u", nerasewrites, 0),

    FUSE_OPT_KEY("-h", KEY_HELP),
    FUSE_OPT_KEY("--help", KEY_HELP),
    FUSE_OPT_KEY("-V", KEY_VERSION),
    FUSE_OPT_KEY("--version", KEY_VERSION),
    FUSE_OPT_END,
};

static int ffsp_opt_proc(void* data, const char* arg, int key, fuse_args* outargs)
{
    switch (key)
    {
        case FUSE_OPT_KEY_NONOPT:
        {
            auto* margs = static_cast<ffsp_mount_arguments*>(data);
            if (margs && margs->device.empty())
            {
                // Running fuse in background mode changes the cwd. Convert a relative
                // device path into an absolute path to support background mode.
#ifdef _WIN32
                if (strlen(arg) >= 4 && arg[1] == ':' && (arg[2] == '\\' || arg[2] == '/'))
#else
                if (strlen(arg) >= 2 && arg[0] == '/')
#endif
                {
                    // absolute path
                    margs->device = arg;
                    return 0;
                }

                // relative path
#ifdef _WIN32
                char* cwd = ::_getcwd(nullptr, 0);
#else
                char* cwd = get_current_dir_name();
#endif
                if (!cwd)
                {
                    return -1;
                }

#ifdef _WIN32
                if (cwd[1] != ':' && (cwd[2] != '\\' || cwd[2] != '/'))
#else
                if (cwd[0] != '/')
#endif
                {
                    free(cwd);
                    return -1;
                }

                margs->device = std::string(cwd) + "/" + arg;
                free(cwd);
                return 0;
            }
            return 1;
        }
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
    ffsp_mount_arguments mntargs;
    if (fuse_opt_parse(&args, &mntargs, ffsp_opt, ffsp_opt_proc) == -1)
    {
        fprintf(stderr, "fuse_opt_parse() failed!\n");
        return EXIT_FAILURE;
    }

    if (mntargs.device.empty() && !mntargs.in_memory)
    {
        fprintf(stderr, "device argument missing\n");
        return EXIT_FAILURE;
    }

    if (mntargs.in_memory)
    {
        ffsp::fuse::set_options(
            mntargs.memsize, { mntargs.clustersize, mntargs.erasesize,
                               mntargs.ninoopen, mntargs.neraseopen,
                               mntargs.nerasereserve, mntargs.nerasewrites });
    }
    else
    {
        if (!mntargs.format)
            ffsp::fuse::set_options(mntargs.device.c_str());
        else
            ffsp::fuse::set_options(
                mntargs.device.c_str(), { mntargs.clustersize, mntargs.erasesize,
                                          mntargs.ninoopen, mntargs.neraseopen,
                                          mntargs.nerasereserve, mntargs.nerasewrites });
    }

    if (fuse_opt_add_arg(&args, "-odefault_permissions") == -1)
    {
        fprintf(stderr, "fuse_opt_add_arg(-odefault_permissions) failed!\n");
        return EXIT_FAILURE;
    }

    ffsp::fuse::fuse_ffsp_operations ffsp_oper{ mntargs.verbosity, mntargs.logfile };
    int rc = fuse_main(args.argc, args.argv, &ffsp_oper.ops_, nullptr);
    if (rc != 0)
    {
        fprintf(stderr, "fuse_main() failed with code %d!\n", rc);
    }

    fuse_opt_free_args(&args);
    return rc;
}
