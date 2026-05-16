# OptiChess — Suivi des bugs algorithmiques

> Fichier de travail interne (recherche / TT / répétitions / éval).
> Mis à jour au fil des sessions. Voir aussi `ALGORITHMS.md` (doc technique).
>
> Légende statut : ✅ corrigé · 🔧 en cours · ⬜ ouvert · 🔍 à vérifier

---

## ✅ Corrigés

### #1 — TT cutoff laissait `_deep_evaluation` incohérente
- **Statut** : ✅ corrigé — **validé runtime** (utilisateur, sur `r1b3k1/p4ppp/5n2/1Rp4q/4B3/2P5/P1PQ2PP/4R1K1 b - - 3 21`, Fa6).
- **Fichier** : `exploration.cpp` — bloc `if (tt_cutoff)`.
- **Symptôme** : (a) bug "fantôme" — un coup -M1 (pire) classé comme principal ; (b) éval = mat mais `score`/`confidence` pas à 0/1 (ex : M4 affiché, score 0.934, conf 87 %).
- **Cause** : au cutoff TT, seul `_deep_evaluation._value` était écrasé ; `_wdl`, `_avg_score`, `_uncertainty`, `_winnable_*` restaient ceux de l'éval **statique**. `get_node_score` combine ces champs → mauvais classement. `get_WDL` remixe `_value` avec `_uncertainty`/`_winnable_*` (`board.cpp:10061-10070`) → `avg_score = win·(1−U)+… ≈ 0.913+0.022 = 0.935`, `conf = 100−100·U = 87 %` (reproduit exactement les chiffres observés).
- **Fix v1 (incomplet)** : recalcul de `_wdl` puis `_avg_score` après cutoff. Corrigeait (a) mais **pas** (b) : `get_WDL` continuait de remixer avec `_uncertainty`/`_winnable_*` statiques.
- **Fix v2 (complet)** : avant `get_WDL()`, si `_value` franchit le seuil de mat (`10*abs > mate_value`, idiome de `is_eval_mate`), forcer `_uncertainty = 0` et `_winnable_white/black` selon le signe — comme le chemin terminal `board.cpp:1575-1577`. Couvre les 3 branches EXACT/BETA/ALPHA. Hors hot path d'éval → perf neutre.
- **Confirmation utilisateur** : « Cela semble avoir corrigé le bug ».

---

## ⬜ Ouverts — par sévérité

