# #11 DAG GHI-correct repetition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `g_tt_node_dag == true` correct on repetition in all cases while keeping full node-sharing (depth), by treating a repetition as a draw-valued leaf that competes in minimax per-path at the consumer, with a GHI-correct caching guard — replacing the entire pile of prior #11 patches.

**Architecture:** Path history is de-aliased and carries `first_ply`; an integer `ply` is threaded through the recursion. At every consumer (selection scoring AND backup), a child already on the current path contributes `dag_draw_eval()` (0); ordinary minimax decides. A node persists its value to its shared `_deep_evaluation` only when every repetition it used lies within its own subtree (`min_earlier_ply >= ply`); otherwise the value is path-local for that traversal only. This is tree-mode minimax with shared caches.

**Tech Stack:** C++17, MSVC `opti_chess.vcxproj` (toolset override `/p:PlatformToolset=v143`), tsl::robin_map, raylib (linked, no window in the harness).

**Spec:** `docs/superpowers/specs/2026-05-24-optichess-dag-ghi-repetition-design.md`.
**Branch:** `feature/tt-main-search`. **Baseline tip before this plan:** `de93f07`.

---

## Build & run reference (used by every task)

```powershell
# Build Release x64 (project targets v145 which is NOT installed -> force v143):
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "C:\Users\poiro\Documents\Info\Chess\C++\opti_chess\opti_chess.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /m /nologo /v:minimal
# Success = a line ending "-> ...Grogros_Chess.exe" and NO line containing ": error". Warnings (C4244/C4324/...) are pre-existing.

# Run harness (Release exe is Grogros_Chess.exe). Optional [batches] [iters] override both repros:
& "C:\Users\poiro\Documents\Info\Chess\C++\opti_chess\x64\Release\Grogros_Chess.exe" --dag-test
& "C:\Users\poiro\Documents\Info\Chess\C++\opti_chess\x64\Release\Grogros_Chess.exe" --dag-test 120 4000
```

---

## File structure

| File | Responsibility | Change |
|------|----------------|--------|
| `opti_chess/exploration.h` | `PathEntry` + de-aliased `PositionHistory`; remove `DagExcl`; new signatures (`ply`, `BackupResult`, path-aware selectors) | Modify |
| `opti_chess/exploration.cpp` | path helpers (`ply`/`first_ply`), the consumer substitution, the backup + cache guard (in BOTH `explore_random_child` AND `explore_new_move`), `out`-channel seed/forward, removals, threading | Modify (primary) |

> **Line-ref convention:** header signatures live at `exploration.h:150` (`grogros_zero`), `:153` (`explore_new_move`), `:156` (`explore_random_child`), `:196` (`pick_random_child`), `:199` (`get_move_scores`), `:205` (`get_best_score_move`); `DagExcl` is `:20-37`. Treat all line numbers as anchors that may have drifted — match on the quoted code, not the number.
| `opti_chess/main_headless.h` | harness: new test positions, sharing-active metric, thresholds | Modify |
| `opti_chess/BUGFIXES.md`, `opti_chess/ALGORITHMS.md` | doc refresh | Modify |

---

## Task 1: Harness — new positions, sharing-active metric, threshold pinning

**Files:** Modify `opti_chess/main_headless.h`.

Establishes the bar BEFORE the mechanism: the new "all cases" positions, and a metric that catches attempt-8's depth-death (`node_map` empty ⇒ no sharing). On the current (attempt-8) code this metric is expected to FAIL for the pawn-endgame repros (they're de-shared) — that is the RED proving depth is dead.

- [ ] **Step 1: Add a sharing-active check and two new repro positions**

In `opti_chess/main_headless.h`, the file already has `#include "exploration.h"` (so `node_map` and `g_tt_node_dag` are visible). After the existing four `run_repro` calls in `main_headless`, and before the `check(...)` block, add capture of `node_map` size for the ON pawn-endgame run and two new positions. Replace the existing repro block:

```cpp
    const int b1 = ov_batches ? ov_batches : 10;
    const int i1 = ov_iters   ? ov_iters   : 2000;
    const int b2 = ov_batches ? ov_batches : 20;
    const int i2 = ov_iters   ? ov_iters   : 3000;
    const int r1_off = run_repro(root, board, &eval, "repro1_kp_h_draw", fen1, b1, i1, false);
    const int r1_on  = run_repro(root, board, &eval, "repro1_kp_h_draw", fen1, b1, i1, true);
    const int r2_off = run_repro(root, board, &eval, "repro2_pawn_win",  fen2, b2, i2, false);
    const int r2_on  = run_repro(root, board, &eval, "repro2_pawn_win",  fen2, b2, i2, true);
```

with:

