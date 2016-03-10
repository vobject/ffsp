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

extern "C" {
#include "libffsp/ffsp.h"
#include "libffsp/io_raw.h"
#include "libffsp/utils.h"
}

#include "spdlog/spdlog.h"

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

struct arguments {
	const char *device;
	int clustersize;
	int erasesize;
	int ninoopen;
	int neraseopen;
	int nerasereserve;
	int nerasewrites;
};

static void show_usage(const char *progname)
{
	printf("%s [OPTION] [DEVICE]\n", progname);
	printf("create a ffsp file system inside the given file [DEVICE]\n\n");
	printf("-c, --clustersize=N use a clusterblock size of N (default:4KiB)\n");
	printf("-e, --erasesize=N use a eraseblock size of N (default:4MiB)\n");
	printf("-i, --open-ino=N support caching of N dirty inodes at a time (default:100)\n");
	printf("-o, --open-eb=N support N open erase blocks at a time (default:5)\n");
	printf("-r, --reserve-eb=N reserve N erase blocks for internal use (default:3)\n");
	printf("-w, --write-eb=N Perform garbage collection after N erase blocks have been written (default:5)\n");
}

static int parse_arguments(int argc, char **argv, struct arguments *args)
{
	static const struct option long_options[] = {
		{ "clustersize", 1, NULL, 'c' },
		{ "erasesize", 1, NULL, 'e' },
		{ "open-ino", 1, NULL, 'i' },
		{ "open-eb", 1, NULL, 'o' },
		{ "reserve-eb", 1, NULL, 'r' },
		{ "write-eb", 1, NULL, 'w' },
		{ NULL, 0, NULL, 0 },
	};

	memset(args, 0, sizeof(*args));
	// Default size for clusters is 4 KiB
	args->clustersize = 1024 * 32;
	// Default size for erase blocks is 4 MiB
	args->erasesize = 1024 * 1024 * 4;
	// Default number of cached dirty inodes is 100
	args->ninoopen = 128;
	// Default number of open erase blocks is 5
	args->neraseopen = 5;
	// Default number of reserved erase blocks is 3
	args->nerasereserve = 3;
	// Default number of erase block finalizations before GC is 5
	args->nerasewrites = 5;

	while (1) {
		int c;

		c = getopt_long(argc, argv, "c:e:i:o:r:w:", long_options, &optind);

		if (c == -1)
			break;

		switch (c) {
			case 'c':
				args->clustersize = atoi(optarg);
				break;
			case 'e':
				args->erasesize = atoi(optarg);
				break;
			case 'i':
				args->ninoopen = atoi(optarg);
				break;
			case 'o':
				args->neraseopen = atoi(optarg);
				break;
			case 'r':
				args->nerasereserve = atoi(optarg);
				break;
			case 'w':
				args->nerasewrites = atoi(optarg);
				break;
			case '?':
				show_usage(argv[0]);
				return -EINVAL;
			default:
				break;
		}
	}

	if (optind != (argc - 1)) {
		fprintf(stderr, "%s: invalid arguments\n", argv[0]);
		return -1;
	}
	args->device = argv[optind];
	return 0;
}

static int get_eraseblk_cnt(const char *device, const int eb_size)
{
	int fd;
	off_t size;

	fd = open(device, O_RDONLY);
	if (fd == -1) {
		perror("open() on filesystem failed\n");
		exit(EXIT_FAILURE);
	}

	size = lseek(fd, 0, SEEK_END);
	if (size == -1) {
		perror("lseek() on filesystem failed\n");
		exit(EXIT_FAILURE);
	}

	if (close(fd) == -1) {
		perror("close() on filesystem failed\n");
		exit(EXIT_FAILURE);
	}
	return size / eb_size;
}

