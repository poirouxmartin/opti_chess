# #11 attempt 8 — DAG selective de-sharing (endgame gate) — Design Spec

**Date:** 2026-05-22
**Branch:** `feature/tt-main-search`
**Baseline:** `a1c0e9c` (attempt-7c committed tip; uncommitted 7d discarded)
**Issue:** #11 — DAG GHI soundness on the two reference repros
**Toggle:** `g_tt_node_dag` (existing, default OFF)

**Status (2026-05-23):** IMPLEMENTED on `feature/tt-main-search` (plan `docs/superpowers/plans/2026-05-23-optichess-dag-endgame-desharing.md`). Headless harness (`opti_chess --dag-test`) PASSES: Repro 1 ON 476→0 (draw recovered), Repro 2 ON 103 (still winning), OFF sanity both. attempt-7 verdict machinery removed. The GUI OFF byte-identicality + real-play gate (§9.3) remains a USER gate.

---

## 1. Goal

Make `g_tt_node_dag == true` STRUCTURALLY CORRECT on both reference repros, by
attacking node-sharing itself rather than injecting a draw value into the shared
graph (the failure class of all 7 prior attempts):

- **Repro 1 (`6k1/8/7P/7K/8/8/8/8 w - - 3 72`, KP(h)-vs-K theoretical draw):**
  root `_deep_evaluation._value` converges to a draw (≈0) under DAG ON, as it
  already does under DAG OFF.
- **Repro 2 (`8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7`, won pawn endgame):**
  root eval stays winning (no phantom draw) under DAG ON.

Validation is **correctness-first**: DAG ON in the de-shared region behaves like
tree mode (OFF). It may be slower than the current DAG there; sharing-perf
recovery is an explicit later follow-up, not part of this milestone.

## 2. Why prior attempts failed, and why this is sound

### 2.1 Root cause (evidence-grounded)

In TREE mode (OFF) Repro 1 converges to 0 because every occurrence of a
king-shuffle position (Kg5, Kh5, …) is a FRESH node: the §3 repetition leaves are
hit and plain minimax backs the draw up to the root.

Under the DAG those positions are SHARED (one `Node`, one path-independent
`_deep_evaluation`). The draw at the §3 leaves cannot back up THROUGH the shared
node, whose stored value stays its shallow static eval ("White +1 pawn" = 476);
and invariant 772183a forbids overwriting it path-locally. **Sharing is exactly
what blocks the draw from backing up.**

The 7 prior attempts (Plan A scalar TT, Option D path-aware selection, opt-1
path-local backup, the bool bubble, Approach A path-local revaluation, attempt-7
all-cycle verdict + 7b/7c/7d) all KEPT sharing and tried to detect the draw with
a traversal-cycle predicate and inject a draw value into the graph. They all
conflated "this edge closes a cycle on the current graph traversal" with "this
position is a forced draw." attempt-7's measured failure (2026-05-21 log): Repro
1 ON `all_cycle_verdicts_emitted=179` but `all_cycle_persisted=0`, root frozen at
476 across 10 batches — because the root's pawn-push h7 leads to a minimax-draw
subtree that is NOT all-draw (…Kf8 → h8=Q wins), so the "ALL children drawish"
gate never fires at the nodes that matter. The relaxed "BEST child drawish" =
Option D / bubble = already regressed.

### 2.2 Why de-sharing is sound by construction

De-sharing never writes a path-dependent value into a shared structure — it
removes the sharing instead. A position that is not shared is explored
per-path exactly as in tree mode, which is already correct (OFF gives Repro 1 =
0). De-sharing therefore CANNOT corrupt other paths (invariant 772183a is
trivially satisfied: there is no shared node to mutate). The only thing it costs
is the DAG speed benefit in the de-shared region.

## 3. Mechanism — endgame gate on node-sharing (criterion C2)

### 3.1 The single share site

`node_map` sharing happens in EXACTLY ONE place: `Node::explore_new_move`
(`exploration.cpp`):
- `:617` `node_map.find(new_board->_zobrist_key)` (lookup → link to a live shared node)
- `:653` `node_map[new_board->_zobrist_key] = child;` (register a fresh node)

`quiescence` never shares (`exploration.cpp:1633`, "Pas de partage via TT"), and
the §3 repetition leaf (`:607`, `init_terminal_draw_child`) is already a fresh,
never-shared node. So gating this one site fully controls sharing.

### 3.2 The gate

Define a path-independent, O(1) predicate on the board:

