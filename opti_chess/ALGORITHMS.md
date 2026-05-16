# OptiChess — Documentation technique des algorithmes

> Référence interne. Décrit la **partie algorithmique** (recherche, évaluation, TT, répétitions).
> Couvre `exploration.cpp/h`, `zobrist.cpp/h` et les hooks dans `board.cpp`.
> Ne décrit pas le GUI ni la génération de coups.

---

## 1. Vue d'ensemble

```
                ┌─────────────────────────────────────────┐
                │ grogros_zero (boucle principale)        │
                │  - itère N fois                         │
                │  - choisit: nouveau coup OU re-explorer │
                └────────────┬───────────────────┬────────┘
                             │                   │
                     explore_new_move    explore_random_child
                             │                   │
                             ▼                   ▼
                       quiescence (init)   grogros_zero (récursif)
                             │
                             ▼
                       quiescence (recherche tactique)
                             │
                             ▼
                  evaluate_position (statique)
```

**Objets principaux**

| Objet | Rôle | Fichier |
|-------|------|---------|
| `Board` | État physique de l'échiquier + Zobrist key + moves | `board.h` |
| `Node` | Noeud de l'arbre de recherche : 1 `Board*`, enfants, évals, stats | `exploration.h` |
| `ChildLink` | Arête parent→enfant : pointeur Node + iterations chosen + nodes propagés | `exploration.h` |
| `Evaluation` | `{_value, _avg_score, _wdl, _uncertainty, _winnable_*, _evaluated}` | `board.h:461` |
| `BoardBuffer` / `NodeBuffer` | Pools mémoire (zéro allocation en recherche) | `buffer.h` |
| `TranspositionTable` | `robin_map<uint64, ZobristEntry>` | `zobrist.h` |
| `PositionHistory` = `RepetitionHistory` | `robin_map<uint64, uint8>` path-local | `exploration.h:9` |

---

## 2. ⚠️ Conventions de signes (CRITIQUE)

Le code mélange **deux conventions** sur les valeurs d'évaluation. Cette section est la **plus importante** à maîtriser avant de toucher à quoi que ce soit.

### 2.1 Convention "white-positive" (absolue)

`Evaluation::_value` est en **forme absolue** :
- `_value > 0` ⇒ avantage blanc
- `_value < 0` ⇒ avantage noir

Tous les champs de `Evaluation` (statique, deep) sont en cette forme.
Source du mate score : `board.cpp:1569` :
```cpp
eval->_value = (-mate_value + _moves_count * mate_ply) * get_color();
```
Le `* get_color()` ramène à la convention blanc-positif (le côté maté = côté au trait à la position terminale).

### 2.2 Convention "side-to-move" (POV du trait)

À l'intérieur de `quiescence()` (négamax), tout est en **POV du trait** :
- `stand_pat`, `alpha`, `beta`, `score`, valeurs stockées/retournées par la TT
- Conversion : `stm_value = _value * color` où `color = _board->get_color()` (+1 blanc, -1 noir)
- Le retour de `quiescence` est **en POV du trait courant**

### 2.3 Frontière entre les deux

| Endroit | Forme |
|---------|-------|
| `Evaluation::_value` (membres `_static_evaluation`, `_deep_evaluation`) | **blanc-positif** |
| `stand_pat`, `alpha`, `beta`, valeur retournée par `quiescence`, eval stocké dans TT | **POV du trait** |
| `child->quiescence(... -beta, -alpha)` puis `score = -child->quiescence(...)` | négation classique négamax |
| `_value * color` | conversion blanc-positif → POV trait |
| `tt_eval * color` | conversion POV trait → blanc-positif (utilisée dans le cutoff TT) |

**Règle d'or** : à chaque manipulation d'une valeur, se demander si elle est blanc-positif ou POV-trait. La majorité des bugs proviennent d'une confusion ici.

---

## 3. Évaluation et scores de mat

### 3.1 Constantes (`board.h:31-34`)
```cpp
constexpr int mate_value = 1e8;   // 100_000_000
constexpr int mate_ply  = 1e5;    // 100_000
```

### 3.2 Encodage du mat (`board.cpp:1569`)
```cpp
eval->_value = (-mate_value + _moves_count * mate_ply) * get_color();
```

