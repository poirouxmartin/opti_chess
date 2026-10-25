# OptiChess architecture

This document describes the code as it is in the tree (2026-10-13). Source files are named, not line numbers. For the audit findings behind the open items, see [AUDIT.md](AUDIT.md). For the algorithm conventions (score signs, mate encoding, `Node` invariants), see [ALGORITHMS.md](../opti_chess/ALGORITHMS.md).

## Overview and data flow

All engine code is in one flat directory, `opti_chess/opti_chess/`. It is built as a static library, `engine_lib`, that the front ends link against.

```text
FEN / startpos + moves
        |
        v
  Board  (board_fen.cpp, movegen.cpp, board.cpp)
        |
        v
  root Node  (exploration.h)
        |
        v
  Node::grogros_zero  (exploration_diag.cpp)
        |-- explore_new_move      expand one unexplored legal move
        |-- explore_random_child  refine an explored child (pick_random_child)
        |-- quiescence            bounded tactical search on new leaves
        |
        +--> Evaluator / Evaluation  (evaluation.h, eval_*.cpp)
        +--> transposition table     (zobrist.h, zobrist.cpp)
        +--> Board and Node pools    (buffer.h, buffer.cpp)
```

Front ends that drive this flow:

- `opti_chess` GUI: `main.cpp` calls into `main_gui.h` (event loop, keys, engine actions). The search runs on a worker thread and the GUI reads a snapshot for display (`gui.cpp`).
- `opti_chess_cli`: `cli.cpp`, headless perft, bench and a minimal UCI loop. The UCI `go` path calls `PuzzleRunner::run` (`puzzle.cpp`).
- `opti_chess_tests`: GoogleTest suite in `tests/unit_test.cpp`.
- `opti_chess_selfplay`: `selfplay.cpp`, search-parameter matches.

## Build targets

Defined in `opti_chess/CMakeLists.txt` (CMake 3.24+, C++20):

| Target | Sources / link | Purpose |
|---|---|---|
| `engine_lib` | static library, `ENGINE_SOURCES` in `CMakeLists.txt` (movegen, board display, `eval_*.cpp`, `exploration_diag.cpp`, `game_tree.cpp`, `neural_network.cpp`, `puzzle.cpp`, `stockfish_adapter.cpp`, `windows_tests.cpp` and others) | Engine core, linked by all other targets |
| `opti_chess` | `main.cpp` + GUI sources, links raylib | Analysis GUI. A POST_BUILD step copies `resources/` next to the executable |
| `opti_chess_tests` | `tests/unit_test.cpp`, links `engine_lib` and `GTest::gtest_main` | GoogleTest suite, registered with `gtest_discover_tests` |
| `opti_chess_cli` | `cli.cpp`, links `engine_lib` | Headless CLI |
| `opti_chess_selfplay` | `selfplay.cpp`, links `engine_lib` | Search parameter matches |

Dependencies are fetched with `FetchContent` at configure time: raylib 5.5, robin-map v1.3.0 (`tsl::robin_map`), GoogleTest v1.14.0. Network access is needed on the first configure only.

Compiler flags:

- MSVC Release: `/Ox /fp:fast /GL /Oi /Ot /Ob3 /GS- /bigobj`, with `/LTCG` at link time. The executables get a 16 MB stack (`/STACK:16777216`).
- GCC and Clang: `-O3 -ffast-math -march=native -flto`.

`opti_chess/CMakePresets.json` defines `windows-release` (binary directory `opti_chess/build/windows-release`) and `windows-debug` (`opti_chess/build/windows-debug`). Executables from the release preset are in `build/windows-release/Release/`. The generator used locally is Visual Studio 18 (Insiders), so its bundled `cmake.exe` is needed there.

## Board representation

Declared in `board.h` (class `Board`).

