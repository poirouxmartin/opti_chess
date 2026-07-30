# Contributing to OptiChess

OptiChess is a personal research project, but issues, questions and pull requests are welcome. This document describes what a change is expected to respect.

## Before you start

Read [`docs/ARCHITECTURE.md`](opti_chess/docs/ARCHITECTURE.md) for the component map and [`ALGORITHMS.md`](opti_chess/ALGORITHMS.md) for the engine invariants. Most non-obvious bugs in this codebase have come from breaking one of those invariants, and [`BUGFIXES.md`](opti_chess/BUGFIXES.md) records the ones already paid for.

## Build and validate

Build instructions are in [`docs/DEVELOPMENT.md`](opti_chess/docs/DEVELOPMENT.md). Windows x64 with Visual Studio 2022 is the only supported toolchain today.

There is no CI yet, so validation is manual and non-negotiable for engine changes:

1. Build the Debug preset.
2. Launch the application and press `T` to run the perft, evaluation and problem suites.
3. Confirm perft still passes on both reference positions. A perft regression is never acceptable.
4. For search, evaluation or move-generation edits, also check by hand in the GUI that loading a FEN, starting and stopping analysis, and navigating the game tree stay responsive.
5. If the change touches the experimental paths, test with `I` and `O` both enabled and disabled.

State in the pull request what you ran and what you observed.

## Engine invariants

These are the rules a change must not silently break:

- **Score conventions.** `Evaluation` is white-positive; negamax, quiescence and the transposition table are side-to-move. Convert at the boundary, and say which convention a new function uses.
- **Mate scores are ply-relative** inside the transposition table. Normalise on store and on retrieval.
- **Repetition state stays path-local** during exploration. Two move orders reaching the same position must not share draw state.
- **The memory model is bounded.** Do not introduce heap allocation in the search hot path, and do not replace the pool model with unbounded growth. A full pool must refine the existing tree.
- **No virtual dispatch** in search or evaluation.
- **The experimental DAG path is experimental.** It must remain switchable at runtime, and both branches must work.

## Performance

`exploration.cpp`, `evaluation.cpp` and `board.cpp` are hot. Profile before and after any change to them, and include the comparison in the pull request. A correctness fix that costs measurable speed is fine — an unmeasured one is not.

## Code style

- Follow the conventions of the file you are editing.
- **New comments are written in English.** The codebase is mid-migration from French; translating the comments in a function you are already touching is welcome, as a separate commit.
- Comment the *why*, the constraints and the trade-offs. Do not comment what the code already says.
- Remove outdated comments and commented-out code rather than leaving them in place.
- Keep runtime assets under `resources/` so CMake packages them with the executable.
- Keep third-party libraries out of the repository — add a pinned CMake `FetchContent` dependency instead.
- Keep Windows-specific system integration isolated in `windows_tests.*`.

## Commits and pull requests

- Conventional commit prefixes (`feat`, `fix`, `refactor`, `docs`, `test`, `chore`), imperative mood, English, one logical change per commit.
- A pull request should state what changed, why, and how it was validated.
- Reference the relevant `BUGFIXES.md` entry when fixing a tracked bug, and update that file's status in the same pull request.
