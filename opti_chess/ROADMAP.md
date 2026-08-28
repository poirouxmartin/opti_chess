# ROADMAP — opti_chess Performance Optimization

## Vision

Make opti_chess strong with **very few nodes**. Like Lc0 or a grandmaster: look deep in interesting variations, discard all other moves quickly. Don't spend time on moves we know will be bad — while never missing a tactical opportunity.

## Current State

- **Search**: GrogrosZero (MCTS + alpha-beta + quiescence)
- **Eval**: Hand-crafted evaluation (symmetric, 2170+ positions validated)
- **Build**: C++20/MSVC/CMake/raylib GUI
- **Lichess 2000 benchmark (100ms/puzzle)**: 181/2000 (9.1%) baseline
- **With selective deepening**: 212/2000 (10.6%) = **+31 puzzles (+17%)**
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

### Search Improvements
- Quiescence check extension 0→1
- Winning-capture LMR exemption
- Move generation overflow fix, pawn push guard
- Repetition detection fixes (minimal_copy_data, copy_data, is_irreversible_move)
- MVV-LVA move ordering (Board::sort_moves)

### Selective Deepening (current best)
- Move 1: full depth, moves 2-5: depth 6, moves 6+: depth 3
- Result: 212/2000 (+31 puzzles), 66% nodes on best move
- **Root cause of 11 regressions**: robin_map iteration order is random, NOT quality-sorted. Moves explored last get depth 3 even if they're the best move.

## Phase 1: Eval-Ordered Children + Move Ordering (NEXT)

### Problem
Selective deepening works (212/2000) but has 11 regressions because it ranks children by **exploration order** (random robin_map iteration), not by **move quality**.

### 1.1 Eval-Ordered Children
**Idea**: After all children are evaluated, sort them by static eval from the moving side's perspective. The top-N children get full depth.

**Implementation**:
- In `explore_new_move`, after evaluating child static eval, insert into a sorted structure
- When picking the next child to explore, prefer children with better eval
- This makes selective deepening quality-aware instead of order-dependent

**Expected gain**: Recover the 11 regressions while keeping the +31 improvements.

### 1.2 Dynamic Move Reordering
**Idea**: Once all children have been explored once, reorder them by their current eval (deep or static). The engine should dynamically shift exploration to the best moves.

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
- TT exists with `tt_main_search` and `tt_node_dag` flags (both OFF by default)
- Need to investigate: does it work? Why is it disabled?

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

## Phase 4: Neural Network Evaluation

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

## Phase 5: NPS Optimization

### Goal
Hot path optimization: make the core search loop as fast as possible.

### 5.1 Hot Path Analysis
- Profile `grogros_zero`, `explore_new_move`, `quiescence`
- Identify cache misses, branch mispredictions
- Optimize inner loops

### 5.2 Memory Layout
- Node struct alignment
- Child map (robin_map) vs sorted array
- Board representation efficiency

### 5.3 SIMD / Parallel
- Evaluate multiple positions simultaneously
- Batch NN inference
- Parallel tree search

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

## Rules

1. **No correctness regressions** — if a "fix" hurts performance, revert it
2. **One change at a time** — isolate each optimization
3. **Always run tests** — `opti_chess_tests.exe --gtest_filter="-*Debug*:*Perf*"`
4. **Benchmark before/after** — measure everything
5. **Commit incremental progress** — small commits, clear messages
6. **Performance first** — reduce nodes, skip useless branches

## Success Criteria

- [ ] 2000+ puzzle benchmark: 250+/2000 (12.5%+) solved at 100ms
- [ ] Node concentration: 80%+ on best move
- [ ] Wasted nodes: <10%
- [ ] TT working and providing measurable node savings
- [ ] NN evaluation integrated and improving strength
- [ ] NPS improved by 50%+
- [ ] Selfplay strength improvement
