# Common settings shared by every target in the Bomberman project.
# Included once from the top-level CMakeLists.txt.

# Standard install layout (lib/, bin/, include/), used by every target's
# install() rules so `conan install/export-pkg` actually packages them.
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# All libraries in this project are dynamic by default.
option(BUILD_SHARED_LIBS "Build libraries as shared (.dll/.so/.dylib)" ON)

# Export every symbol on Windows shared libs so sources don't need
# __declspec(dllexport)/(dllimport) markup to stay portable to Linux/macOS,
# where default symbol visibility already exports everything.
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Keep all binaries next to each other so executables find their
# shared libraries at runtime without extra install steps.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Collects every .cpp/.h/.hpp file from a directory next to the calling
# CMakeLists.txt, so target templates don't need an explicit SOURCES/HEADERS
# list.
#
# bomberman_collect_files(<sources_var> <headers_var>
#     [SRC_DIR <dir>]
#     [EXCLUDE <file1> <file2> ...]
# )
#
# SRC_DIR is relative to the calling directory and defaults to "." (i.e. next
# to the CMakeLists.txt itself). EXCLUDE takes filenames relative to SRC_DIR
# (e.g. "foo.cpp"), not full paths or glob patterns.
function(bomberman_collect_files SOURCES_VAR HEADERS_VAR)
    set(oneValueArgs SRC_DIR)
    set(multiValueArgs EXCLUDE)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_SRC_DIR)
        set(ARG_SRC_DIR ".")
    endif()
    set(_dir ${CMAKE_CURRENT_SOURCE_DIR}/${ARG_SRC_DIR})

    file(GLOB _sources CONFIGURE_DEPENDS
        ${_dir}/*.cpp
        ${_dir}/*.cc
    )
    file(GLOB _headers CONFIGURE_DEPENDS
        ${_dir}/*.h
        ${_dir}/*.hpp
    )

    foreach(_excluded IN LISTS ARG_EXCLUDE)
        list(REMOVE_ITEM _sources ${_dir}/${_excluded})
        list(REMOVE_ITEM _headers ${_dir}/${_excluded})
    endforeach()

    set(${SOURCES_VAR} ${_sources} PARENT_SCOPE)
    set(${HEADERS_VAR} ${_headers} PARENT_SCOPE)
endfunction()
