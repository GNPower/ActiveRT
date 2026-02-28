################################################################################
# docs/CMakeLists.txt
#
# Builds ActiveRT documentation via the Doxygen → Sphinx → HTML pipeline.
#
# Targets:
#   doxygen  — parse C headers/sources, generate XML for Breathe
#   sphinx   — build Sphinx HTML (depends on doxygen)
#   docs     — alias for sphinx (the default docs target)
#
# Usage:
#   cmake --preset docs
#   cmake --build --preset docs
#   # Open build/docs/sphinx/html/index.html
################################################################################

find_package(Doxygen REQUIRED)

find_program(SPHINX_BUILD_EXECUTABLE sphinx-build
    DOC "Path to sphinx-build executable"
)

if(NOT SPHINX_BUILD_EXECUTABLE)
    message(FATAL_ERROR
        "sphinx-build not found. Install docs dependencies with:\n"
        "  pip install -r requirements.txt")
endif()

################################################################################
# Doxygen target
################################################################################

set(DOXYGEN_OUTPUT_DIR  "${CMAKE_CURRENT_BINARY_DIR}/doxygen")
set(DOXYFILE_IN         "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in")
set(DOXYFILE_OUT        "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")

configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

add_custom_target(doxygen
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating Doxygen XML from source headers"
    VERBATIM
)

################################################################################
# Sphinx target
################################################################################

set(SPHINX_SOURCE_DIR   "${CMAKE_CURRENT_SOURCE_DIR}")
set(SPHINX_OUTPUT_DIR   "${CMAKE_CURRENT_BINARY_DIR}/html")
set(SPHINX_CONF_DIR     "${CMAKE_CURRENT_SOURCE_DIR}")

# Pass the Doxygen XML location to Sphinx via an environment variable
# (conf.py reads DOXYGEN_XML_DIR)
add_custom_target(sphinx
    COMMAND ${CMAKE_COMMAND} -E env
                "DOXYGEN_XML_DIR=${DOXYGEN_OUTPUT_DIR}/xml"
            ${SPHINX_BUILD_EXECUTABLE}
                -b html
                -c ${SPHINX_CONF_DIR}
                ${SPHINX_SOURCE_DIR}
                ${SPHINX_OUTPUT_DIR}
    DEPENDS doxygen
    COMMENT "Building Sphinx HTML documentation"
    VERBATIM
)

################################################################################
# Convenience alias
################################################################################

add_custom_target(docs DEPENDS sphinx)
