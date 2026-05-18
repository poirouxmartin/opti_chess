# Bug 1 — Option 1: path-local draw VALUE backup (spec §3 complete) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make a DAG path-local repetition draw contribute a true draw value (0.0) to the parent's evaluation backup for the current traversal, without mutating any shared node/edge, completing spec §3 (`docs/superpowers/specs/2026-05-17-optichess-plan-b-dag-design.md:110-115`).

**Architecture:** Thread an optional per-traversal `Evaluation*` out-parameter up the GrogrosZero call chain. A node that resolves to a path-local repetition draw writes the canonical draw evaluation into that out-param and does **not** touch its own shared `_deep_evaluation`. The parent substitutes that draw value (instead of the cyclic child's stored eval) when computing its best-move backup, and propagates its own path-local value upward the same way. This builds on the already-shipped Bug 1 option 3 (`DagExcl`, which stops the re-selection spin); option 1 adds value-correctness on top so the parent eval is no longer frozen/inconsistent on cyclic lines.

**Tech Stack:** C++17, MSVC (`opti_chess.sln`, Debug x64), single-threaded engine. No automated test framework for search behavior — verification is the manual repro gate + the existing `dag_debug_report()` diagnostic counters (assistant cannot run the GUI; the USER runs the gate).

**Invariants (every task must preserve):**
- **OFF byte-identical:** when `g_tt_node_dag == false`, the new out-param is `nullptr` and never read/written → engine output strictly unchanged vs the post-opt-3 state.
- **No shared mutation (772183a):** the §3 path never writes `this->_deep_evaluation`, `child->_deep_evaluation`, `child_link`, or `node_map`. The draw lives only in the stack-threaded out-param.
- **Performance #1:** the out-param is a single pointer; the only added work on the hot path is one `if (out != nullptr)` on the §3 branch and one pointer copy on the backup line. Zero allocation. No change to selection scoring math.

---

## File Structure

- `opti_chess/exploration.h` — add the out-param to the three signatures (`grogros_zero`, `explore_random_child`, `explore_new_move`). One responsibility: declarations.
- `opti_chess/exploration.cpp` — the propagation logic in `grogros_zero` / `explore_random_child` (and the draw-leaf branch of `explore_new_move` for consistency). Hottest path; all edits `g_tt_node_dag`-gated.
- `docs/superpowers/specs/2026-05-17-optichess-plan-b-dag-design.md` — mark §3 "valeur de nulle" as implemented (doc only).
- `opti_chess/BUGFIXES.md` — record Bug 1 option 1 done.

No new files. Follows the existing toggle-gated, French-comment, single-owned-`PositionHistory` conventions already in `exploration.cpp`.

---

## Reference: current backup site (read before starting)

`exploration.cpp` (post opt-3 state):
- `explore_random_child` backup: `_deep_evaluation = _children[get_best_score_move(alpha, beta)]._node->_deep_evaluation;` (~`:750`).
- `explore_random_child` §3 cut: `if (g_tt_node_dag && position_is_draw_by_repetition(branch_history, *child->_board)) { g_dag_recheck_hits++; ...log...; if (dag_excl != nullptr) dag_excl->add(move); _iterations++; return; }`.
- Canonical draw evaluation producer: `init_terminal_draw_child` (`:129-137`) → `mark_position_as_draw(*board)` then `evaluate_position(...)` yields the engine's draw `Evaluation` (value 0, white-relative). The draw `Evaluation` to inject is the one a freshly created repetition leaf carries in `explore_new_move`'s draw branch (`:485`).

---

## Task 1: Design validation gate (no code)

**Files:** none (discussion + this plan annotated).

This change is on the single hottest path of a performance-critical engine and interacts with mate scores, alpha/beta windows, stand-pat, and (optionally) quiescence. The engine designer must confirm the propagation semantics before code is written. This is a real gate, not a placeholder.

- [x] **Step 1: DECIDED (2026-05-18).** Reuse the exact `Evaluation` produced by `init_terminal_draw_child` (`mark_position_as_draw(*board)` then `evaluate_position(...)`) — byte-identical to a real repetition leaf, no derived-field divergence. Do NOT hand-build the draw struct.

- [x] **Step 2: DECIDED (2026-05-18).** **GrogrosZero only** (`grogros_zero` / `explore_new_move` / `explore_random_child`); `quiescence` left unchanged (no node sharing — `exploration.cpp:1323` "Pas de partage via TT"; no evidence of quiescence repetition mishandling). FUTURE IMPROVEMENT (noted, not now): consider extending the path-local draw threading into `quiescence` if a quiescence-side repetition issue is ever observed.

- [x] **Step 3: CONFIRMED (2026-05-18).** A draw is depth-independent → `0.0` white-relative pre-`tt_normalize_mate` is correct; compares correctly in `get_best_score_move` against mate-scored siblings.

- [x] **Step 4: No commit (design only).**

---

## Task 2: Add the out-parameter to signatures (inert)

**Files:**
- Modify: `opti_chess/exploration.h` (signatures of `grogros_zero`, `explore_random_child`, `explore_new_move`)

- [ ] **Step 1: Edit `exploration.h`** — add a trailing defaulted out-param to each of the three method declarations:

```cpp
void grogros_zero(BoardBuffer* board_buffer, Evaluator* eval, const double alpha, const double beta, const double gamma, int nodes, int quiescence_depth, Network* network = nullptr, PositionHistory *path_history = nullptr, Evaluation* path_local_eval = nullptr);

void explore_new_move(BoardBuffer* board_buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network = nullptr, PositionHistory *path_history = nullptr, DagExcl* dag_excl = nullptr, Evaluation* path_local_eval = nullptr);

void explore_random_child(BoardBuffer* board_buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network = nullptr, PositionHistory *path_history = nullptr, DagExcl* dag_excl = nullptr, Evaluation* path_local_eval = nullptr);
```

(Note: `explore_new_move` does not currently take `DagExcl*`; add it too for signature symmetry only if Task 4 needs it — otherwise add only `Evaluation* path_local_eval`. Decide in Task 1 Step 2; default: add only `path_local_eval` to `explore_new_move`.)

- [ ] **Step 2: Add matching params in the `.cpp` definitions** (no behavior yet — params unused).

- [ ] **Step 3: Build.**

Run: `& 'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe' opti_chess.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m /nologo /clp:ErrorsOnly /v:m`
Expected: `EXITCODE=0`, no errors.

- [ ] **Step 4: Commit.**

```bash
git add opti_chess/exploration.h opti_chess/exploration.cpp
git commit -m "refactor(search): add inert path_local_eval out-param (#11)"
```

---

## Task 3: §3 cut writes the path-local draw (producer side)

**Files:**
- Modify: `opti_chess/exploration.cpp` — the `explore_random_child` §3 cut block.

- [ ] **Step 1: In the §3 cut, set the out-param to the draw evaluation before returning.** Use the Evaluation chosen in Task 1 Step 1. Concretely, just before `_iterations++; return;` inside `if (g_tt_node_dag && position_is_draw_by_repetition(...))`:

```cpp
if (dag_excl != nullptr) dag_excl->add(move);
// Bug 1 opt 1 — spec §3 : remonte une nulle path-locale par valeur de
// retour, SANS muter le Node/arete partages (invariant 772183a). Le parent
// substituera cette valeur a child->_deep_evaluation pour CETTE traversee.
if (path_local_eval != nullptr) {
    Evaluation _draw;            // valeur de nulle canonique (Task 1 §1)
    _draw._value = 0;
    _draw._evaluated = true;
    _draw._wdl = WDL_draw();     // remplacer par l'API WDL nulle reelle
    _draw._avg_score = 0.5;
    tt_fixup_derived(_draw);
    *path_local_eval = _draw;
}
_iterations++;
return;
```

(Replace `WDL_draw()` with the project's actual draw-WDL constructor as used by `init_terminal_draw_child` / `mark_position_as_draw`. Verify the exact field set during Task 1.)

- [ ] **Step 2: Build.** Expected: `EXITCODE=0`.

- [ ] **Step 3: Commit.**

```bash
git commit -am "feat(search): §3 cut emits path-local draw value (#11)"
```

---

## Task 4: Parent consumes the child's path-local draw in its backup

**Files:**
- Modify: `opti_chess/exploration.cpp` — `explore_random_child` (and `grogros_zero` loop, which calls it).

- [ ] **Step 1: In `grogros_zero`, give each `explore_random_child` call a per-iteration scratch `Evaluation` and pass its address under DAG.** Near the existing `DagExcl dag_excl;` declaration:

```cpp
Evaluation child_path_eval; // scratch per-iteration, pile uniquement
```

and at the `explore_random_child(...)` call site add the argument:

```cpp
explore_random_child(board_buffer, eval, alpha, beta, gamma, quiescence_depth, network, base_path_history, g_tt_node_dag ? &dag_excl : nullptr, g_tt_node_dag ? &child_path_eval : nullptr);
```

- [ ] **Step 2: In `explore_random_child`, when its own descent's child resolved to a path-local draw, use that draw for the parent backup instead of the shared stored eval — and propagate upward.** Replace the backup line region so that, when this traversal's chosen edge produced a draw (detected because the recursive call set the scratch eval / the §3 cut fired for `move`), the backup uses the draw value; otherwise unchanged:

```cpp
// Bug 1 opt 1 — backup spec §3. Si l'arete choisie est une nulle path-locale
// sur CE chemin, sa contribution au backup parent est une nulle (0), pas son
// _deep_evaluation partage (cree via un autre chemin). On NE persiste rien de
// partage : la valeur remonte via path_local_eval. OFF : chemin arbre intact.
Move _bm = get_best_score_move(alpha, beta);
const Evaluation* _bm_eval = &_children[_bm]._node->_deep_evaluation;
Evaluation _draw_subst;
if (g_tt_node_dag && dag_excl != nullptr && dag_excl->contains(_bm)) {
    _draw_subst._value = 0; _draw_subst._evaluated = true;
    _draw_subst._wdl = WDL_draw(); _draw_subst._avg_score = 0.5;
    tt_fixup_derived(_draw_subst);
    _bm_eval = &_draw_subst;
}
if (g_tt_node_dag && path_local_eval != nullptr) {
    *path_local_eval = *_bm_eval;          // remonte sans ecrire un champ partage
}
if (!g_tt_node_dag) {
    _deep_evaluation = _children[_bm]._node->_deep_evaluation; // chemin arbre inchange
}
else {
    // Sous DAG `this` peut etre partage : on n'ecrit _deep_evaluation que si
    // ce noeud n'est pas multi-parent sur cette traversee (sinon corruption
    // 772183a). Sinon la valeur ne vit que dans path_local_eval.
    if (_parent_count <= 1) {
        _deep_evaluation = *_bm_eval;
    }
}
```

(The exact predicate for "the chosen edge is a path-local draw" is: it is in `dag_excl` — that set is populated only by §3 cuts on this grogros_zero frame. This reuses the already-shipped opt-3 structure; no new state.)

- [ ] **Step 2b: Apply the identical substitution to the explore_random_child first backup site if present** (there are two `_deep_evaluation = _children[...]._node->_deep_evaluation;` sites: `:653` in `explore_new_move`, `:750` in `explore_random_child`). Only the `explore_random_child` site (the refine path where §3 fires) needs it. Leave `explore_new_move:653` unchanged (its draw children are real distinct leaves already).

- [ ] **Step 3: Build.** Expected: `EXITCODE=0`.

- [ ] **Step 4: Commit.**

```bash
git commit -am "feat(search): parent backup uses path-local draw value, no shared mutation (#11)"
```

---

## Task 5: Verification gate (USER-run; assistant cannot run the GUI)

**Files:** none (uses the existing diagnostic harness).

- [ ] **Step 1: Build Debug x64.** Expected `EXITCODE=0`.

- [ ] **Step 2: USER runs the repro** `8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7` (canonical transposition test) and the TODO Fe6 position `r3r1k1/pp3pbp/1qp3p1/2B5/2BP2b1/Q1n2N2/P4PPP/3R1K1R b - - 0 1`, DAG ON (key **O**, Plan A **I** OFF), several batches; capture console.

- [ ] **Step 3: Expected evidence (pass criteria):**
  - No `negative nodes ...` lines (Bug 2 model A — already shipped; regression check).
  - `[DAG] ... recheck=` no longer spins to thousands per batch on the same `child_key` (opt-3 shipped; regression check).
  - The `[DAG] §3-cut ... parent_eval=` is no longer a frozen non-draw value on the cyclic line — the cyclic line is scored toward a draw (0), and the Fe6 variant evaluations are internally consistent (the original user symptom "incohérences sur les évaluations" is gone).
  - OFF (no O): output identical to a pre-change build on the same position (byte-identical invariant).

- [ ] **Step 4: Update docs.**

```bash
# mark spec §3 "valeur de nulle path-locale" as implemented; BUGFIXES.md #11
git commit -am "docs(search): spec §3 path-local draw value implemented (#11)"
```

---

## Self-Review

**Spec coverage:** spec §3 (`dag-design.md:110-115`) "utiliser la valeur de nulle pour le backup du parent ... sans muter le Node partagé" → Task 3 (producer) + Task 4 (consumer, with the `_parent_count<=1` guard upholding the 772183a no-shared-mutation invariant; otherwise value lives only in the threaded out-param). §3 "ne pas descendre" → already shipped (the early `return` in the §3 cut). Selection devaluation/anti-spin → already shipped (opt-3 `DagExcl`).

**Placeholder scan:** `WDL_draw()` is flagged explicitly as "replace with the project's actual draw-WDL API, verify in Task 1" — this is a known concrete lookup, not a vague TODO; Task 1 Step 1 makes resolving it a required gated step. No other placeholders.

**Type consistency:** `Evaluation* path_local_eval` used consistently across `exploration.h` (Task 2) and the `.cpp` producer/consumer (Tasks 3–4). `DagExcl* dag_excl` reused unchanged from the shipped opt-3 (`exploration.h`). `tt_fixup_derived(Evaluation&)`, `get_best_score_move(double,double)`, `_parent_count` (int), `_deep_evaluation` (Evaluation) all match existing `exploration.cpp` usage.

**Open risk flagged honestly:** if Task 1 determines a shared parent (`_parent_count > 1`) is common on these lines, the value only propagates via the out-param and never reaches a *grandparent's stored-eval selection* — opt-3 already prevents re-selection spin, so the residual is display/eval consistency only. If that proves insufficient at the Task 5 gate, the next step is the full negamax-style return-value backup (out of scope here; would be a separate spec/brainstorm).

---

**Execution note:** No automated test framework exists for GrogrosZero search behavior; "tests" are the build (`EXITCODE=0`) plus the USER-run repro gate with the `dag_debug_report()` counters. This is the project's established acceptance pattern for #11 (see `BUGFIXES.md` #11 and prior plan-b2 Task 6).
