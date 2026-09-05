# ROADMAP — opti_chess Performance Optimization

## Vision

Make opti_chess strong with **very few nodes**. Like Lc0 or a grandmaster: look deep in interesting variations, discard all other moves quickly. Don't spend time on moves we know will be bad — while never missing a tactical opportunity.

## IMMEDIATE ACTION ITEMS (2026-08-31)

**All must be done in order. Nothing skipped.**

### A1. Fix Quiescence/Evaluation Overwrite Bug (DONE)
- **Bug**: When a node is explored via DAG/TT from one path, its deep_eval gets set. When the same position is then explored via the main search path, the new quiescence result OVERWRITES the correct value already stored. E.g. Qxe5 gets correct -1505 from quiescence, then a separate exploration pass overwrites it with static 322.
- **Root cause**: `explore_new_move` re-ran quiescence on already-explored children with reduced depth (selective deepening depth 0), overwriting the correct value.
- **Fix v1** (83f0f29): Save/restore `_value` only — caused mate sign inversion bugs (only _value restored, _avg_score/_wdl inconsistent; comparison by "better for side-to-move" wrong for mates).
- **Fix v2** (b8703bd): Save full `Evaluation` struct + `_quiescence_depth`, compare by depth (deeper = more accurate), restore full struct.
- **Benchmark**: 1183/5000 at 116K NPS (cold CPU) vs 1207 baseline at 135K NPS → comparable after NPS normalization. Mate themes: +11, backRankMate: +9, fork: +4.

### A2. Audit exploration.cpp Features (DONE)
- exploration.cpp has `g_search_value_propagation=true`, `g_search_trust_prior=true`, `g_search_avg_cap=true` (all false in diag)
- All three tested individually on LichessBenchmark5000:
  - `g_search_value_propagation=true`: **992/5000 (-20%)** — negamax hygiene breaks puzzle scoring at 100ms
  - `g_search_trust_prior=true` + `g_search_avg_cap=true`: **1134/5000 (-3%)** — spreads exploration too much
- All three prevent the engine from concentrating on the right move quickly at 100ms
- May be useful at longer time controls or selfplay — keep as tunable flags
- Kept all OFF in exploration_diag.cpp (committed a9b4f4a)

### A3. Investigate 30K vs 130K NPS (GUI vs Tests) (DONE)
- Root cause: combination of thermal throttling (~2×), persistent tree depth (~2-3×), and DAG overhead (~1.5×)
- Not a code bug — inherent to the GUI's persistent-tree architecture

### A4. Harmonize GUI & Tests + Enable DAG (DONE)
- PuzzleRunner defaults changed: alpha=0.005 (was 0.00001), matching GUI
- DAG enabled in PuzzleRunner::run (`g_tt_node_dag=true`)
- NPS diagnostic removed from gui.cpp (temporary tool)
- **New baseline**: 1239/5000 (24.8%) at 133K NPS with DAG ON

### A5. Test 10,2,2 and 10,6,2 vs 10,2,0 (RE-GATED post-fix 2026-09-05)
- Pre-fix conclusion (keep 10,2,0) was CONTAMINATION ARTIFACT — overturned.
- NODES-500 post-fix: A(10,2,0)=304, B(10,2,2)=332 (+28, χ²=16.6),
  C(10,6,2)=353 (+49 vs A χ²=41.9, +21 vs B χ²=12.9). All pairwise significant.
- TIME-5000: C=3300 (66.0%) vs A=2973 (59.5%), net +327, χ²=170.0.
- Decision: **default is now 10,6,2** (TAIL 2, MID 6). Baseline: 3300/5000.
- Pre-fix history (contaminated, kept for traceability): 10,2,0 = 1142 vs
  10,2,2 = 1171 (n.s.); NODES A=426 B=400 C=418 (n.s.); baseline 1133/5000.

### Order & measurement rules (2026-09-05)
- Phases run in roadmap order, **NN (Phase 4) moved LAST**.
- **Every change is benchmarked** before/after. Node-efficiency work uses
  **fixed-NODES budgets** (`OPTI_PUZZLE_NODES` + `OPTI_SEED`, deterministic)
  as the primary gate; TIME 5000 confirms only on pass.

