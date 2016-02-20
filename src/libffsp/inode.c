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

#include "ffsp.h"
#include "log.h"
#include "bitops.h"
#include "eraseblk.h"
#include "gc.h"
#include "io.h"
#include "io_raw.h"
#include "utils.h"
#include "inode_cache.h"
#include "inode_group.h"
#include "inode.h"

#include <sys/stat.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#define S_ISDIR(m)	(((m)&S_IFDIR)==S_IFDIR)
extern char *strndup(const char *s, size_t n);
#endif

struct ffsp_inode *ffsp_allocate_inode(const struct ffsp *fs)
{
	struct ffsp_inode *ino;

	ino = malloc(fs->clustersize);
	if (!ino) {
		FFSP_ERROR("malloc(inode) failed");
		abort();
	}
	memset(ino, 0, fs->clustersize);
	return ino;
}

void ffsp_delete_inode(struct ffsp_inode *ino)
{
	free(ino);
}

void *ffsp_inode_data(struct ffsp_inode *ino)
{
	/*
	 * The inodes' embedded data section is located directly behind
	 * the ffsp_inode structure data. It is also valid because we
	 * always allocate the inode + embedded data together.
	 */
	return ino + 1;
}

/* Return the size of an inode (with its data or indirect pointers) in bytes. */
int ffsp_get_inode_size(const struct ffsp *fs, const struct ffsp_inode *ino)
{
	/*
	 * The size of an inode is sizeof(struct ffsp_inode) plus
	 * - embedded: the file size
	 * - cluster indirect: the size of valid cluster indirect pointers
	 * - erase block indirect: the size of valid eb indirect pointers
	 */

	int inode_size;
	int i_flags;
	int i_size;

	inode_size = sizeof(struct ffsp_inode);
	i_flags = get_be32(ino->i_flags);
	i_size = get_be64(ino->i_size);

	if (i_flags & FFSP_DATA_EMB)
		inode_size += i_size;
	else if (i_flags & FFSP_DATA_CLIN)
		inode_size += (i_size - 1) / fs->clustersize * sizeof(be32_t)
				+ sizeof(be32_t);
	else if (i_flags & FFSP_DATA_EBIN)
		inode_size += (i_size - 1) / fs->erasesize * sizeof(be32_t)
				+ sizeof(be32_t);
	return inode_size;
}

/* Check if a given inode is located at the given cluster id. */
bool ffsp_is_inode_valid(const struct ffsp *fs, unsigned int cl_id,
		const struct ffsp_inode *ino)
{
	const unsigned int ino_no = get_be32(ino->i_no);

	/*
	 * An inode cluster is considered valid if the inode number it
	 * contains points back to the inode's cluster id.
	 */
	return ((ino_no < fs->nino) /* sanity check */
			&& (get_be32(fs->ino_map[ino_no]) == cl_id));
}

static void split_path(const char *path, char **parent, char **name)
{
	// TODO: Check if the last character is a "/"
	// TODO: Make sure name is max NAME_MAX chars long

	const char *tmp = strrchr(path, '/') + 1;
	*name = strndup(tmp, FFSP_NAME_MAX);
	*parent = strndup(path, strlen(path) - strlen(*name) - 1);
}


static unsigned int find_free_inode_no(struct ffsp *fs)
{
	for (unsigned int ino_no = 1; ino_no < fs->nino; ino_no++)
		if (get_be32(fs->ino_map[ino_no]) == FFSP_FREE_CL_ID)
			return ino_no;
	return FFSP_INVALID_INO_NO;
}

static void mk_directory(struct ffsp_inode *ino, unsigned int parent_no)
{
	struct ffsp_dentry *dentry_ptr;

	dentry_ptr = ffsp_inode_data(ino);

	// Add "." and ".." to the embedded data section
	dentry_ptr[0].ino = ino->i_no;
	dentry_ptr[0].len = strlen(".");
	strcpy(dentry_ptr[0].name, ".");

	dentry_ptr[1].ino = put_be32(parent_no);
	dentry_ptr[1].len = strlen("..");
	strcpy(dentry_ptr[1].name, "..");

	// Meta information about "." and ".."
	ino->i_size = put_be64(sizeof(struct ffsp_dentry) * 2);
	ino->i_nlink = put_be32(2);
}

