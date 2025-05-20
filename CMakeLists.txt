cmake_minimum_required(VERSION 3.20)

project(ffsp LANGUAGES CXX)

option(FFSP_BUILD_LIB "Build file system library." ON)
option(FFSP_BUILD_FUSELIB "Build fuse operations wrapper." ON)
option(FFSP_BUILD_MKFS "Build file system creation tool." ON)
option(FFSP_BUILD_MOUNT "Build file system mount tool." ON)
option(FFSP_BUILD_TESTS "Build file system tests." ON)

include(FetchContent)

# Fetch spdlog
set(SPDLOG_BUILD_PIC ON CACHE BOOL "Build spdlog with -fPIC" FORCE)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.3
)
FetchContent_MakeAvailable(spdlog)

if(WIN32)
    set(FUSE_INCLUDE_DIRS ${DOKAN_INCLUDE_DIR} ${DOKAN_INCLUDE_DIR}/fuse)
    set(FUSE_LIBRARIES ${DOKAN_LIB_DIR}/dokanfuse1)

    add_definitions(-DFUSE_USE_VERSION=26 -D_WIN32_WINNT=0x0601)
    add_definitions(-DNOMINMAX -D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS /W4)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
else()
    set(FUSE_INCLUDE_DIRS "/usr/include/fuse")
    set(FUSE_LIBRARIES "fuse" "pthread")

    add_definitions(-DFUSE_USE_VERSION=26 -D_FILE_OFFSET_BITS=64)
    add_definitions(-Wall -Wextra -pedantic)
endif()


if(FFSP_BUILD_LIB)
    set(FFSP_LIBRARY "ffsp")

    set(FFSP_SRC
        src/libffsp/bitops.hpp
        src/libffsp/byteorder.hpp
        src/libffsp/debug.cpp
        src/libffsp/debug.hpp
        src/libffsp/eraseblk.cpp
        src/libffsp/eraseblk.hpp
        src/libffsp/ffsp.hpp
        src/libffsp/gc.cpp
        src/libffsp/gc.hpp
        src/libffsp/inode.cpp
        src/libffsp/inode.hpp
        src/libffsp/inode_cache.cpp
        src/libffsp/inode_cache.hpp
        src/libffsp/inode_group.cpp
        src/libffsp/inode_group.hpp
        src/libffsp/io.cpp
        src/libffsp/io.hpp
        src/libffsp/io_backend.hpp
        src/libffsp/io_backend.cpp
        src/libffsp/io_raw.cpp
        src/libffsp/io_raw.hpp
        src/libffsp/log.cpp
        src/libffsp/log.hpp
        src/libffsp/mkfs.cpp
        src/libffsp/mkfs.hpp
        src/libffsp/mount.cpp
        src/libffsp/mount.hpp
        src/libffsp/summary.cpp
        src/libffsp/summary.hpp
        src/libffsp/utils.cpp
        src/libffsp/utils.hpp
    )
    if(WIN32)
        set(FFSP_PLATFORM_SRC
            src/platform/windows/strndup.c
        )
    endif()
    add_library(${FFSP_LIBRARY} STATIC ${FFSP_SRC} ${FFSP_PLATFORM_SRC})

    target_include_directories(${FFSP_LIBRARY} PRIVATE ${FUSE_INCLUDE_DIRS})
    target_link_libraries(${FFSP_LIBRARY} PRIVATE spdlog::spdlog)
    set_target_properties(${FFSP_LIBRARY} PROPERTIES CXX_STANDARD 20)
endif()


