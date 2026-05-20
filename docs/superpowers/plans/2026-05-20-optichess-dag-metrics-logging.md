# DAG metrics logging — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add persistent file-based JSON-lines metrics logging to characterize DAG search behavior on issue #11 reference positions, so the next design attempt is grounded in measured runtime evidence rather than speculation. Observability-only — no search-logic changes.

**Architecture:** New self-contained TU pair `opti_chess/dag_log.{h,cpp}` exposes a small API (session/batch lifecycle + per-event detail + counter increments). All API calls are short-circuited to no-ops by a `constexpr bool dag_log::enabled` gate so OFF builds are byte-identical to baseline. Instrumentation calls are placed at 5 sites in `exploration.cpp`, the root `grogros_zero` call in `gui.cpp`, and a new repro entry point in `tests.cpp`. **Codebase reality verified Task 1**: `PositionHistory` is `tsl::robin_map<uint64_t, uint8_t>` (a count map) and the search's `path_history` carries only search-traversal positions (not game-history). Per-event detail reports `count_at_fire` (would-be count after a hypothetical push) and `path_size` (current depth), computed via the existing `position_history_count` helper. No structural changes to `PositionHistory`.

**Tech Stack:** C++17, MSVC (`opti_chess.sln`), single-threaded engine. No automated test framework — "test" per task = clean MSBuild + a written OFF-byte-identical argument. Final acceptance is **USER-run** (assistant cannot run the raylib GUI): the user invokes the two repro entry points via keys `1`/`2` and shares the resulting log file.

Design spec: `docs/superpowers/specs/2026-05-20-optichess-dag-metrics-logging-design.md`.

---

## Build command

Variant B (Insiders Debug x64), confirmed on this machine in the prior #11 session. Run from the project working dir in PowerShell:

```
& 'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe' opti_chess.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m /nologo /clp:ErrorsOnly /v:m; "EXITCODE=$LASTEXITCODE"
```

Success = `EXITCODE=0` and no `error` lines. **Do NOT modify any `.vcxproj`/`.sln`/project file.** When a new `.cpp` is created, command-line MSBuild may not auto-discover it. If MSBuild fails to find `dag_log.cpp`, the implementer must NOT edit the `.vcxproj` — instead, report BLOCKED and ask the user to add the new files via Solution Explorer → opti_chess → Add → Existing Item, then resume.

Commits: English, ASCII, conventional commit, `(#11)` ref, **no AI attribution**. French comments use real Unicode (é, è, à, ç, ê, î, ô, û, ù, œ). Do not commit `opti_chess/TODO_list.txt`, `opti_chess/Tests.txt`, `opti_chess/Tests.txt+tmp`, or `opti_chess/dag_metrics.log` (the latter is added to `.gitignore` in Task 2).

Baseline: tip `5fe8584` (plan committed). Functional state `a8b4004` = `6e8c030` content.

---

## File structure

All changes touch five files; two are new:

- **Create**: `opti_chess/dag_log.h` — API + constexpr toggle + Counter enum + forward decls.
- **Create**: `opti_chess/dag_log.cpp` — file handle, lazy-open, JSON formatting via `snprintf`, per-batch counter state, flush at batch boundaries.
- **Modify**: `opti_chess/exploration.cpp` — add 5 instrumentation call sites (§3 cut, `pick_random_child` DagExcl skip, `explore_new_move` entry, `explore_random_child` entry, `grogros_zero` terminal returns). No header changes.
- **Modify**: `opti_chess/gui.cpp` — wrap the root `grogros_zero` call with `session_start`/`batch_start`/`batch_end`; add two key bindings (`1` and `2`) invoking `run_dag_repro_1()`/`run_dag_repro_2()`.
- **Modify**: `opti_chess/tests.cpp` — add `run_dag_repro(name, fen, n_batches, iters_per_batch)` plus `run_dag_repro_1()`/`run_dag_repro_2()` wrappers. Possibly `opti_chess/tests.h` if forward decls are needed for gui.cpp.
- **Modify**: `.gitignore` (workspace root) — add `opti_chess/dag_metrics.log`.

Task order: spec already validated against codebase (L-Task 1 completed inline; findings folded into spec). Tasks 2-5 = code; Task 5 = build + push.

---

## Verified facts from L-Task 1 (already completed inline)

