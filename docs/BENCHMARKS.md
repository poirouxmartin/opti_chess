# Benchmarks

Machine: AMD Ryzen 9 9900X, Windows 11, MSVC Release (`/Ox /GL /LTCG`), tree
`opti_chess/build/windows-release`. Every binary comes from a `--clean-first` rebuild of
tests + GUI + CLI. Reference perft counts are from
[chessprogramming.org](https://www.chessprogramming.org/Perft_Results), never from the engine.

Commands:

- `opti_chess_cli perft-suite full`: 7 positions, 597 236 237 nodes, copy-make, no bulk counting.
- `opti_chess_cli bench 2000`: 8 positions, 2000 iterations each, `OPTI_SEED=42`; node counts and
  the signature are deterministic (`835175` nodes, signature `8a8961478e14dd47`).
- `scripts/perf_compare.ps1 -A old.exe -B new.exe -Rounds 5`: interleaved A/B runs, medians.

Run-to-run spread on this machine is about +-10 % and the absolute level drifts over a session
(the same binary measured 42 to 57 Mnps in different sessions). Only medians from the same
interleaved A/B session are comparable; numbers from different rows of different sessions are not.

## Baseline (2026-10-13, commit 633f76a)

`perft-suite full`, 3 sequential runs, all 7/7:

| position | depth | nodes | Mnps run 1 / 2 / 3 |
|---|---|---|---|
| startpos | 6 | 119 060 324 | 42.07 / 37.29 / 43.06 |
| kiwipete | 5 | 193 690 690 | 36.82 / 40.52 / 40.94 |
| pos3 | 6 | 11 030 083 | 9.67 (outlier) / 28.34 / 27.61 |
| pos4 | 5 | 15 833 292 | 30.17 / 35.72 / 36.70 |
| pos5 | 5 | 89 941 194 | 40.00 / 38.19 / 38.67 |
| pos6 | 5 | 164 075 551 | 37.83 / 37.65 / 38.93 |
| promotion storm | 5 | 3 605 103 | 29.52 / 29.94 / 31.06 |
| **total** | | 597 236 237 | **36.28 / 38.19 / 39.86** |

`bench 2000`: 35 211 / 36 268 / 36 094 nodes/s, signature `8a8961478e14dd47` every run.

## Optimisations (2026-10-13)

Each step: clean rebuild, `scripts/harness.ps1 -NoBuild -Full` (perft-suite full 7/7, gtest
CI set 145/145, UCI smoke, bench signature unchanged), then A/B against the previous step,
5 rounds, median.

| step | commit | change | perft Mnps A -> B | bench nodes/s A -> B | kept |
|---|---|---|---|---|---|
| A | 9e2ecb4 | perft driver reuses one child `Board` per depth (constructing a Board allocated its history map: one malloc/free per internal node). Perft only, no effect on search | 47.7 -> 56.1 (+18 %) | 37 479 -> 38 049 (noise) | yes |
| B | 9789cec | `get_moves()` reads the piece type from `_array` instead of scanning up to 5 bitboards | 57.3 -> 63.0 (+10 %) | 41 311 -> 39 078 (noise, rounds overlap) | yes |
| C | 0852f15 | constexpr attack tables (knight, king, pawn, empty-board rays + first-blocker bitscan): `get_square_attacker` returns at once when the square is not attacked, `get_pins` skips rays without a matching enemy slider, `get_controls_around_king` builds the enemy attack set with bitboards. Same results, move order untouched | 53.4 -> 59.4 (+11 %) | 36 857 -> 40 289 (+9 %, B ahead in all 5 rounds) | yes |

## Baseline vs final

Same interleaved session, `opti_chess_cli_baseline.exe` (633f76a) vs step C (0852f15), 5 rounds:

| | perft-suite full (Mnps, median) | bench 2000 (nodes/s, median) | perft | bench signature |
|---|---|---|---|---|
| baseline | 42.05 | 36 313 | 7/7 | 8a8961478e14dd47 |
| final | 57.22 (+36 %) | 38 732 (+7 %) | 7/7 | 8a8961478e14dd47 |

Per round, final ahead of baseline on perft in 5/5 rounds and on bench in 5/5 rounds.
Search speed (bench) gains less than perft because a search node is dominated by the
evaluation, not by move generation.

## Not done / next candidates

- Bulk counting at depth 1 in perft (count `_got_moves` instead of make + recurse): large perft
  gain, but it measures movegen only and does not speed up the search.
- Move generation of the pieces themselves (`add_knight_moves`, `add_rect_moves`,
  `add_diag_moves`) with attack tables: the generation order would change, which changes the
  search (bench signature), so it must go through the NODES-500/2000 McNemar gate.
- Magic bitboards for sliders (currently classical rays with bitscan).

## Search profile (2026-10-13, after the movegen work)

`bench 2000` sampled with VS Diagnostics (CpuUsageHigh) on a Release build with `/Zi` +
`/DEBUG` (same optimisation flags; built through the `CL` / `_LINK_` environment variables,
nothing changed in CMake), symbolised with `xperf`. Self time, % of samples (85 287 samples):

| % | function | where |
|---|---|---|
| 10.6 | `Board::get_moves` (with the inlined `add_*_moves`) | movegen.cpp |
| 8.8 | `Board::add_piece_controls` (control maps, eval and `sort_moves`) | eval_mobility.cpp |
| 6.9 | `Board::get_weak_squares` (4 calls per evaluation) | eval_misc.cpp |
| 3.9 | `Board::make_move` | board.cpp |
| 3.7 | `Board::get_long_term_piece_mobility` | eval_mobility.cpp |
| 3.7 | `Board::in_check` (array walk) | movegen.cpp |
| 3.2 | `Board::evaluate` (self) | eval_core.cpp |
| 2.4-2.7 each | `get_alignments`, `get_blocked_and_unblocked_pieces`, `get_attacks_and_defenses`, `get_king_safety`, `get_king_attackers`, `get_pawn_structure`, `get_trapped_pieces`, `get_king_defenders` | eval_*.cpp |
| 1.5 | `Board::assign_move_flags` (+ its trial `make_move`) | board.cpp |
| 1.4 + 1.1 | `std::sort` + `Board::sort_moves` | board.cpp |

Inclusive (second profile, after step N2-4): `Node::quiescence` 86 %, `Board::evaluate` 73 %,
`get_king_safety` 27 %, `get_checks_value` 13 % (of which: its own `get_moves` 5.7 %,
trial `make_move` 3.1 %, `in_check` 2.5 %), `get_moves` 11.6 %, `assign_move_flags` 4.6 %,
`sort_moves` 4.5 %. The search is evaluation-bound: move generation is about 12 % of it.

## Search optimisations (2026-10-13, night 2)

Same protocol (clean rebuild of tests + GUI + CLI, harness perft 7/7 + 145 gtest + UCI + bench
signature `8a8961478e14dd47`, eval gtest subset), then A/B against the previous kept step,
7 rounds (5 for N2-1), median. Kept only with a gain where B wins (nearly) every round.

| step | commit | change | bench nodes/s A -> B | kept |
|---|---|---|---|---|
| N2-1 | 9057b4f | `in_check` from the attack tables (shared `enemy_attackers` helper) | 42 690 -> 44 104 (+3.3 %) | yes |
| N2-2 | - | eval control maps (`compute_piece_controls`) from bitboards, same x-ray rules | 43 619 -> 42 725 (-2.0 %, B behind in 5/5) | **no**, reverted |
| N2-3 | b96dbba | eval / quiescence component timers (`steady_clock::now`) read only under `OPTI_QSTATS`, the only case where they are printed | 49 560 -> 50 969 (+2.8 %, 7/7) | yes |
| N2-4 | - | `get_checks_value` skips moves with no square on a king line (coarse filter) | 44 772 -> 44 799 (noise) | **no**, reverted |
| N2-5 | 2aab608 | `assign_move_flags` skips the trial move when `move_may_give_check` is false | 50 992 -> 52 366 (+2.7 %, 7/7) | yes |
| N2-6 | - | per-call `getenv` / map lookups (`SHARED_TRACE`, `OPTI_KS_*`) read once | 53 391 -> 53 647 (noise) | **no**, reverted |
| N2-7 | 75a0d3f | `get_queen_safety`: return 0 without a queen, skip moves whose destination reaches no queen | 53 066 -> 57 143 (+7.7 %, 7/7) | yes |
| N2-8 | 2274378 + 5c6283f | exact check test from the bitboards (castling, en passant, promotions still use the trial move), used by `get_checks_value` and `assign_move_flags`; fix: the skip path keeps the `reset_eval` side effect of the trial move | 56 449 -> 57 095 (+1.1 %, 7/7) | yes |

N2-8 first changed the bench signature (838 066 nodes): skipping the trial move also skipped
its `reset_eval()`, and `sort_moves` reads `_adv` right after `assign_move_flags`. With
`reset_eval()` called on the skip path the signature is identical again; the same latent
difference existed in N2-5 (signature unchanged there by chance), fixed by 2274378.
`MoveGen.MayGiveCheckNeverMissesACheck` checks the filter against the trial move on every
legal move of 8 positions and their 2-ply trees.

### Night 2: before / after

Same interleaved session, 7 rounds, `opti_chess_cli_night2_base.exe` (0230b98) vs HEAD (5c6283f):

| | bench 2000 (nodes/s, median) | perft-suite full (Mnps, median) | perft | bench signature |
|---|---|---|---|---|
| before (0230b98) | 49 912 | 73.08 | 7/7 | 8a8961478e14dd47 |
| after (5c6283f) | 56 863 (+13.9 %) | 72.63 (unchanged, noise) | 7/7 | 8a8961478e14dd47 |

B ahead on bench in 7/7 rounds. Perft code was not touched tonight.

Next candidates: `get_weak_squares` (4 calls, 48-square scans), `get_checks_value`'s
own `get_moves` and the check-path `get_moves` (escape counting), `sort_moves` rebuilding a full
control map per node, `get_moves` itself (generation order must stay identical, or go through
the McNemar gate).
