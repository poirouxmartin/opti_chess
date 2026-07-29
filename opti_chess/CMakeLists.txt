cmake_minimum_required(VERSION 3.24)

project(opti_chess VERSION 0.1.0 LANGUAGES CXX)

# robin-map 1.3.0 supports modern CMake, but still declares compatibility with
# CMake 3.5. CMake 4 requires this explicit opt-in for third-party projects.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for third-party dependencies")

# The GUI's chess-site binding uses Win32 screen-capture and input APIs.
if(NOT WIN32)
    message(FATAL_ERROR "OptiChess currently supports Windows only.")
endif()

include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(RAYLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    robin_map
    GIT_REPOSITORY https://github.com/Tessil/robin-map.git
    GIT_TAG v1.3.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(raylib robin_map)

add_executable(opti_chess
    main.cpp
    board.cpp
    buffer.cpp
    evaluation.cpp
    exploration.cpp
    game_tree.cpp
    generation.cpp
    gui.cpp
    league.cpp
    match.cpp
    neural_network.cpp
    player.cpp
    tests.cpp
    time_tests.cpp
    useful_functions.cpp
    windows_tests.cpp
    zobrist.cpp
)

target_compile_features(opti_chess PRIVATE cxx_std_20)

target_link_libraries(opti_chess PRIVATE
    raylib
    tsl::robin_map
    gdi32
    user32
    winmm
)

# The existing sources include <robin_map.h> rather than <tsl/robin_map.h>.
# Preserve that established include style without requiring a source-wide rewrite.
target_include_directories(opti_chess PRIVATE "${robin_map_SOURCE_DIR}/include/tsl")

if(MSVC)
    target_compile_options(opti_chess PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(opti_chess PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(opti_chess PRIVATE -Wall -Wextra -Wpedantic)
endif()

# The application expects assets next to its working directory. Copy them so the
# executable can be launched directly from the build output directory.
add_custom_command(TARGET opti_chess POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources"
            "$<TARGET_FILE_DIR:opti_chess>/resources"
    COMMENT "Copying OptiChess resources"
)
