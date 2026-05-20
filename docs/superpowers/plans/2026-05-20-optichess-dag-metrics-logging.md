# DAG metrics logging — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add persistent file-based JSON-lines metrics logging to characterize DAG search behavior on issue #11 reference positions, so the next design attempt is grounded in measured runtime evidence rather than speculation. Observability-only — no search-logic changes.

**Architecture:** New self-contained TU pair `opti_chess/dag_log.{h,cpp}` exposes a small API (session/batch lifecycle + per-event detail + counter increments). All API calls are short-circuited to no-ops by a `constexpr bool dag_log::enabled` gate so OFF builds are byte-identical to baseline. Instrumentation calls are placed at 5 sites in `exploration.cpp`, the root `grogros_zero` call in `gui.cpp`, and a new repro entry point in `tests.cpp`. A `PositionHistory::_game_history_size` field (informational only — no logic reads it) lets the §3 cut decompose predicate fires into game-history vs search-traversal matches — the **single decomposition all six prior #11 design attempts lacked direct visibility into**.

**Tech Stack:** C++17, MSVC (`opti_chess.sln`), single-threaded engine. No automated test framework — "test" per task = clean MSBuild + a written OFF-byte-identical argument + a couple of runtime structural checks (e.g., grep that the log file is created with valid JSON-lines after a smoke run). Final acceptance is **USER-run** (assistant cannot run the raylib GUI): the user invokes the two repro entry points and shares the resulting log file.

Design spec: `docs/superpowers/specs/2026-05-20-optichess-dag-metrics-logging-design.md`.

---

## Build command

Variant B (Insiders Debug x64), confirmed on this machine in the prior #11 session. Run from the project working dir in PowerShell:

```
& 'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe' opti_chess.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m /nologo /clp:ErrorsOnly /v:m; "EXITCODE=$LASTEXITCODE"
```

Success = `EXITCODE=0` and no `error` lines. **Do NOT modify any `.vcxproj`/`.sln`/project file.** When a new `.cpp` is created, the VS project auto-discovers it on next IDE open, but command-line MSBuild needs it in the project file. If MSBuild fails to include `dag_log.cpp` automatically, the implementer must NOT edit the `.vcxproj` — instead, ask the user to add the new files to the VS project via the IDE, then resume.

Commits: English, ASCII, conventional commit, `(#11)` ref, **no AI attribution**. French comments use real Unicode (é, è, à, ç, ê, î, ô, û, ù, œ). Do not commit `opti_chess/TODO_list.txt`, `opti_chess/Tests.txt`, `opti_chess/Tests.txt+tmp`, or `opti_chess/dag_metrics.log` (the latter will be added to `.gitignore` in Task 2). Do not commit any prior plan file (e.g., `docs/superpowers/plans/2026-05-19-optichess-dag-forced-draw-bubble.md` left untracked from earlier).

Baseline: tip `48ee48a` (spec update committed; functional state `a8b4004` = `6e8c030` content).

---

## File structure

All changes touch six files; two are new:

- **Create**: `opti_chess/dag_log.h` — API + constexpr toggle + Counter enum + forward decls.
- **Create**: `opti_chess/dag_log.cpp` — file handle, lazy-open, JSON formatting via `snprintf`, per-batch counter state, flush at batch boundaries.
- **Modify**: `opti_chess/exploration.h` — add `_game_history_size` field to `PositionHistory` (and an accessor `game_history_size()`); declarations only.
- **Modify**: `opti_chess/exploration.cpp` — set `_game_history_size` in the FEN-loading code path; add 5 instrumentation call sites (§3 cut, `pick_random_child` DagExcl skip, `explore_new_move` entry, `explore_random_child` entry, `grogros_zero` terminal returns).
- **Modify**: `opti_chess/gui.cpp` — wrap the root `grogros_zero` call with `session_start` / `batch_start` / `batch_end`; add two key bindings ('1' and '2') invoking `run_dag_repro_1()` / `run_dag_repro_2()`.
- **Modify**: `opti_chess/tests.cpp` — add `run_dag_repro(name, fen, n_batches, iters_per_batch)` plus `run_dag_repro_1()` and `run_dag_repro_2()` wrappers.
- **Modify**: `.gitignore` (workspace root) — add `opti_chess/dag_metrics.log`.

