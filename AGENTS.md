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
- **After every important change (search, eval, time management): benchmark before/after.** Eval: `Puzzle.EvaluationSuite`, `Puzzle.EndgameSuite`, `Puzzle.EvalCategoryReport` + `Puzzle.EvalAttribution` on the quiet bank (MAE/gap). Puzzles: `Puzzle.LichessBenchmark5000` NODES-2000/SEED-42 (GATE-50 quick, NODES-500 reference) with `OPTI_BENCH_LOG`, McNemar vs baseline CSV (significant iff χ²>3.84). Prefer NODES (deterministic, load-proof) over TIME (wall-clock, throttle-sensitive); never conclude from a single TIME run on a busy machine.
- **Build hygiene (stale-linkage episode 2026-09-08): incremental MSBuild links under this repo have produced hybrid binaries (edited TUs not recompiled, LNK1104 races with live processes). Before ANY measurement: kill all `opti_chess*` processes, rebuild, verify the exe timestamp is fresh, and confirm with canaries (French FEN eval must read +62; quiet20k MAE ≈101.1). Single outlier measurements on a non-verified binary are worthless — reproduce on a fresh build first.**
- **Commit every atomic change**: each feature, fix, or optimization must be committed separately with a concise message. This keeps the git history clean and makes it easy to track what happened and revert if needed.
- **After every improvement**: rebuild the GUI `opti_chess.exe` (`cmake --build build/release --config Release --target opti_chess`) so it can be tested/investigated manually. Stop any running instance first.
- **After every GUI change: test the GUI yourself before handing back.** Launch `build/release/Release/opti_chess.exe`, drive keys via `keybd_event` (WScript.Shell `SendKeys` does NOT reach raylib — use `Add-Type` keybd_event in the same PowerShell call, types don't persist across calls), and verify in `build/release/Release/opti_chess_debug.log`: worker `iter=1,100,...` climbing (CTRL-G), clean `exit iters=N` (CTRL-H/P), no TIMEOUT/CRITICAL, process stays alive through CTRL-G + J (play) + P (headless). Kill the process after.