- `_moves_count` est le **numéro de coup absolu de la partie** (FEN field, incrémente après le coup noir, `board.cpp:1295`).
- Plus on est avancé dans la partie, plus `|_value|` est petit (le mat est "moins urgent").
- **Distance de mat encodée par `_moves_count`, pas par la profondeur de recherche depuis la racine.**

### 3.3 Détection de mat dans une éval (`board.cpp:3305-3306`)
```cpp
if (10 * abs_eval > mate_value) { // c'est un mate score
    int mate_moves = static_cast<int>(mate_value - abs_eval - _moves_count * mate_ply)
                     * (e > 0 ? 1 : -1) / mate_ply + (_player && e > 0);
}
```

### 3.4 ⚠️ Conséquence : `_moves_count` n'est PAS dans la clé Zobrist

`zobrist.h:13-16` : la clé Zobrist hashe pièces + trait + roques + en-passant. Pas `_moves_count`.

Donc deux chemins de recherche atteignant la même position physique avec des `_moves_count` différents (typique en transposition) auraient la même clé Zobrist mais des scores de mat différents. Cf. section 7 (TT).

---

## 4. `Node` : invariants et champs critiques

### 4.1 Champs (`exploration.h:28-94`)

| Champ | Sens | Forme/Convention |
|-------|------|------------------|
| `_board` | Pointeur vers `Board*` du buffer | - |
| `_children: robin_map<Move, ChildLink>` | Coups explorés + arêtes | - |
| `_static_evaluation` | Éval pure (sans recherche) | blanc-positif |
| `_deep_evaluation` | Éval après recherche/quiescence | blanc-positif (théorique) |
| `_is_stand_pat_eval` | `_deep_evaluation` provient-elle d'un stand pat ? | bool |
| `_iterations` | Nb d'itérations GrogrosZero ayant traversé ce noeud | - |
| `_nodes` | Nb de noeuds dans le sous-arbre | tree-local (cassé si partage) |
| `_quiescence_depth` | Profondeur de quiescence à laquelle ce noeud a été évalué | - |
| `_fully_explored`, `_can_explore`, `_is_terminal`, `_initialized` | Flags d'état | - |
| `_parent_count` | Nb de parents pointant sur ce noeud (pour reset) | - |

### 4.2 `ChildLink` (`exploration.h:13`)

Les stats par-arête vivent ici, pas sur le `Node` enfant, parce qu'un même Node pourrait théoriquement être partagé entre plusieurs parents (à terme, via la TT). Aujourd'hui le partage n'est **pas** activé (cf. section 7).

```cpp
struct ChildLink {
    Node* _node = nullptr;
    int _chosen_iterations = 0;     // nombre de fois choisi par pick_random_child
    int _propagated_nodes = 0;      // nb de noeuds déjà comptés dans _nodes du parent
};
```

---

## 5. GrogrosZero — boucle principale

Fichier : `exploration.cpp:139` — `Node::grogros_zero(...)`

### 5.1 Pseudocode

```
grogros_zero(N itérations):
  ensure_position_in_history(base_path)
  si !initialized:
    quiescence(...)                     # init eval + sous-arbre tactique
    initialized = true
  si terminal: return
  pour i de 1 à N:
    path = base_path.clone()            # branches indépendantes pour les répétitions
    si tous les coups pas encore visités au moins une fois:
      explore_new_move(path)
    sinon:
      explore_random_child(path)        # descente UCT-like
```

### 5.2 `explore_new_move` (`exploration.cpp:212`)
1. Trouve le 1er coup non visité (ou pas `_fully_explored`).
2. Crée le noeud fils (avec gestion des répétitions → noeud terminal nulle).
3. Lance `child->quiescence(...)` pour avoir une éval tactique fiable.
4. Met à jour `_deep_evaluation` du parent via `get_best_score_move`.
5. Marque le fils `_fully_explored = true`.

### 5.3 `explore_random_child` (`exploration.cpp:365`)
1. `pick_random_child(...)` choisit un fils pondéré par score × bonus d'exploration.
2. Récursion `child->grogros_zero(1 itération)`.
3. Met à jour `_deep_evaluation` parent.

### 5.4 Sélection des coups : `pick_random_child` (`exploration.cpp:1046`)

Combine 3 facteurs (cf. `get_node_score`, `exploration.cpp:1278`) :
- **eval_score** : `exp(alpha * (eval_value - max_eval))` — favorise les meilleurs scores
- **score_score** : depuis `_avg_score` (chances de gain)
- **win_adding** : bonus `_wdl.win_chance`

