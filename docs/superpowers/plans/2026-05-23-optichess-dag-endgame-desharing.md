# #11 attempt-8 — DAG endgame de-sharing + headless harness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `g_tt_node_dag == true` structurally correct on both #11 reference repros by de-sharing pawn-endgame nodes (so the §3 repetition draw backs up by ordinary minimax, as in tree mode), remove the now-dead attempt-7 verdict machinery, and add a headless CLI harness that lets the assistant validate without the GUI gate.

**Architecture:** Gate the single `node_map` share site in `Node::explore_new_move` on `!is_pawn_endgame()` — a pawn-endgame position is never registered/looked up, so it is always a fresh per-path node with `_parent_count <= 1`, and the draw backs up without any value injection into shared structure (sound by construction; cannot violate invariant 772183a because there is no shared node to mutate). With the gate in place the attempt-7 all-cycle verdict (measured dead: `all_cycle_persisted=0`) is removed. A new `main_headless()` entry point (selected by `--dag-test` argv) reuses the global buffers + `grogros_zero` with no `InitWindow`, runs both repros ON/OFF, and exits non-zero on any failed assertion.

**Tech Stack:** C++17, MSVC (`v145`, `opti_chess.vcxproj`), raylib (linked but window never initialised in headless mode), tsl::robin_map, MSBuild Release|x64.

**Baseline:** `a1c0e9c` (attempt-7c tip). Branch: `feature/tt-main-search`. Spec: `docs/superpowers/specs/2026-05-22-optichess-dag-endgame-desharing-design.md`.

---

## File Structure

| File | Responsibility | Change |
|------|----------------|--------|
| `opti_chess/main_headless.h` | Headless validation entry point (`main_headless()`), repro runner, assertions | **Create** |
| `opti_chess/main.cpp` | Dispatch `--dag-test` argv to `main_headless()` | Modify |
| `opti_chess/exploration.cpp` | Add the `dag_shareable` gate at the two share sites (`:617`, `:653`); remove verdict block + `_cycled_batch_seq` writes + `g_dag_batch_seq` def | Modify |
| `opti_chess/exploration.h` | Remove `ChildLink::_cycled_batch_seq` field + `g_dag_batch_seq` extern | Modify |
| `opti_chess/gui.cpp` | Remove the two `++g_dag_batch_seq` increments | Modify |
| `opti_chess/board.h` | Remove now-orphan `Evaluation::operator==` | Modify |
| `opti_chess/dag_log.h` / `dag_log.cpp` | Remove the 3 attempt-7 counters from enum + JSON line | Modify |
| `opti_chess/BUGFIXES.md` | Record attempt-8 outcome | Modify |
| `docs/superpowers/specs/2026-05-22-...-design.md` | Mark validated-by-gate | Modify |

---

## Build & run reference (used by every task)

- **Build (Release x64):** prefer the VS IDE, or from a *Developer PowerShell for VS*:
  ```powershell
  $msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
  & $msbuild "C:\Users\poiro\Documents\Info\Chess\C++\opti_chess\opti_chess.vcxproj" /p:Configuration=Release /p:Platform=x64 /m
  ```
  Expected: `Build succeeded.` with `0 Error(s)`.
