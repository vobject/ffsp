#include "ffsp_test_utils.hpp"

#include "libffsp/ffsp.hpp"
#include "libffsp/mkfs.hpp"
#include "libffsp/mount.hpp"
#include "libffsp/io_raw.hpp"
#include "libffsp-fuse/fuse_ffsp.hpp"

#include "fuse.h"

#include <string>
#include <experimental/filesystem>

#include <cstdio>
#include <cstdlib>
#include <sys/types.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace ffsp
{

namespace test
{

io_context* default_io_ctx{ nullptr };

bool create_file(const char* file_path, uint64_t file_size)
{
    FILE* fp = ::fopen(file_path, "w");
    return fp &&
#ifdef _WIN32
        (::chsize(fileno(fp), static_cast<long>(file_size)) == 0) &&
#else
        (::ftruncate(fileno(fp), static_cast<off_t>(file_size)) == 0) &&
#endif
        (::fclose(fp) == 0);
}

bool remove_file(const char* file_path)
{
    return (::remove(file_path) == 0);
}

bool make_fs(io_context* io_ctx, const mkfs_options& opts)
{
    return io_ctx && ffsp::mkfs(*io_ctx, opts);
}

bool mount_fs(io_context* io_ctx, fs_context** fs)
{
    return io_ctx && ((*fs = ffsp::mount(io_ctx)) != nullptr);
}

bool unmount_fs(fs_context* fs)
{
    auto* io_ctx = ffsp::unmount(fs);
    return io_ctx != nullptr;
}

bool mkfs_ffsp(const char* program,
               uint32_t clustersize, uint32_t erasesize, uint32_t ninoopen,
               uint32_t neraseopen, uint32_t nerasereserve, uint32_t nerasewrites,
               const char* device)
{
    const std::string command = std::string(program)
                              + " -c " + std::to_string(clustersize)
                              + " -e " + std::to_string(erasesize)
                              + " -i " + std::to_string(ninoopen)
                              + " -o " + std::to_string(neraseopen)
                              + " -r " + std::to_string(nerasereserve)
                              + " -w " + std::to_string(nerasewrites)
                              + " " + device;
    return std::system(command.c_str()) == 0;
}

bool mount_ffsp(const char* program, const char* device, const char* mountpoint)
{
    const std::string command = std::string(program)
                              + " --logfile=ffsp_fstest.log"
                              + " -vvvv"
                              + " " + device
                              + " " + mountpoint;
    return std::system(command.c_str()) == 0;
}

bool unmount_ffsp(const char* program, const char* mountpoint)
{
    const std::string command = std::string(program)
                              + " " + mountpoint;
    return std::system(command.c_str()) == 0;
}

bool default_create_file()
{
    return create_file(default_fs_path, default_fs_size);
}

bool default_remove_file()
{
    return remove_file(default_fs_path);
}

bool default_open_io_backend(bool in_memory)
{
    if (in_memory)
    {
        default_io_ctx = io_context_init(default_fs_size);
    }
    else
    {
        default_io_ctx = create_file(default_fs_path, default_fs_size)
                            ? io_context_init(default_fs_path)
                            : nullptr;
    }
    return default_io_ctx != nullptr;
}

bool default_close_io_backend()
{
    io_context_uninit(default_io_ctx);
    return os::exists(default_fs_path) ? remove_file(default_fs_path) : true;
}

bool default_make_fs()
{
    return make_fs(default_io_ctx, default_mkfs_options);
}

bool default_mount_fs(fs_context** fs)
{
    return mount_fs(default_io_ctx, fs);
}

bool default_unmount_fs(fs_context* fs)
{
    return unmount_fs(fs);
}

bool default_mkfs_ffsp()
{
    return mkfs_ffsp(default_bin_mkfs,
                     default_mkfs_options.clustersize,
                     default_mkfs_options.erasesize,
                     default_mkfs_options.ninoopen,
                     default_mkfs_options.neraseopen,
                     default_mkfs_options.nerasereserve,
                     default_mkfs_options.nerasewrites,
                     default_fs_path);
}

bool default_mount_ffsp()
{
    if (!os::exists(default_dir_mountpoint))
        os::mkdir(default_dir_mountpoint);

    return mount_ffsp(default_bin_mount, default_fs_path, default_dir_mountpoint);
}

bool default_unmount_ffsp()
{
    return unmount_ffsp(default_bin_unmount, default_dir_mountpoint);
}

namespace os
{

bool exists(const char* path)
{
    namespace fs = std::experimental::filesystem;
    return fs::exists(path);
}

bool mkdir(const char* dir_path)
{
    namespace fs = std::experimental::filesystem;
    return fs::create_directory(dir_path);
}

} // namespace os

} // namespace test

} // namespace ffsp
