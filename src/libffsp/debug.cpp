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

#include "debug.hpp"

#include <string>
#include <sstream>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

static struct ffsp_debug_info
{
    uint64_t read_raw;
    uint64_t write_raw;
    uint64_t fuse_read;
    uint64_t fuse_write;
    uint64_t gc_read;
    uint64_t gc_write;
    int errors;
} debug_info = {};

void ffsp_debug_fuse_stat(ffsp& fs, struct stat* stbuf)
{
    memset(stbuf, 0, sizeof(*stbuf));
    stbuf->st_dev = 0;
    stbuf->st_ino = 0;
#ifdef _WIN32
    stbuf->st_mode = S_IFREG; // FIXME
#else
    stbuf->st_mode = S_IFREG | S_IRWXU | S_IRWXG | S_IRWXO;
#endif
    stbuf->st_nlink = 1;
    stbuf->st_uid = 0;
    stbuf->st_gid = 0;
    stbuf->st_rdev = 0;
    stbuf->st_size = static_cast<off_t>(ffsp_debug_get_info(fs).size());
    stbuf->st_atime = 0;
    stbuf->st_mtime = 0;
    stbuf->st_ctime = 0;
}

std::string ffsp_debug_get_info(ffsp& fs)
{
    std::ostringstream os;

    os << "{\n";

    os << "  \"fd\": " << fs.fd << ",\n";
    os << "  \"flags\": " << fs.flags << ",\n";
    os << "  \"neraseblocks\": " << fs.neraseblocks << ",\n";
    os << "  \"nino\": " << fs.nino << ",\n";
    os << "  \"blocksize\": " << fs.blocksize << ",\n";
    os << "  \"clustersize\": " << fs.clustersize << ",\n";
    os << "  \"erasesize\": " << fs.erasesize << ",\n";
    os << "  \"ninoopen\": " << fs.ninoopen << ",\n";
    os << "  \"neraseopen\": " << fs.neraseopen << ",\n";
    os << "  \"nerasereserve\": " << fs.nerasereserve << ",\n";
    os << "  \"nerasewrites\": " << fs.nerasewrites << ",\n";

    os << "  \"eb_usage\": [\n";
    for (uint32_t i = 0; i < fs.neraseblocks; i++)
    {
        const ffsp_eraseblk& eb = fs.eb_usage[i];

        os << "    {\n";
        os << "      \"type\": " << std::to_string(eb.e_type) << ",\n";
        os << "      \"lastwrite\": " << std::to_string(get_be16(eb.e_lastwrite)) << ",\n";
        os << "      \"cvalid\": " << std::to_string(get_be16(eb.e_cvalid)) << ",\n";
        os << "      \"writeops\": " << std::to_string(get_be16(eb.e_writeops)) << "\n";

        if (i != (fs.neraseblocks - 1))
            os << "    },\n";
        else
            os << "    }\n";
    }
    os << "  ],\n";

    // TODO: inode ids

    os << "  \"debug_info\": {\n";
    os << "    \"read_raw\": " << debug_info.read_raw << ",\n";
    os << "    \"write_raw\": " << debug_info.write_raw << ",\n";
    os << "    \"fuse_read\": " << debug_info.fuse_read << ",\n";
    os << "    \"fuse_write\": " << debug_info.fuse_write << ",\n";
    os << "    \"gc_read\": " << debug_info.gc_read << ",\n";
    os << "    \"gc_write\": " << debug_info.gc_write << ",\n";
    os << "    \"errors\": " << debug_info.errors << "\n";
    os << "  }\n";

    os << "}\n";
    return os.str();
}

void ffsp_debug_update(ffsp& fs, int type, unsigned long val)
{
    (void)fs;

    switch (type)
    {
        case FFSP_DEBUG_READ_RAW:
            debug_info.read_raw += val;
            break;
        case FFSP_DEBUG_WRITE_RAW:
            debug_info.write_raw += val;
            break;
        case FFSP_DEBUG_FUSE_READ:
            debug_info.fuse_read += val;
            break;
        case FFSP_DEBUG_FUSE_WRITE:
            debug_info.fuse_write += val;
            break;
        case FFSP_DEBUG_GC_READ:
            debug_info.gc_read += val;
            break;
        case FFSP_DEBUG_GC_WRITE:
            debug_info.gc_write += val;
            break;
        case FFSP_DEBUG_LOG_ERROR:
            debug_info.errors += val;
            break;
        default:
            break;
    }
}