- Build: Variant B (`C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe`) confirmed.
- `PositionHistory = tsl::robin_map<uint64_t, uint8_t>` aliased from `RepetitionHistory` (defined `board.h:16`, aliased `exploration.h:10`). No ordered list — pure count map.
- Predicate: `position_is_draw_by_repetition(history, board)` = `position_history_count(history, board) + 1 >= search_repetition_limit` (`exploration.cpp:129`). `position_history_count(...)` returns `path_history.find(key)->second` or 0 (`exploration.cpp:73`).
- Root `grogros_zero` call: `gui.cpp:1058` — `_root_exploration_node->grogros_zero(&monte_board_buffer, _grogros_eval, _alpha, _beta, _gamma, iterations == -1 ? iterations_to_explore : iterations, _quiescence_depth);` — note no `path_history` argument; `grogros_zero` creates `local_path_history` (`exploration.cpp:391-393`).
- §3 cut location: inside `Node::explore_random_child` (~`exploration.cpp:790`).
- `position_history_count` is declared `static` (anonymous namespace at `exploration.cpp:73`). Logging-from-anywhere requires either (a) re-doing the lookup inline at the §3 cut site, or (b) lifting the helper out of the anonymous namespace. The plan uses (a): one-line inline lookup via `path_history.find()->second` at the §3 cut site, avoiding any header change.
- `Board::to_fen()`: TO BE VERIFIED at Task 2 Step 0 below (the implementer must grep). The plan's `dag_log.cpp` code assumes a `std::string Board::to_fen() const` returning the FEN; if the actual API differs (e.g., takes a buffer), adapt the four call sites in `dag_log.cpp`.
- `Board::move_label(Move)`: TO BE VERIFIED at Task 2 Step 0 (grep). Adapt similarly.

---

## Task 2: New TU `dag_log.{h,cpp}` + `.gitignore` (fully inert)

Create the logging API + file-output implementation. No caller exists yet → fully byte-identical to baseline.

**Files:**
- Create: `opti_chess/dag_log.h`
- Create: `opti_chess/dag_log.cpp`
- Modify: `.gitignore` (workspace root)

- [ ] **Step 0: Verify `Board::to_fen()` and `Board::move_label(Move)` signatures.** Grep:

```
grep -n "to_fen\|move_label" opti_chess/board.h
```

Record the exact signatures. If `to_fen()` returns `std::string`, no change needed below. If it requires a buffer or has a different name, adapt the four call sites in Step 2 (the `to_fen()`/`move_label(m)` calls inside `pred_fire` and `dag_excl_skip`).

- [ ] **Step 1: Create `opti_chess/dag_log.h`.** Write exactly:

```cpp
#pragma once

// #11 DAG metrics logging (cf. design 2026-05-20).
// Persistent file-based structured event log for runtime evidence of DAG
// search behavior on the two reference repros. Compile-time gated for zero
// runtime cost when disabled. File output is JSON-lines, one event per line,
// at opti_chess/dag_metrics.log (gitignored).

class Move;       // fwd: opti_chess/board.h
class Node;       // fwd: opti_chess/exploration.h

namespace dag_log {

	// Compile-time toggle. When false, every function below short-circuits
	// to a no-op at entry; call sites also wrap in if constexpr to elide
	// argument evaluation in release builds.
	constexpr bool enabled = true;

	// Cap on per-event detail events emitted per batch. Beyond this cap,
	// counter increments still run but detail events are dropped (counted
	// in `events_dropped`).
	constexpr int max_events_per_batch = 200;

	enum class Counter {
		pred_total,
		pred_count_2,
		pred_count_3plus,
		dag_excl_adds,
		dag_excl_skips,
		nodes_terminal,
		nodes_via_explore_new,
		nodes_via_explore_random,
		counter_count
	};

	void session_start(const char* fen, bool dag_on, bool plan_a_on,
		int iter_budget, const char* repro_name);
	void session_end(int batches, int final_root_eval, int final_root_pc);

	void batch_start(int seq, int root_pc, int got_moves, int iter_budget);
	void batch_end(int seq, int iters_done, int root_eval, float root_avg_score);

	// Detail event. Capped at max_events_per_batch per batch.
	// count_at_fire = current count(child key) + 1 (the would-be count after
	// a hypothetical push at the §3 cut site).
	// path_size = path_history.size() at moment of fire.
	void pred_fire(int depth, int count_at_fire, int path_size,
		const Node* parent, const Node* child, const Move& m);

	void dag_excl_skip(int depth, const Node* node, const Move& m);

	void bump(Counter c);

} // namespace dag_log
```

- [ ] **Step 2: Create `opti_chess/dag_log.cpp`.** Write exactly:

```cpp
#include "dag_log.h"
#include "exploration.h"   // Node, _board, _parent_count, _deep_evaluation
#include "board.h"         // Board, Move, to_fen, move_label

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>

namespace dag_log {

namespace {

std::ofstream g_log;
bool g_log_open = false;
std::string g_batch_buffer;
int g_events_this_batch = 0;
int g_events_dropped = 0;
int g_counters[(int)Counter::counter_count] = {};

void open_lazy() {
	if (g_log_open) return;
	g_log.open("opti_chess/dag_metrics.log", std::ios::app);
	g_log_open = g_log.is_open();
	if (g_log_open && g_batch_buffer.capacity() < 64 * 1024) {
		g_batch_buffer.reserve(64 * 1024);
	}
}

void flush_batch_buffer() {
	if (!g_log_open || g_batch_buffer.empty()) return;
	g_log.write(g_batch_buffer.data(), (std::streamsize)g_batch_buffer.size());
	g_log.flush();
	g_batch_buffer.clear();
}

void iso_time_now(char* buf, size_t bufsize) {
	std::time_t now = std::time(nullptr);
	std::tm tm_buf{};
#ifdef _WIN32
	gmtime_s(&tm_buf, &now);
#else
	gmtime_r(&now, &tm_buf);
#endif
	std::strftime(buf, bufsize, "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
}

// Copy a string into out_buf with size limit + JSON-escape `"` and `\`.
// FENs and SAN are otherwise ASCII-safe.
void copy_json_safe(char* out_buf, size_t out_size, const char* in) {
	size_t j = 0;
	for (size_t i = 0; in && in[i] != 0 && j + 2 < out_size; ++i) {
		char c = in[i];
		if (c == '"' || c == '\\') {
			if (j + 3 >= out_size) break;
			out_buf[j++] = '\\';
		}
		out_buf[j++] = c;
	}
	out_buf[j] = 0;
}

} // anon

