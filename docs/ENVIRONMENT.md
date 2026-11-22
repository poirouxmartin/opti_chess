# Environment variables

The engine has no UCI options and no configuration file. Tuning, A/B switches and diagnostics go through environment variables. This list comes from a search for `getenv(` in `opti_chess/*.cpp`, `opti_chess/*.h` and `opti_chess/tests/*.cpp` (2026-10-26, 110 call sites, 12 files). Source files are named, not line numbers.

Conventions:

- "set" means the variable exists in the environment, with any value. "`=0`" means the first character is `0`.
- Most variables are read once per process (static initialisation or a function-local static). Variables read on every `PuzzleRunner::run` call are marked "per run". Set them before starting the process.
- Defaults are the values in the code, not recommendations. `TEMPORARY` marks variables whose code comment says "TEMPORARY" (to be removed once the investigation ends); `TEMP` marks one that the code calls a temporary test.
- Variables that change search or evaluation results change the bench signature (`opti_chess_cli bench`) and the puzzle numbers. Do not mix runs made with different settings.

## Search

| Variable | Read by | Default | Effect |
|---|---|---|---|
| `OPTI_SEED` | `zobrist.cpp` | unset: random per process | Seeds the Zobrist key generator (`atoi`), which makes node counts reproducible. `cli.cpp` and `selfplay.cpp` set it to `42` in `main` when it is unset; the GUI and the tests do not. |
| `OPTI_NO_DAG` | `cli.cpp` | unset: DAG on | Set: `search` and `solve` run the pure tree search. Not read by `PuzzleRunner::run`, which forces the DAG on for the length of the call: `bench`, UCI `go` and the puzzle tests always use the DAG. The GUI uses its own toggle (`O`). |
| `OPTI_DAG_EXACT_EVERY` | `exploration_diag.cpp` | `0` | Iterations between two exact refreshes of the DAG values (`refresh_graph_values`). `0` = `max(1024, reachable nodes / 32)`. The refresh runs in DAG mode only, not under the shared tree, plus once at the end of the outermost call. |
| `OPTI_TT_MAIN` | `cli.cpp` (`search`), `puzzle.cpp` (per run), `gui.cpp` | unset: off | Set: use the transposition table in the main search (`g_tt_main_search`). In the GUI it is applied at the start of an analysis and can be toggled with `I`. |
| `OPTI_NOTT` | `zobrist.cpp` | unset | Set: `TranspositionTable::probe` always misses (A/B isolation of the table). |
| `OPTI_VALUE_PROP` | `exploration_diag.cpp` | on | `=0` restores the legacy ranking (`get_best_score_move`) instead of the value-argmax backup. |
| `OPTI_TRUST_PRIOR` | `exploration_diag.cpp` | off | Set: Laplace blend of the WDL of under-refined subtrees. |
| `OPTI_AVG_CAP` | `exploration_diag.cpp` | off | Set: cap the suppression applied by the avg-score softmax. |
| `OPTI_STANDPAT_MODE` | `exploration_diag.cpp` | `0` | Stand-pat of a partially explored node in the backup. `0`: against the node's current backed-up value while some child is not fully explored. `1`: against the static evaluation, only while a legal move has no child. `2`: never. |
| `OPTI_SOLVER` | `exploration_diag.cpp` | off | `=1` enables the MCTS-Solver: results proven at the leaves propagate up and proven nodes below the root are no longer explored. Experimental. |
| `OPTI_QCHECK_MIN_DEPTH` | `exploration_diag.cpp` | `8` | Quiescence looks at quiet checks only while the remaining depth is at least this. `0` = at every depth. |
| `OPTI_CHECK_EXT` | `exploration_diag.cpp` | `0` | Plies added in quiescence to check-giving moves. `0` = off. |
| `OPTI_ADAPTIVE` | `exploration_diag.cpp` | off | Set: lower the quiescence depth of moves that look bad statically. |
| `OPTI_NO_SELECTIVE` | `exploration_diag.cpp` | selective deepening on | Set: disables selective deepening. When on, a new child is ranked by static evaluation against its explored siblings: with 5 or more better siblings it gets `OPTI_SEL_TAIL` as quiescence depth, with 1 to 4 it gets `min(depth, OPTI_SEL_MID)`. |
| `OPTI_SEL_TAIL` | `exploration_diag.cpp` | `10` | See `OPTI_NO_SELECTIVE`. |
| `OPTI_SEL_MID` | `exploration_diag.cpp` | `10` | See `OPTI_NO_SELECTIVE`. |
| `OPTI_FORCED_EVERY` | `exploration_diag.cpp` | `1 << 30` (off) | Every Nth refinement descends into the least-visited child (anti-starvation round-robin). |
| `OPTI_NO_PRUNE` | `exploration_diag.cpp` | pruning on | Set: when the arenas are full, do not free light subtrees; refine the existing tree only. |
| `OPTI_TIE_RANK` | `exploration_diag.cpp` | off | Set: tied move scores share the rank bonus of their rank, instead of ranks taken from the hash order of the children. |
| `OPTI_PRIOR_PASSER` | `exploration_diag.cpp` | `5.0` | Exploration prior for passed-pawn pushes, decaying with visits. `0` disables it. (The comment next to the code says "off by default"; the code default is 5.0, active.) |
| `OPTI_SCORE_FLOOR` | `exploration_diag.cpp` | `0.01` | When an arena is full, move scores are floored at this fraction of the best score. `0` restores the old numbers. |
| `OPTI_GAMMA` | `puzzle.cpp` (per run), `gui.cpp`, `tests/unit_test.cpp` (`Puzzle.PuzzleDiagnostic`) | `1.10` | Overrides gamma, the diversity of explored moves. |
| `OPTI_ALPHA` | `puzzle.cpp` (per run), `gui.cpp` | `0.005` | Overrides alpha, the weight of the evaluation in move selection. |
| `OPTI_VLOSS` | `puzzle.cpp` (per run), `gui.cpp` | `4` | Virtual loss: each in-flight descent counts as this many visits in `pick_random_child` (`g_virtual_loss`). |