static int add_dentry(struct ffsp *fs, const char *path,
		unsigned int inode_no, mode_t mode)
{
	int rc;
	char *name;
	char *parent;
	struct ffsp_inode *ino;
	struct ffsp_dentry dent;
	uint64_t file_size;

	split_path(path, &parent, &name);
	rc = ffsp_lookup(fs, &ino, parent);

	free(parent);
	if (rc < 0) {
		free(name);
		return rc;
	}

	memset(&dent, 0, sizeof(dent));
	dent.ino = put_be32(inode_no);
	dent.len = strlen(name);
	strcpy(dent.name, name);
	free(name);

	file_size = get_be64(ino->i_size);
	// Append the new dentry at the inode's data.
	rc = ffsp_write(fs, ino, &dent, sizeof(dent), file_size);
	if (rc < 0)
		return rc;

	// The link count of the parent directory must be incremented
	//  in case the path points to a directory instead of a file.
	if (S_ISDIR(mode)) {
		inc_be32(&ino->i_nlink);
		ffsp_mark_dirty(fs, ino);
	}
	// HACK: returning signed int, but i_no is unsigned
	return get_be32(ino->i_no); // Return parents inode_no on success.
}

static int remove_dentry(struct ffsp *fs, const char *path,
		unsigned int inode_no, mode_t mode)
{
	int rc;
	char *name;
	char *parent;
	struct ffsp_inode *ino;
	struct ffsp_dentry *dent_buf;
	int dent_cnt;

	split_path(path, &parent, &name);
	rc = ffsp_lookup(fs, &ino, parent);

	free(name);
	free(parent);

	if (rc < 0)
		return rc;

	// Now that we have its parent directory inode, find the files dentry.
	rc = ffsp_cache_dir(fs, ino, &dent_buf, &dent_cnt);
	if (rc < 0)
		return rc;

	for (int i = 0; i < dent_cnt; i++) {
		if (get_be32(dent_buf[i].ino) == inode_no) {
			inode_no = get_be32(dent_buf[i].ino);
			dent_buf[i].ino = put_be32(0);
			dent_buf[i].len = 0;

			// TODO: write only affected cluster.
			ffsp_write(fs, ino, dent_buf, dent_cnt * sizeof(struct ffsp_dentry), 0);
			break;
		}
	}
	free(dent_buf);

	// Check if the requested name was even found inside the directory.
	if (inode_no == FFSP_INVALID_INO_NO)
		return -ENOENT;

	// The link count of the parent directory must be decremented
	//  in case the path points to a directory instead of a file.
	if (S_ISDIR(mode)) {
		dec_be32(&ino->i_nlink);
		ffsp_mark_dirty(fs, ino);
	}
	return 0;
}

static int find_dentry(const struct ffsp *fs, struct ffsp_inode *ino,
		const char *name, struct ffsp_dentry *dent)
{
	int rc;
	struct ffsp_dentry *dent_buf;
	int dent_cnt;

	// Number of potential ffsp_dentry elements. The exact number is not
	//  tracked. Return value of < 0 indicates an error.
	rc = ffsp_cache_dir(fs, ino, &dent_buf, &dent_cnt);
	if (rc < 0)
		return rc;

	for (int i = 0; i < dent_cnt; i++) {
		if (get_be32(dent_buf[i].ino) == FFSP_INVALID_INO_NO)
			continue; // Invalid ffsp_entry
		if (!strncmp(dent_buf[i].name, name, FFSP_NAME_MAX)) {
			memcpy(dent, &dent_buf[i], sizeof(*dent));
			free(dent_buf);
			return 0;
		}
	}
	// The requested name was not found.
	free(dent_buf);
	return -1;
}

