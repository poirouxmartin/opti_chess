# #11 — DAG GHI-correct repetition: one mechanism — Design Spec

**Date:** 2026-05-24
**Branch:** `feature/tt-main-search`
**Issue:** #11 — node-sharing (DAG) gives depth but breaks repetition (Graph History Interaction)
**Toggle:** `g_tt_node_dag` (existing, default OFF)
**Supersedes:** the 8 prior #11 attempts (Plan A, Option D, opt-1, bool bubble, Approach A, attempt-7 verdict, attempt-8 de-sharing). All are removed by this design.

---

## 1. Goal

Make `g_tt_node_dag == true` correct on repetition/transposition **in all cases**, while
**keeping node-sharing** (so the DAG depth benefit returns). One uniform mechanism replaces the
entire pile of layered patches.

Success = on the assistant-runnable harness:
- **Repro 1** (`6k1/8/7P/7K/8/8/8/8 w - - 3 72`, KP(h)-vs-K theoretical draw): ON → draw (≈0).
- **Repro 2** (`8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7`, won pawn endgame): ON → winning.
- **General cases** (not just pawn endings): a perpetual-check middlegame → draw; a fortress → draw.
- **Depth is preserved**: sharing is demonstrably active under ON (not attempt-8's "correct but
  depth-dead").
- **OFF byte-identical** to baseline (the mechanism is fully `g_tt_node_dag`-gated).

**Out of scope (separate follow-up):** the `play_move_keep` re-root corruption cluster
(advancing moves in a shared position corrupts the tree/board via use-after-recycle of shared
nodes). The "draw/no-win" observed in GUI play is almost certainly this, not the search eval —
the headless harness shows attempt-8 was eval-correct at convergence on both repros
(Repro 1 → 0, Repro 2 → 311) and failed only on depth.

---

## 2. Current implementation, laid flat (the tangle being replaced)

Two concepts the code conflates:

- **Repetition = PATH-dependent.** Whether a position is a draw depends on the move sequence,
  not the position alone. State: `PositionHistory = robin_map<zobrist → count>`
  (`= RepetitionHistory`, board.h:16, **aliased** with the game-level `Board::_positions_history`),
  threaded by pointer and pushed/popped by RAII `PathScope` (exploration.cpp:115). Predicate
  `position_is_draw_by_repetition(path, board) = count+1 >= limit`, `search_repetition_limit = 2`
  (treats the 2nd occurrence as a draw — aggressive, not FIDE threefold; `display_repetition_limit = 3`
  is display-only). Game-level threefold is separate (`Board::_positions_history` + `is_game_over(3)`).

- **Transposition / node-sharing (the DAG) = POSITION-dependent.** One `Node` per zobrist key,
  shared across paths, holding one path-independent `_deep_evaluation`. `node_map: zobrist → Node*`,
  consulted/registered at the single share site `explore_new_move`. `add_child` (exploration.cpp:228)
  bumps `child->_parent_count`; shared ⇒ `_parent_count ≥ 2`. `Node::reset` decrements and recycles
  only at `≤ 0`. Node accounting under DAG = "model A": `_nodes += 1` per iteration (a bounded visit
  proxy, **not** tree size/depth — why "depth" looks flat under DAG).

**The conflict (GHI):** a path-dependent draw cannot be stored in a path-independent shared node;
invariant `772183a` forbids writing the path-local draw into the shared node (corrupts other paths).
In tree mode this never bites: every repetition is a *fresh* leaf (`init_terminal_draw_child`) and
minimax backs it up. Sharing destroys that.

**The layered patches (all removed by this design):** the ON-only §3 *cut* in
`explore_random_child` (don't descend, return); **opt-1** (substitute `dag_draw_eval()` in backup +
persist if `_parent_count≤1` + emit via `path_local_eval`); **opt-3** (`DagExcl` per-frame skip list);
**attempt-7** (already removed); **attempt-8** (`!is_pawn_endgame()` share gate). Why they all failed:
each turned "cycle detected" into a **verdict/override**, or scored the cyclic child as draw in
*selection* but not consistently in *backup* (engine kept picking it → win eroded). No path-cycle
predicate alone can distinguish a true forced draw from a transient transposition cycle.

---

## 3. The one mechanism

**Principle:** a repetition is a **draw-valued leaf that competes in minimax**, evaluated
**per-path at the consumer**, used **identically in selection and backup** — never a verdict.

### 3.1 Path state (de-aliased; carries depth)
- **De-alias** `PositionHistory` (exploration.h:10) from `RepetitionHistory` into its own type:
  `using PositionHistory = tsl::robin_map<uint64_t, PathEntry>;` with
  `struct PathEntry { uint16_t count; uint16_t first_ply; };`. The game-level
  `RepetitionHistory` / `Board::_positions_history` stays `<uint64,uint8>` — **untouched**.
- Thread an integer `ply` (root = 0, `+1` per descent). `PathScope`/`record_position_in_history`/
  `ensure_position_in_history` take `ply`: on first insert set `first_ply = ply`; on increment keep
  the earliest. `unrecord_position_in_history` decrements `count`, erases at 0. Add
  `position_history_first_ply(path, board)`.

### 3.2 The rule (ON-only)
For node `N` at ply `p`, when computing N's value in BOTH `get_node_score`/`get_best_score_move`
(selection) and the backup, each child `C`:
- if `C` is already on the path (`position_is_draw_by_repetition`) → **C contributes `dag_draw_eval()`
  (0)** for this traversal; note `e = first_ply(C)`;
- else → C contributes its stored `C->_deep_evaluation`.

N's value = ordinary minimax (max for side to move) over those contributions. Selection and backup
use the **same** substituted values ⇒ a winning non-repeating line always outscores a draw-substituted
cyclic sibling (**win never eroded**); when every line ≤ draw, the draw wins (**true draw recognised**).
This is exactly tree-mode minimax with the cycle as a leaf value.

### 3.3 GHI-correct caching guard
N tracks `min_earlier_ply` = smallest `e` among repetition substitutions on the line it backed up:
- **`min_earlier_ply ≥ p`** → every repetition lies within N's own subtree (intrinsic,
  path-independent) → **persist `N->_deep_evaluation`** (cache; other fresh paths reuse it → depth kept).
