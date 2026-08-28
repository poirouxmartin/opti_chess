# ROADMAP — opti_chess Performance Optimization

## Vision

Make opti_chess strong with **very few nodes**. Like Lc0 or a grandmaster: look deep in interesting variations, discard all other moves quickly. Don't spend time on moves we know will be bad — while never missing a tactical opportunity.

## Current State

- **Search**: GrogrosZero (MCTS + alpha-beta + quiescence)
- **Eval**: Hand-crafted evaluation (symmetric, 2170+ positions validated)
- **Lichess benchmark (100ms/puzzle)**: 144/1250 (11.5%)
- **Selfplay**: alpha=0.005 gives +126 Elo vs GUI default

## Phase 1: Puzzle & Eval Infrastructure (Current)

### Puzzle Suite (5000+ puzzles, all themes)
- [ ] Download 5000+ puzzles from Lichess with full theme coverage
- [ ] Themes: mate, mateIn1-5, fork, pin, skewer, discoveredAttack, deflection, attraction, clearance, interference, backRankMate, promotion, hangingPiece, trappedPiece, undefendedPiece, endgame, middlegame, opening, advantage, crushing, defensiveMove, sacrifice,promotion, castling, enPassant, master
- [ ] Per-theme breakdown in benchmark
- [ ] TIME mode (100ms/puzzle) as standard A/B metric

### Eval Test Suite (50k positions from Lichess)
- [ ] Download 50,000 evaluated positions from Lichess eval database
- [ ] Format: FEN + Stockfish evaluation (cp or mate)
- [ ] Static eval test (fast, ~microseconds per position)
- [ ] Themed breakdown (opening/middlegame/endgame)
- [ ] Use for eval tuning and regression detection

## Phase 2: Micro-Performance — Fewer Nodes, Same Strength

### Goal
Solve the same puzzles with **fewer nodes**. A position that takes 10k nodes should take 2k.

### 2.1 Quiescence Optimization (HUGE potential)
**Problem**: Quiescence is super heavy. We check ALL moves at each depth with big quiescence, which is slow and wastes nodes on moves we know will be bad.

**Ideas**:
1. **Move ordering in quiescence**: Only search captures that could possibly improve the position (MVV-LVA threshold)
2. **Shallow pre-screening**: Do a fast shallow quiescence for each move, discard most, then do deeper quiescence only on promising candidates
3. **Late move reductions in quiescence**: Already have `move_index * 2` — tune this more aggressively
4. **Capture SEE (Static Exchange Evaluation)**: Skip obviously losing captures early
5. **Delta pruning**: If capture gain < alpha, skip

### 2.2 Main Search — Skip Useless Branches
**Problem**: We explore all moves roughly equally. An efficient engine prunes 90% of moves instantly.

**Ideas**:
1. **Null move pruning**: If doing nothing still beats alpha, skip this branch
2. **Late move reductions (LMR)**: Reduce depth for moves that are unlikely to be good
3. **Futility pruning**: If eval + margin < alpha, skip quiet moves
4. **Razoring**: At low depths, if eval is way below alpha, go straight to quiescence
5. **History heuristic**: Track which moves were good in similar positions, search them first

### 2.3 Move Ordering — Search Best Moves First
**Problem**: If we search the best move first, we get cutoffs early and save nodes.

**Ideas**:
1. **TT move**: If we have a transposition table hit, search that move first
2. **Killer moves**: Track 2 quiet moves that caused cutoffs at this depth
3. **History heuristic**: Track move quality across the game
4. **MVV-LVA**: Most Valuable Victim - Least Valuable Attacker for captures
5. **Countermove heuristic**: The move that refuted the previous move

### 2.4 Node Budget Management
**Problem**: Currently we spend equal time on all positions. Should spend more on complex positions, less on simple ones.

**Ideas**:
1. **Aspiration windows**: Start with narrow window, widen if needed
2. **Iterative deepening**: Search depth 1, then 2, etc. — stop when time is up
3. **Time management**: Spend more time on complex positions
4. **Node limit per move**: If a move is clearly bad, don't spend many nodes on it

## Phase 3: Eval Improvements

### Goal
Better evaluation = fewer nodes needed (engine sees deeper with same node count).

### 3.1 Eval Tuning
- [ ] Use 50k Lichess positions to tune eval weights
- [ ] Test against known eval sets (WAC, Positional Suites)
- [ ] Ensure eval is monotonic (better position = higher eval)

### 3.2 Eval Features to Add
- [ ] King safety improvements (pawn storm, open files near king)
- [ ] Passed pawn evaluation (connected, protected, unstoppable)
- [ ] Bishop pair bonus
- [ ] Rook on open/semi-open files
- [ ] Outpost knights
- [ ] Space advantage

## Phase 4: Transpositions (DAG)

### Goal
Reuse analysis across different move orders. Same position reached via different move sequences should share nodes.

### Status
- DAG was attempted but needs investigation
- If it works: huge win for openings and endgames
- Can easily reduce node count by 2-5x in some positions

### TODO
- [ ] Investigate current DAG implementation
- [ ] Fix any issues
- [ ] Benchmark impact on puzzles and selfplay
- [ ] Consider Zobrist hashing improvements

## Phase 5: Advanced Techniques

### 5.1 Aspiration Windows
- Start with narrow eval window
- Widen if search fails high/low
- Saves nodes when we have a good eval estimate

### 5.2 Singular Extensions
- If one move is clearly best, extend its search
- Helps find tactical shots in complex positions

### 5.3 Multi-Cut
- If many moves beat beta, cut immediately
- Saves time in clearly good positions

### 5.4 Countermove Heuristic
- Track what move refuted the previous move
- Use it for move ordering

## Measurement

### Primary Metric
**Puzzles solved at 100ms/puzzle** on Lichess 5000+ set
- Higher is better
- Per-theme breakdown
- Must be reproducible

### Secondary Metrics
- **Nodes per puzzle** (lower is better)
- **Time per puzzle** (should be ~100ms)
- **Depth reached** (deeper is better, but not at expense of accuracy)
- **Selfplay strength** (Elo estimate)

### A/B Testing Protocol
1. Run baseline on Lichess benchmark (record solved count)
2. Make one change
3. Re-run benchmark
4. If solved count improves AND no regression on other tests: keep change
5. If regression: revert

## Rules

1. **No correctness regressions** — if a "fix" hurts performance, revert it
2. **One change at a time** — isolate each optimization
3. **Always run tests** — `opti_chess_tests.exe --gtest_filter="-*Debug*:*Perf*"`
4. **Benchmark before/after** — measure everything
5. **Commit incremental progress** — small commits, clear messages

## Timeline

- **Week 1-2**: Puzzle + eval infrastructure (5000 puzzles, 50k evals)
- **Week 3-4**: Quiescence optimization (biggest win potential)
- **Week 5-6**: Main search pruning (null move, LMR, futility)
- **Week 7-8**: Move ordering (TT, killers, history)
- **Week 9-10**: Eval tuning with 50k positions
- **Week 11-12**: Transposition table / DAG
- **Week 13+**: Advanced techniques (aspiration, singular extensions)

## Success Criteria

- [ ] 5000+ puzzle benchmark with all themes
- [ ] 50k eval positions for testing
- [ ] Solve 50%+ puzzles at 100ms (currently 11.5%)
- [ ] Reduce nodes per puzzle by 50%+
- [ ] No regression on existing tests
- [ ] Selfplay strength improvement
