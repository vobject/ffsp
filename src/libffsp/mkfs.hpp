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

#ifndef MKFS_HPP
#define MKFS_HPP

#include "ffsp.hpp"

namespace ffsp
{

struct mkfs_options
{
    uint32_t clustersize;
    uint32_t erasesize;
    uint32_t ninoopen;
    uint32_t neraseopen;
    uint32_t nerasereserve;
    uint32_t nerasewrites;
};

bool mkfs(const char* path, const mkfs_options& options);

bool fmkfs(int fd, const mkfs_options& options);

} // namespace ffsp

#endif /* MKFS_HPP */