static int dentry_is_empty(struct ffsp *fs, struct ffsp_inode *ino)
{
	int rc;
	struct ffsp_dentry *dent_buf;
	int dent_cnt;

	// Number of potential ffsp_dentry elements. The exact number is not
	//  tracked. Return value of < 0 indicates an error.
	rc = ffsp_cache_dir(fs, ino, &dent_buf, &dent_cnt);
	if (rc < 0)
		return rc;

	for (int i = 0; i < dent_cnt; i++) {
		if (get_be32(dent_buf[i].ino) == FFSP_INVALID_INO_NO)
			continue; // Invalid ffsp_entry

		if (!strncmp(dent_buf[i].name, ".", FFSP_NAME_MAX))
			continue;
		if (!strncmp(dent_buf[i].name, "..", FFSP_NAME_MAX))
			continue;

		free(dent_buf);
		return 0; // the directory is not empty
	}
	free(dent_buf);
	return 1; // the directory is empty
}

int ffsp_lookup_no(struct ffsp *fs, struct ffsp_inode **ino, uint32_t ino_no)
{
	unsigned int cl_id;
	struct ffsp_inode **inodes;
	int ino_cnt;

	*ino = ffsp_inode_cache_find(fs->ino_cache, put_be32(ino_no));
	if (*ino)
		return 0;

	/* The requested inode is not present inside the inode list.
	 * Read it from disk and add it to the inode list. */

	/* How many inodes may fit into one cluster? */
	inodes = malloc((fs->clustersize / sizeof(struct ffsp_inode)) *
						sizeof(struct ffsp_inode *));
	if (!inodes) {
		FFSP_ERROR("malloc(valid inode pointers) failed");
		abort();
	}

	cl_id = get_be32(fs->ino_map[ino_no]);
	ino_cnt = ffsp_read_inode_group(fs, cl_id, inodes);
	if (ino_cnt < 0) {
		free(inodes);
		return ino_cnt; /* I/O error */
	} else if (ino_cnt == 0) {
		free(inodes);
		return -ENOENT; /* the requested inode does not exists */
	}

	for (int i = 0; i < ino_cnt; i++)
		ffsp_inode_cache_insert(fs->ino_cache, inodes[i]);

	/* the requested inode should now be present inside the inode cache */
	*ino = ffsp_inode_cache_find(fs->ino_cache, put_be32(ino_no));
	free(inodes);
	return 0;
}

int ffsp_lookup(struct ffsp *fs, struct ffsp_inode **ino, const char *path)
{
	int rc;
	char *path_mod;
	const char *token;
	struct ffsp_inode *dir_ino;
	struct ffsp_dentry dentry;

	path_mod = strndup(path, FFSP_NAME_MAX + 1);
	ffsp_lookup_no(fs, &dir_ino, 1);

	for (char *p = path_mod;; p = NULL) {
		token = strtok(p, "/");
		if (!token)
			break;

		// There is yet at least one other token to lookup.
		//  But the inode of the parent token was already a file!
		//  So this can be no valid path.
		if (!S_ISDIR(get_be32(dir_ino->i_mode))) {
			free(path_mod);
			return -ENOENT;
		}

		rc = find_dentry(fs, dir_ino, token, &dentry);
		if (rc < 0) {
			// The name was not found inside the parent folder.
			free(path_mod);
			return -ENOENT;
		}

		// Read the token's inode from disk (if it is not yet cached).
		//  This will be either another entry or a file inode.
		rc = ffsp_lookup_no(fs, &dir_ino, get_be32(dentry.ino));
		if (rc < 0) {
			free(path_mod);
			return rc;
		}
	}
	// Iteration over all of the path's tokens succeeded.
	// We now either have the requested file or entry inode in memory.
	*ino = dir_ino;
	free(path_mod);
	return 0;
}

static bool should_write_inodes(const struct ffsp *fs)
{
	return fs->dirty_ino_cnt >= fs->ninoopen;
}

/* Check if a cached inode is makred as being dirty. */
static bool is_inode_dirty(struct ffsp *fs, struct ffsp_inode *ino)
{
	return test_bit(fs->ino_status_map, get_be32(ino->i_no));
}

/*
 * Search for dirty inodes inside the inode cache and put a pointer to them
 * into the corresponding output buffer. Return the amount of inodes found.
 */
