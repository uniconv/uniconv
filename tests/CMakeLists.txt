# Fetch Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Source files needed for tests
set(UNICONV_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/engine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin_resolver.cpp
    ${CMAKE_SOURCE_DIR}/src/core/preset_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/pipeline_executor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/execution_graph.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin_discovery.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin_loader_cli.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin_loader_native.cpp
    ${CMAKE_SOURCE_DIR}/src/core/config_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/cli/parser.cpp
    ${CMAKE_SOURCE_DIR}/src/cli/pipeline_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/builtins/tee.cpp
    ${CMAKE_SOURCE_DIR}/src/builtins/collect.cpp
    ${CMAKE_SOURCE_DIR}/src/builtins/clipboard.cpp
    ${CMAKE_SOURCE_DIR}/src/builtins/passthrough.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/file_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/string_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/json_output.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/version_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/http_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/mime_detector.cpp
    ${CMAKE_SOURCE_DIR}/src/core/dependency_checker.cpp
    ${CMAKE_SOURCE_DIR}/src/core/dependency_installer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/installed_plugins.cpp
    ${CMAKE_SOURCE_DIR}/src/core/registry_client.cpp
    ${CMAKE_SOURCE_DIR}/src/core/output/console_output.cpp
    ${CMAKE_SOURCE_DIR}/src/core/output/json_output.cpp
)

# Test executable
add_executable(uniconv_tests
    unit/test_types.cpp
    unit/test_string_utils.cpp
    unit/test_file_utils.cpp
    unit/test_preset_manager.cpp
    unit/test_pipeline_parser.cpp
    unit/test_version_utils.cpp
    unit/test_registry_types.cpp
    unit/test_installed_plugins.cpp
    unit/test_clipboard.cpp
    unit/test_output_progress.cpp
    unit/test_scatter_collect.cpp
    ${UNICONV_SOURCES}
)

target_link_libraries(uniconv_tests PRIVATE
    GTest::gtest_main
    CLI11::CLI11
    nlohmann_json::nlohmann_json
    Threads::Threads
    $<TARGET_NAME_IF_EXISTS:unofficial::libmagic::libmagic>
    $<TARGET_NAME_IF_EXISTS:PkgConfig::LIBMAGIC>
    $<TARGET_NAME_IF_EXISTS:LibMagic::LibMagic>
)

target_include_directories(uniconv_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/include
)

if(AVUTIL_FOUND)
    target_include_directories(uniconv_tests PRIVATE ${AVUTIL_INCLUDE_DIRS})
    target_link_libraries(uniconv_tests PRIVATE ${AVUTIL_LIBRARIES})
    target_link_directories(uniconv_tests PRIVATE ${AVUTIL_LIBRARY_DIRS})
endif()

include(GoogleTest)
gtest_discover_tests(uniconv_tests
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)
