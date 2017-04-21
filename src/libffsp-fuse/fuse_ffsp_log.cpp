#include "fuse_ffsp_log.hpp"

#include "spdlog/fmt/ostr.h"
#include "spdlog/spdlog.h"

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

std::ostream& operator<<(std::ostream& os, const fuse_file_info& fi)
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

std::ostream& operator<<(std::ostream& os, const fuse_conn_info& conn)
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
