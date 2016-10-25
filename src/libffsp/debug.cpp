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
#include "inode_group.hpp"

#include <string>
#include <sstream>
#include <vector>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

static struct ffsp_debug_info
{
    uint64_t read_raw;
    uint64_t write_raw;
    uint64_t fuse_read;
    uint64_t fuse_write;
    uint64_t gc_read;
    uint64_t gc_write;
} debug_info = {};


void ffsp_debug_update(const ffsp_fs& fs, int type, unsigned long val)
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
        default:
            break;
    }
}

static std::string get_super_info(ffsp_fs& fs)
{
    std::ostringstream os;

    os << "{";

    os << "\"super\":{";
    os << "\"fsid\":" << fs.fsid << ",";
    os << "\"flags\":" << fs.flags << ",";
    os << "\"neraseblocks\":" << fs.neraseblocks << ",";
    os << "\"nino\":" << fs.nino << ",";
    os << "\"blocksize\":" << fs.blocksize << ",";
    os << "\"clustersize\":" << fs.clustersize << ",";
    os << "\"erasesize\":" << fs.erasesize << ",";
    os << "\"ninoopen\":" << fs.ninoopen << ",";
    os << "\"neraseopen\":" << fs.neraseopen << ",";
    os << "\"nerasereserve\":" << fs.nerasereserve << ",";
    os << "\"nerasewrites\":" << fs.nerasewrites;
    os << "}";

    os << ",";
    os << "\"eraseblocks\":[";
    for (uint32_t i = 0; i < fs.neraseblocks; i++)
    {
        os << i;

        if (i != (fs.neraseblocks - 1))
            os << ",";
    }
    os << "]";

    os << "}";
    return os.str();
}

static std::string get_metrics_info(ffsp_fs& fs)
{
    (void)fs;

    std::ostringstream os;

    os << "{";

    os << "\"debuginfo\":{";
    os << "\"read_raw\":" << debug_info.read_raw << ",";
    os << "\"write_raw\":" << debug_info.write_raw << ",";
    os << "\"fuse_read\":" << debug_info.fuse_read << ",";
    os << "\"fuse_write\":" << debug_info.fuse_write << ",";
    os << "\"gc_read\":" << debug_info.gc_read << ",";
    os << "\"gc_write\":" << debug_info.gc_write;
    os << "}";

    os << "}";
    return os.str();
}

static std::string get_eb_info(ffsp_fs& fs, unsigned int eb_id)
{
    const ffsp_eraseblk& eb = fs.eb_usage[eb_id];

    std::ostringstream os;

    os << "{";

    os << "\"eraseblock\":{";
    os << "\"eb_id\":" << eb_id << ",";
    os << "\"type\":" << int(eb.e_type) << ",";
    os << "\"lastwrite\":" << get_be16(eb.e_lastwrite) << ",";
    os << "\"cvalid\":" << get_be16(eb.e_cvalid) << ",";
    os << "\"writeops\":" << get_be16(eb.e_writeops);
    os << "}";

    os << ",";
    os << "\"clusters\":[";
    const unsigned int cl_per_eb = fs.erasesize / fs.clustersize;
    for (unsigned int i = 0; i < cl_per_eb; i++)
    {
        const unsigned int cl_id = eb_id * fs.erasesize / fs.clustersize + i;
        os << cl_id;

        if (i != (cl_per_eb - 1))
            os << ",";
    }
    os << "]";

    os << "}";
    return os.str();
}

