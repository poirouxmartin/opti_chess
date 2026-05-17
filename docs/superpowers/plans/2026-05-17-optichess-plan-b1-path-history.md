# Plan B-1 — O(1) path history (#7), behavior-preserving — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Replace the per-iteration / per-child `robin_map` copies of the repetition path history with a single owned map mutated by RAII push/pop-undo — strictly behavior-preserving, O(1) per edge — as the mandatory prerequisite for Plan B-2 (the transposition DAG).

**Architecture:** One `PositionHistory` is owned at the search root and threaded by pointer (never copied). Each descent into a child wraps the child-position record in a RAII `PathScope` (record on construct, un-record on destruct) so the map always holds exactly the root→current-node path and every push is balanced on every return path. The current "reset history to empty on an irreversible move" is dropped — provably observably-equivalent because a position's Zobrist key is unique across an irreversible move.

**Tech Stack:** C++17, MSVC (`opti_chess.sln`). Verification = MSBuild clean build + **user runtime gate** (PERFT 1/2 + EVALUATION must be *identical* to baseline — this is behavior-preserving — plus an NPS check: the Plan-A bench showed ~6.1M history copies collapsing NPS; B-1 should raise baseline NPS). Spec: `docs/superpowers/specs/2026-05-17-optichess-plan-b-dag-design.md` §4. No automated CLI test harness exists; "test" steps = build + the user gate.

---

## Correctness foundation (read first — the whole plan rests on this)

`PositionHistory = tsl::robin_map<uint64_t,uint8_t>` keyed by Zobrist key (placement + side + castling + en-passant; **not** move/halfmove counters). The current scheme (`make_child_path_history`, `exploration.cpp:88-94`) returns an **empty** history across an irreversible move (pawn move / capture), otherwise a **copy** of the parent's, then records the new position.

A pawn move or capture is irreversible: pawn structure / material changes permanently and can never be undone. Therefore **no position after an irreversible move can ever share a Zobrist key with any position before it**. Consequences:

1. A query `position_history_count(map, P)` for a position `P` can only ever match occurrences of `P` that lie in the **same reversible epoch** as `P` (cross-epoch keys cannot collide).
2. Hence an *accumulating* single map (never reset on irreversible moves) returns **exactly the same count** for every query as the current "empty-on-irreversible then rebuild" scheme.
3. With strict push-on-descend / pop-on-return, the map at any instant holds exactly the keys of the root→current-node path, so its size is depth-bounded (no need for the irreversible reset to bound size either).

Therefore dropping the copy and the irreversible reset, and threading one mutated map with balanced push/pop, yields **identical `position_is_draw_by_repetition` decisions everywhere** — i.e. identical search behavior — at O(1) per edge instead of O(map) per edge. This equivalence is the gate criterion: PERFT 1/2 + EVALUATION must be unchanged.

**Balance invariant:** every `PathScope` constructed must be destructed on every control-flow path (including the many early `return`s in these functions). RAII guarantees this; do not hand-roll record/un-record pairs.

---

## Testing model

No CLI test runner for search. Each code task: (a) MSBuild clean (exit 0, no `error`), (b) reasoned behavior-preservation argument in the commit/PR. One final **[USER]** gate task: build in VS, confirm PERFT 1/2 + EVALUATION identical to `main`, and report NPS on the reference position vs the Plan-A OFF baseline (expected: higher, no progressive decay).

Build command (the literal `MSBuild opti_chess.sln` does NOT work here — `.vcxproj` pins absent toolset `v145`; use the non-invasive override, from the working dir, PowerShell; success = `EXITCODE=0` and no `error` lines):

```
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "opti_chess.sln" /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /m /nologo /verbosity:minimal /clp:ErrorsOnly; "EXITCODE=$LASTEXITCODE"
```

Do NOT modify any `.vcxproj`/`.sln`/project file. Commits: English, ASCII, conventional, no AI attribution.

---

## File Structure

