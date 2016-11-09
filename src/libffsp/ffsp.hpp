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

#include <cassert>

namespace ffsp
{

constexpr uint32_t FFSP_FILE_SYSTEM_ID{0x46465350}; // FFSP
constexpr int FFSP_VERSION_MAJOR{0};
constexpr int FFSP_VERSION_MINOR{0};
constexpr int FFSP_VERSION_PATCH{1};

constexpr uint32_t FFSP_SUPER_NOATIME{0x01};
constexpr int FFSP_NAME_MAX{248};

struct superblock
{
    be32_t s_fsid;          // file system ID
    be32_t s_flags;         // mount flags - TODO: What are these for? -> noatime(?)
    be32_t s_neraseblocks;  // number of erase blocks
    be32_t s_nino;          // supported number of files on the drive
    be32_t s_blocksize;     // currently same as clustersize
    be32_t s_clustersize;   // size of a cluster (aka inode + data) -> 4096
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

// Invalid index inside the inode map
constexpr uint32_t FFSP_INVALID_INO_NO{0};

// Cluster ids that the inode map points to
constexpr uint32_t FFSP_FREE_CL_ID{0x00000000};
constexpr uint32_t FFSP_RESERVED_CL_ID{0xffffffff};

// erase block ids - 32bit
constexpr uint32_t FFSP_INVALID_EB_ID{0x00000000};

// inode data format - 8bit
//constexpr uint8_t FFSP_DATA_SUPER{0x00};
constexpr uint8_t FFSP_DATA_EMB{0x01};
constexpr uint8_t FFSP_DATA_CLIN{0x02};
constexpr uint8_t FFSP_DATA_EBIN{0x04};
//constexpr uint8_t FFSP_DATA_CLEAN{0x08};

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

// TODO: Find out if these types are REALLY written to the file system!
//  Or if they are only necessary at runtime to determine in which erase block
//  to put data (before it has ever been garbage collected).
enum eraseblock_type : uint8_t
{
    FFSP_EB_SUPER = 0x00,
    FFSP_EB_DENTRY_INODE = 0x01,
    FFSP_EB_DENTRY_CLIN = 0x02,
    FFSP_EB_FILE_INODE = 0x04,
    FFSP_EB_FILE_CLIN = 0x08,
    FFSP_EB_EBIN = 0x10,
    FFSP_EB_EMPTY = 0x20,
    FFSP_EB_INVALID = 0xFF,
};
static_assert(sizeof(eraseblock_type) == 1, "eraseblock_type: unexpected size");

struct eraseblock
{
    eraseblock_type e_type;
    uint8_t reserved;
    be16_t e_lastwrite;
    be16_t e_cvalid;   // valid clusters inside the erase block
    be16_t e_writeops; // how many writes were performed on this eb
};
static_assert(sizeof(eraseblock) == 8, "eraseblock: unexpected size");

struct dentry
{
    be32_t ino;
    uint8_t len;
    uint8_t reserved[3];
    char name[FFSP_NAME_MAX];
};
static_assert(sizeof(dentry) == 256, "dentry: unexpected size");

// In-Memory-only structures

struct inode_cache;
struct summary_cache;
struct gcinfo;

struct fs_context
{
    int fd;

    uint32_t fsid;          // file system ID
    uint32_t flags;         // mount flags - TODO: What are these for? -> noatime(?)
    uint32_t neraseblocks;  // number of erase blocks
    uint32_t nino;          // supported number of files on the drive
    uint32_t blocksize;     // currently same as clustersize
    uint32_t clustersize;   // size of a cluster (aka inode + data) -> 4096
    uint32_t erasesize;     // size of an erase block (in bytes or clusters?)
    uint32_t ninoopen;      // dirty inodes to cache before writing to disk
    uint32_t neraseopen;    // erase blocks to be hold open simultaneously
    uint32_t nerasereserve; // number of erase blocks for internal use
    uint32_t nerasewrites;  // number of erase block to finalize before GC

    // Array with information about every erase block
    eraseblock* eb_usage;

    // This array contains the cluster ids where the specified inode is
    //  located on disk. It is indexed using the inode number (ino->i_no).
    //  It is read at mount time and is occasionally written back to disk.
    //  It resides inside the first erase block of the file system and NOT
    //  inside the log.
    be32_t* ino_map;

    // Head of a linked list that contains all the erase block summary
    //  to all currently open cluster indirect erase blocks. When a
    //  cluster indirect erase block is full its summary is written as
    //  its last cluster. The erase block summary contains a list of all
    //  inode ids that contain indirect clusters inside the erase block.
    // There can be only one erase block summary per erase block type at once.
    // This is because there can be only one open erase block per erase block type at once.
    // Therefore each erase block type can exist only once in the summary list at any point in time.
    ffsp::summary_cache* summary_cache;

    // A data structure that caches all inodes that have been
    //  looked up from the file system. "ino_status_map" (see below) is
    //  used to determine which of those inodes are dirty.
    ffsp::inode_cache* inode_cache;

    // A buffer that represents each (possible) inode with one bit. Its
    //  status indicates whether the (cached) inode was changed (is dirty)
    //  but was not yet written back to the medium.
    uint32_t* ino_status_map;

    /* TODO: replace this with a real data structure and provide
     * accessory functions for manipulation */
    // A buffer where each element represents a cluster in the file system.
    //  It contains information about how many valid inodes are present
    //  inside the concerned cluster.The array is indexed using the
    //  cluster id resp. cluster_offset / cluster_size.
    int* cl_occupancy;

    // A variable that counting the number of dirty inodes cached in
    //  main memory. The dirty inodes should be written back to disk if
    //  this counter reaches fs.ninoopen which is set at mkfs time.
    unsigned int dirty_ino_cnt;

    ffsp::gcinfo* gcinfo;

    // Static helper buffer, one erase block large.
    // It is used for moving around clusters or erase blocks.
    // For example when expanding inode embedded data to cluster indirect
    //  or from cluster indirect to erase block indirect.
    char* buf;
};

} // namespace ffsp

#endif // FFSP_HPP