static std::string get_cl_info(ffsp_fs& fs, unsigned int cl_id)
{
    std::ostringstream os;

    os << "{";

    const unsigned int eb_id = cl_id * fs.clustersize / fs.erasesize;
    const ffsp_eraseblk& eb = fs.eb_usage[eb_id];
    os << "\"eraseblock\":{";
    os << "\"eb_id\":" << eb_id << ",";
    os << "\"type\":" << int(eb.e_type) << ",";
    os << "\"lastwrite\":" << get_be16(eb.e_lastwrite) << ",";
    os << "\"cvalid\":" << get_be16(eb.e_cvalid) << ",";
    os << "\"writeops\":" << get_be16(eb.e_writeops);
    os << "}";

    os << ",";
    os << "\"cluster\":{";
    os << "\"cl_id\":" << cl_id << ",";
    os << "\"cl_offset\":" << (cl_id * fs.clustersize);
    if (eb.e_type == FFSP_EB_DENTRY_INODE || eb.e_type == FFSP_EB_FILE_INODE)
    {
        os << ",";
        os << "\"inodes\":[";
        ffsp_inode** inodes = (ffsp_inode**)malloc((fs.clustersize / sizeof(ffsp_inode)) *
                                      sizeof(ffsp_inode*));
        int ino_cnt = ffsp_read_inode_group(&fs, cl_id, inodes);
        if (ino_cnt)
        {
            for (int i = 0; i < ino_cnt; i++)
            {
                os << get_be32(inodes[i]->i_no);

                if (i != (ino_cnt - 1))
                    os << ",";

                free(inodes[i]);
            }
        }
        free(inodes);
        os << "]";
    }
    os << "}";

    os << "}";
    return os.str();
}

static std::string get_ino_info(ffsp_fs& fs, uint32_t ino_no)
{
    std::ostringstream os;

    os << "{";

    const unsigned int cl_id = get_be32(fs.ino_map[ino_no]);
    const unsigned int eb_id = cl_id * fs.clustersize / fs.erasesize;

    const ffsp_eraseblk& eb = fs.eb_usage[eb_id];
    os << "\"eraseblock\":{";
    os << "\"eb_id\":" << eb_id << ",";
    os << "\"type\":" << int(eb.e_type) << ",";
    os << "\"lastwrite\":" << get_be16(eb.e_lastwrite) << ",";
    os << "\"cvalid\":" << get_be16(eb.e_cvalid) << ",";
    os << "\"writeops\":" << get_be16(eb.e_writeops);
    os << "}";

    os << ",";
    os << "\"cluster\":{";
    os << "\"cl_id\":" << cl_id << ",";
    os << "\"cl_offset\":" << (cl_id * fs.clustersize);
    os << "}";

    os << ",";
    os << "\"inode\":{";
    ffsp_inode** inodes = (ffsp_inode**)malloc((fs.clustersize / sizeof(ffsp_inode)) *
                                  sizeof(ffsp_inode*));
    int ino_cnt = ffsp_read_inode_group(&fs, cl_id, inodes);
    if (ino_cnt)
    {
        for (int i = 0; i < ino_cnt; i++)
        {
            if (get_be32(inodes[i]->i_no) == ino_no)
            {
                os << "\"size\":" << get_be64(inodes[i]->i_size) << ",";
                os << "\"flags\":" << get_be32(inodes[i]->i_flags) << ",";
                os << "\"no\":" << get_be32(inodes[i]->i_no) << ",";
                os << "\"nlink\":" << get_be32(inodes[i]->i_nlink) << ",";
                os << "\"uid\":" << get_be32(inodes[i]->i_uid) << ",";
                os << "\"gid\":" << get_be32(inodes[i]->i_gid) << ",";
                os << "\"mode\":" << get_be32(inodes[i]->i_mode) << ",";
                os << "\"rdev\":" << get_be64(inodes[i]->i_rdev) << ",";
                os << "\"atime\":" << get_be64(inodes[i]->i_atime.sec) << ",";
                os << "\"ctime\":" << get_be64(inodes[i]->i_ctime.sec) << ",";
                os << "\"mtime\":" << get_be64(inodes[i]->i_mtime.sec);
            }
            free(inodes[i]);
        }
    }
    free(inodes);
    os << "}";

    os << "}";
    return os.str();
}

static const std::string DEBUG_DIR{"/.FFSP.d"};
static const std::string DEBUG_SUPER_FILE{"/.FFSP.d/super"};
static const std::string DEBUG_METRICS_FILE{"/.FFSP.d/metrics"};
static const std::string DEBUG_ERASEBLOCK_DIR{"/.FFSP.d/eraseblocks.d"};
static const std::string DEBUG_CLUSTER_DIR{"/.FFSP.d/clusters.d"};
static const std::string DEBUG_INODE_DIR{"/.FFSP.d/inodes.d"};

enum class DebugElementType
{
    Invalid = -1,

    RootDir,
    SuperFile,
    MetricsFile,

    EraseblockDir,
    EraseblockFile,

    ClusterDir,
    ClusterFile,

    InodeDir,
    InodeFile,
};

