# DAG metrics logging — Design Spec

**Date:** 2026-05-20
**Branch:** `feature/tt-main-search`
**Baseline:** `a8b4004` (post-revert of Approach A; functional state of `6e8c030`)
**Issue:** #11 — DAG GHI soundness (preliminary instrumentation step before the 7th design attempt)

**Reference positions:**

- **Position 1 (fix target)**: `6k1/8/7P/7K/8/8/8/8 w - - 3 72` — K+h-pawn vs K, theoretically drawn (defender corners at h8, attacker cannot promote without stalemate). Engine currently *false-wins* under DAG ON. Target after eventual fix: instant draw.
- **Position 2 (non-regression target)**: `8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7` — pawn endgame, white wins by Ke3 or Ke2. Currently *works* under DAG ON: winning plan found in ~2 seconds (vs ~1 minute without DAG — the transposition speedup is the entire point of the DAG). Target after eventual fix: **still wins, still in ~2 seconds**. Any fix that slows Position 2 by more than a small factor or flips its verdict is a regression.

---

## 1. Goal

Add **persistent runtime logging** to characterize the DAG search behavior on issue #11 reference positions, so the next design attempt is grounded in actual runtime evidence rather than speculation about predicates. Six prior design attempts on #11 each failed for a different speculative reason; the logging shifts the discipline from "design then test" to **"measure first, then design"**.

Specific runtime questions the logs must answer:

1. **Predicate-fire characteristics.** When `position_is_draw_by_repetition(branch_history, child)` returns true, what is (a) the count of the child's key already present in `path_history` (would-be count after push), (b) the depth from root at the moment of fire, and (c) the DAG sharing status of the child (`_parent_count`)?
2. **Cycle structure per repro.** For each reference repro, how does the predicate-fire frequency, distribution by depth, distribution by would-be-count, and shared-vs-unique-child split look over multiple batches?
3. **DagExcl saturation.** How often does `pick_random_child` skip a move due to DagExcl membership? At what depths?
4. **Per-batch convergence.** How does root `_deep_evaluation` evolve across batches, especially in the two reference repros?

**Important codebase observation (verified Task 1 of the impl plan)**: `PositionHistory` is `tsl::robin_map<uint64_t, uint8_t>` — a Zobrist-key→count map, not an ordered list. At the root call from the GUI (gui.cpp:1058), `grogros_zero` receives `path_history == nullptr` and creates a fresh `local_path_history` containing only the root's key. Game-played positions tracked separately on `Board::_positions_history` are NOT threaded into the search. Therefore every §3 predicate fire during search is a **search-traversal cycle** (the same key was pushed earlier on this descent via `PathScope`). The originally-proposed "game-history vs search-traversal split" does not apply to this codebase and has been replaced by the `count_at_fire` / `path_size` decomposition below.

Performance remains project priority #1: the logging is compile-time gated so OFF builds are byte-identical to baseline, and ON builds do file I/O only at batch boundaries (never inside the hot loop).

## 2. Scope

- **New TU pair**: `opti_chess/dag_log.h` + `opti_chess/dag_log.cpp`. Self-contained, single responsibility (file-buffered structured logging of DAG-relevant events).
- **Instrumentation calls** at 5 sites in `opti_chess/exploration.cpp` and around the root grogros_zero call in `opti_chess/gui.cpp`.
- **`.gitignore` entry** for the log artifact file.
- **Convenience repro entry point** in `opti_chess/tests.cpp` + key bindings (`1`/`2`) in `gui.cpp` so the user invokes a known FEN + batch budget with one keystroke.
- **No changes to search algorithm logic.** This commit chain is observability-only. No changes to `PositionHistory`, `position_is_draw_by_repetition`, or any data-structure-defining file.

Out of scope (deferred to a future design attempt):