- Piece encoding (`piece_type` enum): 0 empty, 1-6 white pawn, knight, bishop, rook, queen, king, 7-12 the same for black.
- `_array[8][8]` of `uint8_t`: row 0 is rank 1, and square = row * 8 + col.
- `_bitboards[13]` (one per piece code) and `_occupancies[3]` (white, black, all).
- Cached king squares: `_white_king_pos` and `_black_king_pos`, updated on king moves.
- `Move` is a small bitfield struct: start row and column (3 bits each), end row and column (3 bits each), a 2-bit promotion piece (queen, rook, bishop, knight), and a flags byte (`IS_NULL`, `IS_CAPTURE`, `IS_PROMOTION`, `IS_CHECK`, `IS_MATE`, a result field, `FLAGS_EVALUATED`).
- The move list is `_moves[max_moves]` with `max_moves = 100` (`board.h`). Moves beyond 100 are dropped (open item B-09).
- Zobrist key in `_zobrist_key`. The position history used for repetitions is `_positions_history`.

## Move generation

Legal move generation is direct: pins and check information are computed before any move is produced, so there is no make-and-test step. `movegen.cpp` contains:

- `get_moves()`: the entry point. It fills `_moves` for the side to move.
- `get_pins()`: walks rays from the king and returns the pinned pieces. It skips rays that contain no matching enemy slider, using precomputed rays.
- `get_square_attacker()`: finds the attackers of the king square. A bitboard pre-check runs first, and the array walk happens only when the square is attacked.
- `get_controls_around_king()`: builds the enemy control map of the squares around the king. It uses constexpr attack tables for knights, kings and pawns, and classical ray attacks with a first-blocker bitscan. Magic bitboards are not used.
- `get_interpose_mask()`: the squares that block a single check.
- Per-piece generators `add_pawn_moves`, `add_knight_moves`, `add_rect_moves`, `add_diag_moves` and `add_king_moves`. They take the pin direction and the interposition mask. Pieces are iterated from the bitboards, and the type is read from `_array`. All four promotion pieces are generated. `en_passant_exposes_king()` handles the en passant pin case (B-01).

Castling is generated from the rights flag, and FEN parsing drops a right when the king or rook is not on its home square (`board_fen.cpp`).

## make_move and hashing

`Board::make_move` works in place on a copy: callers copy the board and then apply the move (copy-make). There is no `unmake_move`. The Zobrist key is updated incrementally inside `make_move` when `add_to_history` is set.

`zobrist.h` declares the `Zobrist` class (random keys, generated by `generate_zobrist_keys()`) and `shared_zobrist()`. `zobrist.cpp` implements the keys and the transposition table.

## Search (GrogrosZero)

The search is in `exploration.h` (declarations, the `Node` class, `ChildLink`, `MoveScoreList`, `NodeLock`, `NodeBuffer`, globals) and `exploration_diag.cpp` (implementation).

`Node` is one position in the search tree. It owns a `Board`, a map of children (`ChildLink`, robin-map) and the statistics used for selection (evaluation, WDL, visit counts). The root node is built from the current position.

`Node::grogros_zero(...)` is the search entry point. In order:

1. It returns at once if the iteration count is not positive or if the recursion depth reaches `GROGROS_MAX_RECURSION_DEPTH` (96).
2. On the first call it runs `quiescence` on the root.
3. If the node is terminal, it returns.
4. It loops for the requested number of iterations. In each iteration:
   - If the node can still expand and not every legal move has been explored, it calls `explore_new_move`. That adds one child and runs a bounded quiescence on the new leaf.
   - Otherwise, if the node has children, it calls `explore_random_child`, which uses `pick_random_child` to choose a child by a score combining evaluation, WDL, visits and uncertainty (`get_node_score`). An optional forced round-robin (`g_forced_every`, env `OPTI_FORCED_EVERY`) is off by default.
   - Otherwise the buffers are full: the search stops cleanly and logs once.

`quiescence` is a bounded tactical search with stand-pat and alpha-beta bounds. Transposition table entries use the flags `TT_EXACT`, `TT_ALPHA`, `TT_BETA` and `TT_STANDPAT` (`zobrist.h`).

