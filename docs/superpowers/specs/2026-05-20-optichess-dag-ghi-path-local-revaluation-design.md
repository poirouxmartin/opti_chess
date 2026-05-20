# DAG GHI — path-local revaluation (Approche A) — Design Spec

**Date:** 2026-05-20
**Branch:** `feature/tt-main-search`
**Baseline:** `0608af5` (revert of Plan B value-bubbling, code-state == `e3e7baa`)
**Issue:** #11 — DAG soundness on both reference repros (KP(h)-vs-K false-win + won blocked-pawns phantom-draw)
**Toggle:** `g_tt_node_dag` (existing, default OFF)

---

## 1. Goal

Make `g_tt_node_dag == true` (DAG with node sharing) **sound** on the two reference repros while preserving the DAG's node-sharing benefit on every node where path history cannot affect the value.

- **Repro 1:** `8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7` (and any KP(h)-vs-K) — engine must converge to draw, not false-win.
- **Repro 2:** Won "K vs K + blocked pawns" position — engine must show win and stay won, not phantom-draw.

Performance remains project priority #1: the design must be OFF byte-identical and add no allocation/branch on the hot path under OFF.

## 2. Background — why the previous 5 attempts failed

The defect is a textbook instance of the **GHI (Graph History Interaction)** problem in game-tree search: under the DAG, a node's value can depend on the path history (repetition counter), so naively reusing a stored value across paths is unsound.

Five prior attempts all conflated a **traversal-cycle predicate** (the position recurs on the current search path) with a **forced-draw verdict**, and either:

- Mutated shared state with a path-local draw → corruption (`772183a` original fix), or
- Scored cycle-touch children as `dag_draw_eval()` in selection (Option D, `f7a5227`, reverted `1e5461f`) → phantom draw on transposition cycles in winning positions, or
- Bubbled a `bool path_forced_draw` verdict via `best path-aware move ∈ DagExcl` over a **partially-expanded** MCTS children set (Plan B, `735916c..8a686ab`, reverted `0608af5`) → spurious forced-draw whenever MCTS happened to focus on cycle-touch children before enumerating all legal moves.

Key reframing that drives this design: **the §3 predicate** `position_is_draw_by_repetition(branch_history, child)` **is not a forced-draw proof. It is the signal "this child's shared `_deep_evaluation` is untrustworthy on the current path" — nothing more.** The correct action is to *exclude* the child from the path-local backup, not to score it as a draw, and to gate any path-local verdict on **full enumeration** so that MCTS partial-expansion cannot produce spurious draws.

This is also why approach B (HMC-safe sharing only) was rejected: long reversible streaks are precisely the regime of the two repros, so a HMC-based safe-share criterion would lose the DAG benefit exactly where the bug occurs. Approach C (probabilistic path-stamp) was rejected as non-deterministic. Approach A — *honest* path-local revaluation — is the only candidate that (a) attacks the actual conceptual error, (b) preserves sharing on path-independent nodes, and (c) handles both repros by the same mechanism.

## 3. Mechanism

`_deep_evaluation` continues to be the **shared, best-effort path-independent** value, written only by the normal MCTS backup. Invariant `772183a` is preserved: no path-local data ever mutates a shared field except at unshared nodes.

A **second channel** carries the path-local value through `grogros_zero` via an out-parameter, computed once per call in a post-loop step.

### 3.1 Cycle-touch detection (unchanged from §3)

A child M of node N is **cycle-touch on the current path** iff:

```cpp
position_is_draw_by_repetition(branch_history, *M->_board)
```

(`branch_history` = current threaded `PositionHistory` for this traversal; existing B-1 plumbing.) Predicate unchanged — only its interpretation changes.

### 3.2 Path-local backup at node N

Computed exactly once in `grogros_zero`'s post-loop, after the iteration loop and before the time accounting (i.e., where the reverted Plan B verdict block was, `exploration.cpp:467`):

1. **Strict enumeration gate.** If `children_count() < _board->_got_moves`, emit *no* override. The out-param is left at the caller-initialized fallback (`child->_deep_evaluation`, see §4.3). This single rule prevents every partial-expansion false positive.

2. **Full enumeration: partition children.**
   - `S_c` = children M with M cycle-touch on `branch_history`
   - `S_nc` = `_children \ S_c`

