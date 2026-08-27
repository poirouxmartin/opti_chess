cmake_minimum_required(VERSION 3.24)

project(opti_chess VERSION 0.1.0 LANGUAGES CXX)

# robin-map 1.3.0 supports modern CMake, but still declares compatibility with
# CMake 3.5. CMake 4 requires this explicit opt-in for third-party projects.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for third-party dependencies")

# Cross-platform support: Windows, Linux, macOS
# GUI uses raylib which is cross-platform. Platform-specific libraries handled below.

include(FetchContent)
include(CTest)

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

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
)

# Prevent GoogleTest from overriding our compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib robin_map googletest)

add_executable(opti_chess
    main.cpp
    board.cpp
    board_display.cpp
    board_fen.cpp
    buffer.cpp
    evaluation.cpp
    eval_core.cpp
    eval_king.cpp
    eval_pawn.cpp
    eval_mobility.cpp
    eval_misc.cpp
    exploration_diag.cpp
    game_tree.cpp
    gui.cpp
    neural_network.cpp
    tests.cpp
    useful_functions.cpp
    windows_tests.cpp
    zobrist.cpp
)

target_compile_features(opti_chess PRIVATE cxx_std_20)

# Deep search recursion (grogros_zero <-> explore_random_child descent down a
# deep narrow tree) needs more than the 1 MB default main-thread stack.
if(MSVC)
    target_link_options(opti_chess PRIVATE /STACK:16777216)
endif()

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

# ---------------------------------------------------------------------------
# Aggressive compiler optimizations for Release builds
# ASAN_BUILD=ON switches to sanitizer-friendly flags (no /GL //LTCG, /GS on)
# ---------------------------------------------------------------------------
option(ASAN_BUILD "Build with AddressSanitizer (MSVC)" OFF)
if(ASAN_BUILD AND MSVC)
    # /Zi: PDB for symbolized ASAN reports; /FS: parallel cl.exe PDB access;
    # /FI excpt.h: SEH probe macros in every TU (exploration.cpp diagnostics)
    set(MSVC_OPT_FLAGS /O2 /Ob2 /fp:precise /bigobj /Zi /FS /FIexcpt.h /fsanitize=address)
    # Prebuilt googletest is not annotated: match it or the linker refuses.
    add_compile_definitions(_DISABLE_STRING_ANNOTATION _DISABLE_VECTOR_ANNOTATION _DISABLE_OPTIONAL_ANNOTATION)
    add_link_options(/fsanitize=address /DEBUG)
elseif(MSVC)
    # MSVC: /Ox full optimization, /fp:fast fast floats, /GL whole-program,
    # /Oi intrinsics, /Ot favor fast code, /Ob3 aggressive inlining,
    # /GS- no buffer security checks, /bigobj for large TUs
    # /FI excpt.h: SEH probe macros for exploration's diagnostic walk
    set(MSVC_OPT_FLAGS /Ox /fp:fast /GL /Oi /Ot /Ob3 /GS- /bigobj /FIexcpt.h)
endif()

if(MSVC)
    target_compile_options(opti_chess PRIVATE ${MSVC_OPT_FLAGS})
    if(NOT ASAN_BUILD)
        target_link_options(opti_chess PRIVATE /LTCG)
    endif()
else()
    # GCC/Clang: -O3, -ffast-math, native arch, LTO
    set(GCC_OPT_FLAGS -O3 -ffast-math -march=native -flto)
    target_compile_options(opti_chess PRIVATE ${GCC_OPT_FLAGS})
    target_link_options(opti_chess PRIVATE -flto)
endif()

# The application expects assets next to its working directory. Copy them so the
# executable can be launched directly from the build output directory.
add_custom_command(TARGET opti_chess POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources"
            "$<TARGET_FILE_DIR:opti_chess>/resources"
    COMMENT "Copying OptiChess resources"
)

# Install target (optional)
install(TARGETS opti_chess
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)
install(DIRECTORY resources/ DESTINATION share/opti_chess/resources)

# ---------------------------------------------------------------------------
# Unit tests (GoogleTest + CTest)
# ---------------------------------------------------------------------------
# Shared library: engine sources without main.cpp, gui.cpp, main_gui.h,
# main_lichess.h, windows_tests.cpp, tests.cpp (GUI-dependent).
# raylib is linked so that board.h compiles, but tests never open a window.
set(ENGINE_SOURCES
    board.cpp
    board_display.cpp
    board_fen.cpp
    buffer.cpp
    evaluation.cpp
    eval_core.cpp
    eval_king.cpp
    eval_pawn.cpp
    eval_mobility.cpp
    eval_misc.cpp
    exploration_diag.cpp
    game_tree.cpp
    gui.cpp
    neural_network.cpp
    useful_functions.cpp
    windows_tests.cpp
    zobrist.cpp
    stockfish_adapter.cpp
    puzzle.cpp
)

add_library(engine_lib STATIC ${ENGINE_SOURCES})
target_compile_features(engine_lib PUBLIC cxx_std_20)
target_include_directories(engine_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${robin_map_SOURCE_DIR}/include/tsl"
)
target_link_libraries(engine_lib PUBLIC raylib tsl::robin_map)
if(MSVC)
    target_compile_options(engine_lib PRIVATE /W4 /permissive- /utf-8 ${MSVC_OPT_FLAGS})
    target_compile_definitions(engine_lib PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(engine_lib PUBLIC gdi32 user32 winmm shell32 ole32)
elseif(APPLE)
    target_compile_options(engine_lib PRIVATE ${GCC_OPT_FLAGS})
    target_link_libraries(engine_lib PUBLIC "-framework Cocoa" "-framework IOKit" "-framework CoreVideo" "-framework OpenGL")
else()
    target_compile_options(engine_lib PRIVATE ${GCC_OPT_FLAGS})
    target_link_libraries(engine_lib PUBLIC m pthread dl rt X11)
endif()

# Link raylib examples/games off for test build too
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

add_executable(opti_chess_tests
    tests/unit_test.cpp
)
target_link_libraries(opti_chess_tests PRIVATE engine_lib GTest::gtest_main)
target_compile_features(opti_chess_tests PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(opti_chess_tests PRIVATE ${MSVC_OPT_FLAGS})
    if(NOT ASAN_BUILD)
        target_link_options(opti_chess_tests PRIVATE /LTCG)
    endif()
    target_link_options(opti_chess_tests PRIVATE /STACK:16777216)
elseif(NOT WIN32)
    target_compile_options(opti_chess_tests PRIVATE ${GCC_OPT_FLAGS})
    target_link_options(opti_chess_tests PRIVATE -flto)
endif()

# Headless self-play match runner (NEW vs OLD search configurations)
add_executable(opti_chess_selfplay
    selfplay.cpp
)
target_link_libraries(opti_chess_selfplay PRIVATE engine_lib)
target_compile_features(opti_chess_selfplay PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(opti_chess_selfplay PRIVATE ${MSVC_OPT_FLAGS})
    if(NOT ASAN_BUILD)
        target_link_options(opti_chess_selfplay PRIVATE /LTCG)
    endif()
    target_link_options(opti_chess_selfplay PRIVATE /STACK:16777216)
elseif(NOT WIN32)
    target_compile_options(opti_chess_selfplay PRIVATE ${GCC_OPT_FLAGS})
    target_link_options(opti_chess_selfplay PRIVATE -flto)
endif()

include(GoogleTest)
gtest_discover_tests(opti_chess_tests)