Single file: **`opti_chess/exploration.cpp`** (path-history helpers + the four search functions) and its header **`opti_chess/exploration.h`** (one declaration). No new files; the change is a localized representation/threading refactor following existing patterns. `make_child_path_history` becomes unused and is removed in the final cleanup task.

Order is deliberate: introduce the helper (inert), convert leaf-most usage first (`quiescence`), then `explore_new_move`, then `explore_random_child`, then `grogros_zero` (removes the per-iteration copy), then delete the dead `make_child_path_history`. Each task builds and is independently behavior-preserving.

---

## Task 1: Add `unrecord_position_in_history` + `PathScope` RAII (inert)

**Files:**
- Modify: `opti_chess/exploration.cpp` (immediately after `record_position_in_history`, which ends at line 82, before `position_is_draw_by_repetition` at line 84)
- Modify: `opti_chess/exploration.h` (after the `PositionHistory` alias at line 10)

- [ ] **Step 1: Add the un-record helper + RAII guard in exploration.cpp**

Existing context (`exploration.cpp:79-86`):
```cpp
void record_position_in_history(PositionHistory& path_history, Board& board) {
	board.get_zobrist_key();
	path_history[board._zobrist_key]++;
}

bool position_is_draw_by_repetition(const PositionHistory& path_history, Board& board, uint8_t repetition_limit = search_repetition_limit) {
	return position_history_count(path_history, board) + 1 >= repetition_limit;
}
```

Insert between `record_position_in_history`'s closing `}` (line 82) and the blank line before `position_is_draw_by_repetition` (line 84):
```cpp

// #7 / Plan B-1 — annule un record_position_in_history (pop). Symétrique exact :
// décrémente le compteur, efface l'entrée à 0 pour borner la taille de la map.
void unrecord_position_in_history(PositionHistory& path_history, Board& board) {
	board.get_zobrist_key();
	const auto it = path_history.find(board._zobrist_key);
	if (it == path_history.end()) {
		return;
	}
	if (it->second <= 1) {
		path_history.erase(it);
	}
	else {
		it->second--;
	}
}

// RAII : push (record) la position du board dans l'historique à la construction,
// pop (unrecord) à la destruction. Garantit l'équilibrage sur TOUT chemin de
// sortie (returns anticipés inclus) — voir « Balance invariant » du plan.
struct PathScope {
	PositionHistory& _history;
	Board& _board;
	PathScope(PositionHistory& history, Board& board) : _history(history), _board(board) {
		record_position_in_history(_history, _board);
	}
	~PathScope() {
		unrecord_position_in_history(_history, _board);
	}
	PathScope(const PathScope&) = delete;
	PathScope& operator=(const PathScope&) = delete;
};
```

Note: these are defined inside the same anonymous `namespace { ... }` region as the other helpers (it closes at `exploration.cpp:70` — wait, verify: `position_history_count` is inside the namespace which closes at line 70; `record_position_in_history` etc. are AFTER line 70, file scope). Place the new helper + struct **at file scope immediately after `record_position_in_history`** (same scope as it), NOT inside the anonymous namespace. Match the placement to `record_position_in_history`'s scope exactly.

- [ ] **Step 2: Declare them in exploration.h**

Existing context (`exploration.h:10`):
```cpp
using PositionHistory = RepetitionHistory;
```

The existing free functions (`record_position_in_history`, etc.) — check whether they are declared in `exploration.h`. Grep `record_position_in_history` in `exploration.h`. If the existing helpers are NOT declared in the header (file-local usage only), then `unrecord_position_in_history`/`PathScope` likewise need no header declaration — skip this step. If they ARE declared there, add directly below the matching declaration block:
```cpp
void unrecord_position_in_history(PositionHistory& path_history, Board& board);
```
(Determine which case applies by grepping; do not add a header decl if the siblings have none — match the existing pattern.)

- [ ] **Step 3: Build**

Run the build command above. Expected `EXITCODE=0`, no `error` lines (helpers unused → "unused" warnings acceptable).

- [ ] **Step 4: Commit**