```cpp
    const int b1 = ov_batches ? ov_batches : 10;
    const int i1 = ov_iters   ? ov_iters   : 2000;
    const int b2 = ov_batches ? ov_batches : 20;
    const int i2 = ov_iters   ? ov_iters   : 3000;
    // Perpetual-check / repetition draw in a non-pawn-ending (general GHI case).
    const std::string fen3 = "7k/5Q2/8/8/8/8/5q2/7K w - - 0 1";
    // Known fortress draw (opposite-colored bishops, extra pawn cannot break through).
    const std::string fen4 = "8/8/8/4k3/8/3b4/3KP3/8 w - - 0 1";

    const int r1_off = run_repro(root, board, &eval, "repro1_kp_h_draw", fen1, b1, i1, false);
    const int r1_on  = run_repro(root, board, &eval, "repro1_kp_h_draw", fen1, b1, i1, true);
    const int r2_off = run_repro(root, board, &eval, "repro2_pawn_win",  fen2, b2, i2, false);
    const int r2_on  = run_repro(root, board, &eval, "repro2_pawn_win",  fen2, b2, i2, true);
    // node_map size right after the ON pawn-endgame run = sharing-active proxy.
    const size_t r2_on_nodemap = node_map.size();
    const int r3_on  = run_repro(root, board, &eval, "repro3_perpetual", fen3, b2, i2, true);
    const int r4_on  = run_repro(root, board, &eval, "repro4_fortress",  fen4, b2, i2, true);
```

- [ ] **Step 2: Add the new assertions**

Replace the existing `check(...)` block:

```cpp
    check(std::abs(r1_off) <= EPS_DRAW,                              "Repro1 OFF ~ draw (sanity)");
    check(std::abs(r1_on)  <= EPS_DRAW,                              "Repro1 ON  ~ draw (THE FIX)");
    check(r2_off >= WIN_THRESH,                                      "Repro2 OFF winning (sanity)");
    check(r2_on  >= WIN_THRESH && std::abs(r2_on) > EPS_DRAW,        "Repro2 ON  winning (no phantom draw)");
```

with:

```cpp
    check(std::abs(r1_off) <= EPS_DRAW,                              "Repro1 OFF ~ draw (sanity)");
    check(std::abs(r1_on)  <= EPS_DRAW,                              "Repro1 ON  ~ draw (THE FIX)");
    check(r2_off >= WIN_THRESH,                                      "Repro2 OFF winning (sanity)");
    check(r2_on  >= WIN_THRESH && std::abs(r2_on) > EPS_DRAW,        "Repro2 ON  winning (no phantom draw)");
    check(std::abs(r3_on) <= EPS_DRAW,                               "Repro3 ON  perpetual ~ draw");
    check(std::abs(r4_on) <= EPS_DRAW,                               "Repro4 ON  fortress ~ draw");
    std::printf("[DAG-TEST] sharing-active: node_map=%zu after repro2 ON\n", r2_on_nodemap);
    check(r2_on_nodemap > 100,                                       "Sharing active under DAG (depth not dead)");
```

- [ ] **Step 3: Build**

Run the build command. Expected: clean, `-> Grogros_Chess.exe`, no `: error`.

- [ ] **Step 4: Run and pin thresholds + record the RED**

Run `--dag-test` then `--dag-test 120 4000`. From the printed FINAL evals:
- Confirm `repro3_perpetual` OFF-style value is ~0 and `repro4_fortress` ~0 (run them OFF manually too if needed: temporarily flip the `true` to `false` for r3/r4, observe, revert). If either's natural eval is far from 0, replace its FEN with a cleaner one (a position whose OFF eval is within `EPS_DRAW` of 0) — pick from `opti_chess/Tests.txt` via Grep for "nulle"/"fortress"/"perpetual", and re-pin. Document the chosen FENs in the commit message.
- Confirm `EPS_DRAW`/`WIN_THRESH` still separate the values (raise/lower as in the existing comments if needed).
- EXPECTED RED on current attempt-8 code: `Sharing active under DAG` **FAILS** (`node_map` ≈ 0 because pawn endgames are de-shared), and possibly `Repro3/Repro4 ON` fail (no GHI handling yet). `Repro1 ON`/`Repro2 ON` pass (attempt-8). Record the full output.

- [ ] **Step 5: Commit**

```powershell
cd "C:\Users\poiro\Documents\Info\Chess\C++"; git add opti_chess/main_headless.h; git commit -m "test(search): #11 - add perpetual/fortress repros + sharing-active metric"
```

---

## Task 2: Path state — `PathEntry`, de-alias, `ply` threading (behavior-preserving)

**Files:** Modify `opti_chess/exploration.h`, `opti_chess/exploration.cpp`.

Pure plumbing: carry `first_ply` and thread `ply`. `first_ply` is recorded but not yet read, so search behavior is unchanged. After this task the harness output must be IDENTICAL to Task 1's.

- [ ] **Step 1: De-alias `PositionHistory` with `PathEntry` (exploration.h)**

Replace `exploration.h:10` (`using PositionHistory = RepetitionHistory;`) with:

```cpp
// #11 — historique de chemin de RECHERCHE (distinct du RepetitionHistory de
// partie, board.h:16). Porte le compte de visites ET le premier ply où la
// position est apparue sur le chemin courant (first_ply), nécessaire au gardien
// de cache GHI (intrinsèque ssi first_ply >= ply du noeud).
struct PathEntry {
	uint16_t count = 0;
	uint16_t first_ply = 0;
};
using PositionHistory = tsl::robin_map<uint64_t, PathEntry>;
```

