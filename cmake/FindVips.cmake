# FindVips.cmake - Find libvips library
# This module uses pkg-config as the primary method

find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    pkg_check_modules(PC_VIPS QUIET vips-cpp)
endif()

find_path(VIPS_INCLUDE_DIR
    NAMES vips/vips8
    HINTS ${PC_VIPS_INCLUDEDIR} ${PC_VIPS_INCLUDE_DIRS}
    PATH_SUFFIXES vips
)

find_library(VIPS_LIBRARY
    NAMES vips vips-cpp
    HINTS ${PC_VIPS_LIBDIR} ${PC_VIPS_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vips
    REQUIRED_VARS VIPS_LIBRARY VIPS_INCLUDE_DIR
    VERSION_VAR PC_VIPS_VERSION
)

if(Vips_FOUND)
    set(VIPS_LIBRARIES ${VIPS_LIBRARY})
    set(VIPS_INCLUDE_DIRS ${VIPS_INCLUDE_DIR})

    if(NOT TARGET Vips::Vips)
        add_library(Vips::Vips UNKNOWN IMPORTED)
        set_target_properties(Vips::Vips PROPERTIES
            IMPORTED_LOCATION "${VIPS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${VIPS_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(VIPS_INCLUDE_DIR VIPS_LIBRARY)
