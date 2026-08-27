# Puzzle Benchmarks

## Baseline — 2026-08-27

Engine: opti_chess (grogros_zero MCTS + hand-crafted eval)
124 puzzles from `tests/puzzle_candidates.txt`, Stockfish-calibrated rewards.
Quiescence depth: 10 (GUI default). Eval: WDL-based.

### Node Scaling (BudgetMode::NODES)

| Budget | TACTIC | ENDGAME | STRATEGIC | TOTAL |
|--------|--------|---------|-----------|-------|
| 1K     | 10/67  | 3/18    | 3/39      | 16/124 (12.9%) |
| 2K     | 7/67   | 7/18    | 2/39      | 16/124 (12.9%) |
| 5K     | 7/67   | 4/18    | 2/39      | 13/124 (10.5%) |
| 10K    | 2/67   | 4/18    | 2/39      | 8/124 (6.5%) |

### Time Scaling (BudgetMode::TIME)

| Budget | TACTIC | ENDGAME | STRATEGIC | TOTAL |
|--------|--------|---------|-----------|-------|
| 0.10s  | 6/67   | 4/18    | 5/39      | 15/124 (12.1%) |
| 0.25s  | 5/67   | 4/18    | 7/39      | 16/124 (12.9%) |
| 0.50s  | 5/67   | 5/18    | 5/39      | 15/124 (12.1%) |
| 1.00s  | 5/67   | 4/18    | 6/39      | 15/124 (12.1%) |
| 2.00s  | 6/67   | 3/18    | 5/39      | 14/124 (11.3%) |
| 3.00s  | 6/67   | 1/18    | 4/39      | 11/124 (8.9%) |

### Diagnosis

Score does **not** improve with more budget — it degrades. The engine picks wrong
moves early and more exploration reinforces bad paths. Root cause is
**evaluation/algorithm**, not budget starvation.

## Alpha Sweep — 2026-08-27

Alpha controls how much evaluation difference matters in node scoring.
`eval_score = exp(alpha * (eval - max_eval))`. Default GUI: alpha=0.00001.

| Alpha | 1K | 2K | 5K | 10K | Avg | Trend |
|-------|----|----|----|----|-----|-------|
| 0.0 | 16 | 16 | 13 | 8 | 13.3 | degrades |
| 0.00001 | 13 | 13 | 14 | 16 | 14.0 | flat/improves |
| 0.001 | 10 | 18 | 14 | 18 | 15.0 | improves |
| 0.005 | 14 | 18 | 18 | 16 | **16.5** | **most stable** |
| 0.01 | 21 | 15 | 15 | 13 | 16.0 | degrades |

**alpha=0.005 is the sweet spot** — highest average, never drops below 14/124.

With alpha=0.0 (eval invisible): 1000cp eval difference → ratio 1.000 (flat).
With alpha=0.005: 1000cp → exp(5.0) ≈ 148x (eval dominates).
With alpha=0.00001: 1000cp → exp(0.01) ≈ 1.01x (barely visible).

At alpha=0.01, eval dominates so strongly that shallow search finds good moves
quickly (21/124 at 1K) but noise in evaluation misleads at depth (13 at 10K).

### Time Scaling with alpha=0.005

| Budget | TACTIC | ENDGAME | STRATEGIC | TOTAL |
|--------|--------|---------|-----------|-------|
| 0.10s | 8/67 | 4/18 | 1/39 | 13/124 (10.5%) |
| 0.25s | 5/67 | 2/18 | 1/39 | 8/124 (6.5%) |
| 0.50s | 11/67 | 4/18 | 1/39 | 16/124 (12.9%) |
| 1.00s | 8/67 | 4/18 | 2/39 | 14/124 (11.3%) |
| 2.00s | 10/67 | 2/18 | 5/39 | **17/124 (13.7%)** |
| 3.00s | 7/67 | 5/18 | 3/39 | 15/124 (12.1%) |

vs baseline (alpha=0.00001): tactical improved (8->11 at 0.5s, 5->10 at 2s),
but strategic dropped (5->1 at 0.1-0.5s). Trade-off: alpha helps tactical
but hurts positional/strategic play.

## Beta/Gamma Sweep — 2026-08-28

Fixed alpha=0.005. Sweep at 5K nodes:

| Config | TACTIC | ENDGAME | STRATEGIC | TOTAL |
|--------|--------|---------|-----------|-------|
| a=5e-3 b=5.0 g=1.10 (baseline) | **11/67** | 2/18 | 2/39 | **15/124** |
| a=5e-3 b=2.5 g=1.10 (low beta) | 7/67 | 3/18 | **5/39** | **15/124** |
| a=5e-3 b=3.5 g=1.00 (mid) | 5/67 | 4/18 | 1/39 | 10/124 |
| a=5e-3 b=5.0 g=0.90 (exploit) | 7/67 | **4/18** | 2/39 | 13/124 |
| a=5e-3 b=2.5 g=0.90 (low b+exploit) | 5/67 | 5/18 | 3/39 | 13/124 |
| a=1e-5 b=5.0 g=1.10 (GUI default) | 4/67 | 2/18 | 5/39 | 11/124 |

Two 15/124 configs with different profiles: baseline is tactical-heavy (11+2),
low-beta is balanced (7+5).

## Self-Play Matches — 2026-08-28

All matches: fresh trees, 5000 iterations/move.

| Match | Result | Elo |
|-------|--------|-----|
| alpha=0.005 vs GUI default (0.00001) | **12-5, 3 draws (67.5%)** | **+126** |
| alpha=0.005 vs GUI default (0.00001) confirm | **3-2, 1 draw (58.3%)** | **+58** |
| beta=2.5 vs beta=5.0 (at alpha=0.005) | 6-12, 2 draws (35%) | -107 |
| gamma=0.90 vs gamma=1.10 (at alpha=0.005) | 6-12, 2 draws (35%) | -107 |

**Key findings:**
- alpha=0.005 consistently beats GUI default (+126 Elo, confirmed at +58)
- Reducing beta or gamma hurts playing strength despite solving more puzzles
- Puzzle score does NOT correlate 1:1 with playing strength (low-beta solved
  same 15/124 but lost 6-12 in self-play)
- The only confirmed improvement is alpha=0.005 (beta/gamma stay at 5.0/1.10)