Repetition state is path-local: one `PositionHistory` is threaded through the descent and restored by push and pop, so each iteration starts from the same state. Inside the search a position repeated twice is scored as a draw. The game itself still uses the threefold rule.

Transposition modes (both are experimental and both default to off):

- `g_tt_main_search` (`exploration.h`, defined in `exploration_diag.cpp`): use the transposition table in the main search. Toggled at runtime with `I` in the GUI.
- `g_tt_node_dag` (same): share nodes between transpositions in a DAG (`node_map`), instead of a tree. Toggled with `O` in the GUI.
- `g_shared_tree` (same): shared-tree mode with node locks (`NodeLock`), off by default.

With the DAG, `OPTI_DAG_EXACT=1` (off by default) periodically rewrites the stored values of all reachable nodes with the exact value of the game graph, where a repetition cycle is a draw (`graph_game.h`, `refresh_graph_values` in `exploration_diag.cpp`). This removes the ghost values that cycles of king shuffles can keep; details in `opti_chess/ALGORITHMS.md` §8.6.

Search time is limited by `g_search_deadline` and `g_search_abort` (atomics in `exploration.h`).

## Evaluation

`evaluation.h` declares the `Evaluator` class: 30 float weights (for example `_piece_value`, `_king_safety`, `_pawn_structure`, `_trapped_pieces`, `_queen_safety`). The result type `Evaluation` is declared in `board.h`. It holds the value (`_value`), an evaluated flag, and WDL and uncertainty information. Public `Evaluation` values are white-positive. Search code uses side-to-move values, and the conversion happens at the boundaries.

The terms are implemented in `evaluation.cpp` and `eval_core.cpp`, `eval_king.cpp`, `eval_pawn.cpp`, `eval_mobility.cpp` and `eval_misc.cpp`. They cover material, mobility (short and long term), king safety and proximity, pawn structure, piece activity, weak squares, trapped pieces and endgame terms. The evaluation is phase-aware. Some structures are shared with the board through `EvalControls` (`board.h`).

The evaluation is hand-written. No neural network is used in it.

## Memory model

`buffer.h` and `buffer.cpp` provide two arenas, both thread-local:

- `monte_board_buffer` (`BoardBuffer`), defined in `buffer.cpp`: pool of `Board` objects.
- `monte_node_buffer` (`NodeBuffer`), declared in `exploration.h`: pool of `Node` objects.

Each arena keeps a free-index list. `is_full()` reports exhaustion, and `init()` allocates once. `init()` is a no-op on a live arena, so it must be reset before reuse (B-08).

Pool sizes come from `compute_pool_sizing()` (`buffer.cpp`): the budget is `min(ram_fraction * available RAM, hard_cap_bytes)`, with a 1 GiB floor, divided by an RSS overhead factor. The transposition table is capped at a quarter of the budget. Available RAM is read through `get_available_physical_memory()` in `windows_tests.cpp`. When a pool is full, the search stops expanding and refines the existing tree.

## GUI

- `main.cpp` selects the GUI entry point. `main_gui.h` holds the application loop, key handling and engine actions (`IsKeyPressed` checks). The key list is in the README Controls section.
- `gui.h` and `gui.cpp`: raylib rendering of the board, variations, evaluation graph and statistics. The search runs on a worker thread. The GUI reads a snapshot (`_tree_snapshot`) for display. Part of that read is not locked (open item B-17).
- `game_tree.h` and `game_tree.cpp`: the played game and its variations, separate from the search tree. Children are owned through `unique_ptr`.
- `board_display.cpp`: console printing and SAN labels.
- `windows_tests.h` and `windows_tests.cpp`: the Win32 layer. It provides the physical-memory probe and the experimental screen binding (`locate_chessboard`, `click_move`).
- `stockfish_adapter.h` and `stockfish_adapter.cpp`: a subprocess client to an external Stockfish, used by tests and benchmarks.

## Tools and tests

