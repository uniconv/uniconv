cmake_minimum_required(VERSION 3.20)
project(uniconv VERSION 0.1.10 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include custom modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerWarnings)

# Options
option(UNICONV_BUILD_TESTS "Build tests" ON)
option(UNICONV_ENABLE_WARNINGS "Enable compiler warnings" ON)
# Fetch dependencies
include(FetchContent)

# CLI11 for command line parsing
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2
)

# nlohmann/json for JSON handling
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(cli11 json)

# Configure version header
configure_file(
    "${CMAKE_SOURCE_DIR}/include/uniconv/version.h.in"
    "${CMAKE_BINARY_DIR}/include/uniconv/version.h"
    @ONLY
)

# Main executable
add_executable(uniconv
    src/main.cpp
    src/core/types.h
    src/core/engine.h
    src/core/engine.cpp
    src/core/plugin_manager.h
    src/core/plugin_manager.cpp
    src/core/preset_manager.h
    src/core/preset_manager.cpp
    src/core/plugin_manifest.h
    src/core/plugin_discovery.h
    src/core/plugin_discovery.cpp
    src/core/plugin_loader_cli.h
    src/core/plugin_loader_cli.cpp
    src/core/config_manager.h
    src/core/config_manager.cpp
    src/core/plugin_loader_native.h
    src/core/plugin_loader_native.cpp
    src/core/pipeline_executor.cpp
    src/plugins/plugin_interface.h
    src/builtins/tee.cpp
    src/cli/parser.h
    src/cli/parser.cpp
    src/cli/pipeline_parser.cpp
    src/cli/commands/info_command.h
    src/cli/commands/info_command.cpp
    src/cli/commands/formats_command.h
    src/cli/commands/formats_command.cpp
    src/cli/commands/preset_command.h
    src/cli/commands/preset_command.cpp
    src/cli/commands/plugin_command.h
    src/cli/commands/plugin_command.cpp
    src/cli/commands/config_command.h
    src/cli/commands/config_command.cpp
    src/utils/file_utils.h
    src/utils/file_utils.cpp
    src/utils/string_utils.h
    src/utils/string_utils.cpp
    src/utils/json_output.h
    src/utils/json_output.cpp
    src/utils/version_utils.h
    src/utils/version_utils.cpp
    src/utils/http_utils.h
    src/utils/http_utils.cpp
    src/core/registry_types.h
    src/core/dependency_checker.h
    src/core/dependency_checker.cpp
    src/core/installed_plugins.h
    src/core/installed_plugins.cpp
    src/core/registry_client.h
    src/core/registry_client.cpp
)

target_include_directories(uniconv PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(uniconv PRIVATE
    CLI11::CLI11
    nlohmann_json::nlohmann_json
)

if(UNICONV_ENABLE_WARNINGS)
    set_project_warnings(uniconv)
endif()

# Install
install(TARGETS uniconv RUNTIME DESTINATION bin)

# Tests
if(UNICONV_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