void session_start(const char* fen, bool dag_on, bool plan_a_on,
	int iter_budget, const char* repro_name) {
	if constexpr (!enabled) return;
	open_lazy();
	if (!g_log_open) return;

	char ts[64];
	iso_time_now(ts, sizeof(ts));
	char fen_safe[128]; copy_json_safe(fen_safe, sizeof(fen_safe), fen ? fen : "");
	char repro_safe[64]; copy_json_safe(repro_safe, sizeof(repro_safe),
		repro_name ? repro_name : "");

	char line[512];
	if (repro_name) {
		std::snprintf(line, sizeof(line),
			"{\"t\":\"session_start\",\"date\":\"%s\",\"fen\":\"%s\",\"dag\":%s,\"plan_a\":%s,\"iter_budget\":%d,\"repro_name\":\"%s\"}\n",
			ts, fen_safe,
			dag_on ? "true" : "false",
			plan_a_on ? "true" : "false",
			iter_budget, repro_safe);
	} else {
		std::snprintf(line, sizeof(line),
			"{\"t\":\"session_start\",\"date\":\"%s\",\"fen\":\"%s\",\"dag\":%s,\"plan_a\":%s,\"iter_budget\":%d,\"repro_name\":null}\n",
			ts, fen_safe,
			dag_on ? "true" : "false",
			plan_a_on ? "true" : "false",
			iter_budget);
	}
	g_batch_buffer += line;
	flush_batch_buffer();
}

void session_end(int batches, int final_root_eval, int final_root_pc) {
	if constexpr (!enabled) return;
	if (!g_log_open) return;

	char line[256];
	std::snprintf(line, sizeof(line),
		"{\"t\":\"session_end\",\"batches\":%d,\"final_root_eval\":%d,\"final_root_pc\":%d}\n",
		batches, final_root_eval, final_root_pc);
	g_batch_buffer += line;
	flush_batch_buffer();
}

void batch_start(int seq, int root_pc, int got_moves, int iter_budget) {
	if constexpr (!enabled) return;
	open_lazy();
	if (!g_log_open) return;

	g_events_this_batch = 0;
	g_events_dropped = 0;
	for (int i = 0; i < (int)Counter::counter_count; ++i) g_counters[i] = 0;

	char line[256];
	std::snprintf(line, sizeof(line),
		"{\"t\":\"batch_start\",\"seq\":%d,\"root_pc\":%d,\"got_moves\":%d,\"iter_budget\":%d}\n",
		seq, root_pc, got_moves, iter_budget);
	g_batch_buffer += line;
}

void batch_end(int seq, int iters_done, int root_eval, float root_avg_score) {
	if constexpr (!enabled) return;
	if (!g_log_open) return;

	char line[1024];
	std::snprintf(line, sizeof(line),
		"{\"t\":\"batch_end\",\"seq\":%d,\"iters_done\":%d,\"root_eval\":%d,\"root_eval_avg_score\":%.4f,"
		"\"counters\":{\"pred_total\":%d,\"pred_count_2\":%d,\"pred_count_3plus\":%d,"
		"\"dag_excl_adds\":%d,\"dag_excl_skips\":%d,"
		"\"nodes_terminal\":%d,\"nodes_via_explore_new\":%d,\"nodes_via_explore_random\":%d,"
		"\"events_dropped\":%d}}\n",
		seq, iters_done, root_eval, (double)root_avg_score,
		g_counters[(int)Counter::pred_total],
		g_counters[(int)Counter::pred_count_2],
		g_counters[(int)Counter::pred_count_3plus],
		g_counters[(int)Counter::dag_excl_adds],
		g_counters[(int)Counter::dag_excl_skips],
		g_counters[(int)Counter::nodes_terminal],
		g_counters[(int)Counter::nodes_via_explore_new],
		g_counters[(int)Counter::nodes_via_explore_random],
		g_events_dropped);
	g_batch_buffer += line;
	flush_batch_buffer();
}