```bash
git add opti_chess/exploration.cpp opti_chess/exploration.h
git commit -m "feat(search): add unrecord_position_in_history + PathScope RAII (#7, inert)"
```

---

## Task 2: Convert `quiescence` move loop to single-map threading

**Files:**
- Modify: `opti_chess/exploration.cpp` — `Node::quiescence`, the per-move block around lines 1066-1154 and the recursive `child->quiescence(... &child_path_history ...)` call site(s) below it; the function signature already takes `const PositionHistory *path_history`.

Context: quiescence iterates candidate moves; per move it does `PositionHistory child_path_history = make_child_path_history(path_history, *_board, move);` (`:1068`), then either `record_position_in_history(child_path_history, *child->_board)` (already-explored, `:1109`) or the draw-check `position_is_draw_by_repetition(child_path_history, *new_board)` (`:1129`) then `record_position_in_history(child_path_history, *new_board)` (`:1143`), and later recurses `child->quiescence(..., &child_path_history)`.

- [ ] **Step 1: Make the signature non-const and locate the recursive call**

`quiescence` takes `const PositionHistory *path_history`. We must mutate it (push/pop), so change the parameter to `PositionHistory *path_history` in BOTH the definition (`exploration.cpp:825`) and the declaration (`exploration.h:161`). Existing decl (`exploration.h:161`):
```cpp
	int quiescence(BoardBuffer* board_buffer, Evaluator* evaluator, int depth, double search_alpha, double search_beta, int alpha = -INT_MAX, int beta = INT_MAX, Network* network = nullptr, bool evaluate_threats = true, int beta_margin = 0, const PositionHistory *path_history = nullptr);
```
Change `const PositionHistory *path_history` → `PositionHistory *path_history`. Mirror the exact same change on the definition line `exploration.cpp:825`. (No other `const` on the param elsewhere.)

- [ ] **Step 2: Replace the per-move copy + record/draw-check with threaded map + PathScope around the recursion**

Replace this block (`exploration.cpp:1066-1068`):
```cpp
			// Historique indépendant des coups pour les répétitions.
			// Comme en recherche principale, il doit rester local à la branche.
			PositionHistory child_path_history = make_child_path_history(path_history, *_board, move);
```
with:
```cpp
			// #7 / B-1 — historique unique threadé (plus de copie par coup).
			// La répétition reste correcte (clés Zobrist uniques par époque
			// réversible) ; push/pop équilibré via PathScope plus bas.
			PositionHistory& branch_history = *path_history;
```

Then, in the already-explored sub-branch, replace (`:1109`):
```cpp
				record_position_in_history(child_path_history, *child->_board);
```
with: *(delete this line — the push is done by a `PathScope` immediately before the recursive `child->quiescence(...)` call; see Step 3. Recording here then again at the recursion would double-count. The draw check below uses the pre-push count, matching current semantics.)*
```cpp
				// (record différé : PathScope autour de l'appel récursif, Step 3)
```

In the new-node sub-branch, replace the draw check (`:1129`) `position_is_draw_by_repetition(child_path_history, *new_board)` argument and the record (`:1143`):
- `:1129` `if (position_is_draw_by_repetition(child_path_history, *new_board)) {` → `if (position_is_draw_by_repetition(branch_history, *new_board)) {`
- `:1143` `record_position_in_history(child_path_history, *new_board);` → delete (deferred to the PathScope at the recursion, Step 3):
```cpp
					// (record différé : PathScope autour de l'appel récursif, Step 3)
```

For the already-explored branch draw check (there is also a `position_is_draw_by_repetition(child_path_history, *child->_board)`-style usage? — verify by reading the full quiescence move loop): every remaining `child_path_history` reference becomes `branch_history`, every `record_position_in_history(child_path_history, X)` is deleted in favor of the Step-3 PathScope, every `position_is_draw_by_repetition(child_path_history, X)` becomes `position_is_draw_by_repetition(branch_history, X)`. (Read the whole loop body `exploration.cpp:1066`→ the recursive `child->quiescence` call and fix EVERY occurrence; there must be zero `child_path_history` and zero `make_child_path_history` left in `quiescence` after this task.)

