#include "fuse_ffsp_utils.hpp"

#include "libffsp/ffsp.hpp"
#include "libffsp/eraseblk.hpp"
#include "libffsp/inode.hpp"

#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#ifndef _WIN32
#include <sys/statvfs.h>
#include <sys/time.h>
#endif

namespace ffsp
{

namespace fuse
{

static uint64_t fs_size(const fs_context& fs)
{
    /* do not count the first erase block */
    return (fs.neraseblocks - 1) * fs.erasesize;
}

static uint32_t free_cluster_cnt(const fs_context& fs)
{
    uint32_t free_cl_cnt = 0; /* atm clusters and blocks are the same. */

    for (eb_id_t eb_id = 1; eb_id < fs.neraseblocks; eb_id++)
    {
        if (eb_is_type(fs, eb_id, eraseblock_type::ebin))
            continue;

        if (eb_is_type(fs, eb_id, eraseblock_type::empty))
            free_cl_cnt += (fs.erasesize / fs.clustersize);
        else
            free_cl_cnt += (fs.erasesize / fs.clustersize) - get_be16(fs.eb_usage[eb_id].e_cvalid);
    }
    return free_cl_cnt;
}

static uint32_t inode_cnt(const fs_context& fs)
{
    uint32_t free_ino_cnt = 0;

    for (uint32_t ino_no = 1; ino_no < fs.nino; ino_no++)
        if (get_be32(fs.ino_map[ino_no]) == FFSP_FREE_CL_ID)
            free_ino_cnt++;
    return fs.nino - free_ino_cnt;
}

void stat(fs_context& fs, const inode& ino, struct ::stat& stbuf)
{
    (void)fs;

    memset(&stbuf, 0, sizeof(stbuf));
    stbuf.st_dev = 0; // FIXME
    stbuf.st_ino = get_be32(ino.i_no);
    stbuf.st_mode = get_be32(ino.i_mode);
    stbuf.st_nlink = get_be32(ino.i_nlink);
    stbuf.st_uid = get_be32(ino.i_uid);
    stbuf.st_gid = get_be32(ino.i_gid);
    stbuf.st_rdev = get_be64(ino.i_rdev);
    stbuf.st_size = get_be64(ino.i_size);
    stbuf.st_atime = get_be64(ino.i_atime.sec);
    stbuf.st_mtime = get_be64(ino.i_mtime.sec);
    stbuf.st_ctime = get_be64(ino.i_ctime.sec);
#ifndef _WIN32
    stbuf.st_blksize = 0; /* ignored by FUSE */
    stbuf.st_blocks = (get_be64(ino.i_size) + 511) / 512 + 1;
#endif
}

void statfs(fs_context& fs, struct ::statvfs& sfs)
{
    memset(&sfs, 0, sizeof(sfs));
    sfs.f_bsize = fs.blocksize;
    sfs.f_blocks = fs_size(fs) / fs.blocksize;
    sfs.f_bfree = free_cluster_cnt(fs);
    sfs.f_bavail = sfs.f_bfree;
    sfs.f_files = inode_cnt(fs);
    sfs.f_ffree = fs.nino - sfs.f_files;
    sfs.f_namemax = FFSP_NAME_MAX;
}

void utimens(fs_context& fs, inode& ino, const struct ::timespec tv[2])
{
    ino.i_atime.sec = put_be64(tv[0].tv_sec);
    ino.i_atime.nsec = put_be32(tv[0].tv_nsec);
    ino.i_mtime.sec = put_be64(tv[1].tv_sec);
    ino.i_mtime.nsec = put_be32(tv[1].tv_nsec);
    mark_dirty(fs, ino);
    flush_inodes(fs, false);
}

} // namespace fuse

} // namespace ffsp
