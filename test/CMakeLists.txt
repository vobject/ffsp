
add_executable(test.ffsp)

target_sources(test.ffsp
    PRIVATE
        ffsp_test_utils.cpp
        ffsp_basic_fs_api_test.cpp
        ffsp_basic_fs_test.cpp
)

target_include_directories(test.ffsp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${FUSE_INCLUDE_DIRS}
)

target_link_libraries(test.ffsp
    PRIVATE
        ffsp::core
        ffsp::fuse
        spdlog::spdlog
        GTest::gtest
        GTest::gtest_main
)

target_compile_definitions(test.ffsp PRIVATE ${FFSP_PLATFORM_DEFS})

target_compile_options(test.ffsp PRIVATE ${FFSP_PLATFORM_OPTS})

add_test(
    NAME test.ffsp
    COMMAND test.ffsp
)

set_tests_properties(test.ffsp PROPERTIES
    ENVIRONMENT "GTEST_COLOR=1"
    TIMEOUT 300
)
