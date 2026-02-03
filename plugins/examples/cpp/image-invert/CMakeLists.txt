cmake_minimum_required(VERSION 3.16)
project(image-invert-plugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find uniconv include directory (adjust path as needed)
set(UNICONV_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include"
    CACHE PATH "Path to uniconv include directory")

# Optional: Find libvips for actual implementation
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(VIPS vips-cpp)
endif()

# Build shared library
add_library(image_invert SHARED image_invert.cpp)

target_include_directories(image_invert PRIVATE
    ${UNICONV_INCLUDE_DIR}
)

if(VIPS_FOUND)
    target_include_directories(image_invert PRIVATE ${VIPS_INCLUDE_DIRS})
    target_link_directories(image_invert PRIVATE ${VIPS_LIBRARY_DIRS})
    target_link_libraries(image_invert PRIVATE ${VIPS_LIBRARIES})
    target_compile_definitions(image_invert PRIVATE HAS_VIPS=1)
endif()

# Set output name without 'lib' prefix on all platforms for consistency
set_target_properties(image_invert PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "image_invert"
)

# Install to plugin directory
install(TARGETS image_invert
    LIBRARY DESTINATION .
    RUNTIME DESTINATION .
)

install(FILES plugin.json DESTINATION .)
