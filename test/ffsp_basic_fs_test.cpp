#include "gtest/gtest.h"

#include "ffsp_test_utils.hpp"

class BasicFileSystemOperationsTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(ffsp_testing::default_create_file());
        ASSERT_TRUE(ffsp_testing::default_mkfs_ffsp());
        ASSERT_TRUE(ffsp_testing::default_mount_ffsp());
    }

    void TearDown() override
    {
        ASSERT_TRUE(ffsp_testing::default_unmount_ffsp());
        ASSERT_TRUE(ffsp_testing::default_remove_file());
    }
};

TEST_F(BasicFileSystemOperationsTest, SmallFiles)
{
    std::string command;

//    command = std::string("cat ") + ffsp_testing::default_dir_mountpoint + "/.FFSP";
//    ASSERT_TRUE(std::system(command.c_str()) == 0);

    command = std::string("cp /etc/lsb-release ") + ffsp_testing::default_dir_mountpoint;
    ASSERT_TRUE(std::system(command.c_str()) == 0);

//    command = std::string("cat ") + ffsp_testing::default_dir_mountpoint + "/.FFSP";
//    ASSERT_TRUE(std::system(command.c_str()) == 0);
}
