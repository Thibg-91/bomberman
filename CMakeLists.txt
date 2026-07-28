cmake_minimum_required(VERSION 3.20)

project(Bomberman)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(import_dependencies)
include(generic_macros)

add_subdirectory(lib)
add_subdirectory(exe)