`PositionHistory` is currently declared in `opti_chess/exploration.h` (per memory and recent #11 session knowledge). If Task 1 finds it lives elsewhere, the implementer adjusts paths.

Task order: design gate (no code) → new TU `dag_log` (inert) → `PositionHistory` field (inert) → exploration.cpp instrumentation → gui.cpp + tests.cpp invocation + key bindings → final build + push.

---

## Task 1: Design-validation gate (no code)

**Files:** none (this plan, annotated).

Six prior #11 attempts each tripped on an unverified assumption. Confirm the structural facts before any code edit.

- [ ] **Step 1: Confirm the build invocation.** Variant B path exists. Note any deviation if MSBuild reports a project-file issue when a new `.cpp` is added (Task 2 may need the user to add files via VS IDE).

- [ ] **Step 2: Locate `PositionHistory`.** Grep `class PositionHistory\|struct PositionHistory` across `opti_chess/`. Confirm the file. Read its declaration to identify:
  - Storage of zobrist keys (likely a `std::vector<uint64_t>` or a fixed array)
  - Existing accessors (`size()`, `push()`, `pop()`, key iteration)
  - Where it's instantiated and how it's initialized when the GUI loads a FEN
  Record the exact field names and the file paths so Tasks 3 and 4 use the verified API.

- [ ] **Step 3: Locate root `grogros_zero` call in `gui.cpp`.** Grep `_root_exploration_node->grogros_zero\|->grogros_zero(` in `gui.cpp`. There may be one or two call sites (per-batch invocation, possibly a one-shot variant). Record the line numbers + full argument list. Task 5's gui.cpp instrumentation wraps these exact calls.

- [ ] **Step 4: Locate FEN-loading code.** Grep `from_fen\|load_fen\|set_position` in `opti_chess/`. Find where the GUI loads a position string and where `PositionHistory` is (re-)initialized with game-history. Record the exact site so Task 3 sets `_game_history_size` correctly.

- [ ] **Step 5: Confirm `Board::to_fen()` and `Board::move_label(Move)` (or equivalents).** Grep `to_fen\|move_label\|to_string.*move\|move_to_string` in `opti_chess/board.{h,cpp}`. Record the actual signatures. If `to_fen` returns `std::string`, fine; if it requires a buffer, adapt accordingly. Task 2 + Task 4 use these in `pred_fire`/`dag_excl_skip` event payloads.

- [ ] **Step 6: Locate key-binding handling in `gui.cpp`.** Grep `IsKeyPressed\|KEY_O\|KEY_I\b` in `gui.cpp` to find the existing key dispatch block (where `O` toggles DAG and `I` toggles Plan A). Confirm '1' (`KEY_ONE`) and '2' (`KEY_TWO`) are not yet bound. If they are, pick the next free pair and document in this step. Task 5 adds the new bindings there.

- [ ] **Step 7: Confirm `tests.cpp` is in the build.** Grep `void.*test_\|void.*Test\|run_tests` in `opti_chess/tests.cpp` to confirm it compiles as part of the project. Record one existing entry-point signature so the new `run_dag_repro` follows the same pattern.

- [ ] **Step 8: Confirm Position 1 and Position 2 FENs parse.** Spot-check that `6k1/8/7P/7K/8/8/8/8 w - - 3 72` (Position 1) and `8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7` (Position 2) are syntactically valid (no obvious typo). Recommended: by inspection only; no runtime test required.

- [ ] **Step 9: No commit (design only).**

---

## Task 2: New TU `dag_log.{h,cpp}` + `.gitignore` (fully inert)

Create the logging API + file-output implementation. No caller exists yet → fully byte-identical to baseline both at `dag_log::enabled = true` and `dag_log::enabled = false`.

**Files:**
- Create: `opti_chess/dag_log.h`
- Create: `opti_chess/dag_log.cpp`
- Modify: `.gitignore` (workspace root)

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
	// to a no-op at the entry; call sites also wrap in if constexpr to elide
	// argument evaluation in release builds.
	constexpr bool enabled = true;

	// Cap on per-event detail events emitted per batch. Beyond this cap,
	// counter increments still run but detail events are dropped (counted
	// in `events_dropped`).
	constexpr int max_events_per_batch = 200;

	enum class Counter {
		pred_total,
		pred_search_traversal,
		pred_game_history,
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

	void pred_fire(bool is_game_history, int depth, int hist_idx,
		int hist_size, int game_hist_size,
		const Node* parent, const Node* child, const Move& m);

	void dag_excl_skip(int depth, const Node* node, const Move& m);

	void bump(Counter c);

} // namespace dag_log
```

- [ ] **Step 2: Create `opti_chess/dag_log.cpp`.** Write exactly (uses `snprintf` into stack buffers + a single reusable batch buffer to avoid per-event heap allocations):

```cpp
#include "dag_log.h"
#include "exploration.h"   // Node, _board, _parent_count, _deep_evaluation
#include "board.h"         // Board, Move, to_fen, move_label (verified Task 1)

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

// Copy a string into out_buf with size limit, ensuring NUL-termination and
// escaping just `"` and `\` (FENs / SAN are ASCII-safe otherwise).
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
		"\"counters\":{\"pred_total\":%d,\"pred_search_traversal\":%d,\"pred_game_history\":%d,"
		"\"dag_excl_adds\":%d,\"dag_excl_skips\":%d,"
		"\"nodes_terminal\":%d,\"nodes_via_explore_new\":%d,\"nodes_via_explore_random\":%d,"
		"\"events_dropped\":%d}}\n",
		seq, iters_done, root_eval, (double)root_avg_score,
		g_counters[(int)Counter::pred_total],
		g_counters[(int)Counter::pred_search_traversal],
		g_counters[(int)Counter::pred_game_history],
		g_counters[(int)Counter::dag_excl_adds],
		g_counters[(int)Counter::dag_excl_skips],
		g_counters[(int)Counter::nodes_terminal],
		g_counters[(int)Counter::nodes_via_explore_new],
		g_counters[(int)Counter::nodes_via_explore_random],
		g_events_dropped);
	g_batch_buffer += line;
	flush_batch_buffer();
}

