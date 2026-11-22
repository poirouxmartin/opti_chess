# Test suite determinism, 2026-11-19

Branch `dev/poiroux/tests-determinism-1911` (from `main` `7c32ce3`), local only. Toolset MSVC 14.52
(`_MSC_FULL_VER` 195237032), Release build of `opti_chess/build/windows-release`.

## 1. Summary

- The failing test and the order-dependent test are the same one: `Endgame.Fine70ExactDagSolvesLadder`
  passed alone and failed in the full CI set (S4 played `e3d3`).
- Root cause: pooled nodes kept the bucket array of their children map across `NodeBuffer::reset()`.
  `robin_map::clear()` keeps `bucket_count`, and the iteration order of `_children` (so every first-max
  tie-break over the children) depends on it. A search started after a reset therefore depended on
  every search run before it in the process.
- Fix (`exploration_diag.cpp`, `NodeBuffer::reset`): drop the children bucket array of every slot,
  so a reset buffer is exactly a freshly initialised one. No assertion was relaxed, no test skipped.
- The same flaw made the `bench` signature history-dependent: each bench position inherited the maps
  of the previous ones. The signature changes from `29ae53c1996ef1e6` to `4036f83b6901fa85` and is now
  identical whatever the order of the positions.
- The refresh WDL change of the night audit (`docs/fond-1911.md` on `dev/poiroux/fond-1911`) is not
  reintroduced. Only its `compiler=` line of `bench` was cherry-picked.

## 2. Evidence for the root cause

New test `Search.ResetPoolsMakeSearchHistoryIndependent`: search Fine #70 S4 for 4000 iterations, then
the start position for 20000, then S4 again, all after `reset()`; the two S4 fingerprints (root value,
iterations, every root child's visits and value, listed in move order) must be equal.

Without the fix (fix line disabled, temporary build), the test fails:

```
first   "76/4001 2413128:912=76 2414128:1239=76 2415128:724=76 2423128:700=76 2425128:425=76"
second  "76/4001 2413128:1045=76 2414128:1087=76 2415128:628=76 2423128:637=76 2425128:603=76"
```

Same position, same budget, same seed: only the history of the pool slots differs. With the fix it passes.

`bench 2000`, `main` vs fix: position 0 (searched first, fresh pool) is identical (15903 nodes);
positions 2, 3, 4 and 6 differ (e.g. position 3: 125145 vs 99752 nodes).

## 3. Validation (measured)

Full CI gtest set (same filter as `scripts/harness.ps1`), fixed binary:

| Run | Command extra | Result | Time |
|---|---|---|---|
| 1 | (default order) | 157/157 passed, exit 0 | 832 s |
| 2 | `--gtest_shuffle --gtest_random_seed=1911` | 157/157 passed, exit 0 | 766 s |
| 3 | `--gtest_shuffle --gtest_random_seed=42` | 157/157 passed, exit 0 | 748 s |
| `main` | (default order) | 155/156, `Endgame.Fine70ExactDagSolvesLadder` FAILED | 694 s |

`Fine70ExactDagSolvesLadder` trajectories (`best`, `solved_at`) are identical in the three runs,
whatever the order: S4 `e3f3` 9000, S5 `d3e2` 6000, S6 `d2d3` 6500, S7 `a1b1` 12500.
On `main` the full run gave S4 `e3d3` (unsolved) and S7 7500.

Command (run from `opti_chess/`):

```
opti_chess_tests.exe "--gtest_filter=-*Debug*:*Perf*:*LichessBenchmark*:*CalibratedBatch*:*TestsTxtBatch*:*TimeBudgetLadder*:*ScalingBenchmark*:*TimeBenchmark*:*GuiLikeBench*:*ParamSweep*:*OFFvsDAG*" [--gtest_shuffle --gtest_random_seed=N]
```

Other checks on the final build:

- `scripts/harness.ps1 -NoBuild -NoGtest`: perft-suite 7/7 OK, UCI `bestmove d2d4` / `bestmove a5b4`,
  bench signature `4036f83b6901fa85`, ALL OK.
- `opti_chess_cli bench 2000` and `bench 2000 reverse`: both `total nodes=676248`,
  `signature=4036f83b6901fa85` (new `reverse` option: same searches in reverse order, hashed in the
  usual order).
- 5000 Lichess puzzles, `Puzzle.LichessBenchmark5000`, `OPTI_PUZZLE_NODES=500`, `OPTI_SEED=42`
  (the runner resets the pools before each puzzle, so results move): fix 4025 vs `main` 4024
  (`C:\tmp\fond-1911-puz500-base.csv`, same toolset), 17 only `main`, 18 only fix, McNemar chi2 = 0.00,
  not significant.
- Cost: `reset()` is only called between whole searches (tests, puzzle runner, CLI), never inside a
  search; slots never used have no bucket array and cost nothing. Search speed is unchanged.

## 4. Compiler sensitivity of the bench signature

Observed (`docs/fond-1911.md`): the same source built with MSVC 14.51 and 14.52 plays different moves on
Fine #70 S4 at 9000+ iterations, while `bench 2000` gave the same signature for both.

Why the signature does not catch it, and why trajectories move with the compiler:

1. The bench is short (2000 iterations x 8 positions) and only hashes best move + node count.
   A codegen change shifts long knife-edge searches long before it moves a 2000-iteration best move.
2. The Release flags are `/Ox /fp:fast /GL` (`opti_chess/CMakeLists.txt:131`). `/fp:fast` lets the
   optimizer reassociate and contract floating-point expressions (UCT scores, WDL, softmax), so two
   compiler versions may round differently; one ulp is enough to flip a tie between two children.
   (Hypothesis, not measured: needs a 14.51 toolset, only 14.44 and 14.52 are installed.)
3. Move ordering uses `std::sort` with no tie-break (`board.cpp:617`): the order of equal scores is
   unspecified and may change with the STL version. MSVC's insertion sort for small ranges keeps it
   stable today for up to 32 moves, so this matters only for wider positions. (Read, not measured.)
4. Until this branch, the signature also depended on position order (fixed here, section 2).

Proposed robust approach (not implemented, it changes search results and needs measuring):

- Keep comparing binaries of the same toolset only; `bench` now prints `compiler=MSVC <ver>`.
- Make ties explicit: add `a.index < b.index` as tie-break in the move-ordering sort, and break
  selection ties on the move key rather than on map order. Cheap, removes STL and capacity sensitivity.
- Add a "reproducibility" build flavour with `/fp:precise` (no contraction) for reference measurements,
  and measure its speed cost against `/fp:fast` before deciding whether it becomes the default.
- Add a canary to the harness: one long DAG search (Fine #70 S4, 20000 iterations) whose per-chunk best
  move sequence is hashed and printed next to the bench signature, so a codegen drift shows at once.

## 5. Commits

- `feat(cli): print the compiler version in bench` (cherry-pick of `596c8da` from `dev/poiroux/fond-1911`)
- `fix(search): return pooled nodes to a fresh state on NodeBuffer::reset` (fix + regression test)
- `feat(cli): add a reverse order to bench to check order independence`
- `docs: report test suite determinism work` (this file)

Raw logs: `C:\tmp\td1911\` (`fix_run*.log`, `main_full.log`, `bench_*.log`, `puz500-fix.csv`, `harness.log`).
