# #11 attempt 7 — Per-edge cycle flag + all-cycle verdict at node — Design Spec

**Date:** 2026-05-21
**Branch:** `feature/tt-main-search`
**Baseline:** `02dedd4` (DAG metrics logging shipped + ON/OFF sequencing for repros)
**Issue:** #11 — DAG GHI soundness on the two reference repros
**Toggle:** `g_tt_node_dag` (existing, default OFF)

---

## 1. Goal

Make `g_tt_node_dag == true` STRUCTURALLY CORRECT on Repro 1 (theoretical-draw KP(h)-vs-K) WITHOUT regressing Repro 2 (winning pawn endgame). Specifically, after the fix:

- **Repro 1 (`6k1/8/7P/7K/8/8/8/8 w - - 3 72`)**: root eval converges to draw (`0` / `avg_score == 0.5`) within a small number of batches — ideally batch 0 like DAG OFF does.
- **Repro 2 (`8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7`)**: root eval reaches winning magnitude (`avg_score >= 0.7`) at least as fast as the current baseline (~0.9s manual / ~batch 19 of 20×3000 iters in the variable observed runs).

DAG ON must remain **at least as performant and at least as accurate as DAG OFF** on both repros after the fix. Anything worse than DAG OFF is a regression.

This is the 7th fix attempt on issue #11. The six prior attempts are catalogued in memory `project_tt_main_search_state.md`. The dominant prior failure mode is **conflating "edge cycle-touch on this path" (a search-graph artifact) with "node forced draw" (a game-theoretic property)**. This design avoids that conflation by requiring **all children of a node** to be cycle-touch this batch before emitting the draw verdict — not per-edge.

## 2. Baseline state

The fix attacks the search code starting from tip `02dedd4`, which contains:

- §3 cut (`exploration.cpp:790` region): when `position_is_draw_by_repetition(*branch_history, *child->_board)` returns true, the cut adds the move to a stack-local `DagExcl` (anti-spin opt-3), writes `*path_local_eval = dag_draw_eval()` (opt-1 partial), increments `_iterations`, returns.
- Backup region (`exploration.cpp:807-827`): on the parent's `_bm`, if `_bm ∈ dag_excl` then write `dag_draw_eval()` to `_deep_evaluation` only when `_parent_count <= 1` (opt-1 unshared-persist).
- The opt-1 channel is THERE but UNDERPOWERED: the partial only fires when the parent itself is unshared (`_parent_count <= 1`), which is never true for the shared cycle nodes in Repro 1 (their `_parent_count` is 4-12).
- `dag_log` metrics observability is fully wired (TU `dag_log.{h,cpp}`, instrumentation at §3 cut + pick_random_child + grogros_zero entries + GUI session/batch hooks). Two repro entry points (`run_dag_repro_1`/`_2`) bind keys `1`/`2` and run ON then OFF in sequence.

The DAG metrics logging spec is `docs/superpowers/specs/2026-05-20-optichess-dag-metrics-logging-design.md` (revised). It is the foundation this fix builds on — every metric added by this design extends the existing `dag_log` schema.

## 3. Mechanism — per-edge flag + all-cycle gate at the node

### 3.1 Per-edge state on `ChildLink`

Add one field to `ChildLink` (declared in `exploration.h` near `_propagated_nodes`):

```cpp
// #11 attempt-7 (cf. design 2026-05-21). Marqueur de "cette arête a été
// §3-cut pendant le batch courant". Comparé à g_dag_batch_seq pour reset
// paresseux (pas besoin d'itérer toutes les ChildLink à chaque batch).
uint32_t _cycled_batch_seq = 0;
```

Semantics: when the `§3` cut fires on a child via move `M` from parent `P`, the implementation sets `P->_children[M]._cycled_batch_seq = g_dag_batch_seq`. Reading: edge `M` is "cycled this batch" iff `_cycled_batch_seq == g_dag_batch_seq`. Lazy reset: when `g_dag_batch_seq` advances, all stale values become "not cycled" without explicit clearing.

Memory cost: 4 bytes per `ChildLink`. With ~hundreds of thousands of ChildLinks in a deep search tree, that's ~MB scale. Acceptable.

### 3.2 Global batch sequence counter

In `exploration.cpp` (or `exploration.h` if a header-visible declaration is needed):

