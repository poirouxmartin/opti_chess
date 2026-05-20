# DAG metrics logging — Design Spec

**Date:** 2026-05-20
**Branch:** `feature/tt-main-search`
**Baseline:** `a8b4004` (post-revert of Approach A; functional state of `6e8c030`)
**Issue:** #11 — DAG GHI soundness (preliminary instrumentation step before the 7th design attempt)

---

## 1. Goal

Add **persistent runtime logging** to characterize the DAG search behavior on issue #11 reference positions, so the next design attempt is grounded in actual runtime evidence rather than speculation about predicates. Six prior design attempts on #11 each failed for a different speculative reason; the logging shifts the discipline from "design then test" to **"measure first, then design"**.

Specific runtime questions the logs must answer:

1. **Predicate-fire decomposition.** When `position_is_draw_by_repetition(branch_history, child)` returns true, what fraction of fires are caused by (a) a match against a *game-history* entry (a position pre-loaded from the played game) vs (b) a *search-traversal* entry (a position pushed by `PathScope` during descent on the current path)?
2. **Cycle structure per repro.** For each reference repro, how does the predicate-fire frequency, distribution by depth, and game-history-vs-search-traversal split look over multiple batches?
3. **DagExcl saturation.** How often does `pick_random_child` skip a move due to DagExcl membership? At what depths?
4. **Per-batch convergence.** How does root `_deep_evaluation` evolve across batches, especially in the two reference repros?

Performance remains project priority #1: the logging is compile-time gated so OFF builds are byte-identical to baseline, and ON builds do file I/O only at batch boundaries (never inside the hot loop).

## 2. Scope

- **New TU pair**: `opti_chess/dag_log.h` + `opti_chess/dag_log.cpp`. Self-contained, single responsibility (file-buffered structured logging of DAG-relevant events).
- **Instrumentation calls** at 5-7 sites in `opti_chess/exploration.cpp` and 1 in `opti_chess/gui.cpp`.
- **`.gitignore` entry** for the log artifact file.
- **Convenience repro entry point** in `opti_chess/tests.cpp` so the user can invoke a known FEN + batch budget from a single function call.
- **No changes to search algorithm logic.** This commit chain is observability-only.

Out of scope (deferred to a future design attempt):

- Any modification of `position_is_draw_by_repetition`, `compute_path_local_eval`, or any other search predicate.
- Programmatic regression-gate / CI integration. Repros are user-run; the assistant reads the resulting log files.
- Log rotation / compression. Single file, user-truncated when needed.

## 3. Compile-time toggle

```cpp
// dag_log.h
namespace dag_log {
    constexpr bool enabled = true;
    constexpr int  max_events_per_batch = 200;
}
```

Every API call below is wrapped in `if constexpr (dag_log::enabled)` at the call site (via a macro or via inlined function with constexpr-guarded body). When `enabled == false`, the compiler eliminates all calls — OFF byte-identical to baseline.

This is parallel to the existing `dag_debug` constexpr at `opti_chess/exploration.cpp` (see comments around `dag_dbg_take()` in recent commits) and follows the same idiom.

## 4. File output

