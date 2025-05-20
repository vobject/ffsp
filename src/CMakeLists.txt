add_subdirectory(libffsp)
add_subdirectory(libffsp-fuse)

add_executable(mkfs.ffsp)
target_sources(mkfs.ffsp
    PRIVATE
        mkfs.ffsp.cpp
        $<$<PLATFORM_ID:Windows>:platform/windows/getopt.c>
)
target_include_directories(mkfs.ffsp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${FUSE_INCLUDE_DIRS}
)
target_link_libraries(mkfs.ffsp
    PRIVATE
        ffsp::core
        spdlog::spdlog
)
target_compile_definitions(mkfs.ffsp PRIVATE ${FFSP_PLATFORM_DEFS})
target_compile_options(mkfs.ffsp PRIVATE ${FFSP_PLATFORM_OPTS})

add_executable(mount.ffsp)
target_sources(mount.ffsp
    PRIVATE
        mount.ffsp.cpp
)
target_include_directories(mount.ffsp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${FUSE_INCLUDE_DIRS}
)
target_link_libraries(mount.ffsp
    PRIVATE
        ffsp::core
        ffsp::fuse
        ${FUSE_LIBRARIES}
        spdlog::spdlog
)
target_compile_definitions(mount.ffsp PRIVATE ${FFSP_PLATFORM_DEFS})
target_compile_options(mount.ffsp PRIVATE ${FFSP_PLATFORM_OPTS})