static int get_dirty_inodes(struct ffsp *fs, struct ffsp_inode **inodes,
		bool dentry_flag)
{
	int ino_cnt;
	struct ffsp_inode_cache_status status;
	struct ffsp_inode *ino;

	ino_cnt = 0;
	ffsp_inode_cache_init_status(&status);

	while ((ino = ffsp_inode_cache_next(fs->ino_cache, &status))) {
		if (is_inode_dirty(fs, ino)) {
			if (dentry_flag && S_ISDIR(get_be32(ino->i_mode)))
				inodes[ino_cnt++] = ino;
			else if (!dentry_flag && !S_ISDIR(get_be32(ino->i_mode)))
				inodes[ino_cnt++] = ino;
		}
	}
	return ino_cnt;
}

int ffsp_flush_inodes(struct ffsp *fs, bool force)
{
	int rc;
	struct ffsp_inode **inodes;
	int ino_cnt;

	if (!force && !should_write_inodes(fs))
		return 0;

	inodes = malloc(fs->dirty_ino_cnt * sizeof(struct ffsp_inode *));
	if (!inodes) {
		FFSP_ERROR("malloc(dirty inodes cache) failed");
		abort();
	}

	/* process dirty dentry inodes */
	ino_cnt = get_dirty_inodes(fs, inodes, true);
	rc = ffsp_write_inodes(fs, inodes, ino_cnt);

	if (rc == 0) {
		/* process dirty file inodes */
		ino_cnt = get_dirty_inodes(fs, inodes, false);
		rc = ffsp_write_inodes(fs, inodes, ino_cnt);
	}

	free(inodes);
	return rc;
}

int ffsp_release_inodes(struct ffsp *fs)
{
	int rc;
	struct ffsp_inode_cache_status status;
	struct ffsp_inode *ino;

	/* write all dirty inodes to disk */
	rc = ffsp_flush_inodes(fs, true);
	if (rc < 0)
		return rc;

	/* remove all cached inodes from memory */
	ffsp_inode_cache_init_status(&status);
	while ((ino = ffsp_inode_cache_next(fs->ino_cache, &status))) {
		ffsp_inode_cache_remove(fs->ino_cache, ino);
		ffsp_delete_inode(ino);
	}

	/* GC cannot hurt at this point */
	ffsp_gc(fs);
	return 0;
}

int ffsp_create(struct ffsp *fs, const char *path, mode_t mode,
		uid_t uid, gid_t gid, dev_t device)
{
	unsigned int inode_no;
	int parent_no; // HACK: should not have to be signed!
	struct ffsp_inode *ino;

	inode_no = find_free_inode_no(fs);
	if (inode_no == FFSP_INVALID_INO_NO)
		return -ENOSPC; // max number of files in the fs reached

	parent_no = add_dentry(fs, path, inode_no, mode);
	if (parent_no < 0)
		return parent_no; // return error code

	// initialize a file inode by default
	ino = ffsp_allocate_inode(fs);
	ino->i_size = put_be64(0);
	ino->i_flags = put_be32(FFSP_DATA_EMB);
	ino->i_no = put_be32(inode_no);
	ino->i_nlink = put_be32(1);
	ino->i_uid = put_be32(uid);
	ino->i_gid = put_be32(gid);
	ino->i_mode = put_be32(mode);
	ino->i_rdev = put_be64(device);
	ffsp_update_time(&ino->i_ctime);

	// Handle creation of a directory.
	if (S_ISDIR(mode))
		mk_directory(ino, parent_no);

	// HACK: we have to occupy an inode_no inside the inomap.
	//  Otherwise it is still marked as 'free'.
	//  Its content will be updated when the inode is actually written.
	fs->ino_map[inode_no] = put_be32(FFSP_RESERVED_CL_ID);

	ffsp_inode_cache_insert(fs->ino_cache, ino);
	ffsp_mark_dirty(fs, ino);
	ffsp_flush_inodes(fs, false);
	return 0;
}

int ffsp_symlink(struct ffsp *fs, const char *oldpath, const char *newpath,
		uid_t uid, gid_t gid)
{
	int rc;
	mode_t mode;
	struct ffsp_inode *ino;

#ifdef _WIN32
	mode = S_IFLNK; // FIXME
#else
	mode = S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO;
#endif

	rc = ffsp_create(fs, newpath, mode, uid, gid, 0);
	if (rc < 0)
		return rc;

	rc = ffsp_lookup(fs, &ino, newpath);
	if (rc < 0)
		return rc;

	rc = ffsp_write(fs, ino, oldpath, strlen(oldpath), 0);
	if (rc < 0)
		ffsp_unlink(fs, newpath); // Remove empty file

	ffsp_flush_inodes(fs, false);
	return (rc < 0) ? rc : 0;
}

