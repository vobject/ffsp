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

#ifndef INODE_GROUP_HPP
#define INODE_GROUP_HPP

#include "ffsp.hpp"

namespace ffsp
{

int read_inode_group(fs_context& fs, unsigned int cl_id, inode** inodes);
int write_inodes(fs_context& fs, inode** inodes, unsigned int ino_cnt);

} // namespace ffsp

#endif /* INODE_GROUP_HPP */