3. **Determine path-local value.**
   - If `S_nc == ∅` (all legal moves are cycle-touch): emit `dag_draw_eval()`. This is a *true minimax-with-repetition=0 forced draw* — the side to move has no non-repeating move.
   - Otherwise: emit `minimax(S_nc ∪ {dag_draw_eval() as virtual candidate})` from the perspective of `_board->get_color()`. `dag_draw_eval()` enters as one additional candidate (not per-cycle-touch-child) representing the side-to-move's option to voluntarily play a cycle-touch move and accept a draw.
   - **Per-child contribution rule (essential for bubbling through shared interior nodes):** for the one child whose `grogros_zero` was just invoked in the current `explore_random_child` call (call it the *substitution child* with `Move M_sub` and `Evaluation V_sub`), use `V_sub` for that child's contribution instead of `child->_deep_evaluation`. For all other children: use `child->_deep_evaluation` (best-effort fallback). The substitution lets a deep override flow into the parent's minimax even when the parent itself is shared (`_parent_count > 1`) and cannot have its `_deep_evaluation` persisted — bubbling proceeds via the *out-param chain*, not via mutated shared state.

### 3.3 Differences from reverted Plan B (the critical fix)

| Aspect | Plan B (reverted, regressed) | Approach A |
|---|---|---|
| Cycle-touch child treatment in backup ranking | scored as `dag_draw_eval()` in `get_best_score_move` | **skipped** entirely from `S_nc` |
| `dag_draw_eval()` in ranking | once per cycle-touch child (multiplicity) | once as a single virtual candidate |
| Out-channel type | `bool path_forced_draw` | `Evaluation path_local_eval` (+ emission flag) |
| Verdict criterion | `best path-aware move ∈ DagExcl` | `S_nc == ∅` OR explicit draw-choice in minimax |
| Partial-expansion handling | none — verdict computed regardless | **strict enumeration gate**, no override emitted until `children_count() >= _got_moves` |
| Touches `get_best_score_move` | yes (path-aware `DagExcl` param) | **no** — selection / `pick_random_child` / `get_node_score` / `get_move_scores` strictly untouched |

The last row is structural: the path-local computation is a **separate post-loop helper**, not a parameterization of the existing ranking. This preserves Option-D's hard-won lesson ("selection untouched") and isolates GHI handling to a clearly bounded code region.

## 4. Code structure

### 4.1 Files

- `opti_chess/exploration.h` — three signature edits (`grogros_zero`, `explore_random_child`, new helper declaration).
- `opti_chess/exploration.cpp` — matching definitions, the post-loop call site, the helper body, and the persistence guard.

No other files touched. No new files.

### 4.2 Signatures

Replace the current (baseline `e3e7baa`) `grogros_zero` signature:

```cpp
void Node::grogros_zero(
    BoardBuffer* board_buffer, Evaluator* eval,
    const double alpha, const double beta, const double gamma,
    int iterations, int quiescence_depth,
    Network* network = nullptr,
    PositionHistory* path_history = nullptr,
    Evaluation* path_local_eval = nullptr,
    bool* path_local_emitted = nullptr);
```

Replace `explore_random_child`:

```cpp
void Node::explore_random_child(
    BoardBuffer* board_buffer, Evaluator* eval,
    double alpha, double beta, double gamma, int quiescence_depth,
    Network* network = nullptr,
    PositionHistory* path_history = nullptr,
    DagExcl* dag_excl = nullptr,
    Evaluation* path_local_eval = nullptr,
    bool* path_local_emitted = nullptr);
```

New helper declared in `exploration.h`, defined in `exploration.cpp`:

```cpp
// Computes the path-local value of this node on the current traversal,
// per §3 of the GHI design (2026-05-20). Returns true and writes *out if
// a path-local override is emitted (strict enumeration gate satisfied);
// returns false otherwise (caller keeps its fallback = _deep_evaluation).
// DAG-gated by caller — this function assumes g_tt_node_dag == true.
//
// substitution_move / substitution_value: optional override for one
// child's value (the child just descended into in the calling
// explore_random_child). Pass a null Move to disable substitution
// (all children use their shared _deep_evaluation).
bool Node::compute_path_local_eval(
    const PositionHistory& path_history,
    const Move& substitution_move,
    const Evaluation& substitution_value,
    Evaluation* out) const;
```

`get_best_score_move`, `pick_random_child`, `get_node_score`, `get_move_scores`, `explore_new_move` signatures: **unchanged**. `Evaluation* path_local_eval` defaults to `nullptr` so every existing call site is source-compatible and inert.

### 4.3 Out-param contract (caller initialization)

The out-param convention is `Evaluation*` carrying a value plus a `bool*` emission flag. Caller convention in `explore_random_child` (around line 817 baseline):