int ffsp_readlink(struct ffsp *fs, const char *path, char *buf, size_t len)
{
	int rc;
	struct ffsp_inode *ino;

	rc = ffsp_lookup(fs, &ino, path);
	if (rc < 0)
		return rc;

	rc = ffsp_read(fs, ino, buf, len - 1, 0);
	if (rc < 0)
		return rc;

	buf[rc] = '\0';
	return 0;
}

int ffsp_link(struct ffsp *fs, const char *oldpath, const char *newpath)
{
	int rc;
	struct ffsp_inode *ino;
	unsigned int inode_no;
	mode_t mode;

	// We need the inode_no of the existing path.
	rc = ffsp_lookup(fs, &ino, oldpath);
	if (rc < 0)
		return rc;

	inode_no = get_be32(ino->i_no);
	mode = get_be32(ino->i_mode);

	rc = add_dentry(fs, newpath, inode_no, mode);
	if (rc < 0)
		return rc;

	inc_be32(&ino->i_nlink);
	ffsp_flush_inodes(fs, false);
	return 0;
}

int ffsp_unlink(struct ffsp *fs, const char *path)
{
	int rc;
	struct ffsp_inode *ino;
	unsigned int inode_no;
	mode_t mode;
	unsigned int cl_id;
	unsigned int eb_id;
	unsigned int file_flags;
	uint64_t file_size;
	be32_t *ind_ptr;
	int ind_cnt;
	int ind_size;
	int ind_type;

	// We need the inode_no of the existing path so that we can find the
	//  corresponding dentry inside its parent data.
	rc = ffsp_lookup(fs, &ino, path);
	if (rc < 0)
		return rc;

	inode_no = get_be32(ino->i_no);
	mode = get_be32(ino->i_mode);

	rc = remove_dentry(fs, path, inode_no, mode);
	if (rc < 0)
		return rc;

	if (get_be32(ino->i_nlink) > 1) {
		// The inode is referenced by more than one dentry.
		// Do not delete/invalidate it but only reduce its link count.
		dec_be32(&ino->i_nlink);
		ffsp_mark_dirty(fs, ino);
	} else if (get_be32(ino->i_nlink) == 1) {
		/* We just removed the only reference to this inode.
		 * It can now be removed from the file system. */

		/* decrement the number of valid inodes inside the old inode's
		 * cluster (in case it really had one). */
		cl_id = get_be32(fs->ino_map[inode_no]);
		if (cl_id != FFSP_RESERVED_CL_ID) {
			fs->cl_occupancy[cl_id]--;

			/* also decrement the number of valid inode clusters
			 * inside the affected erase block in case the cluster
			 * does not contain any more valid inodes at all. */
			if (!fs->cl_occupancy[cl_id]) {
				eb_id = cl_id * fs->clustersize / fs->erasesize;
				ffsp_eb_dec_cvalid(fs, eb_id);
			//	dec_be16(&fs->eb_usage[eb_id].e_cvalid);
			}
		}

		/* set the old file's inode number to 'free' */
		fs->ino_map[inode_no] = put_be32(FFSP_FREE_CL_ID);

		file_size = get_be64(ino->i_size);
		file_flags = get_be32(ino->i_flags);

		// Release indirect data if needed.
		if (!(file_flags & FFSP_DATA_EMB) && file_size) {
			if (file_flags & FFSP_DATA_CLIN) {
				ind_type = FFSP_DATA_CLIN;
				ind_size = fs->clustersize;
			} else if (file_flags & FFSP_DATA_EBIN) {
				ind_type = FFSP_DATA_EBIN;
				ind_size = fs->erasesize;
			} else {
				FFSP_ERROR("ffsp_unlink(): Invalid inode flags");
				return -1;
			}
			ind_cnt = ((file_size - 1) / ind_size) + 1;
			ind_ptr = ffsp_inode_data(ino);
			ffsp_invalidate_ind_ptr(fs, ind_ptr, ind_cnt, ind_type);
		}
		ffsp_inode_cache_remove(fs->ino_cache, ino);
		ffsp_reset_dirty(fs, ino);
		ffsp_delete_inode(ino);
	} else {
		FFSP_ERROR("ffsp_unlink(): Invalid inode link count");
		return -1;
	}
	ffsp_flush_inodes(fs, false);
	return 0;
}

