# ---------------------------------------------------------------------------
# bomberman_add_library(<name>)
#
# Declares a shared library target named "<project>_<name>" (e.g.
# bomberman_grid.dll) from sources found in the caller's src/ directory.
#
# Usage, from lib/<name>/CMakeLists.txt:
#   bomberman_add_library(<name>)
# ---------------------------------------------------------------------------

macro(bomberman_add_library LIB_NAME)
    string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)
    set(TARGET_NAME ${PROJECT_NAME_LOWER}_${LIB_NAME})

    file(GLOB_RECURSE ${TARGET_NAME}_SOURCES CONFIGURE_DEPENDS src/*.cpp)
    file(GLOB_RECURSE ${TARGET_NAME}_HEADERS CONFIGURE_DEPENDS src/*.h)

    add_library(${TARGET_NAME} SHARED
        ${${TARGET_NAME}_SOURCES}
        ${${TARGET_NAME}_HEADERS}
    )

    target_include_directories(${TARGET_NAME}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
            $<INSTALL_INTERFACE:include/${LIB_NAME}>
    )

    target_compile_features(${TARGET_NAME} PUBLIC cxx_std_20)

    set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME ${TARGET_NAME}
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endmacro()

macro(bomberman_add_executable LIB_NAME)
    string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)
    set(TARGET_NAME ${PROJECT_NAME_LOWER}_${LIB_NAME})

    file(GLOB_RECURSE ${TARGET_NAME}_SOURCES CONFIGURE_DEPENDS src/*.cpp)
    file(GLOB_RECURSE ${TARGET_NAME}_HEADERS CONFIGURE_DEPENDS src/*.h)

    add_executable(${TARGET_NAME}
        ${${TARGET_NAME}_SOURCES}
        ${${TARGET_NAME}_HEADERS}
    )

    target_include_directories(${TARGET_NAME}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

    set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME ${TARGET_NAME}
    )
endmacro()


macro(public_link LIB_NAME)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${LIB_NAME})
endmacro()

macro(private_link LIB_NAME)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIB_NAME})
endmacro()