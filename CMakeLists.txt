cmake_minimum_required(VERSION 3.20)

project(Bomberman CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(CommonMacros)
include(BombermanLibrary)
include(BombermanHeaderOnlyLibrary)
include(BombermanExecutable)

add_subdirectory(lib)
add_subdirectory(exe)