- Any modification of `position_is_draw_by_repetition`, `compute_path_local_eval`, or any other search predicate.
- Loading `Board::_positions_history` (game-played positions) into the search's `path_history`. This is a real semantic gap (the engine ignores game-history during search) but fixing it is a search-logic change, out of scope for observability.
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
{"t":"batch_end","seq":0,"iters_done":1000,"root_eval":850,"root_eval_avg_score":0.62,"counters":{"pred_total":47,"pred_count_2":42,"pred_count_3plus":5,"dag_excl_adds":47,"dag_excl_skips":89,"nodes_terminal":12,"nodes_via_explore_new":214,"nodes_via_explore_random":786,"events_dropped":0}}
```

`events_dropped` counts how many detail events were suppressed by the per-batch cap (§5.3). Non-zero means we lost detail — important for honest analysis.

### 5.3 Per-event detail (capped at `max_events_per_batch`)

```json
{"t":"pred_fire","depth":3,"count_at_fire":2,"path_size":15,"child_pc":2,"child_eval":120,"child_avg":0.55,"node_fen":"<fen>","child_fen":"<fen>","child_move":"Kb7"}
{"t":"pred_fire","depth":7,"count_at_fire":3,"path_size":15,"child_pc":1,"child_eval":850,"child_avg":0.71,"node_fen":"<fen>","child_fen":"<fen>","child_move":"Kg6"}
{"t":"dag_excl_skip","depth":4,"node_pc":3,"move":"Kc6","reason":"in_excl"}
```

After `max_events_per_batch` events emitted in a batch, further events bump `events_dropped` only. This prevents log explosion (one prior repro produced ~8852 predicate fires per batch per memory).

**`count_at_fire`** = `position_history_count(path_history, child) + 1` — the count the child's key WOULD reach if pushed. With `search_repetition_limit == 2`, the predicate fires when `count_at_fire >= 2`. A value of 2 means "this is the first cycle on this path"; a value of 3+ means "this position has been revisited multiple times in one descent" (deeper cycle).

**`path_size`** = `path_history.size()` at moment of fire. With root-only init, this is also the search depth.

**`child_pc`** = `child->_parent_count` — DAG sharing status of the cycle target. `child_pc > 1` means the cycle is via a shared DAG node (transposition-driven).

The combination of `count_at_fire`, `path_size`, and `child_pc` is the **critical decomposition** that all six prior design attempts lacked direct visibility into.

### 5.4 Counter set

Maintained as a per-batch struct, reset at `batch_start`, emitted in `batch_end`:

```cpp
struct BatchCounters {
    int pred_total = 0;
    int pred_count_2 = 0;         // fires with count_at_fire == 2 (first cycle)
    int pred_count_3plus = 0;     // fires with count_at_fire >= 3 (deep cycle)
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
void pred_fire(int depth, int count_at_fire, int path_size,
               const Node* parent, const Node* child, const Move& m);
void dag_excl_skip(int depth, const Node* node, const Move& m);

// Counter-only increments (always run when enabled, no event cap).
enum class Counter {
    pred_total, pred_count_2, pred_count_3plus,
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
   - Call `position_history_count(*branch_history, *child->_board)` to read the current count of the child's key (cheap; the predicate already did this lookup, this is just a re-read).
   - `count_at_fire = current_count + 1` (the would-be count after a hypothetical push).
   - Call `dag_log::pred_fire(depth, count_at_fire, path_size, this, child, move)` and `dag_log::bump(Counter::pred_total)` + `bump(Counter::pred_count_2)` if `count_at_fire == 2` else `bump(Counter::pred_count_3plus)`.

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

## 8. Determining `count_at_fire` and `path_size`

`PositionHistory` is `tsl::robin_map<uint64_t, uint8_t>` — a key→count map. The existing helper `position_history_count(const PositionHistory&, Board&)` (`exploration.cpp:73`) returns the current count for a given board's Zobrist key, or 0 if absent.

At the §3 cut site (where the predicate just returned true), the implementation re-reads the count:

```cpp
const int current_count = (int)position_history_count(*branch_history, *child->_board);
const int count_at_fire = current_count + 1;          // would-be count after push
const int path_size = (int)branch_history->size();
```

Both lookups are cheap (one robin_map find each) and only run when `dag_log::enabled == true`. No structural changes to `PositionHistory` are required.

**No modification of `PositionHistory` or `position_is_draw_by_repetition`**. The originally-proposed `_game_history_size` field is dropped (the game-history vs search-traversal split is not meaningful in this codebase since game-history is on `Board::_positions_history`, not threaded into the search). Decomposing pred fires by `count_at_fire` and `path_size` provides better analysis material with zero data-structure cost.

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

void run_dag_repro_2() {
    // Pawn endgame, white wins (Ke3 or Ke2 is the key move). Used as a
    // NON-REGRESSION anchor — DAG ON currently finds the win in ~2s
    // (vs ~1min without DAG). Any future fix MUST preserve this.
    run_dag_repro("repro2_pawn_endgame_win",
                  "8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7",
                  5, 1000);
}
```

The user invokes `run_dag_repro_1()` (or `_2`) from VS — either by adding a menu hook, by editing `main` temporarily, or by calling it from an existing debug entry. The exact invocation path is left to the user's preference; the spec only commits to the function being present and callable.

## 10. OFF byte-identicality

- `if constexpr (dag_log::enabled == false)` elides every call to `dag_log::*` at compile time.
- New TU `dag_log.cpp` adds symbols, but none are referenced from the OFF code path.
- No structural change to `PositionHistory` — `count_at_fire`/`path_size` are read via the existing `position_history_count` helper, only inside the logging gate.
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
3. ON behavior (toggle true): running `run_dag_repro_1()` AND `run_dag_repro_2()` for 5 batches each produces a `opti_chess/dag_metrics.log` file containing, for each run:
   - 1 `session_start` line
   - 5 `batch_start` + 5 `batch_end` lines
   - At least some `pred_fire` events with both `"kind":"search_traversal"` and `"kind":"game_history"` populated (or all of one kind, if the position genuinely has only one)
   - 1 `session_end` line
4. The log file parses as one valid JSON object per line.
5. No new search-behavior regressions: PERFT 1/2 + EVALUATION stable with logging ON.
6. The assistant can read the log file (committed via `git add -f` if the user wants to share a session, or accessed via Read tool on the user's workspace).

## 15. Implementation plan (separate doc)

Once this spec is approved, the implementation plan will be written to `docs/superpowers/plans/2026-05-20-optichess-dag-metrics-logging.md` via the `writing-plans` skill and executed via `subagent-driven-development`. Approximate task breakdown:

- Task 1: Design-validation gate (no code). Verify `PositionHistory` structure and the §3 cut location post-revert. **STATUS 2026-05-20: completed inline; the spec has been updated with the verified codebase structure.**
- Task 2: New TU `dag_log.{h,cpp}` — API + file output + counters + JSON serialization. `.gitignore` entry.
- Task 3: Instrumentation points in `exploration.cpp` (5 sites: §3 cut, pick_random_child DagExcl skip, explore_new_move entry, explore_random_child entry, grogros_zero terminal returns).
- Task 4: `gui.cpp` hooks around root grogros_zero + `run_dag_repro(...)` in `tests.cpp` + `repro_1`/`repro_2` wrappers + key bindings 1/2.
- Task 5: Build verification with both `enabled=true` and `enabled=false` + push.

(The originally-listed `PositionHistory::_game_history_size` task is dropped — the codebase observation made it unnecessary. Final task count: 5, not 8.)

## 16. Self-review

- **Placeholders**: none. Both repro FENs are populated in §9.
- **Internal consistency**: §3 toggle, §5 schema, §6 API, §7 instrumentation, §10 OFF-byte-identicality all reference the same `dag_log::enabled` gate consistently.
- **Scope**: bounded to observability. No search-logic edits. `PositionHistory` field is informational; no logic depends on it.
- **Ambiguity**: `kind` decomposition rule (§5.3) is precise. `events_dropped` semantics (§5.2) is precise. Implementation choice between `ostringstream` and `snprintf` is explicit in §11.1 with a recommendation.

No remaining placeholders requiring decisions. Ready for user review.
