add_library(ffsp-fuse STATIC)
add_library(ffsp::fuse ALIAS ffsp-fuse)

target_sources(ffsp-fuse
    PRIVATE
        fuse_ffsp.cpp
        fuse_ffsp_utils.cpp
        fuse_get_context.cpp
)

set_target_properties(ffsp-fuse PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(ffsp-fuse
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${FUSE_INCLUDE_DIRS}
)

target_link_libraries(ffsp-fuse
    PUBLIC
        ffsp::core
        ${FUSE_LIBRARIES}
        spdlog::spdlog
)

target_compile_definitions(ffsp-fuse PUBLIC ${FFSP_PLATFORM_DEFS})

target_compile_options(ffsp-fuse PRIVATE ${FFSP_PLATFORM_OPTS})