## Evaluation

| Variable | Read by | Default | Effect |
|---|---|---|---|
| `OPTI_EVAL_CACHE` | `eval_core.cpp` | on | `=0` disables the per-thread evaluation cache (2^17 entries). |
| `OPTI_PAWN_STRUCT` | `eval_core.cpp` | unset: the `Evaluator` weight | Float. Replaces the weight of the whole pawn-structure component. |
| `OPTI_MOB_RELIEF` | `eval_mobility.cpp` | `0.5` | Float. Share of the mobility removed by the blocking discounts that is blended back. |
| `OPTI_KS_SAFE` | `eval_king.cpp` | `250` | Initial value of a safe check (integer, truncated). |
| `OPTI_KS_UNSAFE` | `eval_king.cpp` | `5` | Initial value of an unsafe check (integer, truncated). |
| `OPTI_KS_NOESC` | `eval_king.cpp` | `2.5` | Multiplier when the checked king has no escape. |
| `OPTI_KS_MATE1` | `eval_king.cpp` | `0.05` | Factor on the mate-threat subtotal when there is one mating move and the defender is to move. |
| `OPTI_KS_MATE2` | `eval_king.cpp` | `0.5` | Same, with two mating moves. |
| `OPTI_KS_SINGLE` | `eval_king.cpp` | `0.7` | Factor on a lone checking idea (no mate) when the defender is to move. |
| `OPTI_PP_CAND` | `eval_pawn.cpp` | `1.0` | Factor on candidate passers (local majority). |
| `OPTI_PP_CANDPOT` | `eval_pawn.cpp` | `0.2` | Threat floor of live candidates, as a fraction of the next-rank passer value. |
| `OPTI_PP_CANDMALUS` | `eval_pawn.cpp` | `25.0` | Extra malus for the cost of pushing a candidate. |
| `OPTI_PP_SCALE` | `eval_pawn.cpp` | `5.0` | Scale of the passer sub-component, compensating the x0.2 structure coefficient applied later. |
| `OPTI_PP_RACE` | `eval_pawn.cpp` | `1.0` | Factor on the promotion-race top-up. `0` disables the race. |
| `OPTI_PP_RACE_Q` | `eval_pawn.cpp` | `900.0` | Value the race top-up raises a passer's path value towards. |
| `OPTI_PP_RACE_M1` | `eval_pawn.cpp` | `0.25` | Fraction of the top-up when the race margin is 1. |
| `OPTI_PP_RACE_RW` | `eval_pawn.cpp` | `0.5` | Fraction of the top-up for a runaway passer. |
| `OPTI_PP_BLOCKDIV` | `eval_pawn.cpp` | `5.0` | Divisor of a passer's path value when a square on the path is a stop. |
| `OPTI_PP_TEMPO` | `eval_pawn.cpp` | `0.5` | Side-to-move tempo credit for runners, as a fraction of a square. |

## Threading and shared tree