- **`min_earlier_ply < p`** → a repetition reaches above N (extrinsic / path-dependent) → **do NOT
  overwrite N's cached value**; use the path-local value only for this traversal's backup; propagate
  `min_earlier_ply` upward.

Return channel = `struct BackupResult { Evaluation value; int min_earlier_ply; };` out-param on
`grogros_zero`/`explore_random_child` (replaces the dead `Evaluation* path_local_eval`). `nullptr` ⇒
OFF/tree path (byte-identical).

### 3.4 Don't-descend-on-repetition
If the *selected* child is on the path, do **not** descend (would infinite-loop); its per-traversal
value = draw(0), `e = first_ply`, count the iteration. (This is the §3 cut's structural part, but its
value now feeds the consistent minimax instead of an opt-1 override.)

---

## 4. Why correct in all cases (traces)

- **KP(h)-vs-K (draw):** a king-shuffle node `S` reached fresh at ply `p` repeats *within its own
  subtree* (earlier ply ≥ p) → intrinsic → `S` caches 0; pawn push → Kxh7/stalemate = 0; root
  minimax = 0. ✓
- **Won pawn endgame (win):** the winning line doesn't repeat → its value wins the minimax over the
  draw-substituted cyclic sibling → win preserved *and cached*. ✓
- **Perpetual check / fortress in the middlegame (general):** a real perpetual where all escapes ≤
  draw minimaxes to draw; cacheable iff intrinsic. Covers non-pawn-ending GHI = the "all cases". ✓
- **Extrinsic repetition (hard GHI case):** evaluated correctly *per traversal* (draw value used) but
  not cached → recomputed each occurrence → **no stale/contaminated shared value, no residual.** ✓

---

## 5. Exact changes

**Files:** `exploration.h`, `exploration.cpp` (primary); `board.h` (only if `PathEntry`/type lives
there); `ALGORITHMS.md` (doc refresh — §7/§8 are stale).

**Add:**
- `PathEntry` struct + de-aliased `PositionHistory`; `ply` parameter on `PathScope`/record/ensure;
  `position_history_first_ply` helper.
- `int ply` on `grogros_zero` (default 0), `explore_new_move`, `explore_random_child`; `+1` per descent.
- `BackupResult` out-param channel.
- `const PositionHistory* path, int ply` (default `nullptr`/0) on `get_node_score`,
  `get_move_scores`, `get_best_score_move`; on-path child scored as `dag_draw_eval()` via the existing
  `custom_eval` hook.
- The cache guard (persist iff `min_earlier_ply ≥ ply`) + `min_earlier_ply` propagation in
  `explore_random_child` backup.

