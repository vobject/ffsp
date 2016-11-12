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

    //    command = std::string("cat ") + ffsp_testing::default_dir_mountpoint + "/.FFSP";
    //    ASSERT_TRUE(std::system(command.c_str()) == 0);

    command = std::string("cp /etc/lsb-release ") + ffsp::test::default_dir_mountpoint;
    ASSERT_TRUE(std::system(command.c_str()) == 0);

    //    command = std::string("cat ") + ffsp_testing::default_dir_mountpoint + "/.FFSP";
    //    ASSERT_TRUE(std::system(command.c_str()) == 0);
}

TEST_F(BasicFileSystemOperationsTest, SmallFiles2)
{
    //    std::string command;

    //    command = std::string("cat /dev/urandom > ") + ffsp::test::default_dir_mountpoint + "/urandom";
    //    ASSERT_TRUE(std::system(command.c_str()) == 0);

    //    command = std::string("cp /etc/lsb-release ") + ffsp::test::default_dir_mountpoint;
    //    ASSERT_TRUE(std::system(command.c_str()) == 0);

    //    command = std::string("cat ") + ffsp_testing::default_dir_mountpoint + "/.FFSP";
    //    ASSERT_TRUE(std::system(command.c_str()) == 0);
}