void pred_fire(int depth, int count_at_fire, int path_size,
	const Node* parent, const Node* child, const Move& m) {
	if constexpr (!enabled) return;
	if (!g_log_open) return;
	if (g_events_this_batch >= max_events_per_batch) {
		g_events_dropped++;
		return;
	}
	g_events_this_batch++;

	char node_fen[128] = "";
	char child_fen[128] = "";
	char move_str[16] = "";

	// If Board::to_fen() / move_label have different signatures (verified
	// Task 2 Step 0), adapt the four lines below.
	if (parent && parent->_board) {
		std::string s = parent->_board->to_fen();
		copy_json_safe(node_fen, sizeof(node_fen), s.c_str());
	}
	if (child && child->_board) {
		std::string s = child->_board->to_fen();
		copy_json_safe(child_fen, sizeof(child_fen), s.c_str());
	}
	if (parent && parent->_board) {
		std::string s = parent->_board->move_label(m);
		copy_json_safe(move_str, sizeof(move_str), s.c_str());
	}

	int child_eval = (child && child->_deep_evaluation._evaluated)
		? child->_deep_evaluation._value : 0;
	float child_avg = (child && child->_deep_evaluation._evaluated)
		? child->_deep_evaluation._avg_score : 0.0f;
	int child_pc = child ? child->_parent_count : 0;

	char line[1024];
	std::snprintf(line, sizeof(line),
		"{\"t\":\"pred_fire\",\"depth\":%d,\"count_at_fire\":%d,\"path_size\":%d,"
		"\"child_pc\":%d,\"child_eval\":%d,\"child_avg\":%.4f,"
		"\"node_fen\":\"%s\",\"child_fen\":\"%s\",\"child_move\":\"%s\"}\n",
		depth, count_at_fire, path_size,
		child_pc, child_eval, (double)child_avg,
		node_fen, child_fen, move_str);
	g_batch_buffer += line;
}

void dag_excl_skip(int depth, const Node* node, const Move& m) {
	if constexpr (!enabled) return;
	if (!g_log_open) return;
	if (g_events_this_batch >= max_events_per_batch) {
		g_events_dropped++;
		return;
	}
	g_events_this_batch++;

	char move_str[16] = "";
	int node_pc = node ? node->_parent_count : 0;
	if (node && node->_board) {
		std::string s = node->_board->move_label(m);
		copy_json_safe(move_str, sizeof(move_str), s.c_str());
	}

	char line[256];
	std::snprintf(line, sizeof(line),
		"{\"t\":\"dag_excl_skip\",\"depth\":%d,\"node_pc\":%d,\"move\":\"%s\",\"reason\":\"in_excl\"}\n",
		depth, node_pc, move_str);
	g_batch_buffer += line;
}

void bump(Counter c) {
	if constexpr (!enabled) return;
	g_counters[(int)c]++;
}

} // namespace dag_log
```

- [ ] **Step 3: Add `.gitignore` entry.** Append to the workspace-root `.gitignore`:

```
# #11 DAG metrics runtime log artifact (cf. docs/.../2026-05-20-...-logging-design.md)
opti_chess/dag_metrics.log
```

If `.gitignore` does not yet exist, create it with the two lines above. If it exists, append (don't overwrite).

- [ ] **Step 4: Build.** Run the build command. Expected `EXITCODE=0`, no `error` lines.

  Sub-step on MSBuild not finding `dag_log.cpp`: report BLOCKED with message *"User: please add `opti_chess/dag_log.cpp` and `opti_chess/dag_log.h` to the Visual Studio project (Solution Explorer → opti_chess → Add → Existing Item) and rebuild."* Wait for user confirmation, then resume from Step 4.

- [ ] **Step 5: Commit.**

```bash
git add opti_chess/dag_log.h opti_chess/dag_log.cpp .gitignore
git commit -m "feat(log): add dag_log TU + gitignore for #11 metrics logging (#11, inert)"
```

OFF-byte-identical argument: nothing in `dag_log.{h,cpp}` is called from any other TU yet. Even with `enabled = true`, no symbol is invoked → no side effect. With `enabled = false`, every API entry returns immediately. Build adds two compilation units + binary symbols — all dead code.

---

## Task 3: Instrumentation in `exploration.cpp`

Add the five instrumentation call sites + counter bumps. Each new code block is wrapped in `if constexpr (dag_log::enabled)` so it's elided entirely under the OFF toggle.

**Files:**
- Modify: `opti_chess/exploration.cpp`

- [ ] **Step 1: Add `#include "dag_log.h"`.** At the top of `exploration.cpp`, near the existing `#include "exploration.h"`:

```cpp
#include "dag_log.h"
```

- [ ] **Step 2: §3 cut block — emit `pred_fire` + counters.** Locate (Grep `position_is_draw_by_repetition` inside `Node::explore_random_child`). The current block reads (around line 790 baseline):

```cpp
		if (g_tt_node_dag && position_is_draw_by_repetition(*branch_history, *child->_board)) {
			if (dag_excl != nullptr) dag_excl->add(move);
			// Bug 1 opt 1 — spec §3 : remonte la valeur de nulle path-locale par
			// VALEUR DE RETOUR (out-param), sans muter le Node/arete partages
			// (invariant 772183a). Le parent substituera cette nulle a
			// child->_deep_evaluation pour CETTE traversee (cf. backup).
			if (path_local_eval != nullptr) {
				*path_local_eval = dag_draw_eval();
			}
			_iterations++;
			return;
		}
```

(The branch_history parameter type — `*branch_history` vs `branch_history` — must match the existing call site. Verify by reading the surrounding context.)

Replace with:

