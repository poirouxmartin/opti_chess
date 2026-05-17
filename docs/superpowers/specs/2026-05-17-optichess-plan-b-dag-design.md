# #11 Plan B — DAG de transpositions (partage de nœuds) — SPEC

> Statut : design **validé** (utilisateur, 2026-05-17). Prochaine étape :
> `superpowers:writing-plans`.
> Contexte : Plan A (TT scalaire dans la recherche principale) implémenté,
> revu, mais **empiriquement insuffisant** — la feuille scalaire gelée
> plafonne la profondeur et fait spinner la boucle MCTS (cf. `BUGFIXES.md`
> #11, mesure 2026-05-17). Le gain « type Lague » exige de *continuer le
> sous-arbre profond à travers la transposition* : c'est l'objet du Plan B.

## Objectif

Partager un même `Node` entre tous les chemins qui atteignent une position de
clé Zobrist identique (DAG général), pour éliminer la ré-exploration
combinatoire des sous-arbres transposés et rediriger le budget de recherche
vers la profondeur. Critère de succès (repris de #11) : **même jeu** ; sur la
position de référence (finale roi + pions bloqués, réf. vidéo Sebastian
Lague), à budget d'itérations fixe, toggle ON vs OFF : nettement **moins de
nœuds uniques par profondeur** et un **gain de profondeur visible** ;
PERFT 1/2 + EVALUATION stables (gate runtime utilisateur).

## Constat de faisabilité (vérifié dans le code)

Le code a **déjà anticipé** les transpositions ; les trois blocages
historiquement cités (« casse `_nodes`, backpropagation, cycles ») sont moins
durs qu'annoncé :

- **Répétition path-local** (`exploration.cpp:72-94`) : l'état de répétition
  est passé par la pile via `PositionHistory`, jamais stocké dans
  `Node`/`Board` (« must stay outside Node/Board state if we want
  transpositions later »). Un `Node` partagé ne porte donc aucune information
  path-dépendante.
- **Backprop pull-based** : `_deep_evaluation` est recopié du meilleur fils à
  chaque visite (`exploration.cpp:411`, `:381`) — pas d'accumulation push.
  Chaque parent re-dérive indépendamment depuis le fils partagé : la
  multi-parenté est un quasi non-problème.
- **Cycles bornés** : tout cycle du graphe n'est composé que de coups
  réversibles (un coup de pion / une capture est irréversible et ne peut être
  « dé-joué »). `make_child_path_history` ne réinitialise l'historique que sur
  un coup irréversible (`:88-94`) ; à travers un cycle tout-réversible
  l'historique est préservé, et la 2ᵉ occurrence déclenche
  `position_is_draw_by_repetition` (`:84`, `search_repetition_limit=2`) → le
  cycle est coupé en ≤ la limite de répétition.
- **Cycle de vie déjà DAG-correct** : `Node::_parent_count` (incrémenté
  `add_child:188`, décrémenté `reset:631`, recyclage seulement à `<=0` `:633`)
  est un refcount ; le recyclage free-list ne libère un nœud qu'au dernier
  parent détaché.

Le **vrai** point dur restant : la comptabilité `_nodes` (count de sous-arbre,
hypothèse d'arbre) et la **soundness de répétition** quand un sous-arbre
partagé est traversé via un chemin différent (cf. §3).

## Décisions arrêtées (utilisateur, 2026-05-17)

1. **Portée** : DAG général complet en une conception (pas de phasage).
2. **Soundness de répétition** : *partage maximal + re-vérification de la
   répétition à chaque traversée* (« Option 2 »). Choisi pour le **potentiel
   de gain de profondeur maximal** : la position de référence (pions bloqués,
   aucune capture) est **une seule longue époque réversible** — un partage
   borné aux frontières irréversibles n'y partagerait presque rien. Le partage
   large capte les transpositions denses intra-époque (ordre des coups de roi)
   tout en restant chess-correct via la re-vérification path-locale.
3. **Sécurité récursion** : *Approche 1* — borne par répétition-comme-nulle
   (prouvée) + garde de profondeur de récursion bon marché. Pas de visited-set
   explicite (surcoût hot-path sans gain de partage supplémentaire).
4. **#7 plié dans le périmètre** : remplacer la copie `robin_map` de
   `make_child_path_history` par un historique O(1) push/pop-undo —
   **indispensable** (sinon la re-vérification par arête est net-négative).
5. **`_nodes`** : conserver la mécanique par-arête `ChildLink._propagated_nodes`
   (O(1), non lue par la sélection) ; assouplir les asserts arbre ; métrique
   « travail » du bench → `node_map.size()` (nœuds uniques réels).
6. **Gating** : toggle runtime `g_tt_node_dag`, défaut OFF (même binaire A/B,
   kill-switch), OFF ⇒ comportement arbre actuel au byte près.
7. **Plan A** : conservé sur la branche, OFF (correct, revu, inoffensif ; sa
   correction d'eval-TT #3/#4/#14 reste précieuse). Plan B est additif et
   orthogonal ; les deux toggles ne doivent pas être ON ensemble.

## Conception détaillée

### 1. Carte d'identité de nœud (épine dorsale du DAG)

Global `robin_map<uint64_t, Node*> node_map` (clé Zobrist → `Node*` vivant),
**distinct** de `transposition_table` (TT d'évaluation). Consulté/peuplé
seulement si `g_tt_node_dag`. OFF ⇒ jamais touché ⇒ arbre byte-identique.

### 2. Link-on-create (`explore_new_move`)

Dans la branche non-nulle (aujourd'hui : alloue toujours un `Node` neuf), après
`make_move` + `get_zobrist_key()` et le contrôle d'arête **inchangé**
`position_is_draw_by_repetition` (le chemin nulle fabrique toujours une feuille
de nulle **distincte** — jamais partagée) :

- **Hit** `node_map[child_key]` (Node N vivant) : lier l'arête à N via
  `add_child(N, move)` (fait déjà `_parent_count++` + `ChildLink` par-arête) ;
  `child = N` ; **pas** de recréation board/sous-arbre. Le backup pull-based du
  parent consomme la `_deep_evaluation` existante de N.
- **Miss** : créer le `Node` comme aujourd'hui, puis `node_map[child_key] =
  child` (enregistrement).

Le surcoût par-arête quand ON : un `find` robin_map ; zéro allocation au hit.

### 3. Re-vérification de répétition à chaque traversée (cœur de soundness)

`explore_random_child` ne re-teste **pas** la répétition sur les fils existants
aujourd'hui (`position_is_draw_by_repetition` seulement à la création,
`:381`/`:1129`). Sous partage, un `Node` créé non-nulle via le chemin A puis
descendu via le chemin B ne refléterait pas les nulles-par-répétition de B plus
profond → non-sound.

Ajout : avant de descendre une arête vers un fils existant, exécuter
`position_is_draw_by_repetition` contre l'historique du chemin **courant**. Si
nulle sur ce chemin : utiliser la valeur de nulle pour le backup du parent et
**ne pas descendre** dans le sous-arbre partagé — **sans muter le `Node`
partagé**. Sinon : descente normale. La structure est partagée ; le statut de
nulle est re-dérivé par traversée → chess-correct.

### 4. Historique de chemin O(1) (#7, activateur indispensable)

Remplacer la copie intégrale `robin_map` de `make_child_path_history`
(`:88-94`) et les copies `PositionHistory iteration_path_history =
*base_path_history;` par **une** structure mutable passée par référence :
push à la descente / pop-undo au retour, avec un marqueur d'époque
(coup irréversible) sauvegardé/restauré au dépilage (un coup irréversible vide
logiquement l'historique ; l'état est restauré en remontant). Rend la
re-vérification §3 O(1) et globalement moins chère qu'aujourd'hui.

### 5. Cycle de vie / recyclage

`_parent_count` : déjà DAG-correct (recyclage seulement au dernier parent,
`:633`). Ajouts :
- `recycle_detached_node` doit **effacer l'entrée `node_map`** du nœud
  (sinon pointeur pendant / résurrection).
- Le chemin de reset racine/buffers (à côté de `transposition_table.clear()`)
  doit faire `node_map.clear()` (sinon pointeurs pendants inter-recherches).
- `reset(recursive=true)` ne recycle/récurse que lorsque `_parent_count`
  atteint 0 (déjà le cas `:633`) : un fils multi-parent n'est libéré qu'au
  dernier reset — correct, aucune modification nécessaire.

### 6. Comptabilité `_nodes`

`ChildLink._propagated_nodes` par-arête inchangé (O(1) ; alimente le gate
buffer-plein / l'affichage ; **non lu** par `pick_random_child` /
`get_node_score` / sélection). Assouplir (retirer) les `cout` de garde
`child_nodes >= nodes` (hypothèse d'arbre, fausse en DAG : un fils partagé
dépasse légitimement le cumul d'un parent). Métrique « travail » du bench →
`node_map.size()` (nœuds uniques réels) ; profondeur via `get_main_depth`.
Documenter : `_nodes` racine devient une borne supérieure path-déroulée, pas un
compte unique.

### 7. Sécurité cycle / récursion (Approche 1)

Prouvé : cycle ⇒ tout-réversible ⇒ historique préservé ⇒ 2ᵉ occurrence →
nulle via §3 → cycle coupé en ≤ limite de répétition. Plus un garde-fou de
profondeur de récursion bon marché dans `grogros_zero` (idiome du
`max_depth<=0` `:649`), constante haute (> toute profondeur réelle plausible)
ne se déclenchant que sur une récursion non-répétition pathologique.

### 8. Gating & câblage

`bool g_tt_node_dag = false;` — même schéma que `g_tt_main_search`
(extern `exploration.h`, def `exploration.cpp`, membre GUI, keybind, ligne HUD,
miroir avant l'appel `grogros_zero`). OFF ⇒ `node_map` jamais consulté,
link-on-create sauté, re-vérification §3 sautée ⇒ arbre byte-identique.
Plan A (`g_tt_main_search`) reste OFF ; documenter que les deux toggles ne sont
pas censés être ON ensemble.

### 9. Invariants & soundness

- OFF : comportement arbre actuel au byte près (tous les chemins ajoutés
  court-circuitent sur `g_tt_node_dag`).
- Sélection inchangée : `pick_random_child` reste eval-driven + terme
  d'exploration par-arête (`_chosen_iterations`/`child->_iterations`) ; le
  partage n'altère pas la sélection (les `ChildLink` restent par-arête).
- Répétition chess-correcte par traversée (§3) ; nulles toujours des feuilles
  distinctes par chemin (jamais partagées).
- Pas de fuite : `node_map` purgé au recyclage et au reset racine (§5).
- Perf : §3 + §4 sont sur le chemin le plus chaud ; §4 (O(1) push/pop) doit
  rendre l'ensemble net-positif vs aujourd'hui. À mesurer au gate.
- Commits en anglais ; symboles français Unicode dans les commentaires.

## Bench (critère de succès)

Position de référence : finale roi + pions bloqués (réf. Lague). Chemin
`Tests::problem_test` → `GUI::grogros_analysis(iterations)`. Budget en
**itérations fixes**, même FEN, même binaire, OFF puis ON (key dédiée, HUD
confirme). Mesurer : `node_map.size()` (nœuds uniques), `get_main_depth`,
meilleur coup, temps, stats. Attendu ON : beaucoup moins de nœuds uniques à
profondeur égale **et** profondeur nettement supérieure (direction 8→30),
**même meilleur coup**. Gate de régression : PERFT 1/2 + EVALUATION stables
(lancé par l'utilisateur).

## Hors périmètre

- Plan A : pas de revert (conservé OFF).
- Hygiène TT #5/#6, `operator<` #8 : indépendants, non bloquants.
- Multi-threading : hors sujet (moteur mono-thread).