### 🔴 #2 — `get_zobrist_key()` copie toute la table Zobrist par appel
- **Fichier** : `board.cpp:6996-7043` (ligne 7004)
- **Sévérité** : HAUTE (perf — axe prioritaire #1 du projet ; + correctness latente)
- **Détail** :
  - `Zobrist zobrist = transposition_table._zobrist;` copie `_board_keys[64][12]` (~6 Ko) **à chaque appel**. Hot path : `ensure/record/position_history_count` l'appellent chacun + appels explicites `_board->get_zobrist_key()`. Le refactor répétitions a multiplié ces appels.
  - `if (!zobrist._keys_generated) zobrist.generate_zobrist_keys();` opère sur la **copie** → si table globale non init, clés aléatoires différentes à chaque appel (incohérence). Neutralisé aujourd'hui par `init()` au démarrage (`main_gui.h:87`), mais mine latente.
  - Pas de cache (recalcul O(64) intégral même si clé à jour ; `if (_zobrist_key != 0) return;` est commenté).
- **Fix proposé** :
  1. Immédiat / zéro risque : `const Zobrist& zobrist = transposition_table._zobrist;` (référence).
  2. Mieux : mise à jour **incrémentale** de `_zobrist_key` dans `make_move` (XOR pièce/trait/roque/ep), supprimer le recalcul intégral.
  3. Sortir `generate_zobrist_keys()` du chemin chaud (garantir l'init au démarrage uniquement).

### 🔴 #3 — Scores de mat non normalisés au ply dans la TT
- **Fichiers** : `board.cpp:1569` (encodage mate via `_moves_count`), `board.cpp:7014-7042` (`_moves_count` absent de la clé Zobrist)
- **Sévérité** : HAUTE (correctness — cause probable des "fantômes" résiduels)
- **Détail** : mate score = `(-mate_value + _moves_count * mate_ply) * color`. `_moves_count` (n° de coup absolu) n'est pas hashé. Même position physique atteinte via chemins de longueurs différentes ⇒ même clé Zobrist, mate scores différents stockés/relus.
- **⚠️ Régression de visibilité depuis #1 v2** : l'éval de mat est désormais *cohérente* (`uncertainty=0`, `winnable` net). Conséquence : une distance de mat **fausse** issue d'une transposition s'affiche maintenant *avec confiance* (score 1.000, conf 100 %) au lieu d'être visiblement cassée. Borné (mauvaise *distance*, jamais mauvaise *classification* — seuil `10*abs>mate_value` robuste face au terme `_moves_count`), mais le « tell » visuel a disparu → **#3 = prochain item correctness explicite**.
- **Fix proposé** : au `store`, si valeur = mate, normaliser relatif au ply courant (`eval ± _moves_count * mate_ply`) ; au `probe`, dé-normaliser. Cohérent avec l'encodage existant.

### 🟠 #4 — Stand-pat caché en `TT_EXACT`
- **Fichiers** : `exploration.cpp:770`, `:778` (stockent `stand_pat` en `TT_EXACT`) ; `:815-817` + `:995` (stand_pat monte `alpha` sans fils gagnant → flag `TT_EXACT`)
- **Sévérité** : MOYENNE (soundness)
- **Détail** : une éval **statique** (stand pat) est cachée comme valeur exacte → cutoffs faux sur probes ultérieurs à profondeur ≤ stockée.
- **Fix proposé** : flag dédié `TT_STANDPAT`, ou marquer ces cas `TT_ALPHA` (borne haute) au lieu de `TT_EXACT`.

### 🟠 #5 — `TT_BETA` stocke `beta` au lieu de `score`
- **Fichiers** : `exploration.cpp:801`, `:982`
- **Sévérité** : MOYENNE (perte de précision, perte d'info de mat)
- **Détail** : sur beta cutoff, on stocke `beta` ; un `score` de mat (≫ beta) est écrasé → borne inférieure trop lâche, futurs probes ratent le mat.
- **Fix proposé** : stocker `score` (la vraie valeur ≥ beta) plutôt que `beta`.

### 🟠 #6 — TT non clearée entre recherches racines distinctes
- **Fichiers** : clears épars (`gui.cpp:962,1910`, `game_tree.cpp:95`, `main_gui.h:414`, `board.cpp:10184`), pas systématiques
- **Sévérité** : MOYENNE (contribue aux bugs "fantômes")
- **Fix proposé** : clear (ou aging/génération) au début de chaque recherche depuis une nouvelle racine, sauf si incrémentalisation voulue.

### 🟠 #7 — `make_child_path_history` copie toute la map par enfant
- **Fichier** : `exploration.cpp:33-38`
- **Sévérité** : MOYENNE (perf hot path)
- **Détail** : `robin_map` répétitions copiée à chaque nœud réversible.
- **Fix proposé** : pile push/pop avec undo (entrée/sortie de récursion) au lieu de copies ; ou structure plus légère (vecteur de clés + dernier irréversible).

### 🟡 #8 — `Evaluation::operator<` branches inversées
- **Fichier** : `board.h:512-520`
- **Sévérité** : BASSE (latent — usage `pick_random_child:1158` le neutralise)
- **Détail** : les branches `!_evaluated` sont identiques à `operator>` au lieu d'être inversées.
- **Fix proposé** :
  ```cpp
  bool operator<(Evaluation& other) {
      if (!other._evaluated) return false;
      if (!_evaluated) return true;
      return _value < other._value;
  }
  ```

### 🔴 #11 — Limitation architecturale : la TT ne donne aucun gain de profondeur
- **Fichiers** : `exploration.cpp:739` (seul probe, dans `quiescence`), `:770,778,801,982,996` (seuls stores) ; `:272-273` (partage de nœuds explicitement retiré)
- **Sévérité** : HAUTE (c'est l'objectif initial des transpositions — actuellement non atteint)
- **Symptôme** : sur finale roi+pions bloqués, attendu ~8→30+ de profondeur à temps égal (réf. vidéo Sebastian Lague) ; observé : ~aucun gain.
- **Preuve (stats TT, position roi+pions bloqués)** :
  ```
  1.3k entrées | probes 812k | hits 810k (99%) | cutoffs 810k (99%) | stores 1.4k | overwrites 43
  ```
  ⇒ ~1300 nœuds uniques revisités ~625× chacun. La redondance combinatoire des finales de pions est intacte.
- **Cause** :
  1. La TT n'est utilisée que dans `quiescence()`, jamais dans la recherche principale (`grogros_zero`/`explore_new_move`/`explore_random_child` ne font aucun probe/store). Elle cache l'éval de **feuille**, pas le sous-arbre.
  2. L'arbre principal recrée `Node`+`Board` et ré-explore tout le sous-arbre pour chaque occurrence transposée → explosion combinatoire non touchée.
  3. Les 99% de cutoffs renvoient majoritairement un `stand_pat` statique stocké en `TT_EXACT` (cf. #4) → quiescence de fait neutralisée, pas de signal tactique pour creuser.
  4. Le gain "type Lague" vient de cutoffs TT dans une recherche alpha-bêta principale + iterative deepening — mécanisme absent ici (MCTS-like).
- **Plan** :
  - **A. TT scalaire dans la recherche principale** : probe dans `explore_new_move` *avant* expansion ; si entrée fiable de profondeur suffisante → réutiliser la valeur sans recréer le sous-arbre. Réutilise la valeur, pas l'objet `Node` (évite les soucis `_nodes`/backprop/cycles). **Bloqué par #4 puis #3** (sinon propagation de valeurs fausses). Gain "valeur seule" = on évite la quiescence mais pas forcément la ré-expansion → gain partiel.
  - **B. Partage de nœuds / DAG** : gain maximal (8→30), mais grosse refonte que le code a fuie. Infra à moitié posée (`ChildLink._propagated_nodes`, `_parent_count`). Risqué.
- **Ordre** : #4 → #3 → A (mesurer), puis envisager B si A insuffisant.

### 🔴 #12 — Régression : chargement FEN lent / ne termine parfois jamais
- **Statut** : ⬜ ouvert — signalé utilisateur (régression) · 🔍 cause à confirmer
- **Fichiers** : probable `board.cpp:6996-7043` (`get_zobrist_key`) + chemin de chargement FEN / init de l'historique de répétitions (refactor `a258fb5`).
- **Sévérité** : HAUTE (régression user-facing depuis `5e155ca`/`a258fb5` — TT + refactor répétitions).
- **Symptôme** : depuis les commits TT + refactor répétitions, charger une FEN est souvent lent ; parfois la position « ne se charge jamais ».
- **Cause probable (à confirmer)** : **lien direct avec #2**. `get_zobrist_key()` copie la table Zobrist (~6 Ko) **à chaque appel** ; le refactor répétitions (`a258fb5`) a multiplié ces appels. Le chargement FEN / la (re)construction de l'historique de répétitions appelle massivement `get_zobrist_key` → coût quadratique/explosif. Le « ne termine jamais » ⇒ chemin O(n·6 Ko) ou pire sur historique long.
- **Action** : appliquer d'abord le fix #2 (référence + idéalement clé incrémentale), **re-mesurer le temps de chargement FEN**. Si non résolu : profiler `load_FEN` (init historique répétitions, allocations `robin_map`, boucle d'historique).

### 🔴 #13 — Gel système global quand GrogrosChess tourne (arrière-plan / réflexion)
- **Statut** : ⬜ ouvert — signalé utilisateur · 🔍 à investiguer (rare, repro difficile, non corrélé à une action précise)
- **Sévérité** : HAUTE (rend **toute la machine** inutilisable 30 min+ jusqu'au redémarrage).
- **Symptôme** : rare ; appli ouverte (parfois en arrière-plan) ou réflexion lancée → toute l'UI Windows lague (~5 s de délai par clic/input), Grogros affiché à **12 FPS**, persiste 30 min+ et ne se rétablit qu'au reboot.
- **Cause racine probable (identifiée à l'audit)** : **buffers préalloués gigantesques + allocation O(n)**.
  - Tailles par défaut : `NodeBuffer::init(length = 10_000_000)` (`exploration.h:225`), `BoardBuffer::init(length = 5_000_000)` (`buffer.h:26`), `TranspositionTable::init(5_000_000)` (`zobrist.h:61`). `new Node[10M]` + `new Board[5M]` + TT ⇒ **plusieurs Go committés**. À mesure que la réflexion (même en arrière-plan) touche le buffer, le RSS grimpe → quand il dépasse la RAM physique → swap disque → **thrash système global** (5 s/clic, 12 FPS) jusqu'au reboot. Le ctor par défaut `BoardBuffer` vise même un buffer « 4GB » (`buffer.cpp:6-7`).
  - `BoardBuffer::get_first_free_index` (`buffer.cpp:44-54`) et `NodeBuffer::get_first_free_index` (`exploration.cpp:1545`) = **scan linéaire O(n) sur tout le buffer à chaque allocation**. Quand le buffer se remplit, chaque expansion scanne jusqu'à 10 M entrées ⇒ O(n²), CPU à fond (boucle de rendu encore vive = 12 FPS) — explique aussi #12.
  - Buffer plein : `explore_new_move` fait `return;` silencieux (`exploration.cpp:239-242`), quiescence `return alpha` (`:924-928`). Aucune éviction / pruning de l'arbre ⇒ une fois plein, état dégradé permanent.
- **Note correctness** : `Node::reset()` nettoie bien `_children.clear()` + `_is_active=false` (`exploration.cpp:421-457`) ⇒ le destructeur commenté (`:610-615`) n'est **pas** une fuite (pool). La fuite map n'est pas le coupable principal ; c'est la **taille des buffers**.
- **Fix proposé** : (1) réduire/rendre configurables les tailles par défaut (≪ RAM physique) ; (2) remplacer le scan O(n) par une **free-list** (pile d'indices libres) → alloc/libération O(1) ; (3) borne + éviction quand le buffer sature au lieu d'un `return` silencieux. + #2.
- **À confirmer** : surveiller le RSS du process pendant une réflexion longue (Gestionnaire des tâches / perfmon) → corréler pic RSS ↔ gel.

### 🔴 #14 — Incohérence éval/score persistante sur valeur issue de la TT (généralisation de #1, cas non-mat)
- **Statut** : ⬜ ouvert — signalé utilisateur · même famille que **#1** (qui n'a corrigé que le cas *mat*).
- **Fichiers** : bloc `if (tt_cutoff)` de `exploration.cpp` (cf. #1) ; champs dérivés de `Evaluation` recombinés par `get_node_score` et `Evaluation::get_WDL` (`board.cpp:10004`) / `get_average_score` (`board.cpp:10093`).
- **Sévérité** : HAUTE (correctness, visible utilisateur).
- **Symptôme** : parfois `_value` et le **score affiché** du nœud ne sont pas cohérents ; **dès qu'on explore la variante** (ré-exploration réelle du nœud) il **redevient correct**. Même nature que les « -M4 fantômes » de #1, mais sur des évals **non-mat** quelconques, et plus général.
- **Cause probable** : le fix #1 v2 ne force la cohérence (`_uncertainty=0`, `_winnable_*`) **que** au franchissement du seuil de mat. Pour une valeur **non-mat** issue d'un cutoff / d'une relecture TT, `_value` est mis à jour mais les champs dérivés (`_wdl`, `_avg_score`, `_uncertainty`, `_winnable_*`) restent ceux de l'éval statique/précédente → `get_node_score`/affichage incohérents **jusqu'à** ré-exploration (qui recalcule tout proprement). Couplé à #4 (stand-pat en `TT_EXACT`) et #3.
- **Fix proposé** : au cutoff / à la relecture TT, **recalculer systématiquement** (ou invalider→recalculer) tous les champs dérivés à partir de `_value`, pas seulement sur le seuil de mat — généralisation du fix #1 v2 au cas non-mat. À cadrer avec #4 puis #3.
- **Périmètre** : **hors** du cleanup mémoire/perf en cours (point 1) ; à traiter dans la passe correctness TT (avec #4/#3).

---

## 🔍 À vérifier / clarifier

### #9 — Répétition 2-fold vs 3-fold (FIDE)
- **Fichier** : `exploration.cpp:7,29-31` (`search_repetition_limit = 2`)
- La 1ère répétition dans l'arbre est traitée comme nulle. Optimisation de recherche classique et saine, mais ≠ règle FIDE 3-fold. **Confirmer que c'est voulu** (risque : surévaluer des nulles forcées illusoires).

### #10 — `evaluate_quiescence_threat` / `minimal_quiescence`
- **Statut** : 🔍 vérifié OK — n'utilise PAS la TT, travaille sur `Board` copie à trait inversé. Pas de pollution. Noté pour mémoire.

---

## Ordre de traitement recommandé
1. **#2 + #12** — `get_zobrist_key` par référence : 1 ligne, gros gain perf, zéro risque, **et probable correctif de la régression user-facing #12** (chargement FEN). Re-mesurer le temps de chargement FEN juste après.
2. **#13** (gel système) — impact utilisateur maximal (machine inutilisable). Investigation mémoire ; commencer **après #2** (qui réduit le churn et peut atténuer) avec instrumentation RAM + repro.
3. **#4** (stand-pat ≠ EXACT) → **#3** (mate scores ply-relatifs) → **#14** (généraliser la cohérence des champs dérivés #1 au cas non-mat) — débloquent #11 et corrigent l'incohérence éval/score visible.
4. **#11 plan A** (TT scalaire dans la recherche principale) — **l'objectif** : gain de profondeur. Mesurer après.
5. **#6** puis **#5** (hygiène TT).
6. **#7** (perf path-local + fuite map — re-tester #13) puis **#8** (operator<, rapide, latent).
7. **#11 plan B** (DAG) — seulement si A insuffisant pour le gain visé.
8. **#9** — décision de design à confirmer avec l'utilisateur.