Multiplié par `exploration_score = (parent_iter / child_iter)^gamma` (UCT-like).

**Bonus top-5** (`exploration.cpp:1088-1119`) : les 5 meilleurs coups sont boostés par `{25, 8, 4, 3, 2}`.

### 5.5 ⚠️ Variante principale = `get_best_score_move` récursif

`get_exploration_variants` (`exploration.cpp:560+`) appelle `get_best_score_move(alpha, beta, true)` à chaque niveau pour reconstruire la PV. **PAS** `get_most_explored_child_move` (qui sert au coup à jouer final).

---

## 6. Quiescence search (`exploration.cpp:636+`)

### 6.1 Signature
```cpp
int Node::quiescence(
    BoardBuffer*, Evaluator*,
    int depth,                  // budget de plies restant (start positif, décroît)
    double search_alpha, search_beta,  // hyper-params GrogrosZero
    int alpha = -INT_MAX, int beta = INT_MAX,   // bornes négamax (POV trait)
    Network*,
    bool evaluate_threats = true,
    int beta_margin = 0,
    const PositionHistory* path_history = nullptr
);
```

Retour : score en **POV du trait** du noeud courant.

### 6.2 Flux
1. Init noeud, calcule éval statique si nécessaire.
2. Si terminal : retourne `_value * color`.
3. `stand_pat = _value * color` (POV trait).
4. **TT probe** (cf. section 7).
5. Cutoffs : depth ≤ -4 (emergency), depth ≤ 0 et non-échec (stand pat return).
6. Évalue la menace adverse → ajuste `beta_margin`.
7. Stand pat ≥ beta + marge → cutoff beta.
8. Met à jour alpha si stand_pat > alpha.
9. Boucle sur captures/échecs/promotions :
   - Réduction de profondeur progressive (`move_index * 2`).
   - Delta pruning (skip si `stand_pat + best_gain + delta < alpha`).
   - Crée fils + récursion `score = -child->quiescence(... -beta, -alpha)`.
   - Met à jour `_deep_evaluation` parent via `get_best_score_move`.
   - Beta cutoff si `score >= beta`.
10. **TT store** final avec `TTFlag = (alpha <= original_alpha) ? TT_ALPHA : TT_EXACT`.

### 6.3 Profondeur
- `depth > 0` : recherche normale.
- `depth == 0` (et !in_check) : retourne stand_pat.
- `depth ≤ -4` : emergency cutoff.

---

## 7. Table de transposition (TT)

État actuel : **freshly implemented** (modifs locales). Utilisée uniquement dans `quiescence`.

### 7.1 Structure (`zobrist.h:30`)
```cpp
struct ZobristEntry {
    int _eval;     // valeur en POV du trait
    int _depth;    // profondeur de recherche au moment du store
    TTFlag _flag;  // TT_EXACT | TT_ALPHA (upper) | TT_BETA (lower)
};
```

### 7.2 Probe (`exploration.cpp:739`)
```cpp
if (tt_entry && tt_entry->_depth >= depth) {
    si TT_EXACT:    cutoff direct, retourne tt_eval
    si TT_BETA && tt_eval >= beta:    cutoff, retourne beta
    si TT_ALPHA && tt_eval <= alpha:  cutoff, retourne alpha
    aussi: _deep_evaluation._value = (tt_eval | beta | alpha) * color
}
```

### 7.3 Store
Points de stockage dans `quiescence` :
| Ligne | Valeur | Flag | Contexte |
|-------|--------|------|----------|
| 770 | `stand_pat` | TT_EXACT | emergency cutoff (depth ≤ -4) ⚠️ pas vraiment exact |
| 778 | `stand_pat` | TT_EXACT | depth ≤ 0 et !in_check ⚠️ pas vraiment exact |
| 801 | `beta` | TT_BETA | stand_pat ≥ beta + margin |
| 982 | `beta` | TT_BETA | beta cutoff après recherche d'un fils |
| 996 | `alpha` | TT_ALPHA si fail-low, sinon TT_EXACT | sortie normale |

### 7.4 Politique de remplacement
`zobrist.cpp:109` : on garde l'entrée de profondeur **maximale** (depth-preferred).

### 7.5 ⚠️ Pièges connus de la TT

