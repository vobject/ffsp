#ifndef FFSP_TEST_UTILS_HPP
#define FFSP_TEST_UTILS_HPP

#include "libffsp/mkfs.hpp"

namespace ffsp
{

struct io_backend;

namespace test
{

bool create_file(const char* file_path, uint64_t file_size);
bool remove_file(const char* file_path);

bool make_fs(io_backend* io_ctx, const mkfs_options& opts);

bool mount_fs(fs_context** fs, const char* file_path);
bool unmount_fs(fs_context* fs);

bool mkfs_ffsp(const char* program,
               uint32_t clustersize, uint32_t erasesize, uint32_t ninoopen,
               uint32_t neraseopen, uint32_t nerasereserve, uint32_t nerasewrites,
               const char* device);
bool mount_ffsp(const char* program, const char* device, const char* mountpoint);
bool unmount_ffsp(const char* program, const char* mountpoint);

/*
    Max number of inodes:

    number of erase blocks = file system size / erase block size
    number of indes = ((erase block size)
                       - (size of cluster)
                       - (number of erase blocks * size of erase block struct)
                       - (size of (root) inode id))
                      / (size of inode id)

    Default file system parameters for small files:
    - file system size = 128 MiB
    - erase block size = 4 MiB
    - cluster size = 32 KiB
    - number of erase blocks = 32
    - number of inodes = (4194304 - 32768 - 32*8 - 4) / 4 = 1040319
*/
extern io_backend* default_io_ctx;
#ifdef _WIN32
const constexpr char* const default_fs_path{ "test.ffsp_fs" };
#else
const constexpr char* const default_fs_path{ "/tmp/test.ffsp_fs" };
#endif
const constexpr uint64_t default_fs_size{ 1024 * 1024 * 128 };      // 128 MiB
const constexpr mkfs_options default_mkfs_options{ 1024 * 32,       // cluster
                                                   1024 * 1024 * 4, // eraseblock
                                                   128,             // open inodes
                                                   5,               // open eraseblocks
                                                   3,               // reserved eraseblocks
                                                   5 };             // gc trigger

const constexpr char* const default_bin_mkfs{ "./mkfs.ffsp" };
const constexpr char* const default_bin_mount{ "./mount.ffsp" };
const constexpr char* const default_bin_unmount{ "fusermount -u" };
const constexpr char* const default_dir_mountpoint{ "mnt" };

bool default_create_file();
bool default_remove_file();

bool default_open_io_backend(bool in_memory);
bool default_close_io_backend();

bool default_make_fs();

bool default_mount_fs(fs_context** fs);
bool default_unmount_fs(fs_context* fs);

bool default_mkfs_ffsp();
bool default_mount_ffsp();
bool default_unmount_ffsp();

namespace os
{

bool exists(const char* path);
bool mkdir(const char* dir_path);

} // namespace os

} // namespace test

} // namespace ffsp

#endif // FFSP_TEST_UTILS_HPP