- [ ] **Step 2: Update the path helpers + `PathScope` to carry `ply` (exploration.cpp:74-132)**

Replace the helper block (the `namespace { ... position_history_count ... }`, `ensure_position_in_history`, `record_position_in_history`, `unrecord_position_in_history`, `PathScope`, `position_is_draw_by_repetition`) with:

```cpp
uint8_t position_history_count(const PositionHistory& path_history, Board& board) {
	board.get_zobrist_key();
	const auto it = path_history.find(board._zobrist_key);
	return it == path_history.end() ? 0 : (uint8_t)it->second.count;
}

// #11 GHI — ply de la PREMIÈRE occurrence de la position sur le chemin courant
// (UINT16_MAX si absente). Sert à classer une répétition en intrinsèque
// (first_ply >= ply du noeud) vs extrinsèque (first_ply < ply).
uint16_t position_history_first_ply(const PositionHistory& path_history, Board& board) {
	board.get_zobrist_key();
	const auto it = path_history.find(board._zobrist_key);
	return it == path_history.end() ? UINT16_MAX : it->second.first_ply;
}

} // namespace

void ensure_position_in_history(PositionHistory& path_history, Board& board, uint16_t ply) {
	board.get_zobrist_key();
	path_history.try_emplace(board._zobrist_key, PathEntry{1, ply});
}

void record_position_in_history(PositionHistory& path_history, Board& board, uint16_t ply) {
	board.get_zobrist_key();
	auto it = path_history.find(board._zobrist_key);
	if (it == path_history.end())
		path_history.emplace(board._zobrist_key, PathEntry{1, ply});
	else
		it.value().count++;
}

void unrecord_position_in_history(PositionHistory& path_history, uint64_t key) {
	const auto it = path_history.find(key);
	if (it == path_history.end()) {
		return; // appel non équilibré : ne devrait jamais arriver
	}
	if (it->second.count <= 1) {
		path_history.erase(it);
	}
	else {
		it.value().count--;
	}
}

struct PathScope {
	PositionHistory& _history;
	uint64_t _key;
	PathScope(PositionHistory& history, Board& board, uint16_t ply) : _history(history) {
		board.get_zobrist_key();
		_key = board._zobrist_key;
		auto it = _history.find(_key);
		if (it == _history.end())
			_history.emplace(_key, PathEntry{1, ply});
		else
			it.value().count++;
	}
	~PathScope() {
		unrecord_position_in_history(_history, _key);
	}
	PathScope(const PathScope&) = delete;
	PathScope& operator=(const PathScope&) = delete;
};

bool position_is_draw_by_repetition(const PositionHistory& path_history, Board& board, uint8_t repetition_limit = search_repetition_limit) {
	return position_history_count(path_history, board) + 1 >= repetition_limit;
}
```

