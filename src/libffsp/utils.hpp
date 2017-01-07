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

#ifndef UTILS_HPP
#define UTILS_HPP

#include "ffsp.hpp"

#include <ctime>

struct stat;
struct statvfs;

namespace ffsp
{

bool update_time(timespec& dest);

void stat(fs_context& fs, const inode& ino, struct ::stat& stbuf);
void statfs(fs_context& fs, struct ::statvfs& sfs);
void utimens(fs_context& fs, inode& ino, const struct ::timespec tvi[2]);

} // namespace ffsp

#endif /* UTILS_HPP */