```cpp
// #11 attempt-7. Sequence numérique global incrémenté à chaque batch
// grogros_zero racine. Sert de marqueur de fraîcheur pour
// ChildLink::_cycled_batch_seq (reset paresseux par génération).
extern uint32_t g_dag_batch_seq;
```

Defined in `exploration.cpp`. Initialised to `1` (NOT 0 — so initial `_cycled_batch_seq = 0` reads as stale). Incremented by `GUI::grogros_analysis` at batch start (a single `++g_dag_batch_seq;` before the existing call to `_root_exploration_node->grogros_zero(...)`).

The repro entry points `GUI::run_dag_repro` also increment it between batches inside their for-loop (same pattern as `grogros_analysis`).

### 3.3 §3 cut instrumentation

In `Node::explore_random_child`, the §3 cut block becomes (additions marked):

```cpp
if (g_tt_node_dag && position_is_draw_by_repetition(*branch_history, *child->_board)) {
    if (dag_excl != nullptr) dag_excl->add(move);

    // existing opt-1 path_local_eval write — PRESERVED VERBATIM
    if (path_local_eval != nullptr) {
        *path_local_eval = dag_draw_eval();
    }

    // #11 attempt-7 — marque l'arête comme cycle pour CE batch.
    // Le verdict "tous les enfants cyclent" est calculé en fin de grogros_zero.
    this->_children[move]._cycled_batch_seq = g_dag_batch_seq;

    // existing dag_log instrumentation — PRESERVED VERBATIM
    if constexpr (dag_log::enabled) {
        /* ... pred_fire + counters ... */
    }

    _iterations++;
    return;
}
```

`this` in `explore_random_child` IS the parent node (the function is called on parent, with `move` selected to descend to `child`). So `this->_children[move]` is the right edge.

### 3.4 All-cycle verdict at `grogros_zero` tail

After the iteration loop in `Node::grogros_zero`, BEFORE the existing time/return code, add:

```cpp
// #11 attempt-7 — verdict all-cycle (cf. design 2026-05-21 §3.4).
// Si tous les coups légaux ont été §3-cut au moins une fois pendant
// CE batch (chaque ChildLink::_cycled_batch_seq == g_dag_batch_seq),
// alors ce nœud est forced-draw sur le chemin courant. On émet le verdict
// via path_local_eval (canal existant, baseline 02dedd4). Persistance
// dans _deep_evaluation UNIQUEMENT si _parent_count <= 1 (invariant
// 772183a — pas de mutation partagée). OFF / hors DAG : tout est élidé.
bool all_cycle_persisted_this_iter = false;
if (g_tt_node_dag && path_local_eval != nullptr) {
    // Porte d'énumération stricte : tous les coups légaux doivent être
    // dans _children (sinon le verdict serait spéculatif).
    if (children_count() >= static_cast<size_t>(_board->_got_moves)) {
        bool all_cycle = true;
        for (auto const& [m, link] : _children) {
            if (link._cycled_batch_seq != g_dag_batch_seq) {
                all_cycle = false;
                break;
            }
        }
        if (all_cycle) {
            const Evaluation draw = dag_draw_eval();
            *path_local_eval = draw;
            if (_parent_count <= 1 && !(draw == _deep_evaluation)) {
                _deep_evaluation = draw;
                all_cycle_persisted_this_iter = true;
            }
            if constexpr (dag_log::enabled) {
                dag_log::bump(dag_log::Counter::all_cycle_verdicts_emitted);
                if (all_cycle_persisted_this_iter) {
                    dag_log::bump(dag_log::Counter::all_cycle_persisted);
                }
            }
        } else if constexpr (dag_log::enabled) {
            // Strict enum gate passed but not all-cycle — silent.
        }
    } else if constexpr (dag_log::enabled) {
        dag_log::bump(dag_log::Counter::enum_gate_blocks);
    }
}
```

`all_cycle_persisted_this_iter` is consumed by §3.5 (TT writeback guard). `Evaluation::operator==` was added during Approach A and reverted; this design re-adds it (header-only inline in `board.h`, identical to the prior implementation).

### 3.5 Plan A TT writeback guard

The Plan A TT writeback at `exploration.cpp:831-832` (inside `explore_random_child` after the existing backup region) gets one additional conjunct:

