# Benchmarks

Machine: AMD Ryzen 9 9900X, Windows 11, MSVC Release (`/Ox /GL /LTCG`), tree
`opti_chess/build/windows-release`. Every binary comes from a `--clean-first` rebuild of
tests + GUI + CLI. Reference perft counts are from
[chessprogramming.org](https://www.chessprogramming.org/Perft_Results), never from the engine.

Commands:

- `opti_chess_cli perft-suite full`: 7 positions, 597 236 237 nodes, copy-make, no bulk counting.
- `opti_chess_cli bench 2000`: 8 positions, 2000 iterations each, `OPTI_SEED=42`; node counts and
  the signature are deterministic (`835175` nodes, signature `8a8961478e14dd47` up to the
  repetition fix of 2026-10-15; since then `832192` nodes, signature `e5cc2d99503b0425`).
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

## Night 3 (2026-10-13, 05:56-07:30): check counting and quiescence overhead

Callee breakdown of the second profile (direct callees, % of all samples):
`get_checks_value` 13.9 % = its `get_moves` for the attacker 5.7 + trial `make_move` 3.1 +
`in_check` 2.5 + `std::map` env lookups 1.1 + self 0.9 + `Board` copy 0.5; by call site,
the check-path `b_check.get_moves()` (escape counting) is the largest single line (3.4 %).
`Node::quiescence` 92 % inclusive = `evaluate` 76.5 + `assign_move_flags` 4.7 + `sort_moves` 4.7
(`compute_piece_controls` 1.7, `std::sort` 1.6, self 1.2) + `get_moves` 1.6 + self 1.2.
N2-8 (exact check test, measured above) already removed most trial `make_move` / `in_check`
calls from `get_checks_value`.

| step | change | A/B (7 rounds, bench nodes/s median) | verdict |
|---|---|---|---|
| N3-1 | slider moves (`add_rect_moves`, `add_diag_moves`, used by every `get_moves`, including the two in `get_checks_value`) from the ray tables, same generation order (nearest square first, same direction order) | 55 498 -> 53 486 (-3.6 %, B ahead 1/7) | **no**, reverted (perft 7/7 and signature were identical) |
| N3-2 | `sort_moves` walks only the opponent's pieces (the only map it reads) | 55 670 -> 53 888 (-3.2 %, 1/7); re-measured against a fresh control build of HEAD: 54 049 -> 53 400 (-1.2 %, 1/7) | **no**, reverted |
| control | two clean builds of the same commit (79791f9), 5 rounds | 54 579 vs 54 563 (2/5) | build-to-build effect below 1 % |

Both rejected changes do strictly less work per call, yet measured slower in a repeatable way
(and the control rules out a build or ordering artefact); the likely cause is code layout /
inlining changes under `/GL /LTCG` in the hot `get_moves` and `sort_moves` paths. Patches kept
locally (`C:\tmp\n3s1_slider_rays_rejected.patch`, `C:\tmp\n3s2_sort_opponent_map.patch`)
for a later attempt with PGO or explicit `__forceinline` control.

### Before / after (all of 2026-10-13)

| | bench 2000 (nodes/s) | perft-suite full (Mnps) | perft | signature | same-session A/B |
|---|---|---|---|---|---|
| start of night 1 (633f76a) | 36 313 | 42.05 | 7/7 | 8a8961478e14dd47 | vs 0852f15 |
| end of night 1 (0852f15) | 38 732 (+7 %) | 57.22 (+36 %) | 7/7 | 8a8961478e14dd47 | vs 633f76a |
| start of night 2 (0230b98) | 49 912 | 73.08 | 7/7 | 8a8961478e14dd47 | vs 5c6283f |
| end of night 2 / night 3 (5c6283f code, HEAD 79791f9) | 56 863 (+13.9 %) | 72.63 | 7/7 | 8a8961478e14dd47 | vs 0230b98 |

Night 3 kept no code change: both candidates failed the A/B. Absolute levels differ between
sessions (machine state); compare only within a row pair measured in the same session.

## Night 4 (2026-10-14/15, branch dev/perf-showcase)

Same machine and protocol (clean rebuild of tests + GUI + CLI, `scripts/harness.ps1 -Full`: perft-suite full 7/7, 146 gtest, UCI smoke, bench `835175` nodes / signature `8a8961478e14dd47`; eval gtest subset `Puzzle.EvaluationSuite:Puzzle.EndgameSuite:Eval*`, 19/19). Puzzle gates on `tests/lichess_5000.txt` (`Puzzle.LichessBenchmark5000`, `OPTI_SEED=42`, run from the repo root), compared puzzle by puzzle with McNemar (significant iff chi2 > 3.84).

Start of session (59d7939): harness perft 59.04 Mnps, bench 48 478 nodes/s; NODES-500 (`OPTI_PUZZLE_NODES=500`) 4049/5000 (81.0 %), 52 522 nodes/s; TIME-100 (`OPTI_PUZZLE_TIME=0.1`) 3559/5000 (71.2 %), 52 225 nodes/s.

### Profile (bench 2000, 57 645 samples, VS Diagnostics CpuUsageHigh + xperf, /Zi build)

| | self % | inclusive % |
|---|---|---|
| `Node::quiescence` | 1.2 | 91.7 |
| `Board::evaluate` | 3.9 | 77.9 |
| `Board::get_king_safety` | 2.7 | 26.9 |
| `Board::get_moves` | 11.9 | 12.2 |
| `Board::get_checks_value` | - | 11.7 |
| `Board::get_weak_squares` | 8.0 | 8.0 |
| `Board::get_long_term_piece_mobility` | 4.0 | 7.5 |
| `Board::get_queen_safety` | 1.3 | 6.8 |
| `Board::add_piece_controls` | 5.2 | 5.2 |
| `Board::assign_move_flags` | 1.0 | 5.0 |
| `Board::sort_moves` | 1.2 | 4.9 |
| `Board::get_alignments` | 4.7 | 4.7 |

A bench "node" is a tree node with one full evaluation and one quiescence search; the search is evaluation-bound.

### Steps

| step | commit | change | A/B | kept |
|---|---|---|---|---|
| N4-1 | b4864df | per-thread evaluation cache (2^17 direct-mapped entries) keyed by a recomputed Zobrist key, plus the halfmove clock when the fortress term reads it; a hit returns the exact stored result | bench 48 782 -> 52 112 (+6.8 %, 5/5, same binary with `OPTI_EVAL_CACHE=0` as A) | yes |
| N4-2 | 7346715 | perft bulk counting (`perft-bulk`, `perft-suite [quick|full] bulk`): depth 1 returns the legal move count; `Perft.BulkCountingMatchesReference` | perft-suite full copy-make 59.75 / 60.27 / 56.99 vs bulk 220.05 / 219.08 / 221.63 Mnps, 7/7 both | yes (perft only) |
| N4-3 | 1ec60fa | weak-square classification (pawn scans, 4 calls per evaluation) computed once per pawn structure, per-thread table keyed by both pawn bitboards (exact key) | bench 52 940 -> 54 714 (+3.4 %, 7/7) | yes |
| N4-4 | - | legal moves of each side generated once per evaluation and shared by `get_checks_value` and `get_queen_safety` (4 -> 2 `get_moves` per evaluation) | v1 (array in `EvalControls`): 56 550 -> 52 524 (2/7); v2 (thread_local array): 55 026 -> 53 545 (2/9) | **no**, reverted (patch `C:\tmp\n4_shared_legal_moves_rejected.patch`) |
| N4-5 | - | MSVC PGO (`/GENPROFILE`, training `bench 2000` + `perft-suite quick`, relink `/USEPROFILE`, 100 % of functions optimised with profile data) | bench 55 714 -> 53 372 (1/7), perft 56.93 -> 55.27 | **no** |

N4-4 does strictly less work and stayed slower over two variants, the same pattern as N3-1 and N3-2; PGO did not recover it either. While writing N4-4 we found that `get_queen_safety` patched a copied board before `add_piece_controls`, but `add_piece_controls` reads `this`, so the patch never had an effect: the slider-blocking intent of that term is not implemented today (behaviour unchanged, noted for a later eval fix).

All kept steps leave every search output identical: NODES-500 at 7cb5af9 gives 4049/5000 with the same chosen move in all 5000 puzzles and the same total node count (93 200 396) as at 59d7939, at 56 792 nodes/s instead of 52 522.

### Parallel search (`OPTI_THREADS`, Lichess 5000, 100 ms per puzzle)

| configuration | solved | aggregate nodes/s | vs 1 thread (McNemar) |
|---|---|---|---|
| 1 thread (1ec60fa) | 3636 (72.7 %) | 58 090 | - |
| 12 threads, separate trees, merge on summed visits (1ec60fa) | 3235 (64.7 %) | 524 203 | -401, chi2 298 |
| 12 threads, separate trees, merge also keeps proven mates (7cb5af9) | 3596 (71.9 %) | 525 307 | -40, chi2 8.5 |
| 12 threads, separate trees + per-thread gamma stagger | 3584 (71.7 %) | 531 576 | -52, chi2 13.3; **rejected** |
| 12 threads, shared tree (`OPTI_SHARED=1`) | died after 1279 puzzles, 109 `BUG: too many attacks` lines | - | data race |
| 12 threads, shared tree + `OPTI_GAMMA_STAGGER=1` | 3741 (74.8 %) | 368 011 | +105, chi2 11.7; not published (same race) |
| 12 threads, shared tree, after the `init_node` fix | access violation after 1390 puzzles (no `BUG:` line any more) | 365 199 (partial) | still racing |
| 12 threads, shared tree + stagger, after the `init_node` fix | access violation after 416 puzzles | 380 594 (partial) | still racing |

7cb5af9 fixes the merge: workers reported only visits, while the single-thread choice also plays a terminal child that mates (the UCT loop never visits terminal children). The search is deterministic, so 12 separate trees explore nearly the same lines and the 9x throughput does not turn into solved puzzles; only the shared tree does, and it is not safe yet. In NODES mode (`bench`) the iterations are split across threads, which shrinks every tree and makes nodes/s scaling meaningless, so it is not reported.

The `BUG: too many attacks` lines came from `Node::init_node`: it set `_initialized` before generating and sorting the moves, so another thread skipped the lock and read the board while `assign_move_flags` had a trial move applied in place. The flag is now published (release) at the end of the initialisation and read with acquire; single-thread output is unchanged (bench signature, NODES-500). The shared tree still crashes: evaluations and quiescence searches on existing shared nodes run without the node lock (`exploration_diag.cpp` quiescence entry and `evaluate_position`), so the shared mode stays off and its 3741 is not a published result.

### Before / after (same interleaved session, 5 rounds, 59d7939 vs 7cb5af9)

| | perft-suite full copy-make (Mnps) | perft-suite full bulk (Mnps) | bench 2000 (nodes/s) | perft | signature |
|---|---|---|---|---|---|
| 59d7939 | 57.07 | - | 46 951 | 7/7 | 8a8961478e14dd47 |
| 7cb5af9 | 65.26 (4/5 ahead; copy-make code untouched this session) | 223.05 | 52 321 (+11.4 %, 5/5 ahead) | 7/7 | 8a8961478e14dd47 |

Puzzles: TIME-100 3559 -> 3636 (+77, chi2 42.2, B-only 107 / A-only 30); NODES-500 4049 -> 4049 (identical).

## Repetitions under the DAG (2026-10-15, branch dev/poiroux/gui-audit, a328976)

Not a speed change: a search correctness fix (see the commit message). `opti_chess_cli search
100000 <fen>`, DAG on, value white POV, after 100k iterations:

| position | expected | before | after |
|---|---|---|---|
| Fine #70 `8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1` | win, only 1.Kb1 | +199, 1 182 boards (search spinning on cut cycles), Kb2 | +378, 500k boards, Kb2, the PV wins a5 |
| `8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7` | win | +140 | +559 |
| `6k1/8/7P/7K/8/8/8/8 w - - 3 72` | draw | +612 | 0 |
| `8/8/8/7k/8/qp6/p7/K5R1 w - - 0 1` | draw (desperado rook) | 0 | 0 |
| `rn2k1r1/pppRpRpp/bb6/q7/6n1/8/PPPPPPPP/1NB1KBN1 w - - 0 1` | draw (perpetual) | 0 | 0 |

Still open on Fine #70: the root plays 1.Kb2 (only 1.Kb1 wins) and the value drifts down once
the 500k-board arena is full (+635 at 40k iterations, +67 at 300k).

Non-regression: `Puzzle.LichessBenchmark5000` NODES-500 SEED-42 4052/5000 vs 4049/5000
(`build/bench_bin/nodes500_full.csv`), b=17 c=20, McNemar chi2=0.11 (not significant).
`bench 2000`: 832 192 nodes, new signature `e5cc2d99503b0425`, 56 044 nodes/s (single run, no
A/B; the speed comparison belongs to the optimisation pass).

### Pruning full arenas (same day)

Once an arena was full the search only refined the existing tree: values stopped improving
(Fine #70 +635 at 40k iterations, +67 at 300k). The search root now collapses the subtrees of
rarely visited nodes (visit threshold x4 until a quarter of the node arena is free); a collapsed
node keeps its value and can be expanded again. `OPTI_NO_PRUNE=1` restores refine-only.

`search 300000` Fine #70: with pruning the value reaches +1283..+1334 (PV queens the a-pawn)
between 120k and 210k iterations, then settles at +450 (root plays Ka2); without it, +67 (Kb2).
Pruning costs time once the arena is full (expansions instead of refines): 278 s vs 42 s for
the 300k iterations. Draws stay 0 (`6k1/8/7P/7K/8/8/8/8 w`, rook desperado, perpetual).
`bench 2000` and NODES-500 never fill the arena: signature `e5cc2d99503b0425` unchanged.

## Single-thread pass (2026-10-15, after the repetition fix)

### Diagnosis

Profile of `bench 2000` (VS Diagnostics CpuUsageHigh + xperf, `/Zi` + `/DEBUG` through `CL` /
`_LINK_`, 53 966 samples in the CLI module): `Node::quiescence` 90.6 % inclusive,
`Board::evaluate` 75.5 %, `get_king_safety` 25.8 %, `get_moves` 11.9 % (11.6 % self),
`get_checks_value` 11.9 %, `get_long_term_piece_mobility` 7.6 %, `get_queen_safety` 7.3 %,
`add_piece_controls` 5.9 %, `sort_moves` 5.5 %, `get_pawn_structure` 5.2 %, `assign_move_flags`
5.2 %, `move_may_give_check` 4.7 %, then about 15 eval terms at 2-4 % self each. No single
hot spot: the evaluation is a long list of array-scanning terms.

Quiescence census (`OPTI_QSTATS=1 opti_chess_cli bench 2000`, now printed by the bench):
874 990 entries, 818 612 full static evaluations (94 %); exits: stand-pat cutoff 246 337 (28 %),
cutoff after a move 247 196, full loop 247 045, depth exhausted 88 154, TT 44 516. Time: eval
12 s, node init (move generation, flags, sort) 2 s, of about 16 s. A bench "node" is one full
evaluation at about 15 us: 1M nodes/s needs about 1 us per node, out of reach of any tuning of
this evaluation (see the roadmap below).

### Steps (A/B `scripts/perf_compare.ps1`, 7 interleaved rounds, medians)

| change | bench nodes/s A -> B | perft Mnps A -> B | signature | verdict |
|---|---|---|---|---|
| `init_node` flagged every move twice (`assign_all_move_flags`, then again in `sort_moves`): one pass | 53 714 -> 55 088 (+2.6 %, 5/7) | 61.4 -> 63.1 (6/7) | unchanged | **kept** |
| `/arch:AVX2` | 54 113 -> 55 887 (+3.3 %, 5/7) | 60.1 -> 62.7 (5/7) | changed (FMA rounding) | no: noise level, would need the puzzle gate |
| defer move flags + sort in quiescence past the stand-pat / depth / TT exits | - | - | changed twice (the flag pass's `reset_eval` and trial moves feed the eval and the sort's raw `_adv`) | no: not exact |
| eval cache 2^17 -> 2^20 entries | 54 352 -> 54 746 (+0.7 %, 4/7) | 60.3 -> 60.4 | unchanged | no: noise, +40 MB |

Noise on this machine is about +-5 % per run; only steps that do strictly less work with an
unchanged signature are kept on a 5/7 majority.

### Roadmap towards 1M nodes/s (not started)

1. Attack maps from magic bitboards computed once per evaluation (both sides, x-rays), and every
   term (mobility, controls, king attackers/defenders, alignments, safe checks, queen safety)
   reading them instead of scanning `_array`. `get_checks_value` and `get_queen_safety` alone run
   up to four legal move generations per evaluation. Changes the evaluation: McNemar gate.
2. Lazy evaluation in quiescence: material + PST first, full evaluation only near the window
   (28 % of quiescence nodes cut on the stand pat).
3. Lighter nodes: `Board::_moves[100]` (300 B) and three `Evaluation` per `Node` dominate the
   arena entry; moves in a separate pool would at least double the arena capacity.
4. Incremental evaluation (or an NNUE-style network) once the terms are bitboard-based.

## Fine #70 ladder (2026-10-15/16, main)

Lasker-Reichhelm 1901, worked backwards from the end of the winning line (Tests.txt:1822-1825,
901-904). `opti_chess_cli solve <max> <moves> 50 <fen>`: first 500-iteration chunk from which the
most explored root move is an expected one with value >= +50, held until 100k iterations.

| step | FEN | expected | before (legacy backup, all quiet checks) | after (value propagation + quiet checks in the first 3 quiescence plies) |
|---|---|---|---|---|
| S1 | `8/8/3p1k2/p2P1p2/P2P1P1K/8/8/8 w - - 20 11` | Kh5 | 45 000 it, 5.5 s | 500 it, 16 ms |
| S2 | `8/4k3/3p4/p2P1p2/P2P1P2/6K1/8/8 w - - 18 10` | Kh4 | not in 100k (Kf2) | 500 it, 12 ms |
| S4 | `8/2k5/3p4/p2P1p2/P2P1P2/4K3/8/8 w - - 14 8` | Kf2/Kf3 | not in 100k | not in 100k (Kd3) |
| S5 | `8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7` | Ke3/Ke2 | not in 100k | not in 100k (Kd2); tree mode 11 000 it |
| S6 | `8/2k5/3p4/p2P1p2/P2P1P2/8/3K4/8 w - - 10 6` | Kd3 | not in 100k | not in 100k |
| S7 | `8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1` | Kb1 | not in 100k (Kb2) | Kb1 from 13 500 it (1.7 s) at 20k, Kb2 again at 100k |

`Endgame.Fine70LadderEasySteps` checks S1 and S2 within 2000 iterations.

Causes found on the way:
- The legacy backup ranks children by the softmax score (value and WDL mixed), not by value: under
  the DAG it made S2 take 87 500 iterations instead of 500 (`OPTI_VALUE_PROP=1`).
- Quiescence considered quiet checks at every depth: once queens appear it chained checks (up to
  68 quiescence nodes per iteration on S4, 1.36M entries for 20k iterations).
- Arena pruning reported success after freeing a handful of nodes and then walked the whole tree on
  every iteration; it also left cycles that no reset could reach, and `Node::reset` collected cycles
  only in the root's own arena (GUI: root in the main thread arena, tree built by the worker).

Gates (`Puzzle.LichessBenchmark5000`, SEED-42, McNemar vs the previous default):

| setting | NODES-500 | TIME-100 |
|---|---|---|
| previous default | 4052 | 3636 (night 4 reference) |
| value propagation | 4070 (b=61 c=79, chi2 2.1) | 3665 |
| quiet checks limited (qcheck 8) | 4016 (b=67 c=31, chi2 12.5, worse) | - |
| both (new default) | 4024 (b=109 c=81, chi2 3.84) | **3846 (vs value propagation alone: b=94 c=275, chi2 87.8)** |

At equal nodes the check limit loses puzzles; at equal time it wins clearly (40 % less work per
node). Known cost: the Lasker mate `rn3rk1/pbppq1pp/1p2pb2/4N2Q/3PN3/3B4/PPP2PPP/R3K2R w KQ - 0 1`
(Qxh7+, mate in 7 through quiet checks) needs about 100k iterations instead of 20k
(`Puzzle.Dxh7MateAnnounced` runs with `g_qcheck_min_depth = 0`). `bench 2000`: 701 866 nodes,
signature `469254f17b2357bb`.

### Same ladder with a classical alpha-beta + TT (`opti_chess_cli absolve`, experimental)

Iterative-deepening negamax, 4M-entry TT (TT move first, then captures), repetition on the path =
draw, evaluation = material + 6 cp per pawn rank. Independent of GrogrosZero.

| step | expected | first depth with the move | time | stable from |
|---|---|---|---|---|
| S1 | Kh5 | 5 | < 1 ms | depth 5 (+248..+260) |
| S2 | Kh4 | 7 | < 1 ms | depth 7 |
| S4 | Kf2/Kf3 | 11 (Kf2) | < 1 ms | depth 11, depth 30 in 0.5 s |
| S7 | **Kb1** | 21 | 9 ms | depth 25, 33 ms (+254); depth 34 in 1.3 s |

The transposition table collapses the king manoeuvres: depth 25+ in tens of milliseconds, where
the GrogrosZero tree does not hold 1.Kb1 after 100k iterations (about 40 s).
