################################################################################
# ActiveRT - Active Object Framework for FreeRTOS
# Root CMakeLists.txt
################################################################################

cmake_minimum_required(VERSION 3.16)

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

option(ACTIVERT_BUILD_DOCS
    "Build Doxygen + Sphinx documentation" OFF)

################################################################################
# Library source files
################################################################################

set(ACTIVERT_SOURCES
    src/activert_active.c
    src/activert_event.c
    src/activert_post.c
    src/activert_queue.c
    src/activert_stats.c
)

################################################################################
# Installable static library target
################################################################################

add_library(activert STATIC ${ACTIVERT_SOURCES})
add_library(ActiveRT::activert ALIAS activert)

target_include_directories(activert
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(activert PUBLIC c_std_11)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CompilerWarnings)
    set_project_warnings(activert)
endif()

################################################################################
# Installation & CMake package config
################################################################################

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