- [ ] **Step 3: Wrap the recursive `child->quiescence` call in a PathScope**

Replace exactly (`exploration.cpp:1162-1163`):
```cpp
				// Appel récursif sur le fils
				int score = - child->quiescence(board_buffer, eval, new_depth - 1, search_alpha, search_beta, -beta, -alpha, network, false, beta_margin, &child_path_history);
```
with (push child position for the recursion duration, pop guaranteed on every exit path; `score` declared before the block since it is used afterwards for the beta-cutoff/alpha update):
```cpp
				// Appel récursif sur le fils — #7/B-1 : push la position du fils
				// pour la durée de la récursion, pop garanti à la sortie de scope.
				int score;
				{
					PathScope _ps(branch_history, *child->_board);
					score = - child->quiescence(board_buffer, eval, new_depth - 1, search_alpha, search_beta, -beta, -alpha, network, false, beta_margin, &branch_history);
				}
```
This is the only recursive `child->quiescence` call in the function. `score` is consumed unchanged below (`if (score >= beta)`, `if (score > alpha)`). After this task there must be zero `child_path_history` and zero `make_child_path_history` remaining in `quiescence`.

- [ ] **Step 4: Build**

Build command. Expected `EXITCODE=0`, no errors. Zero `child_path_history` / `make_child_path_history` remain in `quiescence`.

- [ ] **Step 5: Commit**

```bash
git add opti_chess/exploration.cpp opti_chess/exploration.h
git commit -m "refactor(search): thread single path history through quiescence (#7)"
```

---

## Task 3: Convert `explore_new_move` to single-map threading

**Files:**
- Modify: `opti_chess/exploration.cpp:341-521` (`Node::explore_new_move`)

- [ ] **Step 1: Replace the copy with a threaded reference**

Replace (`exploration.cpp:345`):
```cpp
	PositionHistory child_path_history = make_child_path_history(path_history, *_board, move);
```
with:
```cpp
	// #7 / B-1 — historique unique threadé (plus de copie par coup).
	PositionHistory& branch_history = *path_history;
```

- [ ] **Step 2: Rewrite the already-explored branch**

Replace (`:358-367`):
```cpp
	if (already_explored) {
		child = child_link->_node;
		record_position_in_history(child_path_history, *child->_board);

		if (_nodes <= child_link->_propagated_nodes) {
			cout << "child nodes >= nodes???" << endl;
		}

		_nodes -= child_link->_propagated_nodes;
	}
```
with (drop the eager record — child position is pushed via PathScope around the quiescence call in Step 4; the `_nodes` bookkeeping is unchanged):
```cpp
	if (already_explored) {
		child = child_link->_node;

		if (_nodes <= child_link->_propagated_nodes) {
			cout << "child nodes >= nodes???" << endl;
		}

		_nodes -= child_link->_propagated_nodes;
	}
```

- [ ] **Step 3: Fix the draw check + new-node record in the else branch**

Replace (`:381`):
```cpp
		if (position_is_draw_by_repetition(child_path_history, *new_board)) {
```
with:
```cpp
		if (position_is_draw_by_repetition(branch_history, *new_board)) {
```
Replace (`:398`):
```cpp
			record_position_in_history(child_path_history, *new_board);
			new_board->get_zobrist_key();
```
with (record deferred to the PathScope in Step 4; keep the zobrist call):
```cpp
			new_board->get_zobrist_key();
```

- [ ] **Step 4: Wrap the two `child->quiescence` calls in a PathScope**

