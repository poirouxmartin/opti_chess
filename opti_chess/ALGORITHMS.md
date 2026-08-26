# OptiChess — Algorithm reference

> Internal reference. Describes the **algorithmic side** (search, evaluation, TT, repetitions).
> Covers `exploration.cpp/h`, `zobrist.cpp/h` and the hooks in `board.cpp`.
> It does not describe the GUI or move generation.

---

## 1. Overview

```
                ┌─────────────────────────────────────────┐
                │ grogros_zero (main loop)                │
                │  - iterates N times                     │
                │  - picks: a new move OR re-exploration  │
                └────────────┬───────────────────┬────────┘
                             │                   │
                     explore_new_move    explore_random_child
                             │                   │
                             ▼                   ▼
                       quiescence (init)   grogros_zero (recursive)
                             │
                             ▼
                       quiescence (tactical search)
                             │
                             ▼
                  evaluate_position (static)
```

**Main objects**

| Object | Role | File |
|--------|------|------|
| `Board` | Physical board state + Zobrist key + moves | `board.h` |
| `Node` | Search-tree node: one `Board*`, children, evaluations, statistics | `exploration.h` |
| `ChildLink` | Parent→child edge: Node pointer + iterations chosen + propagated nodes | `exploration.h` |
| `Evaluation` | `{_value, _avg_score, _wdl, _uncertainty, _winnable_*, _evaluated}` | `board.h:461` |
| `BoardBuffer` / `NodeBuffer` | Memory pools (zero allocation during search) | `buffer.h` |
| `TranspositionTable` | `robin_map<uint64, ZobristEntry>` | `zobrist.h` |
| `PositionHistory` = `RepetitionHistory` | Path-local `robin_map<uint64, uint8>` | `exploration.h:9` |

---

## 2. ⚠️ Sign conventions (CRITICAL)

The code mixes **two conventions** for evaluation values. This is the **single most important** section to understand before touching anything.

### 2.1 "White-positive" convention (absolute)

`Evaluation::_value` is in **absolute form**:
- `_value > 0` ⇒ White is better
- `_value < 0` ⇒ Black is better

Every field of `Evaluation` (static and deep) uses this form.
Mate scores originate at `board.cpp:1569`:
```cpp
eval->_value = (-mate_value + _moves_count * mate_ply) * get_color();
```
The `* get_color()` brings the value back to the white-positive convention (the mated side is the side to move at the terminal position).

### 2.2 "Side-to-move" convention (mover's point of view)

Inside `quiescence()` (negamax), everything is from the **mover's point of view**:
- `stand_pat`, `alpha`, `beta`, `score`, and the values stored in / returned by the TT
- Conversion: `stm_value = _value * color`, where `color = _board->get_color()` (+1 White, -1 Black)
- The return value of `quiescence` is **from the point of view of the current side to move**

### 2.3 The boundary between the two

| Location | Form |
|----------|------|
| `Evaluation::_value` (the `_static_evaluation`, `_deep_evaluation` members) | **white-positive** |
| `stand_pat`, `alpha`, `beta`, the value returned by `quiescence`, the evaluation stored in the TT | **side-to-move** |
| `child->quiescence(... -beta, -alpha)` then `score = -child->quiescence(...)` | the classic negamax negation |
| `_value * color` | white-positive → side-to-move |
| `tt_eval * color` | side-to-move → white-positive (used in the TT cutoff) |

**Golden rule**: every time a value is manipulated, ask whether it is white-positive or side-to-move. Most bugs in this code come from confusing the two.

---

## 3. Evaluation and mate scores

### 3.1 Constants (`board.h:31-34`)
```cpp
constexpr int mate_value = 1e8;   // 100_000_000
constexpr int mate_ply  = 1e5;    // 100_000
```

### 3.2 Mate encoding (`board.cpp:1569`)
```cpp
eval->_value = (-mate_value + _moves_count * mate_ply) * get_color();
```

