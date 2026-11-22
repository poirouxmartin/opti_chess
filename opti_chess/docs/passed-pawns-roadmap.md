# Passed pawns: formalisation + Grogros roadmap

Working document (Martin's papers formalised + prioritised roadmap).
Conventions: White perspective (symmetric for Black), values in cp.

---

## PART A: Passed pawn model

### A.1 Value per square `V(s)`

Let a passed pawn be on `s0`, with the squares in front of it `s1..sk` (up to promotion).
Enemy controls `C(s)` (with the value of the pieces), allied controls `F(s)`.

```
V(s) = V_base(rank)                       if no enemy control
V(s) = min(V_base,  min_{p ∈ C(s)} val(p)) if enemy controls
```

- `V_base(rank)`: per-rank table, in [400..800] (to calibrate, e.g.
  2nd→400, 3rd→450, 4th→550, 5th→650, 6th→750, 7th→800).
- Logic of the min: the cheapest blocker fixes the ceiling (we can always
  sacrifice it against the pawn at minimum).
- If enemy controls > allied controls on `s`: division per piece
  (current mechanism kept).
- If an allied pawn controls `s`: **the boost is skipped** (V_base applies).

### A.2 Blockade (piece on the route)

Weak passed pawn, **but the blocking piece is penalised too**:

```
V_passed_blocked = V_base × small-factor(blocker)
blocker_malus = f(piece)   // knight = excellent blocker (big malus
                           // for the opposing side = letting it go is costly),
                           // queen = very poor blocker (she must not
                           // babysit: deterrent malus on the queen)
```

In other words: the ideal blocker (knight) costs its side little for a
big effect; sticking one's queen there is punished. The malus depends on
the piece that blocks, not on the pawn.

### A.3 Protected / candidate passed pawns

- **Protected** (allied pawn controls its square or the square in front): no boost
  (A.1), standard connection bonus.
- **Candidate** (to define, open): pawn that WILL become passed if the majority
  advances or if the minority forces an exchange. Lead: bonus = V_base × 0.3 if
  the majority of the wing can force its creation (effective counts on the
  adjacent files), 0 otherwise. To be decided together.

### A.4 Global value: V0..V5

Question: `V = Vmin`, or `V = Vmin − k·dist`?

```
V = max_i ( Vi − k·i ),   k ≈ 75 (tunable)
```

- A pure `Vmin` has **no gradient**: nothing encourages advancing the pawn.
- `Vmin − k·dist` creates the incentive (each step brings it closer to the max) while
  keeping the weakest link as the ceiling. Recommended.
- Multi-square protected case: take the **min** (weakest link).

### A.5 Pawn chains

- **A-chain** (points toward the enemy, base behind): strong tip,
  **2 weaknesses** (holes on either side of the base).
  `score = +tip_bonus − 2 × flank_hole_malus`
- **V-chain** (point on one side, fan behind): **1 big central weakness**, but if it breaks, **the whole chain breaks on both sides** (fragility factor: if the tip is contested, the whole
  chain loses value).
  `score = +iron_bonus + fragility × central_malus`
- **Side-of-play rule**: play happens where the chain points.
  Proposed formalisation: vector base→tip (or shift of the pawns' centre of
  mass), `wing_focus` = projection on the wing; bonus
  `wing_focus × (space + activity)` on that side, symmetric, to test
  on the bank (correlate wing_focus with the gap per wing).

### A.6 Majorities / minorities

This is often what decides the endgame: **pushing one's majority** (it creates a
passer, cf. candidates A.3). Formalisation: majority score per wing (counts weighted by advancement) → candidate bonus (A.3) +
space bonus for the majority side. Minority: exchange lever creating a passer
(minority attack), same machinery, sign reversed on the weak wing.

---

## PART B: Grogros roadmap (prioritised)

Legend: [DONE] [IN PROGRESS] [NEXT] [LATER] [EXPERIMENTAL].
Rule: every change = benchmark before/after (eval + NODES + McNemar).