**Remove:** opt-1 (backup draw substitution + `_parent_count≤1` persist + `path_local_eval` emission);
opt-3 `DagExcl` struct (exploration.h) + all its plumbing; the ON-only §3 *cut* branch in
`explore_random_child` (subsumed by 3.2/3.4); attempt-8 `dag_shareable`/`is_pawn_endgame()` share gate
in `explore_new_move` (**full sharing re-enabled**). Remove the dead `dag_child_eval` scratch in
`grogros_zero`.

**Keep:** `node_map` sharing + `add_child`/`_parent_count`; model-A `_nodes` accounting; the fresh
§3 draw leaf in `explore_new_move` (`init_terminal_draw_child` — intrinsic, tree-correct); `dag_draw_eval()`;
`display_repetition_limit` (display threefold); `dag_log` infrastructure.

**Verify during implementation:** that no caller passes `Board::_positions_history` where a search
`PositionHistory` is expected (the de-alias must not break game-level repetition); grep all
`PositionHistory` uses.

---

## 6. Testing (assistant-runnable, iterable)

Harness `opti_chess --dag-test [n_batches] [iters]` (built; per-batch eval+nodes trajectory). Assert:
1. **Low AND high iters** (e.g. 20×3000 and ≥120×4000): Repro 1 ON → `|eval| ≤ EPS_DRAW`; Repro 2 ON →
   `eval ≥ WIN_THRESH`; OFF sanity for both. Convergence asserted as bands (MCTS run-to-run variance).
2. **New positions (all cases):** add a perpetual-check middlegame (→ draw ON) and a fortress/known
   draw (→ draw ON). FENs pinned in plan Task 1 from measured OFF trajectories.
3. **Depth/sharing metric** (guards against attempt-8's depth-death): assert sharing active under ON
   — `node_map` link-hits > 0 AND unique nodes ≪ iterations (expose via the existing `g_dag_link_*`
   counters / `dag_debug_report`). A bare eval check is insufficient.
4. **OFF byte-identical:** mechanism fully `g_tt_node_dag`-gated; OFF path unchanged from baseline.
5. **USER GUI gate (final, assistant cannot run):** OFF PERFT 1/2 + EVALUATION vs baseline; DAG ON in
   real play — both repros correct, depth visibly restored, no regression.

---

## 7. Performance (#1 priority — explicit validation)

The per-child `path` lookup added to the **selection loop** (`get_node_score`/`get_best_score_move`)
is the one hot-path risk. Mitigations: ON-only (OFF untouched); `path` is a small `robin_map` O(1)
find. The plan **measures NPS ON vs baseline** via the harness's nodes/time; if it regresses, fall
back to substituting only across the candidate set from the `max` pre-pass while keeping
selection/backup consistent. Removing `DagExcl` + opt-1 + the share gate is a net simplification
(likely neutral-to-faster). No new heap allocations (`PathEntry` widens the existing map value;
`BackupResult` is stack-only).

---

## 8. Acceptance criteria

1. Build clean (no `error`) with `g_tt_node_dag` both states.
2. Harness exits 0: Repro 1 ON draw, Repro 2 ON win at low AND high iters; perpetual → draw;
   fortress → draw; OFF sanity all; sharing-active metric passes.
3. NPS ON not materially worse than baseline (or the documented fallback applied).
4. OFF byte-identical (assistant harness) — final PERFT/EVAL OFF + real-play ON = USER GUI gate.
5. `opti_chess/BUGFIXES.md` #11 updated; `ALGORITHMS.md` §7/§8 refreshed; this spec marked
   validated-by-gate.

---

## 9. Self-review

- **Placeholders:** none — every mechanism site is named; the only deferred values are the new test
  FENs + numeric thresholds, pinned in plan Task 1 from measured OFF runs (the established #11 pattern,
  a concrete lookup not a vague TODO).
- **Internal consistency:** §3 (rule + guard) ⇒ §4 (traces) ⇒ §1 (goal) ⇒ §6 OFF-gated identicality.
  §5 removals don't touch §5 keeps; the §3 don't-descend replaces the removed §3 cut so descent
  termination is preserved.
- **Scope:** one mechanism (rule + caching guard) + threading + removals + tests. Re-root/play
  corruption explicitly out of scope (§1). Focused for a single plan.
- **Ambiguity:** "intrinsic vs extrinsic" made concrete via `min_earlier_ply ≥ ply`; "all cases"
  made concrete via the perpetual + fortress test positions; "depth preserved" made concrete via the
  sharing-active metric.
