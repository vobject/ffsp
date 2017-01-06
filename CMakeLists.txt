cmake_minimum_required(VERSION 3.2)

project(ffsp)

option(FFSP_BUILD_MKFS "Build file system creation tool." ON)
option(FFSP_BUILD_MOUNT "Build file system mount tool." ON)
option(FFSP_BUILD_BENCHMARKS "Build file system benchmarks." ON)
option(FFSP_BUILD_TESTS "Build file system tests." ON)


set(SPDLOG_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/lib/spdlog-0.11.0/include)

# Create ffsp static library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/libffsp)
set(FFSP_LIBRARIES "ffsp")


if(WIN32)
    set(FUSE_LIBRARY_DIRS "D:/Development/fs/dokany/x64/Debug")
    set(FUSE_INCLUDE_DIRS "D:/Development/fs/dokany/dokan_fuse/include")
    set(FUSE_LIBRARIES "dokanfuse1")

    include_directories(src/libffsp/win32)
    file(GLOB FFSP_SRC_WIN32 src/libffsp/win32/*.h src/libffsp/win32/*.c)
    set(FFSP_SRC "${FFSP_SRC}" "${FFSP_SRC_WIN32}")

    add_definitions(-DFUSE_USE_VERSION=26 -D_WIN32_WINNT=0x0601)
    add_definitions(-DNOMINMAX -D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS /W4)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
else()
    set(FUSE_INCLUDE_DIRS "/usr/include/fuse")
    set(FUSE_LIBRARIES "fuse" "pthread")

    add_definitions(-DFUSE_USE_VERSION=26 -D_FILE_OFFSET_BITS=64)
    add_definitions(-Wall -Wextra -pedantic)
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++1z")
endif()


if(FFSP_BUILD_MKFS)
    set(MKFS_BINARY "mkfs.ffsp")

    set(MKFS_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/mkfs.ffsp.cpp
    )
    add_executable(${MKFS_BINARY} ${MKFS_SRC})

    target_include_directories(${MKFS_BINARY} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${SPDLOG_INCLUDE_DIRS})
    target_link_libraries(${MKFS_BINARY} ${FFSP_LIBRARIES})
    set_target_properties(${MKFS_BINARY} PROPERTIES CXX_STANDARD 14)
endif()


if(FFSP_BUILD_MOUNT)
    set(MOUNT_BINARY "mount.ffsp")

    set(MOUNT_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libffsp-fuse/fuse_ffsp.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libffsp-fuse/fuse_ffsp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/mount.ffsp.cpp
    )
    add_executable(${MOUNT_BINARY} ${MOUNT_SRC})

    target_include_directories(${MOUNT_BINARY} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${SPDLOG_INCLUDE_DIRS})
    target_link_libraries(${MOUNT_BINARY} ${FFSP_LIBRARIES} ${FUSE_LIBRARIES})
    set_target_properties(${MOUNT_BINARY} PROPERTIES CXX_STANDARD 14)
endif()


if(FFSP_BUILD_BENCHMARKS)
    set(BENCHMARKS_BINARY "benchmark.ffsp")

    # Create Google benchmark library
    set(GBENCHMARK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/lib/benchmark-1.1.0)
    set(GBENCHMARK_INCLUDE_DIRS ${GBENCHMARK_ROOT}/include)
    set(GBENCHMARK_LIBRARIES "benchmark")
    add_subdirectory(${GBENCHMARK_ROOT})

    set(BENCHMARKS_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libffsp-fuse/fuse_ffsp.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libffsp-fuse/fuse_ffsp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libffsp-fuse/fuse_get_context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bench/b.cpp
    )
    add_executable(${BENCHMARKS_BINARY} ${BENCHMARKS_SRC})

    target_include_directories(${BENCHMARKS_BINARY} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${SPDLOG_INCLUDE_DIRS} ${GBENCHMARK_INCLUDE_DIRS})
    target_link_libraries(${BENCHMARKS_BINARY} ${FFSP_LIBRARIES} ${GBENCHMARK_LIBRARIES})
    set_target_properties(${BENCHMARKS_BINARY} PROPERTIES CXX_STANDARD 14)
endif()


if(FFSP_BUILD_TESTS)
    set(TESTS_BINARY "test.ffsp")

    # Create Google test library
    set(GTEST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/lib/googletest-release-1.8.0/googletest)
    set(GTEST_INCLUDE_DIRS ${GTEST_ROOT}/include)
    set(GTEST_LIBRARIES "gtest" "gtest_main")
    add_subdirectory(${GTEST_ROOT})

    set(TESTS_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libffsp-fuse/fuse_ffsp.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libffsp-fuse/fuse_ffsp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/libffsp-fuse/fuse_get_context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test/ffsp_test_utils.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test/ffsp_test_utils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test/ffsp_basic_fs_api_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test/ffsp_basic_fs_test.cpp
    )
    add_executable(${TESTS_BINARY} ${TESTS_SRC})

    target_include_directories(${TESTS_BINARY} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${SPDLOG_INCLUDE_DIRS} ${GTEST_INCLUDE_DIRS})
    if(WIN32)
        target_link_libraries(${TESTS_BINARY} ${FFSP_LIBRARIES} ${GTEST_LIBRARIES})
    else()
        target_link_libraries(${TESTS_BINARY} ${FFSP_LIBRARIES} ${GTEST_LIBRARIES} stdc++fs)
    endif()
    set_target_properties(${BENCHMARKS_BINARY} PROPERTIES CXX_STANDARD 14)
endif()