```cpp
// #11 attempt-8 — une position de finale de pions est dé-partagée sous DAG :
// chaque chemin reçoit son propre sous-arbre -> les feuilles §3 de répétition
// remontent la nulle par minimax ordinaire (comportement mode-arbre, déjà
// correct). is_pawn_endgame() (board.cpp:10389) = uniquement rois + pions ;
// couvre les deux repros et c'est la porte SÛRE la plus étroite (dé-partage le
// moins). OFF : jamais consultée. Coût : un scan 8x8 borné, à la création de
// nœud seulement (jamais dans la boucle de sélection/éval).
const bool dag_shareable = g_tt_node_dag && !new_board->is_pawn_endgame();
```

- At `:617`: probe `node_map` only when `dag_shareable` (else fall to the
  fresh-create branch — a pawn-endgame position is never looked up).
- At `:653`: register in `node_map` only when `dag_shareable` (a pawn-endgame
  position is never registered → never found → never shared).

Both gates use the same `dag_shareable`. A pawn-endgame position is thus never in
`node_map`, so it is always a fresh per-path node. Promotion exits the pawn
endgame (pieces appear) → post-promotion positions remain shareable; those lines
are decisive (KQ-vs-K), not draw-prone, so sharing them is GHI-safe.

### 3.3 Gate signal choice

`is_pawn_endgame()` is the milestone gate: exact, cheap, covers both repros, and
narrowest-sound. Widening to general endgames (rook endgames, fortress draws via
a piece-count threshold) is an explicit FOLLOW-UP (§8), not this milestone — both
repros are pawn endgames, and a wider gate de-shares more (less DAG benefit) for
no milestone benefit.

## 4. Remove the now-dead attempt-7 verdict machinery

With C2, every node in the de-shared (pawn-endgame) region has
`_parent_count <= 1`, so the ordinary minimax backup is correct WITHOUT any value
injection. The attempt-7 all-cycle verdict can never do anything useful (it was
already measured dead: `all_cycle_persisted=0`) and only adds hot-path cost.
Remove it:

- `ChildLink._cycled_batch_seq` (field) + its `_cycled_batch_seq = g_dag_batch_seq`
  writes at the §3 cut and the descent-cascade in `explore_random_child`.
- `g_dag_batch_seq` global + its `++g_dag_batch_seq` increments in
  `GUI::grogros_analysis` and `GUI::run_dag_repro`.
- The all-cycle verdict block at the `grogros_zero` tail (`exploration.cpp`
  ~`:502-550`).
- `Evaluation::operator==` (re-added for the verdict's change-detection; verify
  no other consumer before removing — if any, keep it).
- The attempt-7 `dag_log` counters `all_cycle_verdicts_emitted`,
  `all_cycle_persisted`, `enum_gate_blocks` (and any `all_cycle_verdict` event
  type), unless still useful for the harness.

KEEP intact (orthogonal, still correct/needed): the §3 structural cut
(`position_is_draw_by_repetition` → don't descend, count iteration, mutate
nothing shared), opt-3 anti-spin `DagExcl`, Bug-2 model-A node accounting,
display threefold, the `dag_log` infrastructure itself.

This removal is part of attempt-8 (user decision 2026-05-22). It must be
behaviour-preserving given the gate: in pawn endgames the verdict never persisted
anyway; outside pawn endgames (shareable) the verdict block also never produced a
correct result. Net behavioural change comes solely from §3's de-sharing.

## 5. Headless validation harness

A non-GUI entry point lets the assistant validate in CLI before any USER gate,
breaking the manual-gate bottleneck that cost 7 cycles.

### 5.1 Entry

A headless mode in `main.cpp` (e.g. a `bool headless` flag mirroring the existing
`lichess` flag, or a `main_headless()`), which does NOT call `InitWindow`. It
reuses the existing globals `monte_node_buffer`, `node_map`,
`monte_board_buffer` (exploration.cpp:2317/2325, buffer.cpp:116 — all global, no
GUI needed) and the search/eval/board code (no rendering on the search path).

### 5.2 Per-repro routine

Replicate the minimal root setup of `GUI::load_FEN` (gui.cpp:1650-1700, root node
at `:1689`) outside the GUI: parse FEN → Board → root `Node` from
`monte_node_buffer`. Then, mirroring `GUI::run_dag_repro` (gui.cpp:1105):

1. set `g_tt_node_dag` (on/off per case), `g_tt_main_search = false`;
2. loop `root->grogros_zero(&monte_board_buffer, eval, alpha, beta, gamma,
   iters_per_batch, quiescence_depth)` for N batches;
3. read `root->_deep_evaluation._value` (and `._avg_score`).

Reset `node_map` / buffers between cases as the GUI reset sites do, so cases do
not leak state.

### 5.3 Assertions (exit code)

