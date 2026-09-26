# Template: executable target.
#
# bomberman_add_executable(<name>
#     [EXCLUDE <file1> <file2> ...]
#     [LINK_LIBRARIES <lib1> <lib2> ...]
# )
#
# Every .cpp/.h/.hpp file next to the calling CMakeLists.txt is picked up
# automatically; use EXCLUDE to leave specific files out.
function(bomberman_add_executable TARGET_NAME)
    set(multiValueArgs EXCLUDE LINK_LIBRARIES)
    cmake_parse_arguments(ARG "" "" "${multiValueArgs}" ${ARGN})

    bomberman_collect_files(_sources _headers EXCLUDE ${ARG_EXCLUDE})

    add_executable(${TARGET_NAME} ${_sources} ${_headers})

    if(ARG_LINK_LIBRARIES)
        target_link_libraries(${TARGET_NAME} PRIVATE ${ARG_LINK_LIBRARIES})
    endif()

    install(TARGETS ${TARGET_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endfunction()
