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

#include "inode.hpp"
#include "bitops.hpp"
#include "eraseblk.hpp"
#include "ffsp.hpp"
#include "gc.hpp"
#include "inode_cache.hpp"
#include "inode_group.hpp"
#include "io.hpp"
#include "io_raw.hpp"
#include "log.hpp"
#include "utils.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#ifdef _WIN32
#ifndef S_ISDIR
#include <io.h>
#define S_ISDIR(mode) (((mode)&S_IFMT) == S_IFDIR)
#endif
extern "C" char* strndup(const char* s, size_t n);
#endif

namespace ffsp
{

inode* allocate_inode(const fs_context& fs)
{
    inode* ino = (inode*)malloc(fs.clustersize);
    if (!ino)
    {
        log().critical("malloc(inode) failed");
        abort();
    }
    memset(ino, 0, fs.clustersize);
    return ino;
}

void delete_inode(inode* ino)
{
    free(ino);
}

void* inode_data(inode* ino)
{
    /*
     * The inodes' embedded data section is located directly behind
     * the ffsp_inode structure data. It is also valid because we
     * always allocate the inode + embedded data together.
     */
    return ino + 1;
}

/* Return the size of an inode (with its data or indirect pointers) in bytes. */
unsigned int get_inode_size(const fs_context& fs, const inode* ino)
{
    /*
     * The size of an inode is sizeof(ffsp_inode) plus
     * - embedded: the file size
     * - cluster indirect: the size of valid cluster indirect pointers
     * - erase block indirect: the size of valid eb indirect pointers
     */

    unsigned int ino_size = sizeof(inode);
    unsigned int i_size = get_be64(ino->i_size);

    // the lower 8 bit carry the inode data type
    inode_data_type data_type = static_cast<inode_data_type>(get_be32(ino->i_flags) & 0xff);

    if (data_type == inode_data_type::emb)
        ino_size += i_size;
    else if (data_type == inode_data_type::clin)
        ino_size += (i_size - 1) / fs.clustersize * sizeof(be32_t) + sizeof(be32_t);
    else if (data_type == inode_data_type::ebin)
        ino_size += (i_size - 1) / fs.erasesize * sizeof(be32_t) + sizeof(be32_t);
    return ino_size;
}

/* Check if a given inode is located at the given cluster id. */
bool is_inode_valid(const fs_context& fs, cl_id_t cl_id, const inode* ino)
{
    ino_t ino_no = get_be32(ino->i_no);

    /*
     * An inode cluster is considered valid if the inode number it
     * contains points back to the inode's cluster id.
     */
    return ((ino_no < fs.nino) /* sanity check */
            && (get_be32(fs.ino_map[ino_no]) == cl_id));
}

static void split_path(const char* path, char** parent, char** name)
{
    // TODO: Check if the last character is a "/"
    // TODO: Make sure name is max NAME_MAX chars long

    const char* tmp = strrchr(path, '/') + 1;
    *name = strndup(tmp, FFSP_NAME_MAX);
    *parent = strndup(path, strlen(path) - strlen(*name) - 1);
}

static unsigned int find_free_inode_no(fs_context& fs)
{
    for (ino_t ino_no = 1; ino_no < fs.nino; ino_no++)
        if (get_be32(fs.ino_map[ino_no]) == FFSP_FREE_CL_ID)
            return ino_no;
    return FFSP_INVALID_INO_NO;
}

static void mk_directory(inode* ino, ino_t parent_ino_no)
{
    dentry* dentry_ptr = (dentry*)inode_data(ino);

    // Add "." and ".." to the embedded data section
    dentry_ptr[0].ino = ino->i_no;
    dentry_ptr[0].len = strlen(".");
    strcpy(dentry_ptr[0].name, ".");

    dentry_ptr[1].ino = put_be32(parent_ino_no);
    dentry_ptr[1].len = strlen("..");
    strcpy(dentry_ptr[1].name, "..");

    // Meta information about "." and ".."
    ino->i_size = put_be64(sizeof(dentry) * 2);
    ino->i_nlink = put_be32(2);
}

static int add_dentry(fs_context& fs, const char* path, ino_t ino_no,
                      mode_t mode, ino_t* parent_ino_no)
{
    char* name;
    char* parent;
    split_path(path, &parent, &name);

    inode* parent_ino;
    int rc = lookup(fs, &parent_ino, parent);

    free(parent);
    if (rc < 0)
    {
        free(name);
        return rc;
    }

    dentry dent;
    memset(&dent, 0, sizeof(dent));
    dent.ino = put_be32(ino_no);
    dent.len = strlen(name);
    strcpy(dent.name, name);
    free(name);

    // Append the new dentry at the inode's data.
    rc = write(fs, parent_ino, (char*)(&dent), sizeof(dent), get_be64(parent_ino->i_size));
    if (rc < 0)
        return rc;

    // The link count of the parent directory must be incremented
    //  in case the path points to a directory instead of a file.
    if (S_ISDIR(mode))
    {
        inc_be32(parent_ino->i_nlink);
        mark_dirty(fs, *parent_ino);
    }
    if (parent_ino_no)
    {
        // Return parents inode_no on success.
        *parent_ino_no = get_be32(parent_ino->i_no);
    }
    return 0;
}

static int remove_dentry(fs_context& fs, const char* path, ino_t ino_no, mode_t mode)
{
    char* name;
    char* parent;
    split_path(path, &parent, &name);

    inode* ino;
    int rc = lookup(fs, &ino, parent);

    free(name);
    free(parent);

    if (rc < 0)
        return rc;

    // Now that we have its parent directory inode, find the files dentry.
    dentry* dent_buf;
    int dent_cnt;
    rc = cache_dir(fs, ino, &dent_buf, &dent_cnt);
    if (rc < 0)
        return rc;

    for (int i = 0; i < dent_cnt; i++)
    {
        if (get_be32(dent_buf[i].ino) == ino_no)
        {
            ino_no = get_be32(dent_buf[i].ino);
            dent_buf[i].ino = put_be32(0);
            dent_buf[i].len = 0;

            // TODO: write only affected cluster.
            write(fs, ino, (char*)dent_buf, dent_cnt * sizeof(dentry), 0);
            break;
        }
    }
    free(dent_buf);

    // Check if the requested name was even found inside the directory.
    if (ino_no == FFSP_INVALID_INO_NO)
        return -ENOENT;

    // The link count of the parent directory must be decremented
    //  in case the path points to a directory instead of a file.
    if (S_ISDIR(mode))
    {
        dec_be32(ino->i_nlink);
        mark_dirty(fs, *ino);
    }
    return 0;
}

static int find_dentry(fs_context& fs, inode* ino, const char* name, dentry* dent)
{
    // Number of potential ffsp_dentry elements. The exact number is not
    //  tracked. Return value of < 0 indicates an error.
    dentry* dent_buf;
    int dent_cnt;
    int rc = cache_dir(fs, ino, &dent_buf, &dent_cnt);
    if (rc < 0)
        return rc;

    for (int i = 0; i < dent_cnt; i++)
    {
        if (get_be32(dent_buf[i].ino) == FFSP_INVALID_INO_NO)
            continue; // Invalid ffsp_entry
        if (!strncmp(dent_buf[i].name, name, FFSP_NAME_MAX))
        {
            memcpy(dent, &dent_buf[i], sizeof(*dent));
            free(dent_buf);
            return 0;
        }
    }
    // The requested name was not found.
    free(dent_buf);
    return -1;
}

static int dentry_is_empty(fs_context& fs, inode* ino)
{
    // Number of potential ffsp_dentry elements. The exact number is not
    //  tracked. Return value of < 0 indicates an error.
    dentry* dent_buf;
    int dent_cnt;
    int rc = cache_dir(fs, ino, &dent_buf, &dent_cnt);
    if (rc < 0)
        return rc;

    for (int i = 0; i < dent_cnt; i++)
    {
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

int lookup_no(fs_context& fs, inode** ino, ino_t ino_no)
{
    *ino = inode_cache_find(*fs.inode_cache, ino_no);
    if (*ino)
        return 0;

    /* The requested inode is not present inside the inode list.
     * Read it from disk and add it to the inode list. */

    cl_id_t cl_id = get_be32(fs.ino_map[ino_no]);
    std::vector<inode*> inodes;
    int rc = read_inode_group(fs, cl_id, inodes);
    if (rc < 0)
        return rc; // I/O error

    if (inodes.size() == 0)
        return -ENOENT; // no inodes in the given cluster

    for (const auto& inode : inodes)
        inode_cache_insert(*fs.inode_cache, inode);

    /* the requested inode should now be present inside the inode cache */
    *ino = inode_cache_find(*fs.inode_cache, ino_no);
    return 0;
}

int lookup(fs_context& fs, inode** ino, const char* path)
{
    inode* dir_ino;
    lookup_no(fs, &dir_ino, 1);

    char* path_mod = strndup(path, FFSP_NAME_MAX + 1);
    for (char* p = path_mod;; p = nullptr)
    {
        const char* token = strtok(p, "/");
        if (!token)
            break;

        // There is yet at least one other token to lookup.
        //  But the inode of the parent token was already a file!
        //  So this can be no valid path.
        if (!S_ISDIR(get_be32(dir_ino->i_mode)))
        {
            free(path_mod);
            return -ENOENT;
        }

        dentry dentry;
        int rc = find_dentry(fs, dir_ino, token, &dentry);
        if (rc < 0)
        {
            // The name was not found inside the parent folder.
            free(path_mod);
            return -ENOENT;
        }

        // Read the token's inode from disk (if it is not yet cached).
        //  This will be either another entry or a file inode.
        rc = lookup_no(fs, &dir_ino, get_be32(dentry.ino));
        if (rc < 0)
        {
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

static bool should_write_inodes(const fs_context& fs)
{
    return fs.dirty_ino_cnt >= fs.ninoopen;
}

/* Check if a cached inode is makred as being dirty. */
static bool is_inode_dirty(const fs_context& fs, const inode& ino)
{
    return test_bit(fs.ino_status_map, get_be32(ino.i_no));
}

/*
 * Search for dirty inodes inside the inode cache and put a pointer to them
 * into the corresponding output buffer. Return the amount of inodes found.
 */
static std::vector<inode*> get_dirty_inodes(const fs_context& fs, bool dentries)
{
    return inode_cache_get_if(*fs.inode_cache, [&](const inode& ino) {
        if (is_inode_dirty(fs, ino))
        {
            if (dentries && S_ISDIR(get_be32(ino.i_mode)))
                return true;
            else if (!dentries && !S_ISDIR(get_be32(ino.i_mode)))
                return true;
        }
        return false;
    });
}

int flush_inodes(fs_context& fs, bool force)
{
    if (!force && !should_write_inodes(fs))
        return 0;

    /* process dirty dentry inodes */
    auto inodes = get_dirty_inodes(fs, true);
    int rc = write_inodes(fs, inodes);

    if (rc == 0)
    {
        /* process dirty file inodes */
        inodes = get_dirty_inodes(fs, false);
        rc = write_inodes(fs, inodes);
    }

    return rc;
}

int release_inodes(fs_context& fs)
{
    /* write all dirty inodes to disk */
    int rc = flush_inodes(fs, true);
    if (rc < 0)
        return rc;

    /* remove all cached inodes from memory */
    for (const auto& ino : inode_cache_get(*fs.inode_cache))
    {
        inode_cache_remove(*fs.inode_cache, ino);
        delete_inode(ino);
    }

    /* GC cannot hurt at this point */
    gc(fs);
    return 0;
}

int create(fs_context& fs, const char* path, mode_t mode, uid_t uid, gid_t gid, dev_t device)
{
    ino_t ino_no = find_free_inode_no(fs);
    if (ino_no == FFSP_INVALID_INO_NO)
        return -ENOSPC; // max number of files in the fs reached

    ino_t parent_ino_no;
    int rc = add_dentry(fs, path, ino_no, mode, &parent_ino_no);
    if (rc < 0)
        return rc;

    // initialize a file inode by default
    inode* ino = allocate_inode(fs);
    ino->i_size = put_be64(0);
    ino->i_flags = put_be32(static_cast<uint32_t>(inode_data_type::emb));
    ino->i_no = put_be32(ino_no);
    ino->i_nlink = put_be32(1);
    ino->i_uid = put_be32(uid);
    ino->i_gid = put_be32(gid);
    ino->i_mode = put_be32(mode);
    ino->i_rdev = put_be64(device);
    update_time(ino->i_ctime);

    // Handle creation of a directory.
    if (S_ISDIR(mode))
        mk_directory(ino, parent_ino_no);

    // We have to occupy an inode_no inside the inomap. Otherwise it is still
    // marked as 'free' and there's no control over the max supported amount of
    // inodes in the file system. The inomap is updated with the inode's cluster
    // id when the inode is actually written.
    fs.ino_map[ino_no] = put_be32(FFSP_RESERVED_CL_ID);

    inode_cache_insert(*fs.inode_cache, ino);
    mark_dirty(fs, *ino);
    flush_inodes(fs, false);
    return 0;
}

int symlink(fs_context& fs, const char* oldpath, const char* newpath, uid_t uid, gid_t gid)
{
#ifdef _WIN32
    mode_t mode = S_IFLNK; // FIXME
#else
    mode_t mode = S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO;
#endif
    int rc = create(fs, newpath, mode, uid, gid, 0);
    if (rc < 0)
        return rc;

    inode* ino;
    rc = lookup(fs, &ino, newpath);
    if (rc < 0)
        return rc;

    rc = write(fs, ino, oldpath, strlen(oldpath), 0);
    if (rc < 0)
        unlink(fs, newpath); // Remove empty file

    flush_inodes(fs, false);
    return (rc < 0) ? rc : 0;
}

int readlink(fs_context& fs, const char* path, char* buf, size_t bufsize)
{
    inode* ino;
    int rc = lookup(fs, &ino, path);
    if (rc < 0)
        return rc;

    rc = read(fs, ino, buf, bufsize - 1, 0);
    if (rc < 0)
        return rc;

    buf[rc] = '\0';
    return 0;
}

int link(fs_context& fs, const char* oldpath, const char* newpath)
{
    // We need the inode_no of the existing path.
    inode* ino;
    int rc = lookup(fs, &ino, oldpath);
    if (rc < 0)
        return rc;

    unsigned int inode_no = get_be32(ino->i_no);
    mode_t mode = get_be32(ino->i_mode);

    rc = add_dentry(fs, newpath, inode_no, mode, nullptr);
    if (rc < 0)
        return rc;

    inc_be32(ino->i_nlink);
    flush_inodes(fs, false);
    return 0;
}

int unlink(fs_context& fs, const char* path)
{
    // We need the inode_no of the existing path so that we can find the
    // corresponding dentry inside its parent data.
    inode* ino;
    int rc = lookup(fs, &ino, path);
    if (rc < 0)
        return rc;

    ino_t ino_no = get_be32(ino->i_no);
    mode_t mode = get_be32(ino->i_mode);

    rc = remove_dentry(fs, path, ino_no, mode);
    if (rc < 0)
        return rc;

    if (get_be32(ino->i_nlink) > 1)
    {
        // The inode is referenced by more than one dentry.
        // Do not delete/invalidate it but only reduce its link count.
        dec_be32(ino->i_nlink);
        mark_dirty(fs, *ino);
    }
    else if (get_be32(ino->i_nlink) == 1)
    {
        /* We just removed the only reference to this inode.
         * It can now be removed from the file system. */

        /* decrement the number of valid inodes inside the old inode's
         * cluster (in case it really had one). */
        cl_id_t cl_id = get_be32(fs.ino_map[ino_no]);
        if (cl_id != FFSP_RESERVED_CL_ID)
        {
            fs.cl_occupancy[cl_id]--;

            /* also decrement the number of valid inode clusters
             * inside the affected erase block in case the cluster
             * does not contain any more valid inodes at all. */
            if (!fs.cl_occupancy[cl_id])
            {
                eb_id_t eb_id = cl_id * fs.clustersize / fs.erasesize;
                eb_dec_cvalid(fs, eb_id);
            }
        }

        /* set the old file's inode number to 'free' */
        fs.ino_map[ino_no] = put_be32(FFSP_FREE_CL_ID);

        uint64_t file_size = get_be64(ino->i_size);
        inode_data_type data_type = static_cast<inode_data_type>(get_be32(ino->i_flags) & 0xff);

        // Release indirect data if needed.
        if (data_type != inode_data_type::emb && file_size)
        {
            int ind_size;
            inode_data_type ind_type;
            if (data_type == inode_data_type::clin)
            {
                ind_type = inode_data_type::clin;
                ind_size = fs.clustersize;
            }
            else if (data_type == inode_data_type::ebin)
            {
                ind_type = inode_data_type::ebin;
                ind_size = fs.erasesize;
            }
            else
            {
                log().error("ffsp_unlink(): Invalid inode flags");
                return -1;
            }
            int ind_cnt = ((file_size - 1) / ind_size) + 1;
            be32_t* ind_ptr = (be32_t*)inode_data(ino);
            invalidate_ind_ptr(fs, ind_ptr, ind_cnt, ind_type);
        }
        inode_cache_remove(*fs.inode_cache, ino);
        reset_dirty(fs, *ino);
        delete_inode(ino);
    }
    else
    {
        log().error("ffsp_unlink(): Invalid inode link count");
        return -1;
    }
    flush_inodes(fs, false);
    return 0;
}

int rmdir(fs_context& fs, const char* path)
{
    // We need the inode_no of the existing path so that we can find the
    //  corresponding dentry inside its parent data.
    inode* ino;
    int rc = lookup(fs, &ino, path);
    if (rc < 0)
        return rc;

    if (dentry_is_empty(fs, ino) == 0)
        return -ENOTEMPTY;

    ino_t ino_no = get_be32(ino->i_no);
    mode_t mode = get_be32(ino->i_mode);

    // Invalidate the dentry inside the parent directory and also
    //  decrement the link count of the parent directory.
    rc = remove_dentry(fs, path, ino_no, mode);
    if (rc < 0)
        return rc;

    // From this point on "path" cannot be found inside any directory
    //  but its inode and potential data clusters/erase blocks still occupy
    //  memory on the drive.

    /* decrement the number of valid inodes inside the old inode's
     * cluster (in case it really had one). */
    cl_id_t cl_id = get_be32(fs.ino_map[ino_no]);
    if (cl_id != FFSP_RESERVED_CL_ID)
    {
        fs.cl_occupancy[cl_id]--;

        /* also decrement the number of valid inode clusters
         * inside the affected erase block in case the cluster
         * does not contain any more valid inodes at all. */
        if (!fs.cl_occupancy[cl_id])
        {
            eb_id_t eb_id = cl_id * fs.clustersize / fs.erasesize;
            eb_dec_cvalid(fs, eb_id);
        }
    }

    /* set the old file's inode number to 'free' */
    fs.ino_map[ino_no] = put_be32(FFSP_FREE_CL_ID);

    uint64_t file_size = get_be64(ino->i_size);
    inode_data_type data_type = static_cast<inode_data_type>(get_be32(ino->i_flags) & 0xff);

    // Release indirect data if needed.
    if (data_type != inode_data_type::emb)
    {
        int ind_size;
        inode_data_type ind_type;
        if (data_type == inode_data_type::clin)
        {
            ind_type = inode_data_type::clin;
            ind_size = fs.clustersize;
        }
        else if (data_type == inode_data_type::ebin)
        {
            ind_type = inode_data_type::ebin;
            ind_size = fs.erasesize;
        }
        else
        {
            log().error("ffsp_rmdir(): Invalid inode flags");
            return -1;
        }
        int ind_cnt = ((file_size - 1) / ind_size) + 1;
        be32_t* ind_ptr = (be32_t*)inode_data(ino);
        invalidate_ind_ptr(fs, ind_ptr, ind_cnt, ind_type);
    }
    inode_cache_remove(*fs.inode_cache, ino);
    reset_dirty(fs, *ino);
    delete_inode(ino);
    flush_inodes(fs, false);
    return 0;
}

int rename(fs_context& fs, const char* oldpath, const char* newpath)
{
    (void)fs;
    (void)oldpath;
    (void)newpath;

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

    flush_inodes(fs, false);
    return -1;
}

void mark_dirty(fs_context& fs, const inode& ino)
{
    if (is_inode_dirty(fs, ino))
        /* the inode is already dirty */
        return;

    ino_t ino_no = get_be32(ino.i_no);
    set_bit(fs.ino_status_map, ino_no);
    fs.dirty_ino_cnt++;

    log().debug("inode {} is now dirty - dirty_ino_cnt={}", ino_no, fs.dirty_ino_cnt);

    /* decrement the number of valid inodes inside the old inode's
     * cluster (in case it really had one). */
    cl_id_t cl_id = get_be32(fs.ino_map[ino_no]);
    if (cl_id != FFSP_RESERVED_CL_ID)
    {
        fs.cl_occupancy[cl_id]--;

        /* also decrement the number of valid inode clusters
         * inside the affected erase block in case the cluster does
         * not contain any more valid inodes at all. */
        if (!fs.cl_occupancy[cl_id])
        {
            eb_id_t eb_id = cl_id * fs.clustersize / fs.erasesize;
            eb_dec_cvalid(fs, eb_id);
        }
    }
}

void reset_dirty(fs_context& fs, const inode& ino)
{
    if (is_inode_dirty(fs, ino))
    {
        ino_t ino_no = get_be32(ino.i_no);
        clear_bit(fs.ino_status_map, ino_no);
        fs.dirty_ino_cnt--;
        log().debug("inode {} is now CLEAN - dirty_ino_cnt={}", ino_no, fs.dirty_ino_cnt);
    }
}

int cache_dir(fs_context& fs, inode* ino, dentry** dent_buf, int* dent_cnt)
{
    // Number of bytes till the end of the last valid dentry.
    uint64_t data_size = get_be64(ino->i_size);

    *dent_buf = (dentry*)malloc(data_size);
    if (!*dent_buf)
    {
        log().critical("ffsp_cache_dir(): malloc() failed.");
        return -1;
    }

    ssize_t rc = read(fs, ino, (char*)(*dent_buf), data_size, 0);
    if (rc < 0)
    {
        free(*dent_buf);
        return rc;
    }
    // Potential amount of valid ffsp_dentry elements.
    *dent_cnt = data_size / sizeof(dentry);
    return 0;
}

void invalidate_ind_ptr(fs_context& fs, const be32_t* ind_ptr, int cnt, inode_data_type ind_type)
{
    for (int i = 0; i < cnt; ++i)
    {
        uint32_t ind_id = get_be32(ind_ptr[i]);

        if (!ind_id)
            continue; // File hole, not a real indirect pointer.

        if (ind_type == inode_data_type::clin)
        {
            // Tell the erase block usage information that there is now
            // one additional cluster invalid in the specified erase block.
            cl_id_t cl_id = ind_id;
            eb_id_t eb_id = cl_id * fs.clustersize / fs.erasesize;
            eb_dec_cvalid(fs, eb_id);
            //	dec_be16(&fs.eb_usage[eb_id].e_cvalid);
        }
        else if (ind_type == inode_data_type::ebin)
        {
            // The erase block type is the only field of importance in this case.
            // Set the erase blocks usage information to EMPTY and we are done.
            eb_id_t eb_id = ind_id;
            fs.eb_usage[eb_id].e_type = eraseblock_type::empty;
        }
    }
}

} // namespace ffsp
