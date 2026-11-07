# Template: header-only library target.
#
# bomberman_add_header_only_library(<name>
#     [EXCLUDE <file1> <file2> ...]
#     [LINK_LIBRARIES <lib1> <lib2> ...]
# )
#
# Every .h/.hpp file in the calling CMakeLists.txt's src/ subfolder is picked
# up automatically; use EXCLUDE to leave specific files out.
# Creates target <name> (INTERFACE) plus an ALIAS Bomberman::<name>.
function(bomberman_add_header_only_library TARGET_NAME)
    set(multiValueArgs EXCLUDE LINK_LIBRARIES)
    cmake_parse_arguments(ARG "" "" "${multiValueArgs}" ${ARGN})

    bomberman_collect_files(_sources _headers SRC_DIR src EXCLUDE ${ARG_EXCLUDE})

    add_library(${TARGET_NAME} INTERFACE ${_headers})
    add_library(Bomberman::${TARGET_NAME} ALIAS ${TARGET_NAME})

    target_include_directories(${TARGET_NAME} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    )

    if(ARG_LINK_LIBRARIES)
        target_link_libraries(${TARGET_NAME} INTERFACE ${ARG_LINK_LIBRARIES})
    endif()

    install(TARGETS ${TARGET_NAME})
    install(FILES ${_headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${TARGET_NAME})
endfunction()
