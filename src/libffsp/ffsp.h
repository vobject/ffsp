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

#ifndef FFSP_H
#define FFSP_H

#include "byteorder.h"

#include <assert.h>
#include <stdint.h>

#define FFSP_FILE_SYSTEM_ID	0x46465350
#define FFSP_VERSION_MAJOR	0
#define FFSP_VERSION_MINOR	0
#define FFSP_VERSION_PATCH	1

// #define FFSP_SUPER_NOATIME	0x01
#define FFSP_NAME_MAX	248

struct ffsp_super {
	be32_t s_fsid; // file system ID
	be32_t s_flags; // mount flags - TODO: What are these for? -> noatime(?)
	be32_t s_neraseblocks; // number of erase blocks
	be32_t s_nino; // supported number of files on the drive
	be32_t s_blocksize; // currently same as clustersize
	be32_t s_clustersize; // size of a cluster (aka inode + data) -> 4096
	be32_t s_erasesize; // size of an erase block (in bytes or clusters?)
	be32_t s_ninoopen; // dirty inodes to cache before writing to disk
	be32_t s_neraseopen; // erase blocks to be hold open simultaneously
	be32_t s_nerasereserve; // number of erase blocks for internal use
	be32_t s_nerasewrites; // number of erase block to finalize before GC

	be32_t reserved[21]; // extend to 128 Bytes
};
static_assert(sizeof(struct ffsp_super) == 128, "struct ffsp_super: unexpected size");

struct ffsp_timespec {
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
static_assert(sizeof(struct ffsp_timespec) == 12, "struct ffsp_timespec: unexpected size");

// Invalid index inside the inode map
#define FFSP_INVALID_INO_NO	0

// Cluster ids that the inode map points to
#define FFSP_FREE_CL_ID	0x00000000
#define FFSP_RESERVED_CL_ID	0xffffffff

// erase block ids - 32bit
#define FFSP_INVALID_EB_ID	0x00000000

// inode data format - 8bit
//#define FFSP_DATA_SUPER	0x00
#define FFSP_DATA_EMB		0x01
#define FFSP_DATA_CLIN		0x02
#define FFSP_DATA_EBIN		0x04
//#define FFSP_DATA_CLEAN	0x08

struct ffsp_inode {
	be64_t i_size;
	be32_t i_flags; // e.g. multiple inodes in the same cluster, inode type
	be32_t i_no;
	be32_t i_nlink;
	be32_t i_uid;
	be32_t i_gid;
	be32_t i_mode; // inode type and permissions
	be64_t i_rdev;

	struct ffsp_timespec i_atime;
	struct ffsp_timespec i_ctime;
	struct ffsp_timespec i_mtime;

	be32_t reserved[13]; // extend to 128 Bytes
};
static_assert(sizeof(struct ffsp_inode) == 128, "struct ffsp_inode: unexpected size");

// TODO: Find out if these types are REALLY written to the file system!
//  Or if they are only necessary at runtime to determine in which erase block
//  to put data (before it has ever been garbage collected).
#define FFSP_EB_SUPER		0x00
#define FFSP_EB_DENTRY_INODE	0x01
#define FFSP_EB_DENTRY_CLIN	0x02
#define FFSP_EB_FILE_INODE	0x04
#define FFSP_EB_FILE_CLIN	0x08
#define FFSP_EB_EBIN		0x10
#define FFSP_EB_EMPTY		0x20

struct ffsp_eraseblk {
	uint8_t e_type;
	uint8_t reserved;
	be16_t e_lastwrite;
	be16_t e_cvalid; // valid clusters inside the erase block
	be16_t e_writeops; // how many writes were performed on this eb
};
static_assert(sizeof(struct ffsp_eraseblk) == 8, "struct ffsp_eraseblk: unexpected size");

struct ffsp_dentry {
	be32_t ino;
	uint8_t len;
	uint8_t reserved[3];
	char name[FFSP_NAME_MAX];
};
static_assert(sizeof(struct ffsp_dentry) == 256, "struct ffsp_dentry: unexpected size");

// In-Memory-only structures

struct ffsp_inode_cache;

struct ffsp_summary_list_node {
	int eb_type;
	be32_t *summary;
	struct ffsp_summary_list_node *next;
};

struct ffsp_gcinfo {
	int eb_type;
	unsigned int write_time;
	unsigned int write_cnt;
};

struct ffsp {
	int fd;

	uint32_t fsid; // file system ID
	uint32_t flags; // mount flags - TODO: What are these for? -> noatime(?)
	uint32_t neraseblocks; // number of erase blocks
	uint32_t nino; // supported number of files on the drive
	uint32_t blocksize; // currently same as clustersize
	uint32_t clustersize; // size of a cluster (aka inode + data) -> 4096
	uint32_t erasesize; // size of an erase block (in bytes or clusters?)
	uint32_t ninoopen; // dirty inodes to cache before writing to disk
	uint32_t neraseopen; // erase blocks to be hold open simultaneously
	uint32_t nerasereserve; // number of erase blocks for internal use
	uint32_t nerasewrites; // number of erase block to finalize before GC

	// Array with information about every erase block
	struct ffsp_eraseblk *eb_usage;

	// This array contains the cluster ids where the specified inode is
	//  located on disk. It is indexed using the inode number (ino->i_no).
	//  It is read at mount time and is occasionally written back to disk.
	//  It resides inside the first erase block of the file system and NOT
	//  inside the log.
	be32_t *ino_map;

	// Head of a linked list that contains all the erase block summary
	//  to all currently open cluster indirect erase blocks. When a
	//  cluster indirect erase block is full its summary is written as
	//  its last cluster. The erase block summary contains a list of all
	//  inode ids that contain indirect clusters inside the erase block.
	struct ffsp_summary_list_node summary_head;

	// A data structure that caches all inodes that have been
	//  looked up from the file system. "ino_status_map" (see below) is
	//  used to determine which of those inodes are dirty.
	struct ffsp_inode_cache *ino_cache;

	// A buffer that represents each (possible) inode with one bit. Its
	//  status indicates whether the (cached) inode was changed (is dirty)
	//  but was not yet written back to the medium.
	uint32_t *ino_status_map;

	/* TODO: replace this with a real data structure and provide
	 * accessory functions for manipulation */
	// A buffer where each element represents a cluster in the file system.
	//  It contains information about how many valid inodes are present
	//  inside the concerned cluster.The array is indexed using the
	//  cluster id resp. cluster_offset / cluster_size.
	int *cl_occupancy;

	// A variable that counting the number of dirty inodes cached in
	//  main memory. The dirty inodes should be written back to disk if
	//  this counter reaches fs.ninoopen which is set at mkfs time.
	unsigned int dirty_ino_cnt;

	struct ffsp_gcinfo *gcinfo;

	// Static helper buffer, one erase block large.
	// It is used for moving around clusters or erase blocks.
	// For example when expanding inode embedded data to cluster indirect
	//  or from cluster indirect to erase block indirect.
	char *buf;
};

#endif // FFSP_H
