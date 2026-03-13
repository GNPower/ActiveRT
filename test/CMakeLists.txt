################################################################################
# test/CMakeLists.txt
#
# Host-side unit tests using the FreeRTOS POSIX/Linux simulator.
#
# The FreeRTOS kernel is fetched from GitHub and built with the GCC_POSIX
# port so that FreeRTOS tasks, queues, semaphores, and queue-sets are real
# (not mocked) during testing.
#
# Each test executable:
#   - Compiles all ActiveRT library sources + the unit test file
#   - Links against freertos_kernel and pthread
#   - Is launched by common/freertos_test_main.c which starts the scheduler
#   - Calls run_tests() inside a FreeRTOS task, then exits with Unity's result
################################################################################

cmake_minimum_required(VERSION 3.16)

include(FetchContent)

################################################################################
# FreeRTOS Kernel — POSIX port
################################################################################

FetchContent_Declare(
    freertos_kernel
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
    GIT_TAG        V11.1.0
    GIT_SHALLOW    TRUE
)

# The FreeRTOS CMake build requires an INTERFACE library named freertos_config
# that provides FreeRTOSConfig.h.
add_library(freertos_config INTERFACE)

target_include_directories(freertos_config SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/posix_config"    # FreeRTOSConfig.h
)

# Force the POSIX port and heap_4.c (malloc-backed FreeRTOS heap)
set(FREERTOS_PORT "GCC_POSIX" CACHE STRING "FreeRTOS port" FORCE)
set(FREERTOS_HEAP "4"         CACHE STRING "FreeRTOS heap impl" FORCE)

FetchContent_MakeAvailable(freertos_kernel)

# Give the root 'activert' library target access to FreeRTOS headers.
# This ensures compile_commands.json entries for library sources include the
# correct paths, which fixes both the CI build and clang-tidy analysis.
# CMake targets are global, so this is valid from a subdirectory.
target_link_libraries(activert PRIVATE freertos_kernel)

################################################################################
# Unity test framework
################################################################################

FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.5.2
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(unity)

################################################################################
# ActiveRT library sources for host builds
#
# We compile the library sources directly into each test executable (rather
# than linking the 'activert' CMake target from the root) so that FreeRTOS
# headers from this test/ directory take precedence over any embedded BSP.
################################################################################

set(ACTIVERT_LIB_SOURCES
    "${CMAKE_SOURCE_DIR}/src/activert_active.c"
    "${CMAKE_SOURCE_DIR}/src/activert_event.c"
    "${CMAKE_SOURCE_DIR}/src/activert_stats.c"
    "${CMAKE_SOURCE_DIR}/src/activert_cli.c"
    "${CMAKE_SOURCE_DIR}/src/activert_queue.c"
    "${CMAKE_SOURCE_DIR}/src/activert_post.c"
    "${CMAKE_SOURCE_DIR}/src/activert_notify.c"
)

################################################################################
# Helper function: add_activert_test(name source)
#
# Creates a test executable from:
#   - The named unit test source file (must define run_tests())
#   - common/freertos_test_main.c  (starts scheduler, calls run_tests())
#   - All ActiveRT library sources
#
# Links against freertos_kernel, unity, and pthread.
# Registers with CTest.
################################################################################

function(add_activert_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME}
        "${TEST_SOURCE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/common/freertos_test_main.c"
        ${ACTIVERT_LIB_SOURCES}
    )

    target_include_directories(${TEST_NAME} PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/posix_config"    # FreeRTOSConfig.h
        "${unity_SOURCE_DIR}/src"
    )

    target_link_libraries(${TEST_NAME} PRIVATE
        freertos_kernel
        unity
        pthread
    )

    # Suppress overzealous warnings from FreeRTOS internals on some GCC versions
    target_compile_options(${TEST_NAME} PRIVATE
        -Wno-unused-parameter
        -Wno-pedantic
    )

    # Timeout: if a test deadlocks the scheduler will spin; abort after 30s
    add_test(
        NAME    ${TEST_NAME}
        COMMAND ${TEST_NAME}
    )
    set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 30)
endfunction()

################################################################################
# Test executables
################################################################################

add_activert_test(test_event_pool
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_event_pool.c")

add_activert_test(test_active_basic
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_active_basic.c")

add_activert_test(test_stats_registry
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_stats_registry.c")
