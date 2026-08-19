# OptiChess — Algorithmic bug ledger

> Internal working file (search / TT / repetitions / evaluation).
> Updated as sessions go. See also `ALGORITHMS.md` (technical reference).
>
> Status legend: ✅ fixed · 🔧 in progress · ⬜ open · 🔍 to verify

---

## ✅ Fixed

### #1 — TT cutoff left `_deep_evaluation` inconsistent
- **Status**: ✅ fixed — **validated at runtime** (by the user, on `r1b3k1/p4ppp/5n2/1Rp4q/4B3/2P5/P1PQ2PP/4R1K1 b - - 3 21`, Ba6).
- **File**: `exploration.cpp` — `if (tt_cutoff)` block.
- **Symptom**: (a) "ghost" bug — a -M1 move (the worst one) ranked as the principal variation; (b) evaluation reported mate while `score`/`confidence` were not 0/1 (e.g. M4 displayed with score 0.934, confidence 87%).
- **Cause**: on a TT cutoff, only `_deep_evaluation._value` was overwritten; `_wdl`, `_avg_score`, `_uncertainty` and `_winnable_*` still held the values of the **static** evaluation. `get_node_score` combines those fields, hence the wrong ranking. `get_WDL` re-mixes `_value` with `_uncertainty`/`_winnable_*` (`board.cpp:10061-10070`), giving `avg_score = win·(1−U)+… ≈ 0.913+0.022 = 0.935` and `confidence = 100−100·U = 87%` — reproducing the observed numbers exactly.
- **Fix v1 (incomplete)**: recompute `_wdl`, then `_avg_score`, after the cutoff. This fixed (a) but **not** (b): `get_WDL` kept re-mixing with the static `_uncertainty`/`_winnable_*`.
- **Fix v2 (complete)**: before calling `get_WDL()`, if `_value` crosses the mate threshold (`10*abs > mate_value`, the `is_eval_mate` idiom), force `_uncertainty = 0` and set `_winnable_white/black` from the sign — exactly like the terminal path at `board.cpp:1575-1577`. Covers all three EXACT/BETA/ALPHA branches. Outside the evaluation hot path, so performance-neutral.
- **User confirmation**: "this seems to have fixed the bug".

### #2 — `get_zobrist_key()` copied the whole Zobrist table on every call
- **Status**: ✅ fixed — validated at runtime (commit `2026018`).
- **Fix**: `const Zobrist& zobrist = transposition_table._zobrist;` (a reference instead of a ~6 KB copy per call); keys are generated once at startup, with an idempotent guard that copies nothing. A performance win, but **not** the cause of #12 (hypothesis disproved by a runtime test).

### #12 — Regression: loading a FEN was slow, and sometimes never finished
- **Status**: ✅ fixed — validated at runtime (2026-05-16, commit `d7a3554`).
- **Root cause**: `GUI::reset_buffers()` swept 5M `Board` + 10M `Node` entries (each `reset` performing a `robin_map::clear()` since `a258fb5`) on **every** `load_FEN`, i.e. O(capacity).
- **Fix**: `reset_buffers()` now only calls `transposition_table.clear()`; O(used) reclamation is still guaranteed by the recursive `_root_exploration_node->reset()` that was already in place. FEN loading is instantaneous again, including after a search. PERFT 1/2 unchanged.

### #13 — System-wide freeze while GrogrosChess was running
- **Status**: ✅ fixed — validated at runtime by the user (2026-05-16, branch `fix/buffer-memory-perf`, PERFT 1/2 unchanged, no freeze).
- **Root cause**: gigantic preallocated buffers (`new Node[10M]` + `new Board[5M]` + TT, several GB) combined with **O(n)** allocation (a linear scan on every `get_first_free_*`), causing disk thrashing and O(n²) CPU.
- **Fix** (plan `docs/superpowers/plans/2026-05-16-optichess-memory-perf.md`; commits `82c3b9f` `bf048ec` `3d2db54` `2081e10`):
  1. **O(1) free list** (a stack of free indices) replacing the O(n) scan; each `Board`/`Node` carries its own `_buffer_index`; recycling follows "approach B" (only the children detached during recursion are pushed back — never the node reset in place).
  2. **Adaptive memory sizing** derived from the available physical RAM (`compute_pool_sizing`, budget = min(0.5×RAM, 4 GB) / RSS overhead factor) — replacing the magic constants 1E7/5E6; RSS is bounded by construction.
  3. **Full buffer → clean refinement** of the existing tree (the `can_expand` gate in `grogros_zero`) instead of a busy spin; the per-iteration `cout` was removed.
- **⚠️ Known residual (non-blocking)**: some console spam remains in the buffer-full condition, coming from a source other than the `cout` calls removed in T5; the regression gate (PERFT 1/2 + EVALUATION stable) still passes. To be tracked separately.