- `_moves_count` is the **absolute move number of the game** (the FEN field, incremented after Black's move, `board.cpp:1295`).
- The further into the game, the smaller `|_value|` (the mate is "less urgent").
- **The mate distance is encoded by `_moves_count`, not by the search depth from the root.**

### 3.3 Detecting a mate inside an evaluation (`board.cpp:3305-3306`)
```cpp
if (10 * abs_eval > mate_value) { // this is a mate score
    int mate_moves = static_cast<int>(mate_value - abs_eval - _moves_count * mate_ply)
                     * (e > 0 ? 1 : -1) / mate_ply + (_player && e > 0);
}
```

### 3.4 ⚠️ Consequence: `_moves_count` is NOT part of the Zobrist key

`zobrist.h:13-16`: the Zobrist key hashes pieces + side to move + castling rights + en passant. Not `_moves_count`.

So two search paths reaching the same physical position with different `_moves_count` values (the typical transposition) would share a Zobrist key but carry different mate scores. See section 7 (TT).

---

## 4. `Node`: invariants and critical fields

### 4.1 Fields (`exploration.h:28-94`)

| Field | Meaning | Form/convention |
|-------|---------|-----------------|
| `_board` | Pointer to the `Board*` in the buffer | - |
| `_children: robin_map<Move, ChildLink>` | Explored moves + edges | - |
| `_static_evaluation` | Pure evaluation (no search) | white-positive |
| `_deep_evaluation` | Evaluation after search/quiescence | white-positive (in principle) |
| `_is_stand_pat_eval` | Does `_deep_evaluation` come from a stand pat? | bool |
| `_iterations` | Number of GrogrosZero iterations that went through this node | - |
| `_nodes` | Number of nodes in the subtree | tree-local (broken if nodes are shared) |
| `_quiescence_depth` | Quiescence depth at which this node was evaluated | - |
| `_fully_explored`, `_can_explore`, `_is_terminal`, `_initialized` | State flags | - |
| `_parent_count` | Number of parents pointing at this node (for reset) | - |

### 4.2 `ChildLink` (`exploration.h:13`)

Per-edge statistics live here rather than on the child `Node`, because one `Node` could in principle be shared between several parents (eventually, through the TT). Sharing is **not** enabled today (see section 7).

```cpp
struct ChildLink {
    Node* _node = nullptr;
    int _chosen_iterations = 0;     // how many times pick_random_child chose it
    int _propagated_nodes = 0;      // nodes already counted in the parent's _nodes
};
```

---

## 5. GrogrosZero — the main loop

File: `exploration.cpp:139` — `Node::grogros_zero(...)`

### 5.1 Pseudocode

```
grogros_zero(N iterations):
  ensure_position_in_history(base_path)
  if !initialized:
    quiescence(...)                     # initial evaluation + tactical subtree
    initialized = true
  if terminal: return
  for i in 1..N:
    path = base_path.clone()            # independent branches for repetition state
    if some moves have not been visited at least once:
      explore_new_move(path)
    else:
      explore_random_child(path)        # UCT-like descent
```

### 5.2 `explore_new_move` (`exploration.cpp:212`)
1. Find the first unvisited move (or one that is not `_fully_explored`).
2. Create the child node (handling repetitions, which may make it a terminal draw node).
3. Run `child->quiescence(...)` to get a reliable tactical evaluation.
4. Update the parent's `_deep_evaluation` through `get_best_score_move`.
5. Mark the child `_fully_explored = true`.

### 5.3 `explore_random_child` (`exploration.cpp:365`)
1. `pick_random_child(...)` picks a child, weighted by score × exploration bonus.
2. Recurse into `child->grogros_zero(1 iteration)`.
3. Update the parent's `_deep_evaluation`.

### 5.4 Move selection: `pick_random_child` (`exploration.cpp:1046`)

It combines three factors (see `get_node_score`, `exploration.cpp:1278`):
- **eval_score**: `exp(alpha * (eval_value - max_eval))` — favours the best scores
- **score_score**: derived from `_avg_score` (winning chances)
- **win_adding**: a `_wdl.win_chance` bonus

Multiplied by `exploration_score = (parent_iter / child_iter)^gamma` (UCT-like).

**Top-5 bonus** (`exploration.cpp:1088-1119`): the five best moves are boosted by `{25, 8, 4, 3, 2}`.

### 5.5 ⚠️ The principal variation comes from a recursive `get_best_score_move`

`get_exploration_variants` (`exploration.cpp:560+`) calls `get_best_score_move(alpha, beta, true)` at every level to rebuild the PV. **Not** `get_most_explored_child_move`, which is what picks the move actually played.

---

## 6. Quiescence search (`exploration.cpp:636+`)

### 6.1 Signature
```cpp
int Node::quiescence(
    BoardBuffer*, Evaluator*,
    int depth,                  // remaining ply budget (starts positive, decreases)
    double search_alpha, search_beta,  // GrogrosZero hyper-parameters
    int alpha = -INT_MAX, int beta = INT_MAX,   // negamax bounds (side-to-move)
    Network*,
    bool evaluate_threats = true,
    int beta_margin = 0,
    const PositionHistory* path_history = nullptr
);
```

Returns: a score from the **point of view of the side to move** at the current node.

### 6.2 Flow
1. Initialise the node, computing the static evaluation if needed.
2. If terminal: return `_value * color`.
3. `stand_pat = _value * color` (side-to-move).
4. **TT probe** (see section 7).
5. Cutoffs: depth ≤ -4 (emergency), and depth ≤ 0 while not in check (stand-pat return).
6. Evaluate the opponent's threat → adjust `beta_margin`.
7. Stand pat ≥ beta + margin → beta cutoff.
8. Raise alpha if `stand_pat > alpha`.
9. Loop over captures / checks / promotions:
   - Progressive depth reduction (`move_index * 2`).
   - Delta pruning (skip if `stand_pat + best_gain + delta < alpha`).
   - Create the child and recurse: `score = -child->quiescence(... -beta, -alpha)`.
   - Update the parent's `_deep_evaluation` through `get_best_score_move`.
   - Beta cutoff if `score >= beta`.
10. Final **TT store** with `TTFlag = (alpha <= original_alpha) ? TT_ALPHA : TT_EXACT`.

### 6.3 Depth
- `depth > 0`: normal search.
- `depth == 0` (and not in check): return the stand pat.
- `depth ≤ -4`: emergency cutoff.

---

## 7. Transposition table (TT)

Used only inside `quiescence` — the main search never probes or stores. That limitation, and the measurements behind it, are tracked in `BUGFIXES.md` #11.

### 7.1 Structure (`zobrist.h:30`)
```cpp
struct ZobristEntry {
    int _eval;     // value from the side-to-move point of view
    int _depth;    // search depth at the time of the store
    TTFlag _flag;  // TT_EXACT | TT_ALPHA (upper) | TT_BETA (lower)
};
```

### 7.2 Probe (`exploration.cpp:739`)
```cpp
if (tt_entry && tt_entry->_depth >= depth) {
    if TT_EXACT:    cut immediately, return tt_eval
    if TT_BETA && tt_eval >= beta:    cut, return beta
    if TT_ALPHA && tt_eval <= alpha:  cut, return alpha
    also: _deep_evaluation._value = (tt_eval | beta | alpha) * color
}
```

### 7.3 Store
Store sites inside `quiescence`:
| Line | Value | Flag | Context |
|------|-------|------|---------|
| 770 | `stand_pat` | TT_EXACT | emergency cutoff (depth ≤ -4) ⚠️ not actually exact |
| 778 | `stand_pat` | TT_EXACT | depth ≤ 0 and not in check ⚠️ not actually exact |
| 801 | `beta` | TT_BETA | stand_pat ≥ beta + margin |
| 982 | `beta` | TT_BETA | beta cutoff after searching a child |
| 996 | `alpha` | TT_ALPHA on fail-low, otherwise TT_EXACT | normal exit |

### 7.4 Replacement policy
`zobrist.cpp:109`: the entry with the **greater** depth is kept (depth-preferred).

### 7.5 ⚠️ Known TT pitfalls

#### a) `_deep_evaluation` only partially overwritten
On a TT cutoff, only `_value` is updated — **not** `_avg_score`, `_wdl` or `_uncertainty`. A parent inheriting through `_deep_evaluation = _children[best]._node->_deep_evaluation` receives an **inconsistent** evaluation (a mate value alongside the `_avg_score` of an ordinary position). `get_node_score` combines those fields, so the ranking can come out wrong.

#### b) Mate scores and `_moves_count`
`_moves_count` is not part of the Zobrist key. Two paths of different lengths to the same physical position share a key but carry different mate evaluations. The standard fix is to normalise on store (a mate score relative to the current ply) and de-normalise on probe.

#### c) "Exact" cutoffs that are not exact
The emergency cutoff and the stand-pat return are stored as `TT_EXACT` when they are only stand-pat approximations. They should be `TT_ALPHA`, or carry a dedicated new flag.

#### d) Bound information is lost
`TT_BETA` stores `beta` rather than `score` (which was ≥ beta). A mate score (≫ beta) is overwritten by beta, leaving a lower bound that is too loose.

#### e) Persistence between searches
The TT is not systematically cleared between searches, so entries from a previous session pollute the current one. Combined with (b), this produces the "ghost" bugs.

#### f) No Node sharing through the TT
The code explicitly notes (`exploration.cpp:272-273`) that `Node` objects are **not** shared through the TT: it would break `_nodes` and backpropagation, and create cycles. The TT stores scalar evaluations only.

---

## 8. Repetitions — path-local history

File: `exploration.cpp:5-39`

### 8.1 Principle
Position history belongs to the **current exploration branch**, and is not attached to the `Node` — because the same position reached by two different paths has two different histories.

### 8.2 Type
```cpp
using PositionHistory = RepetitionHistory;  // robin_map<uint64, uint8>
```
Key: the Zobrist key. Value: the visit count along this path.

### 8.3 Building the child path history
`make_child_path_history(parent_path, parent_board, move)`:
- If the move is irreversible (capture / pawn push / castling) → return an empty history (nothing can repeat across an irreversible move).
- Otherwise → clone the parent's history.

### 8.4 Detection
```cpp
position_is_draw_by_repetition(path, board) {
    return path_count(board) + 1 >= search_repetition_limit;  // currently 2
}
```
**Note**: the limit is 2 visits — in practice the second occurrence is treated as a draw, so this is effectively 2-fold, not strict 3-fold.

### 8.5 Cloning per iteration
`grogros_zero` clones `base_path` on every outer iteration (`exploration.cpp:185`), so sibling branches cannot pollute each other.

---

## 9. Computing the "best move" — the flow

### 9.1 `get_best_score_move(alpha, beta, consider_standpat, qdepth)` (`exploration.cpp:1330`)
1. Compute `max_eval` and `max_avg_score` over all children (and the stand pat, if requested).
2. For each child: `score = child->get_node_score(alpha, beta, max_eval, max_avg_score, parent_player)`.
3. Return the move with the highest score.

### 9.2 `get_node_score` (`exploration.cpp:1278`)
```cpp
eval_score = exp(alpha * (eval._value * color - max_eval)) + min_constant
score_score = exp(-beta * (1 - avg_score) / (1 - max_avg_score) * max_avg_score / avg_score) + min_constant
win_adding = (wdl.win_chance + 0.25 * avg_score) * 0.00025
score = eval_score * score_score + adding + win_adding
```

The **alpha/beta hyper-parameters** here are the GrogrosZero ones (the relative weight of evaluation against win rate), not the negamax bounds.

### 9.3 ⚠️ Subtlety
`get_node_score` **uses several `Evaluation` fields** (`_value`, `_avg_score`, `_wdl`). An inconsistency between them (see 7.5.a) can surface the wrong move even when `_value` is correct.

---

## 10. Lifecycle and cleanup

- **Buffers**: `BoardBuffer` and `NodeBuffer` are preallocated at startup (`main_gui.h:87`), and reset between major searches.
- **`Node::reset(recursive=true)`** (`exploration.cpp:415`):
  - Decrements the children's `_parent_count`; recurses into a child only when its `_parent_count <= 0`.
  - This is what will allow node sharing later without a double free.
- **TT clear**: called from various places (see 7.5.e), but **not** systematically between searches.

---

## 11. Recurring pitfalls — checklist before touching the code

1. **Sign convention**: white-positive or side-to-move? For every variable.
2. **Mate scores**: is `_moves_count` consistent between the store and the probe?
3. **Consistent evaluation**: if `_value` is modified, are the other fields (`_avg_score`, `_wdl`) modified too?
4. **Path-local state**: repetition state must NEVER be stored in `Node` or `Board`.
5. **TT vs node sharing**: the TT stores **evaluations**, not nodes.
6. **Reset between searches**: buffers + TT + tree.
7. **Signed depth**: `depth` can be negative (down to -4) in quiescence.
8. **Negamax**: `score = -child->quiescence(... -beta, -alpha)`. Always negate and swap the bounds.
9. **`_iterations` vs `_chosen_iterations`**: `_iterations` lives on the `Node` (across all parents), `_chosen_iterations` on the `ChildLink` edge (per parent).
10. **`get_best_score_move` vs `get_most_explored_child_move`**: the first for the PV, the second for the move finally played.

---

## 12. Suggested TT fixes

(Hypotheses about the origin of the "a worse move ranks best" bugs described during the session. Their current status is tracked in `BUGFIXES.md`.)

### Priority 1 — `_deep_evaluation` coherence on a TT cutoff
Either store the whole `Evaluation` in `ZobristEntry`, or **do not overwrite** `_deep_evaluation` on a cutoff (just use the return value). Storing the complete `Evaluation` is preferable if memory allows; otherwise leave `_deep_evaluation` alone. *(Addressed — see `BUGFIXES.md` #1 and #14.)*

### Priority 2 — Ply-relative mate scores in the TT
On store:
```cpp
int adjusted = is_mate(eval) ? eval - sign(eval) * current_ply_offset : eval;
```
On probe: invert it. The `current_ply_offset` can be `_moves_count * mate_ply`, which is consistent with the existing encoding. *(Implemented — see `BUGFIXES.md` #3.)*

### Priority 3 — A dedicated flag for a stored stand pat
Add `TT_STANDPAT` (or flag the depth ≤ 0 cutoffs `TT_ALPHA`). Do not claim `TT_EXACT` for an approximation. *(Implemented — see `BUGFIXES.md` #4.)*

### Priority 4 — Fix `Evaluation::operator<` (`board.h:512-520`)
The "not evaluated" branches are identical to those of `operator>`. Invert them:
```cpp
bool operator<(Evaluation& other) {
    if (!other._evaluated) return false;
    if (!_evaluated) return true;
    return _value < other._value;
}
```

### Priority 5 — Clear the TT systematically between searches
At the start of every new search from a different root, unless incremental reuse is genuinely wanted.

---

## Appendix — Where things live

| Topic | File | Key symbols |
|-------|------|-------------|
| Mate scoring | `board.cpp:1569, 3305` | `mate_value`, `mate_ply`, `_moves_count` |
| Negamax loop | `exploration.cpp:636-1002` | `Node::quiescence` |
| GrogrosZero | `exploration.cpp:139-209` | `Node::grogros_zero` |
| Move selection (PV) | `exploration.cpp:1330` | `Node::get_best_score_move` |
| Move selection (sampling) | `exploration.cpp:1046` | `Node::pick_random_child` |
| Score formula | `exploration.cpp:1278` | `Node::get_node_score` |
| TT probe/store | `exploration.cpp:739, 770-996` | `transposition_table.{probe,store}` |
| TT structure | `zobrist.h:30-67` | `ZobristEntry`, `TranspositionTable` |
| Repetitions | `exploration.cpp:5-39` | `PositionHistory`, `make_child_path_history` |
| Evaluation struct | `board.h:461-561` | `Evaluation`, `>` / `<` operators |
| Evaluation flow | `exploration.cpp:1037` | `Node::evaluate_position` |


---

## AUDIT ALGORITHMIQUE — 2026-08-25 (post-défauts legacy)

Périmètre : `exploration_diag.cpp` (version compilée), `zobrist.h/cpp`. État : toggles
`g_search_*` = false (config legacy gagnante), `g_tt_main_search`/`g_tt_node_dag` = false.

### Constats confirmés, par priorité

**A1. TT — politique de remplacement multi-échelle incohérente** (`zobrist.cpp:store`)
La règle « on garde la plus grande profondeur » compare deux échelles qui n'ont pas le
même sens : la quiescence stocke des profondeurs de ply (-4..+10), la recherche principale
(Plan A) stocke `QDEPTH_BAND(256)+log2(nœuds)`. Toute entrée main-search bloque donc à
jamais les mises à jour quiescence d'une même position (256 > tout ply), et inversement
une entrée profonde de quiescence gèle les réécritures. Bénin aujourd'hui (une seule
échelle active), **bloquant dès l'activation du TT principal**. Pistes : champ
`generation/scale` dans `ZobristEntry`, ou préférence profondeur + âge dans la même échelle.

**A2. TT — aucun plafond mémoire effectif** (`TranspositionTable::init`)
`_length` est stocké mais jamais appliqué : la robin_map croît sans borne pendant une
recherche (des millions d'entrées possibles sur une partie longue → RAM + cache-misses).
Prévoir un éviction when-full (batches aléatoires ou âge).

**A3. Quiescence — LMR aveugle sur les captures** (`exploration_diag.cpp:1603`)
`new_depth -= move_index * 2` s'applique aux captures sans notion de SEE/MVV-LVA :
après ~5 coups explorés, les grosses captures tardives sortent en `new_depth <= 0`
et sont purement sautées. Candidat : exempter les captures « gagnantes » (MVV-LVA > 0)
ou réduire de 1 au lieu de 2. À valider par NAC + selfplay.

**A4. Extension d'échec câblée mais désactivée** (`exploration_diag.cpp:1598`,
`check_extension = 0`) : les cas « Search stops before the end of the line » documentés
en tête de fonction pointent tous vers ça. A/B candidat naturel (1/2 de bonus).

**A5. Asymétrie de backup stand-pat entre les deux chemins**
`explore_new_move` (ligne 785) préserve le stand-pat tant que tous les coups ne sont pas
explorés ; `explore_random_child` (ligne 926) écrase avec le max des enfants SANS cette
garde côté value-propagation. Le chemin legacy passe par `get_best_score_move` dont
`consider_standpat` est vrai par défaut. Incohérence de sémantique entre toggles —
à unifier si value-propagation revient un jour.

**A6. Divers mineurs**
- `test=false` mort dans `explore_new_move` (687-699) : branche expérimentale morte.
- Commentaire dupliqué 1601-1602 (cosmétique).
- `get_node_score` : formule avg_term convolue mais bornée (ε) ; cap conditionnel
  (`g_search_avg_cap`) — comportement documenté.
- `probe()` retourne un pointeur dans la map : fragile si un store survient avant lecture
  complète (usage actuel : copie immédiate ✓).
- `tt_writeback_depth(_nodes)` : _nodes = taille de sous-arbre, pas une profondeur —
  même famille que A1.

### Points vérifiés SAINS
Cycle de vie node_map/DAG (erase au recyclage, purges centralisées #6), gardes
null-move/broken-edge/proven-win, comptabilité `_propagated_nodes` hors DAG,
`PositionHistory` push/pop par PathScope, détection de répétition path-local sous DAG,
`minimal_quiescence`, garde NaN de `get_best_score_move`.

### Ordre d'attaque proposé
1. A1+A2 ensemble (même fichier, tests TT dédiés : remplacement inter-échelles, plafond)
2. A3/A4 en A/B mesurés (NAC + ladders + selfplay 8×2000)
3. A5 seulement si value-propagation est réanimé
