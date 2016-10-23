#include "gtest/gtest.h"

#include "libffsp/debug.hpp"
#include "libffsp/log.hpp"
#include "libffsp/io_raw.hpp"
#include "libffsp/mkfs.hpp"
#include "libffsp/mount.hpp"

#include "ffsp_test_utils.hpp"

#include "fuse_ffsp.hpp"

class BasicFileSystemOperationsApiTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ffsp_log_init("ffsp_test", spdlog::level::debug);

        ASSERT_TRUE(ffsp_testing::default_create_file());
        ASSERT_TRUE(ffsp_testing::default_make_fs());
        ASSERT_TRUE(ffsp_testing::default_mount_fs(fs_));
    }

    void TearDown() override
    {
        ASSERT_TRUE(ffsp_testing::default_unmount_fs(fs_));
        ASSERT_TRUE(ffsp_testing::default_remove_file());

        ffsp_log_deinit();
    }

    ffsp_fs fs_;
};

//TEST_F(BasicFileSystemOperationsTest, SmallFile)
//{
//    const char* const file_path = "/SmallFileTest";
//    fuse_file_info fi = {};

//    const mode_t mode = S_IFREG;
//    const dev_t device = 0;
//    ASSERT_EQ(0, fuse_ffsp::mknod(fs_, file_path, mode, device));

//    const std::vector<char> write_buf(4096, '#');
//    const size_t write_offset = 0;
//    ASSERT_EQ(0, fuse_ffsp::open(fs_, file_path, &fi));
//    ASSERT_EQ(4096, fuse_ffsp::write(fs_, file_path, write_buf.data(), write_buf.size(), write_offset, &fi));
//    ASSERT_EQ(0, fuse_ffsp::release(fs_, file_path, &fi));

//    std::vector<char> read_buf(4096);
//    const size_t read_offset = 0;
//    ASSERT_EQ(0, fuse_ffsp::open(fs_, file_path, &fi));
//    ASSERT_EQ(4096, fuse_ffsp::read(fs_, file_path, read_buf.data(), read_buf.size(), read_offset, &fi));
//    ASSERT_EQ(0, fuse_ffsp::release(fs_, file_path, &fi));
//}

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
