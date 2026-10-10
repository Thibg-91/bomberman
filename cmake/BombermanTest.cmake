# Template: GoogleTest unit test executable.
#
# bomberman_add_test(<name>
#     [EXCLUDE <file1> <file2> ...]
#     [LINK_LIBRARIES <lib1> <lib2> ...]
# )
#
# Every .cpp/.h/.hpp file next to the calling CMakeLists.txt is picked up
# automatically; use EXCLUDE to leave specific files out. The executable is
# linked against GTest::gtest_main and each test case is registered in CTest.
include(GoogleTest)

function(bomberman_add_test TARGET_NAME)
    set(multiValueArgs EXCLUDE LINK_LIBRARIES)
    cmake_parse_arguments(ARG "" "" "${multiValueArgs}" ${ARGN})

    bomberman_collect_files(_sources _headers EXCLUDE ${ARG_EXCLUDE})

    add_executable(${TARGET_NAME} ${_sources} ${_headers})

    target_link_libraries(${TARGET_NAME} PRIVATE ${ARG_LINK_LIBRARIES} GTest::gtest_main)

    # Tests can locate the level files shipped with the project.
    target_compile_definitions(${TARGET_NAME} PRIVATE
        BOMBERMAN_DATA_DIR="${PROJECT_SOURCE_DIR}/data"
    )

    # PRE_TEST: discovery runs at ctest time, so building the tests never
    # depends on the shared libraries being loadable.
    gtest_discover_tests(${TARGET_NAME} DISCOVERY_MODE PRE_TEST)
endfunction()
