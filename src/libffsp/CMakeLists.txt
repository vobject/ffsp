cmake_minimum_required(VERSION 3.2)

project(ffsp)

set(SRC_FILES
    bitops.hpp
    byteorder.hpp
    debug.cpp
    debug.hpp
    eraseblk.cpp
    eraseblk.hpp
    ffsp.hpp
    gc.cpp
    gc.hpp
    inode.cpp
    inode.hpp
    inode_cache.cpp
    inode_cache.hpp
    inode_group.cpp
    inode_group.hpp
    io.cpp
    io.hpp
    io_backend.hpp
    io_backend.cpp
    io_raw.cpp
    io_raw.hpp
    log.cpp
    log.hpp
    mkfs.cpp
    mkfs.hpp
    mount.cpp
    mount.hpp
    summary.cpp
    summary.hpp
    utils.cpp
    utils.hpp
)

if(WIN32)
    set(PLATFORM_SRC_FILES
        getopt.h
        getopt.c
        gettimeofday.c
        strndup.c
    )
endif()

add_library(${PROJECT_NAME} STATIC ${SRC_FILES} ${PLATFORM_SRC_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE ${SPDLOG_INCLUDE_DIRS})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 14)
