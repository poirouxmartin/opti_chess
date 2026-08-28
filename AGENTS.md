# AGENTS.md — opti_chess

## Description

Moteur d'échecs haute performance en Rust + IA par réseaux de neurones (NNUE supervisé et auto-play RL). Interface d'analyse GUI via raylib. Recherche hybride GrogrosZero (UCT + alpha-beta + quiescence), évaluation WDL, search borné zéro-allocation.

## Stack

- C++20
- CMake
- raylib (GUI d'analyse)
- Visual Studio (sln)

## Conventions

- Build via CMake ou `opti_chess.sln`
- Code research-oriented, pas un engine UCI
- **After every code fix**: build + run `opti_chess_tests.exe --gtest_filter="-*Debug*:*Perf*"` to verify no regression. Only proceed to the next fix after tests pass.
- **After validation**: commit with a concise message describing the fix, then push.
- **Commit every atomic change**: each feature, fix, or optimization must be committed separately with a concise message. This keeps the git history clean and makes it easy to track what happened and revert if needed.
- **After every improvement**: rebuild the GUI `opti_chess.exe` (`cmake --build build/release --config Release --target opti_chess`) so it can be tested/investigated manually. Stop any running instance first.
