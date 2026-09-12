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
- **Build hygiene (stale-linkage episode 2026-09-08): incremental MSBuild links under this repo have produced hybrid binaries (edited TUs not recompiled, LNK1104 races with live processes). Before ANY measurement: kill all `opti_chess*` processes, rebuild, verify the exe timestamp is fresh, and confirm with canaries (French FEN eval must read +62; quiet20k MAE ≈101.1). Single outlier measurements on a non-verified binary are worthless — reproduce on a fresh build first. 2026-09-12 addendum (phantom-MAE episode): an incremental dev binary reported EG-dyn MAE 240.2 while a clean full rebuild of the SAME commit reported 254.7 (290/491 `ours` differing, pawn-only positions identical). When two binaries of the same commit disagree: full-rebuild BOTH (delete `build/release/*.dir`) and re-measure; all numbers taken with the stale binary are void.**
- **Follow user instructions literally. NEVER take initiatives contrary to what was asked (no silent reverts, no scope cuts, no judging a direction by MAE when the user ordered it). When in doubt, ask. Disagreements go in the report, not in the code.**
- **Commit every atomic change**: each feature, fix, or optimization must be committed separately with a concise message. This keeps the git history clean and makes it easy to track what happened and revert if needed.
- **NEVER build tests without the GUI (and vice versa): one paired build, never decorrelated.** MSBuild takes a single project per call, so chain BOTH vcxproj in ONE command (`opti_chess_tests.vcxproj; if ($?) { opti_chess.vcxproj }`, Release/minimal) from the same tree, then re-snap the `*_last.exe` pair. No source edit between the two builds, ever.
- **NEVER give a PARTIAL FEN: always full FEN with clocks (`... w - - 0 1`), so it can be pasted directly.**
- **Baseline/last discipline (no checkout of old commits): at every breakthrough, freeze a baseline build and keep it intact until the next breakthrough.** A build is ALWAYS tests + GUI together (`opti_chess_tests.exe` + `opti_chess.exe`, they go as a pair). Sibling worktree `../opti_chess_baseline` pinned on the baseline commit (no cmake configure available for VS18: `build/release/*.vcxproj` copied over with source paths repointed, ZERO_CHECK refs removed, gtest/raylib linked from main `build/release` libs). Snapped binaries live in `build/release/Release/` next to the dev pair (`opti_chess_tests_baseline.exe` + `opti_chess_baseline.exe` = frozen, `*_last.exe` = re-snapped dev pair from master tip after every validated change; MSBuild targets only ever overwrite `opti_chess.exe`/`opti_chess_tests.exe`, never the snaps). The dev build is always last. Compare dev against the frozen baseline: `Puzzle.LichessBenchmark5000` NODES-500 (gate, ~20 min) then NODES-2000 (~80 min), SEED-42, CSVs in `build/bench_bin/`, McNemar significant iff χ²>3.84. Promote to new baseline only when a master is validated (re-pin worktree + rebuild BOTH + re-snap).
- **After every GUI change: test the GUI yourself before handing back.** Launch `build/release/Release/opti_chess.exe`, drive keys via `keybd_event` (WScript.Shell `SendKeys` does NOT reach raylib — use `Add-Type` keybd_event in the same PowerShell call, types don't persist across calls), and verify in `build/release/Release/opti_chess_debug.log`: worker `iter=1,100,...` climbing (CTRL-G), clean `exit iters=N` (CTRL-H/P), no TIMEOUT/CRITICAL, process stays alive through CTRL-G + J (play) + P (headless). Kill the process after.
