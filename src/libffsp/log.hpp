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

#include "spdlog/fmt/ostr.h"
#include "spdlog/spdlog.h"

#include <string>

namespace ffsp
{

void log_init(const std::string& logname, spdlog::level::level_enum level, const std::string& logfile = "");
void log_uninit();
spdlog::logger& log();

} // namespace ffsp

std::ostream& operator<<(std::ostream& os, const ffsp::superblock& sb);
std::ostream& operator<<(std::ostream& os, const ffsp::timespec& ts);
std::ostream& operator<<(std::ostream& os, const ffsp::inode& inode);
std::ostream& operator<<(std::ostream& os, const ffsp::eraseblock& eb);
std::ostream& operator<<(std::ostream& os, const ffsp::dentry& dent);

std::ostream& operator<<(std::ostream& os, const ffsp::inode_data_type& inode_type);
std::ostream& operator<<(std::ostream& os, const ffsp::eraseblock_type& eb_type);

#endif /* LOG_HPP */
