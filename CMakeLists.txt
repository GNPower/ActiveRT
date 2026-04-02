################################################################################
# ActiveRT - Active Object Framework for FreeRTOS
# Root CMakeLists.txt
################################################################################

cmake_minimum_required(VERSION 3.16)

# Version detection: reads VERSION file, falls back to `git describe --tags`
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/modules")
include(GetVersion)
get_version(ACTIVERT_VERSION)

project(ActiveRT
    VERSION     ${ACTIVERT_VERSION}
    DESCRIPTION "Active Object real-time framework for FreeRTOS"
    LANGUAGES   C
)

################################################################################
# Options
################################################################################

option(ACTIVERT_BUILD_TESTS
    "Build host-side unit tests using the FreeRTOS POSIX simulator" ON)
option(ACTIVERT_BUILD_DOCS
    "Build Doxygen + Sphinx documentation" OFF)

################################################################################
# Library source files
# activert_tests.c is excluded — it contains hardware-specific Xilinx tests.
# It is compiled separately in the test/ subdirectory for embedded builds.
################################################################################

set(ACTIVERT_SOURCES
    src/activert_active.c
    src/activert_event.c
    src/activert_stats.c
    src/activert_cli.c
    src/activert_queue.c
    src/activert_post.c
    src/activert_notify.c
)

################################################################################
# Installable static library target
#
# NOTE: This target does NOT link FreeRTOS because FreeRTOS is provided by the
# embedding project's toolchain/BSP. The caller must supply FreeRTOS headers
# in their include path (e.g. via FREERTOS_INCLUDE_DIR or their own target).
################################################################################

add_library(activert STATIC ${ACTIVERT_SOURCES})
add_library(ActiveRT::activert ALIAS activert)

target_include_directories(activert
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(activert PUBLIC c_std_11)

# Apply compiler warnings (only when this is the top-level project)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CompilerWarnings)
    set_project_warnings(activert)
endif()

################################################################################
# Tests
################################################################################

if(ACTIVERT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

################################################################################
# Documentation
################################################################################

if(ACTIVERT_BUILD_DOCS)
    add_subdirectory(docs)
endif()

################################################################################
# Installation & CMake package config (for find_package support)
#
# Skipped when ACTIVERT_BUILD_TESTS is ON: the test build links activert
# against freertos_kernel (a FetchContent target) which CMake would require
# to be in the export set. Test builds are never installed, so the install
# and export rules are only registered for release/library-only builds.
################################################################################

if(NOT ACTIVERT_BUILD_TESTS)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS activert
        EXPORT      ActiveRTTargets
        ARCHIVE     DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY     DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME     DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )

    install(EXPORT ActiveRTTargets
        FILE        ActiveRTTargets.cmake
        NAMESPACE   ActiveRT::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ActiveRT
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ActiveRTConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/ActiveRTConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ActiveRT"
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/ActiveRTConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ActiveRTConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ActiveRTConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ActiveRT"
    )
endif()