static DebugElementType get_debug_elem_type(ffsp_fs& fs, const char* path)
{
    (void)fs;

    const size_t path_len = strlen(path);

    if (strncmp(path, DEBUG_ERASEBLOCK_DIR.c_str(), DEBUG_ERASEBLOCK_DIR.size()) == 0)
    {
        if (path_len == DEBUG_ERASEBLOCK_DIR.size())
            return DebugElementType::EraseblockDir;
        else if ((path_len > (DEBUG_ERASEBLOCK_DIR.size() + 1)) && (path[DEBUG_ERASEBLOCK_DIR.size()] == '/'))
            return DebugElementType::EraseblockFile;
    }

    if (strncmp(path, DEBUG_CLUSTER_DIR.c_str(), DEBUG_CLUSTER_DIR.size()) == 0)
    {
        if (path_len == DEBUG_CLUSTER_DIR.size())
            return DebugElementType::ClusterDir;
        else if ((path_len > (DEBUG_CLUSTER_DIR.size() + 1)) && (path[DEBUG_CLUSTER_DIR.size()] == '/'))
            return DebugElementType::ClusterFile;
    }

    if (strncmp(path, DEBUG_INODE_DIR.c_str(), DEBUG_INODE_DIR.size()) == 0)
    {
        if (path_len == DEBUG_INODE_DIR.size())
            return DebugElementType::InodeDir;
        else if ((path_len > (DEBUG_INODE_DIR.size() + 1)) && (path[DEBUG_INODE_DIR.size()] == '/'))
            return DebugElementType::InodeFile;
    }

    if (path == DEBUG_METRICS_FILE)
        return DebugElementType::MetricsFile;

    if (path == DEBUG_SUPER_FILE)
        return DebugElementType::SuperFile;

    if (path == DEBUG_DIR)
        return DebugElementType::RootDir;

    return DebugElementType::Invalid;
}

static void get_default_dir_stat(struct stat& stbuf)
{
    memset(&stbuf, 0, sizeof(stbuf));
    stbuf.st_nlink = 1;
    stbuf.st_uid = getuid();
    stbuf.st_gid = getgid();
    stbuf.st_mode = S_IFDIR | S_IRUSR | S_IXUSR;
}

static void get_default_file_stat(struct stat& stbuf)
{
    memset(&stbuf, 0, sizeof(stbuf));
    stbuf.st_nlink = 1;
    stbuf.st_uid = getuid();
    stbuf.st_gid = getgid();
    stbuf.st_mode = S_IFREG | S_IRUSR | S_IXUSR;
}

static uint32_t get_path_id(const char* path, DebugElementType type)
{
    std::string id_str;
    switch (type)
    {
    case DebugElementType::EraseblockFile:
        id_str = path + DEBUG_ERASEBLOCK_DIR.size() + 1;
        break;
    case DebugElementType::ClusterFile:
        id_str = path + DEBUG_CLUSTER_DIR.size() + 1;
        break;
    case DebugElementType::InodeFile:
        id_str = path + DEBUG_INODE_DIR.size() + 1;
        break;
    default:
        break;
    }

    const auto id = std::stoul(id_str);
    return static_cast<uint32_t>(id);
}

bool ffsp_debug_is_debug_path(ffsp_fs& fs, const char* path)
{
    (void)fs;

    return strncmp(path, DEBUG_DIR.c_str(), DEBUG_DIR.size()) == 0;
}

bool ffsp_debug_getattr(ffsp_fs& fs, const char* path, struct stat& stbuf)
{
    const auto type = get_debug_elem_type(fs, path);
    uint64_t file_size = 0;

    switch (type)
    {
    case DebugElementType::Invalid:
        return false;

    case DebugElementType::RootDir:
    case DebugElementType::EraseblockDir:
    case DebugElementType::ClusterDir:
    case DebugElementType::InodeDir:
        get_default_dir_stat(stbuf);
        return true;

    case DebugElementType::SuperFile:
        file_size = get_super_info(fs).size();
        break;
    case DebugElementType::MetricsFile:
        file_size = get_metrics_info(fs).size();
        break;
    case DebugElementType::EraseblockFile:
        file_size = get_eb_info(fs, get_path_id(path, type)).size();
        break;
    case DebugElementType::ClusterFile:
        file_size = get_cl_info(fs, get_path_id(path, type)).size();
        break;
    case DebugElementType::InodeFile:
        file_size = get_ino_info(fs, get_path_id(path, type)).size();
        break;
    }

    get_default_file_stat(stbuf);
    stbuf.st_size = static_cast<off_t>(file_size);
    return true;
}