if(FFSP_BUILD_FUSELIB)
    set(FUSELIB_LIBRARY "ffsp-fuse")

    set(FUSELIB_SRC
        ${FFSP_SRC}
        ${FFSP_PLATFORM_SRC}
        src/libffsp-fuse/fuse_ffsp.hpp
        src/libffsp-fuse/fuse_ffsp.cpp
        src/libffsp-fuse/fuse_ffsp_log.hpp
        src/libffsp-fuse/fuse_ffsp_utils.hpp
        src/libffsp-fuse/fuse_ffsp_utils.cpp
        src/libffsp-fuse/fuse_get_context.cpp
    )
    add_library(${FUSELIB_LIBRARY} SHARED ${FUSELIB_SRC})

    target_include_directories(${FUSELIB_LIBRARY} PRIVATE src ${FUSE_INCLUDE_DIRS})
    target_link_libraries(${FUSELIB_LIBRARY} PRIVATE ${FUSE_LIBRARIES} spdlog::spdlog)
    set_target_properties(${FUSELIB_LIBRARY} PROPERTIES CXX_STANDARD 20)
endif()


if(FFSP_BUILD_MKFS)
    set(MKFS_BINARY "mkfs.ffsp")

    set(MKFS_SRC
        src/mkfs.ffsp.cpp
    )
    if(WIN32)
        set(MKFS_PLATFORM_SRC
            src/platform/windows/getopt.h
            src/platform/windows/getopt.c
        )
        set(MKFS_PLATFORM_INCLUDE_DIRS src/platform/windows)
    endif()
    add_executable(${MKFS_BINARY} ${MKFS_SRC} ${MKFS_PLATFORM_SRC})

    target_include_directories(${MKFS_BINARY} PRIVATE src ${MKFS_PLATFORM_INCLUDE_DIRS} ${FUSE_INCLUDE_DIRS})
    target_link_libraries(${MKFS_BINARY} PRIVATE ${FFSP_LIBRARY} spdlog::spdlog)
    set_target_properties(${MKFS_BINARY} PROPERTIES CXX_STANDARD 20)
endif()


if(FFSP_BUILD_MOUNT)
    set(MOUNT_BINARY "mount.ffsp")

    set(MOUNT_SRC
        src/libffsp-fuse/fuse_ffsp.hpp
        src/libffsp-fuse/fuse_ffsp.cpp
        src/libffsp-fuse/fuse_ffsp_log.hpp
        src/libffsp-fuse/fuse_ffsp_utils.hpp
        src/libffsp-fuse/fuse_ffsp_utils.cpp
        src/mount.ffsp.cpp
    )
    add_executable(${MOUNT_BINARY} ${MOUNT_SRC})

    target_include_directories(${MOUNT_BINARY} PRIVATE src ${FUSE_INCLUDE_DIRS})
    target_link_libraries(${MOUNT_BINARY} PRIVATE ${FFSP_LIBRARY} ${FUSE_LIBRARIES} spdlog::spdlog)
    set_target_properties(${MOUNT_BINARY} PROPERTIES CXX_STANDARD 20)
endif()


if(FFSP_BUILD_TESTS)
    enable_testing()
    set(TESTS_BINARY "test.ffsp")

    # Fetch GoogleTest
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
    )
    FetchContent_MakeAvailable(googletest)

    set(TESTS_SRC
        src/libffsp-fuse/fuse_ffsp.hpp
        src/libffsp-fuse/fuse_ffsp.cpp
        src/libffsp-fuse/fuse_ffsp_log.hpp
        src/libffsp-fuse/fuse_ffsp_utils.hpp
        src/libffsp-fuse/fuse_ffsp_utils.cpp
        src/libffsp-fuse/fuse_get_context.cpp
        test/ffsp_test_utils.hpp
        test/ffsp_test_utils.cpp
        test/ffsp_basic_fs_api_test.cpp
        test/ffsp_basic_fs_test.cpp
    )
    add_executable(${TESTS_BINARY} ${TESTS_SRC})

    target_include_directories(${TESTS_BINARY} PRIVATE src ${FUSE_INCLUDE_DIRS} ${spdlog_SOURCE_DIR}/include)
    target_link_libraries(${TESTS_BINARY} PRIVATE ${FFSP_LIBRARY} spdlog::spdlog GTest::gtest GTest::gtest_main)
    set_target_properties(${TESTS_BINARY} PROPERTIES CXX_STANDARD 20)

    add_test(NAME ${TESTS_BINARY} COMMAND ${TESTS_BINARY})
endif()
