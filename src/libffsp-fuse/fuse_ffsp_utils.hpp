#ifndef FUSE_FFSP_UTILS_HPP
#define FUSE_FFSP_UTILS_HPP

#include <ctime>

struct stat;
struct statvfs;

namespace ffsp
{

struct fs_context;
struct inode;

namespace fuse
{

void stat(fs_context& fs, const inode& ino, struct ::stat& stbuf);
void statfs(fs_context& fs, struct ::statvfs& sfs);
void utimens(fs_context& fs, inode& ino, const struct ::timespec tvi[2]);

} // namespace fuse

} // namespace ffsp

#endif /* FUSE_FFSP_UTILS_HPP */