int ffsp_rmdir(struct ffsp *fs, const char *path)
{
	int rc;
	struct ffsp_inode *ino;
	unsigned int inode_no;
	mode_t mode;
	unsigned int cl_id;
	unsigned int eb_id;
	unsigned int file_flags;
	uint64_t file_size;
	be32_t *ind_ptr;
	int ind_cnt;
	int ind_size;
	int ind_type;

	// We need the inode_no of the existing path so that we can find the
	//  corresponding dentry inside its parent data.
	rc = ffsp_lookup(fs, &ino, path);
	if (rc < 0)
		return rc;

	if (dentry_is_empty(fs, ino) == 0)
		return -ENOTEMPTY;

	inode_no = get_be32(ino->i_no);
	mode = get_be32(ino->i_mode);

	// Invalidate the dentry inside the parent directory and also
	//  decrement the link count of the parent directory.
	rc = remove_dentry(fs, path, inode_no, mode);
	if (rc < 0)
		return rc;

	// From this point on "path" cannot be found inside any directory
	//  but its inode and potential data clusters/erase blocks still occupy
	//  memory on the drive.

	/* decrement the number of valid inodes inside the old inode's
	 * cluster (in case it really had one). */
	cl_id = get_be32(fs->ino_map[inode_no]);
	if (cl_id != FFSP_RESERVED_CL_ID) {
		fs->cl_occupancy[cl_id]--;

		/* also decrement the number of valid inode clusters
		 * inside the affected erase block in case the cluster
		 * does not contain any more valid inodes at all. */
		if (!fs->cl_occupancy[cl_id]) {
			eb_id = cl_id * fs->clustersize / fs->erasesize;
			ffsp_eb_dec_cvalid(fs, eb_id);
		//	dec_be16(&fs->eb_usage[eb_id].e_cvalid);
		}
	}

	/* set the old file's inode number to 'free' */
	fs->ino_map[inode_no] = put_be32(FFSP_FREE_CL_ID);

	file_size = get_be64(ino->i_size);
	file_flags = get_be32(ino->i_flags);

	// Release indirect data if needed.
	if (!(file_flags & FFSP_DATA_EMB)) {
		if (file_flags & FFSP_DATA_CLIN) {
			ind_type = FFSP_DATA_CLIN;
			ind_size = fs->clustersize;
		} else if (file_flags & FFSP_DATA_EBIN) {
			ind_type = FFSP_DATA_EBIN;
			ind_size = fs->erasesize;
		} else {
			FFSP_ERROR("ffsp_rmdir(): Invalid inode flags");
			return -1;
		}
		ind_cnt = ((file_size - 1) / ind_size) + 1;
		ind_ptr = ffsp_inode_data(ino);
		ffsp_invalidate_ind_ptr(fs, ind_ptr, ind_cnt, ind_type);
	}
	ffsp_inode_cache_remove(fs->ino_cache, ino);
	ffsp_reset_dirty(fs, ino);
	ffsp_delete_inode(ino);
	ffsp_flush_inodes(fs, false);
	return 0;
}

int ffsp_rename(struct ffsp *fs, const char *oldpath, const char *newpath)
{
	(void) fs;
	(void) oldpath;
	(void) newpath;

	// TODO: Implement Me!


	// new already exists -> it will be overwritten (with some exceptions)

	// old & new are hardlinks to the same file -> do nothing

	// old is a dir -> new must not exist or specify an empty dir
	//  ---> FUSE does not take care of this!

	// old refers to a symlink -> the link is renamed

	// new refers to a symlink -> the link will be overwritten

	// try to make a directory a subdirectory of itself -> return -EINVAL

	// new is an existing directory, but old is not a directory -> return -EISDIR

	// Too many symbolic links when in resolving old or new -> return -ELOOP

	// ENOENT: The link named by oldpath does not exist; or, a directory component
	//  in newpath does not exist; or, oldpath or newpath is an empty string

	// ENOSPC: The device containing the file has no room for the new directory entry

	// ENOTDIR: A  component  used  as a directory in oldpath or newpath is not,
	//  in fact, a directory.  Or, oldpath is a directory, and newpath exists
	//  but is not a directory

	// ENOTEMPTY or EEXIST: newpath is a nonempty directory, that is,
	//  contains entries other than "." and ".."


	ffsp_flush_inodes(fs, false);
	return -1;
}

