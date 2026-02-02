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
    ${CMAKE_SOURCE_DIR}/src/core/preset_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/plugins/image_transform.cpp
    ${CMAKE_SOURCE_DIR}/src/cli/parser.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/file_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/string_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/json_output.cpp
)

# Test executable
add_executable(uniconv_tests
    unit/test_types.cpp
    unit/test_string_utils.cpp
    unit/test_file_utils.cpp
    unit/test_preset_manager.cpp
    ${UNICONV_SOURCES}
)

target_link_libraries(uniconv_tests PRIVATE
    GTest::gtest_main
    CLI11::CLI11
    nlohmann_json::nlohmann_json
)

target_include_directories(uniconv_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/include
)

if(VIPS_FOUND)
    target_include_directories(uniconv_tests PRIVATE ${VIPS_INCLUDE_DIRS})
    target_link_libraries(uniconv_tests PRIVATE ${VIPS_LIBRARIES})
    target_link_directories(uniconv_tests PRIVATE ${VIPS_LIBRARY_DIRS})
endif()

include(GoogleTest)
gtest_discover_tests(uniconv_tests
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)
