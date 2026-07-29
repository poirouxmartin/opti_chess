# OptiChess architecture

OptiChess is an experimental single-process chess application. The main executable launches a raylib GUI and embeds the GrogrosZero engine directly; there is no separate engine service or command-line protocol in the supported desktop workflow.

## High-level flow

```text
raylib input and GUI
        |
        v
      Board <----> GameTree
        |
        +--> Evaluator
        |
        +--> GrogrosZero root node
                  |
                  +--> quiescence search
                  +--> transposition table
                  +--> Board / Node memory pools
```

`main.cpp` selects the GUI entry point. `main_gui.h` owns application initialization, the event loop and engine actions triggered from the UI. The default supported mode is the GUI (`lichess` is set to `false`).

## Core components

### Board and game state

`Board` represents a chess position and its related state: side to move, castling and en-passant information, history, legal moves and evaluation data. It supports FEN import/export and is the boundary between UI/game navigation and search.

`GameTree` records played variations independently from the search tree, allowing the UI to move backward and forward through a game without conflating that history with GrogrosZero's exploratory nodes.

### Evaluation

`Evaluator` and the evaluation types provide a static, white-positive position score. The engine also derives win/draw/loss information and uncertainty used by move selection. Tactical and terminal positions can override ordinary positional scoring.

### Search: GrogrosZero

The search lives in `exploration.*` and uses `Node` objects. It combines:

1. expansion of unexplored legal moves;
2. a quiescence search to stabilize tactical positions;
3. score- and exploration-weighted child selection; and
4. propagation of the best explored evaluation toward the root.

This is deliberately a hybrid experiment rather than a conventional fixed-depth alpha-beta engine. A more detailed, implementation-level account—including score conventions and transposition-table semantics—is available in [ALGORITHMS.md](../ALGORITHMS.md).

### Transpositions and repetitions

`zobrist.*` supplies Zobrist keys and a depth-aware transposition table. Search values in the table use a side-to-move convention, while public `Evaluation` values remain white-positive; conversions happen at search boundaries.

Repetition history is path-local during exploration so that equivalent positions reached through different sequences do not corrupt each other's draw state. The current code also contains an experimental optional node-DAG path for sharing transpositions; it is intended for investigation, not as a stability guarantee.

### Memory model

Search avoids per-node heap allocation by drawing `Board` and `Node` objects from preallocated pools. Pool capacity is calculated from available physical memory, capped by a process-memory budget and adjusted for container overhead. When a pool is full, the engine refines the existing tree rather than allocating indefinitely.

### GUI and resources

`gui.*` builds the desktop board, evaluates/display positions and renders variations. It uses raylib for rendering, input, fonts, shaders and sound. Runtime resources are addressed relative to the executable working directory; the CMake post-build step copies `resources/` next to the executable for that reason.

`windows_tests.*` is a platform-specific adapter used both for available-memory measurement and an experimental screen-binding feature. The latter can detect board pixels and simulate mouse input for supported chess sites; it is intentionally separate from normal local play and analysis.

## Important conventions for engine changes

- Scores can be **white-positive** (`Evaluation`) or **side-to-move** (negamax/quiescence and transposition table). Verify the convention at each boundary.
- Search state must remain path-local unless it is explicitly safe to share. Repetition tracking is the canonical example.
- Avoid allocations, virtual dispatch and unbounded work in evaluation/search hot paths.
- Preserve the bounded memory model when changing `Board`, `Node` or map ownership.
- Treat the node-DAG path as experimental and test with it both enabled and disabled.

## Known limitations

- Windows is the supported platform today because of the Win32 integration layer.
- The default product surface is the GUI; the alternate Lichess/UCI-oriented code path is unfinished and not documented as a supported engine interface.
- The test suite is invoked from the GUI, not from CTest yet.
- This is research code under active development; performance and engine behavior can change between commits.