#### a) `_deep_evaluation` partiellement écrasée
Au cutoff TT, seul `_value` est mis à jour, **pas** `_avg_score`, `_wdl`, `_uncertainty`. Le parent qui hérite via `_deep_evaluation = _children[best]._node->_deep_evaluation` reçoit une éval **incohérente** (valeur de mat mais `_avg_score` d'une position normale). `get_node_score` combine ces champs → mauvais classement possible.

#### b) Mate scores et `_moves_count`
`_moves_count` n'est pas dans la Zobrist key. Deux chemins de longueurs différentes vers la même position physique ⇒ même clé, mais évals de mat différentes. Fix standard : normaliser au stockage (mate score relatif au ply courant) et dé-normaliser au probe.

#### c) Cutoffs "exacts" qui ne le sont pas
Emergency cutoff et stand-pat return stockés en `TT_EXACT` alors que ce ne sont que des approximations stand pat. Devraient être `TT_ALPHA` ou un nouveau flag dédié.

#### d) Perte d'info sur les bornes
`TT_BETA` stocke `beta` au lieu de `score` (qui était ≥ beta). Un score de mat (>> beta) est écrasé par beta, et future borne inférieure trop lâche.

#### e) Persistance entre recherches
La TT n'est pas systématiquement clearée entre recherches. Les entrées d'une session précédente polluent. Combiné avec (b), donne des bugs "fantômes".

#### f) Pas de partage de Node via la TT
Le code commente explicitement (`exploration.cpp:272-273`) qu'on ne partage **pas** les `Node` via la TT : ça casserait `_nodes`, la backpropagation et créerait des cycles. La TT ne stocke que des évals scalaires.

---

## 8. Répétitions — path-local history

Fichier : `exploration.cpp:5-39`

### 8.1 Principe
L'historique des positions est **propre à la branche d'exploration courante**, pas attaché au `Node` (parce que la même position via deux chemins différents a deux historiques différents).

### 8.2 Type
```cpp
using PositionHistory = RepetitionHistory;  // robin_map<uint64, uint8>
```
Clé : Zobrist. Valeur : compte de visites sur ce chemin.

### 8.3 Construction du child path history
`make_child_path_history(parent_path, parent_board, move)` :
- Si coup irréversible (capture / poussée de pion / roque) → retourne historique vide (rien ne peut se répéter au-delà d'un coup irréversible).
- Sinon → clone de l'historique parent.

### 8.4 Détection
```cpp
position_is_draw_by_repetition(path, board) {
    return path_count(board) + 1 >= search_repetition_limit;  // = 2 actuellement
}
```
**Note** : la limite est 2 visites (en réalité on traite la 2e occurrence comme nulle, donc effectivement 2-fold, pas 3-fold strict).

### 8.5 Clonage par itération
`grogros_zero` clone `base_path` à chaque outer iteration (`exploration.cpp:185`), pour que des branches sœurs ne se polluent pas.

---

## 9. Calcul du "meilleur coup" — flux

### 9.1 `get_best_score_move(alpha, beta, consider_standpat, qdepth)` (`exploration.cpp:1330`)
1. Calcule `max_eval` et `max_avg_score` sur tous les enfants (et stand pat si demandé).
2. Pour chaque enfant : `score = child->get_node_score(alpha, beta, max_eval, max_avg_score, parent_player)`.
3. Retourne le coup au score max.

### 9.2 `get_node_score` (`exploration.cpp:1278`)
```cpp
eval_score = exp(alpha * (eval._value * color - max_eval)) + min_constant
score_score = exp(-beta * (1 - avg_score) / (1 - max_avg_score) * max_avg_score / avg_score) + min_constant
win_adding = (wdl.win_chance + 0.25 * avg_score) * 0.00025
score = eval_score * score_score + adding + win_adding
```

**Hyper-paramètres alpha/beta** ici sont les hyper-params GrogrosZero (poids relatif eval vs winrate), pas les bornes négamax.

### 9.3 ⚠️ Subtilité
`get_node_score` **utilise plusieurs champs de Evaluation** (`_value`, `_avg_score`, `_wdl`). Une incohérence entre ces champs (cf. 7.5.a) peut faire ressortir le mauvais coup même si `_value` est correct.

---

## 10. Lifecycle & nettoyage

- **Buffers** : `BoardBuffer` et `NodeBuffer` préalloués au démarrage (`main_gui.h:87`). Reset entre recherches majeures.
- **`Node::reset(recursive=true)`** (`exploration.cpp:415`) :
  - Décrémente `_parent_count` des enfants ; reset récursif uniquement si `_parent_count <= 0`.
  - Permet le futur partage de noeuds sans double-free.
- **TT clear** : appelée à divers points (cf. section 7.5.e), mais **pas** systématique entre recherches.

---

## 11. Pièges récurrents / checklist avant de toucher au code

1. **Convention de signe** : valeur blanc-positif ou POV-trait ? À chaque variable.
2. **Mate scores** : `_moves_count` est-il cohérent entre store et probe ?
3. **Évaluation cohérente** : si `_value` est modifié, les autres champs (`_avg_score`, `_wdl`) le sont-ils aussi ?
4. **Path-local state** : la répétition ne doit JAMAIS être stockée dans Node/Board.
5. **TT vs Node sharing** : la TT stocke des **évals**, pas des noeuds.
6. **Reset entre recherches** : buffer + TT + arbre.
7. **Profondeur signée** : depth peut être négative (jusqu'à -4) en quiescence.
8. **Negamax** : `score = -child->quiescence(... -beta, -alpha)`. Toujours négation + swap des bornes.
9. **`_iterations` vs `_chosen_iterations`** : `_iterations` sur le `Node` (tous parents confondus), `_chosen_iterations` sur l'arête `ChildLink` (par-parent).
10. **`get_best_score_move` vs `get_most_explored_child_move`** : le 1er pour la PV, le 2e pour le coup à jouer final.

---

## 12. Suggestions de fixes (TT)

(Hypothèses sur l'origine des bugs "coup pire = meilleur" décrits dans la session.)

### Priorité 1 — Cohérence `_deep_evaluation` au cutoff TT
Soit on stocke toute la `Evaluation` dans `ZobristEntry`, soit on **n'écrase pas** `_deep_evaluation` au cutoff (juste utiliser la valeur de retour). Préférable : stocker l'`Evaluation` complète si la mémoire le permet, sinon ne pas toucher `_deep_evaluation`.

### Priorité 2 — Mate scores ply-relatifs dans la TT
Au store :
```cpp
int adjusted = is_mate(eval) ? eval - sign(eval) * current_ply_offset : eval;
```
Au probe : inverser. Le `current_ply_offset` peut être `_moves_count * mate_ply` (cohérent avec l'encodage existant).

### Priorité 3 — Flag dédié pour stand-pat stocké
Ajouter `TT_STANDPAT` (ou marquer `TT_ALPHA` pour les cutoffs depth ≤ 0). Ne pas dire `TT_EXACT` pour des approximations.

### Priorité 4 — Fix de `Evaluation::operator<` (`board.h:512-520`)
Les branches "unévalué" sont identiques à `operator>`. Inverser :
```cpp
bool operator<(Evaluation& other) {
    if (!other._evaluated) return false;
    if (!_evaluated) return true;
    return _value < other._value;
}
```

### Priorité 5 — Clear systématique TT entre recherches
Au début de chaque nouvelle recherche depuis une racine différente (sauf si on veut vraiment incrémentaliser).

---

## Annexe — Where things live

| Topic | Fichier | Symboles clés |
|-------|---------|---------------|
| Mate scoring | `board.cpp:1569, 3305` | `mate_value`, `mate_ply`, `_moves_count` |
| Negamax loop | `exploration.cpp:636-1002` | `Node::quiescence` |
| GrogrosZero | `exploration.cpp:139-209` | `Node::grogros_zero` |
| Move selection (PV) | `exploration.cpp:1330` | `Node::get_best_score_move` |
| Move selection (sampling) | `exploration.cpp:1046` | `Node::pick_random_child` |
| Score formula | `exploration.cpp:1278` | `Node::get_node_score` |
| TT probe/store | `exploration.cpp:739, 770-996` | `transposition_table.{probe,store}` |
| TT structure | `zobrist.h:30-67` | `ZobristEntry`, `TranspositionTable` |
| Repetitions | `exploration.cpp:5-39` | `PositionHistory`, `make_child_path_history` |
| Evaluation struct | `board.h:461-561` | `Evaluation`, ops `>` / `<` |
| Eval flow | `exploration.cpp:1037` | `Node::evaluate_position` |
