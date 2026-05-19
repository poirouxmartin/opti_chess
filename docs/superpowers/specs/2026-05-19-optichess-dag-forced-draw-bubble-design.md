# #11 Plan B — DAG forced-draw vs transient-cycle: bubbled bool verdict — DESIGN

> Status: design **validated** (user, 2026-05-19). Next step: `superpowers:writing-plans`.
> Solves the core unsolved DAG-soundness problem stated in
> `docs/superpowers/specs/2026-05-18-optichess-dag-true-draw-vs-transient-cycle.md`.
> Prerequisite context (assume read): that brief, `opti_chess/CLAUDE.md`
> (perf is priority #1), `opti_chess/BUGFIXES.md` #11, the B-2 spec
> `docs/superpowers/specs/2026-05-17-optichess-plan-b-dag-design.md` §3,
> the opt-1 plan `docs/superpowers/plans/2026-05-18-optichess-bug1-opt1-path-local-draw-backup.md`,
> and memory `project-tt-main-search-state`.

## Problem (one sentence)

With the transposition DAG enabled (`g_tt_node_dag`, key **O**), the engine
cannot bubble a genuinely-forced repetition draw to the root through **shared**
intermediate nodes, producing a false win in a dead draw (KP(h) vs K) and, for
any threshold/selection hack tried so far, a phantom draw in a won position.

## The criterion (what "forced draw vs transient cycle" actually is)

It is **minimax with repetition = 0**, nothing more exotic. A node `N` is a
**forced** repetition draw *on the current path* iff its best move — ranked
with every *proven* forced-draw edge counted as a draw (`dag_draw_eval()`, 0
white-relative) — is itself a draw: no progress move gives the side to move
better than 0. A **transient** cycle is benign automatically: a real winning
child is not a proven forced-draw edge, so the negamax `max` keeps it over the
draws. This is **not** a path-count threshold (both reverted threshold patches
were mis-justified — side-to-move is in the Zobrist key, see the brief's
"Verified facts") and **not** a per-pick repetition predicate (that was the
reverted Option D).

Spec angle mapping (from the brief's "Suggested first step"): this realizes
angle **(c)** (keep the §3 structural cut as-is; change only how/whether its
value is trusted for backup) implemented via angle **(b)** (a per-traversal
proven-forced-draw verdict carried by return value). Angle **(a)** ("no
progress move exists at the repeating node") is *emergent* from the minimax —
no separate detector is built.

## Architecture

### The proven forced-draw edge set: `DagExcl`, generalized

`DagExcl` (`exploration.h:26`, stack-local per `grogros_zero` frame, `CAP=24`,
zero-alloc) is today "edges the §3 structural cut fired on, this frame"
(opt-3 anti-spin). **Generalize its meaning** to: *"edges proven to be a
forced repetition draw on this traversal-frame"* =

- §3 direct structural cuts (descending the edge would be a path repetition —
  unchanged, `exploration.cpp:768`), **plus**
- descended children whose own recursive `grogros_zero` verdict came back
  forced-draw (the new bubbling).

It stays stack-local, never written to any shared `Node`/`ChildLink`/`node_map`
(invariant 772183a). On `CAP` overflow it falls back to the existing
conservative behaviour (a forced-draw edge simply not excluded that frame —
sound, slightly sub-optimal; endgames like KP(h) have < 24 legal moves).

### The verdict channel: one `bool*` on `grogros_zero` only

Repetition is the **only** path-dependent verdict (mate/eval are
position-truths carried by the shared node — see B-2 spec "Foundations"). So
the path-local correction is strictly binary. The channel is therefore a single
`bool`, and it lives on **`grogros_zero` only**:

- `grogros_zero(..., PositionHistory* path_history, bool* path_forced_draw = nullptr)`.
- **Remove** the now-dead `Evaluation* path_local_eval` from **both**
  `explore_random_child` and `explore_new_move`. It was write-only and never
  bubbled (the documented opt-1 residual). `explore_random_child` owns the
  descended child's verdict as a stack local; `explore_new_move` never
  participates (its draw leaves are real distinct terminal draws, already
  carrying 0 in `_deep_evaluation`).

`dag_draw_eval()` (`exploration.cpp:154`) is **retained** — it is the value
substituted in the backup ranking and the value persisted to an *unshared*
node's `_deep_evaluation`.

## Wiring (every new branch `g_tt_node_dag`-gated; OFF byte-identical)

1. **§3 structural cut** (`exploration.cpp:768` block). Structurally
   unchanged: do not descend, count the iteration, `dag_excl->add(move)`,
   early `return`. The `if (path_local_eval != nullptr) *path_local_eval =
   dag_draw_eval();` lines (`:794-796`) are **removed** — the edge is already
   recorded in `dag_excl`, which is now the single source of truth for "this
   edge is a proven forced draw this frame".

2. **Descended child** (`exploration.cpp:801-805` region). Replace the
   recursive call so the child reports its verdict, then promote the edge:

   ```cpp
   bool child_forced_draw = false;
   {
       PathScope _ps(branch_history, *child->_board);
       child->grogros_zero(board_buffer, eval, alpha, beta, gamma, 1,
           quiescence_depth, network, &branch_history,
           g_tt_node_dag ? &child_forced_draw : nullptr);
   }
   if (g_tt_node_dag && child_forced_draw && dag_excl != nullptr) {
       dag_excl->add(move); // ce fils est une nulle forcee sur CE chemin
   }
   ```

3. **`get_best_score_move`** (`exploration.cpp:1939`). Add an optional
   trailing `const DagExcl* dag_excl = nullptr`. In its child loop, when
   `dag_excl != nullptr && dag_excl->contains(move)`, score that child via the
   **existing** `custom_eval` path of `get_node_score` (`:1899`,
   `Evaluation *custom_eval`) with a `dag_draw_eval()` value instead of
   `child->_deep_evaluation`. The `max_eval`/`max_avg_score` pre-pass should
   likewise treat a `dag_excl` child as the draw value so the softmax
   normalization is consistent. Default `nullptr` → only the DAG backup call
   passes it; selection/display callers are byte-identical.

4. **Backup** (`exploration.cpp:808-827`). `_bm = get_best_score_move(alpha,
   beta, /*consider_standpat as today*/, /*qdepth as today*/,
   g_tt_node_dag ? dag_excl : nullptr)` — `_bm` is now the path-aware best
   move. The existing branch structure stays:
   - `if (g_tt_node_dag && dag_excl != nullptr && dag_excl->contains(_bm))`:
     `N`'s best move is a proven forced-draw edge → `N` is a forced draw on
     this path. Persist `dag_draw_eval()` to `_deep_evaluation` **only if
     `_parent_count <= 1`** (772183a); otherwise the value lives solely in the
     bubbled bool.
   - `else`: `_deep_evaluation = _children[_bm]._node->_deep_evaluation;`
     (unchanged; OFF path strictly byte-identical).

5. **`grogros_zero` post-loop verdict** (after the `while` loop, before the
   final timing, `exploration.cpp:~465`). Computed **once per call**, not
   per-iteration, from the frame's accumulated `dag_excl`:

   ```cpp
   if (g_tt_node_dag && path_forced_draw != nullptr) {
       const Move _bm = get_best_score_move(alpha, beta, /*as today*/, /*as today*/, &dag_excl);
       *path_forced_draw = (!_bm.is_null_move()) && dag_excl.contains(_bm);
   }
   ```

   The early-return paths (`_is_terminal`, `!_can_explore`, `iterations <= 0`,
   `_got_moves <= 0`, recursion-guard) leave `*path_forced_draw` at its
   caller-initialized `false`: a terminal mate/draw already carries the
   authoritative value in the shared `_deep_evaluation`, so no path override is
   needed there.

The bubble closes recursively: a parent's `explore_random_child` learns the
descended child's verdict via `&child_forced_draw` (step 2), promotes the edge
into the parent frame's `dag_excl`, which feeds the parent's path-aware
`get_best_score_move` (steps 3-4) and the parent's own post-loop verdict
(step 5), which its own parent reads — up to the root, which is unshared
(`_parent_count <= 1`) and therefore gets the corrected value persisted into
`_deep_evaluation` for display/play.

## Soundness against the two repro positions

### Repro 1 — `8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w` and KP(h) vs K (true draw, must NOT false-win)

Every line repeats or is itself a forced draw. Each descended child recursively
returns `child_forced_draw = true`; each §3 cut adds its edge. Over MCTS
iterations every edge enters the frame's `DagExcl`. Path-aware
`get_best_score_move` then scores **every** child as `dag_draw_eval()` →
`_bm ∈ DagExcl` → node verdict = forced draw → the bool bubbles up the stack
through the (shared, massively multi-parent) KP-vs-K nodes **without mutating
their shared `_deep_evaluation`** → reaches the unshared root → root
`_deep_evaluation` = draw. The current opt-1 fails here precisely because the
draw dies at the first shared node (`_parent_count > 1` blocks persistence and
nothing forwards it); the bubbled bool fixes exactly that. ✓

### Repro 2 — won blocked-pawn position (transient cycle, must NOT phantom-draw)

The real progress move `C` makes progress: its line does not repeat, its
recursive verdict is `false`, so `C` never enters `DagExcl`. The king-shuffle
edges do repeat → §3-cut → enter `DagExcl` → scored as `dag_draw_eval()` in the
backup ranking. Path-aware `get_best_score_move` therefore ranks `C` (real
winning shared eval) **above** the draw-pinned shuffle edges → `_bm = C`,
`C ∉ DagExcl` → verdict `false` → the win bubbles. The cyclic edges can no
longer outrank the real win even though their *shared* eval is also "winning",
because in the backup ranking they are pinned to 0. ✓

### Why this is not the reverted Option D

Option D (`f7a5227`, reverted `1e5461f`) applied
`position_is_draw_by_repetition(path, child)` inside
`pick_random_child`/`get_move_scores`/`get_node_score` for **every child on
every selection** — turning "this traversal happens to revisit child via a
transposition" into a draw value, poisoning exploration and broadly
mis-scoring winning children reached via a cyclic path. This design:

- **Selection is untouched.** `pick_random_child` stays eval-driven; it only
  *skips* already-proven `DagExcl` edges (anti-spin, already shipped). No
  per-pick `position_is_draw_by_repetition` in any scoring path.
- The path-draw substitution happens **only in the backup's best-move
  ranking**, **only for edges in `DagExcl`** — a set populated solely by the
  §3 *structural* cut and by recursively *proven* forced-draw children, never
  by a bare per-pick "did this traversal revisit" test.

Tight proven set in the backup, not a broad predicate in selection.

## Invariants / constraints (non-negotiable, all preserved)

- **772183a:** no path-local value is ever written to a shared
  `Node`/`ChildLink`/`node_map`. `get_best_score_move` only reads.
  `_deep_evaluation` is persisted only when `_parent_count <= 1`. The verdict
  travels by the stack-threaded `bool*` and the stack-local `DagExcl`.
- **OFF byte-identical:** every new branch is `g_tt_node_dag`-gated;
  `get_best_score_move`'s new param defaults `nullptr` and only the DAG backup
  / post-loop calls pass it; the removed `Evaluation* path_local_eval` was
  already DAG-only. OFF = post-`c749698` tree behaviour at the byte.
- **Performance #1:** no allocation. ON adds, on the **backup path only** (not
  the `pick_random_child` selection hot loop): a ≤24 linear `DagExcl` scan per
  child, plus one extra `get_best_score_move` per `grogros_zero` call for the
  post-loop verdict (O(children), once). Removing the `Evaluation*` copies is a
  net win vs the prior opt-1 design. Profiled at the user gate.
- Commits English, ASCII, conventional, `(#11)`, no AI attribution. French
  comments use real Unicode (é, è, à, ç, …).

## Honest residual (inherent, accepted — not introduced here)

Within a single iteration the backup is still MCTS-pull-based for
non-descended children (their path-free shared `_deep_evaluation`);
correctness emerges over iterations as each child is descended and its verdict
bubbles. The spec already accepts "MCTS convergence, not an instant flip"
(memory `project-tt-main-search-state`, Option-D gate note). A position that
is a forced draw only because of the twofold search horizon but would win
deeper is a fundamental engine-wide repetition-pruning limitation, explicitly
intended (`search_repetition_limit = 2`, `exploration.cpp:7`), not introduced
by this design.

## Out of scope (deferred, tracked in memory; do not fix here)

`play_move_keep` (`gui.cpp:886`) × DAG re-root corruption cluster (buffer-full
mismatch, "no moves in grogros_zero" spam, post-move mispricing, corrupted
board on scrolling variations); Bug E (UI crash, PC sleep/wake); standalone
bug #1 (`get_main_depth` returns 0 before a heavy TT-hit node on a fresh
search). Bug 2 model A and the display threefold
(`display_repetition_limit = 3`) are intact and validated — untouched.

## Acceptance (manual USER gate — assistant cannot run the raylib GUI)

Build: MSBuild `opti_chess.sln` Debug x64, expect `EXITCODE=0`.

1. **OFF byte-identical:** with the toggle OFF (do not press **O**), PERFT 1/2
   + EVALUATION identical to the post-`c749698` baseline.
2. **Repro 1:** `8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7` and a KP(h) vs K
   theoretical draw, DAG **ON** → engine no longer thinks it wins / circles;
   root eval converges to draw over iterations.
3. **Repro 2:** the won "K vs K + blocked pawns" position, DAG **ON** → shows
   the win and **stays** won (no drift to phantom draw); navigating moves does
   not flip draw↔win for the eval reason (the separate re-root corruption
   cluster is out of scope).

## Suggested first plan task

A design-validation gate task confirming, against `exploration.cpp` as it
stands at branch tip, the exact `get_best_score_move` signature insertion
point and `consider_standpat`/`qdepth` argument forwarding, and that the
`max_eval`/`max_avg_score` pre-pass substitution keeps the `get_node_score`
softmax consistent — before any hot-path edit.
