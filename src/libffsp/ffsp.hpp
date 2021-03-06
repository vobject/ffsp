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

#ifndef FFSP_HPP
#define FFSP_HPP

#include "byteorder.hpp"

#include <vector>

#include <cassert>

namespace ffsp
{

constexpr uint32_t FFSP_FILE_SYSTEM_ID{ 0x46465350 }; // "FFSP"
constexpr int FFSP_VERSION_MAJOR{ 0 };
constexpr int FFSP_VERSION_MINOR{ 0 };
constexpr int FFSP_VERSION_PATCH{ 1 };

struct superblock
{
    be32_t s_fsid;          // file system ID
    be32_t s_flags;         // mount flags - TODO: What are these for? -> noatime(?)
    be32_t s_neraseblocks;  // number of erase blocks
    be32_t s_nino;          // supported number of files on the drive
    be32_t s_blocksize;     // currently same as clustersize
    be32_t s_clustersize;   // size of a cluster (aka inode + data)
    be32_t s_erasesize;     // size of an erase block (in bytes or clusters?)
    be32_t s_ninoopen;      // dirty inodes to cache before writing to disk
    be32_t s_neraseopen;    // erase blocks to be hold open simultaneously
    be32_t s_nerasereserve; // number of erase blocks for internal use
    be32_t s_nerasewrites;  // number of erase block to finalize before GC

    be32_t reserved[21]; // extend to 128 Bytes
};
static_assert(sizeof(superblock) == 128, "superblock: unexpected size");

struct timespec
{
#ifdef _WIN32
#pragma pack(push, 4)
    be64_t sec;
    be32_t nsec;
#pragma pack(pop)
#else
    be64_t sec __attribute__((packed, aligned(4)));
    be32_t nsec;
#endif
};
static_assert(sizeof(timespec) == 12, "timespec: unexpected size");

// inode data format - the lower end 8bit of the inode flags
enum class inode_data_type : uint8_t
{
    // Data embedded in the inode's cluster.
    // For small files (actual max size depends on the cluster size).
    emb = 0x01,

    // The inode's data section contains cluster ids which contain the data
    // For medium sized files (actual max size depends on the cluster size).
    clin = 0x02,

    // The inode's data section contains erase block ids which contain the data
    // For large files (actual max size depends on the cluster size).
    ebin = 0x04,
};

struct inode
{
    be64_t i_size;
    be32_t i_flags; // e.g. multiple inodes in the same cluster, inode type
    be32_t i_no;
    be32_t i_nlink;
    be32_t i_uid;
    be32_t i_gid;
    be32_t i_mode; // inode type and permissions
    be64_t i_rdev;

    timespec i_atime;
    timespec i_ctime;
    timespec i_mtime;

    be32_t reserved[13]; // extend to 128 Bytes
};
static_assert(sizeof(inode) == 128, "inode: unexpected size");

enum class eraseblock_type : uint8_t
{
    super = 0x00,
    dentry_inode = 0x01,
    dentry_clin = 0x02,
    file_inode = 0x04,
    file_clin = 0x08,
    ebin = 0x10,
    empty = 0x20,
    invalid = 0xff,
};
static_assert(sizeof(eraseblock_type) == 1, "eraseblock_type: unexpected size");

struct eraseblock
{
    eraseblock_type e_type{eraseblock_type::invalid};
    uint8_t reserved;
    be16_t e_lastwrite{0};
    be16_t e_cvalid{0};   // valid clusters inside the erase block
    be16_t e_writeops{0}; // how many writes were performed on this eb
};
static_assert(sizeof(eraseblock) == 8, "eraseblock: unexpected size");

const unsigned int FFSP_NAME_MAX{ 248 };

struct dentry
{
    be32_t ino;
    uint8_t len;
    uint8_t reserved[3];
    char name[FFSP_NAME_MAX];
};
static_assert(sizeof(dentry) == 256, "dentry: unexpected size");

// In-memory-types and data structures

using ino_t = uint32_t;
using cl_id_t = uint32_t;
using eb_id_t = uint32_t;

// Invalid index inside the inode map - 32bit
const ino_t FFSP_INVALID_INO_NO{ 0 };

// Cluster ids that the inode map points to - 32bit
const cl_id_t FFSP_FREE_CL_ID{ 0x00000000 };
const cl_id_t FFSP_RESERVED_CL_ID{ 0xffffffff };

// Erase block ids - 32bit
const eb_id_t FFSP_INVALID_EB_ID{ 0x00000000 };

struct io_backend;
struct inode_cache;
struct summary_cache;
struct gcinfo;

struct fs_context
{
    io_backend* io_ctx{nullptr};

