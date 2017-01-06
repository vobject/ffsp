#include "gtest/gtest.h"

#include "libffsp/debug.hpp"
#include "libffsp/io_raw.hpp"
#include "libffsp/log.hpp"
#include "libffsp/mkfs.hpp"
#include "libffsp/mount.hpp"
#include "libffsp-fuse/fuse_ffsp.hpp"

#include "ffsp_test_utils.hpp"

class BasicFileSystemOperationsApiTest : public testing::Test
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

        ffsp::log_deinit();
    }

    ffsp::fs_context* fs_{ nullptr };
};

TEST_F(BasicFileSystemOperationsApiTest, SmallFile)
{
    const char* const file_path = "/SmallFileTest";
    fuse_file_info fi = {};

    const mode_t mode = S_IFREG;
    const dev_t device = 0;
    ASSERT_EQ(0, ffsp::fuse::mknod(*fs_, file_path, mode, device));

    const std::vector<char> write_buf(4096, '#');
    const size_t write_offset = 0;
    ASSERT_EQ(0, ffsp::fuse::open(*fs_, file_path, &fi));
    ASSERT_EQ(4096, ffsp::fuse::write(*fs_, file_path, write_buf.data(), write_buf.size(), write_offset, &fi));
    ASSERT_EQ(0, ffsp::fuse::release(*fs_, file_path, &fi));

    std::vector<char> read_buf(4096);
    const size_t read_offset = 0;
    ASSERT_EQ(0, ffsp::fuse::open(*fs_, file_path, &fi));
    ASSERT_EQ(4096, ffsp::fuse::read(*fs_, file_path, read_buf.data(), read_buf.size(), read_offset, &fi));
    ASSERT_EQ(0, ffsp::fuse::release(*fs_, file_path, &fi));
}

TEST_F(BasicFileSystemOperationsApiTest, SmallFiles)
{
    //    const std::string rootDir{"/"};

    //    for (int i = 1;;i++)
    //    {
    //        const std::string testFile{rootDir + std::to_string(i)};
    //        fuse_file_info fi = {};

    //        const mode_t mode = S_IFREG;
    //        const dev_t device = 0;
    //        ASSERT_EQ(0, fuse_ffsp::mknod(fs_, testFile.c_str(), mode, device));

    //        const size_t write_offset = 0;
    //        const std::vector<char> write_buf(4096, '#');
    //        ASSERT_EQ(0, fuse_ffsp::open(fs_, testFile.c_str(), &fi));
    //        ASSERT_EQ(4096, fuse_ffsp::write(fs_, testFile.c_str(), write_buf.data(), write_buf.size(), write_offset, &fi));
    //        ASSERT_EQ(0, fuse_ffsp::release(fs_, testFile.c_str(), &fi));

    //        std::vector<char> read_buf(4096);
    //        const size_t read_offset = 0;
    //        ASSERT_EQ(0, fuse_ffsp::open(fs_, testFile.c_str(), &fi));
    //        ASSERT_EQ(4096, fuse_ffsp::read(fs_, testFile.c_str(), read_buf.data(), read_buf.size(), read_offset, &fi));
    //        ASSERT_EQ(0, fuse_ffsp::release(fs_, testFile.c_str(), &fi));
    //    }

    //    ffsp_log().info(ffsp_debug_get_info(fs_));
}