- Repro 1 ON: `abs(eval) <= EPS_DRAW` after N batches → PASS (draw recovered).
- Repro 1 OFF: `abs(eval) <= EPS_DRAW` (sanity: tree mode already draws).
- Repro 2 ON: `eval >= WIN_THRESH` and `abs(eval) > EPS_DRAW` (still winning, no
  phantom draw).
- Repro 2 OFF: `eval >= WIN_THRESH` (sanity).

`EPS_DRAW`, `WIN_THRESH`, N, iters are pinned in plan Task 1 from the measured OFF
trajectories (Repro 1 OFF reached 0; Repro 2 OFF climbs slowly — choose N large
enough that OFF Repro 2 is already clearly positive, or assert the ON-vs-OFF
trajectory rather than an absolute by a fixed batch). Exit non-zero on any
failed assertion so the harness is CI/CLI-runnable by the assistant.

The harness is DAG-only behaviour under test; it must not change OFF search
behaviour. It links the full binary (raylib included) but never initialises the
window.

## 6. OFF byte-identicality

Under `g_tt_node_dag == false`:
- `dag_shareable` short-circuits to `false`; `node_map` is never probed or
  written (already the case OFF — the gate only narrows ON behaviour).
- The attempt-7 removal deletes code that was entirely `g_tt_node_dag`-gated or
  DAG-only; OFF never executed it.
- The headless harness is an additive entry point; the normal `main_ui()` path is
  untouched.

OFF behaviour is byte-identical to baseline `a1c0e9c`.

## 7. Performance

- One `is_pawn_endgame()` call (bounded 8×8 scan) per node creation in
  `explore_new_move`, ON only. Not in the selection loop, not in `get_node_score`,
  not in eval. Negligible relative to a node expansion (which already evaluates
  the position).
- Removing the attempt-7 verdict block removes an O(|children|) tail loop per
  `grogros_zero` call + the per-§3-cut marker write → small ON speedup.
- The DAG is disabled in pawn endgames → slower there than current DAG (accepted,
  correctness-first). Middlegame/piece-endgame sharing unchanged.

Hot path (selection, `get_best_score_move`, `get_node_score`, eval): UNCHANGED.
No new allocations.

## 8. Documented residual & follow-ups

- **Residual (in scope to document, not to fix now):** C2 de-shares only pawn
  endgames. A repetition/perpetual draw in a non-pawn-endgame position (e.g.
  perpetual check in the middlegame, a piece-endgame fortress) is still
  mis-evaluated under DAG (GHI uncovered there). Both reference repros are pawn
  endgames, so the milestone is correct; this residual is acknowledged.
- **Follow-up 1 (perf):** recover DAG sharing in the de-shared region with a more
  targeted criterion (C3 cycle-participation, or path-indexed eval C) once
  correctness is locked.
- **Follow-up 2 (coverage):** widen the gate beyond pawn endgames (piece-count
  threshold) to cover general endgame repetition draws.
- **Out of scope (unchanged):** the `play_move_keep` re-root corruption cluster
  (A/B/C/D), post-move mispricing, the sleep/wake UI crash (E), standalone-#1
  [DMD]. These are separate from search-eval GHI.

## 9. Acceptance criteria

1. Build clean (EXITCODE=0, no `error` lines) with `g_tt_node_dag` both states.
2. Headless harness exits 0: Repro 1 ON ≈ draw, Repro 2 ON winning, OFF sanity
   both. (Assistant-runnable; gates the algorithm.)
3. USER GUI gate (final, assistant cannot run): OFF byte-identical (PERFT 1/2 +
   EVALUATION vs baseline `a1c0e9c`) on the usual positions; DAG ON Repro 1 no
   longer false-wins; DAG ON Repro 2 stays won; no regression in real play.
4. `opti_chess/BUGFIXES.md` #11 updated with the attempt-8 outcome; this spec
   marked validated-by-gate.

## 10. Self-review

- **Placeholders:** none. Gate signal pinned (`is_pawn_endgame()`); single share
  site identified by line; attempt-7 removal enumerated; harness assertions
  defined (with the numeric thresholds explicitly deferred to plan Task 1, a
  concrete lookup, not a vague TODO — the established #11 pattern).
- **Internal consistency:** §3 (gate) ⇒ `_parent_count<=1` in pawn endgames ⇒ §4
  (verdict removal is safe) ⇒ §2.2 (sound by construction) ⇒ §6 (OFF identical).
  No section contradicts another.
- **Scope:** one mechanism (de-share gate) + one cleanup (attempt-7 removal) + one
  test infra (headless harness). Focused for a single plan.
- **Ambiguity:** "endgame" is made concrete = `is_pawn_endgame()`; broader
  endgames are an explicit named follow-up, not this milestone.