    uint32_t fsid{ 0 };          // file system ID
    uint32_t flags{ 0 };         // mount flags - TODO: What are these for? -> noatime(?)
    uint32_t neraseblocks{ 0 };  // number of erase blocks
    uint32_t nino{ 0 };          // supported number of files on the drive
    uint32_t blocksize{ 0 };     // currently same as clustersize
    uint32_t clustersize{ 0 };   // size of a cluster (aka inode + data)
    uint32_t erasesize{ 0 };     // size of an erase block (in bytes or clusters?)
    uint32_t ninoopen{ 0 };      // dirty inodes to cache before writing to disk
    uint32_t neraseopen{ 0 };    // erase blocks to be hold open simultaneously
    uint32_t nerasereserve{ 0 }; // number of erase blocks for internal use
    uint32_t nerasewrites{ 0 };  // number of erase block to finalize before GC

    // Array with information about every erase block
    std::vector<eraseblock> eb_usage;

    // This array contains the cluster ids where the specified inode is
    //  located on disk. It is indexed using the inode number (ino->i_no).
    //  It is read at mount time and is occasionally written back to disk.
    //  It resides inside the first erase block of the file system and NOT
    //  inside the log.
    std::vector<be32_t> ino_map;

    // Head of a linked list that contains all the erase block summary
    //  to all currently open cluster indirect erase blocks. When a
    //  cluster indirect erase block is full its summary is written as
    //  its last cluster. The erase block summary contains a list of all
    //  inode ids that contain indirect clusters inside the erase block.
    // There can be only one erase block summary per erase block type at once.
    // This is because there can be only one open erase block per erase block type at once.
    // Therefore each erase block type can exist only once in the summary list at any point in time.
    ffsp::summary_cache* summary_cache{ nullptr };

    // A data structure that caches all inodes that have been
    //  looked up from the file system. "ino_status_map" (see below) is
    //  used to determine which of those inodes are dirty.
    ffsp::inode_cache* inode_cache{ nullptr };

    // A buffer that represents each (possible) inode with one bit. Its
    //  status indicates whether the (cached) inode was changed (is dirty)
    //  but was not yet written back to the medium.
    uint32_t* ino_status_map{ nullptr };

    /* TODO: replace this with a real data structure and provide
     * accessory functions for manipulation */
    // A buffer where each element represents a cluster in the file system.
    //  It contains information about how many valid inodes are present
    //  inside the concerned cluster.The array is indexed using the
    //  cluster id resp. cluster_offset / cluster_size.
    std::vector<int> cl_occupancy;

    // A variable that counting the number of dirty inodes cached in
    //  main memory. The dirty inodes should be written back to disk if
    //  this counter reaches fs.ninoopen which is set at mkfs time.
    unsigned int dirty_ino_cnt{ 0 };

    ffsp::gcinfo* gcinfo{ nullptr };

    // Static helper buffer, one erase block large.
    // It is used for moving around clusters or erase blocks.
    // For example when expanding inode embedded data to cluster indirect
    //  or from cluster indirect to erase block indirect.
    char* buf{ nullptr };
};

} // namespace ffsp

#endif // FFSP_HPP
