# Plan B-2 — Transposition DAG (node sharing), #11 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Share one `Node` across every path that reaches a position with the same Zobrist key (a general DAG), so transposed subtrees are explored once and the search budget goes to depth instead of re-exploring combinatorially.

**Architecture:** A global `robin_map<uint64_t, Node*> node_map` (Zobrist key → live `Node*`), distinct from the evaluation `transposition_table`. `explore_new_move` links a new edge to an existing shared node on a key hit (link-on-create) instead of allocating a fresh subtree. Repetition stays chess-correct by **re-verifying draw-by-repetition against the current path on every traversal** (the shared structure carries no path state — that lives only in the threaded `PositionHistory` from B-1). Everything is behind a runtime toggle `g_tt_node_dag` (default OFF); OFF is byte-identical to the current tree engine.

**Tech Stack:** C++17, MSVC (`opti_chess.sln`). Verification = MSBuild clean build + **user runtime gate** (OFF must be *identical* to the post-B-1 baseline; ON must produce fewer unique nodes per depth + a depth gain on the reference position, same best move, PERFT 1/2 + EVALUATION stable). Spec: `docs/superpowers/specs/2026-05-17-optichess-plan-b-dag-design.md`. Prerequisite **B-1 (#7 O(1) path history) is complete** on this branch. No automated CLI test harness exists for search; "test" = build + the user gate.

---

## Foundations (read first — the plan rests on these)

From the validated spec, already verified in code:

- **Path state is not in `Node`/`Board`.** Repetition is path-local, carried only by the threaded `PositionHistory` (B-1, `exploration.cpp:64-118`). A shared `Node` therefore carries no path-dependent information — sharing the structure is safe; only the *repetition verdict* is path-dependent and must be re-derived per traversal (§3 below).
- **Backprop is pull-based.** `_deep_evaluation` is recopied from the best child on each visit (`exploration.cpp:532`, `:578`); each parent re-derives independently from a shared child, so multi-parenting is not a backprop problem.
- **Lifecycle is already DAG-correct.** `Node::_parent_count` is a refcount: `++` in `add_child` (`exploration.cpp:216`), `--` in `reset` (`:665`), recycle only at `<=0` (`:667-671`). A shared child is freed only at its last detach.
- **Cycles are bounded.** Any graph cycle is composed only of reversible moves; the threaded history is preserved through an all-reversible cycle, so the 2nd occurrence triggers `position_is_draw_by_repetition` (`search_repetition_limit = 2`) and the cycle is cut within the repetition limit — provided §3 (per-traversal recheck) is in place.

**OFF-safety invariant (every code task must preserve it):** when `g_tt_node_dag == false`, `node_map` is never read or written, link-on-create is skipped, the §3 recheck is skipped, and the relaxed `_nodes` guards behave exactly as today ⇒ the engine is byte-identical to the post-B-1 state. Plan A (`g_tt_main_search`) stays OFF and independent; the two toggles are not meant to be ON together (document, do not enforce).

---

## Testing model

No CLI test runner for search. Each code task: (a) MSBuild clean (exit 0, no `error`), (b) a written OFF-byte-identical argument in the commit. One final **[USER]** gate task: build in VS, confirm OFF == post-B-1 baseline (PERFT 1/2 + EVALUATION), then toggle ON on the reference position and record `node_map.size()` (unique nodes), `get_main_depth`, best move, time.

Build command (the literal `MSBuild opti_chess.sln` does NOT work — `.vcxproj` pins absent toolset `v145`; use the override, from the working dir, PowerShell; success = `EXITCODE=0` and no `error` lines):

```
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "opti_chess.sln" /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /m /nologo /verbosity:minimal /clp:ErrorsOnly; "EXITCODE=$LASTEXITCODE"
```

Do NOT modify any `.vcxproj`/`.sln`/project file. Commits: English, ASCII, conventional, no AI attribution. French comments use real Unicode (é, è, à, ç, …).

---

## File Structure

All changes are localized to existing files (no new files); the DAG is a behavior added behind a toggle, following the exact pattern of Plan A's `g_tt_main_search`:

- `opti_chess/exploration.h` — extern declarations: `g_tt_node_dag`, `node_map`.
- `opti_chess/exploration.cpp` — global definitions; link-on-create + node_map registration in `explore_new_move`; per-traversal repetition recheck in `explore_random_child`; `node_map` erase in `recycle_detached_node`; relaxed tree-assumption `cout` guards; recursion-depth safety guard in `grogros_zero`.
- `opti_chess/gui.h` — GUI member `_tt_node_dag`.
- `opti_chess/gui.cpp` — toggle mirror before `grogros_zero`; HUD line.
- `opti_chess/main_gui.h` — `KEY_O` keybind; `node_map.clear()` at the search-reset site.
- `opti_chess/game_tree.cpp`, `opti_chess/board.cpp` — `node_map.clear()` next to their `transposition_table.clear()` search-reset calls.

Task order is deliberate: scaffold (inert) → lifecycle plumbing (populate only, still byte-identical even ON) → link-on-create + soundness recheck together (the DAG turns on, sound from the first moment it exists) → guard relaxation → recursion safety → user gate.

---

## Task 1: Gating scaffold (inert)

**Files:**
- Modify: `opti_chess/exploration.h` (after line 265, `extern bool g_tt_main_search;`)
- Modify: `opti_chess/exploration.cpp` (near line 1861, `bool g_tt_main_search = false;`)
- Modify: `opti_chess/gui.h` (after line 109, `bool _tt_main_search = false;`)
- Modify: `opti_chess/gui.cpp` (after line 1055, the `g_tt_main_search` mirror; and line 1418, the HUD string)
- Modify: `opti_chess/main_gui.h` (after the `KEY_I` block at lines 307-311)

- [ ] **Step 1: Declare extern symbols in exploration.h**

After `exploration.h:265` (`extern bool g_tt_main_search;`), insert:
```cpp

// #11 Plan B — DAG de transpositions (partage de noeuds). A/B runtime, defaut
// OFF : OFF = arbre actuel au byte pres. Pas cense etre ON avec g_tt_main_search.
extern bool g_tt_node_dag;

// #11 Plan B — cle Zobrist -> Node* VIVANT. Distinct de transposition_table
// (TT d'evaluation). Consulte/peuple uniquement si g_tt_node_dag. Meme style
// de map que Node::_children (robin_map, exploration.h:43).
extern robin_map<uint64_t, Node*> node_map;
```

- [ ] **Step 2: Define the globals in exploration.cpp**

`exploration.cpp:1861` is `bool g_tt_main_search = false;` and `:1856` is `NodeBuffer monte_node_buffer;`. Immediately after the `bool g_tt_main_search = false;` line, insert:
```cpp
bool g_tt_node_dag = false; // #11 Plan B — voir exploration.h
robin_map<uint64_t, Node*> node_map; // #11 Plan B — voir exploration.h
```

- [ ] **Step 3: Add the GUI member in gui.h**

`gui.h:109` is `bool _tt_main_search = false; // #11 Plan A ...`. Immediately after it, insert:
```cpp
	bool _tt_node_dag = false; // #11 Plan B — DAG de transpositions (A/B runtime, defaut OFF)
```

- [ ] **Step 4: Mirror the toggle to the global before grogros_zero (gui.cpp)**

`gui.cpp:1055` is:
```cpp
	g_tt_main_search = _tt_main_search; // #11 Plan A — propage le toggle au global lu dans exploration.cpp
```
Immediately after that line, insert:
```cpp
	g_tt_node_dag = _tt_node_dag; // #11 Plan B — propage le toggle au global lu dans exploration.cpp
```

- [ ] **Step 5: Add the HUD line (gui.cpp)**

`gui.cpp:1418` builds `monte_carlo_text` and contains the substring:
```cpp
+ "\nTT main search : " + (_tt_main_search ? "true" : "false") + " (I)";
```
Replace that exact trailing fragment with:
```cpp
+ "\nTT main search : " + (_tt_main_search ? "true" : "false") + " (I)" + "\nTT node DAG : " + (_tt_node_dag ? "true" : "false") + " (O)";
```

- [ ] **Step 6: Add the KEY_O keybind (main_gui.h)**

`main_gui.h:307-311` is the `KEY_I` block:
```cpp
		// I - #11 Plan A : bascule TT dans la recherche principale (A/B runtime)
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_I)) {
			main_GUI._tt_main_search = !main_GUI._tt_main_search;
			cout << "TT main search : " << (main_GUI._tt_main_search ? "true" : "false") << endl;
		}
```
Immediately after that block's closing `}` (line 311), insert (KEY_O verified unused in `main_gui.h`):
```cpp

		// O - #11 Plan B : bascule le DAG de transpositions (A/B runtime)
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_O)) {
			main_GUI._tt_node_dag = !main_GUI._tt_node_dag;
			cout << "TT node DAG : " << (main_GUI._tt_node_dag ? "true" : "false") << endl;
		}
```

- [ ] **Step 7: Build**

Run the build command. Expected `EXITCODE=0`, no `error` lines (`node_map`/`g_tt_node_dag` defined but unused yet → acceptable).

- [ ] **Step 8: Commit**

```bash
git add opti_chess/exploration.h opti_chess/exploration.cpp opti_chess/gui.h opti_chess/gui.cpp opti_chess/main_gui.h
git commit -m "feat(search): g_tt_node_dag toggle + node_map scaffold + HUD/keybind O (#11, inert)"
```

OFF-byte-identical argument: `node_map` and `g_tt_node_dag` exist but are read by no code path; the keybind/HUD only display/flip a flag nothing consumes. Byte-identical ON and OFF.

---

## Task 2: node_map lifecycle — register-on-miss, erase-on-recycle, clear-on-reset (populate only)

This wires the map's *lifetime* without anyone consuming it for linking yet. Even with the toggle ON the engine stays byte-identical (we populate/erase the map but never branch on a hit — that is Task 3).

**Files:**
- Modify: `opti_chess/exploration.cpp` — `explore_new_move` non-draw branch (lines 425-438); `recycle_detached_node` (lines 625-635)
- Modify: `opti_chess/main_gui.h` (search-reset site near line 420, `transposition_table.clear();`)
- Modify: `opti_chess/gui.cpp` (lines 982 and 1942, `transposition_table.clear();`)
- Modify: `opti_chess/game_tree.cpp` (line 95, `transposition_table.clear();`)
- Modify: `opti_chess/board.cpp` (line 10189, `transposition_table.clear();`)

- [ ] **Step 1: Register the freshly-created node in explore_new_move (miss path)**

Current non-draw `else` branch in `explore_new_move` (`exploration.cpp:425-438`):
```cpp
		// Sinon, on crée un nouveau noeud normalement
		else {
			new_board->get_zobrist_key();

			// Création du noeud fils (pas de partage via TT — un noeud partagé entre
			// plusieurs parents casse _nodes, backpropagation et crée des cycles)
			child = monte_node_buffer.get_first_free_node();

			if (child == nullptr)
				return;

			child->_board = new_board;
			created_new_node = true;
		}
```
Replace with (register the new node under its Zobrist key when the DAG is ON; comment updated — sharing is now opt-in via the DAG, see Task 3):
```cpp
		// Sinon, on crée un nouveau noeud normalement
		else {
			new_board->get_zobrist_key();

			// Création du noeud fils. #11 Plan B — quand g_tt_node_dag, on
			// l'enregistre dans node_map pour que les chemins transposés futurs
			// puissent le partager (link-on-create, Task 3). OFF : node_map
			// jamais touché → arbre au byte près.
			child = monte_node_buffer.get_first_free_node();

			if (child == nullptr)
				return;

			child->_board = new_board;
			created_new_node = true;

			if (g_tt_node_dag) {
				node_map[new_board->_zobrist_key] = child;
			}
		}
```

- [ ] **Step 2: Erase the node_map entry on recycle**

`recycle_detached_node` (`exploration.cpp:625-635`) currently:
```cpp
void recycle_detached_node(Node* node) {
	if (node == nullptr)
		return;

	Board* board = node->_board;
	if (board != nullptr && board->_buffer_index >= 0 && !monte_board_buffer._bulk_resetting)
		monte_board_buffer.free_index(board->_buffer_index);

	if (node->_buffer_index >= 0 && !monte_node_buffer._bulk_resetting)
		monte_node_buffer.free_index(node->_buffer_index);
}
```
Insert, immediately after `if (node == nullptr) return;` and before `Board* board = node->_board;`:
```cpp
	// #11 Plan B — un noeud detache et recycle ne doit plus etre joignable via
	// node_map (sinon pointeur pendant / resurrection). On efface l'entree
	// seulement si elle pointe bien vers CE noeud (un miss a pu la reecrire).
	if (g_tt_node_dag && node->_board != nullptr) {
		const auto it = node_map.find(node->_board->_zobrist_key);
		if (it != node_map.end() && it->second == node) {
			node_map.erase(it);
		}
	}
```

- [ ] **Step 3: Clear node_map at every search-tree reset (next to transposition_table.clear())**

At EACH of these exact sites, insert `node_map.clear();` on the line immediately after the `transposition_table.clear();` call, matching its indentation:
- `opti_chess/main_gui.h:420`
- `opti_chess/gui.cpp:982`
- `opti_chess/gui.cpp:1942`
- `opti_chess/game_tree.cpp:95`
- `opti_chess/board.cpp:10189`

Inserted line (same indentation as the adjacent `transposition_table.clear();`):
```cpp
	node_map.clear(); // #11 Plan B — purge le DAG en meme temps que la TT (pas de pointeur pendant inter-recherches)
```
`node_map.clear()` on an always-empty map (toggle OFF) is a no-op, so this is unconditional and OFF-safe. If `transposition_table.clear();` is not at the exact cited line in a file (line drift), locate the call by content in that file and insert immediately after it; do not add it anywhere else.

- [ ] **Step 4: Build**

Build command. Expected `EXITCODE=0`, no errors.

- [ ] **Step 5: Commit**

```bash
git add opti_chess/exploration.cpp opti_chess/main_gui.h opti_chess/gui.cpp opti_chess/game_tree.cpp opti_chess/board.cpp
git commit -m "feat(search): node_map lifecycle - register/erase/clear (#11, populate-only)"
```

OFF-byte-identical argument: all new code is `g_tt_node_dag`-guarded except `node_map.clear()` on an empty map (no-op). With the toggle ON the map is populated and pruned but **no code reads a hit yet** → search behavior still byte-identical; only memory is touched. Linking is Task 3.

---

## Task 3: Link-on-create + per-traversal repetition re-verification (the DAG, sound from first existence)

This is the soundness-critical task: the moment edges start sharing nodes (link-on-create), the per-traversal repetition recheck must exist in the same commit, so there is never a committed state where sharing is unsound (even behind the toggle).

**Files:**
- Modify: `opti_chess/exploration.cpp` — `explore_new_move` non-draw branch (the `else` from Task 1/2); `explore_random_child` (lines 554-592)

- [ ] **Step 1: Link-on-create in explore_new_move (hit path)**

After Task 2, the non-draw `else` branch ends with the `if (g_tt_node_dag) node_map[...] = child;` registration. Replace that whole `else` branch with the hit/miss split:
```cpp
		// Sinon, on crée un nouveau noeud normalement
		else {
			new_board->get_zobrist_key();

			// #11 Plan B — link-on-create. Si une position de meme cle Zobrist
			// est deja un Node VIVANT, on lie l'arete a ce Node partage au lieu
			// de recreer un sous-arbre. La branche nulle (au-dessus) fabrique
			// toujours une feuille distincte — jamais partagee. OFF : saute tout.
			Node* shared = nullptr;
			if (g_tt_node_dag) {
				const auto it = node_map.find(new_board->_zobrist_key);
				if (it != node_map.end()) {
					shared = it->second;
				}
			}

			if (shared != nullptr) {
				// Hit : pas d'allocation. On rend le board pris au buffer et on
				// pointe vers le Node existant ; add_child (plus bas, :504) lie
				// l'arete et incremente shared->_parent_count. created_new_node
				// reste false → pas de probe TT Plan A sur un noeud partage.
				if (new_board->_buffer_index >= 0 && !monte_board_buffer._bulk_resetting) {
					monte_board_buffer.free_index(new_board->_buffer_index);
				}
				child = shared;
			}
			else {
				// Miss : création normale + enregistrement dans node_map.
				child = monte_node_buffer.get_first_free_node();

				if (child == nullptr)
					return;

				child->_board = new_board;
				created_new_node = true;

				if (g_tt_node_dag) {
					node_map[new_board->_zobrist_key] = child;
				}
			}
		}
```

Note on the existing flow below this branch (unchanged): `if (child == nullptr) return;` still holds (a hit gives a non-null `shared`). The Plan-A probe at `:455` is `g_tt_main_search && created_new_node`; a hit keeps `created_new_node == false`, so it is correctly skipped. `if (!child->_fully_explored)` then refines a not-yet-finished shared node via quiescence (legitimate); a finished shared node is consumed as-is. `add_child(child, move)` at `:504` (run because `!already_explored`) links the edge and does `child->_parent_count++` — the DAG multi-parent link.

- [ ] **Step 2: Per-traversal repetition re-verification in explore_random_child**

Current `explore_random_child` head + descent (`exploration.cpp:554-592`):
```cpp
void Node::explore_random_child(BoardBuffer* board_buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network, PositionHistory *path_history) {

	// Prend un fils aléatoire
	const Move move = pick_random_child(alpha, beta, gamma);
	ChildLink& child_link = _children[move];
	Node *child = child_link._node;
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
Insert the recheck immediately after the `const int initial_child_nodes = child_link._propagated_nodes;` line and before the `// Explore le fils` block:
```cpp

	// #11 Plan B — soundness du DAG. Le sous-arbre du fils peut etre PARTAGE :
	// cree via un chemin A, descendu ici via un chemin B. Le statut de nulle
	// par repetition est PATH-LOCAL (il n'est pas dans le Node). On le
	// re-derive contre l'historique du chemin COURANT avant de descendre.
	// Si nulle sur ce chemin : on NE descend PAS dans le sous-arbre partage
	// et on NE le mute PAS ; l'arete contribue une valeur de nulle au backup
	// du parent (meme valeur que la branche nulle de explore_new_move). OFF :
	// saute (comportement arbre actuel — explore_random_child ne re-testait
	// jamais la repetition sur un fils existant).
	if (g_tt_node_dag && position_is_draw_by_repetition(branch_history, *child->_board)) {
		_deep_evaluation.reset();
		_deep_evaluation._value = 0.0;
		_deep_evaluation._evaluated = true;
		_iterations++;
		return;
	}
```

Rationale (sound, no mutation of the shared node): on a draw-by-repetition along the *current* path we do not call `child->grogros_zero` (no descent, no mutation of the shared subtree), and the parent's value for this traversal is a draw (`_value = 0.0`), which is exactly the value a fresh repetition leaf carries in `explore_new_move`'s draw branch. `_iterations++` keeps the iteration accounted (mirrors the terminal-draw child getting `_iterations = 1`); `_nodes` is intentionally not grown (no new unique work). This is conservative and chess-correct: a position reached by a repeating line is scored 0 on that line without corrupting the shared structure other paths rely on.

- [ ] **Step 3: Build**

Build command. Expected `EXITCODE=0`, no errors.

- [ ] **Step 4: Commit**

```bash
git add opti_chess/exploration.cpp
git commit -m "feat(search): DAG link-on-create + per-traversal repetition recheck (#11)"
```

OFF-byte-identical argument: both additions are fully `g_tt_node_dag`-guarded. The hit path only triggers on a `node_map` find (empty/untouched when OFF). The recheck's `if (g_tt_node_dag && ...)` short-circuits before `position_is_draw_by_repetition` runs. With OFF, `explore_new_move` always takes the miss/create path (identical to pre-Task-1 code) and `explore_random_child` descends exactly as today.

---

## Task 4: Relax tree-assumption `_nodes` guards under the DAG

Under sharing, a child's propagated node count can legitimately exceed a single parent's running cumulative `_nodes` (the same subtree is counted under multiple parents). The two diagnostic `cout`s assume a tree and would spam the console in DAG mode. Silence them only when the DAG is ON; keep them firing in tree mode (they still catch real tree bugs).

**Files:**
- Modify: `opti_chess/exploration.cpp` — `explore_new_move` guard (lines 391-393); `explore_random_child` guard (lines 564-566)

- [ ] **Step 1: Guard the explore_new_move diagnostic**

`exploration.cpp:391-393`:
```cpp
		if (_nodes <= child_link->_propagated_nodes) {
			cout << "child nodes >= nodes???" << endl;
		}
```
Replace with:
```cpp
		if (!g_tt_node_dag && _nodes <= child_link->_propagated_nodes) {
			cout << "child nodes >= nodes???" << endl; // tree-only : faux sous DAG (sous-arbre partage multi-parent)
		}
```

- [ ] **Step 2: Guard the explore_random_child diagnostic**

`exploration.cpp:564-566`:
```cpp
	if (child_link._propagated_nodes >= _nodes) {
		cout << "child nodes >= nodes in random exploration??? main position: " << _board->to_fen() << ", child position: " << child->_board->to_fen() << endl;
	}
```
Replace with:
```cpp
	if (!g_tt_node_dag && child_link._propagated_nodes >= _nodes) {
		cout << "child nodes >= nodes in random exploration??? main position: " << _board->to_fen() << ", child position: " << child->_board->to_fen() << endl; // tree-only : faux sous DAG
	}
```

- [ ] **Step 3: Build**

Build command. Expected `EXITCODE=0`, no errors.

- [ ] **Step 4: Commit**

```bash
git add opti_chess/exploration.cpp
git commit -m "refactor(search): silence tree-only _nodes guards under the DAG (#11)"
```

OFF-byte-identical argument: `!g_tt_node_dag` is `true` when OFF, so the conditions and the `cout` side effects are exactly as before.

---

## Task 5: Recursion-depth safety guard in grogros_zero (spec §7, Approach 1)

The repetition argument (a cycle is all-reversible ⇒ cut at the repetition limit via Task 3) bounds DAG recursion. Add a cheap belt-and-braces recursion-depth guard so a pathological non-repetition recursion can never run away. Active only when the DAG is ON; OFF is byte-identical.

**Files:**
- Modify: `opti_chess/exploration.cpp` — file-scope counter near the globals (`:1856-1862`); `grogros_zero` entry (after the `iterations <= 0` guard, `:282-285`)

- [ ] **Step 1: Add a file-scope recursion-depth counter + RAII guard**

Immediately after the two Task-1 global definitions (`bool g_tt_node_dag = false;` / `robin_map<uint64_t, Node*> node_map;` near `exploration.cpp:1862`), insert:
```cpp

// #11 Plan B §7 — garde-fou anti-runaway. Constante haute (> toute profondeur
// reelle plausible) : ne se declenche QUE sur une recursion pathologique
// non-repetition (la repetition est deja coupee par le recheck Task 3).
static int g_dag_recursion_depth = 0;
constexpr int DAG_MAX_RECURSION_DEPTH = 1024;

struct DagRecursionScope {
	DagRecursionScope() { g_dag_recursion_depth++; }
	~DagRecursionScope() { g_dag_recursion_depth--; }
	DagRecursionScope(const DagRecursionScope&) = delete;
	DagRecursionScope& operator=(const DagRecursionScope&) = delete;
};
```
(`g_dag_recursion_depth`/`DagRecursionScope` are file-local — single-threaded engine, no header declaration needed. They must be defined *above* `grogros_zero` is *used* recursively; file-scope statics defined at `:1862` are visible to `grogros_zero` only if that line precedes the function — it does NOT (`grogros_zero` is at `:273`). Therefore place this block instead immediately BEFORE `void Node::grogros_zero(` at `exploration.cpp:273`, not at `:1862`.)

- [ ] **Step 2: Apply the guard at grogros_zero entry**

`grogros_zero` entry (`exploration.cpp:281-285`):
```cpp
	// FIXME *** cela ne devrait pas arriver
	if (iterations <= 0) {
		cout << "iterations <= 0 in grogros_zero" << endl;
		return;
	}
```
Immediately after that block (before `// Temps de calcul` / `const clock_t begin_monte_time = clock();` at `:287-288`), insert:
```cpp

	// #11 Plan B §7 — borne de securite sur la profondeur de recursion DAG.
	// OFF : jamais arme. ON : la repetition (Task 3) coupe deja tout cycle ;
	// ceci n'attrape qu'une recursion pathologique non-repetition.
	if (g_tt_node_dag) {
		if (g_dag_recursion_depth >= DAG_MAX_RECURSION_DEPTH) {
			return;
		}
	}
	DagRecursionScope _dag_rec_scope; // no-op de cout quand OFF (juste ++/--)
```
(`DagRecursionScope` is unconditional but its only effect is an `int` ++/-- ; to keep OFF *byte*-identical in spirit and avoid even that, gate the increment: replace the unconditional `DagRecursionScope _dag_rec_scope;` with the block below.)

Use this exact form for the inserted code instead:
```cpp

	// #11 Plan B §7 — borne de securite sur la profondeur de recursion DAG.
	// OFF : jamais arme (ni compteur ni test). ON : la repetition (Task 3)
	// coupe deja tout cycle ; ceci n'attrape qu'une recursion pathologique.
	if (g_tt_node_dag && g_dag_recursion_depth >= DAG_MAX_RECURSION_DEPTH) {
		return;
	}
	g_dag_recursion_depth += g_tt_node_dag ? 1 : 0;
	struct DagRecGuard {
		~DagRecGuard() { g_dag_recursion_depth -= g_tt_node_dag ? 1 : 0; }
	} _dag_rec_guard;
```
Drop the `DagRecursionScope` struct from Step 1 (keep only `static int g_dag_recursion_depth = 0;` and `constexpr int DAG_MAX_RECURSION_DEPTH = 1024;`); the balanced guard is the local `DagRecGuard` above. Net OFF effect: one `int` read in the `if` (false) and two `g_tt_node_dag ? :` evaluating to 0 — no state change, no output. (If strict byte-identity of generated code is demanded, this is behaviorally identical though not instruction-identical; the user gate validates OFF == baseline functionally.)

- [ ] **Step 3: Build**

Build command. Expected `EXITCODE=0`, no errors.

- [ ] **Step 4: Commit**

```bash
git add opti_chess/exploration.cpp
git commit -m "feat(search): DAG recursion-depth safety guard in grogros_zero (#11)"
```

OFF behavioral argument: when `g_tt_node_dag == false` the early-return condition is `false`, the counter delta is `0`, and the guard destructor delta is `0` — no observable state change, no output. Functionally identical to baseline (validated by the user gate).

---

## Task 6: [USER] Acceptance gate + BUGFIXES.md

No code. User-run; this is the B-2 acceptance gate.

- [ ] **Step 1: [USER] Build in Visual Studio** as usual (IDE resolves `v145`).

- [ ] **Step 2: [USER] OFF byte-identical** — with the toggle **OFF** (do not press `O`), confirm PERFT 1/2 + EVALUATION are identical to the post-B-1 baseline on the usual positions. Any divergence ⇒ an OFF-guard leak; report which position; do not proceed.

- [ ] **Step 3: [USER] ON behavior** — press `O` (HUD shows `TT node DAG : true`), on the reference position (king + blocked pawns, Lague reference), fixed iterations, same FEN. Record: `node_map.size()` (unique nodes), `get_main_depth`, best move, time. Expectation vs OFF at equal budget: **far fewer unique nodes per depth and a clear depth gain (toward 8→30), same best move**. Also confirm a known perpetual-check / repetition position is still scored as a draw with the DAG ON (Task 3 soundness).

- [ ] **Step 4: [USER] Regression gate** — PERFT 1/2 + EVALUATION stable with the toggle ON.

- [ ] **Step 5: Record + commit**

Update `opti_chess/BUGFIXES.md` #11: mark Plan B (DAG) implemented, with the measured OFF==baseline confirmation and the ON unique-node/depth numbers and best move.
```bash
git add opti_chess/BUGFIXES.md
git commit -m "docs(bugfixes): #11 Plan B DAG implemented - measured node/depth gain"
```

---

## Self-Review

**Spec coverage** (spec §1-§9):
- §1 node identity map → Task 1 (`node_map` global, distinct from `transposition_table`).
- §2 link-on-create → Task 3 Step 1 (hit links via existing `add_child`; miss registers).
- §3 per-traversal repetition recheck → Task 3 Step 2 (no shared-node mutation; draw value to parent backup).
- §4 O(1) path history → done in B-1 (prerequisite; this plan depends on the threaded `PositionHistory`/`PathScope`).
- §5 lifecycle/recycling → Task 2 (erase in `recycle_detached_node`, `node_map.clear()` at all reset sites); `_parent_count` already correct (no change, documented in Foundations).
- §6 `_nodes` accounting → Task 4 (relax tree-only guards under DAG); bench metric `node_map.size()` / `get_main_depth` → Task 6 Step 3.
- §7 cycle/recursion safety → repetition bound via Task 3 + depth guard Task 5.
- §8 gating & wiring → Task 1 (same scheme as `g_tt_main_search`: extern/def/GUI member/KEY_O/HUD/mirror).
- §9 invariants → OFF-byte-identical argument in every code task; selection (`pick_random_child`) untouched; nulls always distinct leaves (never shared — `explore_new_move` draw branch unchanged); no leak (Task 2). No spec gap.

**Placeholder scan:** none. Every code step has exact before/after with real code; file sites cited by current line with a content-locate fallback for drift; `KEY_O` verified unused in `main_gui.h`; `robin_map<uint64_t, Node*>` matches the codebase's existing unqualified `robin_map` style (`exploration.h:43`).

**Type consistency:** `g_tt_node_dag : bool` (extern h / def cpp / GUI `_tt_node_dag` mirror) consistent across Tasks 1,3,4,5. `node_map : robin_map<uint64_t, Node*>` declared Task 1, used Tasks 2-3 with `find`/`erase`/`operator[]`/`clear` only. `add_child(Node*, Move)` reused unchanged (already `_parent_count++`). `position_is_draw_by_repetition(const PositionHistory&, Board&)` reused with `branch_history` (the B-1 threaded `PositionHistory&`) — signature consistent with B-1. `recycle_detached_node` uses `node->_board->_zobrist_key` consistent with the registration key in `explore_new_move`. `g_dag_recursion_depth : int` / `DAG_MAX_RECURSION_DEPTH : constexpr int` defined once (Task 5 Step 1, relocated above `grogros_zero`), used Task 5 Step 2.

**Risk note:** the single soundness risk is §3 (a shared subtree traversed via a drawing path). Task 3 keeps it sound by never mutating the shared node and backing up a draw value; Task 6 Step 3 explicitly validates a repetition/perpetual position is scored as a draw with the DAG ON. The other risk is an OFF-guard leak; every code task carries an explicit OFF-byte-identical argument and Task 6 Step 2 is the hard gate.
