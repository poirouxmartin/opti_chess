# OptiChess

> An experimental C++ chess engine and desktop analysis board built around **GrogrosZero**, a hybrid Monte-Carlo / minimax search.

![OptiChess analysis board](resources/readme/project_highlight.png)

OptiChess is a personal engine project focused on the practical engineering behind chess search: fast board representation, tactical stability, transposition handling, memory-bounded exploration and an interactive visual debugger built with raylib.

## Try it in a few minutes

**Platform:** Windows 10/11 x64
**Requirements:** [CMake 3.24+](https://cmake.org/download/) and Visual Studio 2022 (or newer) with the **Desktop development with C++** workload.

```powershell
git clone <repository-url>
cd opti_chess
cmake --preset windows-release
cmake --build --preset windows-release
.\build\windows-release\Release\opti_chess.exe
```

The first CMake configuration downloads the two open-source dependencies automatically: [raylib](https://www.raylib.com/) and [robin-map](https://github.com/Tessil/robin-map). The build copies the required textures, fonts, sounds, opening book and shader into the executable directory, so it can be launched from there directly.

No manual include paths, library paths or asset copying are required.

### Open in Visual Studio

Visual Studio can open the repository folder directly and detect `CMakePresets.json`. Select the **Windows x64 — Release** preset, build `opti_chess`, then run it. The legacy `.vcxproj` is kept for historical compatibility; CMake is the supported setup path.

## What to explore

The application starts on an interactive chessboard. It is designed as a research and debugging UI, so the most useful first interactions are:

| Action | Control |
| --- | --- |
| Analyse the current position continuously | `Ctrl` + `G` |
| Stop automatic analysis | `Ctrl` + `H` |
| Run one analysis step | `Enter` (`Shift` + `Enter` runs 10) |
| Play the most explored move | `P` |
| Flip the board | `F` |
| Copy / load the current FEN | `X` / `V` |
| Navigate the game tree | `Left` / `Right` |
| Toggle the controls panel | `H` |
| Toggle fullscreen | `F11` |

Use the window close button or `Esc` to exit. Search pools size themselves from available RAM and use a bounded memory budget; a modern desktop with at least 8 GB RAM is recommended for comfortable analysis.

## Technical highlights

- **GrogrosZero search:** Monte-Carlo-style exploration combined with minimax principles, tactical quiescence search and WDL-aware move scoring.
- **Chess core:** legal move generation, FEN/PGN handling, evaluation, game-tree navigation, repetitions and Zobrist hashing.
- **Transposition work:** depth-aware scalar transposition table, mate-score normalization and an experimental DAG mode for node sharing.
- **Performance-oriented memory model:** `Board` and search `Node` pools with O(1) free lists, adaptive sizing and a hard memory budget.
- **Native UI:** a raylib desktop board with principal variations, evaluation display, arrows, FEN clipboard support and optional experimental screen binding.

The engine is experimental—not a production-strength or rated engine—and the code deliberately exposes ongoing search experiments in the UI. See [the architecture guide](docs/ARCHITECTURE.md) for design details and [the development guide](docs/DEVELOPMENT.md) for build and contribution notes.

## Repository guide

| Path | Purpose |
| --- | --- |
| `board.*` | Board state, legal moves, FEN/PGN and evaluation-facing chess logic |
| `exploration.*` | GrogrosZero search, quiescence, node selection and transposition integration |
| `evaluation.*` | Static position evaluation |
| `buffer.*` | Adaptive, bounded board/node pools |
| `zobrist.*` | Position hashing and transposition table |
| `gui.*`, `main_gui.h` | raylib interface and application loop |
| `tests.*`, `Tests.txt` | Interactive test harness and test-position corpus |
| `resources/` | Runtime fonts, textures, sounds, shaders and opening book |
| `docs/` | Architecture and development documentation |

## Development

Use the debug preset while changing the project:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
.\build\windows-debug\Debug\opti_chess.exe
```

The current test harness is interactive: press `T` in the application to run its configured suite. It is useful for engine work but is not yet packaged as a non-interactive CTest target. See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for validation guidance and troubleshooting.

## Dependencies

- [raylib 5.5](https://github.com/raysan5/raylib) — graphics, windowing, audio and input
- [robin-map 1.3.0](https://github.com/Tessil/robin-map) — cache-friendly hash maps

Both are pinned in `CMakeLists.txt` and fetched only at configure time.

## Notes on platform support

The build currently targets Windows because the experimental chess-site binding uses Win32 screen capture and input simulation. The engine and GUI are otherwise organized so that this boundary can be isolated for a future cross-platform port.

## License

No license file is currently included. Please contact the repository owner before redistributing or reusing the code.