Note: `position_history_first_ply` is declared inside the anonymous namespace (with `position_history_count`); `record`/`ensure`/`unrecord`/`PathScope` stay at file scope (they're called from other TUs / declared in headers — keep their existing linkage). Verify the closing `}` of the anonymous namespace lands right after `position_history_first_ply`.

- [ ] **Step 3: Update declarations of the changed helpers (exploration.h)**

If `ensure_position_in_history` / `record_position_in_history` are declared in `exploration.h`, add the `uint16_t ply` parameter to those declarations. Grep `record_position_in_history|ensure_position_in_history` across `opti_chess/` and update EVERY call site to pass a ply (root callers pass `0`).

- [ ] **Step 4: Update the dag_log count read (exploration.cpp ~828)**

In the `if constexpr (dag_log::enabled)` block inside `explore_random_child`, the line `const int current_count = (it == branch_history.end()) ? 0 : (int)it->second;` must become `... : (int)it->second.count;` (the value is now `PathEntry`). Grep for `->second` and `.value()` across `exploration.cpp` and fix any that assumed the old `uint8_t` value (e.g. arithmetic on `it->second`).

- [ ] **Step 5: Thread `int ply` through the recursion (exploration.cpp / .h)**

- `grogros_zero`: add trailing param `int ply = 0` (exploration.h:150 + exploration.cpp:357). At the root-history insert (exploration.cpp:394) pass `ply`: `ensure_position_in_history(*base_path_history, *_board, (uint16_t)ply);`.
- `explore_new_move` (exploration.h:153, exploration.cpp:486) and `explore_random_child` (exploration.h:156, exploration.cpp:748): add `int ply` param. In `grogros_zero` pass `ply` to both calls (exploration.cpp:452, 457).
- In `explore_random_child`, the descent `PathScope _ps(branch_history, *child->_board)` (exploration.cpp:851) becomes `PathScope _ps(branch_history, *child->_board, (uint16_t)(ply + 1))`, and the recursive `child->grogros_zero(...)` gains a trailing `ply + 1`.
- In `explore_new_move`, the `PathScope _ps(branch_history, *child->_board)` (exploration.cpp:641) becomes `PathScope _ps(branch_history, *child->_board, (uint16_t)(ply + 1))`; the §3 draw leaf (`init_terminal_draw_child`) needs no ply.

- [ ] **Step 6: Build**

Run the build command. Expected clean. Fix any `it->second` arithmetic the compiler flags (now a struct).

- [ ] **Step 7: Run harness — must be IDENTICAL to Task 1**

Run `--dag-test` and `--dag-test 120 4000`. Expected: same evals and same PASS/FAIL pattern as end of Task 1 (this task is behavior-preserving; `first_ply` is recorded but unused). If anything changed, the threading broke balance — debug before proceeding.

- [ ] **Step 8: Commit**

```powershell
cd "C:\Users\poiro\Documents\Info\Chess\C++"; git add opti_chess/exploration.h opti_chess/exploration.cpp; git commit -m "refactor(search): #11 - de-alias PositionHistory with first_ply, thread ply (inert)"
```

---

## Task 3: The mechanism — consumer draw value + cache guard; remove the patch pile

**Files:** Modify `opti_chess/exploration.h`, `opti_chess/exploration.cpp`.

This is one atomic change (the mechanism only works whole). The edits below do not individually compile; build + test at the end of the task.

- [ ] **Step 1: Add `BackupResult` and a cached draw eval; remove `DagExcl` (exploration.h)**

Delete the `DagExcl` struct (exploration.h:20-37, the comment block + struct). Add, after the `ChildLink` struct:

```cpp
// #11 GHI — valeur remontée d'un appel grogros_zero/explore_random_child à son
// parent : la valeur PATH-LOCALE de ce sous-arbre sur la traversée courante, et
// min_earlier_ply = le plus petit ply de première-occurrence d'une répétition
// utilisée dans la valeur remontée (INT32_MAX si aucune). Le parent persiste sa
// valeur dans _deep_evaluation seulement si min_earlier_ply >= son propre ply
// (répétitions toutes intra-sous-arbre => intrinsèque => path-indépendant).
struct BackupResult {
	Evaluation value;
	int min_earlier_ply = INT32_MAX;
};
```

- [ ] **Step 2: Add a cached draw-eval pointer helper (exploration.cpp, near `dag_draw_eval` ~155)**

After `dag_draw_eval()`'s definition add:

```cpp
// #11 GHI — pointeur stable vers une Evaluation de nulle canonique (réutilisée
// comme custom_eval dans le scoring path-aware). Position-indépendante.
static const Evaluation& dag_draw_eval_ref() {
	static const Evaluation s_draw = dag_draw_eval();
	return s_draw;
}
```

- [ ] **Step 3: Make selection path-aware — `get_move_scores`, `get_best_score_move`, `pick_random_child` (exploration.cpp)**

Add `const PositionHistory* path, int ply` (defaults `nullptr, 0`) to the signatures of `get_move_scores` (exploration.cpp:1886 + its decl in exploration.h), `get_best_score_move` (exploration.cpp:1995 + decl), and `pick_random_child` (exploration.cpp:1692 + decl). Remove the `const DagExcl* dag_excl` param from `pick_random_child` (and its decl). In each of these three functions, define a per-child substituted-eval accessor and use it in BOTH the max pre-pass and the per-child scoring. Concretely, in each `for (... child_link ...)` loop replace reads of `child->_deep_evaluation` used for ranking with:

```cpp
const Evaluation& ceval =
	(path != nullptr && g_tt_node_dag && position_is_draw_by_repetition(*path, *child->_board))
		? dag_draw_eval_ref()
		: child->_deep_evaluation;
```

and use `ceval._value` / `ceval._avg_score` in the max computation, and pass `const_cast<Evaluation*>(&ceval)` as the `custom_eval` argument to `child->get_node_score(...)` — `get_node_score` (cpp:1943) only *reads* `*custom_eval` (cpp:1955), so the `const_cast` is read-only and safe. (`get_node_score` already supports `custom_eval`.) Inside `pick_random_child`, also delete the `dag_excl`-skip logic (the block that skips `dag_excl->contains(...)` moves) — on-path children are now naturally down-ranked as draws. `pick_random_child` calls `get_move_scores(alpha, beta)` at line 1726 → change to `get_move_scores(alpha, beta, false, -100, path, ply)` (defaults confirmed `consider_standpat=false, qdepth=-100` at exploration.h:199).

**Also make `explore_new_move`'s own backup path-aware:** its `get_best_score_move` call at exploration.cpp:718 must pass `path`/`ply` too (handled in full by Step 4b below). Selection (here) and BOTH backups (`explore_random_child` Step 4, `explore_new_move` Step 4b) must use the SAME substitution, or a repeating sibling is draw-scored in selection but raw-scored in the expand backup → win erodes (the exact failure mode of prior attempts).

- [ ] **Step 4: Rewrite `explore_random_child` — don't-descend / descent / backup / cache guard / channel (exploration.cpp:748-912)**

Change the signature (exploration.cpp:748 + exploration.h:156): drop `DagExcl* dag_excl` and `Evaluation* path_local_eval`, add `int ply` and `BackupResult* out`:

```cpp
void Node::explore_random_child(BoardBuffer* board_buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network, PositionHistory *path_history, int ply, BackupResult* out) {
```

Replace the body from the pick through the end of the function (current cpp:749-911), i.e. everything that today is: `pick_random_child` (751) + null-move handling (757-760) + the §3-cut block (790-844) + the descent (850-853) + the opt-1/DagExcl backup (856-875) + the model-A accounting (887-911). **Note:** that span includes the `g_tt_main_search` TT write-back at cpp:877-885 — the replacement below **re-includes it** (do NOT drop it; it is the headline feature of this branch and is independent of `g_tt_node_dag`). Replace with:

```cpp
	PositionHistory& branch_history = *path_history;

	// Sélection path-aware : un fils déjà sur le chemin est scoré comme nulle,
	// donc rarement choisi sauf si toutes les lignes valent <= nulle.
	const Move move = pick_random_child(alpha, beta, gamma, g_tt_node_dag ? &branch_history : nullptr, ply);
	if (move.is_null_move()) {
		_iterations++;
		return;
	}

	ChildLink& child_link = _children[move];
	Node* child = child_link._node;
	const int initial_child_nodes = child_link._propagated_nodes;

	// #11 GHI — si le fils CHOISI est une répétition sur CE chemin, on ne descend
	// PAS (sinon boucle infinie). Sa valeur path-locale pour cette traversée est
	// une nulle ; on note le ply de sa première occurrence pour le gardien de cache.
	BackupResult child_res;
	bool descended = false;
	if (g_tt_node_dag && position_is_draw_by_repetition(branch_history, *child->_board)) {
		child_res.value = dag_draw_eval();
		child_res.min_earlier_ply = (int)position_history_first_ply(branch_history, *child->_board);
		g_dag_recheck_hits++;
	}
	else {
		PathScope _ps(branch_history, *child->_board, (uint16_t)(ply + 1));
		child->grogros_zero(board_buffer, eval, alpha, beta, gamma, 1, quiescence_depth, network, &branch_history, g_tt_node_dag ? &child_res : nullptr, ply + 1);
		descended = true;
		if (!g_tt_node_dag) child_res.value = child->_deep_evaluation; // chemin arbre : valeur normale
	}

	// Backup path-aware : minimax sur les fils, fils-sur-chemin comptés nulle.
	const Move _bm = get_best_score_move(alpha, beta, false, -100, g_tt_node_dag ? &branch_history : nullptr, ply);

	int this_min_earlier_ply = INT32_MAX;
	Evaluation this_value;
	if (g_tt_node_dag && _bm.is_null_move()) {
		// aucun fils rankable (tous nuls/standpat) : conserver l'éval courante.
		this_value = _deep_evaluation;
	}
	else if (g_tt_node_dag && position_is_draw_by_repetition(branch_history, *_children[_bm]._node->_board)) {
		// le meilleur coup est une répétition sur ce chemin -> contribution = nulle.
		this_value = dag_draw_eval();
		this_min_earlier_ply = (int)position_history_first_ply(branch_history, *_children[_bm]._node->_board);
	}
	else if (g_tt_node_dag && _bm == move && descended) {
		// le meilleur coup est le fils qu'on vient de descendre : valeur path-locale.
		this_value = child_res.value;
		this_min_earlier_ply = child_res.min_earlier_ply;
	}
	else {
		// fils non-descendu ou mode arbre : sa valeur cachée (intrinsèque par construction).
		this_value = _children[_bm]._node->_deep_evaluation;
	}

	// #11 GHI — gardien de cache. ON : persiste seulement si toutes les
	// répétitions utilisées sont intra-sous-arbre (min_earlier_ply >= ply) ;
	// sinon valeur path-locale, on ne corrompt pas le noeud partagé. OFF :
	// comportement arbre inchangé (persiste toujours la valeur du meilleur fils).
	if (!g_tt_node_dag) {
		_deep_evaluation = _children[_bm]._node->_deep_evaluation;
	}
	else if (this_min_earlier_ply >= ply) {
		_deep_evaluation = this_value;
	}
	// sinon : extrinsèque -> on NE touche PAS _deep_evaluation (cache intrinsèque conservé).

	// #11 Plan A — write-back TT main-search PRÉSERVÉ (était exploration.cpp:877-885,
	// supprimé par erreur dans la 1re rédaction de ce plan). Sous DAG : ne persiste que
	// l'intrinsèque (même garde que _deep_evaluation), jamais une nulle path-locale dans
	// une TT indexée par position. OFF (g_tt_node_dag faux) : condition réduite à
	// l'originale -> byte-identique.
	if (g_tt_main_search && !_is_terminal && !_is_stand_pat_eval && _deep_evaluation._evaluated
		&& (!g_tt_node_dag || this_min_earlier_ply >= ply)) {
		transposition_table.store(_board->_zobrist_key,
			tt_normalize_mate(_deep_evaluation._value * _board->get_color(), _board->_moves_count), // #3
			tt_writeback_depth(_nodes), TT_EXACT);
	}

	if (out != nullptr) {
		out->value = this_value;
		out->min_earlier_ply = this_min_earlier_ply;
	}

	// Comptabilité des noeuds. Bug 2 model A sous DAG (delta par-arête borné >=0) ;
	// branche arbre inchangée (byte-identique OFF).
	if (g_tt_node_dag) {
		_nodes += 1;
		child_link._propagated_nodes += 1;
	}
	else {
		_nodes += child->_nodes - initial_child_nodes;
		child_link._propagated_nodes = child->_nodes;
	}
	if (!g_tt_node_dag && _nodes <= 0) {
		cout << "negative nodes in explore_random_child???" << endl;
	}
	if (!g_tt_node_dag && child->_nodes <= 0) {
		cout << "negative nodes in explore_random_child child???" << endl;
	}

	_iterations++;
}
```

Notes for the implementer: keep any `dag_log` `pred_fire`/counter calls you want for diagnostics, adapted to the new flow (optional). The `_bm == move` comparison uses `Move::operator==` (board.h:139). `get_best_score_move`'s defaults are `consider_standpat=false, qdepth=-100` (exploration.h:205) — pass them positionally before `path, ply`. The `_bm == move && descended` case relies on `child_res` being reliably filled by the descent; that is guaranteed by the `out`-channel seed in `grogros_zero` (Step 5) — without it, an expand-path descent leaves `child_res.value` uninitialized (`Evaluation::_value` has no default, board.h:466).

- [ ] **Step 4b: Make `explore_new_move` path-aware + cache-guarded + fill the channel (exploration.cpp:486)**

The expand path must mirror `explore_random_child`'s backup, or (a) the `out` channel is never filled when `grogros_zero` descends through expansion (the common case for young nodes → garbage `BackupResult`), and (b) a repeating sibling is draw-scored in selection but raw-scored here (win erodes). Its current `Evaluation* path_local_eval` param is dead (never written) — repurpose it as the channel.

Change the signature (exploration.cpp:486 + exploration.h:153): replace trailing `Evaluation* path_local_eval` with `int ply, BackupResult* out` (order `(path_history, ply, out)` to match `explore_random_child`):

```cpp
void Node::explore_new_move(BoardBuffer* board_buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network, PositionHistory *path_history, int ply, BackupResult* out) {
```

The `PathScope` at cpp:641 already carries `ply + 1` (Task 2). Replace the backup block (cpp:718-739, from `Move best_move = get_best_score_move(...)` through the closing `}` of the `else` that holds the Plan A store) with:

```cpp
	bool all_moves_explored = (get_fully_explored_children_count()) == _board->_got_moves;

	// Backup path-aware (cohérent avec explore_random_child) : un fils sur le
	// chemin contribue dag_draw_eval(), sinon son _deep_evaluation.
	Move best_move = get_best_score_move(alpha, beta, !all_moves_explored, -100, g_tt_node_dag ? path_history : nullptr, ply);

	if (best_move.is_null_move()) {
		_is_stand_pat_eval = true;
		if (out != nullptr) { out->value = _deep_evaluation; out->min_earlier_ply = INT32_MAX; }
	}
	else {
		_is_stand_pat_eval = false;

		int this_min_earlier_ply = INT32_MAX;
		Evaluation this_value;
		if (g_tt_node_dag && position_is_draw_by_repetition(*path_history, *_children[best_move]._node->_board)) {
			// le meilleur coup est une répétition sur ce chemin (p.ex. feuille §3
			// init_terminal_draw_child) -> contribution = nulle ; note son first_ply.
			this_value = dag_draw_eval();
			this_min_earlier_ply = (int)position_history_first_ply(*path_history, *_children[best_move]._node->_board);
		}
		else {
			this_value = _children[best_move]._node->_deep_evaluation;
		}

		// #11 GHI — même gardien de cache que explore_random_child (spec §3.3).
		if (!g_tt_node_dag) {
			_deep_evaluation = _children[best_move]._node->_deep_evaluation;
		}
		else if (this_min_earlier_ply >= ply) {
			_deep_evaluation = this_value;
		}
		// sinon extrinsèque -> _deep_evaluation conservé ; valeur path-locale via out.

		if (out != nullptr) { out->value = this_value; out->min_earlier_ply = this_min_earlier_ply; }

		// #11 Plan A — write-back TT main-search préservé, gardé sur l'intrinsèque.
		if (g_tt_main_search && !_is_terminal && !_is_stand_pat_eval && _deep_evaluation._evaluated
			&& (!g_tt_node_dag || this_min_earlier_ply >= ply)) {
			transposition_table.store(_board->_zobrist_key,
				tt_normalize_mate(_deep_evaluation._value * _board->get_color(), _board->_moves_count), // #3
				tt_writeback_depth(_nodes), TT_EXACT);
		}
	}
```

(The `all_moves_explored` computation already exists at cpp:710 — fold it in or keep the existing line; just ensure `get_best_score_move` here receives `path`/`ply`.)

- [ ] **Step 5: Update `grogros_zero` — channel + ply + seed/forward + remove dead scratch (exploration.cpp:357-468)**

- Signature: change the trailing `Evaluation* path_local_eval` to `BackupResult* out`, then add `int ply = 0` after it (order `(path_history, out, ply)`, matching the descent call below) — exploration.h:150 + cpp:357.
- Delete `DagExcl dag_excl;` (exploration.cpp:433-437, comment + decl) and `Evaluation dag_child_eval;` (439-442, comment + decl).
- **Seed the channel** (THE fix for the expand-path gap). Immediately after the `if (!_initialized) { quiescence(...); _iterations++; }` block (after cpp:401), insert:
  ```cpp
  // #11 GHI — graine du canal de remontée : valeur intrinsèque par défaut.
  // Les early-return (terminal / feuille TT / no-moves) la conservent ; les
  // consommateurs (explore_new_move / explore_random_child) l'écrasent par la
  // valeur path-locale exacte. Garantit qu'aucun BackupResult non initialisé
  // ne remonte (Evaluation::_value n'a pas d'init par défaut, board.h:466).
  // OFF : out == nullptr -> no-op.
  if (out != nullptr) {
  	out->value = _deep_evaluation;
  	out->min_earlier_ply = INT32_MAX;
  }
  ```
  Placing it here (after init, before the `_is_terminal` / `!_can_explore` / `_got_moves <= 0` early returns at cpp:404-431) means those returns inherit the intrinsic fallback automatically — no per-return edits needed.
- The `explore_new_move` call (452) becomes:
  ```cpp
  explore_new_move(board_buffer, eval, alpha, beta, gamma, quiescence_depth, network, base_path_history, ply, out);
  ```
- The `explore_random_child` call (457) becomes:
  ```cpp
  explore_random_child(board_buffer, eval, alpha, beta, gamma, quiescence_depth, network, base_path_history, ply, out);
  ```
  Both consumers fill `out` on their normal exit and inherit the seed if they early-return. For multi-iteration root calls `out` is `nullptr` (no-op). The seed runs once; the loop's last iteration's consumer overwrites it.

- [ ] **Step 6: Remove the attempt-8 share gate — full sharing (exploration.cpp explore_new_move)**

In `explore_new_move`, delete the `const bool dag_shareable = g_tt_node_dag && !new_board->is_pawn_endgame();` line (cpp:560) and revert both gated checks `if (dag_shareable)` back to `if (g_tt_node_dag)` (the lookup at cpp:568 and the register at cpp:605). This re-enables sharing for every node (depth restored). Keep the §3 draw-leaf branch (`init_terminal_draw_child`, cpp:534-547) and its `ply`-threaded `PathScope` (already from Task 2 / Step 4b).

- [ ] **Step 7: Build**

Run the build command. Resolve compile errors (signature mismatches at call sites are expected — fix each: any other caller of `explore_random_child`/`explore_new_move`/`grogros_zero`/`get_best_score_move`/`get_move_scores`/`pick_random_child` must pass the new params; grep them). Note `explore_new_move` lost its dead `path_local_eval` and gained `int ply, BackupResult* out` — grep `explore_new_move(` and `path_local_eval` to catch any stragglers. Expected: clean once all call sites updated.

- [ ] **Step 8: Run harness — expect ALL GREEN**

Run `--dag-test` then `--dag-test 120 4000`. Expected exit 0, all PASS:
- Repro1 ON ≈ draw, Repro2 ON winning, Repro3 perpetual ≈ draw, Repro4 fortress ≈ draw.
- **Sharing active** PASS (`node_map` now large — full sharing restored).
- OFF sanity all PASS.
If Repro2 erodes to draw → selection/backup substitution is inconsistent (re-check Step 3 applies the substitution in BOTH the max pre-pass and the score loop, and Step 4 uses the same in backup). If Repro1/3/4 don't reach draw → the cache guard isn't persisting intrinsic draws (re-check `this_min_earlier_ply >= ply` and `first_ply` recording). If sharing metric still ~0 → Step 6 not applied.

- [ ] **Step 9: Commit**

```powershell
cd "C:\Users\poiro\Documents\Info\Chess\C++"; git add opti_chess/exploration.h opti_chess/exploration.cpp; git commit -m "fix(search): #11 - GHI-correct repetition via consumer draw value + cache guard (#11)"
```

---

## Task 4: Performance check (hot-path validation, #1 priority)

**Files:** possibly Modify `opti_chess/exploration.cpp`.

- [ ] **Step 1: Measure NPS ON vs OFF**

Run `--dag-test 120 4000` and read the per-batch `nodes=` and the wall time (use PowerShell `Measure-Command { ... }`). Compute approximate iterations/second ON vs OFF for Repro 2 (same budget). Record both.

- [ ] **Step 2: Decide**

- If ON throughput is within ~15% of OFF (or faster — removing DagExcl/opt-1 is a simplification), **no change needed**; record the numbers and proceed.
- If ON is materially slower (the per-child `position_is_draw_by_repetition` in the selection loops dominates), apply the fallback: in `get_move_scores`/`get_best_score_move`/`pick_random_child`, gate the per-child path lookup behind a cheap pre-check — only run it when `branch_history` is non-trivially populated for the subtree (e.g. skip when `path->size()` is below a small threshold), keeping selection and backup consistent. Re-run Step 1 to confirm recovery, and re-run the full harness to confirm correctness is retained.

- [ ] **Step 3: Commit (only if Step 2 changed code)**

```powershell
cd "C:\Users\poiro\Documents\Info\Chess\C++"; git add opti_chess/exploration.cpp; git commit -m "perf(search): #11 - guard path-aware scoring lookup in selection loop"
```

---

## Task 5: Documentation

**Files:** Modify `opti_chess/BUGFIXES.md`, `opti_chess/ALGORITHMS.md`.

- [ ] **Step 1: BUGFIXES.md #11 — record the GHI mechanism outcome**

Under the #11 section, append an entry: the GHI-correct mechanism (repetition = competing draw value at the consumer, evaluated per-path; cache guard `min_earlier_ply >= ply`; full sharing kept → depth restored) replaced all prior patches (opt-1/opt-3/§3-cut/attempt-7/attempt-8). State harness results (Repro 1 draw, Repro 2 win, perpetual draw, fortress draw, sharing-active, OFF identical) and the NPS finding from Task 4. Note the out-of-scope re-root/play corruption follow-up. Real French Unicode.

- [ ] **Step 2: ALGORITHMS.md §8 (and §7.5.f) — refresh the stale repetition/transposition docs**

§8.3/§8.5 reference the removed `make_child_path_history` and per-iteration cloning (gone since #7). Rewrite §8 to describe: the de-aliased `PositionHistory` (`{count, first_ply}`), `PathScope` push/pop, `ply` threading, the consumer draw-value rule, and the `min_earlier_ply >= ply` cache guard. Update §7.5.f (which says nodes are not shared) to point to the DAG (`g_tt_node_dag`, `node_map`, `_parent_count`) and this mechanism.

- [ ] **Step 3: Mark the spec validated and commit**

Add a one-line status note at the top of `docs/superpowers/specs/2026-05-24-optichess-dag-ghi-repetition-design.md` (implemented; harness green; GUI user-gate pending). Then:

```powershell
cd "C:\Users\poiro\Documents\Info\Chess\C++"; git add opti_chess/BUGFIXES.md opti_chess/ALGORITHMS.md "docs/superpowers/specs/2026-05-24-optichess-dag-ghi-repetition-design.md"; git commit -m "docs(search): #11 - record GHI mechanism, refresh ALGORITHMS rep/transposition (#11)"
```

---

## Final acceptance (USER GUI gate — assistant cannot run)

1. Build clean both toggle states.
2. OFF byte-identical to baseline: PERFT 1/2 + EVALUATION.
3. DAG ON (key `O`, repros `1`/`2`, plus a perpetual and a fortress in real play): both repros correct, depth visibly restored (PV depth ON ≫ OFF at equal time), no regression.

The assistant-runnable gate (`--dag-test`, exit 0, sharing-active) is satisfied by Tasks 1-3.

---

## Self-Review

- **Spec coverage:** §3.1 path state → Task 2. §3.2 consumer rule → Task 3 Steps 3, 4, **4b** (the substitution is applied in selection AND in BOTH backups — `explore_random_child` and `explore_new_move`). §3.3 cache guard → Task 3 Steps 4 + 4b (`min_earlier_ply >= ply`). §3.4 don't-descend → Task 3 Step 4. §5 removals (DagExcl/opt-1/§3-cut/attempt-8 gate/dead scratch) → Task 3 Steps 1,4,5,6. §6 tests (new positions, sharing metric, low+high iters, OFF identical) → Task 1 + Task 3 Step 8. §7 perf → Task 4. §8 docs → Task 5. De-alias safety grep → Task 2 Steps 3-4. No gap.
- **Channel completeness (root of the expand-path bug):** the `BackupResult* out` channel is filled on EVERY `grogros_zero` exit — seeded from `_deep_evaluation` after init (Step 5, covers terminal/leaf early-returns and buffers-full break) and overwritten by whichever consumer runs (`explore_new_move` Step 4b / `explore_random_child` Step 4). No path leaves `out` with an uninitialized `Evaluation`.
- **TT write-back preserved:** the `g_tt_main_search` store survives in BOTH `explore_random_child` (Step 4) and `explore_new_move` (Step 4b), gated to persist only the intrinsic value under DAG and identical to baseline when `g_tt_node_dag` is OFF — so the branch's headline feature is not regressed and OFF stays byte-identical.
- **Placeholder scan:** test FENs for Repro3/4 are concrete starting values with a Task 1 Step 4 verification/replacement procedure (the established #11 pinning pattern), not a vague TODO. All code steps show complete code or exact edit targets with current line refs.
- **Type/name consistency:** `PathEntry{count, first_ply}`, `PositionHistory`, `BackupResult{value, min_earlier_ply}`, `position_history_first_ply`, `dag_draw_eval_ref`, the `(path, ply)` selector params. Signature orders (intentionally differ): `grogros_zero(... PositionHistory* path_history, BackupResult* out, int ply)`; `explore_random_child(... PositionHistory* path_history, int ply, BackupResult* out)`; `explore_new_move(... PositionHistory* path_history, int ply, BackupResult* out)` — every call site in Steps 4/4b/5 matches these. `ply+1` on descent and `min_earlier_ply >= ply` guard match the spec.