### External-truth discipline (post-Qe1 rule, 2026-09-05) — NEVER violate
The Qe1 incident (a whole theory built on unverified SF readings; 4 wasted
experiments; user caught it from the GUI in 1 minute) must not recur:
1. **Stockfish truth ONLY via `StockfishAdapter.analyze()`** (best_move +
   eval_cp + depth + raw PV). Raw `stockfish.exe` CLI pipes DROP info lines
   in this environment — bestmove-only readings are FORBIDDEN as evidence.
2. **Sign convention stated every time**: adapter `eval_cp` is
   SIDE-TO-MOVE-relative (it flips SF's white-relative score on `" b "`);
   our `_value` is WHITE-relative. Convert explicitly before comparing.
3. **A bestmove alone proves NOTHING** (losing positions have bestmoves).
   Require the SCORE. A static `eval` on a hand-made FEN is not a search
   truth — verify legality and sanity first.
4. **Material/common-sense gate**: if an external reading contradicts basic
   counting (hanging queen ⇒ losing, period), the READING is wrong. Stop,
   re-verify; never build on it.
5. **Position tracking**: every claim states FULL FEN + side to move. Never
   reason about "the position" across plies ambiguously (the pos1-vs-root
   drift cost another cycle).
6. **Two-source rule for mechanisms**: internal evidence (iters/values) +
   verified external truth. On conflict, re-verify the EXTERNAL reading
   first — the engine's numbers were right twice (Qe1, pos1).

### Key Discoveries (2026-08-31)
- `exploration.cpp` is NOT dead code — has unique features (`g_search_value_propagation`, `g_search_trust_prior`, `g_search_avg_cap`) that exploration_diag.cpp lacks
- `exploration_diag.cpp` was created at commit 7030754 as temp diagnostic copy; files drifted apart
- GUI and tests compile same source files (exploration_diag.cpp) but use DIFFERENT parameters
- Qxe5 bug: selective deepening gives qdepth=0 to tail moves, hiding tactics. Also: quiescence overwrite bug discovered — correct eval gets overwritten by stale exploration.
- NPS gap 130K (tests) vs 30K (GUI) is NOT just rendering — needs investigation

## Current State

- **Search**: GrogrosZero (MCTS + alpha-beta + quiescence) selective `10,2,0` (move1 full 10, moves2-5 depth2, rest 0 static-only); DAG node sharing ON
- **Eval**: Hand-crafted evaluation (symmetric, 2170+ positions validated)
- **Build**: C++20/MSVC/CMake/raylib GUI (`build/release/Release/opti_chess.exe`)
- **Repetition**: Stockfish-style twofold (twofold for non-root, threefold for root); dead quiescence rep check removed
- **Lichess 5000 benchmark (100ms/puzzle)**: `3761/5000 (75.2%)` selective 10,6,2 + eval-ordered tiers + DAG ON (2026-09-05, `bench_5000_F.csv`)
- **Lichess 2000 benchmark (100ms/puzzle)**: `538/2000 (26.9%)`
- **Node concentration**: 53% → 66% on best move with selective deepening
- **Selfplay**: alpha=0.005 gives +126 Elo vs GUI default

## Completed Work

### Infrastructure
- Puzzle suite: 5000 Lichess puzzles with 41 themes (`tests/lichess_5000.txt`)
- 2000-puzzle benchmark set (`tests/lichess_2000.txt`)
- 50k eval positions (`tests/lichess_evals.txt`)
- PuzzleDiagnostic test: per-move node distribution stats
- Stockfish adapter (PIMPL, eval flipping)
- `OPTI_TEST_SCALE`, `OPTI_ADAPTIVE`, `OPTI_SELECTIVE` env vars

### Bug Fixes
- 6 memory bugs (block_division OOB, pawns_black OOB, proven-win override, etc.)
- Eval symmetry validated (2170 corpus)
- FEN parser `case 'q'` truncation, castling order-independent
- eval_delta formula inversion (critical: was giving opposite sign)
- TT scale isolation, memory cap
- `get_winnable` OOB, `reset_bitboard` bound, `get_pawn_push_threats` OOB
- **Stack overflow**: `Node::reset(true)` converted from recursive to iterative worklist (exploration.cpp + exploration_diag.cpp)
- **`_saved_zobrist`**: Saved before `reset_board()` in `reset_node_fields()`, used by `recycle_detached_node()` for correct `node_map` erase
- **Board buffer exhaustion**: `reset_game()` frees old root board before allocating new one
- **Buffer sizing**: 500MB minimum floor in `compute_pool_sizing()` to prevent tiny buffers on battery

### Search Improvements
- Quiescence check extension 0→1
- Winning-capture LMR exemption
- Move generation overflow fix, pawn push guard
- Repetition detection fixes (minimal_copy_data, copy_data, is_irreversible_move)
- MVV-LVA move ordering (Board::sort_moves)

### Selective Deepening (current best `10,2,0`)
- Move1 `10` full, moves2-5 `2`, rest `0` static-only
- Result: `251/2000 (12.6%)` vs `181` baseline; `10,2,2` `237`, `10,6,0` `240` — even depths only
- **Root cause of regressions**: `board.cpp:1707` `sort_moves` MVV-LVA + `pen` puts best tail (e.g. `after Bc4 Qe2 static -1195` `score -31500 oc1`) last → gets `0`. `Qe2` hallucination at `depth0` (looks winning) vs `depth2` (refuted by `Bxe2`) explains `10,2,0` vs `10,2,2` gap
- **Puzzle reward**: variable `0.5` luck (move correct but eval says losing) vs `1.0` true find (`puzzle.cpp` deep eval sign check)

## Phase 1: Eval-Ordered Children + Move Ordering (NEXT)

### Problem
Selective deepening works (212/2000) but has 11 regressions because it ranks children by **exploration order** (random robin_map iteration), not by **move quality**.

### 1.1 Eval-Ordered Children — ACCEPTED post-fix (2026-09-05)
Pre-fix rejection was contamination artifact. Re-gated under 10,6,2:
NODES-500 +19 (χ²=9.8), TIME-5000 3369 (67.4%) vs 3300 (+69, χ²=14.2).
With tail=2 (not 0) the sac demotion costs less, and rank-0 full depth on
statically-best moves wins more than it loses. Now unconditional (no flag).

**Implementation**:
- In `explore_new_move`, after evaluating child static eval, insert into a sorted structure
- When picking the next child to explore, prefer children with better eval
- This makes selective deepening quality-aware instead of order-dependent

**Expected gain**: Recover the 11 regressions while keeping the +31 improvements.

### 1.2 Dynamic Reordering — ALREADY EXISTS (2026-09-05)
`pick_random_child` already concentrates refinement by deep eval
(`get_move_scores` + UCT exploration term + top-5 rank boost). No duplicate built.

### Gamma sweep (2026-09-05) — REJECTED again post-fix, `OPTI_GAMMA` hook kept
Pre-fix TIME signal did not survive: NODES-500 γ=2.00 vs 1.10 → 401 vs 403,
net -2, χ²=0.03 n.s. Global exploration pressure reallocates without gain.

### A2 re-gate post-fix (2026-09-05) — rejections CONFIRMED, hooks kept
NODES-500: value_prop 397 vs 403 (-6, χ²=1.6 n.s. — the -20% was
contamination-amplified); trust_prior+avg_cap 401 vs 403 (χ²=0.03 n.s.).
All OFF. Env hooks (`OPTI_VALUE_PROP`, `OPTI_TRUST_PRIOR`, `OPTI_AVG_CAP`)
committed for future sweeps.
Trust-prior v2 (blend _value toward static too): 367 vs 403, χ²=16.6
SIGNIFICANT REGRESSION — scheduling follows static until ~4000 visits,
neutering search feedback. Reverted to avg-only (neutral).

### Qe1-disease: RETRACTED 2026-09-05 — false premise, do not use
User (GUI) correction: Qe1 LOSES immediately (Rxe1+ wins the queen), Kh2 is
the only move — the engine's -1983 vs -173 was CORRECT. The author's SF
readings supporting Qe1-good were unverified garbage (bare bestmove +
hand-made FEN static). The 4 rejected experiments stay rejected, but the
"defense starvation" mechanism is UNPROVEN — do not build on it.
Follow-up on the SAME line with VERIFIED method (adapter + raw PV):
- ROOT `3r2k1/pp4bp/4qpp1/3Pp3/8/4Q2P/4B1P1/2rR3K w`: SF-D32 +285 (dxe6),
  we play dxe6 but value it -173. Gap ~450cp stands, both numbers verified.
- SF is depth-UNSTABLE here: root-D32 +285 vs pos1-D34 -297 on overlapping
  lines (after-dxe6-D30: -287). Neither SF number is ground truth; the
  position is knife-edge. Our -173 sits inside SF's flap range.
- VERIFIED starvation (internal, no SF needed): post-Rd6 (white) Qb3/Qc5 at
  **1 iteration /2000** (SF-PV move unseen); e4 (+254, right direction) at 9
  iters vs Rcxd1+ (-173) at 1366; Rd6 (SF pos1 move) at 13 iters vs Rd4
  (502) — black's Rd4-over-Rd6 preference is a differential-refinement
  artifact. Single-file descent, everything else starved. Legitimate
  breadth-starvation (WAC lesson again), actionable without external truth.
- pos1-as-black-better is NOT a bug: SF-D34 -297 ≈ our static -256 (70cp).
  (User's pos1 eval complaint needs a GUI recheck — both engines agree.)
- No aliens: all 28 Kh2-node children legal, nodefens coherent, DAG off in
  direct calls (bench via run() may differ — note for autopsies).
- #229 CONCLUSION (user, 2026-09-05): hard 6-ply quiet sequence
  (dxe6 Rcxd1+ Bxd1 Rxd1+ Kh2 Rd6 Qb3! Kf8 Qb4! Re7 Qxb7+), not a shocking
  bug — engine evaluates the post-exchange position as losing for a long
  time and never plays Qb3. Not surprising at 2000 iters. DEPRIORITIZED:
  quiet-tactic depth is NN/move-ordering territory. Higher-yield work first
  (WDL sac-blindness: moves FOUND but scored low; theme clusters).

### Interleave — REJECTED (2026-09-05, code removed)
Root-level: 403 vs 403 bit-identical (fired never — `iteration_index` is
legacy dead, never incremented — first gate measured nothing). Fixed that,
re-gated at depth: forced least-visited refinement at ANY depth = 392 vs 403,
net -11, χ²=5.3 SIGNIFICANT REGRESSION (dilutes focus on hopeless lines).
UCT starving is globally right; the Qe1 disease needs a targeted cure
(min-refinement quota), not blind breadth.

### Pawn-break extension — REJECTED (2026-09-05, code removed)
NODES-500: 401 vs 403 (χ²=0.1 n.s.). Quiet 5th-rank pushes + terminating +1
change nothing on this subset. advancedPawn (57% fail) needs another angle.

**Implementation**:
- After all children are created, sort `_children` by eval
- `pick_random_child` should prefer children with better eval
- This is orthogonal to selective deepening and should stack

### 1.3 Move Ordering Improvements
- TT move first (if available)
- MVV-LVA for captures (already in Board::sort_moves)
- Killer moves / history heuristic
- Countermove heuristic

## Phase 2: Node Efficiency — Waste < 1%

### 2.0 Cross-puzzle contamination FIX (2026-09-05, commit f950412) — READ FIRST
Repeating one puzzle 16×/process flipped the answer (Bc7+ → d1=R → Bh8,
nodes 6026–17707): `get_first_free_node()` never reset eval state, so stale
`_deep_evaluation` + `_quiescence_depth` from prior puzzles was restored over
fresh quiescence results by the A1 overwrite-guard. **ALL benchmarks before
this fix are contaminated** (batch order/contents affected outcomes):
NODES-500 102 → 304 (χ²=165.6), CalibratedBatch 11 → 28 (stale ≥62 threshold,
already red). Baselines below marked [PRE-FIX] must be re-established.

### Goal
Currently ~44% of nodes go to "other" moves (not best, not expected). Want <1%.

### 2.1 Selective Deepening Refinement
- Tune tier thresholds (currently 1/5/3)
- Consider tier 0: moves that look terrible get 0 quiescence (depth 0 only)
- Per-theme analysis: which puzzle types benefit most

### 2.2 Quiescence Optimization
- Move ordering in quiescence (MVV-LVA threshold)
- SEE (Static Exchange Evaluation) for captures
- Delta pruning
- Late move reductions in quiescence

### 2.3 Main Search Pruning
- Null move pruning
- Late move reductions (LMR)
- Futility pruning
- Razoring

## Phase 3: Transposition Table Investigation

### Status
- DAG (`tt_node_dag`) enabled by default — +56 puzzles vs OFF, +15% NPS (1239/5000 at 133K)
- TT main search (`tt_main_search`) still OFF by default

### TODO
- [ ] Test with TT enabled (`_tt_main_search=true`)
- [ ] Test with DAG enabled (`_tt_node_dag=true`)
- [ ] Measure node savings
- [ ] Identify bugs if any
- [ ] If TT works: it should reduce nodes significantly in repetitive positions

### Key Questions
- Is TT storing/looking up correctly?
- Is Zobrist key collision an issue?
- Does DAG actually share nodes between transpositions?
- What's the overhead vs savings?

## Phase 4: Neural Network Evaluation (LAST — after Phases 5/6/7)

### Goal
Replace hand-crafted eval with a neural network. Add a **policy value** for each move (probability it's the best move).

### 4.1 Architecture
- **Value head**: Evaluate position (win/draw/loss probability)
- **Policy head**: For each legal move, probability it's the best move
- Input: board representation (piece-square tables or 119-plane encoding)
- Output: scalar value + policy vector

### 4.2 Training Data
- Use Stockfish to evaluate millions of positions
- Extract policy from Stockfish's move ordering (or from search tree)
- Self-play games for reinforcement learning

### 4.3 Benefits
- **Move ordering**: Policy gives instant ranking of all moves
- **Evaluation accuracy**: NN eval should be much better than hand-crafted
- **Node savings**: If policy is 90% accurate, we can skip 90% of moves immediately
- **Like Lc0**: Lc0 uses policy + value from neural network

### 4.4 Implementation Options
1. **ONNX Runtime**: Load pre-trained model, run inference in C++
2. **Custom inference**: Write NN inference in C++ (faster but more work)
3. **Hybrid**: Use NN for eval, keep hand-crafted for move ordering

### 4.5 Training Pipeline
1. Generate positions with Stockfish evaluations
2. Train value head (position evaluation)
3. Train policy head (move quality)
4. Export to ONNX
5. Integrate into opti_chess
6. Benchmark improvement

## Phase 5: UI Tunability (next)

- Sliders / textBoxes for `alpha, beta, gamma, quiescence_depth, selective tiers (10,2,0), _explore_checks, _tt_main_search, _tt_node_dag` in `gui.h:97` / `gui.cpp:1138`
- Live per-move `iters/nodes/static/deep` display already in `PuzzleDiagnostic` — expose in GUI arrows panel
- Presets: `10` baseline / `10,2,0` / `10,2,2` + variable-reward benchmark button

## Phase 6: Parallelisation

### Goal
Exploit multi-core CPUs to search more positions in the same wall-clock time.

### 6.1 Parallel Iterative Deepening
- Multiple threads search the same root with independent time budgets
- Best result from any thread is used; PV is merged
- Simple to implement, near-linear speedup with 2-4 threads

### 6.2 Root Parallelisation
- Each thread searches a different root move
- Best move across all threads is selected
- Ideal when move ordering is accurate (policy head gives good priors)

### 6.3 Lazy SMP (Long-term)
- Threads share a single TT (read/write with atomics or locks)
- Each thread explores independently but benefits from TT hits from other threads
- Stockfish-style: proven technique, ~3x speedup with 4 threads

### 6.4 SIMD Batch Evaluation
- Evaluate multiple positions simultaneously (SIMD lanes)
- Complements NN evaluation (batch inference)
- Useful for quiescence search where many positions are evaluated independently

### 6.5 Implementation Notes
- Use `std::thread` or thread pool (no OS-specific APIs)
- TT must be thread-safe (atomic writes or fine-grained locking)
- Node buffer allocation must be thread-local or partitioned
- Board buffer: each thread gets its own sub-range
- Benchmark: measure NPS scaling with 1/2/4/8 threads

## Phase 7: NPS Optimization

### Goal
Hot path optimization: make the core search loop as fast as possible.

### 7.1 Hot Path Analysis
- Profile `grogros_zero`, `explore_new_move`, `quiescence`
- Identify cache misses, branch mispredictions
- Optimize inner loops

### 7.2 Memory Layout
- Node struct alignment
- Child map (robin_map) vs sorted array
- Board representation efficiency

### 7.3 SIMD
- Evaluate multiple positions simultaneously (SIMD lanes)
- Batch NN inference

## Measurement

### Primary Metric
**Puzzles solved at 100ms/puzzle** on Lichess 2000 set
- Higher is better
- Per-theme breakdown
- Node distribution (% on best move)

### Secondary Metrics
- **Node concentration**: % of nodes on the best move (target: >80%)
- **Wasted nodes**: % on moves that aren't best or expected (target: <10%)
- **NPS**: Nodes per second (higher is better)
- **Selfplay strength**: Elo estimate

### A/B Testing Protocol
1. Run baseline on Lichess benchmark (record solved count + node distribution)
2. Make one change
3. Re-run benchmark
4. If solved count improves AND no regression on other tests: keep change
5. If regression: analyze root cause, fix or revert

## Roadmap (Current Priority Order)

### 1. GUI Crash & Random Position Investigation (DONE)
- **Stack overflow**: Fixed — `Node::reset(true)` converted to iterative worklist
- **Random position bug**: Corruption detector added to `draw()`. Need debug log to identify root cause.
- **Board buffer exhaustion**: Fixed — `reset_game()` now frees old root board before allocating new one
- **Node buffer leak**: Fixed — `recycle_tree()` walks reachable tree before `node_map.clear()`, freeing all nodes to buffer
- **Buffer sizing floor**: Fixed — 1GB minimum (up from 500MB)
- **Remaining**: Reproduce random position with corruption detector, identify root cause

### 2. Remove Fullscreen Default (DONE)
- Default window 1280x720 (was 1920x1080)
- `HideCursor()` commented out at startup

### 3. Twofold Repetition Fix (DONE)
- `g_search_root_key` set only when `g_dag_recursion_depth == 0` (Stockfish-style depth-0)
- `repetition_limit = 3` (FIDE threefold); twofold for non-root positions via `g_search_root_key` check
- Both `exploration.cpp` and `exploration_diag.cpp` aligned (commit d708f3c)
- Root position keeps threefold (opponent might have avoided it before search)
- `position_is_draw_by_repetition` expanded with explicit root-key check

### 4. Selective Deepening Re-evaluation
- `10,2,0` is current best. With bug fixes accumulated, re-test:
  - `10,2,2`: move1-5 full 10, moves6+ depth 2
  - `10,6,2`: move1-5 full 10, moves6-10 depth 6, rest depth 2
- Expected: some regressions fixed by accumulated improvements

### 5. Full Algorithmic Audit
- Search for implementation mistakes hiding in the code
- Systematic review of every search function
- Focus on correctness bugs, not performance

### 6. Node Usage / Wastage Optimisation
- Target: 90% of nodes spent on top move on average
- Target: <1% of nodes on worthless moves
- Current: ~66% on best, ~34% on other moves

### 7. TT + DAG Audit & Improvements
- Check code for mistakes in TT storage/lookup
- Analyse TT hit rate % and other stats
- DAG node sharing efficiency

### 8. CPU Hotpath Optimisation + RAM Usage
- Profile hot path, reduce cache misses
- Optimise inner loops
- Reduce memory footprint

### 9. Parallelisation
- Parallel iterative deepening (2-4 threads, near-linear speedup)
- Root parallelisation (one thread per root move)
- Lazy SMP with shared TT (long-term, ~3x speedup)
- SIMD batch evaluation for quiescence

### 10. Buffer Management Fix (DONE)
- **Buffer sizing floor**: 1GB minimum in `compute_pool_sizing()`
- **Old root board freed**: `reset_game()` frees before allocating new
- **DAG orphaned node leak fixed**: `recycle_tree()` walks reachable tree before `node_map.clear()` (commit 2955933)

## Rules

1. **No correctness regressions** — if a "fix" hurts performance, revert it
2. **One change at a time** — isolate each optimization
3. **Always run tests** — `opti_chess_tests.exe --gtest_filter="-*Debug*:*Perf*"`
4. **Benchmark before/after** — measure everything
5. **Commit incremental progress** — small commits, clear messages
6. **Performance first** — reduce nodes, skip useless branches

## Success Criteria

- [ ] 5000 puzzle benchmark: 4000+/5000 (80%+) solved at 100ms
- [ ] 2000 puzzle benchmark: 1600+/2000 (80%+) solved at 100ms
- [ ] Node concentration: 90%+ on best move
- [ ] Wasted nodes: <1%
- [ ] No GUI crashes during analysis + move playback
- [ ] TT working and providing measurable node savings
- [ ] NN evaluation integrated and improving strength
- [ ] NPS improved by 50%+
- [ ] Selfplay strength improvement