### P0: In progress
- [IN PROGRESS] **Passed pawns** (this document): tempo + square controls,
  minor-piece endgame extension, then A.1→A.6 in benchmarked pieces.

### P1: Method (infra already in place, to exploit)
- [DONE] Quiet banks + SF static/dynamic labels, attribution by gap,
  quietness screen, tuning without rebuild (`OPTI_KS_*`, `OPTI_MOB_RELIEF`).
- [NEXT] Plug each candidate A into this infra (pawn-flip slices,
  best-slice safeguard, NODES-500 + McNemar).

### P2: Research: Lc0-style selectivity
- [NEXT] Huge depth + critical selection: at a given depth,
  do not look at ALL moves; the main move at ~90%. Biggest force lever,
  but design first (micro benchmarks per testcase).
- [NEXT] Killer bias + threat detection in move ordering.
- [NEXT] Early quiescence cutoff that is not arbitrary: pre-eval ("−1500 →
  direct cut") rather than rank; `q_depth` main sufficient.
- [NEXT] Better sorting + prior (policy), micro testcases.

### P3: Transpositions + exactness
- [NEXT] Transposition audit: clean everything up + micro benchmarks
  (prove it works, not just that it does not crash).
- [NEXT] Parallelisation: to reconsider (mixed results).
- [NEXT] `game_over()` to optimise (already memoised, any juice left?).

### P4: Eval: endgames + win conditions
- [NEXT] Endgame: king proximity depending on passers, king+pawn waiting
  moves, win conditions (mating potential + passers).
- [NEXT] Counterplay = uncertainty+ (checks, attacks, imbalances,
  activity); rebalance open position (activity++/KS++) vs closed
  (positioning++/draw++): to benchmark and fine-tune.
- [NEXT] Overextension / V-structure (nothing exists, TODO placed).

### P5: Micro-perf (batch, when functionality demands it)
Cache L1>L2>L3 (compact structures), predictable `if`, compiler flags,
`noexcept` everywhere, `+ > - > x > /` (fast operations), `constexpr` →
`consteval`/`constinit`, `const&`, robin_hash everywhere, magic bitboards
EVERYWHERE + BB in the eval, `get_moves` (white/black separated) + sub-functions,
move/node buffers (nodes: done), compact structures, reuse of
variables between eval parameters, incremental evaluation, `position_history`
in robin_hash (no more vectors), cognitive complexity (warnings), `game_over`
opti, `opti get_moves`, `opti eval`, global profiling (hotpath mainly),
benchmarks eval/get_moves/sort/others. Boost: to evaluate (no dependency
without proof). Async UI: done (? to verify).

### P6: Uncertainty and long term
- [LATER] Uncertainty++ if the position is complex in calculation: to evaluate.
- [EXPERIMENTAL] Rollout/simulation for the long term.
- [EXPERIMENTAL] Compensation network: f(P) = delta(deep − static),
  trained on itself to predict its deep evaluation statically.

### P7: Tests (continuous)
- [DONE] CI gate (quick suites + GATE-500 threshold 400).
- [IN PROGRESS] Fegatello bug (quiescence blind to 4-ply compensations).
- Extend: every new mechanism = a dedicated non-regression test.

### P8: Noted ideas (not coded, to benchmark)
- [NOTED 2026-09-12] "Pawn to move": if the passer has the move, count its path from the square in front of it (push seen as acquired). Doubt: the
  push is already in the search tree (double count); only credit in static the calm advances beyond the horizon. Promotion EXCLUDED from the mechanism
  (separate about-to-move bonus, to specify). Caveat: uncertain, benchmark
  MAE + NODES before deciding.
- [IN PROGRESS 2026-09-15] Smart race (replaces the removed oos): v1
  arrivals in plies (2d−1 to move, 2d otherwise, no tempo under exclusion),
  certain = king outside the square with tempo + clear route + tenable square,
  winner-takes-all, margin 1 → half / ≥2 → full towards quasi-queen (900),
  capped at the end. No BFS (Chebyshev suffices), no distant interception
  (A.1, next step); knobs OPTI_PP_RACE / OPTI_PP_RACE_Q.