```cpp
Evaluation child_local = child->_deep_evaluation;  // safe fallback
bool child_local_emitted = false;
{
    PathScope _ps(branch_history, *child->_board);
    child->grogros_zero(
        board_buffer, eval, alpha, beta, gamma, 1, quiescence_depth,
        network, &branch_history,
        g_tt_node_dag ? &child_local : nullptr,
        g_tt_node_dag ? &child_local_emitted : nullptr);
}
```

The child's `grogros_zero` post-loop calls `compute_path_local_eval`; if it returns true (gate satisfied), `*path_local_eval = out_value; *path_local_emitted = true`. If false, both out-params are left untouched (child_local retains the shared fallback, child_local_emitted stays false).

Terminal early-returns (`_is_terminal`, `_got_moves <= 0`, `_can_explore == false`, recursion guard) never reach the post-loop — caller's initialized fallback (`child->_deep_evaluation`, which is the terminal's stored mate/draw value) is correct in those cases.

### 4.4 Path-local emission and persistence in `explore_random_child`

Both the *emission to caller* (via the out-param) and the *persistence at this node* (when `_parent_count <= 1`) live in `explore_random_child`, immediately after the normal backup at `exploration.cpp:807-808`. Single call site, single substitution argument, no separate grogros_zero post-loop step required.

Replace the baseline backup line with:

```cpp
// Normal MCTS backup — shared, path-independent. SEMANTICALLY UNCHANGED.
const Move _bm = get_best_score_move(alpha, beta);
_deep_evaluation = _children[_bm]._node->_deep_evaluation;
bool path_local_persisted_this_iter = false;

// #11 GHI path-local revaluation (design 2026-05-20). Compute this node's
// path-local value with the just-descended child's emission substituted
// in (essential for bubbling through shared interior nodes whose
// _deep_evaluation cannot be persisted). Strict enumeration gate inside
// compute_path_local_eval — emits override only when every legal move has
// been added to _children. Out-param emission ALWAYS bubbles up the
// out-param chain regardless of _parent_count (never mutates shared
// state). Persistence to _deep_evaluation ONLY at _parent_count<=1
// (invariant 772183a).
if (g_tt_node_dag && path_local_eval != nullptr && path_history != nullptr) {
    Evaluation node_local;
    const Move& sub_move = child_local_emitted ? move : Move();
    if (compute_path_local_eval(*path_history, sub_move, child_local, &node_local)) {
        *path_local_eval = node_local;
        if (path_local_emitted != nullptr) *path_local_emitted = true;
        if (_parent_count <= 1 && !(node_local == _deep_evaluation)) {
            _deep_evaluation = node_local;
            path_local_persisted_this_iter = true;
        }
    }
}
```

