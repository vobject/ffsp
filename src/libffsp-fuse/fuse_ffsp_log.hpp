#ifndef FUSE_FFSP_LOG_HPP
#define FUSE_FFSP_LOG_HPP

#include "fuse.h"

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

template <>
struct fmt::formatter<fuse_conn_info> : fmt::formatter<std::string> {
    auto format(const fuse_conn_info& conn, format_context &ctx) const -> decltype(ctx.out()) {
#ifndef _WIN32
        return fmt::format_to(ctx.out(), "{{protocol_version={}.{}, async_read={}, max_write={}, max_readahead={}, capable=0x{:x}, want=0x{:x}, max_background={}, congestion_threshold={}}}",
            conn.proto_major, conn.proto_minor, conn.async_read, conn.max_write, conn.max_readahead, conn.capable, conn.want, conn.max_background, conn.congestion_threshold);
#else
        return fmt::format_to(ctx.out(), "{{protocol_version={}.{}, async_read={}, max_write={}, max_readahead={}}}",
            conn.proto_major, conn.proto_minor, conn.async_read, conn.max_write, conn.max_readahead);
#endif
    }
};

template <>
struct fmt::formatter<fuse_file_info> : fmt::formatter<std::string> {
    auto format(const fuse_file_info& fi, format_context &ctx) const -> decltype(ctx.out()) {
#ifndef _WIN32
        return fmt::format_to(ctx.out(), "{{flags=0x{:x}, fh=0x{:x}, fh_old=0x{:x}, lock_owner={}, writepage={}, direct_io={}, keep_cache={}, flush={}, nonseekable={}, flock_release={}}}",
            fi.flags, fi.fh, fi.fh_old, fi.lock_owner, fi.writepage, fi.direct_io, fi.keep_cache, fi.flush, fi.nonseekable, fi.flock_release);
#else
        return fmt::format_to(ctx.out(), "{{flags=0x{:x}, fh=0x{:x}, fh_old=0x{:x}, lock_owner={}, writepage={}, direct_io={}, keep_cache={}, flush={}}}",
            fi.flags, fi.fh, fi.fh_old, fi.lock_owner, fi.writepage, fi.direct_io, fi.keep_cache, fi.flush);
#endif
    }
};

#ifdef _WIN32
template <>
struct fmt::formatter<struct FUSE_STAT> : fmt::formatter<std::string> {
    auto format(const struct FUSE_STAT& stat, format_context &ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "{{dev={}, ino={}, nlink={}, mode=0x{:x}, uid={}, gid={}, rdev={}, size={}, blksize={}, blocks={}, atime={}.{}, mtime={}.{}, ctime={}.{}, birthtim={}}}",
            stat.st_dev, stat.st_ino, stat.st_nlink, stat.st_mode, stat.st_uid, stat.st_gid, stat.st_rdev, stat.st_size, stat.st_blksize, stat.st_blocks,
            stat.st_atim.tv_sec, stat.st_atim.tv_nsec, stat.st_mtim.tv_sec, stat.st_mtim.tv_nsec, stat.st_ctim.tv_sec, stat.st_ctim.tv_nsec, stat.st_birthtim.tv_sec, stat.st_birthtim.tv_nsec);
    }
};
#else
template <>
struct fmt::formatter<struct stat> : fmt::formatter<std::string> {
    auto format(const struct stat& stat, format_context &ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "{{dev={}, ino={}, nlink={}, mode=0x{:x}, uid={}, gid={}, rdev={}, size={}, blksize={}, blocks={}, atime={}, mtime={}, ctime={}}}",
            stat.st_dev, stat.st_ino, stat.st_nlink, stat.st_mode, stat.st_uid, stat.st_gid, stat.st_rdev, stat.st_size, stat.st_blksize, stat.st_blocks,
            stat.st_atime, stat.st_mtime, stat.st_ctime);
    }
};
#endif

#ifdef _WIN32
template <>
struct fmt::formatter<struct statvfs> : fmt::formatter<std::string> {
    auto format(const struct statvfs& sfs, format_context &ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "{{bsize={}, frsize={}, blocks={}, bfree={}, bavail={}, files={}, ffree={}, favail={}, fsid={}, flag=0x{:x}, namemax={}}}",
            sfs.f_bsize, sfs.f_frsize, sfs.f_blocks, sfs.f_bfree, sfs.f_bavail, sfs.f_files, sfs.f_ffree, sfs.f_favail, sfs.f_fsid, sfs.f_flag, sfs.f_namemax);
    }
};
#else
template <>
struct fmt::formatter<struct statvfs> : fmt::formatter<std::string> {
    auto format(const struct statvfs& sfs, format_context &ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "{{bsize={}, frsize={}, blocks={}, bfree={}, bavail={}, files={}, ffree={}, favail={}, fsid={}, flag=0x{:x}, namemax={}, type={}}}",
            sfs.f_bsize, sfs.f_frsize, sfs.f_blocks, sfs.f_bfree, sfs.f_bavail, sfs.f_files, sfs.f_ffree, sfs.f_favail, sfs.f_fsid, sfs.f_flag, sfs.f_namemax, sfs.f_type);
    }
};
#endif

template <>
struct fmt::formatter<struct timespec> : fmt::formatter<std::string> {
    auto format(const timespec& tv, format_context &ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "{{sec={}, nsec={}}}", tv.tv_sec, tv.tv_nsec);
    }
};

#endif // FUSE_FFSP_LOG_HPP