```cpp
if (g_tt_main_search && !_is_terminal && !_is_stand_pat_eval
    && _deep_evaluation._evaluated
    && !(g_tt_node_dag && all_cycle_persisted_this_iter)) {
    transposition_table.store(_board->_zobrist_key, ...);
    if constexpr (dag_log::enabled) { /* existing or no-op */ }
} else if constexpr (dag_log::enabled) {
    // If g_tt_main_search etc. true but blocked by the new conjunct, bump:
    if (g_tt_main_search && !_is_terminal && !_is_stand_pat_eval
        && _deep_evaluation._evaluated
        && g_tt_node_dag && all_cycle_persisted_this_iter) {
        dag_log::bump(dag_log::Counter::tt_writeback_blocked);
    }
}
```

`all_cycle_persisted_this_iter` is a local `bool` declared at the top of the §4.4 block above; it must be reachable at the TT-writeback site (same enclosing function scope — `Node::explore_random_child`).

**However**: the §3.4 verdict block lives in `Node::grogros_zero`, NOT in `Node::explore_random_child`. The TT writeback is in `explore_random_child`. So `all_cycle_persisted_this_iter` from the verdict block in `grogros_zero` is NOT in scope at the TT writeback in `explore_random_child`.

**Resolution**: the TT writeback's guard uses the *path_local_eval value being a draw* as the signal instead of `all_cycle_persisted_this_iter`. Specifically:

```cpp
// In explore_random_child's TT writeback site:
const bool path_local_is_draw = (g_tt_node_dag && path_local_eval != nullptr
    && (*path_local_eval == dag_draw_eval()));
if (g_tt_main_search && !_is_terminal && !_is_stand_pat_eval
    && _deep_evaluation._evaluated
    && !path_local_is_draw) {
    transposition_table.store(_board->_zobrist_key, ...);
}
```

Reading `*path_local_eval` after the child's `grogros_zero` has populated it (via the verdict block in §3.4) gives us the path-local verdict. If it's a draw, don't write to TT (would leak path-local state into the path-independent TT).

This is a small read of an existing pointer; cheap.

### 3.6 Reset semantics

`g_dag_batch_seq` is `uint32_t`. Increments once per batch (key press or GUI tick). Overflow after ~4 billion batches — at one batch every 16ms (60 FPS), that's ~2 years of continuous engine running. Acceptable; the wraparound case can be addressed separately if it ever matters.

ChildLink instances are created fresh in NodeBuffer when nodes expand. `_cycled_batch_seq = 0` initialised; stays stale forever unless §3 cut fires on this edge. No iteration-cost penalty for the reset.

## 4. Metrics — comprehensive logging

The fix extends the existing `dag_log` schema with metrics specific to the all-cycle mechanism. Updated `Counter` enum:

```cpp
enum class Counter {
    // Existing (baseline 02dedd4):
    pred_total,
    pred_count_2,
    pred_count_3plus,
    dag_excl_adds,
    dag_excl_skips,
    nodes_terminal,
    nodes_via_explore_new,
    nodes_via_explore_random,
    // #11 attempt-7 NEW (cf. design 2026-05-21):
    all_cycle_verdicts_emitted,   // node was all-cycle at end of grogros_zero
    all_cycle_persisted,          // verdict was written to _deep_evaluation (unshared)
    enum_gate_blocks,             // verdict suppressed because children_count() < _got_moves
    tt_writeback_blocked,         // Plan A TT write skipped due to path-local draw
    counter_count
};
```

The existing `batch_end` JSON emits all counters; the new ones get appended naturally.

A `"batch_seq"` field is added to `batch_start` and `batch_end` for cross-referencing with the global counter:

```json
{"t":"batch_start","seq":0,"batch_seq":42,"root_pc":1,"got_moves":8,"iter_budget":2000}
```

`seq` = per-session sequence (0-indexed within a session); `batch_seq` = global g_dag_batch_seq value at the time. Allows correlation across sessions.

A new event type `all_cycle_verdict` is emitted (capped by `max_events_per_batch`) when the verdict fires:

```json
{"t":"all_cycle_verdict","node_fen":"<fen>","node_pc":5,"got_moves":7,"children_count":7,"persisted":false,"path_local_eval_value":0,"path_local_eval_avg":0.5}
```