Notes:
- `Move()` produces a null Move (the existing engine convention; `Move::is_null_move()` exists per `exploration.cpp:735`).
- When `child_local_emitted == false` (child's gate not satisfied), substitution is disabled by passing a null Move; the helper falls through to `child->_deep_evaluation` for every child, including the just-descended one. Correct because the child has no refined override to substitute.
- The helper is called regardless of `child_local_emitted`: even without substitution, this node's own enumeration may have just been completed (e.g., last `explore_new_move` filled the children set), so emission may become valid here.
- No separate post-loop computation in `grogros_zero`. The path_local_eval out-param is left at its last-iteration value when grogros_zero returns; if no iteration was `explore_random_child` (e.g., only `explore_new_move`), no emission occurs and the caller's fallback stands.
- The `Evaluation::operator==` may not exist; if not, the implementation adds an inline header `operator==` comparing `_value` and `_avg_score` (sufficient for change-detection).

### 4.5 Plan A (TT writeback) guard

The TT writeback at `exploration.cpp:847` runs after the backup region and stores `_deep_evaluation` into the flat TT under Plan A. Persisting a path-local override into `_deep_evaluation` and then writing it back to the TT would leak path-local state into the path-independent TT — readable from any other path.

**Guard:** add `!path_local_persisted_this_iter` to the writeback condition. The local boolean is declared and set in §4.4:

```cpp
if (g_tt_main_search && !_is_terminal && !_is_stand_pat_eval
    && _deep_evaluation._evaluated
    && !(g_tt_node_dag && path_local_persisted_this_iter)) {
    transposition_table.store(_board->_zobrist_key, ...);
}
```

This is the only modification to the Plan A writeback site — gated, additive, OFF-inert (under OFF, `path_local_persisted_this_iter` is initialized `false` and never set, so the new guard term `!(g_tt_node_dag && false)` is always true; the additional conjunct doesn't alter the writeback decision).

## 5. OFF byte-identicality

Every new behaviour is gated `g_tt_node_dag` AND triggered only when out-params are non-null:

| Site | OFF guard |
|---|---|
| `explore_random_child` path-local block (§4.4) | `if (g_tt_node_dag && path_local_eval != nullptr && path_history != nullptr)` |
| `explore_random_child` child recursion arg-passing | passes `g_tt_node_dag ? &child_local : nullptr` and `g_tt_node_dag ? &child_local_emitted : nullptr` |
| `_deep_evaluation = node_local;` persistence write | inside §4.4 guard above → unreachable under OFF |
| TT writeback guard (§4.5) | only adds `&& !(g_tt_node_dag && path_local_persisted_this_iter)`; OFF: first conjunct false → guard inert |
| New helper `compute_path_local_eval` | only called from the §4.4 DAG-gated site |
| Signatures | all new params default `nullptr`/`false`; existing OFF callers pass nothing → defaults bind |

OFF behaviour: byte-identical to baseline `e3e7baa` / commit `0608af5` by construction.

## 6. Perf characteristics

- Hot path under OFF: no change (gated out).
- Hot path under ON: `compute_path_local_eval` is called *at most once per `explore_random_child` call* (i.e., once per refinement iteration of `grogros_zero`'s loop), and only when `path_local_eval != nullptr`. Each call is `O(|_children|) * O(|branch_history|)` for the cycle-touch scan plus an `O(|S_nc|)` minimax. Comparable to one `get_best_score_move`. No allocation (stack-only locals, no heap, no new containers).
- Iterations that go through `explore_new_move` (initial expansion of a child not yet in `_children`) do **not** call the helper — path-local computation is purely a backup-region concern.
- Optional micro-opt (deferred until measured): build a small stack-allocated hash-set of `branch_history`'s Zobrist keys once per `compute_path_local_eval` call, lookup O(1) per child. Apply only if profiling shows the per-child `position_is_draw_by_repetition` is hot.

No regression risk to OFF perf; ON perf cost is bounded and confined to two helper calls per frame.

## 7. Repros — traced behaviour

### 7.1 KP(h)-vs-K (Repro 1)

- Few legal moves per node (typically 3-5). MCTS rapidly satisfies `children_count() >= _got_moves` at each node.
- The attacking king maneuver visits positions that recur in `branch_history` (same key, same trait — true game repetition). Every "progress" attempt becomes cycle-touch.
- At deep nodes: `S_nc == ∅` → emit `dag_draw_eval()` → propagate up.
- Recursively the verdict bubbles up the out-param chain, with per-child substitution (§3.2 step 3) carrying the draw through any shared interior nodes whose `_deep_evaluation` cannot be mutated. At the root (`_parent_count <= 1`) the persistence rule (§4.4) writes `_deep_evaluation = dag_draw_eval()`. GUI shows draw. ✓
- Convergence is over multiple MCTS iterations (not instant), consistent with the established acceptance pattern.

### 7.2 Won blocked-pawns (Repro 2)

- More legal moves per node (8-15), but bounded; MCTS enumerates within iteration budget on focused lines.
- A winning maneuver exists by hypothesis. The winning child of any node N is reached by a move that does **not** make a key already present in `branch_history` (else it would be a real game repetition, contradicting "winning maneuver").
- Therefore the winning child ∈ `S_nc` at every relevant N. The minimax over `S_nc ∪ {dag_draw_eval()}` picks the winning value (for the winning side, winning eval > draw).
- No path-local draw override emitted, no contamination. ✓
- Even if some siblings of the winning child are cycle-touch (transposition cycles in the search graph), they are simply skipped from `S_nc` — they cannot drag the verdict to draw.
- During partial expansion (`children_count() < _got_moves`), no override is emitted by the gate → MCTS proceeds with shared `_deep_evaluation` (winning, propagated from the explored winning fils). No phantom draw possible. ✓

## 8. Edge cases

1. **Voluntary perpetual (losing side forces draw by repetition).** `S_nc` non-empty but all its values < `dag_draw_eval()` for the side to move. The virtual draw candidate wins the minimax. Path-local value = draw. Game-theoretically correct. ✓

2. **Terminal nodes (mate / stalemate / insufficient material / `_is_terminal`).** `grogros_zero` early-returns before reaching the post-loop. No override emitted. Caller's fallback (`child->_deep_evaluation`) carries the terminal's true value (mate/draw). ✓

3. **`_got_moves <= 0` early-return.** Same as terminal — no override, fallback used.

4. **`_parent_count == 0` (root or detached).** Persistence allowed (`<= 1`). ✓

5. **Plan A interaction.** Handled by §4.6 — TT writeback blocked when persistence occurred this iteration. Prevents path-local leak into flat TT.

6. **Display (`get_exploration_variants`, `get_main_depth`).** Read shared `_deep_evaluation` without path threading. At root (persisted) the display is correct. At shared interior nodes (`_parent_count > 1`, no persistence) the display uses the shared value — best-effort, consistent with the documented residual that display has no path context.

7. **`branch_history` consistency.** B-1 acquisition: a single threaded `PositionHistory*` is pushed/popped via `PathScope` around child recursion. No change required.

8. **`Evaluation` equality.** If `Evaluation::operator==` does not exist, add a header-only inline `operator==` comparing `_value` and `_avg_score` (the change-detection-relevant fields). Trivial.

9. **`compute_path_local_eval` called with empty `_children`.** Defensive: if `_children.empty()`, return `false` (no override). The post-loop site already guards via `_got_moves > 0` (early-return at `:421`) so this is only a safety net.

10. **`path_history == nullptr` at `grogros_zero` entry.** Post-loop call guarded `path_history != nullptr` (§4.4). OFF callers and any unusual non-DAG caller path are inert.

## 9. Out of scope / known residuals

- **`get_exploration_variants` / `get_main_depth` PV display on shared interior nodes** — they read `_deep_evaluation` without path threading. Already documented residual (memory 2026-05-18); not addressed here. May be revisited if PV display is materially confusing after this change stabilizes.
- **`play_move_keep` re-root corruption cluster (Bugs A/B/C/D, memory 2026-05-18)** — independent defect cluster, deferred.
- **Plan A keep-vs-revert decision** — orthogonal, unaffected.
- **Quiescence path** — not under `grogros_zero`'s frame; this design does not touch the quiescence code path.
- **`search_repetition_limit` tuning (2 vs 3)** — orthogonal; baseline twofold preserved. Threefold display limit (commit `8f7eb74`) stays.

## 10. Acceptance gate (Task 5 of the upcoming plan)

The assistant cannot run the raylib GUI; acceptance is a USER gate. Two repros + OFF guard:

1. **OFF byte-identical.** Toggle OFF (do not press `O`). Confirm PERFT 1/2 + EVALUATION identical to pre-change baseline on the usual positions.
2. **Repro 1 — no false win.** `8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7` and KP(h)-vs-K, DAG ON, multiple batches. The root evaluation converges to a draw (MCTS convergence, not necessarily instant).
3. **Repro 2 — no phantom draw.** Won blocked-pawns position, DAG ON. Shows a win and stays won across iterations. (The separate `play_move_keep` re-root corruption may still affect navigating played moves; out of scope.)
4. **Regression check.** PERFT 1/2 + EVALUATION stable with DAG ON.
5. Record outcomes in `opti_chess/BUGFIXES.md` #11 and commit a `docs(bugfixes)` follow-up after the gate.

## 11. Self-review note

This design is a refinement of the 2026-05-19 bubble-bool design with **three structural changes**:

1. **Path-local value channel** instead of `bool` forced-draw verdict (`Evaluation* path_local_eval` + `bool* path_local_emitted` out-params on `grogros_zero` and `explore_random_child`).
2. **Strict enumeration gate** in the helper — no override emitted until `children_count() >= _got_moves`. This single rule eliminates the partial-expansion false-positives that regressed Plan B.
3. **Per-child substitution in the helper** (`substitution_move` / `substitution_value`) — essential for bubbling a deep override up through shared interior nodes whose `_deep_evaluation` cannot be persisted. The 2026-05-19 design omitted this and would have failed for the same reason its bool predecessor failed.

The 2026-05-19 design's other claims (DagExcl as proven-forced-draw, value computed once post-loop, no shared mutation, OFF byte-identical) are revised here: the proven-forced-draw membership predicate is rejected as the GHI-misframing of the previous five attempts; instead, the path-local channel honestly computes the minimax with cycle-touch children **skipped** (not draw-scored) and `dag_draw_eval()` admitted as a single virtual candidate. The computation lives **inside `explore_random_child`** (a single call site immediately after the normal backup), not in a separate `grogros_zero` post-loop step. All other architectural invariants (no shared mutation outside `_parent_count<=1`, OFF byte-identical, perf #1, selection / `pick_random_child` / `get_node_score` / `get_move_scores` strictly untouched) are preserved.