bool ffsp_debug_readdir(ffsp_fs& fs, const char* path, std::vector<std::string>& dirs)
{
    dirs.clear();
    switch (get_debug_elem_type(fs, path))
    {
    case DebugElementType::Invalid:
    case DebugElementType::SuperFile:
    case DebugElementType::MetricsFile:
    case DebugElementType::EraseblockFile:
    case DebugElementType::ClusterFile:
    case DebugElementType::InodeFile:
        return false;

    case DebugElementType::RootDir:
        dirs.push_back("super");
        dirs.push_back("metrics");
        dirs.push_back("eraseblocks.d");
        dirs.push_back("clusters.d");
        dirs.push_back("inodes.d");
        break;
    case DebugElementType::EraseblockDir:
        dirs.reserve(fs.neraseblocks);
        for (uint32_t i = 0; i < fs.neraseblocks; i++)
            dirs.push_back(std::to_string(i));
        break;
    case DebugElementType::ClusterDir:
        dirs.reserve(fs.neraseblocks * fs.erasesize / fs.clustersize);
        for (uint32_t i = 0; i < (fs.neraseblocks * fs.erasesize / fs.clustersize); i++)
            dirs.push_back(std::to_string(i));
        break;
    case DebugElementType::InodeDir:
        {
            const unsigned int cl_per_eb = fs.erasesize / fs.clustersize;
            ffsp_inode** inodes = (ffsp_inode**)malloc((fs.clustersize / sizeof(ffsp_inode)) * sizeof(ffsp_inode*));
            for (uint32_t eb_id = 0; eb_id < fs.neraseblocks; eb_id++)
            {
                const ffsp_eraseblk& eb = fs.eb_usage[eb_id];
                if (eb.e_type == FFSP_EB_DENTRY_INODE || eb.e_type == FFSP_EB_FILE_INODE)
                {
                    for (unsigned int cl_idx = 0; cl_idx < cl_per_eb; cl_idx++)
                    {
                        const unsigned int cl_id = eb_id * fs.erasesize / fs.clustersize + cl_idx;

                        int ino_cnt = ffsp_read_inode_group(&fs, cl_id, inodes);
                        if (ino_cnt)
                        {
                            for (int ino_idx = 0; ino_idx < ino_cnt; ino_idx++)
                            {
                                dirs.push_back(std::to_string(get_be32(inodes[ino_idx]->i_no)));
                                free(inodes[ino_idx]);
                            }
                        }
                    }
                }
            }
            free(inodes);
        }
        break;
    }
    return true;
}

bool ffsp_debug_open(ffsp_fs& fs, const char* path)
{
    (void)fs;
    (void)path;

    return true;
}

bool ffsp_debug_release(ffsp_fs& fs, const char* path)
{
    (void)fs;
    (void)path;

    return true;
}

bool ffsp_debug_read(ffsp_fs& fs, const char* path, char* buf, uint64_t count, uint64_t offset, uint64_t& read)
{
    const auto type = get_debug_elem_type(fs, path);
    std::string str;

    switch (type)
    {
    case DebugElementType::Invalid:
    case DebugElementType::RootDir:
    case DebugElementType::EraseblockDir:
    case DebugElementType::ClusterDir:
    case DebugElementType::InodeDir:
        return false;

    case DebugElementType::SuperFile:
        str = get_super_info(fs);
        break;
    case DebugElementType::MetricsFile:
        str = get_metrics_info(fs);
        break;
    case DebugElementType::EraseblockFile:
        str = get_eb_info(fs, get_path_id(path, type));
        break;
    case DebugElementType::ClusterFile:
        str = get_cl_info(fs, get_path_id(path, type));
        break;
    case DebugElementType::InodeFile:
        str = get_ino_info(fs, get_path_id(path, type));
        break;
    }

    if (offset < str.size())
    {
        read = str.size() - offset;
        if (read > count)
            read = count;
        memcpy(buf, &str[offset], read);
        return true;
    }
    return -EIO;
}