```cpp
		if (g_tt_node_dag && position_is_draw_by_repetition(*branch_history, *child->_board)) {
			if (dag_excl != nullptr) {
				dag_excl->add(move);
				if constexpr (dag_log::enabled) {
					dag_log::bump(dag_log::Counter::dag_excl_adds);
				}
			}
			// Bug 1 opt 1 — spec §3 : remonte la valeur de nulle path-locale par
			// VALEUR DE RETOUR (out-param), sans muter le Node/arete partages
			// (invariant 772183a). Le parent substituera cette nulle a
			// child->_deep_evaluation pour CETTE traversee (cf. backup).
			if (path_local_eval != nullptr) {
				*path_local_eval = dag_draw_eval();
			}

			if constexpr (dag_log::enabled) {
				// One inline lookup ; same key the predicate just checked.
				child->_board->get_zobrist_key();
				const auto it = branch_history->find(child->_board->_zobrist_key);
				const int current_count = (it == branch_history->end()) ? 0 : (int)it->second;
				const int count_at_fire = current_count + 1;
				const int path_size = (int)branch_history->size();
				dag_log::bump(dag_log::Counter::pred_total);
				if (count_at_fire == 2) {
					dag_log::bump(dag_log::Counter::pred_count_2);
				} else {
					dag_log::bump(dag_log::Counter::pred_count_3plus);
				}
				dag_log::pred_fire(path_size, count_at_fire, path_size,
					this, child, move);
			}

			_iterations++;
			return;
		}
```

(The opt-1 `*path_local_eval = dag_draw_eval()` write is preserved verbatim — it is part of the baseline functional state at `6e8c030`/`a8b4004` and this plan changes no search semantics. Touching it would be out-of-scope.)

Note: `depth` parameter to `pred_fire` is set to `path_size` here as a proxy (since the search has no separate depth counter; `path_history.size()` IS the depth from root). Using the same value for both `depth` and `path_size` is intentional and documented in the spec.

- [ ] **Step 3: `pick_random_child` DagExcl skip — emit `dag_excl_skip` + counter.** Locate (Grep `dag_excl->contains\|dag_excl.contains` inside `Node::pick_random_child`). The current site filters out moves in DagExcl with a `continue`. Wrap that branch:

```cpp
			if (dag_excl != nullptr && dag_excl->contains(move)) {
				if constexpr (dag_log::enabled) {
					dag_log::bump(dag_log::Counter::dag_excl_skips);
					// depth=0 sentinel — pick_random_child doesn't have branch_history in
					// scope ; event still useful via the node's _board FEN if needed later.
					dag_log::dag_excl_skip(0, this, move);
				}
				continue; // opt-3 anti-spin (existing baseline)
			}
```

(If the current code uses a slightly different filter form, adapt — but the `if`/`continue` shape is the goal.)

- [ ] **Step 4: `explore_new_move` entry — bump `nodes_via_explore_new`.** Locate `void Node::explore_new_move(` (around exploration.cpp:479 baseline). Add at the very start of the function body (before any early-return guards is fine — the counter just tracks invocations):

```cpp
	if constexpr (dag_log::enabled) {
		dag_log::bump(dag_log::Counter::nodes_via_explore_new);
	}
```

- [ ] **Step 5: `explore_random_child` entry (post §3 cut) — bump `nodes_via_explore_random`.** Locate the same function. After the `}` closing the §3 cut block (modified in Step 2) — i.e., when we DIDN'T cut — add:

```cpp
	if constexpr (dag_log::enabled) {
		dag_log::bump(dag_log::Counter::nodes_via_explore_random);
	}
```

- [ ] **Step 6: `grogros_zero` terminal early-returns — bump `nodes_terminal`.** Locate `void Node::grogros_zero(` (around exploration.cpp:356 baseline). Find these specific early-return paths and add the bump just before each `return;`:

- The `if (_is_terminal)` block (~line 403)
- The `if (_board->_got_moves <= 0)` block (~line 421)

For each:

```cpp
		if constexpr (dag_log::enabled) {
			dag_log::bump(dag_log::Counter::nodes_terminal);
		}
		return;
```

Do NOT add to the `iterations <= 0` (line 365) or `recursion depth guard` (line 373) returns — those are not chess-terminal.

- [ ] **Step 7: Build.** Expected `EXITCODE=0`, no `error` lines.

- [ ] **Step 8: Commit.**

```bash
git add opti_chess/exploration.cpp
git commit -m "feat(log): instrument exploration.cpp with DAG metrics events + counters (#11, inert)"
```

OFF-byte-identical argument: every new call is wrapped in `if constexpr (dag_log::enabled)` which elides at compile time when toggle is false. With `enabled = true`, the new code runs only on the §3 cut path (rare; one extra `find()` + a snprintf-into-string append) and adds a single integer increment at each `explore_new_move`/`explore_random_child`/`grogros_zero` terminal-return entry — comparable cost to existing `_iterations++` increments. No allocation per event (the per-batch string buffer is pre-reserved).

---

## Task 4: `gui.cpp` hooks + `tests.cpp` repros + key bindings

Wire the root grogros_zero call to `session_start`/`batch_start`/`batch_end`; add `run_dag_repro_*` entry points in `tests.cpp`; bind keys `1` and `2` in `gui.cpp` for one-key repro invocation.