void ffsp_mark_dirty(struct ffsp *fs, struct ffsp_inode *ino)
{
	unsigned int ino_no;
	unsigned int cl_id;
	unsigned int eb_id;

	if (is_inode_dirty(fs, ino))
		/* the inode is already dirty */
		return;

	ino_no = get_be32(ino->i_no);
	set_bit(fs->ino_status_map, ino_no);
	fs->dirty_ino_cnt++;

	FFSP_DEBUG("inode %u is now DIRTY - dirty_ino_cnt=%u",
			ino_no, fs->dirty_ino_cnt);

	/* decrement the number of valid inodes inside the old inode's
	 * cluster (in case it really had one). */
	cl_id = get_be32(fs->ino_map[ino_no]);
	if (cl_id != FFSP_RESERVED_CL_ID) {
		fs->cl_occupancy[cl_id]--;

		/* also decrement the number of valid inode clusters
		 * inside the affected erase block in case the cluster does
		 * not contain any more valid inodes at all. */
		if (!fs->cl_occupancy[cl_id]) {
			eb_id = cl_id * fs->clustersize / fs->erasesize;
			ffsp_eb_dec_cvalid(fs, eb_id);
		//	dec_be16(&fs->eb_usage[eb_id].e_cvalid);
		}
	}
}

void ffsp_reset_dirty(struct ffsp *fs, struct ffsp_inode *ino)
{
	unsigned int ino_no;

	if (is_inode_dirty(fs, ino)) {
		ino_no = get_be32(ino->i_no);
		clear_bit(fs->ino_status_map, ino_no);
		fs->dirty_ino_cnt--;
		FFSP_DEBUG("inode %u is now CLEAN - dirty_ino_cnt=%u",
				ino_no, fs->dirty_ino_cnt);
	}
}

int ffsp_cache_dir(const struct ffsp *fs, struct ffsp_inode *ino,
		struct ffsp_dentry **dent_buf, int *dent_cnt)
{
	int rc;
	uint64_t data_size;

	// Number of bytes till the end of the last valid dentry.
	data_size = get_be64(ino->i_size);

	*dent_buf = malloc(data_size);
	if (!*dent_buf) {
		FFSP_ERROR("ffsp_cache_dir(): malloc() failed.");
		return -1;
	}

	rc = ffsp_read(fs, ino, *dent_buf, data_size, 0);
	if (rc < 0) {
		free(*dent_buf);
		return rc;
	}
	// Potential amount of valid ffsp_dentry elements.
	*dent_cnt = data_size / sizeof(struct ffsp_dentry);
	return 0;
}

void ffsp_invalidate_ind_ptr(struct ffsp *fs, const be32_t *ind_ptr,
		int cnt, int ind_type)
{
	uint32_t ind_id;
	uint32_t eb_id;

	for (int i = 0; i < cnt; ++i) {
		ind_id = get_be32(ind_ptr[i]);

		if (!ind_id)
			continue; // File hole, not a real indirect pointer.

		if (ind_type == FFSP_DATA_CLIN) {
			// Tell the erase block usage information that there is
			//  now one additional cluster invalid in the specified
			//  erase block.
			eb_id = ind_id * fs->clustersize / fs->erasesize;
			ffsp_eb_dec_cvalid(fs, eb_id);
		//	dec_be16(&fs->eb_usage[eb_id].e_cvalid);
		} else if (ind_type == FFSP_DATA_EBIN) {
			// The erase block type is the only field of importance
			//  in this case. Just set the erase blocks usage
			//  information to EMPTY and we are done.
			fs->eb_usage[ind_id].e_type = FFSP_EB_EMPTY;
		}
	}
}
