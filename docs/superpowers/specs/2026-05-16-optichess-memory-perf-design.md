# Spec — OptiChess : cleanup mémoire/perf (point 1)

> Date : 2026-05-16 · Statut : validé (design approuvé par l'utilisateur)
> Périmètre : bugs **#12**, **#13**, **#2** de `opti_chess/BUGFIXES.md`
> Contrainte projet n°1 : la performance runtime prime sur tout. Iso-comportement de recherche/éval.

## 1. Problème

Depuis les commits TT + refactor répétitions, deux régressions user-facing et un anti-pattern perf :

- **#12** — Chargement FEN lent, parfois infini.
- **#13** — Gel système global (toute l'UI Windows à ~5 s/clic, Grogros à 12 FPS, 30 min+ jusqu'au reboot).
- **#2** — `get_zobrist_key()` recopie tout le struct `Zobrist` (~6 Ko) à chaque appel.

### Causes racines (identifiées à l'audit)

1. **Allocation O(n)** : `BoardBuffer::get_first_free_index` (`buffer.cpp:44-54`) et `NodeBuffer::get_first_free_index` (`exploration.cpp:1545`) scannent linéairement tout le buffer à chaque allocation. Quand le buffer se remplit → O(n²) → CPU saturé, FEN lente/infinie (#12), boucle de rendu encore vive (= 12 FPS).
2. **Mémoire non bornée** : buffers préalloués énormes — `NodeBuffer` défaut 10 M (`exploration.h:225`), `BoardBuffer` défaut 5 M (`buffer.h:26`, appelé sans argument en `gui.cpp:110,142`), TT 5 M (`zobrist.h:61`). À mesure que l'arbre grossit (réflexion soutenue / arrière-plan), le RSS dépasse la RAM physique → swap → thrash système (#13).
3. **Buffer plein mal géré** : `return;` silencieux (`exploration.cpp:239-242`) / `return alpha` (`:924-928`) + `cout` à chaque appel ; aucune borne ni reprise propre.
4. **#2** : `board.cpp:7004` `Zobrist zobrist = transposition_table._zobrist;` — copie de struct (~6 Ko, dont `_board_keys[64][12]`) sur un chemin chaud, multipliée par le refactor répétitions.

## 2. Objectif

Usage mémoire **borné** et allocation **O(1)**, sans aucun changement de comportement de jeu :

- #12 : chargement FEN rapide et déterministe.
- #13 : impossible **par construction** sur n'importe quelle machine (RSS plafonné < RAM dispo).
- #2 : zéro copie de struct sur le chemin chaud.

## 3. Design

### 3.1 Free-list O(1) — NodeBuffer & BoardBuffer (cœur)

Remplacer le scan linéaire par une **pile d'indices libres** :

- À `init()` : empiler tous les indices `[0, length)`.
- `get_first_free_*` = `pop()` → O(1). Pile vide ⇒ `nullptr` (= « plein »).
- **Libération explicite** : un point de passage unique `buffer.free(index)` repousse l'index sur la pile, appelé là où un Node/Board redevient inactif (`Node::reset()` / passage `_is_active=false`). Chaque Node/Board stocke son index de buffer (1 champ entier).
- Le tableau contigu sous-jacent ne bouge pas → tous les `Node*`/`Board*` détenus par l'arbre restent **stables** (zéro risque).
- Le curseur rotatif `_iterator` est supprimé ; `reset()` (reset global du buffer) ré-empile tous les indices.

### 3.2 Budget mémoire adaptatif + plafond configurable

- Au démarrage, interroger la RAM physique (Windows : `GlobalMemoryStatusEx`).
- `budget = min(fraction × RAM_physique_dispo, plafond_dur)`.
- Constantes de config regroupées dans un bloc unique. **Valeurs par défaut** : `fraction = 0.5`, `plafond_dur = 4 Go`.
- Répartition `budget` → Board / Node / TT selon un **ratio configurable** ; `length_T = part_T / sizeof(T)`. **Défaut explicite** : TT = `min(taille_TT_demandée, part_TT_max)` (part dédiée plafonnée), puis le reste réparti pour obtenir **autant d'entrées Node que d'entrées Board** (un `Board` ≈ un `Node` expansé). Le plan d'implémentation fixera les `sizeof` réels.
- Supprime les défauts magiques (`buffer.h:26` 5M, `exploration.h:225` 10M, `buffer.cpp:6` « 4GB »).

### 3.3 Comportement « buffer plein » propre

- Plus de `cout` par appel : log **une seule fois** (flag/état) « buffer plein, arbre plafonné à N nœuds ».
- Quand plein : **arrêter l'expansion, continuer à raffiner l'arbre existant** (comportement MCTS acceptable), pas d'état cassé.
- **Pas** d'éviction LRU dans ce périmètre (reportée — voir §6).

### 3.4 Fix #2 (référence seule)

- `board.cpp:7004` : `Zobrist zobrist = …` → `const Zobrist& zobrist = transposition_table._zobrist;`.
- Génération des clés déjà garantie au démarrage (`main_gui.h:87` via `transposition_table.init`) → retirer la mutation `if (!_keys_generated) generate_zobrist_keys();` du chemin chaud (remplacer par un `assert` de garde).
- Zéro changement de comportement (clé identique).

### 3.5 Hygiène in-scope (fichiers déjà touchés)

- `buffer.cpp:8-9` : supprimer le code mort `_length = calc; _length = 0;`.
- Remplacer `4000000000` en `unsigned long` (fragile MSVC 32-bit) par `size_t`/type explicite.
- Uniformiser le contrat : ctor par défaut **n'alloue pas**, `init()` obligatoire, identique pour `BoardBuffer` et `NodeBuffer`.

## 4. Validation (obligatoire — perf = priorité projet)

- **Correctness** : `validate_nodes_count_at_depth` (perft) **inchangé** avant/après → movegen + Zobrist intacts. Vérifier que la clé Zobrist est identique avant/après §3.4 sur un échantillon de positions.
- **Perf** :
  - Temps de chargement FEN avant/après (symptôme #12) — doit être petit et constant.
  - Microbench allocation : remplir le buffer, mesurer le temps par allocation — doit passer de O(n²) à O(1).
  - RSS du process sur une réflexion longue **et** en arrière-plan — doit rester **≤ plafond configuré** (Gestionnaire des tâches / `GetProcessMemoryInfo`).
- Mise à jour de `BUGFIXES.md` : #2/#12/#13 → corrigés + preuve runtime.

## 5. Risques

- **Double libération / oubli de libération** d'un index (free-list corrompue) : mitigé par un point de passage unique `buffer.free()` + assert (index non déjà libre).
- **Probe RAM** spécifique Windows : isoler derrière une petite fonction (portage CMake futur trivial).
- **§3.4** : si une position atteint `get_zobrist_key` avant l'init des clés → géré par l'assert + garantie d'init au démarrage.

## 6. Hors périmètre (reporté, tracé dans BUGFIXES.md)

- Buffers *growable* par blocs ; éviction LRU de sous-arbres.
- Clé Zobrist **incrémentale** dans `make_move` (#2 profond).
- Découpage de `board.cpp` (point 2 du cleanup global).
- Correctness TT : **#4** (stand-pat en `TT_EXACT`) → **#3** (mate scores ply-relatifs) → **#14** (généraliser la cohérence des champs dérivés `_wdl/_avg_score/_uncertainty/_winnable_*` au cas non-mat — incohérence éval/score signalée par l'utilisateur, qui se corrige à la ré-exploration) → **#11** (gain de profondeur). Passe correctness séparée, après ce cleanup.