| Variable | Read by | Default | Effect |
|---|---|---|---|
| `OPTI_THREADS` | `puzzle.cpp` (per run) | `1` | Number of search threads in `PuzzleRunner::run` (clamped to 1..64), so also for `bench`, UCI `go` and the puzzle tests. Without `OPTI_SHARED`, each thread builds its own tree and the root visits are summed. |
| `OPTI_SHARED` | `puzzle.cpp` (per run) | unset | Set, with `OPTI_THREADS` above 1: all threads refine one shared tree (node locks and atomics). Known data race, not counted in the benchmarks. |
| `OPTI_GAMMA_STAGGER` | `puzzle.cpp` (per run) | unset | Set, shared tree only: a different gamma per thread (factors 0.6 to 2.0 over 8 slots). |
| `OPTI_NOHEADSTART` | `puzzle.cpp` (per run) | unset | TEMPORARY. Set, shared tree only: skip the 1-iteration head start of the root. |
| `OPTI_MAIN_IDLE` | `puzzle.cpp` (per run) | unset | TEMPORARY. Set, shared tree only: the main thread does not search (one worker alone on the shared tree). |
| `OPTI_SERIALIZE` | `exploration_diag.cpp` | unset | TEMPORARY. Set: one global mutex serialises whole `grogros_zero` calls (probe for concurrency-induced corruption). |
| `OPTI_SERIALIZE_Q` | `exploration_diag.cpp` | unset | TEMPORARY. Set: same, for the quiescence calls too. |
| `OPTI_NOPLACEHOLDER` | `exploration_diag.cpp` | unset | TEMPORARY. Set, shared tree only: no placeholder insert when claiming a move (pure re-scan). |
| `OPTI_BOOST` | `main_gui.h` | boost on | GUI. `=0` starts without the process-priority boost (HIGH priority, HIGHEST worker threads). `Ctrl` + `B` toggles it at runtime. |

## Diagnostics and tracing

| Variable | Read by | Default | Effect |
|---|---|---|---|
| `OPTI_QSTATS` | `exploration_diag.cpp` | off | Set: counts the quiescence exit paths; printed by `dump_qstats()` (end of `solve`). |
| `SHARED_TRACE` | `exploration_diag.cpp` | off | Set: shared-tree loop trace on stderr. |
| `ENM_DEBUG` | `exploration_diag.cpp` | off | Set: dump on stderr when `explore_new_move` finds no unexplored move. |
| `ERC_DEBUG` | `exploration_diag.cpp` | off | Set: trace on stderr when a broken edge is skipped in `explore_random_child`. |
| `OPTI_KS_DETAIL` | `eval_king.cpp` | off | Set: adds check counts to the evaluation breakdown (`KSDETAIL` lines). |
| `OPTI_PP_DIAG` | `eval_pawn.cpp` | off | Set: adds the path value of every passer to the breakdown (`PPDIAG` lines). |
| `OPTI_PP_SQDBG` | `eval_pawn.cpp` | off | Set: adds the per-square base values of the path (`PPSQ` lines). |
| `OPTI_PP_RDIAG` | `eval_pawn.cpp` | off | Set: adds the promotion-race top-ups (`RDIAG` and `RDUEL` lines). |
| `OPTI_CANDDBG` | `eval_pawn.cpp` | off | Set: adds the candidate-passer analysis (`CAND` lines). |
| `OPTI_SEARCH_CENSUS` | `cli.cpp` (`search`) | off | `=1`: after the search, a census of the explored graph. |
| `OPTI_SEARCH_MINIMAX` | `cli.cpp` (`search`) | off | Set: recompute a plain minimax over the explored graph and compare it with the stored values. |
| `OPTI_SEARCH_COPIES` | `cli.cpp` (`search`) | unset | FEN. With `OPTI_SEARCH_MINIMAX`: lists every node of that position, at all plies. |
| `OPTI_SEARCH_DUMP` | `cli.cpp` (`search`) | off | Set: dump the root children and those of the most explored one. |
| `OPTI_SEARCH_LINE` | `cli.cpp` (`search`) | unset | Space-separated UCI moves. Walks that line in the explored graph and prints each node. |
| `OPTI_SOLVE_TRACE` | `cli.cpp` (`solve`) | off | Set: one `trace` line (best move, value) per 500-iteration chunk. |
| `OPTI_NO_BROWSER` | `main_gui.h` | off | GUI. Set: `Q` logs `[url] ...` instead of opening the browser (used by `scripts/gui_drive.ps1`). |

## Tests and benchmarks

All of these are read in `opti_chess/tests/unit_test.cpp`.

