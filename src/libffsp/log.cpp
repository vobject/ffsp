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

#include "log.hpp"

#include "spdlog/sinks/null_sink.h"

namespace ffsp
{

static std::string s_logname;

void log_init(const std::string& logname, spdlog::level::level_enum level, const std::string& logfile /*= ""*/)
{
    auto logger = spdlog::get(logname);
    if (!logger)
    {
        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_sink_mt>());
        if (!logfile.empty())
            sinks.push_back(std::make_shared<spdlog::sinks::simple_file_sink_mt>(logfile, true));
        logger = std::make_shared<spdlog::logger>(logname, std::begin(sinks), std::end(sinks));
        logger->set_level(level);

        spdlog::register_logger(logger);
        s_logname = logname;
        logger->info("logger {} initialized", logname);
    }
    else
    {
        logger->warn("logger {} already initialized", logname);
    }
}

void log_uninit()
{
    log().info("logger {} about to be uninitialized", s_logname);
    spdlog::drop(s_logname);
}

spdlog::logger& log()
{
    auto logger = spdlog::get(s_logname);
    if (logger)
    {
        return *logger;
    }
    else
    {
        static auto null_logger{ std::make_shared<spdlog::logger>("null", std::make_shared<spdlog::sinks::null_sink_st>()) };
        return *null_logger;
    }
}

} // namespace ffsp


std::ostream& operator<<(std::ostream& os, const ffsp::superblock& sb)
{
    return os << "{"
              << "fsid=" << get_be32(sb.s_fsid)
              << ", flags=" << get_be32(sb.s_flags)
              << ", neraseblocks=" << get_be32(sb.s_neraseblocks)
              << ", nino=" << get_be32(sb.s_nino)
              << ", blocksize=" << get_be32(sb.s_blocksize)
              << ", clustersize=" << get_be32(sb.s_clustersize)
              << ", erasesize=" << get_be32(sb.s_erasesize)
              << ", ninoopen=" << get_be32(sb.s_ninoopen)
              << ", neraseopen=" << get_be32(sb.s_neraseopen)
              << ", nerasereserve=" << get_be32(sb.s_nerasereserve)
              << ", nerasewrites=" << get_be32(sb.s_nerasewrites)
              << "}";
}

std::ostream& operator<<(std::ostream& os, const ffsp::timespec& ts)
{
    return os << "{"
              << "sec=" << get_be64(ts.sec)
              << ", nsec=" << get_be32(ts.nsec)
              << "}";
}

std::ostream& operator<<(std::ostream& os, const ffsp::inode& inode)
{
    return os << "{"
              << "size=" << get_be64(inode.i_size)
              << ", flags=" << get_be32(inode.i_flags)
              << ", no=" << get_be32(inode.i_no)
              << ", nlink=" << get_be32(inode.i_nlink)
              << ", uid=" << get_be32(inode.i_uid)
              << ", gid=" << get_be32(inode.i_gid)
              << ", mode=" << get_be32(inode.i_mode)
              << ", rdev=" << get_be64(inode.i_rdev)
              << ", atime=" << inode.i_atime
              << ", ctime=" << inode.i_ctime
              << ", mtime=" << inode.i_mtime
              << "}";
}

std::ostream& operator<<(std::ostream& os, const ffsp::eraseblock& eb)
{
    return os << "{"
              << "type=" << eb.e_type
              << ", lastwrite=" << get_be16(eb.e_lastwrite)
              << ", cvalid=" << get_be16(eb.e_cvalid)
              << ", writeops=" << get_be16(eb.e_writeops)
              << "}";
}

std::ostream& operator<<(std::ostream& os, const ffsp::dentry& dent)
{
    return os << "{"
              << "ino=" << get_be32(dent.ino)
              << ", len=" << dent.len
              << ", name=" << dent.name
              << "}";
}

std::ostream& operator<<(std::ostream& os, const ffsp::inode_data_type& inode_type)
{
    switch (inode_type)
    {
    case ffsp::inode_data_type::emb:
        return os << "emb";
    case ffsp::inode_data_type::clin:
        return os << "clin";
    case ffsp::inode_data_type::ebin:
        return os << "ebin";
    }
    return os << "unknown";
}

std::ostream& operator<<(std::ostream& os, const ffsp::eraseblock_type& eb_type)
{
    switch (eb_type)
    {
    case ffsp::eraseblock_type::super:
        return os << "super";
    case ffsp::eraseblock_type::dentry_inode:
        return os << "dentry_inode";
    case ffsp::eraseblock_type::dentry_clin:
        return os << "dentry_clin";
    case ffsp::eraseblock_type::file_inode:
        return os << "file_inode";
    case ffsp::eraseblock_type::file_clin:
        return os << "file_clin";
    case ffsp::eraseblock_type::ebin:
        return os << "ebin";
    case ffsp::eraseblock_type::empty:
        return os << "empty";
    case ffsp::eraseblock_type::invalid:
        return os << "invalid";
    }
    return os << "unknown";
}

std::ostream& operator<<(std::ostream& os, const ffsp::ptr_wrapper<const char*>& wrapper)
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