Headless CLI (`cli.cpp`, built as `opti_chess_cli`):

```text
opti_chess_cli perft <depth> [fen...]
opti_chess_cli divide <depth> [fen...]
opti_chess_cli perft-suite [quick|full]
opti_chess_cli bench [iterations]
opti_chess_cli uci
```

- `bench` sets `OPTI_SEED=42` unless the variable is already set, and prints nodes per second and a signature of the results.
- `uci` is a minimal loop: `uci`, `isready`, `ucinewgame`, `position startpos|fen ... [moves ...]`, `go`, `quit`. `go` is synchronous and `stop` is ignored. Budget precedence: `nodes`, then `movetime`, then a clock-based budget (capped to 0.05-10 s), then `infinite` (20000 iterations), then the default of 2000 iterations. `depth` is parsed but does not set a budget.

GoogleTest (`tests/unit_test.cpp`, built as `opti_chess_tests`): FEN, perft, move generation, evaluation, search and puzzle tests. CI runs the set selected by the exclusion regex in `.github/workflows/build.yml`.

Scripts:

- `scripts/harness.ps1` [-NoBuild] [-NoGtest] [-Full] [-BenchIters 2000]: builds tests, GUI and CLI from one tree, then runs `perft-suite`, the gtest set, a UCI smoke session and `bench`.
- `scripts/perf_compare.ps1` -A old.exe -B new.exe [-Rounds 3] [-BenchIters 2000]: interleaved A/B runs of `perft-suite full` and `bench`, with medians.

Legacy in-GUI suite: `tests.cpp` (run with `T` in the GUI) reads `Tests.txt` at runtime.

CI (`.github/workflows/build.yml`): runs on push to `main` and on pull requests. It configures and builds the `windows-release` preset, runs ctest with the exclusion regex, then runs the GATE-500 puzzle check (`Puzzle.LichessBenchmark5000` must solve at least 400).

## Training pipeline

There is no training code in C++ and no NNUE in this tree.

- `scripts/download_lichess_puzzles.py`: downloads the Lichess puzzle database (`lichess_db_puzzle.csv.zst`), filters puzzles by theme and converts moves to SAN with python-chess.
- `scripts/download_lichess_evals.py`: downloads the Lichess evaluation database and writes FEN|eval_cp lines to `tests/lichess_evals.txt` (options `--count` and `--min-depth`).
- `neural_network.h` and `neural_network.cpp`: a `Network` class with layers {768, 64, 1} and random weight generation. `main_gui.h` creates an instance and fills it with random weights. Nothing else uses it, and the search receives no network (the `network` argument defaults to null).
- `selfplay.cpp` (`opti_chess_selfplay [games] [iterations_per_move]`, options `--new-alpha=`, `--old-alpha=`, `--new-beta=`, `--old-beta=`, `--new-gamma=`, `--old-gamma=`, `--persist`, `-vp`, `-tp`, `-cap`): plays matches between two search parameter sets.

## Known limitations

Open items from [AUDIT.md](AUDIT.md) (section 5):

- B-09: move lists are capped at 100 and extra moves are dropped (accepted risk).
- B-14, B-15: quiescence early exits and stand-pat while in check can leave children unrefined. Open, needs the McNemar gate.
- B-16: `pick_random_child` dereferences a child without the null check used elsewhere (shared-tree mode only).
- B-17: GUI snapshot read without the lock.
- B-18: `neural_network.cpp` writes past its input on malformed FEN (dead module).
- B-19: `Tests.txt` rewrite ignores stream errors (legacy harness).
- B-20: `from_fen` writes GUI globals, so it is not safe with several search threads.
- B-21: a static used by all threads in `zobrist.cpp`.

Other limits:

- Windows is the only supported platform (the Win32 layer is not abstracted).
- The UCI support is minimal (see above). There is no `stop` and no `info` streaming beyond one `info nodes` line.
- Search results depend on the order of previous searches in the same process (see the note in AUDIT.md, section 5).
