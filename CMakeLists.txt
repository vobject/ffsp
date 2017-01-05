cmake_minimum_required(VERSION 3.2)

project(ffsp)

option(FFSP_BUILD_MKFS "Build file system creation tool." ON)
option(FFSP_BUILD_MOUNT "Build file system mount tool." ON)
option(FFSP_BUILD_TESTS "Build file system tests." ON)
option(FFSP_BUILD_BENCHMARKS "Build file system benchmarks." OFF)

set(FFSP_LIB "ffsp")
set(MKFS_BINARY "mkfs.ffsp")
set(MOUNT_BINARY "mount.ffsp")
set(TEST_BINARY "test.ffsp")
set(BENCHMARK_BINARY "benchmark.ffsp")

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected. Default to Debug")
    set(CMAKE_BUILD_TYPE "Debug")
endif()

file(GLOB MKFS_SRC src/mkfs.ffsp.cpp)
file(GLOB MOUNT_SRC src/fuse_ffsp.hpp src/fuse_ffsp.cpp src/mount.ffsp.cpp)
file(GLOB FFSP_SRC src/libffsp/*.hpp src/libffsp/*.cpp)
file(GLOB TESTS_SRC src/fuse_ffsp.hpp src/fuse_ffsp.cpp test/*.hpp test/*.cpp)
file(GLOB BENCHMARKS_SRC src/fuse_ffsp.hpp src/fuse_ffsp.cpp bench/*.hpp bench/*.cpp)

set(SPDLOG_INCLUDE_DIRS "lib/spdlog-0.9.0/include")

if(WIN32)
    set(FUSE_INCLUDE_DIRS "D:/Development/fs/dokany/dokan_fuse/include")
    set(FUSE_LIBRARY_DIRS "D:/Development/fs/dokany/x64/Debug")
    set(FUSE_LIBRARIES "dokanfuse1")

    include_directories(src/libffsp/win32)
    file(GLOB FFSP_SRC_WIN32 src/libffsp/win32/*.h src/libffsp/win32/*.c)
    set(FFSP_SRC "${FFSP_SRC}" "${FFSP_SRC_WIN32}")

    add_definitions(-DFUSE_USE_VERSION=26 -D_WIN32_WINNT=0x0601)
    add_definitions(-DNOMINMAX -D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS /W4)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
else()
    set(SPDLOG_LIBRARIES "pthread")

    set(FUSE_INCLUDE_DIRS "/usr/include/fuse")
    set(FUSE_LIBRARIES "fuse" "pthread")

    add_definitions(-DFUSE_USE_VERSION=26 -D_FILE_OFFSET_BITS=64)
    add_definitions(-Wall -Wextra -pedantic)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++1z")
endif()

include_directories(${FUSE_INCLUDE_DIRS} ${SPDLOG_INCLUDE_DIRS})
link_directories(${FUSE_LIBRARY_DIRS})

add_library(${FFSP_LIB} ${FFSP_SRC})

if(FFSP_BUILD_MKFS)
    add_executable(${MKFS_BINARY} ${MKFS_SRC})
    target_link_libraries(${MKFS_BINARY} ${FFSP_LIB} ${SPDLOG_LIBRARIES})
endif()

if(FFSP_BUILD_MOUNT)
    add_executable(${MOUNT_BINARY} ${MOUNT_SRC})
    target_link_libraries(${MOUNT_BINARY} ${FFSP_LIB} ${SPDLOG_LIBRARIES} ${FUSE_LIBRARIES})
endif()

if(FFSP_BUILD_TESTS)
    set(GTEST_ROOT "lib/googletest-release-1.8.0/googletest")
    add_subdirectory(${GTEST_ROOT})
    include_directories(src ${GTEST_ROOT}/include ${GTEST_ROOT})

    add_executable(${TEST_BINARY} ${TESTS_SRC})
    if(WIN32)
        target_link_libraries(${TEST_BINARY} ${FFSP_LIB} gtest gtest_main)
    else()
        target_link_libraries(${TEST_BINARY} ${FFSP_LIB} gtest gtest_main stdc++fs)
    endif()
endif()

if(FFSP_BUILD_BENCHMARKS)
    set(GBENCHMARK_ROOT "lib/benchmark-1.1.0")
    add_subdirectory(${GBENCHMARK_ROOT})
    include_directories(src ${GBENCHMARK_ROOT}/include ${GBENCHMARK_ROOT})

    add_executable(${BENCHMARK_BINARY} ${BENCHMARKS_SRC})
    target_link_libraries(${BENCHMARK_BINARY} ${FFSP_LIB} benchmark)
endif()