static int get_inode_cnt(const int eb_size, const int cl_size,
		const int eb_cnt)
{
	// Note that the first inode number is always invalid.
	return (eb_size	 // Only look at the first erase block
		- cl_size // super block aligned to clustersize
		- (eb_cnt * sizeof(struct ffsp_eraseblk))) // eb usage
		/ sizeof(uint32_t); // inodes are 4 bytes in size

	// TODO: FFSP_RESERVED_INODE_ID is not taken care of.
	//  But it is highly unlikely that the file system is created with
	//  as many inodes that FFSP_RESERVED_INODE_ID could be tried
	//  to be used as a valid inode_no.
}

static int setup_fs(const struct arguments *args)
{
	int rc;
	int fd;
	int eb_cnt;
	int ino_cnt;
	char *eb_buf;
	int eb_buf_written;
	unsigned int max_writeops;
	struct ffsp_super sb;
	struct ffsp_eraseblk eb;
	
	memset(&sb, 0, sizeof(sb));
	memset(&eb, 0, sizeof(eb));

	auto console = spdlog::stdout_logger_mt("console");
	console->info("Size of in memory data structures:");
	console->info("\tffsp: {}", sizeof(struct ffsp));
	console->info("\tffsp_super: {}", sizeof(struct ffsp_super));
	console->info("\tffsp_inode: {}", sizeof(struct ffsp_inode));
	console->info("\tffsp_dentry: {}", sizeof(struct ffsp_dentry));
	console->info("\tffsp_timespec: {}", sizeof(struct ffsp_timespec));
	console->info("\tffsp_eraseblk: {}", sizeof(struct ffsp_eraseblk));

	console->info("Setup file system:");
	console->info("\tdevice: {}", args->device);
	console->info("\tclustersize: {}", args->clustersize);
	console->info("\terasesize: {}", args->erasesize);
	console->info("\tninoopen: {}", args->ninoopen);
	console->info("\tneraseopen: {}", args->neraseopen);
	console->info("\tnerasereserve: {}", args->nerasereserve);
	console->info("\tnerasewrites: {}", args->nerasewrites);

	// Setup the first eraseblock with super, usage and inodemap
	eb_buf = (char*)malloc(args->erasesize);
	if (!eb_buf) {
		perror("malloc(erasesize)\n");
		return -1;
	}
	max_writeops = args->erasesize / args->clustersize;
	eb_cnt = get_eraseblk_cnt(args->device, args->erasesize);
	ino_cnt = get_inode_cnt(args->erasesize, args->clustersize, eb_cnt);

	sb.s_fsid = put_be32(FFSP_FILE_SYSTEM_ID);
	sb.s_flags = put_be32(0);
	sb.s_neraseblocks = put_be32(eb_cnt);
	sb.s_nino = put_be32(ino_cnt);
	sb.s_blocksize = put_be32(args->clustersize);
	sb.s_clustersize = put_be32(args->clustersize);
	sb.s_erasesize = put_be32(args->erasesize);
	sb.s_ninoopen = put_be32(args->ninoopen);
	sb.s_neraseopen = put_be32(args->neraseopen);
	sb.s_nerasereserve = put_be32(args->nerasereserve);
	sb.s_nerasewrites = put_be32(args->nerasewrites);
	memcpy(eb_buf, &sb, sizeof(sb));
	eb_buf_written = sizeof(sb);

	// align eb_usage + ino_map to clustersize
	memset(eb_buf + eb_buf_written, 0, args->clustersize - sizeof(sb));
	eb_buf_written += args->clustersize - sizeof(sb);

	// The first EB is for the superblock, erase block usage and inodes ids
	eb.e_type = FFSP_EB_SUPER;
	eb.e_lastwrite = put_be16(0);
	eb.e_cvalid = put_be16(0);
	eb.e_writeops = put_be16(0);
	memcpy(eb_buf + eb_buf_written, &eb, sizeof(eb));
	eb_buf_written += sizeof(eb);

	// The second EB is for directory entries
	eb.e_type = FFSP_EB_DENTRY_INODE;
	eb.e_lastwrite = put_be16(0);
	eb.e_cvalid = put_be16(1); // Only the root directory exists...
	eb.e_writeops = put_be16(max_writeops); // ...but the eb is closed.
	memcpy(eb_buf + eb_buf_written, &eb, sizeof(eb));
	eb_buf_written += sizeof(eb);

	for (int i = 2; i < eb_cnt; ++i) {
		// The remaining erase blocks are empty
		eb.e_type = FFSP_EB_EMPTY;
		eb.e_lastwrite = put_be16(0);
		eb.e_cvalid = put_be16(0);
		eb.e_writeops = put_be16(0);
		memcpy(eb_buf + eb_buf_written, &eb, sizeof(eb));
		eb_buf_written += sizeof(eb);
	}

	// inode id 0 is defined to be invalid
	be32_t cl_id = put_be32(0xffffffff); // Value does not matter
	memcpy(eb_buf + eb_buf_written, &cl_id, sizeof(cl_id));
	eb_buf_written += sizeof(cl_id);

	// inode id 1 will point to the root inode
	cl_id = put_be32(args->erasesize / args->clustersize);
	memcpy(eb_buf + eb_buf_written, &cl_id, sizeof(cl_id));
	eb_buf_written += sizeof(cl_id);

	// The rest of the cluster id array will be set to 0 -> no indes.
	memset(eb_buf + eb_buf_written, 0, args->erasesize - eb_buf_written);

	// Write the first erase block into the file.
#ifdef _WIN32
	fd = open(args->device, O_WRONLY);
#else
	fd = open(args->device, O_WRONLY | O_SYNC);
#endif
	if (fd == -1) {
		perror("open()\n");
		free(eb_buf);
		return -1;
	}

	rc = ffsp_write_raw(fd, eb_buf, args->erasesize, 0);
	if (rc < 0) {
		errno = -rc;
		perror("setup_fs(): writing first eraseblock failed\n");
		free(eb_buf);
		return -1;
	}

	// Create the inode for the root directory
	struct ffsp_inode root;
	memset(&root, 0, sizeof(root));
	root.i_size = put_be64(sizeof(struct ffsp_dentry) * 2);
	root.i_flags = put_be32(FFSP_DATA_EMB);
	root.i_no = put_be32(1);
	root.i_nlink = put_be32(2);
#ifdef _WIN32
	root.i_uid = put_be32(0);
	root.i_gid = put_be32(0);
	root.i_mode = put_be32(S_IFDIR);
#else
	root.i_uid = put_be32(getuid());
	root.i_gid = put_be32(getgid());
	root.i_mode = put_be32(S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif
	ffsp_update_time(&root.i_ctime);
	memcpy(eb_buf, &root, sizeof(root));
	eb_buf_written = sizeof(root);

	// Fill the embedded data with file and/or directory entries
	struct ffsp_dentry dot; // "."
	memset(&dot, 0, sizeof(dot));
	dot.ino = put_be32(1);
	dot.len = (uint8_t)strlen(".");
	strcpy(dot.name, ".");
	memcpy(eb_buf + eb_buf_written, &dot, sizeof(dot));
	eb_buf_written += sizeof(dot);

	struct ffsp_dentry dotdot; // ".."
	memset(&dotdot, 0, sizeof(dotdot));
	dotdot.ino = put_be32(1);
	dotdot.len = (uint8_t)strlen("..");
	strcpy(dotdot.name, "..");
	memcpy(eb_buf + eb_buf_written, &dotdot, sizeof(dotdot));
	eb_buf_written += sizeof(dotdot);

	rc = ffsp_write_raw(fd, eb_buf, eb_buf_written, args->erasesize);
	if (rc < 0) {
		errno = -rc;
		perror("setup_fs(): writing second eraseblock failed\n");
		free(eb_buf);
		return -1;
	}
	free(eb_buf);

	if (close(fd) == -1) {
		perror("setup_fs(): closing filesystem failed\n");
		return -1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	struct arguments args;

	if (parse_arguments(argc, argv, &args) < 0)
		return EXIT_FAILURE;

	if (setup_fs(&args) == -1) {
		fprintf(stderr, "%s: failed to setup file system", argv[0]);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}