void pred_fire(bool is_game_history, int depth, int hist_idx,
	int hist_size, int game_hist_size,
	const Node* parent, const Node* child, const Move& m) {
	if constexpr (!enabled) return;
	if (!g_log_open) return;
	if (g_events_this_batch >= max_events_per_batch) {
		g_events_dropped++;
		return;
	}
	g_events_this_batch++;

	// FEN + move-label extraction. APIs verified in Task 1; if Board::to_fen
	// requires a buffer rather than returning std::string, adapt these calls.
	char node_fen[128] = "";
	char child_fen[128] = "";
	char move_str[16] = "";

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
		"{\"t\":\"pred_fire\",\"kind\":\"%s\",\"depth\":%d,\"hist_idx\":%d,\"hist_size\":%d,\"game_hist_size\":%d,"
		"\"child_pc\":%d,\"child_eval\":%d,\"child_avg\":%.4f,"
		"\"node_fen\":\"%s\",\"child_fen\":\"%s\",\"child_move\":\"%s\"}\n",
		is_game_history ? "game_history" : "search_traversal",
		depth, hist_idx, hist_size, game_hist_size,
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

(If Task 1 found that `Board::to_fen` or `Board::move_label` have different names/signatures, adapt the four lines that call them. The remainder is signature-independent.)

- [ ] **Step 3: Add `.gitignore` entry.** Append to the workspace-root `.gitignore`:

```
# #11 DAG metrics runtime log artifact (cf. docs/.../2026-05-20-...-logging-design.md)
opti_chess/dag_metrics.log
```

If `.gitignore` does not yet exist at workspace root, create it with the two lines above. If it exists, append (don't overwrite).

- [ ] **Step 4: Build.** Run the build command. Expected `EXITCODE=0`, no `error` lines.

  **Sub-step on linker complaint about `dag_log.cpp` not being in the project**: MSBuild may not auto-discover the new `.cpp`. If it reports `dag_log.cpp` is unbuilt, do NOT edit `opti_chess.vcxproj`. Pause, report `BLOCKED` with the message: *"User: please add `opti_chess/dag_log.cpp` and `opti_chess/dag_log.h` to the Visual Studio project (Solution Explorer → opti_chess → Add → Existing Item) and rebuild."* Once user confirms, resume from Step 4.

- [ ] **Step 5: Commit.**

```bash
git add opti_chess/dag_log.h opti_chess/dag_log.cpp .gitignore
git commit -m "feat(log): add dag_log TU + gitignore for #11 metrics logging (#11, inert)"
```

OFF-byte-identical argument: nothing in `dag_log.{h,cpp}` is called from any other TU yet (Tasks 4-5 wire the callers). Even with `enabled = true`, no symbol is invoked → no side effect. With `enabled = false`, every API entry returns immediately. Build adds two compilation units + one binary symbol per public function — all dead code.

---

## Task 3: `PositionHistory::_game_history_size` field (fully inert)

Add the informational field + accessor + initialization in the FEN loader. No production code reads it (only Task 4's logging-gated reads will). OFF byte-identical.

**Files:**
- Modify: `opti_chess/exploration.h` (or wherever `PositionHistory` lives — verified Task 1 Step 2)
- Modify: `opti_chess/exploration.cpp` (or the FEN loader file — verified Task 1 Step 4)

- [ ] **Step 1: Add the field + accessor.** In the `PositionHistory` declaration (location confirmed Task 1 Step 2), add:

```cpp
	// #11 GHI metrics — informational. Split between game-history-prefix
	// (entries from the played game, loaded with the FEN) and search-
	// traversal entries (pushed by PathScope during descent). Set when
	// PositionHistory is (re-)initialized from a FEN ; never modified by
	// PathScope push/pop. Aucun chemin de production ne lit ce champ ;
	// uniquement le logging gate (dag_log::enabled) le consulte.
	size_t _game_history_size = 0;

	size_t game_history_size() const { return _game_history_size; }
```

Place the field next to existing storage members (per the existing convention in the struct/class).

- [ ] **Step 2: Set the field in the FEN loader.** At the site identified in Task 1 Step 4 — where `PositionHistory` is (re-)initialized for a freshly loaded position — set `_game_history_size = <current_size_after_history_load>`. Concrete example shape (adjust to the actual code):

```cpp
// existing FEN-load code that builds the history from played-game moves:
hist.push_or_init_from_played_moves(...);
hist._game_history_size = hist.size();   // mark end of game-history prefix
```

If the FEN loader is in a different TU, modify there. If the existing loader doesn't explicitly populate history (e.g., starts from `position startpos`), `_game_history_size` stays at 0 — that's correct (no game-history → every pred fire is a search-traversal match).

- [ ] **Step 3: Build.** Expected `EXITCODE=0`, no `error` lines.

- [ ] **Step 4: Commit.**

```bash
git add opti_chess/exploration.h opti_chess/exploration.cpp
git commit -m "feat(history): add PositionHistory::_game_history_size informational field (#11, inert)"
```

(If the FEN loader is in a different file than `exploration.cpp`, include that file in the `git add` instead/also.)

OFF-byte-identical argument: the field is initialized to 0 (so existing behavior with no game-history is preserved); only Task 4's logging code reads it; logging code is gated `dag_log::enabled`. With `enabled = false`, the field is allocated but never read → no observable effect.

---

## Task 4: Instrumentation in `exploration.cpp`

Add the five instrumentation call sites + counter bumps. Each new call is gated implicitly by the `if constexpr (dag_log::enabled)` inside the `dag_log::*` functions; for clarity and to ensure full elision under `enabled = false`, wrap the call site itself in `if constexpr (dag_log::enabled)` too where multiple lines are involved.

**Files:**
- Modify: `opti_chess/exploration.cpp`

- [ ] **Step 1: Add `#include "dag_log.h"`.** At the top of `exploration.cpp`, near the existing `#include "exploration.h"`:

```cpp
#include "dag_log.h"
```

- [ ] **Step 2: §3 cut block — emit `pred_fire` + counters.** Locate (Grep `position_is_draw_by_repetition` inside `Node::explore_random_child`). The current block is:

```cpp
		if (g_tt_node_dag && position_is_draw_by_repetition(*branch_history, *child->_board)) {
			if (dag_excl != nullptr) dag_excl->add(move);
			// existing comment block...
			_iterations++;
			return;
		}
```

(The exact branch_history parameter type — `*branch_history` vs `branch_history` — depends on whether it's a pointer or reference at this call site. Verify and match.)

Replace with (keep the structural cut, add logging hooks; if logging disabled, the `if constexpr` block is dead code):

```cpp
		if (g_tt_node_dag && position_is_draw_by_repetition(*branch_history, *child->_board)) {
			if (dag_excl != nullptr) {
				dag_excl->add(move);
				if constexpr (dag_log::enabled) {
					dag_log::bump(dag_log::Counter::dag_excl_adds);
				}
			}

			if constexpr (dag_log::enabled) {
				// Re-walk branch_history to find matching index. Only runs when
				// logging is enabled; cost amortized over the rarity of §3 fires.
				const uint64_t target_key = child->_board->_zobrist_key;
				const int hist_size = (int)branch_history->size();
				const int game_hist_size = (int)branch_history->game_history_size();
				int hist_idx = -1;
				for (int i = 0; i < hist_size; ++i) {
					if (branch_history->key_at(i) == target_key) {
						hist_idx = i;
						break;
					}
				}
				if (hist_idx >= 0) {
					const bool is_game_history = (hist_idx < game_hist_size);
					dag_log::bump(is_game_history
						? dag_log::Counter::pred_game_history
						: dag_log::Counter::pred_search_traversal);
					dag_log::bump(dag_log::Counter::pred_total);
					const int depth = hist_size - game_hist_size;
					dag_log::pred_fire(is_game_history, depth, hist_idx,
						hist_size, game_hist_size, this, child, move);
				}
			}

			_iterations++;
			return;
		}
```

Note: the exact `PositionHistory` API for `size()` and key-by-index access depends on Task 1 Step 2 findings. If the actual API is `entry_at(i).key` or `[](size_t)`, substitute accordingly. The `key_at(i)` form is a placeholder for "whatever the verified API is".

If `PositionHistory` does NOT currently expose by-index key access, add it as part of THIS task (additive: `uint64_t key_at(size_t i) const { return _keys[i]; }` or equivalent). No logic change.

- [ ] **Step 3: `pick_random_child` DagExcl skip — emit `dag_excl_skip` + counter.** Locate (Grep `dag_excl->contains\|dag_excl.contains` inside `Node::pick_random_child`). The current site looks like:

```cpp
			if (dag_excl != nullptr && dag_excl->contains(move)) {
				continue; // opt-3 anti-spin
			}
```

(The exact form may differ — Task 1 Step 2 confirmed the file; verify by reading the area.)

Add logging:

```cpp
			if (dag_excl != nullptr && dag_excl->contains(move)) {
				if constexpr (dag_log::enabled) {
					dag_log::bump(dag_log::Counter::dag_excl_skips);
					// `branch_history` may not be in scope inside pick_random_child;
					// depth=0 sentinel is acceptable (event still useful via node FEN).
					dag_log::dag_excl_skip(0, this, move);
				}
				continue; // opt-3 anti-spin
			}
```

If `branch_history` IS in scope at this site (verify), pass `(int)branch_history->size() - (int)branch_history->game_history_size()` instead of 0 for `depth`.

- [ ] **Step 4: `explore_new_move` entry — bump `nodes_via_explore_new`.** Locate `void Node::explore_new_move(` (around exploration.cpp:479 baseline). Add at the very start of the function body, after any early-return guards but before the main work:

```cpp
	if constexpr (dag_log::enabled) {
		dag_log::bump(dag_log::Counter::nodes_via_explore_new);
	}
```

- [ ] **Step 5: `explore_random_child` entry (post §3 cut) — bump `nodes_via_explore_random`.** Locate the same function (around exploration.cpp:726 baseline). After the §3 cut block (the one modified in Step 2) — i.e., after the `}` closing the `if (g_tt_node_dag && position_is_draw_by_repetition(...))` block — add:

```cpp
	if constexpr (dag_log::enabled) {
		dag_log::bump(dag_log::Counter::nodes_via_explore_random);
	}
```

- [ ] **Step 6: `grogros_zero` terminal early-returns — bump `nodes_terminal`.** Locate `void Node::grogros_zero(` (around exploration.cpp:356 baseline) and find its early-return paths (`_is_terminal`, `_got_moves <= 0`, possibly `!_can_explore`). At each early-return site, just before the `return;`, add:

```cpp
		if constexpr (dag_log::enabled) {
			dag_log::bump(dag_log::Counter::nodes_terminal);
		}
```

Specifically the `_is_terminal` and `_got_moves <= 0` returns. The recursion guard and `iterations <= 0` returns are NOT terminal in the chess sense — leave them unmodified.

- [ ] **Step 7: Build.** Expected `EXITCODE=0`, no `error` lines.

- [ ] **Step 8: Commit.**

```bash
git add opti_chess/exploration.cpp opti_chess/exploration.h
git commit -m "feat(log): instrument exploration.cpp with DAG metrics events + counters (#11, inert)"
```

(Include `exploration.h` if Step 2 added `key_at` accessor to `PositionHistory`.)

OFF-byte-identical argument: every new call is wrapped in `if constexpr (dag_log::enabled)` which elides at compile time when toggle is false. The new `key_at` accessor (if added) is unused except by the logging block. With `enabled = true`, the new code runs only on the §3 cut path (rare) and on each entry/exit of explore_new/explore_random/grogros_zero (very cheap counter bump, single integer increment).

---

## Task 5: `gui.cpp` hooks + `tests.cpp` repros + key bindings

Wire the root grogros_zero call to `session_start`/`batch_start`/`batch_end`; add `run_dag_repro_*` entry points in `tests.cpp`; bind keys '1' and '2' in `gui.cpp` for one-key repro invocation.

**Files:**
- Modify: `opti_chess/gui.cpp`
- Modify: `opti_chess/tests.cpp`
- Modify: `opti_chess/tests.h` (if needed to expose `run_dag_repro_*` to `gui.cpp`)

- [ ] **Step 1: Add `#include "dag_log.h"` and `#include "tests.h"` at the top of `gui.cpp`** (if not already present).

- [ ] **Step 2: Wrap the root `grogros_zero` call with session/batch hooks.** Using the call-site location verified in Task 1 Step 3, modify the per-batch invocation. Pattern (adapt to actual variable names — `_alpha`, `_beta`, `_iterations_per_batch`, `_root_exploration_node`, etc.):

```cpp
	// existing per-batch code, e.g.:
	// _root_exploration_node->grogros_zero(... _iterations_per_batch ...);
```

Replace with:

```cpp
	// #11 DAG metrics logging hooks (cf. dag_log.h). Wrapping the root call
	// only ; OFF byte-identique car if constexpr (dag_log::enabled==false)
	// élimine tout à la compilation.
	if constexpr (dag_log::enabled) {
		static std::string s_last_fen;
		std::string cur_fen = _board->to_fen();   // adjust to actual GUI member
		if (cur_fen != s_last_fen) {
			dag_log::session_start(cur_fen.c_str(), g_tt_node_dag, g_tt_main_search,
				_iterations_per_batch, nullptr);
			s_last_fen = cur_fen;
			s_batch_seq = 0;
		}
		dag_log::batch_start(s_batch_seq,
			_root_exploration_node ? _root_exploration_node->_parent_count : 0,
			_board->_got_moves, _iterations_per_batch);
	}

	_root_exploration_node->grogros_zero(/* existing args */);

	if constexpr (dag_log::enabled) {
		dag_log::batch_end(s_batch_seq,
			_iterations_per_batch,
			_root_exploration_node ? _root_exploration_node->_deep_evaluation._value : 0,
			_root_exploration_node ? _root_exploration_node->_deep_evaluation._avg_score : 0.0f);
		s_batch_seq++;
	}
```

`s_batch_seq` is a static int local to the enclosing function (or a file-scope static if the call sits in a free function). If multiple call sites exist, repeat the pattern, but use ONE shared `s_batch_seq` so the sequence numbers are coherent across call types.

If the GUI surfaces a "position changed" event (e.g., on FEN paste or move-played), prefer hooking `session_start` there rather than via FEN comparison every batch — Task 1 Step 4 should have surfaced this; if not, the FEN-comparison fallback above is fine.

- [ ] **Step 3: Add `run_dag_repro` + wrappers in `tests.cpp`.** Append to `opti_chess/tests.cpp`:

```cpp
#include "dag_log.h"
#include "exploration.h"
#include "buffer.h"
#include "evaluation.h"
// (any other includes the existing tests use to set up grogros_zero — match
// the pattern of an existing test entry-point per Task 1 Step 7.)

// External engine state toggles (declared in exploration.cpp).
extern bool g_tt_node_dag;
extern bool g_tt_main_search;

// #11 DAG metrics repro entry — runs `n_batches` batches of `iters_per_batch`
// grogros_zero iterations on the given FEN with DAG ON, emitting structured
// metrics to opti_chess/dag_metrics.log. Self-contained ; called from GUI key
// bindings (cf. gui.cpp KEY_ONE / KEY_TWO).
void run_dag_repro(const char* repro_name, const char* fen,
	int n_batches, int iters_per_batch) {

	const bool saved_dag = g_tt_node_dag;
	const bool saved_pa  = g_tt_main_search;
	g_tt_node_dag = true;
	g_tt_main_search = false;

	// Setup: match the pattern of an existing test entry-point. Concretely
	// (adapt to actual API verified in Task 1 Step 7):
	BoardBuffer board_buf;
	NodeBuffer  node_buf;
	Evaluator   evaluator;

	Board* root_board = board_buf.alloc();
	root_board->from_fen(fen);

	Node* root = node_buf.alloc();
	root->_board = root_board;
	root->_parent_count = 1;

	PositionHistory hist;
	hist.init_from_board(*root_board);            // adjust to actual init API
	hist._game_history_size = hist.size();        // mark game-history prefix

	dag_log::session_start(fen, g_tt_node_dag, g_tt_main_search,
		iters_per_batch, repro_name);

	int final_eval = 0;
	int final_pc = 0;

	for (int b = 0; b < n_batches; ++b) {
		dag_log::batch_start(b, root->_parent_count,
			root_board->_got_moves, iters_per_batch);

		// Call grogros_zero with the project's standard arg conventions ;
		// verify exact arg order in Task 1 Step 3. Trailing path_history is
		// passed as &hist so the logging sees a populated game-history prefix.
		root->grogros_zero(&board_buf, &evaluator,
			/* alpha */ 1.0, /* beta */ 1.0, /* gamma */ 1.0,
			iters_per_batch, /* quiescence_depth */ 0,
			/* network */ nullptr, /* path_history */ &hist);

		final_eval = root->_deep_evaluation._value;
		final_pc = root->_parent_count;

		dag_log::batch_end(b, iters_per_batch,
			root->_deep_evaluation._value,
			root->_deep_evaluation._avg_score);
	}

	dag_log::session_end(n_batches, final_eval, final_pc);

	// Cleanup: reset/return buffers per existing convention.
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

The exact arg list of `grogros_zero` and the buffer-allocation pattern depend on the project's existing conventions (Task 1 Steps 3 and 7). The block above is the structural skeleton; the implementer reads an existing test and uses its setup pattern verbatim.

- [ ] **Step 4: Declare the entry points in `tests.h`** (if other TUs need to see them; if `gui.cpp` includes `tests.h` and the rest of the project does too, add forward decls):

```cpp
// #11 DAG metrics logging — manual repro entry points (cf. tests.cpp).
void run_dag_repro_1();
void run_dag_repro_2();
```

If `tests.h` doesn't exist or isn't used, declare them locally in `gui.cpp` (e.g., `extern "C"` not needed since same TU language; `extern void run_dag_repro_1(); extern void run_dag_repro_2();` near the includes).

- [ ] **Step 5: Bind keys '1' and '2' in the GUI dispatch.** Using the location verified in Task 1 Step 6, add to the key-handling block:

```cpp
	if (IsKeyPressed(KEY_ONE)) {
		run_dag_repro_1();
	}
	if (IsKeyPressed(KEY_TWO)) {
		run_dag_repro_2();
	}
```

(If '1' and '2' are already bound to something else per Task 1 Step 6, use the next-free pair documented there and adjust this snippet's key names accordingly.)

- [ ] **Step 6: Build.** Expected `EXITCODE=0`, no `error` lines.

  Sub-step on linker complaint: if MSBuild reports the new `tests.cpp` functions as unresolved (because the project file doesn't auto-discover added functions in an existing TU — unlikely but possible), the implementer reports `BLOCKED` and asks user to refresh the VS project.

- [ ] **Step 7: Commit.**

```bash
git add opti_chess/gui.cpp opti_chess/tests.cpp opti_chess/tests.h
git commit -m "feat(log): gui+tests instrumentation - session/batch hooks + repro entry points + keys 1/2 (#11)"
```

(Omit `tests.h` from the git add if Step 4 didn't modify it.)

OFF-byte-identical argument: all new gui.cpp code is wrapped in `if constexpr (dag_log::enabled)`. With `enabled = false`, the wrapper compiles to nothing → root grogros_zero call is invoked identically to baseline. The key bindings are new behavior but only fire on user input; they don't run automatically. When `run_dag_repro_*` runs, it temporarily flips `g_tt_node_dag` to true and restores it — but ONLY when the user presses the key.

---

## Task 6: Final build verification + push

Verify the cumulative change builds clean, push the branch.

- [ ] **Step 1: Build with `dag_log::enabled = true`.** Run the build command. Expected `EXITCODE=0`, no `error` lines.

- [ ] **Step 2: Temporary toggle off check.** Edit `opti_chess/dag_log.h`, change `constexpr bool enabled = true;` to `constexpr bool enabled = false;`. Run the build command. Expected `EXITCODE=0`, no `error` lines. (This verifies the OFF-path also compiles cleanly — all the `if constexpr` wrappers must be syntactically valid in both states.) Revert the toggle to `true`. Do NOT commit the off-toggle change.

- [ ] **Step 3: Verify the toggle is back to `true`.**

```bash
grep -n "constexpr bool enabled" opti_chess/dag_log.h
```

Expected output: `constexpr bool enabled = true;` (the default we ship with).

- [ ] **Step 4: Push the branch.**

```bash
git push origin feature/tt-main-search
```

(If push is configured for a different remote/branch, adjust. The user has been working on `feature/tt-main-search` throughout the #11 chain.)

- [ ] **Step 5: Final summary message to user.**

Report:
- Total commits added this session for the logging plan.
- Branch tip SHA.
- Instructions for user: "open Position 1 or Position 2 in the GUI (or just launch the engine and press '1' for Repro 1 / '2' for Repro 2). The log will accumulate at `opti_chess/dag_metrics.log`. Once you have a few hundred lines of data on each repro, either commit it with `git add -f opti_chess/dag_metrics.log && git commit && git push`, or paste a representative slice into a new chat session — and I'll do the analysis."

No further code commits in this plan. The implementation is COMPLETE at this point.

---

## Out of scope (handled in a separate future plan)

- Reading the log file and doing the analysis. This happens AFTER the user runs the repros and shares the log.
- Designing the actual #11 fix. The fix design is a separate brainstorm informed by the analysis. The terminal state of THIS plan is "logging shipped and pushed".
- Removing the temporary `dag_log::enabled = true` default after analysis is done. Leaving it `true` is fine while the bug is open; can be set to `false` for perf-measurement runs by editing one line.

---

## Self-Review

**Spec coverage** (`docs/superpowers/specs/2026-05-20-optichess-dag-metrics-logging-design.md`):

- §2 scope (new TU pair, instrumentation, .gitignore, repro entry) → Tasks 2, 3, 4, 5. ✓
- §3 compile-time toggle → Task 2 Step 1 defines `constexpr bool enabled`; every API entry checks it; Task 4/5 wrap call sites in `if constexpr`. ✓
- §4 file output (JSON-lines, append, gitignored, lazy open) → Task 2 Step 2 + Step 3. ✓
- §5 event schema (session/batch/pred_fire/dag_excl_skip + counters) → Task 2 Step 2 emits matching JSON; Task 4 calls match the signatures. ✓
- §6 API → Task 2 Step 1. ✓
- §7 instrumentation points (5 in exploration.cpp + 1 in gui.cpp) → Task 4 (5 sites) + Task 5 (1 site, hooks around root call). ✓
- §8 game_history_size split → Task 3 (field + accessor + FEN-load init) + Task 4 Step 2 (uses the accessor at §3 cut to label `kind`). ✓
- §9 repro convenience entry point → Task 5 Step 3 (`run_dag_repro` + `_1` + `_2`). ✓
- §10 OFF byte-identicality → per-task OFF arguments + Task 6 Step 2 explicit OFF-toggle build check. ✓
- §11 performance (zero alloc per event via `snprintf`, file I/O only at batch boundaries, capped detail events) → Task 2 Step 2 implementation matches. ✓
- §12 edge cases (multi-session, crash mid-batch, very long batches, DAG OFF runs, empty game-history) → handled by the implementation in Task 2 Step 2. ✓
- §14 acceptance criteria (build both toggles, session_start/batch/session_end present in log, valid JSON-lines) → Task 6 Steps 1-3 cover the build half; the runtime half is the USER step. ✓
- §15 task breakdown → matches this plan. ✓

No spec requirement is left without a task.

**Placeholder scan:** No `TBD`/`TODO`/`fill in later` markers in any task step. Each instrumentation site has its exact code shown. The `<actual PositionHistory by-index API>` is acknowledged as Task-1-verified and tasks adapt accordingly; that's not a placeholder, it's a verification gate.

**Type consistency:** `dag_log::Counter` enum values (`pred_total`, `pred_search_traversal`, `pred_game_history`, `dag_excl_adds`, `dag_excl_skips`, `nodes_terminal`, `nodes_via_explore_new`, `nodes_via_explore_random`) match between Task 2 Step 1 (decl) and Task 2 Step 2 (counter array indexing in `batch_end`) and Task 4 Steps 2-6 (call sites). `dag_log::pred_fire` signature `(bool is_game_history, int depth, int hist_idx, int hist_size, int game_hist_size, const Node*, const Node*, const Move&)` matches between Task 2 Step 1 and Task 2 Step 2 (def) and Task 4 Step 2 (call). `dag_log::dag_excl_skip(int depth, const Node*, const Move&)` matches the same way. `PositionHistory::game_history_size()` accessor matches between Task 3 Step 1 (decl) and Task 4 Step 2 (read).

---

**Execution note:** No automated test runner; per task the "test" is the clean MSBuild + the written OFF-byte-identical argument. The single end-to-end runtime validation is the **USER** step after Task 6 (`run_dag_repro_1()` + `run_dag_repro_2()` invocation via keys '1'/'2' in the GUI). The assistant cannot run the raylib GUI. Once the user shares the resulting `opti_chess/dag_metrics.log`, the analysis phase begins in a separate session.
