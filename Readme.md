# OptiChess

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](opti_chess/LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/platform-Windows%20|%20Linux%20|%20macOS-lightgray)](opti_chess/CMakeLists.txt)
[![Build system](https://img.shields.io/badge/build-CMake-064F8C)](opti_chess/CMakeLists.txt)

**OptiChess** is a research-oriented chess engine written in modern C++20. It features a novel hybrid search algorithm called **GrogrosZero**, combining Monte Carlo tree search principles with classical alpha-beta minimax, integrated quiescence search, and WDL (Win/Draw/Loss) statistical evaluation.

> **Note:** OptiChess is an experimental project focused on algorithmic research and performance optimization. It is not intended for production use or competitive play.

---

## Engine Architecture

OptiChess uses a hybrid search approach that diverges from traditional chess engines:

| Component | Description |
|-----------|-------------|
| **GrogrosZero** | Hybrid search: UCT-style node selection + alpha-beta pruning + quiescence |
| **Evaluation** | ~40 heuristics (material, mobility, king safety, piece-square tables) |
| **Transposition Table** | Zobrist-hashed position cache (`robin_map`) |
| **Move Generation** | Array-based generation with bitboard-style optimizations |
| **Memory Management** | Custom memory pools (`NodeBuffer`, `BoardBuffer`) — zero heap allocation during search |
| **Neural Network** | Optional NN evaluation integration (experimental) |

### Key Design Decisions

- **Zero-allocation search**: All nodes and boards are allocated from pre-allocated pools, eliminating heap fragmentation and GC pauses during the hot path.
- **Hybrid exploration**: Combines the exploration/exploitation balance of UCT with the pruning efficiency of alpha-beta.
- **Cross-platform**: Builds on Windows (MSVC), Linux (GCC/Clang), and macOS.

---

## Tech Stack

| Technology | Purpose |
|------------|---------|
| **C++20** | Language standard (CMake `cxx_std_20`) |
| **CMake 3.24+** | Build system with presets for Debug/Release |
| **raylib 5.5** | Cross-platform GUI (board rendering, move input) |
| **tsl::robin_map 1.3** | High-performance hash maps for TT and node children |
| **GoogleTest 1.14** | Unit testing framework |

---

## Building & Running

### Prerequisites

- CMake 3.24 or later
- A C++20-capable compiler (MSVC, GCC 11+, Clang 14+)
- Internet connection (CMake FetchContent downloads dependencies automatically)

### Quick Build (Windows — Recommended)

```bash
# Configure and build Release
cmake --preset windows-release
cmake --build --preset windows-release

# Run the engine CLI
.\opti_chess\build\windows-release\opti_chess.exe

# Or launch the raylib GUI (requires a display)
.\opti_chess\build\windows-release\opti_chess.exe --gui
```

### Debug Build

```bash
cmake --preset windows-debug
cmake --build --preset windows-debug
```

### Linux / macOS

```bash
# Generate build files
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j$(nproc)

# Run
./build/opti_chess
```

> **Dependencies are fetched automatically** via CMake FetchContent (raylib 5.5, robin-map 1.3.0). The first build may take several minutes to compile raylib from source.

---

## Testing

```bash
# Build with tests enabled (default)
cmake --preset windows-debug -DBUILD_TESTS=ON

# Run unit tests
ctest --preset windows-debug --output-on-failure

# Or run the test binary directly
.\opti_chess\build\windows-debug\opti_chess_tests.exe
```

The test suite covers:
- Board state manipulation (move legality, FEN parsing, castling, en passant)
- Move generation correctness
- Evaluation symmetry and consistency
- Zobrist hashing uniqueness

---

## Project Structure

```
opti_chess/
├── board.cpp/h            # Core board state, move generation, FEN, Zobrist hashing
├── exploration.cpp/h      # GrogrosZero search, quiescence, node tree
├── evaluation.cpp/h       # ~40 evaluation heuristics
├── zobrist.cpp/h          # Zobrist hashing (position keys, TT hashes)
├── buffer.cpp/h           # Memory pools (NodeBuffer, BoardBuffer)
├── gui.cpp/h              # raylib-based graphical interface
├── neural_network.cpp/h   # Optional NN evaluation (experimental)
├── game_tree.cpp/h        # Game tree / variation tracking
├── tests.cpp/h            # Unit tests
├── main_gui.h             # GUI entry point
├── CMakeLists.txt         # Build configuration
├── CMakePresets.json      # Presets for Windows Debug/Release
└── resources/             # Assets (images, opening book)
```

---

## Performance

OptiChess prioritizes search speed above all else:

- **No heap allocations** in the search hot path (memory pools)
- **No virtual calls** in evaluation or search
- **Cache-friendly** data layout for frequently accessed structures
- **Branch prediction hints** on hot conditional paths
- **constexpr** compile-time computed values where possible

Performance profiling is recommended before and after any change to `exploration.cpp`, `evaluation.cpp`, or `board.cpp`.

---

## Documentation

| File | Purpose |
|------|---------|
| `opti_chess/ALGORITHMS.md` | Detailed algorithm reference (search, eval, TT, repetitions) |
| `opti_chess/docs/ARCHITECTURE.md` | High-level architecture overview |
| `opti_chess/docs/DEVELOPMENT.md` | Developer setup guide |
| `opti_chess/BUGFIXES.md` | Bug fix history and analysis |
| `opti_chess/CONTRIBUTING.md` | Contribution guidelines |

---

## License

OptiChess is released under the [MIT License](opti_chess/LICENSE).

---

*This project is a work in progress. Contributions, feedback, and issues are welcome.*
