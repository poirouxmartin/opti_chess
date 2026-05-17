# #11 — TT scalaire dans la recherche principale (Plan A) — SPEC

> Statut : design **validé** (utilisateur, 2026-05-17). Remplace le doc de
> brainstorming interrompu. Prochaine étape : `superpowers:writing-plans`.
> Pré-requis TT-correctness (#4 stand-pat, #14 cohérence value↔score, #3 mat
> ply-relatif) ✅ corrigés & validés runtime — Plan A est désormais *sain*.

## Objectif

Sortir la TT de la seule `quiescence()` pour attaquer la redondance
combinatoire des transpositions dans l'arbre de recherche principal (MCTS-like
`grogros_zero`). Deux mouvements :

1. **probe** dans `explore_new_move` *avant* la quiescence du nouveau fils ;
2. **write-back** de la `_deep_evaluation` raffinée des nœuds principaux dans
   la TT — le levier réel : sans write-back on ne relit que des feuilles de
   quiescence peu profondes.

Critère de succès : « même jeu, moins de travail » (moins de nœuds / temps à
budget d'itérations égal sur positions à transpositions) **et** un gain de
profondeur visible. Behavior-preserving autant que possible.

## Constat de conception (de-risk majeur)

La sélection MCTS ne dépend **pas** de `_nodes`. Dans `pick_random_child`
(`exploration.cpp:1224`) : `score = move_score × exploration_score` avec
- `move_score = get_node_score(...)` = fonction **pure** de `_deep_evaluation`
  (value / avg_score / wdl) — aucun terme `_nodes` (`:1361-1410`) ;
- `exploration_score = pow(_iterations / child_iterations, γ)` —
  `child_iterations = max(_chosen_iterations, child->_iterations)`, **pas
  `_nodes`** (`:1215-1221`) ;
- `child->_can_explore` (`:1281`) garde déjà la descente : un fils non
  explorable contribue son éval au backup parent mais n'est jamais redescendu.

Conséquence : un fils réutilisé depuis la TT n'a besoin d'**aucun** proxy de
visites/`_nodes` synthétique pour la sélection. Il lui faut seulement une
`_deep_evaluation` cohérente. Structurellement il est **identique à la feuille
de nulle-par-répétition existante** (`exploration.cpp:285-292` :
`_nodes=1`, `_iterations=1`, non expansible). Le gain combinatoire est gratuit :
le fils reste `_nodes=1` (pas de sous-arbre redondant) tout en portant la
valeur *profonde* issue du write-back.

Le seul vrai point de conception restant : le **proxy de profondeur du
write-back** + le **seuil de confiance**, parce que `ZobristEntry._depth` est
aujourd'hui la petite profondeur de quiescence et que le remplacement est
depth-preferred (`zobrist.cpp:113`) — un write-back doit vivre dans une bande
supérieure pour que la porte de réutilisation distingue « sous-arbre raffiné
profond » de « feuille de quiescence ».

## Décisions arrêtées (utilisateur, 2026-05-17)

1. **Stance de réutilisation** : *feuille gelée*. Un hit TT fiable devient un
   nœud feuille non expansible portant la valeur profonde de la TT. Gain de
   travail / profondeur maximal ; le remplacement depth-preferred + une porte
   de réutilisation stricte gardent les entrées profondes.
2. **Proxy de profondeur du write-back** : *bande `log2(_nodes)`*
   `depth = QDEPTH_BAND + floor(log2(_nodes+1))`. O(1), monotone avec la taille
   du sous-arbre, borné, nettement séparé de la bande de quiescence.
3. **Gating** : *toggle runtime, défaut OFF*. Même binaire pour l'A/B,
   kill-switch immédiat, coût nul quand OFF.

## Conception détaillée

### 1. Gating

Booléen runtime global (p. ex. `tt_main_search`), **défaut OFF**, placé auprès
des toggles de recherche existants (style `_explore_checks` /
`_quiescence_depth`), lu en lecture seule aux sites probe et write-back.
OFF ⇒ comportement actuel au byte près ; les deux branches ajoutées sont dans
`explore_new_move` / `explore_random_child`, **hors** des boucles serrées
d'éval / génération de coups. Même binaire pour A/B ; kill-switch immédiat.

### 2. Probe (dans `explore_new_move`, branche nouveau nœud)

Après `new_board->get_zobrist_key()` (`exploration.cpp:298`) et la création du
`child`, **avant** le bloc `child->quiescence(...)` (`:316-331`) :

- `transposition_table.probe(new_board->_zobrist_key)` ;
- correction de la distance de mat :
  `tt_denormalize_mate(entry->_eval, new_board->_moves_count)` (identique au
  probe quiescence `:784`, satisfait #3) ;
- **porte de réutilisation** :
  `entry->_flag == TT_EXACT && entry->_depth >= QDEPTH_BAND + MIN_REUSE_LOG2`.
  Les bornes (`TT_STANDPAT` / `TT_BETA` / `TT_ALPHA`) ne sont **jamais**
  réutilisées comme valeur de nœud — sémantique de borne sans fenêtre de
  recherche en MCTS. La constante de bande garantit qu'on ne réutilise jamais
  une simple feuille de quiescence comme valeur profonde ;
- sur hit ⇒ **feuille TT gelée**, structurellement la feuille de
  nulle-par-répétition existante (`:285-292`) :
  - synthèse de `_deep_evaluation` en **réutilisant exactement** la logique de
    synthèse du bloc `tt_cutoff` (`:781-810`, conventions #1/#14), factorisée
    en un helper partagé : `_value` = valeur dé-canonisée, `_evaluated=true`,
    `_uncertainty=0` ; si mat (`10·|value| > mate_value`) `_winnable_*` par le
    signe ; puis `get_WDL()` + `get_average_score()` pour dériver
    `_wdl` / `_avg_score` de façon cohérente ;
  - `_nodes=1`, `_iterations=1`, `_can_explore=false`,
    `_fully_explored=true`, `_is_stand_pat_eval=false` ;
  - **on saute l'appel `quiescence()`**.
- Le `Board` alloué est conservé (c'est le board de la feuille) ; **pas de
  partage de `Node`** — on réutilise la *valeur*, pas l'objet (invariant #11).

### 3. Write-back (le levier)

En `exploration.cpp:381` (`explore_new_move`) et `:411`
(`explore_random_child`), juste après la copie de `_deep_evaluation` depuis le
meilleur fils :

- garde : toggle ON `&& !_is_terminal && !_is_stand_pat_eval &&
  _deep_evaluation._evaluated`. `!_is_terminal` exclut les feuilles de nulle
  par répétition (path-dependent — ne doivent pas être cachées en EXACT de
  position) ; côté probe c'est déjà sain car la répétition est court-circuitée
  avant le probe (`:280`) ;
- `transposition_table.store(_board->_zobrist_key,
  tt_normalize_mate(_deep_evaluation._value, _board->_moves_count),
  QDEPTH_BAND + floor(log2(_nodes+1)), TT_EXACT)`. Le remplacement
  depth-preferred (`zobrist.cpp:113`) garde l'entrée la plus profonde ;
  l'offset `QDEPTH_BAND` (au-dessus de toute profondeur de quiescence possible)
  classe chaque write-back MCTS au-dessus de chaque entrée de quiescence et
  permet à la porte de réutilisation d'exiger un *vrai sous-arbre raffiné*.

### 4. Constantes ajustables

- `QDEPTH_BAND` ≈ 256 (> profondeur de quiescence max possible).
- `MIN_REUSE_LOG2` = 4 au départ ⇒ réutiliser seulement les entrées
  représentant ≥ 16 nœuds raffinés.

Deux constantes compile-time, ajustées au gate runtime utilisateur.

### 5. Invariants & soundness

- `_nodes=1` garde `_nodes += child->_nodes` et les asserts
  `child_nodes >= nodes` corrects (comme la feuille de nulle).
- `_can_explore=false` ⇒ `pick_random_child:1281` ne redescend jamais.
- Sélection purement éval-driven ⇒ la `_deep_evaluation` synthétisée suffit ;
  aucun proxy de visites/`_nodes` synthétique nécessaire.
- Hérite de #6 (TT non clearée entre racines) **inchangé** — non aggravé,
  OFF par défaut, l'utilisateur clear à l'A/B.
- Perf : deux opérations TT O(1) sans allocation, à des points qui scannent
  déjà le meilleur fils ; hors boucles chaudes ; défaut OFF.
- Pas de réintroduction du partage de `Node` ; commits style utilisateur
  (ASCII FR, sans attribution IA).

### 6. Bench (critère de succès)

Position de référence : finale roi + pions bloqués (réf. vidéo Sebastian
Lague). Chemin existant `Tests::problem_test` (tests.cpp:122) → boucle
`GUI::grogros_analysis(iterations)` (gui.cpp:1020). Budget en **itérations
fixes** (pas wall-time) sur une FEN fixe, OFF vs ON même binaire. Mesurer
`_root_exploration_node->_nodes`, `get_main_depth(_alpha,_beta)`
(gui.cpp:1467), meilleur coup, temps.

Attendu ON : moins de nœuds **et** profondeur supérieure à itérations égales,
**même meilleur coup** sur positions à transpositions. Gate de régression :
PERFT 1/2 + EVALUATION stables (lancé par l'utilisateur).

## Hors périmètre

- Plan B (partage de nœuds / DAG) : seulement si A insuffisant pour le gain.
- #6 (clear TT entre racines), #5 (`TT_BETA` stocke `beta`), #7
  (`make_child_path_history` copie la map) : hygiène TT séparée, non bloquante
  pour A.
