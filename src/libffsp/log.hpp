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

#ifndef LOG_HPP
#define LOG_HPP

#include "ffsp.hpp"

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

#include <string>

namespace ffsp
{

void log_init(const std::string& logname, spdlog::level::level_enum level, const std::string& logfile = "");
void log_uninit();
spdlog::logger& log();

template <typename T>
struct log_ptr_wrapper
{
    explicit log_ptr_wrapper(T p)
        : ptr_{ p }
    {
    }
    T const ptr_;
};

template <typename T>
log_ptr_wrapper<const T*> log_ptr(const T* ptr)
{
    return log_ptr_wrapper<const T*>{ ptr };
}

} // namespace ffsp

template<>
struct fmt::formatter<ffsp::superblock> : fmt::formatter<std::string>
{
    auto format(const ffsp::superblock& super, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{{"
            "fsid={}, flags={}, neraseblocks={}, nino={}, blocksize={}, clustersize={}, erasesize={}, ninoopen={}, neraseopen={}, nerasereserve={}, nerasewrites={}"
            "}}",
            get_be32(super.s_fsid),
            get_be32(super.s_flags),
            get_be32(super.s_neraseblocks),
            get_be32(super.s_nino),
            get_be32(super.s_blocksize),
            get_be32(super.s_clustersize),
            get_be32(super.s_erasesize),
            get_be32(super.s_ninoopen),
            get_be32(super.s_neraseopen),
            get_be32(super.s_nerasereserve),
            get_be32(super.s_nerasewrites)
        );
    }
};

template<>
struct fmt::formatter<ffsp::timespec> : fmt::formatter<std::string>
{
    auto format(const ffsp::timespec& ts, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{{"
            "sec={}, nsec={}"
            "}}",
            get_be64(ts.sec),
            get_be32(ts.nsec)
        );
    }
};

template<>
struct fmt::formatter<ffsp::inode> : fmt::formatter<std::string>
{
    auto format(const ffsp::inode& inode, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{{"
            "size={}, flags={}, no={}, nlink={}, uid={}, gid={}, mode={}, rdev={}, atime={}, ctime={}, mtime={}"
            "}}",
            get_be64(inode.i_size),
            get_be32(inode.i_flags),
            get_be32(inode.i_no),
            get_be32(inode.i_nlink),
            get_be32(inode.i_uid),
            get_be32(inode.i_gid),
            get_be32(inode.i_mode),
            get_be64(inode.i_rdev),
            inode.i_atime,
            inode.i_ctime,
            inode.i_mtime
        );
    }
};

template<>
struct fmt::formatter<ffsp::eraseblock_type> : fmt::formatter<std::string>
{
    auto format(const ffsp::eraseblock_type& eb_type, format_context& ctx) const -> decltype(ctx.out())
    {
        switch (eb_type) {
        case ffsp::eraseblock_type::super:
            return fmt::format_to(ctx.out(), "super");
        case ffsp::eraseblock_type::dentry_inode:
            return fmt::format_to(ctx.out(), "dentry_inode");
        case ffsp::eraseblock_type::dentry_clin:
            return fmt::format_to(ctx.out(), "dentry_clin");
        case ffsp::eraseblock_type::file_inode:
            return fmt::format_to(ctx.out(), "file_inode");
        case ffsp::eraseblock_type::file_clin:
            return fmt::format_to(ctx.out(), "file_clin");
        case ffsp::eraseblock_type::ebin:
            return fmt::format_to(ctx.out(), "ebin");
        case ffsp::eraseblock_type::empty:
            return fmt::format_to(ctx.out(), "empty");
        case ffsp::eraseblock_type::invalid:
            return fmt::format_to(ctx.out(), "invalid");
        }
        return fmt::format_to(ctx.out(), "unknown");
    }
};

template<>
struct fmt::formatter<ffsp::eraseblock> : fmt::formatter<std::string>
{
    auto format(const ffsp::eraseblock& eb, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{{"
            "type={}, lastwrite={}, cvalid={}, writeops={}"
            "}}",
            eb.e_type,
            get_be16(eb.e_lastwrite),
            get_be16(eb.e_cvalid),
            get_be16(eb.e_writeops)
        );
    }
};

template<>
struct fmt::formatter<ffsp::dentry> : fmt::formatter<std::string>
{
    auto format(const ffsp::dentry& dent, format_context& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{{"
            "ino={}, len={}, name={}"
            "}}",
            get_be32(dent.ino),
            dent.len,
            dent.name
        );
    }
};

template<>
struct fmt::formatter<ffsp::inode_data_type> : fmt::formatter<std::string>
{
    auto format(const ffsp::inode_data_type& inode_type, format_context& ctx) const -> decltype(ctx.out())
    {
        switch (inode_type) {
        case ffsp::inode_data_type::emb:
            return fmt::format_to(ctx.out(), "emb");
        case ffsp::inode_data_type::clin:
            return fmt::format_to(ctx.out(), "clin");
        case ffsp::inode_data_type::ebin:
            return fmt::format_to(ctx.out(), "ebin");
        }
        return fmt::format_to(ctx.out(), "unknown");
    }
};

template <typename T>
struct fmt::formatter<ffsp::log_ptr_wrapper<const T*>> : fmt::formatter<std::string> {
    auto format(ffsp::log_ptr_wrapper<const T*> wrapper, format_context &ctx) const -> decltype(ctx.out()) {
        if (wrapper.ptr_)
        {
            return fmt::format_to(ctx.out(), "{}", *wrapper.ptr_);
        }
        else
        {
            return fmt::format_to(ctx.out(), "{}", static_cast<void*>(nullptr));
        }
    }
};

#endif /* LOG_HPP */