The block `exploration.cpp:437-452` calls `child->quiescence(..., &child_path_history)` in two places (lines 442 and 448). Replace the whole block:
```cpp
	if (!child->_fully_explored) {

		bool test = false;

		if (test) {
			child->quiescence(board_buffer, eval, 2, alpha, beta, -INT32_MAX, INT32_MAX, network, true, 0, &child_path_history); // TODO *** faire un cutoff plus facile, si l'éval de base est déjà mauvaise? par rapport à l'évaluation statique
		}

		// Si l'évaluation est meilleure que celle de base, on regarde la quiescence
		else if (!test || child->_static_evaluation._value * _board->get_color() > _static_evaluation._value * _board->get_color()) {

			child->quiescence(board_buffer, eval, quiescence_depth, alpha, beta, -INT32_MAX, INT32_MAX, network, true, 0, &child_path_history);
		}

		child->_fully_explored = true;
	}
```
with (push child position for the quiescence call duration; `branch_history` threaded; `&child_path_history` → `&branch_history`):
```cpp
	if (!child->_fully_explored) {

		bool test = false;

		PathScope _ps(branch_history, *child->_board); // push child position (popped on scope exit, all paths)

		if (test) {
			child->quiescence(board_buffer, eval, 2, alpha, beta, -INT32_MAX, INT32_MAX, network, true, 0, &branch_history); // TODO *** faire un cutoff plus facile, si l'éval de base est déjà mauvaise? par rapport à l'évaluation statique
		}

		// Si l'évaluation est meilleure que celle de base, on regarde la quiescence
		else if (!test || child->_static_evaluation._value * _board->get_color() > _static_evaluation._value * _board->get_color()) {

			child->quiescence(board_buffer, eval, quiescence_depth, alpha, beta, -INT32_MAX, INT32_MAX, network, true, 0, &branch_history);
		}

		child->_fully_explored = true;
	}
```
Note: `_ps` lives to the end of the `if (!child->_fully_explored)` scope; quiescence is the only path-history consumer inside it, so the push window matches the old `child_path_history` lifetime exactly. After this task there must be zero `child_path_history` / `make_child_path_history` in `explore_new_move`.

- [ ] **Step 5: Build**

Build command. Expected `EXITCODE=0`, no errors.

- [ ] **Step 6: Commit**

```bash
git add opti_chess/exploration.cpp
git commit -m "refactor(search): thread single path history through explore_new_move (#7)"
```

---

## Task 4: Convert `explore_random_child` to single-map threading

**Files:**
- Modify: `opti_chess/exploration.cpp:524-570` (`Node::explore_random_child`)

- [ ] **Step 1: Replace the copy + eager record; wrap the recursion in PathScope**

Replace (`exploration.cpp:530-541`):
```cpp
	PositionHistory child_path_history = make_child_path_history(path_history, *_board, move);
	record_position_in_history(child_path_history, *child->_board);

	if (child_link._propagated_nodes >= _nodes) {
		cout << "child nodes >= nodes in random exploration??? main position: " << _board->to_fen() << ", child position: " << child->_board->to_fen() << endl;
	}

	// Nombre de noeuds du fils
	const int initial_child_nodes = child_link._propagated_nodes;

	// Explore le fils
	child->grogros_zero(board_buffer, eval, alpha, beta, gamma, 1, quiescence_depth, network, &child_path_history); // L'évaluation du fils est mise à jour ici
```
with:
```cpp
	// #7 / B-1 — historique unique threadé ; push de la position du fils pour
	// la durée de la récursion uniquement, pop garanti à la sortie de scope.
	PositionHistory& branch_history = *path_history;

	if (child_link._propagated_nodes >= _nodes) {
		cout << "child nodes >= nodes in random exploration??? main position: " << _board->to_fen() << ", child position: " << child->_board->to_fen() << endl;
	}

	// Nombre de noeuds du fils
	const int initial_child_nodes = child_link._propagated_nodes;

	// Explore le fils
	{
		PathScope _ps(branch_history, *child->_board);
		child->grogros_zero(board_buffer, eval, alpha, beta, gamma, 1, quiescence_depth, network, &branch_history); // L'évaluation du fils est mise à jour ici
	}
```
Everything after (the write-back block, `_nodes`/`_iterations` updates) is unchanged. After this task there must be zero `child_path_history` / `make_child_path_history` in `explore_random_child`.

