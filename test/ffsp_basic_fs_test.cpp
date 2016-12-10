#include "gtest/gtest.h"

#include "ffsp_test_utils.hpp"

class BasicFileSystemOperationsTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(ffsp::test::default_create_file());
        ASSERT_TRUE(ffsp::test::default_mkfs_ffsp());
        ASSERT_TRUE(ffsp::test::default_mount_ffsp());
    }

    void TearDown() override
    {
        ASSERT_TRUE(ffsp::test::default_unmount_ffsp());
        ASSERT_TRUE(ffsp::test::default_remove_file());
    }
};

TEST_F(BasicFileSystemOperationsTest, SmallFiles)
{
    std::string command;

    ////    command = std::string("cat ") + ffsp_testing::default_dir_mountpoint + "/.FFSP";
    ////    ASSERT_TRUE(std::system(command.c_str()) == 0);

    command = std::string("cp /etc/lsb-release ") + ffsp::test::default_dir_mountpoint;
    ASSERT_TRUE(std::system(command.c_str()) == 0);

    command = std::string("mkdir ") + ffsp::test::default_dir_mountpoint + "/test.d_0";
    ASSERT_TRUE(std::system(command.c_str()) == 0);

    command = std::string("cp ") + ffsp::test::default_dir_mountpoint + "/lsb-release " + ffsp::test::default_dir_mountpoint + "/test.d_0";
    ASSERT_TRUE(std::system(command.c_str()) == 0);

    command = std::string("tree ") + ffsp::test::default_dir_mountpoint;
    ASSERT_TRUE(std::system(command.c_str()) == 0);

    ////    command = std::string("cat ") + ffsp_testing::default_dir_mountpoint + "/.FFSP";
    ////    ASSERT_TRUE(std::system(command.c_str()) == 0);
}

TEST_F(BasicFileSystemOperationsTest, ManySmallFilesInRoot)
{
    std::string command;

    const std::string mnt{ffsp::test::default_dir_mountpoint};

    const int files{512};
    const std::string chunk{"\\xde\\xad\\xbe\\xef"};

    std::string content;
    content.reserve(chunk.size() * files);

    for (int i = 0; i < files; i++)
    {
        content.clear();
        const auto fpath{mnt + "/" + std::to_string(i)};
        const auto fsize = i;

        for (auto byte_cnt = 0; byte_cnt < fsize; byte_cnt++)
            content += chunk;

        command = "echo -n -e '" + content + "' >> " + fpath;
        ASSERT_TRUE(std::system(command.c_str()) == 0);
    }
}
