cmake_minimum_required(VERSION 3.20)

project(ffsp LANGUAGES CXX VERSION 0.1.0)

option(FFSP_BUILD_TESTS "Build file system tests." ON)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SPDLOG_BUILD_PIC ON CACHE BOOL "Build spdlog with -fPIC" FORCE)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.3
)
FetchContent_MakeAvailable(spdlog)

find_package(Threads REQUIRED)

if(WIN32)
    set(FUSE_INCLUDE_DIRS ${DOKAN_INCLUDE_DIR} ${DOKAN_INCLUDE_DIR}/fuse)
    set(FUSE_LIBRARIES ${DOKAN_LIB_DIR}/libdokanfuse1.dll.a)
    set(FFSP_PLATFORM_DEFS FUSE_USE_VERSION=26 _WIN32_WINNT=0x0601 NOMINMAX _CRT_NONSTDC_NO_DEPRECATE _CRT_SECURE_NO_WARNINGS)
    set(FFSP_PLATFORM_OPTS /W4)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FUSE REQUIRED fuse)
    add_library(FUSE::fuse INTERFACE IMPORTED)
    set_target_properties(FUSE::fuse PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${FUSE_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${FUSE_LIBRARIES}"
    )
    set(FUSE_LIBRARIES FUSE::fuse Threads::Threads)
    set(FFSP_PLATFORM_DEFS FUSE_USE_VERSION=26 _FILE_OFFSET_BITS=64)
    set(FFSP_PLATFORM_OPTS -Wall -Wextra -pedantic)
endif()

add_subdirectory(src)

if(FFSP_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
    )
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_subdirectory(test)
endif()