---

## ⬜ Open — by severity

### ✅ #3 — Mate scores not ply-normalised in the TT
- **Status**: ✅ fixed — **validated at runtime by the user**.
- **Files**: encoding at `board.cpp:1569` `(-mate_value + _moves_count·mate_ply)·get_color()`; decoding at `board.cpp:3301` `is_eval_mate` (which uses the `_moves_count` of the current board); Zobrist key without `_moves_count`. TT stores at `exploration.cpp:844/852/875/1053/1079`, probe consumed at `:784`.
- **Severity**: HIGH (correctness — the cause of the residual mate-distance "ghosts").
- **Detail**: the magnitude of a mate score encodes the `_moves_count` of the *terminal* node; `is_eval_mate` decodes it with the `_moves_count` of the *current* node, giving the distance `D = mc_terminal − mc_node`. The scheme is consistent **along a single path** (mc grows monotonically). But the Zobrist key excludes `_moves_count`: the same position reached through two paths of different lengths (e.g. Ng1-f3-g1) shares one key, so a mate stored via path A (mc_A) and read back via path B (mc_B≠mc_A) decodes a distance that is wrong by `mc_A−mc_B` plies — a distance "ghost", now displayed *confidently* since #1 v2 removed the visual tell.
- **Fix applied**: ply-relative canonicalisation. On store, `tt_normalize_mate(eval, mc) = eval + sign(eval)·mc·mate_ply` when the score is a mate (threshold `10·|e|>mate_value`, the `is_eval_mate`/#1 idiom), so `|stored| = mate_value − D·mate_ply`, independent of `_moves_count` (a property of the position). On probe, `tt_denormalize_mate(stored, mc_probe) = stored − sign·mc_probe·mate_ply` reconstructs a value consistent with the `_moves_count` of the node reading it (the identity when the path is the same). Helpers live in an anonymous namespace in `exploration.cpp`; the TT entry itself is unchanged (no mutation of the shared `ZobristEntry`, de-normalisation happens on the local copy `tt_eval`). Non-mate scores: a no-op by threshold, performance-neutral.
- **Threshold robustness**: `|stored| ≈ mate_value(1e8) − D·mate_ply + mc·mate_ply`, where `D` and `mc` are small (hundreds) against `mate_ply=1e5`, so `10·|stored| > mate_value` still holds (a mate is re-detected on probe) and non-mate evaluations (≪1e7) are never reclassified — consistent with `is_eval_mate` and #1.

### ✅ #4 — Stand-pat stored as `TT_EXACT`
- **Status**: ✅ fixed — **validated at runtime by the user** (commit `783135d`).
- **Files (line numbers as fixed — the old document had drifted with later commits)**: `stand_pat` stores that were `TT_EXACT` are now at `exploration.cpp:811` (emergency cutoff) and `:819` (depth≤0); in the post-loop at `:1033-1034`, `stand_pat` raises `alpha` at `:856-858` and then flags `TT_EXACT` if no child beats the floor. Probing and flag consumption are centralised at `:764-804`.
- **Severity**: MEDIUM (soundness; a blocker for #11 plan A).
- **Detail**: `stand_pat` is a **lower bound** on the true value (the side to move can always decline the captures, and the selective, pruned capture loop never proves it exhaustive). Stored as `TT_EXACT`, the probe cuts **unconditionally** (window-independent, `:768`/`:801`) and returns a **static** evaluation as if it had been searched — producing false cutoffs that hide tactics, and propagating a static value into #11 plan A.
- **Fix applied**: a dedicated `TT_STANDPAT` flag (`zobrist.h:28`), consumed **as a lower bound** (i.e. like `TT_BETA`: cut only if `tt_eval >= beta`, returning `beta`). The stores at `:811`/`:819` now use `TT_STANDPAT`; at `:1033`, if `alpha > original_alpha` **and** `alpha == stand_pat`, the entry is flagged `TT_STANDPAT` (otherwise `TT_ALPHA`/`TT_EXACT` are unchanged). An exact tie between a child and `stand_pat` is rare, so degrading `TT_EXACT`→`TT_STANDPAT` there is conservative and always sound.
- **Rejected alternative**: "flag it `TT_ALPHA`" (the initial proposal) is **incorrect** — `TT_ALPHA` is an *upper* bound (`zobrist.h:26`) whereas `stand_pat` is a *lower* one; it would have introduced false fail-low cutoffs (hiding tactics in the opposite direction). `TT_STANDPAT`/`TT_BETA` is the correct semantics.

### 🟠 #5 — `TT_BETA` stores `beta` instead of `score`
- **Status**: ✅ fixed
- **Files**: `exploration.cpp:1335,1519`
- **Severity**: MEDIUM (loss of precision, loss of mate information)
- **Detail**: on a beta cutoff we store `beta`; a mate `score` (≫ beta) is overwritten, leaving a lower bound that is too loose, so later probes miss the mate.
- **Fix**: store `score` (the true value, ≥ beta) rather than `beta`.

### 🟠 #6 — TT not cleared between distinct root searches
- **Files**: scattered clears (`gui.cpp:962,1910`, `game_tree.cpp:95`, `main_gui.h:414`, `board.cpp:10184`), none systematic
- **Severity**: MEDIUM (contributes to the "ghost" bugs)
- **Proposed fix**: clear (or age / use generations) at the start of every search from a new root, unless incremental reuse is deliberate.

### 🟠 #7 — `make_child_path_history` copies the whole map per child
- **File**: `exploration.cpp:33-38`
- **Severity**: MEDIUM (hot-path performance)
- **Detail**: the repetition `robin_map` is copied at every reversible node.
- **Proposed fix**: push/pop on a stack with undo (on entering and leaving the recursion) instead of copies; or a lighter structure (a vector of keys plus the last irreversible ply).

### 🟡 #8 — `Evaluation::operator<` has its branches inverted
- **Status**: ✅ fixed
- **File**: `board.h:530-538`
- **Severity**: LOW (latent — the `pick_random_child:1158` usage neutralises it)
- **Detail**: the `!_evaluated` branches were identical to those of `operator>` instead of being inverted.
- **Fix**: swapped the branches so that an unevaluated `this` is considered less than any evaluated peer.

### 🔴 #11 — Architectural limitation: the TT yields no depth gain
- **Files**: `exploration.cpp:739` (the only probe, inside `quiescence`), `:770,778,801,982,996` (the only stores); `:272-273` (node sharing explicitly removed)
- **Severity**: HIGH (this is the original point of transpositions — currently not achieved)
- **Symptom**: on a blocked king-and-pawn endgame, the expectation was roughly 8→30+ plies of depth for the same time budget (cf. Sebastian Lague's video); observed: essentially no gain.
- **Evidence (TT statistics, blocked king-and-pawn position)**:
  ```
  1.3k entries | probes 812k | hits 810k (99%) | cutoffs 810k (99%) | stores 1.4k | overwrites 43
  ```
  i.e. ~1300 unique nodes revisited ~625 times each. The combinatorial redundancy of pawn endgames is untouched.
- **Cause**:
  1. The TT is only used inside `quiescence()`, never in the main search (`grogros_zero`/`explore_new_move`/`explore_random_child` neither probe nor store). It caches the **leaf** evaluation, not the subtree.
  2. The main tree recreates `Node`+`Board` and re-explores the whole subtree for every transposed occurrence, so the combinatorial explosion is untouched.
  3. The 99% of cutoffs mostly return a static `stand_pat` stored as `TT_EXACT` (cf. #4), so quiescence is effectively neutralised and there is no tactical signal to search deeper.
  4. The "Lague-style" gain comes from TT cutoffs inside a main alpha-beta search with iterative deepening — a mechanism absent here (this search is MCTS-like).
- **Plan**:
  - **A. Scalar TT in the main search**: probe in `explore_new_move` *before* expanding; if a reliable entry of sufficient depth exists, reuse its value without recreating the subtree. Reuses the value, not the `Node` object (avoiding `_nodes`/backpropagation/cycle problems). **Blocked by #4 then #3** (otherwise it propagates wrong values). A "value only" gain means quiescence is skipped but re-expansion is not necessarily avoided, so the gain is partial.
  - **B. Node sharing / DAG**: the maximal gain (8→30), but a large refactor the code has been avoiding. The infrastructure is half in place (`ChildLink._propagated_nodes`, `_parent_count`). Risky.
- **Order**: #4 → #3 → A (measure), then consider B if A is insufficient.
- **✅ A implemented & measured (2026-05-17, branch `feature/tt-main-search`) — INSUFFICIENT, as anticipated.** A complete implementation behind the runtime toggle `g_tt_main_search` (default OFF), reviewed by several agents, byte-identical when OFF (non-regression on PERFT 1/2 + EVALUATION confirmed by the user).
  - **Measurement (blocked king-and-pawn endgame, fixed iteration budget)**: OFF → Nodes 431k, Iter 431k (1:1), NPS 40k and stable. ON → Nodes 447k (unchanged), **Iter 6.1M (14×)**, Stores **29.8M ≈ Overwrites 29.8M** for **12.8k entries** (≈0 net), NPS **40k→7k and falling**, **no depth gain at all** (if anything, less).
  - **Root cause (architectural, not a bug)**: a *frozen scalar* TT leaf truncates every transposed line at the depth of its first freeze (a shallow one), so the PV cannot deepen — a depth cap. And once a front is entirely frozen, the iteration-budgeted MCTS loop has no progress left to make and spins (14× the iterations, collapsing NPS, write-back rewriting the same value 29.8M times). The "Lague-style" gain requires *continuing the deep subtree through the transposition* (sharing), which scalar value reuse structurally cannot do. This confirms the contingency stated in the spec ("B only if A is insufficient").
  - **User decision (2026-05-17)**: **pivot to plan B (DAG / node sharing)**. The plan A code is kept on its branch, OFF (correct and harmless); whether to keep it OFF or revert it is to be settled at the plan B brainstorm. Plan A spec and plan: `docs/superpowers/{specs,plans}/2026-05-17-optichess-tt-main-search*`.

### ✅ #14 — Persistent evaluation/score inconsistency on a value coming from the TT (generalisation of #1 to the non-mate case)
- **Status**: ✅ fixed — **validated at runtime by the user** ("seems to be working fine").
- **Files**: the `if (tt_cutoff)` block at `exploration.cpp:781-810`; `Evaluation::get_WDL` (`board.cpp:10009`) recombines `_value` with `_uncertainty`/`_winnable_*`; `get_average_score` (`board.cpp:10098`) derives `_avg_score = _wdl.win + 0.5·_wdl.draw`; display at `exploration.cpp:546-549` (value=`_value`, score=`_avg_score`).
- **Severity**: HIGH (correctness, user-visible).
- **Symptom**: `_value` and the **displayed score** sometimes failed to correlate; **as soon as the variation was explored** it became correct again.
- **Root cause (refined)**: since #1, the cutoff block already calls `get_WDL()`+`get_average_score()`, so `_wdl`/`_avg_score` *are* recomputed from the new `_value`. What remained: `_uncertainty` (and `_winnable_*`) still held the values of the **static** evaluation (static uncertainty encodes the material complexity of the position, `board.cpp:9975-10005` — **not** the reliability of a searched value). `get_WDL` was therefore filtering the deep TT `_value` through static uncertainty, damping or diverging the score until a real re-exploration (a full `Evaluation` copy of the resolved best child, `exploration.cpp:362/392/1016`) restored a coherent `_uncertainty`.
- **Fix applied**: generalise the #1 v2 fix to the non-mate case — force `_uncertainty = 0` on **every** TT cutoff (consistent with the *trusted* terminal/mate/NN paths at `board.cpp:1558/1575/1592`, which all set `_uncertainty=0` and then recompute the WDL). The `_winnable_* = 0/1 by sign` override stays **mate-only** (it avoids the `winning_eval/_winnable` scaling on a huge mate score; outside mate the static `_winnable_*` are a legitimate property of the position and are preserved). `_avg_score` becomes a deterministic monotone function of `_value`, so value and score always correlate.
- **⚠️ Expected effect on the search**: `get_node_score` consumes `_uncertainty`; quiescence leaves that hit a cutoff go from static uncertainty to 0, so their WDL/score becomes sharper. The *display* change is intended; the impact on move ordering is to be validated at the runtime gate (playing strength / EVALUATION).
- **Interaction with #3 (not fixed here)**: #3 (wrong mate distance through a transposition, `_moves_count` not hashed) remains open and independent — it affects the *magnitude* inside the mate band, not the value↔score correlation. To be handled separately.

---

## 🔍 To verify / clarify

### #9 — 2-fold vs 3-fold repetition (FIDE)
- **File**: `exploration.cpp:7,29-31` (`search_repetition_limit = 2`)
- The first repetition inside the tree is treated as a draw. This is a classic and sound search optimisation, but it is not the FIDE 3-fold rule. **Confirm that it is intended** (risk: overvaluing illusory forced draws).

### #10 — `evaluate_quiescence_threat` / `minimal_quiescence`
- **Status**: 🔍 verified OK — does not use the TT; it works on a copied `Board` with the side to move flipped. No pollution. Noted for the record.

---

## Recommended order of work
> #2, #12 and #13 are fixed and runtime-validated (see the ✅ Fixed section). Next:
1. ~~**#4** (stand-pat ≠ EXACT)~~ ✅ runtime-validated (`783135d`) · ~~**#14** (value↔score coherence, non-mate case)~~ ✅ runtime-validated (`1e56a68`) · ~~**#3** (ply-relative mate scores)~~ ✅ runtime-validated — the TT correctness pass is complete; **#11** remains.
2. **#11 plan A** (scalar TT in the main search) — **the objective**: a depth gain. Measure afterwards.
3. **#6** then **#5** (TT hygiene).
4. **#7** (path-local performance + map leak — re-test #13), then **#8** (`operator<`, quick, latent).
5. **#11 plan B** (DAG) — only if A is insufficient for the intended gain.
6. **#9** — a design decision to confirm with the user.