| Variable | Used by | Default | Effect |
|---|---|---|---|
| `OPTI_TEST_SCALE` | many search tests | `1` | Integer, at least 1. Divides the heavy search budgets (puzzles, prove-win ladder, determinism runs). `8` with `Debug.*` and `Perf.*` excluded is the fast smoke tier. |
| `OPTI_PUZZLE_NODES` | `Puzzle.LichessBenchmark5000` | `0` (TIME mode) | Node budget per puzzle. Above 0, runs `BudgetMode::NODES` instead of TIME. Deterministic with `OPTI_SEED`. |
| `OPTI_PUZZLE_TIME` | `Puzzle.LichessBenchmark5000`, `LichessBenchmark2000`, `PuzzleDiagnostic` | `0.1` | Seconds per puzzle in TIME mode. |
| `OPTI_PUZZLE_MAX` | same three tests | `5000`, `2000`, `50` | Maximum number of puzzles. |
| `OPTI_PUZZLE_SKIP` | `Puzzle.LichessBenchmark5000` | `0` | Skips the first N puzzles (targeted repro, resume after a hang). |
| `OPTI_BENCH_LOG` | `Puzzle.LichessBenchmark5000` | `bench_lichess5000.csv` | Path of the per-puzzle CSV log, flushed per line. |
| `OPTI_SELECTIVE` | `Puzzle.PuzzleDiagnostic` | unset | Only changes the mode label printed by the test (`SELECTIVE`, `HYBRID`). The engine does not read it; see `OPTI_NO_SELECTIVE`. |
| `PUZZLE_DEBUG` | puzzle helper in `unit_test.cpp` | off | Set: prints the root children ranked by visits with their values when a puzzle is checked. |
| `PUZZLE_BATCH_FILE` | `Puzzle.BatchValidate` | unset: test skipped | Path of a candidates file (FEN and SAN) to validate. |
| `OPTI_TEST_TRACE` | `Endgame.Fine70ExactDagSolvesLadder` | off | Set: trace output. |
| `OPTI_EVAL_DATASET` | `Puzzle.EvalAttribution`, `Puzzle.EvalQuietScreen` | unset | Path of an evaluation dataset. `EvalQuietScreen` is skipped without it; `EvalAttribution` falls back to live labels. |
| `OPTI_EVAL_DEPTH` | `Puzzle.EvalAttribution`, `Puzzle.EvalLabel` | `14`, `20` | Reference-engine depth; only values 8 to 40 are accepted. |
| `OPTI_EVAL_TOP` | `Puzzle.EvalAttribution` | `20` | Number of worst positions printed. |
| `OPTI_EVAL_REPORT` | `Puzzle.EvalAttribution` | unset | Path of a CSV report (FEN, bucket, ours, reference, error, gap). |
| `OPTI_EVAL_QGATE` | `Puzzle.EvalQuietScreen` | `60` | Quiet-position gate in cp (at least 5). |
| `OPTI_EVAL_QMAX` | `Puzzle.EvalQuietScreen` | `0` (all) | Cap on the number of rows. |
| `OPTI_EVAL_QDEPTH` | `Puzzle.EvalQuietScreen` | `10` | Quiescence depth used for the screen (at least 1). |
| `OPTI_EVAL_QSTRIDE` | `Puzzle.EvalQuietScreen` | `1` | Keeps one row in N (at least 1). |
| `OPTI_EVAL_QUIET_OUT` | `Puzzle.EvalQuietScreen` | `eval_quiet.csv` | Output CSV path. |
| `OPTI_EVAL_QVERBOSE` | `Puzzle.EvalQuietScreen` | off | Set: one trace line per row. |
| `OPTI_EVAL_FENS` | `Puzzle.EvalLabel` | unset: test skipped | Input file of FENs to label with Stockfish. |
| `OPTI_EVAL_OUT` | `Puzzle.EvalLabel` | `eval_labels.csv` | Output CSV path. |
| `OPTI_EVAL_STABLE` | `Puzzle.EvalLabel` | `60` | Stability gate in cp (at least 10). |
| `OPTI_EVAL_STATIC` | `Puzzle.EvalLabel` | off | Set, first character not `0`: static labelling without search. |
| `OPTI_EVAL_DUMP` | `Debug.DumpComponents` | unset: test skipped | Path of a FEN list whose evaluation breakdown is printed. |
| `OPTI_EVAL_DUMPN` | `Debug.DumpComponents` | `50` | Number of FENs dumped. |
| `OPTI_DUMP_MOVES` | `Debug.DumpComponents` | off | Set: also prints the move list of each FEN. |
| `OPTI_EVAL_MOBFEN` | `Debug.MobilityPerPiece` | unset: test skipped | TEMP. FEN whose per-piece mobility is printed. |