**Files:**
- Modify: `opti_chess/gui.cpp`
- Modify: `opti_chess/tests.cpp`
- Modify: `opti_chess/tests.h` (if needed to expose `run_dag_repro_*` to `gui.cpp`)

- [ ] **Step 1: Verify gui.cpp key dispatch + existing test entry pattern.** Grep:

```
grep -n "IsKeyPressed" opti_chess/gui.cpp | head -20
grep -n "KEY_ONE\|KEY_TWO\|KEY_O\b\|KEY_I\b" opti_chess/gui.cpp
grep -n "^void\|^bool" opti_chess/tests.cpp | head -20
```

Record:
- The block where `IsKeyPressed(KEY_O)` or similar is dispatched (this is where the new key bindings go).
- Confirm `KEY_ONE` (the raylib `1`) and `KEY_TWO` (the raylib `2`) are not bound. If they are, pick the next free pair (e.g., `KEY_F1`/`KEY_F2`) and use those.
- Note one existing entry-point in `tests.cpp` to mirror the include/setup pattern of `run_dag_repro`.

- [ ] **Step 2: Add `#include "dag_log.h"` to `gui.cpp`.** Near the existing includes.

- [ ] **Step 3: Wrap the root `grogros_zero` call (gui.cpp:1058) with session/batch hooks.** Locate the call at line 1058 (verified L-Task 1):

```cpp
	_root_exploration_node->grogros_zero(&monte_board_buffer, _grogros_eval, _alpha, _beta, _gamma, iterations == -1 ? iterations_to_explore : iterations, _quiescence_depth); // TODO: nombre de noeuds à paramétrer
```

Replace with:

```cpp
	// #11 DAG metrics logging hooks (cf. dag_log.h). Wrapping the root call.
	// OFF byte-identique car if constexpr (dag_log::enabled==false) élimine
	// tout à la compilation.
	const int eff_iterations = iterations == -1 ? iterations_to_explore : iterations;

	if constexpr (dag_log::enabled) {
		static std::string s_last_fen;
		static int s_batch_seq = 0;
		std::string cur_fen = _board->to_fen();   // adapt if to_fen has different signature
		if (cur_fen != s_last_fen) {
			extern bool g_tt_node_dag;
			extern bool g_tt_main_search;
			dag_log::session_start(cur_fen.c_str(), g_tt_node_dag, g_tt_main_search,
				eff_iterations, nullptr);
			s_last_fen = cur_fen;
			s_batch_seq = 0;
		}
		dag_log::batch_start(s_batch_seq,
			_root_exploration_node ? _root_exploration_node->_parent_count : 0,
			_board->_got_moves, eff_iterations);
	}

	_root_exploration_node->grogros_zero(&monte_board_buffer, _grogros_eval, _alpha, _beta, _gamma, eff_iterations, _quiescence_depth); // TODO: nombre de noeuds à paramétrer

	if constexpr (dag_log::enabled) {
		static int s_batch_seq_after = 0;  // separate counter for post-call increment
		dag_log::batch_end(s_batch_seq_after,
			eff_iterations,
			_root_exploration_node ? _root_exploration_node->_deep_evaluation._value : 0,
			_root_exploration_node ? _root_exploration_node->_deep_evaluation._avg_score : 0.0f);
		s_batch_seq_after++;
	}
```

