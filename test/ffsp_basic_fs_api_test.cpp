#include "gtest/gtest.h"

#include "libffsp/debug.hpp"
#include "libffsp/io_backend.hpp"
#include "libffsp/io_raw.hpp"
#include "libffsp/log.hpp"
#include "libffsp/mkfs.hpp"
#include "libffsp/mount.hpp"
#include "libffsp-fuse/fuse_ffsp.hpp"

#include "ffsp_test_utils.hpp"

#include <cmath>
#include <cstring>

class SingleMountFileSystemOperationsApiTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ffsp::log_init("ffsp_test", spdlog::level::debug);

        ASSERT_TRUE(ffsp::test::default_open_io_backend(true));
        ASSERT_TRUE(ffsp::test::default_make_fs());
        ASSERT_TRUE(ffsp::test::default_mount_fs(&fs_));
    }

    void TearDown() override
    {
        ASSERT_TRUE(ffsp::test::default_unmount_fs(fs_));
        ASSERT_TRUE(ffsp::test::default_close_io_backend());

        ffsp::log_uninit();
    }

    ffsp::fs_context* fs_{ nullptr };
};

TEST_F(SingleMountFileSystemOperationsApiTest, FilesReadWrite)
{
    // iterate file sizes 0 through 16 MiB
    for (auto pow = 0; pow <= 24; pow++)
    {
        const uint64_t size = std::pow(2, pow);
        const auto path = "/file_" + std::to_string(size);

        fuse_file_info fi = {};
        const auto& write_buf = ffsp::test::file_content(size);
        std::vector<char> read_buf(size);

        ASSERT_EQ(0, ffsp::fuse::mknod(*fs_, path.c_str(), S_IFREG, 0));

        ASSERT_EQ(0, ffsp::fuse::open(*fs_, path.c_str(), &fi));
        ASSERT_EQ(int(size), ffsp::fuse::write(*fs_, path.c_str(), (const char*)write_buf.data(), write_buf.size(), 0, &fi));
        ASSERT_EQ(0, ffsp::fuse::release(*fs_, path.c_str(), &fi));

        ASSERT_EQ(0, ffsp::fuse::open(*fs_, path.c_str(), &fi));
        ASSERT_EQ(int(size), ffsp::fuse::read(*fs_, path.c_str(), read_buf.data(), size, 0, &fi));
        ASSERT_EQ(0, ffsp::fuse::release(*fs_, path.c_str(), &fi));

        ASSERT_EQ(0, std::memcmp(write_buf.data(), read_buf.data(), size));
    }
}

TEST_F(SingleMountFileSystemOperationsApiTest, GrowFile)
{
    fuse_file_info fi = {};
    const auto path = "/file_growing";

    const uint64_t size = std::pow(2, 23); // 8MiB
    const uint64_t step = std::pow(2, 12); // 4KiB

    ASSERT_EQ(0, ffsp::fuse::mknod(*fs_, path, S_IFREG, 0));
    ASSERT_EQ(0, ffsp::fuse::open(*fs_, path, &fi));

    for (auto offset = 0u; offset < size; offset += step)
    {
        const auto& write_buf = ffsp::test::file_content(step);
        std::vector<char> read_buf(step);

        ASSERT_EQ(int(step), ffsp::fuse::write(*fs_, path, (const char*)write_buf.data(), step, offset, &fi));
        ASSERT_EQ(int(step), ffsp::fuse::read(*fs_, path, read_buf.data(), step, offset, &fi));

        ASSERT_EQ(0, std::memcmp(write_buf.data(), read_buf.data(), step));
    }
    ASSERT_EQ(0, ffsp::fuse::release(*fs_, path, &fi));
}

class MultiMountFileSystemOperationsApiTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ffsp::log_init("ffsp_test", spdlog::level::debug);
        io_ = ffsp::io_backend_init(ffsp::test::default_fs_size);

        ASSERT_TRUE(ffsp::test::make_fs(io_, ffsp::test::default_mkfs_options));
    }

    void TearDown() override
    {
        ffsp::io_backend_uninit(io_);
        ffsp::log_uninit();
    }

    ffsp::io_backend* io_{ nullptr };
    ffsp::fs_context* fs_{ nullptr };
};

TEST_F(MultiMountFileSystemOperationsApiTest, FilesReadWrite)
{
    ASSERT_TRUE(ffsp::test::mount_fs(io_, &fs_));
    for (auto pow = 0; pow <= 24; pow++)
    {
        // write files sizes 0 through 16 MiB
        const uint64_t size = std::pow(2, pow);
        const auto path = "/file_" + std::to_string(size);

        fuse_file_info fi = {};
        const auto& write_buf = ffsp::test::file_content(size);

        ASSERT_EQ(0, ffsp::fuse::mknod(*fs_, path.c_str(), S_IFREG, 0));

        ASSERT_EQ(0, ffsp::fuse::open(*fs_, path.c_str(), &fi));
        ASSERT_EQ(int(size), ffsp::fuse::write(*fs_, path.c_str(), (const char*)write_buf.data(), write_buf.size(), 0, &fi));
        ASSERT_EQ(0, ffsp::fuse::release(*fs_, path.c_str(), &fi));
    }
    ASSERT_TRUE(ffsp::test::unmount_fs(fs_));

    ASSERT_TRUE(ffsp::test::mount_fs(io_, &fs_));
    for (auto pow = 0; pow <= 24; pow++)
    {
        // read files sizes 0 through 16 MiB
        const uint64_t size = std::pow(2, pow);
        const auto path = "/file_" + std::to_string(size);

        fuse_file_info fi = {};
        const auto& expected_buf = ffsp::test::file_content(size);
        std::vector<char> read_buf(size);

        ASSERT_EQ(0, ffsp::fuse::open(*fs_, path.c_str(), &fi));
        ASSERT_EQ(int(size), ffsp::fuse::read(*fs_, path.c_str(), read_buf.data(), size, 0, &fi));
        ASSERT_EQ(0, ffsp::fuse::release(*fs_, path.c_str(), &fi));

        ASSERT_EQ(0, std::memcmp(expected_buf.data(), read_buf.data(), size));
    }
    ASSERT_TRUE(ffsp::test::unmount_fs(fs_));
}