- [ ] **Step 2: Build**

Build command. Expected `EXITCODE=0`, no errors.

- [ ] **Step 3: Commit**

```bash
git add opti_chess/exploration.cpp
git commit -m "refactor(search): thread single path history through explore_random_child (#7)"
```

---

## Task 5: Remove the per-iteration history clone in `grogros_zero`

**Files:**
- Modify: `opti_chess/exploration.cpp:262-327` (`Node::grogros_zero` init + loop)

- [ ] **Step 1: Drop the per-iteration copy; pass the base map directly**

Replace (`exploration.cpp:262-266`):
```cpp
	// Base path for this call. Each outer iteration clones it so branches never leak
	// repetition state into one another.
	PositionHistory local_path_history;
	PositionHistory* base_path_history = path_history != nullptr ? path_history : &local_path_history;
	ensure_position_in_history(*base_path_history, *_board);
	_board->get_zobrist_key();
```
with (semantics preserved: at the root call `path_history==nullptr` we own a local map; `ensure_position_in_history` still seeds this node's own position; the per-iteration isolation that the old clone provided is now guaranteed by the balanced PathScope push/pop in `explore_*` — each iteration returns the map to exactly this base state):
```cpp
	// #7 / B-1 — un seul historique possédé à la racine, threadé par pointeur.
	// Plus de clone par itération : l'isolement inter-itérations est garanti
	// par le push/pop équilibré (PathScope) dans explore_new_move /
	// explore_random_child — chaque itération restitue l'historique à cet état.
	PositionHistory local_path_history;
	PositionHistory* base_path_history = path_history != nullptr ? path_history : &local_path_history;
	ensure_position_in_history(*base_path_history, *_board);
	_board->get_zobrist_key();
```
(The block text is unchanged except the comment; the substantive change is Step 2 removing the clone.)

Replace (`exploration.cpp:300-301`):
```cpp
	while (iterations > 0) {
		PositionHistory iteration_path_history = *base_path_history;
```
with:
```cpp
	while (iterations > 0) {
```

And update the two dispatch calls (`:308`, `:313`) to pass `base_path_history` instead of `&iteration_path_history`:
- `:308` `explore_new_move(board_buffer, eval, alpha, beta, gamma, quiescence_depth, network, &iteration_path_history);` → `explore_new_move(board_buffer, eval, alpha, beta, gamma, quiescence_depth, network, base_path_history);`
- `:313` `explore_random_child(board_buffer, eval, alpha, beta, gamma, quiescence_depth, network, &iteration_path_history);` → `explore_random_child(board_buffer, eval, alpha, beta, gamma, quiescence_depth, network, base_path_history);`

After this task there must be zero `iteration_path_history` in the file.

- [ ] **Step 2: Verify the `quiescence` init call still passes the base map**

`grogros_zero:271` calls `quiescence(..., base_path_history)` already (the `if (!_initialized)` block). Confirm it passes `base_path_history` (a `PositionHistory*`) and that Task 2 made `quiescence`'s param non-const so this compiles. No change expected here beyond what Task 2 did; just confirm during build.

- [ ] **Step 3: Build**

Build command. Expected `EXITCODE=0`, no errors. Grep the file: zero occurrences of `iteration_path_history`.

- [ ] **Step 4: Commit**

```bash
git add opti_chess/exploration.cpp
git commit -m "refactor(search): remove per-iteration path-history clone in grogros_zero (#7)"
```

---

## Task 6: Delete dead `make_child_path_history` + drop the irreversible-reset path

**Files:**
- Modify: `opti_chess/exploration.cpp:88-94` (and any header decl)

- [ ] **Step 1: Confirm it is now unused, then delete it**

Grep `make_child_path_history` across `opti_chess/`. After Tasks 2-5 the only occurrence must be its definition (`exploration.cpp:88-94`) and any header declaration. If any *call site* remains, STOP — a prior task missed a conversion; fix that first (do not delete while referenced).

Delete the definition (`exploration.cpp:88-94`):
```cpp
PositionHistory make_child_path_history(const PositionHistory* path_history, const Board& parent_board, const Move& move) {
	if (path_history == nullptr || parent_board.is_irreversible_move(move)) {
		return PositionHistory();
	}

	return *path_history;
}
```
and its header declaration if one exists (grep `make_child_path_history` in `exploration.h`; delete the matching line if present). This removes the irreversible-reset behavior; per the Correctness Foundation this is observably equivalent (cross-epoch Zobrist uniqueness).

- [ ] **Step 2: Build**

Build command. Expected `EXITCODE=0`, no errors, no `make_child_path_history` anywhere.

- [ ] **Step 3: Commit**

```bash
git add opti_chess/exploration.cpp opti_chess/exploration.h
git commit -m "refactor(search): drop dead make_child_path_history + irreversible reset (#7)"
```

---

## Task 7: [USER] Behavior-preservation + perf gate

No code. User-run; this is the B-1 acceptance gate (B-2/DAG must not start until this passes).

- [ ] **Step 1: [USER] Build in Visual Studio** as usual (IDE resolves `v145`).

- [ ] **Step 2: [USER] Behavior identical** — confirm **PERFT 1/2 and EVALUATION are unchanged vs `main`** on the usual positions. B-1 is behavior-preserving by construction (Correctness Foundation); any divergence means a push/pop imbalance — report which position and the divergence; do not proceed.

- [ ] **Step 3: [USER] Perf check** — on the reference position (king + blocked pawns), fixed iterations, record NPS and whether it stays stable (no progressive decay). Expectation vs the Plan-A OFF baseline (~40k NPS) / the Plan-A ON pathology: B-1 removes ~6.1M `robin_map` copies, so NPS should be **≥ baseline and non-decaying**. Report the numbers.

- [ ] **Step 4: Record + commit**

Update `opti_chess/BUGFIXES.md` #7 entry: mark fixed, note the O(1) push/pop-undo representation and the cross-epoch-Zobrist correctness argument, with the measured NPS.
```bash
git add opti_chess/BUGFIXES.md
git commit -m "docs(bugfixes): #7 fixed - O(1) push/pop path history, behavior-preserving"
```

---

## Self-Review

**Spec coverage (spec §4):** §4 requires replacing the `robin_map` copy + per-iteration copy with one reference-threaded structure, push/pop-undo, irreversible epoch handling, making the future per-edge recheck O(1). Covered: Task 1 (PathScope/unrecord), Tasks 2-5 (thread the single map through quiescence/explore_new_move/explore_random_child/grogros_zero, remove per-iteration clone), Task 6 (drop dead copy + irreversible reset, justified by the Correctness Foundation = the "epoch handling" — proven unnecessary rather than implemented, which §4's intent allows). Task 7 = the behavior-preserving validation §4 implies. No spec gap for B-1's scope. (B-2/DAG is a separate plan, intentionally out of scope here.)

**Placeholder scan:** None. The recursive `child->quiescence` call (Task 2 Step 3) is now concrete (`exploration.cpp:1162-1163` verbatim). The two `child->quiescence` calls in `explore_new_move` (Task 3 Step 4, lines 442/448) are literal. All steps have complete before/after code.

**Type consistency:** `unrecord_position_in_history(PositionHistory&, Board&)` and `PathScope(PositionHistory&, Board&)` defined Task 1, used Tasks 2-4 with consistent signatures. `branch_history` is `PositionHistory&` everywhere. `quiescence`'s param de-const'd in Task 2 (decl + def) before Task 5 relies on passing a mutable `PositionHistory*`. `make_child_path_history` removed only in Task 6 after all call sites converted (Tasks 2-5). Consistent.

**Risk note:** the only behavioral risk is a push/pop imbalance on some early-return path; RAII `PathScope` (Task 1) structurally prevents it, and Task 7 Step 2 is the hard gate. Tasks 2/3 require reading the real `quiescence` call lines — the executor must read them, not guess.
