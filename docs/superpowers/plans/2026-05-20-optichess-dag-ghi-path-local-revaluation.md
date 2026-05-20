# DAG GHI — path-local revaluation (Approche A) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the two open #11 repros (KP(h)-vs-K false-win, won blocked-pawns phantom-draw) under `g_tt_node_dag == true` by honestly computing a path-local node value: a minimax over the *non-cycle-touch* children plus `dag_draw_eval()` as a single virtual draw candidate, gated by strict full-enumeration (`children_count() >= _got_moves`). The path-local value bubbles up via an `Evaluation* path_local_eval` + `bool* path_local_emitted` channel; persistence into `_deep_evaluation` only at unshared nodes (`_parent_count <= 1`). No selection / `get_best_score_move` / `pick_random_child` / `get_node_score` / `get_move_scores` change — this is the precise difference from the reverted Plan B bubble approach.

**Architecture:** Refinement of the 2026-05-19 bubble design (which regressed and was reverted, commit `0608af5`). The §3 cycle-touch predicate `position_is_draw_by_repetition(branch_history, *M->_board)` stays unchanged — only its *interpretation* changes: cycle-touch is "this child's shared `_deep_evaluation` is untrustworthy on this path", NOT "this child is a forced draw". Path-local backup at node N **skips** cycle-touch children from the minimax set and admits `dag_draw_eval()` as one additional virtual candidate (side-to-move's draw option). Computation lives in a new helper `Node::compute_path_local_eval(...)`, called once per `explore_random_child` refinement iteration immediately after the normal backup. Per-child substitution carries deep overrides through shared interior nodes (`_parent_count > 1`) via the out-param chain (no shared mutation). Plan A TT writeback is guarded so a path-local persisted value never leaks into the flat TT.

**Tech Stack:** C++17, MSVC (`opti_chess.sln`), single-threaded engine. No automated test framework for GrogrosZero search behaviour — "test" = a clean MSBuild (`EXITCODE=0`, no `error` lines) plus a written OFF-byte-identical argument per task, then one final manual **[USER]** acceptance gate (the assistant cannot run the raylib GUI). This is the established acceptance pattern for #11.

Design spec: `docs/superpowers/specs/2026-05-20-optichess-dag-ghi-path-local-revaluation-design.md`.

---

## Build command

Use Variant B (Debug x64 Insiders MSBuild) — confirmed to exist on this machine. Run from the project working dir in PowerShell:

```
& 'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe' opti_chess.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m /nologo /clp:ErrorsOnly /v:m; "EXITCODE=$LASTEXITCODE"
```

Success = `EXITCODE=0` and no `error` lines. Do NOT modify any `.vcxproj`/`.sln` file.

Commits: English, ASCII, conventional, `(#11)` ref, **no AI attribution**. French comments use real Unicode (é, è, à, ç, ê, î, ô, û, ù, œ). Do not commit `opti_chess/TODO_list.txt`, `opti_chess/Tests.txt`, `opti_chess/Tests.txt+tmp`, or any prior plan markdown files.

Baseline: tip `6e8c030` (post-revert, functionally identical to `e3e7baa`).

---

## File Structure

All code changes live in three existing files (no new files):

- `opti_chess/board.h` — one tiny additive edit: header-only inline `operator==(const Evaluation&)` (only if not already present), comparing `_value` and `_avg_score` (the change-detection-relevant fields).
- `opti_chess/exploration.h` — three signature edits (`grogros_zero`, `explore_random_child`, new `Node::compute_path_local_eval` declaration).
- `opti_chess/exploration.cpp` — matching definitions, the helper body, the §4.4 emission/persistence block, the §4.5 TT writeback guard, and the descent-recursion caller-init pattern.

Reference (current tip `6e8c030`, verified line numbers from baseline):
- `exploration.h:135` `children_count()` decl · `:150` `grogros_zero` decl · `:153` `explore_new_move` decl · `:156` `explore_random_child` decl · `:205` `get_best_score_move` decl (UNCHANGED in this plan) · `:202` `get_node_score` decl (UNCHANGED).
- `exploration.cpp:154` `static Evaluation dag_draw_eval()` · `:257` `children_count()` def · `:356` `grogros_zero` def · `:430` `DagExcl dag_excl;` · `:445` `explore_new_move` call (UNCHANGED) · `:450` `explore_random_child` call · `:467-475` `grogros_zero` tail · `:479` `explore_new_move` def · `:726` `explore_random_child` def · `:790-799` §3 cut block · `:801-805` descent block · `:807-827` backup region · `:847` Plan A TT writeback.
- `board.h:461` `struct Evaluation`. (No `operator==` exists — Task 2 adds one.)

Task order: design gate (no code) → header-only `operator==` + plumbing (signatures + helper decl, all defaults preserve byte-identicality) → helper body (still inert, no non-null caller) → atomic soundness (descent caller-init + emission/persistence + TT guard, all `g_tt_node_dag`-gated) → USER acceptance gate.

---

## Task 1: Design-validation gate (no code)

**Files:** none (this plan, annotated).

Confirm assumptions before any hot-path edit. The previous attempt that violated several of these assumptions regressed and was reverted.

- [ ] **Step 1: Confirm the build invocation.** Variant B (`C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe`, Debug x64) is confirmed to exist on this machine and is the version to use for every subsequent task's build.

- [ ] **Step 2: Confirm `Evaluation::operator==` status.** Grep `operator==` in `opti_chess/board.h`. Confirm NO existing `operator==(const Evaluation&)` definition — only `Move`, `CastlingRights`, `Pos`, `Board`. Task 2 will add a header-only inline `bool operator==(const Evaluation&) const { return _value == other._value && _avg_score == other._avg_score; }` inside `struct Evaluation` (board.h:461-499 region). This is the *change-detection-relevant* equality used at §4.4 to decide whether to write `_deep_evaluation = node_local`.

- [ ] **Step 3: Confirm `children_count()` and `_got_moves` access.** `Node::children_count()` is declared at `exploration.h:135`, defined at `exploration.cpp:257` (returns `_children.size()`). `Board::_got_moves` is the legal move count cached on the board. The strict enumeration gate `children_count() >= _board->_got_moves` is computable inside `compute_path_local_eval(...) const` (the helper is `const`; both accessors are const-correct).

- [ ] **Step 4: Confirm `position_is_draw_by_repetition(branch_history, *child->_board)` semantics.** The §3 cut at `exploration.cpp:790-799` already calls this predicate inside `explore_random_child` against `branch_history` (the threaded `PositionHistory*`). The helper takes a `const PositionHistory&` parameter so it can call the same predicate on each child. Confirm the predicate is `const`-safe.

- [ ] **Step 5: Confirm `dag_draw_eval()` is reusable.** `static Evaluation dag_draw_eval()` at `exploration.cpp:154` is the canonical draw value (position-independent, copied from `board.cpp:1551` draw branch). The helper will call it once per invocation to get the virtual draw candidate.

- [ ] **Step 6: Confirm Plan A TT writeback site.** `transposition_table.store(...)` after the backup region (`exploration.cpp:847` baseline). Currently guarded by `g_tt_main_search && !_is_terminal && !_is_stand_pat_eval && _deep_evaluation._evaluated`. Task 5 will add `&& !(g_tt_node_dag && path_local_persisted_this_iter)` (additive, OFF-inert).

- [ ] **Step 7: Confirm color/minimax convention.** `Node::get_best_score_move` reads `int color = _board->get_color();` and compares `child->_deep_evaluation._value * color`. The new helper uses the SAME convention: maximize `ev._value * color` over `S_nc ∪ {dag_draw_eval()}`. `dag_draw_eval()._value` is 0 (mirror of the board.cpp draw branch).

- [ ] **Step 8: Confirm signature placement.** All new params on `grogros_zero` / `explore_random_child` are *appended* at the end with default values (`nullptr` / `false`), so every existing call site at ≤ current arg count remains source-compatible. Existing root callers (`gui.cpp`) pass nothing → defaults bind → inert.

- [ ] **Step 9: No commit (design only).**

---

## Task 2: Inert plumbing (signatures + `operator==` + helper decl)

Behaviour-preserving. Add the two out-params to `grogros_zero` and `explore_random_child` (defaults `nullptr`/`false`); declare the new helper; add header-only `operator==` for `Evaluation`. No callers pass non-null values → byte-identical ON and OFF.

**Files:**
- Modify: `opti_chess/board.h` (one additive `operator==`)
- Modify: `opti_chess/exploration.h` (three decls)
- Modify: `opti_chess/exploration.cpp` (three matching def signatures)

- [ ] **Step 1: `board.h` — add inline `Evaluation::operator==`.** Inside `struct Evaluation { … };` (line 461 region), immediately after the existing `Evaluation& operator=(const Evaluation& other) { … }` block and before the closing `};`, add:

```cpp
	// #11 GHI — change-detection equality (cf. design 2026-05-20 §4.4).
	// Compare uniquement les champs pertinents pour decider d'une re-ecriture
	// de _deep_evaluation : _value et _avg_score. Ce n'est PAS une egalite
	// structurelle complete.
	inline bool operator==(const Evaluation& other) const {
		return _value == other._value && _avg_score == other._avg_score;
	}
```

(French Unicode: `décider`, `re-écriture`, `n'est`, `égalité` — write them with real accents.)

- [ ] **Step 2: `exploration.h` — `grogros_zero` decl.** Replace the current trailing `, PositionHistory *path_history = nullptr);` with the two new out-params:

```cpp
	void grogros_zero(BoardBuffer* board_buffer, Evaluator* eval, const double alpha, const double beta, const double gamma, int nodes, int quiescence_depth, Network* network = nullptr, PositionHistory *path_history = nullptr, Evaluation* path_local_eval = nullptr, bool* path_local_emitted = nullptr);
```

- [ ] **Step 3: `exploration.h` — `explore_random_child` decl.** Append the two new out-params after the existing `DagExcl* dag_excl = nullptr` (which stays):

```cpp
	void explore_random_child(BoardBuffer* board_buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network = nullptr, PositionHistory *path_history = nullptr, DagExcl* dag_excl = nullptr, Evaluation* path_local_eval = nullptr, bool* path_local_emitted = nullptr);
```

- [ ] **Step 4: `exploration.h` — declare `compute_path_local_eval`.** Add inside the `Node` class (place it near `get_best_score_move` at `:205` or in the natural grouping of search helpers; pick a coherent local spot and report it in the commit). The declaration:

```cpp
	// #11 GHI path-local revaluation (cf. design 2026-05-20 §3, §4.4).
	// Calcule la valeur path-locale de ce noeud sur la traversee courante :
	// minimax sur les fils NON cycle-touch (= dont _deep_evaluation est de
	// confiance sur ce chemin) plus dag_draw_eval() comme candidat virtuel
	// representant l'option du trait de jouer une nulle volontaire. Renvoie
	// true et ecrit *out si la porte d'enumeration stricte est satisfaite
	// (children_count() >= _got_moves) ; sinon false (appelant garde son
	// fallback). Substitution d'UN fils (substitution_move != null Move) :
	// utiliser substitution_value pour ce fils au lieu de son
	// _deep_evaluation partage -- essentiel pour faire bubbler une valeur
	// raffinee profonde a travers des noeuds interieurs partages
	// (_parent_count > 1) dont _deep_evaluation ne peut etre persiste.
	// L'appelant garantit g_tt_node_dag == true.
	bool compute_path_local_eval(
		const PositionHistory& path_history,
		const Move& substitution_move,
		const Evaluation& substitution_value,
		Evaluation* out) const;
```

(Write real Unicode accents in the comment: `Calcule`, `traversée`, `confiance`, `représentant`, `Renvoie`, `écrit`, `énumération`, `appelant`, `fallback`, `Substitution`, `essentiel`, `bubbler`, `valeur`, `raffinée`, `à travers`, `intérieurs`, `partagés`, `persisté`. The block above shows ASCII placeholders for the spec text; the implementer MUST substitute the real accents.)

- [ ] **Step 5: `exploration.cpp:356` — `grogros_zero` def signature.** Match the header. Final:

```cpp
void Node::grogros_zero(BoardBuffer* board_buffer, Evaluator* eval, const double alpha, const double beta, const double gamma, int iterations, int quiescence_depth, Network* network, PositionHistory *path_history, Evaluation* path_local_eval, bool* path_local_emitted) {
```

- [ ] **Step 6: `exploration.cpp:726` — `explore_random_child` def signature.** Match the header. Final:

```cpp
void Node::explore_random_child(BoardBuffer* board_buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network, PositionHistory *path_history, DagExcl* dag_excl, Evaluation* path_local_eval, bool* path_local_emitted) {
```

- [ ] **Step 7: Build.** Expected `EXITCODE=0`, no `error` lines. (`compute_path_local_eval` declared but undefined will NOT yet link-fail because there are no callers in this commit. If MSBuild reports an undefined-reference because the linker eagerly references it via vtable etc., make sure the decl alone does not force a TU. If it does, defer Step 4 to Task 3 where the body lands together with the decl; report it as a sub-step adjustment.)

- [ ] **Step 8: Commit.**

```bash
git add opti_chess/board.h opti_chess/exploration.h opti_chess/exploration.cpp
git commit -m "feat(search): plumbing for DAG GHI path-local revaluation - signatures, operator==, helper decl (#11, inert)"
```

OFF-byte-identical argument: every new param defaults to `nullptr`/`false`; no caller passes a non-null. ON and OFF: no observable change. `Evaluation::operator==` is a new method but it is not yet called from anywhere.

---

## Task 3: Implement `compute_path_local_eval` body (still inert)

Define the helper. Still no caller passes non-null out-params → still byte-identical ON and OFF until Task 4.

**Files:**
- Modify: `opti_chess/exploration.cpp` (add definition; place it near `get_best_score_move` or just before/after it for locality).

- [ ] **Step 1: Implement the body.** Algorithm (matching the design spec §3.2):

```cpp
// #11 GHI path-local revaluation helper (cf. design 2026-05-20 §3, §4.4).
// Voir la declaration dans exploration.h pour le contrat detaille.
bool Node::compute_path_local_eval(
	const PositionHistory& path_history,
	const Move& substitution_move,
	const Evaluation& substitution_value,
	Evaluation* out) const {

	// Defensive : pas d'enfants -> pas d'override.
	if (_children.empty()) {
		return false;
	}

	// Porte d'enumeration stricte (§3.2 step 1). Tant que tous les coups
	// legaux ne sont pas representes dans _children, on ne peut PAS prouver
	// "tous les coups menent a une repetition" (le coup gagnant peut etre
	// celui pas encore expanse). Aucun override.
	if (children_count() < static_cast<size_t>(_board->_got_moves)) {
		return false;
	}

	const int color = _board->get_color();

	// Minimax sur S_nc (fils non cycle-touch) plus dag_draw_eval() comme
	// candidat virtuel (§3.2 step 3). dag_draw_eval()._value == 0 ; le
	// candidat virtuel est admis UNE SEULE fois, pas par fils cycle-touch.
	Evaluation best;
	bool best_seen = false;

	for (auto const& [move, child_link] : _children) {
		const Node* child = child_link._node;

		// Cycle-touch ? -> skip de S_nc (NE PAS scorer dag_draw_eval ici --
		// difference critique avec le Plan B reverte 0608af5).
		if (position_is_draw_by_repetition(path_history, *child->_board)) {
			continue;
		}

		// Substitution pour le fils tout juste descendu (§3.2 step 3).
		const Evaluation& contrib =
			(!substitution_move.is_null_move() && move == substitution_move)
				? substitution_value
				: child->_deep_evaluation;

		if (!best_seen || contrib._value * color > best._value * color) {
			best = contrib;
			best_seen = true;
		}
	}

	// Candidat virtuel "nulle volontaire" -- une seule fois.
	const Evaluation draw = dag_draw_eval();
	if (!best_seen || draw._value * color > best._value * color) {
		best = draw;
		best_seen = true;
	}

	// best_seen == true ici par construction (le candidat virtuel garantit
	// au moins un candidat).
	*out = best;
	return true;
}
```

Real Unicode accents in comments: `défensive`, `Énumération`, `légaux`, `représentés`, `mènent`, `répétition`, `coup gagnant`, `expansé`, `différence`, `réverté`, `tout juste descendu`, `virtuel`, `nulle volontaire`, `Garantit`, `construction`. The block above shows ASCII placeholders for spec text; substitute real accents.

(Note on signed/unsigned: `_board->_got_moves` is `uint8_t` per board.h; cast to `size_t` for the comparison to avoid a sign warning. If `children_count()` returns `size_t`, the `static_cast<size_t>` is correct; if it returns something else, adjust to match. Verify by reading `exploration.h:135`.)

(Note on `S_nc == ∅`: when every legal child is cycle-touch, `best_seen` stays false through the loop. Then the virtual draw candidate sets `best = draw; best_seen = true;` — the design's "all legal moves are cycle-touch ⇒ emit draw" rule is implemented by the same fallthrough. ✓)

- [ ] **Step 2: Build.** Expected `EXITCODE=0`, no `error` lines.

- [ ] **Step 3: Commit.**

```bash
git add opti_chess/exploration.cpp
git commit -m "feat(search): implement compute_path_local_eval helper for GHI path-local revaluation (#11, inert)"
```

OFF-byte-identical argument: the new function is defined but never called (no consumer until Task 4). Compilation produces a new symbol but no runtime path reaches it. Behaviourally identical ON and OFF.

---

## Task 4: Atomic soundness — descent caller-init + §4.4 emission/persistence + TT writeback guard

The soundness-critical commit. Three pieces land together so the path-local channel is sound from the first committed moment it exists.

**Files:**
- Modify: `opti_chess/exploration.cpp` (the descent block in `explore_random_child`, the backup region §4.4, and the Plan A TT writeback site §4.5).

- [ ] **Step 1: Descent caller-init in `explore_random_child`.** Locate (Grep) the descent block — the one ending with `child->grogros_zero(...)`. Currently:

```cpp
	// Explore le fils
	{
		PathScope _ps(branch_history, *child->_board);
		child->grogros_zero(board_buffer, eval, alpha, beta, gamma, 1, quiescence_depth, network, &branch_history); // L'évaluation du fils est mise à jour ici
	}
```

Replace with (real Unicode accents in the comment):

```cpp
	// Explore le fils. #11 GHI — caller-init pour le canal path-local
	// (design 2026-05-20 §4.3) : fallback safe = _deep_evaluation partagé,
	// flag d'émission initialisé false. Si la passe récursive du fils
	// satisfait la porte d'énumération stricte, elle écrasera child_local
	// et marquera child_local_emitted = true ; sinon les deux out-params
	// restent au fallback. OFF / hors DAG : les pointeurs sont nullptr ->
	// aucune écriture, aucun effet.
	Evaluation child_local = child->_deep_evaluation;
	bool child_local_emitted = false;
	{
		PathScope _ps(branch_history, *child->_board);
		child->grogros_zero(board_buffer, eval, alpha, beta, gamma, 1, quiescence_depth, network, &branch_history, g_tt_node_dag ? &child_local : nullptr, g_tt_node_dag ? &child_local_emitted : nullptr); // L'évaluation du fils est mise à jour ici
	}
```

(Replace the trailing argument list of `child->grogros_zero(...)` with the new last two args. Keep the inline comment "L'évaluation du fils est mise à jour ici" on the same line as before.)

- [ ] **Step 2: §4.4 emission/persistence block — after the normal backup.** Locate (Grep) the backup region. Current baseline (`6e8c030`, post-Task-2-then-3-undone) reads:

```cpp
	// Met à jour l'évaluation du plateau avec le meilleur coup
	const Move _bm = get_best_score_move(alpha, beta);
	if (g_tt_node_dag && dag_excl != nullptr && dag_excl->contains(_bm)) {
		// Bug 1 opt 1 / spec §3 — le meilleur coup est une arete coupee comme
		// nulle path-locale sur CE chemin (DagExcl rempli par les coupes §3 de
		// ce frame). Sa vraie valeur ici est une NULLE, pas son
		// _deep_evaluation partage (calcule via un autre chemin). On remonte
		// la nulle par l'out-param. Persistance sur this->_deep_evaluation
		// SEULEMENT si this n'est pas partage sur cette traversee
		// (_parent_count<=1) : ecrire une nulle path-locale sur un noeud
		// partage corromprait les autres chemins (invariant 772183a). Le
		// backup normal (else) reste STRICTEMENT inchange (pas de regression
		// du backup MCTS des noeuds partages).
		const Evaluation _draw = dag_draw_eval();
		if (path_local_eval != nullptr) *path_local_eval = _draw;
		if (_parent_count <= 1) _deep_evaluation = _draw;
	}
	else {
		_deep_evaluation = _children[_bm]._node->_deep_evaluation;
		if (g_tt_node_dag && path_local_eval != nullptr) *path_local_eval = _deep_evaluation;
	}
```

The opt-1 partial dag_excl branch (`if (g_tt_node_dag && dag_excl != nullptr && dag_excl->contains(_bm))`) and the else branch's `path_local_eval` write are SUPERSEDED by the §4.4 design. Replace the entire if/else region with:

```cpp
	// Met à jour l'évaluation du plateau avec le meilleur coup. Backup MCTS
	// normal — partagé, indépendant du chemin. SÉMANTIQUEMENT INCHANGÉ par
	// rapport au baseline (sélection et calcul identiques OFF).
	const Move _bm = get_best_score_move(alpha, beta);
	_deep_evaluation = _children[_bm]._node->_deep_evaluation;
	bool path_local_persisted_this_iter = false;

	// #11 GHI path-local revaluation (design 2026-05-20 §4.4). Calcule la
	// valeur path-locale de CE nœud sur cette traversée via le helper, avec
	// substitution de la valeur ramenée par le fils tout juste descendu
	// (essentiel pour bubbler à travers des nœuds intérieurs partagés). La
	// porte d'énumération stricte (children_count() >= _got_moves) est
	// vérifiée dans le helper et bloque tout faux positif de
	// partial-expansion. L'émission via out-param bubble TOUJOURS (jamais de
	// mutation partagée). Persistance dans _deep_evaluation UNIQUEMENT si
	// _parent_count <= 1 (invariant 772183a). OFF / hors DAG : la garde
	// court-circuite à la première conjoncture.
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
- `move` is the `Move` already in scope at this point in `explore_random_child` (the move that led to `child`); reuse it as `sub_move` when `child_local_emitted == true`. When the child did not emit (gate not satisfied), `sub_move = Move()` (null Move) disables substitution and the helper falls back to each child's `_deep_evaluation`.
- `path_local_persisted_this_iter` is a local `bool` consumed by Step 3 (TT writeback guard). Declared even when the `if` is unreached so the guard's identifier is always in scope — but ONLY the DAG-gated branch ever sets it to true.
- The previous `if (g_tt_node_dag && dag_excl != nullptr && dag_excl->contains(_bm))` opt-1 direct-case branch is REMOVED. Rationale: §4.4 supersedes it — the §3 cut's `dag_excl->add(move)` still records the structural cut for `pick_random_child` skipping (opt-3 anti-spin), but the value channel now comes from the §4.4 helper, which correctly handles BOTH the opt-1 direct-case (the cycle-touch child is skipped from `S_nc`) AND the deep-bubble case (substitution). This is the only way to get sound path-local values; keeping both would double-count or contradict.

- [ ] **Step 3: §4.5 Plan A TT writeback guard.** Locate (Grep `transposition_table.store(` in `exploration.cpp`) — the writeback inside `explore_random_child`, around line 847 baseline. It currently looks like:

```cpp
	if (g_tt_main_search && !_is_terminal && !_is_stand_pat_eval && _deep_evaluation._evaluated) {
		transposition_table.store(_board->_zobrist_key, ...);
	}
```

Add the path-local guard:

```cpp
	if (g_tt_main_search && !_is_terminal && !_is_stand_pat_eval
		&& _deep_evaluation._evaluated
		&& !(g_tt_node_dag && path_local_persisted_this_iter)) {
		// #11 GHI — sous DAG, si on vient de persister une valeur path-locale
		// dans _deep_evaluation (uniquement quand _parent_count<=1, §4.4),
		// NE PAS écrire cette valeur dans la TT plate. La TT est
		// path-indépendante ; un leak path-local serait lu depuis d'autres
		// chemins. OFF (g_tt_node_dag==false) ou pas de persistance ce tour
		// (path_local_persisted_this_iter==false) -> garde inerte, writeback
		// identique au baseline.
		transposition_table.store(_board->_zobrist_key, ...);
	}
```

(Adjust the actual `store` arg list to match the baseline; do NOT alter any other writeback site or any other field. Only add the `&& !(g_tt_node_dag && path_local_persisted_this_iter)` conjunct and the comment.)

- [ ] **Step 4: Drop the now-dead opt-1 direct-case branch.** Confirm via Grep that no other site references the removed `dag_excl->contains(_bm)` value-substitution; only the §3 cut's `dag_excl->add(move)` and `pick_random_child`'s skip remain (opt-3 anti-spin, unchanged).

- [ ] **Step 5: Build.** Expected `EXITCODE=0`, no `error` lines.

- [ ] **Step 6: Commit.**

```bash
git add opti_chess/exploration.cpp
git commit -m "feat(search): GHI path-local revaluation - emission, persistence, TT guard (#11)"
```

OFF-byte-identical argument: Step 1's recursive args become `nullptr` under OFF (no out-param writes); Step 2's whole block is gated `g_tt_node_dag && path_local_eval != nullptr && path_history != nullptr` (all three false/null under OFF); Step 3's guard adds `&& !(g_tt_node_dag && false)` ≡ `&& true` under OFF (the writeback decision is unchanged). The removed opt-1 direct-case branch was DAG-only (`g_tt_node_dag &&` first conjunct) so its removal is also OFF-inert. ON: the new helper runs and emits/persists path-local overrides under the strict enumeration gate; cycle-touch children are skipped (not draw-scored) from the minimax. This is the design's intended ON behaviour.

---

## Task 5: [USER] Acceptance gate + docs

No code. User-run; assistant cannot run the raylib GUI.

- [ ] **Step 1: [USER] Build** (Visual Studio IDE or the confirmed MSBuild). Expected: success.

- [ ] **Step 2: [USER] OFF byte-identical.** Toggle **OFF** (do not press `O`). Confirm PERFT 1/2 + EVALUATION identical to the post-`6e8c030` baseline on the usual positions. Any divergence ⇒ an OFF-guard leak; report the position; stop.

- [ ] **Step 3: [USER] Repro 1 — no false win.** `8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7` and a KP(h)-vs-K theoretical draw, DAG **ON** (key `O`, Plan A `I` OFF), several batches. Expected: the engine no longer thinks it wins / circles indirectly; the root evaluation converges to a draw over iterations (MCTS convergence, not necessarily an instant flip).

- [ ] **Step 4: [USER] Repro 2 — no phantom draw.** The won "K vs K + blocked pawns" position, DAG **ON**. Expected: shows the win and **stays** won; the eval does not drift to a draw and does not flip draw↔win. (The separate `play_move_keep` re-root corruption cluster is out of scope.)

- [ ] **Step 5: [USER] Regression.** PERFT 1/2 + EVALUATION stable with the toggle **ON**.

- [ ] **Step 6: Record + commit docs.** Update `opti_chess/BUGFIXES.md` #11 (Approach A implemented; OFF==baseline confirmed; Repro 1/2 outcomes) and mark the design spec validated-by-gate.

```bash
git add opti_chess/BUGFIXES.md docs/superpowers/specs/2026-05-20-optichess-dag-ghi-path-local-revaluation-design.md
git commit -m "docs(bugfixes): #11 DAG GHI path-local revaluation - gate outcomes"
```

---

## Self-Review

**Spec coverage** (design `2026-05-20-...-revaluation-design.md`):
- §3.1 cycle-touch predicate unchanged → kept (Task 3 calls `position_is_draw_by_repetition` directly). ✓
- §3.2 strict enumeration gate → Task 3 Step 1 (`children_count() < _got_moves` → return false). ✓
- §3.2 `S_nc` skip (not score) of cycle-touch children → Task 3 Step 1 loop body's `continue`. ✓
- §3.2 single virtual draw candidate after the loop → Task 3 Step 1 post-loop block. ✓
- §3.2 per-child substitution → Task 3 Step 1 `(move == substitution_move) ? substitution_value : child->_deep_evaluation`. ✓
- §3.3 `get_best_score_move` UNCHANGED → no edit in this plan. ✓
- §3.3 selection (`pick_random_child` / `get_node_score` / `get_move_scores`) UNCHANGED → no edit. ✓
- §4.2 signatures → Task 2 Steps 2-3, 5-6. ✓
- §4.2 helper decl → Task 2 Step 4. ✓
- §4.3 caller-init `child_local`/`child_local_emitted` → Task 4 Step 1. ✓
- §4.4 emission via out-param chain → Task 4 Step 2 (`*path_local_eval = node_local; if (path_local_emitted) *path_local_emitted = true;`). ✓
- §4.4 persistence only `_parent_count <= 1` → Task 4 Step 2 (`if (_parent_count <= 1 && !(node_local == _deep_evaluation)) _deep_evaluation = node_local;`). ✓
- §4.5 TT writeback guard → Task 4 Step 3. ✓
- §5 OFF byte-identicality → per-task OFF arguments; all new behaviour gated `g_tt_node_dag` + non-null out-param. ✓
- §6 Perf — no allocation, one extra helper call per `explore_random_child` iteration, no change to selection. ✓
- §10 Acceptance gate → Task 5. ✓

**Differences from the reverted Plan B bubble plan (the explicit failure-class avoidance):**
- This plan does NOT add a `const DagExcl*` param to `get_best_score_move`. ✓
- This plan does NOT score cycle-touch children as `dag_draw_eval()` in any selection-ranking site. ✓
- This plan DOES introduce a strict enumeration gate (`children_count() >= _got_moves`) inside the helper — the single fix that blocks partial-expansion false positives. ✓
- This plan DOES implement per-child substitution to bubble through shared interior nodes — the structural omission that doomed the reverted bool-channel design. ✓
- This plan does NOT touch `pick_random_child` / `get_node_score` / `get_move_scores`. ✓

**Placeholder scan:** none. Every code step shows complete before/after against the current baseline (`6e8c030`); the design decisions (no `Evaluation::operator==` → add one; `_got_moves` cast; substitution semantics for null Move; `path_local_persisted_this_iter` local scoping) are explicit. Build-system blockers (Task 2 Step 7 link-of-undefined-helper) have a documented adjustment path.

**Type consistency:** `Evaluation* path_local_eval` and `bool* path_local_emitted` consistent across `exploration.h:150` (grogros_zero decl), `exploration.h:156` (explore_random_child decl), `exploration.cpp:356` and `:726` (defs), the descent recursion arg-passing (Task 4 Step 1), and the emission writes (Task 4 Step 2). `Node::compute_path_local_eval(const PositionHistory&, const Move&, const Evaluation&, Evaluation*) const` declared at `exploration.h` (Task 2 Step 4) and defined at `exploration.cpp` (Task 3 Step 1) with byte-exact arg lists. `Evaluation::operator==` declared in `board.h:Evaluation` struct (Task 2 Step 1) and used at Task 4 Step 2. `dag_draw_eval()` at `exploration.cpp:154` (unchanged). `position_is_draw_by_repetition` at the existing call site (unchanged). `_board->_got_moves` is `uint8_t`; cast to `size_t` in the gate. No signature drift.

---

**Execution note:** No automated test runner for GrogrosZero search behaviour; per task the "test" is the clean MSBuild + the written OFF-byte-identical argument; the single end-to-end validation is the **[USER]** gate (Task 5). This is the project's established acceptance pattern for #11.