- **Run harness:**
  ```powershell
  & "C:\Users\poiro\Documents\Info\Chess\C++\x64\Release\opti_chess.exe" --dag-test
  echo "exit=$LASTEXITCODE"
  ```
  (Confirm the exe path from the build's `OutDir`; the existing Debug build is at `x64\Debug\opti_chess.exe`, so Release is `x64\Release\opti_chess.exe`.)

---

## Task 1: Headless validation harness (the test, RED on current code)

**Files:**
- Create: `opti_chess/main_headless.h`
- Modify: `opti_chess/main.cpp:1-13`

This task adds the harness and proves it captures the bug: on the **unchanged** DAG, Repro 1 ON false-wins (≈476) → FAIL, while OFF sanity + Repro 2 PASS. Task 2 turns Repro 1 ON green.

- [ ] **Step 1: Create the headless harness**

Create `opti_chess/main_headless.h` with exactly this content:

```cpp
#pragma once

// #11 attempt-8 — banc de validation headless (design 2026-05-22 §5).
// Pas de fenêtre raylib : réutilise les buffers globaux (monte_board_buffer,
// monte_node_buffer, node_map) + grogros_zero, sans rien dessiner. Lancé via
// `opti_chess --dag-test`. Sortie != 0 si une assertion échoue -> exécutable
// en CLI/CI par l'assistant, ce qui casse le goulot du gate GUI manuel.

#include "board.h"
#include "exploration.h"
#include "evaluation.h"
#include "buffer.h"
#include "zobrist.h"
#include <cmath>
#include <cstdio>
#include <string>

namespace dag_headless {

// Seuils épinglés depuis les trajectoires OFF/ON mesurées (design §5.3 ;
// faux-gain Repro 1 ON mesuré = 476 ; Repro 2 ON atteint ~298). Confirmés/
// ajustés au Step 4 d'après la sortie réelle.
constexpr int EPS_DRAW   = 120; // |eval| <= EPS_DRAW  => nulle
constexpr int WIN_THRESH = 80;  // eval  >= WIN_THRESH => gain net

// Paramètres de recherche : miroir exact de gui.h:98-105.
constexpr double ALPHA  = 0.00001;
constexpr double BETA   = 5.0;
constexpr double GAMMA  = 1.10;
constexpr int    QDEPTH = 10;

// Charge la FEN sur (root, board) déjà alloués, force le toggle DAG, tourne
// n_batches × iters_per_batch itérations, renvoie l'éval racine finale.
// Reset d'état entre cas = miroir de GUI::load_FEN (gui.cpp:1650) + du handler
// DELETE de la GUI (main_gui.h:437-438) : node_map + TT purgés, root reset.
inline int run_repro(Node* root, Board* board, Evaluator* eval,
                     const char* name, const std::string& fen,
                     int n_batches, int iters_per_batch, bool dag_on) {
    root->reset();
    root->_board = board;
    board->from_fen(fen);
    root->_is_active = true;
    board->_is_active = true;
    node_map.clear();
    transposition_table.clear();

    g_tt_node_dag = dag_on;
    g_tt_main_search = false;

    for (int b = 0; b < n_batches; ++b) {
        root->grogros_zero(&monte_board_buffer, eval, ALPHA, BETA, GAMMA,
                           iters_per_batch, QDEPTH);
    }

    const int eval_value = root->_deep_evaluation._value;
    std::printf("[DAG-TEST] %-26s dag=%d  eval=%6d  avg=%.3f\n",
                name, dag_on ? 1 : 0, eval_value,
                root->_deep_evaluation._avg_score);
    return eval_value;
}

} // namespace dag_headless

inline int main_headless() {
    using namespace dag_headless;

    // Init des pools globaux : miroir de GUI::init_buffers (gui.cpp:2051).
    const PoolSizing ps = compute_pool_sizing();
    if (!monte_board_buffer._init) monte_board_buffer.init(ps.board_length);
    if (!monte_node_buffer._init)  monte_node_buffer.init(ps.node_length);
    transposition_table.init(ps.tt_length, nullptr, true);

    Evaluator eval; // défaut, comme gui.h:167 (new Evaluator()).

    Board* board = monte_board_buffer.get_first_free_board();
    Node*  root  = monte_node_buffer.get_first_free_node();

    // Repro 1 : KP(h)-vs-K, nulle théorique. Repro 2 : finale de pions gagnée.
    const std::string fen1 = "6k1/8/7P/7K/8/8/8/8 w - - 3 72";
    const std::string fen2 = "8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7";

    // Mêmes budgets que GUI::run_dag_repro_1/2 (gui.cpp:1166-1187).
    const int r1_off = run_repro(root, board, &eval, "repro1_kp_h_draw", fen1, 10, 2000, false);
    const int r1_on  = run_repro(root, board, &eval, "repro1_kp_h_draw", fen1, 10, 2000, true);
    const int r2_off = run_repro(root, board, &eval, "repro2_pawn_win",  fen2, 20, 3000, false);
    const int r2_on  = run_repro(root, board, &eval, "repro2_pawn_win",  fen2, 20, 3000, true);

    int fails = 0;
    auto check = [&](bool ok, const char* desc) {
        std::printf("[DAG-TEST] %-44s %s\n", desc, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    };

    check(std::abs(r1_off) <= EPS_DRAW,                              "Repro1 OFF ~ draw (sanity)");
    check(std::abs(r1_on)  <= EPS_DRAW,                              "Repro1 ON  ~ draw (THE FIX)");
    check(r2_off >= WIN_THRESH,                                      "Repro2 OFF winning (sanity)");
    check(r2_on  >= WIN_THRESH && std::abs(r2_on) > EPS_DRAW,        "Repro2 ON  winning (no phantom draw)");

    std::printf("[DAG-TEST] %d failure(s)\n", fails);
    return fails == 0 ? 0 : 1;
}
```

- [ ] **Step 2: Wire the `--dag-test` argv into `main.cpp`**

Replace the whole of `opti_chess/main.cpp` (currently lines 1-13) with:

```cpp
#include "main_lichess.h"
#include "main_gui.h"
#include "main_headless.h"
#include <string>

bool lichess = false;

int main(int argc, char** argv) {

	// #11 attempt-8 — banc headless de validation DAG (design §5). N'initialise
	// jamais la fenêtre raylib ; sortie != 0 si une assertion échoue.
	if (argc > 1 && std::string(argv[1]) == "--dag-test")
		return main_headless();

	if (lichess)
		main_lichess();
	else
		main_ui();

	return 0;
}
```

- [ ] **Step 3: Build (Release x64)**

Run the build command from *Build & run reference*.
Expected: `Build succeeded.`, `0 Error(s)`. If `main_headless.h` symbols are unresolved, confirm `transposition_table` is declared in an included header (it is used in `gui.cpp`/`main_gui.h`; it is declared in `board.h`/`zobrist.h` — all already included here).

- [ ] **Step 4: Run the harness on UNCHANGED DAG — observe RED + pin thresholds**

Run the harness command. Expected output shape:
```
[DAG-TEST] repro1_kp_h_draw           dag=0  eval=     0  avg=...
[DAG-TEST] repro1_kp_h_draw           dag=1  eval=   476  avg=...
[DAG-TEST] repro2_pawn_win            dag=0  eval=  >0   avg=...
[DAG-TEST] repro2_pawn_win            dag=1  eval=  >0   avg=...
[DAG-TEST] Repro1 OFF ~ draw (sanity)                        PASS
[DAG-TEST] Repro1 ON  ~ draw (THE FIX)                       FAIL   <-- documents the bug
[DAG-TEST] Repro2 OFF winning (sanity)                       PASS
[DAG-TEST] Repro2 ON  winning (no phantom draw)              PASS
[DAG-TEST] 1 failure(s)
exit=1
```
Verification: exit code is **1**, and the only FAIL is `Repro1 ON ~ draw`. This proves the harness detects the bug.

Threshold adjustment (do this from the printed numbers, not guesses):
- If `repro1 dag=0` eval is not within ±`EPS_DRAW` of 0, raise `EPS_DRAW` in `main_headless.h` to comfortably cover the OFF-converged value while staying well below 476 (e.g. half-way). Rebuild.
- If `repro2` (either) eval is below `WIN_THRESH`, lower `WIN_THRESH` to comfortably below the smaller of the two observed Repro-2 evals while staying above `EPS_DRAW`. Rebuild.
- Re-run until OFF sanity (Repro1 OFF, Repro2 OFF, Repro2 ON) all PASS and only `Repro1 ON` FAILs.

- [ ] **Step 5: Commit the harness (test infra)**

```bash
git add opti_chess/main_headless.h opti_chess/main.cpp
git commit -m "test(search): #11 attempt-8 - headless DAG repro harness (--dag-test)"
```

---

## Task 2: Endgame de-sharing gate (THE FIX — turns Repro 1 ON green)

**Files:**
- Modify: `opti_chess/exploration.cpp:609-621` and `opti_chess/exploration.cpp:642-656`

Gate the single `node_map` share site on `!new_board->is_pawn_endgame()` so a pawn-endgame position is never registered nor looked up → always a fresh per-path node → the §3 repetition draw backs up by ordinary minimax.

- [ ] **Step 1: Add the gate at the lookup site (`:609-621`)**

In `Node::explore_new_move`, the current block is:

```cpp
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
```

Replace it with (introduces `dag_shareable`, used at both sites):

```cpp
		else {
			new_board->get_zobrist_key();

			// #11 attempt-8 (design 2026-05-22 §3) — une finale de pions est
			// DÉ-PARTAGÉE sous DAG : chaque chemin reçoit son propre sous-arbre,
			// donc les feuilles §3 de répétition remontent la nulle par minimax
			// ordinaire (comportement mode-arbre, déjà correct). is_pawn_endgame()
			// = rois + pions seuls ; couvre les deux repros et c'est la porte SÛRE
			// la plus étroite. OFF : jamais consultée. Coût : un scan 8x8 borné, à
			// la création de nœud uniquement (jamais dans la boucle sélection/éval).
			const bool dag_shareable = g_tt_node_dag && !new_board->is_pawn_endgame();

			// #11 Plan B — link-on-create. Si une position de meme cle Zobrist
			// est deja un Node VIVANT, on lie l'arete a ce Node partage au lieu
			// de recreer un sous-arbre. La branche nulle (au-dessus) fabrique
			// toujours une feuille distincte — jamais partagee. OFF / finale de
			// pions : saute tout (jamais cherchée -> jamais trouvée -> jamais partagée).
			Node* shared = nullptr;
			if (dag_shareable) {
				const auto it = node_map.find(new_board->_zobrist_key);
				if (it != node_map.end()) {
					shared = it->second;
				}
			}
```

- [ ] **Step 2: Add the gate at the register site (`:642-656`)**

The current miss branch is:

```cpp
			else {
				// Miss : création normale + enregistrement dans node_map.
				child = monte_node_buffer.get_first_free_node();

				if (child == nullptr)
					return;

				child->_board = new_board;
				created_new_node = true;

				if (g_tt_node_dag) {
					node_map[new_board->_zobrist_key] = child;
					g_dag_link_misses++;
				}
			}
```

Replace the `if (g_tt_node_dag)` guard with `if (dag_shareable)`:

```cpp
			else {
				// Miss : création normale + enregistrement dans node_map
				// (sauf finale de pions, dé-partagée : jamais enregistrée).
				child = monte_node_buffer.get_first_free_node();

				if (child == nullptr)
					return;

				child->_board = new_board;
				created_new_node = true;

				if (dag_shareable) {
					node_map[new_board->_zobrist_key] = child;
					g_dag_link_misses++;
				}
			}
```

- [ ] **Step 3: Build (Release x64)**

Run the build command. Expected: `Build succeeded.`, `0 Error(s)`.

- [ ] **Step 4: Run the harness — expect ALL GREEN**

Run the harness command. Expected:
```
[DAG-TEST] Repro1 OFF ~ draw (sanity)                        PASS
[DAG-TEST] Repro1 ON  ~ draw (THE FIX)                       PASS   <-- now fixed
[DAG-TEST] Repro2 OFF winning (sanity)                       PASS
[DAG-TEST] Repro2 ON  winning (no phantom draw)              PASS
[DAG-TEST] 0 failure(s)
exit=0
```
Verification: exit code **0**. Repro 1 ON `eval` is now within ±`EPS_DRAW` of 0; Repro 2 ON still ≥ `WIN_THRESH`.

- [ ] **Step 5: Commit**

```bash
git add opti_chess/exploration.cpp
git commit -m "fix(search): #11 attempt-8 - de-share pawn-endgame nodes under DAG (#11)"
```

---

## Task 3: Remove dead attempt-7 verdict machinery (refactor — stays green)

**Files:**
- Modify: `opti_chess/exploration.cpp` (verdict block `:479-535`; `_cycled_batch_seq` writes `:872-877`, `:904-919`; global def `:2324`)
- Modify: `opti_chess/exploration.h` (field `:19-24`; extern `:297-302`)
- Modify: `opti_chess/gui.cpp` (`:1062-1064`, `:1128-1130`)
- Modify: `opti_chess/board.h` (`:501-507`)
- Modify: `opti_chess/dag_log.h` (`:35-38`), `opti_chess/dag_log.cpp` (`:139-140`, `:151-153`)

With the gate, every pawn-endgame node has `_parent_count <= 1` and ordinary minimax is correct — the verdict (measured dead, `all_cycle_persisted=0`) is removed. KEEP intact: the §3 structural cut, opt-1 `path_local_eval` emission (`:868-870`), opt-3 `DagExcl` (`:858-863`, `:922-941`), Bug-2 model-A node accounting, the `dag_log` infrastructure and `pred_*`/`dag_excl_*`/`nodes_*` counters.

- [ ] **Step 1: Remove the all-cycle verdict block in `exploration.cpp` (`:479-535`)**

Delete the entire comment + block. Find it by its first comment line `// #11 attempt-7 — verdict all-cycle` and its `if (g_tt_node_dag && !_children.empty()) { ... }` body that ends just before `// Temps de calcul`. After deletion the code reads:

```cpp
	// FIXME *** cela ne devrait pas arriver. Sous DAG, _nodes est une borne
	// proxy par-arete (Bug 2 model A, clamp >=0) : garde arbre silencee (Task 4).
	if (!g_tt_node_dag && _nodes <= 0) {
		cout << "negative nodes in grogros zero???" << endl;
	}

	// Temps de calcul
	_time_spent += clock() - begin_monte_time;

	return;
}
```

(That is: the block between the `negative nodes in grogros zero???` `}` and `// Temps de calcul` is entirely removed — comment lines `:479-501` plus the `if`-block `:502-535`.)

- [ ] **Step 2: Remove the `_cycled_batch_seq` write in the §3-cut branch (`:872-877`)**

Delete this comment + line (inside `Node::explore_random_child`, right after the opt-1 `path_local_eval` emission):

```cpp
		// #11 attempt-7 (cf. design 2026-05-21 §3.3) — marque l'arête comme
		// cycle-touch sur CE batch. Le verdict all-cycle est calculé en fin
		// de Node::grogros_zero (§3.4). `this` est le parent, `move` est
		// l'arête vers child. Aucune mutation partagée : ChildLink est dans
		// _children du parent, propre à ce parent (invariant 772183a).
		this->_children[move]._cycled_batch_seq = g_dag_batch_seq;
```

The preceding `if (path_local_eval != nullptr) { *path_local_eval = dag_draw_eval(); }` (opt-1) and the following `if constexpr (dag_log::enabled) { ... pred_fire ... }` stay.

- [ ] **Step 3: Drop the verdict cascade scratch in the descent path (`:904-919`)**

Replace this block:

```cpp
	// #11 attempt-7 (cf. design 2026-05-21 §3.4) — on capture
	// le verdict all-cycle du fils via un scratch path-local. Si le fils est
	// forced-draw (tous SES coups légaux cyclent), sa fin de grogros_zero écrit
	// dag_draw_eval dans child_verdict ; on marque alors NOTRE arête vers ce
	// fils comme cycle-touch pour CE batch, permettant au verdict de remonter
	// en cascade jusqu'à un nœud non partagé (la racine). OFF / hors DAG :
	// nullptr passé -> child_verdict reste non évalué -> aucun effet.
	Evaluation child_verdict;
	child_verdict._evaluated = false;
	{
		PathScope _ps(branch_history, *child->_board);
		child->grogros_zero(board_buffer, eval, alpha, beta, gamma, 1, quiescence_depth, network, &branch_history, g_tt_node_dag ? &child_verdict : nullptr); // L'évaluation du fils est mise à jour ici
	}
	if (g_tt_node_dag && child_verdict._evaluated) {
		this->_children[move]._cycled_batch_seq = g_dag_batch_seq;
	}
```

with (descent kept; verdict scratch + cascade marking gone; `path_local_eval` defaults to nullptr — its only consumer was the removed marking, opt-1/DagExcl backup uses `_deep_evaluation` + `dag_excl`):

```cpp
	{
		PathScope _ps(branch_history, *child->_board);
		child->grogros_zero(board_buffer, eval, alpha, beta, gamma, 1, quiescence_depth, network, &branch_history); // L'évaluation du fils est mise à jour ici
	}
```

- [ ] **Step 4: Remove the `g_dag_batch_seq` definition in `exploration.cpp` (`:2324`)**

Delete:

```cpp
uint32_t g_dag_batch_seq = 1; // #11 attempt-7 — voir exploration.h §3.2
```

- [ ] **Step 5: Remove the `ChildLink::_cycled_batch_seq` field in `exploration.h` (`:19-24`)**

The struct becomes:

```cpp
struct ChildLink {
	Node* _node = nullptr;
	int _chosen_iterations = 0;
	int _propagated_nodes = 0;
};
```

- [ ] **Step 6: Remove the `g_dag_batch_seq` extern in `exploration.h` (`:297-302`)**

Delete the comment block `// #11 attempt-7 (cf. design 2026-05-21 §3.2). Séquence numérique globale ...` and the line `extern uint32_t g_dag_batch_seq;`. Leave the surrounding `extern bool g_tt_node_dag;` and `extern robin_map<uint64_t, Node*> node_map;` intact.

- [ ] **Step 7: Remove the two `++g_dag_batch_seq` increments in `gui.cpp`**

In `GUI::grogros_analysis` (`:1062-1064`) delete:

```cpp
	// #11 attempt-7 — nouvelle génération de batch pour le reset paresseux des
	// marqueurs ChildLink::_cycled_batch_seq (design 2026-05-21 §3.2).
	++g_dag_batch_seq;
```

In `GUI::run_dag_repro` (`:1128-1130`) delete:

```cpp
		// #11 attempt-7 — nouvelle génération de batch (reset paresseux des
		// marqueurs ChildLink::_cycled_batch_seq, design 2026-05-21 §3.2).
		++g_dag_batch_seq;
```

- [ ] **Step 8: Remove the orphan `Evaluation::operator==` in `board.h` (`:501-507`)**

Its only consumer was the verdict block (`exploration.cpp:521`, removed in Step 1 — verified: `grep "== _deep_evaluation"` / `"draw =="` shows no other use). Delete:

```cpp
	// #11 attempt-7 — égalité de détection-changement (cf. design 2026-05-21).
	// Compare uniquement les champs pertinents pour décider d'une réécriture
	// de _deep_evaluation : _value et _avg_score. Ce n'est PAS une égalité
	// structurelle complète.
	inline bool operator==(const Evaluation& other) const {
		return _value == other._value && _avg_score == other._avg_score;
	}
```

- [ ] **Step 9: Remove the 3 attempt-7 counters from the `dag_log` enum (`dag_log.h:35-38`)**

The enum tail becomes:

```cpp
		nodes_terminal,
		nodes_via_explore_new,
		nodes_via_explore_random,
		counter_count
	};
```

(Delete the `// #11 attempt-7 ... métriques du verdict all-cycle.` comment and the three enumerators `all_cycle_verdicts_emitted`, `all_cycle_persisted`, `enum_gate_blocks`.)

- [ ] **Step 10: Drop the 3 counters from the `batch_end` JSON in `dag_log.cpp` (`:135-154`)**

In the `snprintf` format string, change the tail from:

```cpp
		"\"nodes_terminal\":%d,\"nodes_via_explore_new\":%d,\"nodes_via_explore_random\":%d,"
		"\"all_cycle_verdicts_emitted\":%d,\"all_cycle_persisted\":%d,"
		"\"enum_gate_blocks\":%d,"
		"\"events_dropped\":%d}}\n",
```

to:

```cpp
		"\"nodes_terminal\":%d,\"nodes_via_explore_new\":%d,\"nodes_via_explore_random\":%d,"
		"\"events_dropped\":%d}}\n",
```

and delete the three argument lines:

```cpp
		g_counters[(int)Counter::all_cycle_verdicts_emitted],
		g_counters[(int)Counter::all_cycle_persisted],
		g_counters[(int)Counter::enum_gate_blocks],
```

(The remaining args end at `g_counters[(int)Counter::nodes_via_explore_random],` followed by `g_events_dropped);` — verify the `%d` count in the format matches the argument count after editing.)

- [ ] **Step 11: Build (Release x64)**

Run the build command. Expected: `Build succeeded.`, `0 Error(s)`.
If any unresolved-symbol error mentions `g_dag_batch_seq`, `_cycled_batch_seq`, or `all_cycle_*`, re-grep for leftover references:
```powershell
```
Use Grep for `g_dag_batch_seq|_cycled_batch_seq|all_cycle_|enum_gate_blocks` across `opti_chess/` — expected: **no matches**.

- [ ] **Step 12: Run the harness — expect ALL GREEN (behaviour-preserving)**

Run the harness command. Expected: identical to Task 2 Step 4 — `0 failure(s)`, `exit=0`. The removal must not change the eval trajectory (verdict was dead; opt-1/DagExcl/§3-cut kept).

- [ ] **Step 13: Commit**

```bash
git add opti_chess/exploration.cpp opti_chess/exploration.h opti_chess/gui.cpp opti_chess/board.h opti_chess/dag_log.h opti_chess/dag_log.cpp
git commit -m "refactor(search): #11 attempt-8 - remove dead attempt-7 all-cycle verdict (#11)"
```

---

## Task 4: Documentation

**Files:**
- Modify: `opti_chess/BUGFIXES.md`
- Modify: `docs/superpowers/specs/2026-05-22-optichess-dag-endgame-desharing-design.md`

- [ ] **Step 1: Record the attempt-8 outcome in `BUGFIXES.md`**

Under the #11 section, append an attempt-8 entry stating: pawn-endgame nodes are de-shared under DAG (`is_pawn_endgame()` gate at the single `node_map` share site in `Node::explore_new_move`); the §3 repetition draw now backs up by ordinary minimax (tree-mode behaviour, already correct); attempt-7 all-cycle verdict machinery removed (was dead, `all_cycle_persisted=0`); validated by the headless harness (`opti_chess --dag-test`): Repro 1 ON ≈ draw, Repro 2 ON winning, OFF sanity both. Note the documented residual: non-pawn-endgame repetition/perpetual draws under DAG are still uncovered (follow-up). Use real French Unicode symbols (é, è, ê, ç, à, …), per project convention.

- [ ] **Step 2: Mark the spec validated-by-gate**

In `docs/superpowers/specs/2026-05-22-optichess-dag-endgame-desharing-design.md`, append a one-line status note at the top (under the header block) recording that attempt-8 is implemented and passes the headless harness; the GUI OFF byte-identicality + real-play gate (acceptance §9.3) remains a USER gate.

- [ ] **Step 3: Commit**

```bash
git add opti_chess/BUGFIXES.md docs/superpowers/specs/2026-05-22-optichess-dag-endgame-desharing-design.md
git commit -m "docs(search): #11 attempt-8 - record de-sharing outcome, mark spec validated (#11)"
```

---

## Final acceptance (USER GUI gate — assistant cannot run)

After Task 4, hand off to the user for the final gate (spec §9.3), which the assistant cannot execute:
1. Build clean with `g_tt_node_dag` both states (runtime toggle; default OFF).
2. OFF byte-identical to baseline `a1c0e9c`: PERFT 1/2 + EVALUATION on the usual positions.
3. DAG ON in the GUI (key `O`, then repro keys `1`/`2`): Repro 1 no longer false-wins; Repro 2 stays won; no regression in real play.

The assistant-runnable gate (`opti_chess --dag-test`, exit 0) is satisfied by Tasks 2-3.

---

## Self-Review

- **Spec coverage:** §3 gate → Task 2 (both share sites, `dag_shareable`). §4 verdict removal → Task 3 (all enumerated items: `_cycled_batch_seq` field+writes, `g_dag_batch_seq` global+increments, verdict block, `operator==`, the 3 `dag_log` counters; KEEP list preserved — §3 cut, opt-1 emission, DagExcl, model-A accounting, dag_log infra). §5 harness → Task 1 (`main_headless()`, per-repro routine mirroring `run_dag_repro`/`load_FEN`, exit-code assertions). §6 OFF identicality → gate short-circuits to `false` OFF; removed code was DAG-only; harness is additive. §9 acceptance → Task 1/2/3 harness + Task 4 docs + USER gate section. No gap.
- **Placeholder scan:** thresholds `EPS_DRAW`/`WIN_THRESH` have concrete starting values (120/80) plus a measured-adjustment step (§5.3 deferral, the established #11 pattern) — not a vague TODO. All edits show full before/after code. No "handle edge cases" / "similar to".
- **Type/name consistency:** `dag_shareable` (Task 2) used identically at both sites. `main_headless()` declared in `main_headless.h`, called in `main.cpp` (Task 1). `run_repro` signature matches its call sites. `grogros_zero` arg list matches `exploration.h:157`. Counters removed from both enum (`dag_log.h`) and consumer (`dag_log.cpp`) together.
