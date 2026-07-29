cmake_minimum_required(VERSION 3.24)

project(opti_chess VERSION 0.1.0 LANGUAGES CXX)

# robin-map 1.3.0 supports modern CMake, but still declares compatibility with
# CMake 3.5. CMake 4 requires this explicit opt-in for third-party projects.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for third-party dependencies")

# Cross-platform support: Windows, Linux, macOS
# GUI uses raylib which is cross-platform. Platform-specific libraries handled below.

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

# Option to build tests
option(BUILD_TESTS "Build unit tests" ON)

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

# Platform-specific libraries
if(WIN32)
    target_link_libraries(opti_chess PRIVATE
        raylib
        tsl::robin_map
        gdi32
        user32
        winmm
        shell32
        ole32
    )
elseif(APPLE)
    target_link_libraries(opti_chess PRIVATE
        raylib
        tsl::robin_map
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework OpenGL"
    )
else()  # Linux
    target_link_libraries(opti_chess PRIVATE
        raylib
        tsl::robin_map
        m
        pthread
        dl
        rt
        X11
    )
endif()

# The existing sources include <robin_map.h> rather than <tsl/robin_map.h>.
# Preserve that established include style without requiring a source-wide rewrite.
target_include_directories(opti_chess PRIVATE "${robin_map_SOURCE_DIR}/include/tsl")

if(MSVC)
    target_compile_options(opti_chess PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(opti_chess PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(opti_chess PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# The application expects assets next to its working directory. Copy them so the
# executable can be launched directly from the build output directory.
add_custom_command(TARGET opti_chess POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources"
            "$<TARGET_FILE_DIR:opti_chess>/resources"
    COMMENT "Copying OptiChess resources"
)

# Enable testing if requested
if(BUILD_TESTS)
    enable_testing()
    # Add test executable (GoogleTest will be fetched)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googletest)
    
    add_executable(opti_chess_tests
        tests.cpp
        board.cpp
        buffer.cpp
        evaluation.cpp
        exploration.cpp
        game_tree.cpp
        generation.cpp
        zobrist.cpp
        useful_functions.cpp
    )
    target_compile_features(opti_chess_tests PRIVATE cxx_std_20)
    target_link_libraries(opti_chess_tests PRIVATE gtest_main gtest)
    target_include_directories(opti_chess_tests PRIVATE "${robin_map_SOURCE_DIR}/include/tsl")
    
    if(MSVC)
        target_compile_options(opti_chess_tests PRIVATE /W4 /permissive- /utf-8)
        target_compile_definitions(opti_chess_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(opti_chess_tests PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
    endif()
    
    add_test(NAME opti_chess_unit_tests COMMAND opti_chess_tests)
endif()

# Install target (optional)
install(TARGETS opti_chess
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)
install(DIRECTORY resources/ DESTINATION share/opti_chess/resources)