- **Path**: `opti_chess/dag_metrics.log` (workspace-relative, beside the .cpp/.h sources for discoverability).
- **Mode**: append. Opened lazily on first event via a static `std::ofstream`. Closed at process exit via static destructor (RAII).
- **Format**: JSON-lines — one event per line, each line a self-contained JSON object. Trivially grep-able, trivially machine-parseable, robust to truncation.
- **Gitignored** by default (it's a runtime artifact, not source). User can `git add -f opti_chess/dag_metrics.log` to share a specific session with the assistant for analysis.

## 5. Event schema

### 5.1 Session events

```json
{"t":"session_start","date":"2026-05-20T15:34:21Z","fen":"<root_fen>","dag":true,"plan_a":false,"iter_budget":1000,"repro_name":"repro1_kp_h_draw"}
{"t":"session_end","batches":5,"final_root_eval":850,"final_root_pc":1}
```

`repro_name` is optional, set only when invoked via the convenience repro entry point (§9).

### 5.2 Batch events

```json
{"t":"batch_start","seq":0,"root_pc":1,"got_moves":5,"iter_budget":1000}
{"t":"batch_end","seq":0,"iters_done":1000,"root_eval":850,"root_eval_avg_score":0.62,"counters":{"pred_total":47,"pred_search_traversal":31,"pred_game_history":16,"dag_excl_adds":31,"dag_excl_skips":89,"nodes_terminal":12,"nodes_via_explore_new":214,"nodes_via_explore_random":786,"events_dropped":0}}
```

`events_dropped` counts how many detail events were suppressed by the per-batch cap (§5.3). Non-zero means we lost detail — important for honest analysis.

### 5.3 Per-event detail (capped at `max_events_per_batch`)

```json
{"t":"pred_fire","kind":"search_traversal","depth":3,"hist_idx":11,"hist_size":15,"game_hist_size":3,"child_pc":2,"child_eval":120,"child_avg":0.55,"node_fen":"<fen>","child_fen":"<fen>","child_move":"Kb7"}
{"t":"pred_fire","kind":"game_history","depth":7,"hist_idx":2,"hist_size":15,"game_hist_size":3,"child_pc":1,"child_eval":850,"child_avg":0.71,"node_fen":"<fen>","child_fen":"<fen>","child_move":"Kg6"}
{"t":"dag_excl_skip","depth":4,"node_pc":3,"move":"Kc6","reason":"in_excl"}
```

After `max_events_per_batch` events emitted in a batch, further events bump `events_dropped` only. This prevents log explosion (one prior repro produced ~8852 predicate fires per batch per memory).

`kind` is computed at the §3 cut site by comparing `hist_idx` to `game_hist_size`:

- `hist_idx < game_hist_size` → `"game_history"`
- `hist_idx >= game_hist_size` → `"search_traversal"`

This is the **critical decomposition** that all six prior design attempts lacked direct visibility into.

### 5.4 Counter set

Maintained as a per-batch struct, reset at `batch_start`, emitted in `batch_end`:

```cpp
struct BatchCounters {
    int pred_total = 0;
    int pred_search_traversal = 0;
    int pred_game_history = 0;
    int dag_excl_adds = 0;
    int dag_excl_skips = 0;
    int nodes_terminal = 0;
    int nodes_via_explore_new = 0;
    int nodes_via_explore_random = 0;
    int events_dropped = 0;
};
```

Extensible — adding a new counter is one field + one inline increment + one JSON key in `batch_end`. No event-schema changes.

## 6. API

```cpp
namespace dag_log {

constexpr bool enabled = true;
constexpr int  max_events_per_batch = 200;

void session_start(const char* fen, bool dag_on, bool plan_a_on,
                   int iter_budget, const char* repro_name = nullptr);
void session_end(int batches, int final_root_eval, int final_root_pc);

void batch_start(int seq, int root_pc, int got_moves, int iter_budget);
void batch_end(int seq, int iters_done, int root_eval, float root_avg_score);

// Detail events (no-op past max_events_per_batch).
void pred_fire(bool is_game_history, int depth, int hist_idx, int hist_size,
               int game_hist_size, const Node* parent, const Node* child,
               const Move& m);
void dag_excl_skip(int depth, const Node* node, const Move& m);

// Counter-only increments (always run when enabled, no event cap).
enum class Counter {
    pred_total, pred_search_traversal, pred_game_history,
    dag_excl_adds, dag_excl_skips,
    nodes_terminal, nodes_via_explore_new, nodes_via_explore_random
};
void bump(Counter c);

}  // namespace dag_log
```

Each function body in `dag_log.cpp` begins with `if constexpr (!enabled) return;` to short-circuit when disabled. The call site can also use `if constexpr (dag_log::enabled)` to elide the call entirely; both forms achieve identical optimization in MSVC release mode (and the per-function gate is the safety net for call sites that don't bother).

## 7. Instrumentation points

In `opti_chess/exploration.cpp`:

1. **§3 cut block** (inside `Node::explore_random_child`, the `if (g_tt_node_dag && position_is_draw_by_repetition(branch_history, *child->_board))` block, ~line 785):
   - Determine `hist_idx` of the matching entry by re-walking `branch_history` (cheap; only when logging enabled).
   - Determine `kind` via `hist_idx < game_hist_size`.
   - Call `dag_log::pred_fire(...)` and `dag_log::bump(Counter::pred_total)` + `bump(Counter::pred_search_traversal | pred_game_history)`.

2. **`pick_random_child` DagExcl skip** (inside the loop that filters by DagExcl membership, ~line 1746 area):
   - Call `dag_log::dag_excl_skip(...)` and `dag_log::bump(Counter::dag_excl_skips)`.

3. **§3 cut `dag_excl->add(move)`** (same block as #1):
   - `dag_log::bump(Counter::dag_excl_adds)`.

4. **`explore_new_move` entry** (~line 479):
   - `dag_log::bump(Counter::nodes_via_explore_new)`.

5. **`explore_random_child` entry, post §3 cut** (~line 802 area, after the cut block):
   - `dag_log::bump(Counter::nodes_via_explore_random)`.

6. **`grogros_zero` terminal early-return** (the `_is_terminal` and `_got_moves <= 0` branches):
   - `dag_log::bump(Counter::nodes_terminal)`.

7. **`grogros_zero` ROOT call entry/exit** (the top-level call from GUI, distinguished by the fact that `path_history == nullptr` or by a depth-zero marker):
   - This is harder to detect from inside `grogros_zero` because the same function is also called recursively. Resolution: the root call comes from GUI (`gui.cpp`) or from `tests.cpp` (the repro entry point); instrumentation lives at the **caller** of the root `grogros_zero`, not inside `grogros_zero`.

In `opti_chess/gui.cpp`:

8. **Root `grogros_zero` call site** (the GUI batch entry — the user pressing the batch key):
   - Before the call: `dag_log::session_start(...)` if a new position; `dag_log::batch_start(...)` always.
   - After the call: `dag_log::batch_end(...)`.
   - On position change or process exit: `dag_log::session_end(...)`.

In `opti_chess/tests.cpp` (new):

9. **Repro convenience entry point** (§9 below).

## 8. Determining the `game_history_size` split

The `PositionHistory` structure threads through search; current implementation does NOT expose the split between game-history-prefix and search-traversal-appended entries.

**Resolution**: add a `size_t _game_history_size` field to `PositionHistory`. Set when the history is initialized from a FEN (in the FEN loader / GUI position setter), never changed by `PathScope` push/pop. Exposed via `size_t game_history_size() const`.

This is the only **non-instrumentation** modification in this design — but it's still observability-only because:
- The field is *informational*. No search logic reads it.
- It defaults to zero (treat all entries as search-traversal if uninitialized). Backward compatible.
- The §3 cut reads it only when `dag_log::enabled == true` to label the event `kind`.

OFF byte-identicality preserved because the field is never read on the OFF path.

## 9. Repro convenience entry point

Add to `opti_chess/tests.cpp`:

```cpp
// docs/superpowers/specs/2026-05-20-optichess-dag-metrics-logging-design.md §9
// Run a known repro FEN for N batches of K iterations with DAG ON, emitting
// structured metrics to opti_chess/dag_metrics.log. User invokes from VS
// (via a tests menu / debug entry) without manual GUI setup.
void run_dag_repro(const char* repro_name, const char* fen,
                   int n_batches, int iters_per_batch);
```

Plus two pre-defined wrappers:

```cpp
void run_dag_repro_1() {
    // Theoretical-draw K+h-pawn-vs-K. Expected: convergent draw.
    // Currently engine false-wins under DAG ON (issue #11 Repro 1).
    run_dag_repro("repro1_kp_h_draw",
                  "6k1/8/7P/7K/8/8/8/8 w - - 3 72",
                  5, 1000);
}

void run_dag_repro_2_placeholder() {
    // Awaiting confirmed FEN for the won-blocked-pawns position
    // (lost from TODO_list.txt 2026-05-20). User will provide.
    // Once known, replace the FEN literal below.
    run_dag_repro("repro2_won_blocked_pawns",
                  "<FEN_TBD>",
                  5, 1000);
}
```

The user invokes `run_dag_repro_1()` (or `_2`) from VS — either by adding a menu hook, by editing `main` temporarily, or by calling it from an existing debug entry. The exact invocation path is left to the user's preference; the spec only commits to the function being present and callable.

## 10. OFF byte-identicality

- `if constexpr (dag_log::enabled == false)` elides every call to `dag_log::*` at compile time.
- New TU `dag_log.cpp` adds symbols, but none are referenced from the OFF code path.
- `PositionHistory::_game_history_size` is initialized to 0 and unused on the OFF path; the field's presence has zero observable runtime effect.
- `.gitignore` entry adds no runtime effect.

Conclusion: identical to baseline `a8b4004` when `dag_log::enabled == false`.

## 11. Performance characteristics

When `enabled == true`:

- **Per pred_fire / dag_excl_skip event**: one `std::ostringstream` append + counter bump. Cost is the JSON serialization (small fixed string + a few int → string conversions). Amortized cheap; `std::ostringstream` does heap allocation though — a real concern for the hot path. Mitigation: write to a static `std::string` (reused across events, `clear()` between batches) instead of `std::ostringstream`. Detail in implementation: §11.1.
- **File I/O**: only at batch boundaries (`batch_end` flush). The `std::ofstream` writes the accumulated batch buffer in one `<<`-and-`flush()`.
- **Cap behavior**: after `max_events_per_batch`, the detail-event functions short-circuit on the first line. Counter increments still run (cheap).

### 11.1 Heap-allocation discipline

The hot-path concern is `std::ostringstream` (allocates a `std::stringbuf`). To stay zero-alloc per event:

- Reserve a `static thread_local std::string buffer_;` in `dag_log.cpp` with `reserve(64 * 1024)` once at first event.
- Append JSON manually via `buffer_ += "..."` + `std::to_string(...)` (which itself allocates, unfortunately). Or use `fmt::format_to_n` if available.
- Alternative cheap path: `char tmp[256]; std::snprintf(tmp, ...); buffer_ += tmp;` — zero allocation per event.

The implementation will favor `snprintf` into stack buffers + string append to a pre-reserved per-batch buffer. Final flush is one `ofstream::write(buffer_.data(), buffer_.size())`.

When `enabled == false`: zero of any of this exists at runtime.

## 12. Edge cases

1. **Multiple sessions in one process run** (user loads several positions): `session_end` is called when a new `session_start` arrives with a different FEN. Final session ends at process exit (static destructor flushes and closes).
2. **Process crash mid-batch**: the per-batch buffer is lost. Already-flushed batches remain on disk. Acceptable — partial log is still useful.
3. **Very long batches** (`iter_budget > 100000`): events_dropped grows; counters still valid. The capped detail-event count is enough to characterize the distribution.
4. **DAG OFF runs**: all DAG-specific counters stay at 0 (the §3 cut block is OFF-gated). Session/batch events still fire — useful for baseline OFF comparisons.
5. **`PositionHistory` with no game-history prefix** (e.g., starting from `position startpos`): `_game_history_size == 0` → every pred_fire labeled `search_traversal`. Correct.

## 13. Out of scope / deferred

- Auto-rotation of `dag_metrics.log` (let it grow; user can `> opti_chess/dag_metrics.log` to truncate).
- Live tail / dashboard (the assistant reads the file once you commit/share it).
- Histograms / aggregations in-engine (raw events; aggregation is the analysis step, done offline by reading the JSON-lines).
- Programmatic regression-gate / CI (the user runs the repro entry point in VS; the resulting log file is the evidence).
- The actual fix for #11 — this spec is observability infrastructure only.

## 14. Acceptance criteria

1. Build succeeds (`EXITCODE=0`, no `error` lines) on both `dag_log::enabled = true` and `dag_log::enabled = false`.
2. OFF behavior (toggle false) byte-identical to baseline `a8b4004` on PERFT 1/2 + EVALUATION (the established #11 acceptance pattern).
3. ON behavior (toggle true): running `run_dag_repro_1()` for 5 batches produces a `opti_chess/dag_metrics.log` file containing:
   - 1 `session_start` line
   - 5 `batch_start` + 5 `batch_end` lines
   - At least some `pred_fire` events with both `"kind":"search_traversal"` and `"kind":"game_history"` populated (or all of one kind, if the position genuinely has only one)
   - 1 `session_end` line
4. The log file parses as one valid JSON object per line.
5. No new search-behavior regressions: PERFT 1/2 + EVALUATION stable with logging ON.
6. The assistant can read the log file (committed via `git add -f` if the user wants to share a session, or accessed via Read tool on the user's workspace).

## 15. Implementation plan (separate doc)

Once this spec is approved, the implementation plan will be written to `docs/superpowers/plans/2026-05-20-optichess-dag-metrics-logging.md` via the `writing-plans` skill and executed via `subagent-driven-development`. Approximate task breakdown:

- Task 1: Design-validation gate (no code). Confirm `PositionHistory`'s current structure and the §3 cut location post-revert.
- Task 2: Add `PositionHistory::_game_history_size` field + accessor + initialization in FEN loader (additive, defaulted 0).
- Task 3: New TU `dag_log.{h,cpp}` — API + file output + counters + JSON serialization.
- Task 4: Instrumentation points in `exploration.cpp` + `gui.cpp`.
- Task 5: `run_dag_repro(...)` entry point in `tests.cpp` + `repro1` wrapper.
- Task 6: `.gitignore` entry.
- Task 7: Build + smoke test (run `run_dag_repro_1()` if user can; otherwise build-only).
- Task 8: Commit and push.

## 16. Self-review

- **Placeholders**: `<FEN_TBD>` in §9 for Repro 2 — explicit and intentional. The repro_1 entry point is fully populated.
- **Internal consistency**: §3 toggle, §5 schema, §6 API, §7 instrumentation, §10 OFF-byte-identicality all reference the same `dag_log::enabled` gate consistently.
- **Scope**: bounded to observability. No search-logic edits. `PositionHistory` field is informational; no logic depends on it.
- **Ambiguity**: `kind` decomposition rule (§5.3) is precise. `events_dropped` semantics (§5.2) is precise. Implementation choice between `ostringstream` and `snprintf` is explicit in §11.1 with a recommendation.

No remaining placeholders requiring decisions. Ready for user review.
