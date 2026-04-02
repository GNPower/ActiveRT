################################################################################
# GetVersion.cmake
# Reads the project version from a VERSION file or `git describe --tags`.
# Adapted from CppLib_Template.
#
# Usage:
#   include(tools/cmake/modules/GetVersion.cmake)
#   get_version(MY_PROJECT_VERSION)
#   project(MyProject VERSION ${MY_PROJECT_VERSION})
################################################################################

function(get_version OUTPUT)
    set(MESSAGE_ERROR_PREFIX "[get_version] could not determine project version:")
    set(VERSION_FILE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

    if(EXISTS ${VERSION_FILE_PATH})
        message(STATUS "[get_version] Reading version from VERSION file")
        file(READ ${VERSION_FILE_PATH} VERSION_STRING)
        string(STRIP ${VERSION_STRING} VERSION_STRING)
    else()
        message(STATUS "[get_version] No VERSION file found; trying `git describe`")
        find_package(Git QUIET)
        if(Git_FOUND)
            execute_process(
                COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                RESULT_VARIABLE GIT_DESCRIBE_RESULT
                OUTPUT_VARIABLE GIT_DESCRIBE_OUTPUT
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if(GIT_DESCRIBE_RESULT EQUAL "0")
                set(VERSION_STRING ${GIT_DESCRIBE_OUTPUT})
            else()
                message(WARNING "${MESSAGE_ERROR_PREFIX} `git describe` failed; defaulting to v0.0.0")
                set(VERSION_STRING "v0.0.0")
            endif()
        else()
            message(WARNING "${MESSAGE_ERROR_PREFIX} Git not found; defaulting to v0.0.0")
            set(VERSION_STRING "v0.0.0")
        endif()
    endif()

    # Accept both "v1.2.3" and "1.2.3" formats
    string(REGEX MATCH "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)" VERSION_MATCH "${VERSION_STRING}")
    if(VERSION_MATCH)
        string(REGEX REPLACE "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\1.\\2.\\3"
               CLEAN_VERSION "${VERSION_STRING}")
        set(${OUTPUT} "${CLEAN_VERSION}" PARENT_SCOPE)
        message(STATUS "[get_version] Version: ${CLEAN_VERSION}")
    else()
        message(SEND_ERROR
            "${MESSAGE_ERROR_PREFIX} '${VERSION_STRING}' is not a valid semver (expected vX.Y.Z)")
        set(${OUTPUT} "0.0.0" PARENT_SCOPE)
    endif()
endfunction()