################################################################################
# CompilerWarnings.cmake
# Applies a standard set of compiler warnings to a target.
#
# Usage:
#   include(tools/cmake/modules/CompilerWarnings.cmake)
#   set_project_warnings(my_target)
################################################################################

function(set_project_warnings TARGET)
    set(MSVC_WARNINGS
        /W4
        /w14640   # thread-unsafe static member initialisation
        /w14826   # conversion is sign-extended
        /w14928   # illegal copy-init
    )

    set(GCC_CLANG_WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wformat=2
        -Wwrite-strings
        -Wmissing-prototypes
        -Wstrict-prototypes
        -Wold-style-definition
        -Wno-unused-parameter   # FreeRTOS task functions always receive pvParameters
    )

    if(MSVC)
        target_compile_options(${TARGET} PRIVATE ${MSVC_WARNINGS})
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${TARGET} PRIVATE ${GCC_CLANG_WARNINGS})
    endif()
endfunction()
