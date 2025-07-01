add_library(ffsp STATIC)
add_library(ffsp::core ALIAS ffsp)

target_sources(ffsp
    PRIVATE
        debug.cpp
        eraseblk.cpp
        gc.cpp
        inode.cpp
        inode_cache.cpp
        inode_group.cpp
        io.cpp
        io_backend.cpp
        io_raw.cpp
        log.cpp
        mkfs.cpp
        mount.cpp
        summary.cpp
        utils.cpp
        $<$<PLATFORM_ID:Windows>:../platform/windows/strndup.c>
)

set_target_properties(ffsp PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(ffsp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<$<PLATFORM_ID:Windows>:${FUSE_INCLUDE_DIRS}>
)

target_link_libraries(ffsp PUBLIC spdlog::spdlog)

target_compile_definitions(ffsp PUBLIC ${FFSP_PLATFORM_DEFS})

target_compile_options(ffsp PRIVATE ${FFSP_PLATFORM_OPTS})