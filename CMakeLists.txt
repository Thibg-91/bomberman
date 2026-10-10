cmake_minimum_required(VERSION 3.20)

project(Bomberman CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

option(BOMBERMAN_BUILD_TESTS "Build the unit tests" ON)

include(CommonMacros)
include(BombermanLibrary)
include(BombermanHeaderOnlyLibrary)
include(BombermanExecutable)

add_subdirectory(lib)
add_subdirectory(exe)

if(BOMBERMAN_BUILD_TESTS)
    include(BombermanTest)
    find_package(GTest REQUIRED)
    enable_testing()
    add_subdirectory(tests)
endif()