Persisted boolean tells us whether `_deep_evaluation` was updated (true only when `_parent_count <= 1`). Diagnostic for "verdict reached root vs got stuck at shared ancestors".

## 5. OFF byte-identicality

Under `g_tt_node_dag == false`:

- §3 cut block is gated `g_tt_node_dag &&` at the outermost; unreached → `_cycled_batch_seq` writes never happen.
- §3.4 verdict block is gated `g_tt_node_dag && path_local_eval != nullptr &&`; unreached.
- TT writeback `path_local_is_draw` evaluates `g_tt_node_dag && ...` which short-circuits false; effective guard term is `&& !false = && true`; original writeback decision unchanged.
- `_cycled_batch_seq` field is added to ChildLink but never read on OFF path. Memory overhead present (4 bytes/edge) but no runtime effect.
- `g_dag_batch_seq` is incremented unconditionally by GUI; but its value is never read on OFF path.
- New `dag_log` counters bumped only inside `if constexpr (dag_log::enabled)` gates, which are also gated on `g_tt_node_dag` at the call sites.

Behaviour identical to baseline `02dedd4` when DAG OFF.

## 6. Performance

ON-path overhead per `grogros_zero` call:
- One `for (auto const& [m, link] : _children)` loop at tail, O(|children|) — typically 5-20 elements for endgames. Negligible.
- One `children_count() >= _got_moves` comparison.
- One conditional set of `_cycled_batch_seq` at §3 cut sites (already O(1) at an existing branch).
- No new allocations.

Memory overhead: 4 bytes per ChildLink. Scales with active search tree size. Typical chess engines have ~10⁵ - 10⁶ active nodes/edges at any time → ~MB scale total. Acceptable on modern hardware.

Hot path (selection loop, get_best_score_move, get_node_score): **UNCHANGED**. No new branches, no new reads.

## 7. Case matrix — coverage verification