(If `_board->to_fen()` doesn't compile because the actual API differs, adapt to the verified signature. If `_board` is a different member name in this enclosing function, use the actual GUI member.)

Note on the two `s_batch_seq` statics: they're separately scoped to keep the pre-call and post-call sequence numbers independent and monotonic. If the enclosing function is called from multiple sites, both statics advance together — acceptable for our use case (we just need monotonic-ish sequence labels in the log).

- [ ] **Step 4: Add `#include "dag_log.h"` and `#include "exploration.h"` to `tests.cpp`.** Near existing includes.

- [ ] **Step 5: Add `run_dag_repro` + wrappers in `tests.cpp`.** Append to `opti_chess/tests.cpp`:

```cpp
// #11 DAG metrics — manual repro entry points (cf. design 2026-05-20 §9 ;
// dag_log.h). Loads a known FEN, sets DAG ON (Plan A OFF), runs N batches of
// K iterations of grogros_zero, emits structured metrics to
// opti_chess/dag_metrics.log. Self-contained ; called from GUI key bindings.

#include "dag_log.h"
#include "exploration.h"
#include "buffer.h"
#include "evaluator.h"

extern bool g_tt_node_dag;
extern bool g_tt_main_search;

void run_dag_repro(const char* repro_name, const char* fen,
	int n_batches, int iters_per_batch) {

	const bool saved_dag = g_tt_node_dag;
	const bool saved_pa  = g_tt_main_search;
	g_tt_node_dag = true;
	g_tt_main_search = false;

	// Setup. Match the pattern of an existing test function in tests.cpp
	// (verified Task 4 Step 1). The skeleton below is illustrative ; the
	// implementer adapts to actual BoardBuffer/NodeBuffer/Evaluator APIs.
	BoardBuffer board_buf;
	NodeBuffer  node_buf;
	Evaluator   evaluator;

	Board* root_board = board_buf.alloc();
	root_board->from_fen(fen);

	Node* root = node_buf.alloc();
	root->_board = root_board;
	root->_parent_count = 1;

	dag_log::session_start(fen, g_tt_node_dag, g_tt_main_search,
		iters_per_batch, repro_name);

	int final_eval = 0;
	int final_pc = 0;

	for (int b = 0; b < n_batches; ++b) {
		dag_log::batch_start(b, root->_parent_count,
			root_board->_got_moves, iters_per_batch);

		// grogros_zero with the project's standard arg conventions. Trailing
		// path_history = nullptr matches the GUI call site (gui.cpp:1058) —
		// grogros_zero creates its own local_path_history (exploration.cpp:391).
		root->grogros_zero(&board_buf, &evaluator,
			/* alpha */ 1.0, /* beta */ 1.0, /* gamma */ 1.0,
			iters_per_batch, /* quiescence_depth */ 0,
			/* network */ nullptr);

		final_eval = root->_deep_evaluation._value;
		final_pc = root->_parent_count;

		dag_log::batch_end(b, iters_per_batch,
			root->_deep_evaluation._value,
			root->_deep_evaluation._avg_score);
	}

	dag_log::session_end(n_batches, final_eval, final_pc);

	// Cleanup. Match the project's reset/free conventions.
	root->reset(true);
	g_tt_node_dag = saved_dag;
	g_tt_main_search = saved_pa;
}

void run_dag_repro_1() {
	// Position 1 — théoriquement nulle (K+pion h vs K). Moteur false-wins
	// actuellement sous DAG ON. Cible après fix : nulle quasi-instantanée.
	run_dag_repro("repro1_kp_h_draw",
		"6k1/8/7P/7K/8/8/8/8 w - - 3 72",
		5, 1000);
}

void run_dag_repro_2() {
	// Position 2 — gain blanc (Ke3 ou Ke2). Anchor de NON-RÉGRESSION.
	// DAG ON trouve le gain en ~2s actuellement (vs ~1min sans DAG).
	run_dag_repro("repro2_pawn_endgame_win",
		"8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7",
		5, 1000);
}
```

If `tests.cpp` has actual existing test infrastructure with a different setup pattern (e.g., uses helper functions to alloc Board+Node), use those helpers instead. The block above is the structural target; the implementer adapts.

If `BoardBuffer`/`NodeBuffer`/`Evaluator` APIs don't expose `alloc()` (they likely use indexed access or `_heap_boards[i]` per memory), match the existing project pattern.

- [ ] **Step 6: Declare the entry points in `tests.h`** so `gui.cpp` can call them. Append:

```cpp
// #11 DAG metrics logging — manual repro entry points (cf. tests.cpp).
void run_dag_repro_1();
void run_dag_repro_2();
```

If `tests.h` doesn't exist, declare them at the top of `gui.cpp` as `extern` instead:

```cpp
extern void run_dag_repro_1();
extern void run_dag_repro_2();
```

- [ ] **Step 7: Bind keys `1` and `2` in `gui.cpp`.** At the location confirmed Step 1, in the key-handling block:

```cpp
	if (IsKeyPressed(KEY_ONE)) {
		run_dag_repro_1();
	}
	if (IsKeyPressed(KEY_TWO)) {
		run_dag_repro_2();
	}
```

(Use the keys confirmed free in Step 1; if `KEY_ONE`/`KEY_TWO` are taken, swap to the free pair noted there.)

- [ ] **Step 8: Build.** Expected `EXITCODE=0`, no `error` lines.

  If the build fails because `run_dag_repro` uses APIs that don't exist (e.g., wrong constructor for `BoardBuffer`), adapt to the actual project pattern by reading an existing `tests.cpp` entry-point. If the build fails because `tests.cpp` isn't auto-discovered by MSBuild after adding new functions to it, report BLOCKED.

- [ ] **Step 9: Commit.**

```bash
git add opti_chess/gui.cpp opti_chess/tests.cpp opti_chess/tests.h
git commit -m "feat(log): gui+tests hooks - session/batch around root grogros_zero + repro keys 1/2 (#11)"
```

(Omit `tests.h` from `git add` if Step 6 didn't create/modify it.)

OFF-byte-identical argument: all new gui.cpp code is wrapped in `if constexpr (dag_log::enabled)`. With `enabled = false`, the wrappers compile to nothing → root grogros_zero call is invoked identically to baseline (with `eff_iterations` substituted but value identical to the previous inline ternary). Key bindings are new but only fire on user input. `run_dag_repro_*` only runs when the user presses the key; it temporarily flips `g_tt_node_dag` and restores it.

---

## Task 5: Final build verification + push

Verify the cumulative change builds clean on both toggle states, push the branch.

- [ ] **Step 1: Build with `dag_log::enabled = true`.** Run the build command. Expected `EXITCODE=0`, no `error` lines.

- [ ] **Step 2: Temporary toggle off check.** Edit `opti_chess/dag_log.h`, change `constexpr bool enabled = true;` to `constexpr bool enabled = false;`. Run the build command. Expected `EXITCODE=0`, no `error` lines. (Verifies the OFF-path compiles cleanly — all the `if constexpr` wrappers must be syntactically valid in both states.) Revert the toggle to `true`. Do NOT commit the off-toggle change.

- [ ] **Step 3: Verify the toggle is back to `true`.**

```bash
grep -n "constexpr bool enabled" opti_chess/dag_log.h
```

Expected output: `constexpr bool enabled = true;`.

- [ ] **Step 4: Push the branch.**

```bash
git push origin feature/tt-main-search
```

- [ ] **Step 5: Final summary message to user.** Report:

- Total commits added this session for the logging plan.
- Branch tip SHA.
- Instructions for user: *"In the GUI, press `1` to run Repro 1 (KP(h)-vs-K theoretical draw) for 5 batches × 1000 iters with DAG ON, or `2` to run Repro 2 (winning pawn endgame, non-regression anchor). The log accumulates at `opti_chess/dag_metrics.log` (gitignored). Once you have data from each repro, either `git add -f opti_chess/dag_metrics.log && git commit && git push`, or paste a representative slice into a new chat — I'll do the analysis."*

No further code commits in this plan. The implementation is COMPLETE at this point.

---

## Out of scope (handled in a separate future plan)

- Reading the log file and doing the analysis. Happens AFTER user runs the repros.
- Designing the actual #11 fix. The fix design is a separate brainstorm informed by the analysis.
- Removing the `dag_log::enabled = true` default after analysis is done. Leaving it `true` is fine while the bug is open.
- Loading `Board::_positions_history` into the search's `path_history`. This is a real semantic gap noted in the spec §2, but fixing it is a search-logic change — out of scope for observability.

---

## Self-Review

**Spec coverage** (`docs/superpowers/specs/2026-05-20-optichess-dag-metrics-logging-design.md`):

- §2 scope (new TU pair, instrumentation, .gitignore, repro entry) → Tasks 2, 3, 4. ✓
- §3 compile-time toggle → Task 2 Step 1 defines `constexpr bool enabled`; every API entry checks it; Task 3/4 wrap call sites in `if constexpr`. ✓
- §4 file output (JSON-lines, append, gitignored, lazy open) → Task 2 Step 2 + Step 3. ✓
- §5 event schema (session/batch/pred_fire/dag_excl_skip + counters) → Task 2 Step 2 emits matching JSON; Task 3 calls match the signatures. ✓
- §6 API → Task 2 Step 1. ✓
- §7 instrumentation points (5 in exploration.cpp + 1 in gui.cpp) → Task 3 (5 sites) + Task 4 Step 3 (1 site, hooks around root call). ✓
- §8 `count_at_fire`/`path_size` computation → Task 3 Step 2 (inline `find()` lookup at §3 cut site). ✓
- §9 repro convenience entry point → Task 4 Steps 5-7 (`run_dag_repro` + `_1` + `_2` + key bindings). ✓
- §10 OFF byte-identicality → per-task OFF arguments + Task 5 Step 2 explicit OFF-toggle build check. ✓
- §11 performance (zero alloc per event via `snprintf`, file I/O only at batch boundaries, capped detail events) → Task 2 Step 2 implementation matches. ✓
- §14 acceptance criteria (build both toggles, expected log shape) → Task 5 covers build half; runtime half is USER step. ✓

No spec requirement is left without a task. The `_game_history_size` task originally numbered Task 3 in the plan has been dropped in coordination with the spec update.

**Placeholder scan:** No `TBD`/`TODO`/`fill in later`. The `Board::to_fen()` / `Board::move_label(Move)` signature verification is an explicit pre-step (Task 2 Step 0) with a documented adaptation path if they differ. The `tests.cpp` setup pattern adaptation is documented (Task 4 Step 5) as "match an existing entry-point" — this is a real verification gate, not a placeholder.

**Type consistency:** `dag_log::Counter` enum values (`pred_total`, `pred_count_2`, `pred_count_3plus`, `dag_excl_adds`, `dag_excl_skips`, `nodes_terminal`, `nodes_via_explore_new`, `nodes_via_explore_random`) match between Task 2 Step 1 (decl), Task 2 Step 2 (counter array indexing in `batch_end`), and Task 3 Steps 2-6 (call sites). `dag_log::pred_fire(int depth, int count_at_fire, int path_size, const Node*, const Node*, const Move&)` matches between Task 2 Step 1 (decl), Task 2 Step 2 (def), and Task 3 Step 2 (call). `dag_log::dag_excl_skip(int, const Node*, const Move&)` matches the same way. `dag_log::session_start` / `session_end` / `batch_start` / `batch_end` signatures consistent across def + caller in Task 4 Step 3. `run_dag_repro_1()` / `run_dag_repro_2()` signature `void()` consistent across decl, def, and caller.

---

**Execution note:** No automated test runner; per task the "test" is the clean MSBuild + the written OFF-byte-identical argument. Final runtime validation is the **USER** step after Task 5 (`run_dag_repro_1()` + `run_dag_repro_2()` via keys `1`/`2` in the GUI). The assistant cannot run the raylib GUI. Once the user shares the resulting `opti_chess/dag_metrics.log`, the analysis phase begins in a separate brainstorm.