| # | Case | Behaviour |
|---|---|---|
| 1 | Repro 1 — every child cycles | strict enum gate passes once enumerated; all-cycle = true; verdict emitted; draw bubbles via out-param; persists at root → root eval = 0 ✓ |
| 2 | Repro 2 — winning move sometimes cycles | At parent of Ke2 etc: Ke2's flag set, but Kc3/Kd3/Kf3 etc. NOT cycle-touch this batch; all-cycle = false; no verdict; normal minimax over stored evals → Ke2 picked → winning ✓ |
| 3 | Partial enumeration (3 of 10 moves explored, all cycle) | `children_count() < _got_moves`; enum gate blocks; no verdict; standard MCTS continues exploring → eventually full enum reached, then verdict ✓ |
| 4 | 8 of 10 moves cycle, 2 don't (after full enum) | all-cycle = false (the 2 non-cycle break the loop); no verdict; normal minimax → 2 non-cycle moves' values dominate ✓ |
| 5 | Cycle leaf at depth N (terminal mate or eval leaf as one descent) | The descent that reached the terminal contributes its real value via standard MCTS backup; the terminal child's `_cycled_batch_seq` is NOT set (because the descent didn't §3-cut). At the parent: not all-cycle. Verdict suppressed; normal backup. ✓ |
| 6 | Shared cycle node `child_pc = 5`, 3 parents cycle 2 don't | Each parent's view of THIS edge is independent: parent A sets `_children[M_to_N]._cycled_batch_seq = g_dag_batch_seq`, parent B does NOT. Parent A may emit verdict (if its OTHER children also cycle), parent B does not. **Per-parent decision, no shared mutation, no cross-talk.** ✓ |
| 7 | Plan A TT writeback after path-local draw persisted | `path_local_is_draw` check fires → write skipped; counter `tt_writeback_blocked` bumped. No flat-TT leakage of path-local value. ✓ |
| 8 | False-positive: a child whose `_cycled_batch_seq` was set on a PREVIOUS batch but not this one | Lazy reset via batch_seq comparison: stale value is ignored. No false positive. ✓ |
| 9 | Root-level all-cycle | Root has `_parent_count <= 1` (it's the root) → persist; root's `_deep_evaluation = dag_draw_eval`; GUI shows draw. ✓ |
| 10 | The root's `grogros_zero` is called with `path_local_eval == nullptr` (GUI default) | §3.4 verdict block is gated `path_local_eval != nullptr`; the root's verdict is NOT emitted via out-param (no caller to receive it) BUT IS persisted to `_deep_evaluation` if all-cycle (because the root is unshared, `_parent_count <= 1`). **Resolution**: extend the §3.4 block to handle this — when `path_local_eval == nullptr` AND all-cycle AND `_parent_count <= 1`, still persist. The verdict is informational locally; no caller to bubble to. |

Case 10 reveals a subtle issue with my §3.4 sketch. Refined:

```cpp
if (g_tt_node_dag) {
    if (children_count() >= static_cast<size_t>(_board->_got_moves)) {
        bool all_cycle = true;
        for (auto const& [m, link] : _children) {
            if (link._cycled_batch_seq != g_dag_batch_seq) {
                all_cycle = false;
                break;
            }
        }
        if (all_cycle) {
            const Evaluation draw = dag_draw_eval();
            if (path_local_eval != nullptr) {
                *path_local_eval = draw;
            }
            if (_parent_count <= 1 && !(draw == _deep_evaluation)) {
                _deep_evaluation = draw;
                all_cycle_persisted_this_iter = true;
            }
            if constexpr (dag_log::enabled) {
                dag_log::bump(dag_log::Counter::all_cycle_verdicts_emitted);
                if (all_cycle_persisted_this_iter) {
                    dag_log::bump(dag_log::Counter::all_cycle_persisted);
                }
            }
        }
    } else if constexpr (dag_log::enabled) {
        dag_log::bump(dag_log::Counter::enum_gate_blocks);
    }
}
```

The verdict block no longer gates on `path_local_eval != nullptr` — it runs whenever DAG is ON. The out-param write is conditional (only if non-null), but the persistence to unshared `_deep_evaluation` runs regardless. This handles Case 10 (root with nullptr out-param) correctly.

## 8. Test/validation plan

The user runs keys `1` (Repro 1 ON+OFF) and `2` (Repro 2 ON+OFF) after each implementation increment. The log (`dag_metrics.log`) provides the evidence:

**Repro 1 acceptance**: DAG ON `final_root_eval` should drop from baseline 476 toward 0 within ≤10 batches. New counter `all_cycle_verdicts_emitted` should be `> 0`. `all_cycle_persisted` `> 0` (at root). Comparison: DAG OFF still converges in batch 0.

**Repro 2 acceptance**: DAG ON `final_root_eval`/`avg_score` trajectory should be at least as good as baseline (the variable observed behaviour: sometimes finds win at avg_score ≥ 0.7). The `all_cycle_verdicts_emitted` should be very low or zero on Repro 2 (winning positions aren't all-cycle). If `all_cycle_persisted > 0` AND root eval dropped, that's a regression — the verdict is firing inappropriately.

**OFF preservation**: DAG OFF behaviour must be byte-identical to baseline — Repro 1 OFF instant draw, Repro 2 OFF slow climb.

## 9. Out of scope

- Optimisation of `get_node_score` to use the per-edge cycle flag for selection bias. Soft-penalty in selection is Approach 2 — orthogonal, not needed if all-cycle gate works.
- Compaction / removal of opt-3 anti-spin (DagExcl + pick_random_child skip). Data shows it's dead code (`dag_excl_skips = 0` across 80k+ iters), but removing it is a separate cleanup commit.
- Recursive bubbling beyond standard MCTS backup. Standard backup handles parent-of-cycle-parent eval propagation; no special chain needed.
- `Evaluation::operator==` lives in `board.h`. Re-added inline header-only, identical to Approach A's prior implementation (reverted at `a8b4004`).

## 10. Self-review

- **Placeholders**: none.
- **Internal consistency**: §3.1 (field), §3.2 (counter), §3.3 (cut), §3.4 (verdict), §3.5 (TT guard), §3.6 (reset), §5 (OFF), §6 (perf), §7 (cases) all reference the same `_cycled_batch_seq` semantics and the same all-cycle predicate.
- **Ambiguity**: Case 10 was a real ambiguity in the initial §3.4 sketch — resolved inline.
- **Scope**: focused single-purpose fix; metrics extensions are observability-only.
- **Performance**: hot path unchanged; ON overhead bounded by one O(|children|) loop per grogros_zero call.
