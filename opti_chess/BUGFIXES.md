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

### #2 — `get_zobrist_key()` copiait toute la table Zobrist par appel
- **Statut** : ✅ corrigé — validé runtime (commit `2026018`).
- **Fix** : `const Zobrist& zobrist = transposition_table._zobrist;` (référence au lieu d'une copie ~6 Ko/appel) ; clés générées une fois au démarrage, garde-fou idempotent sans copie. Gain perf ; **n'était pas** la cause de #12 (hypothèse réfutée par test runtime).

### #12 — Régression : chargement FEN lent / ne terminait parfois jamais
- **Statut** : ✅ corrigé — validé runtime (2026-05-16, commit `d7a3554`).
- **Cause racine** : `GUI::reset_buffers()` rebalayait 5 M `Board` + 10 M `Node` (chaque `reset` faisant un `robin_map::clear()` depuis `a258fb5`) à **chaque** `load_FEN` ⇒ O(capacité).
- **Fix** : `reset_buffers()` ne fait plus que `transposition_table.clear()` ; la réclamation O(utilisé) reste assurée par le `_root_exploration_node->reset()` récursif déjà présent. Chargement FEN redevenu instantané, y compris après recherche. PERFT 1/2 inchangé.

### #13 — Gel système global quand GrogrosChess tournait
- **Statut** : ✅ corrigé — validé runtime utilisateur (2026-05-16, branche `fix/buffer-memory-perf`, PERFT 1/2 inchangé, pas de gel).
- **Cause racine** : buffers préalloués gigantesques (`new Node[10M]` + `new Board[5M]` + TT, plusieurs Go) + allocation **O(n)** (scan linéaire à chaque `get_first_free_*`) ⇒ thrash disque + O(n²) CPU.
- **Fix** (plan `docs/superpowers/plans/2026-05-16-optichess-memory-perf.md` ; commits `82c3b9f` `bf048ec` `3d2db54` `2081e10`) :
  1. **free-list O(1)** (pile d'indices libres) remplaçant le scan O(n) ; chaque `Board`/`Node` porte son `_buffer_index` ; recyclage « Approche B » (seuls les enfants détachés pendant la récursion sont repoussés — jamais le nœud reseté en place).
  2. **dimensionnement mémoire adaptatif** depuis la RAM physique dispo (`compute_pool_sizing`, budget = min(0.5×RAM, 4 Go) / facteur d'overhead RSS) — remplace les constantes magiques 1E7/5E6 ; RSS borné par construction.
  3. **buffer plein → raffinage propre** de l'arbre existant (gate `can_expand` dans `grogros_zero`) au lieu d'un busy-spin ; retrait du `cout` par-itération.
- **⚠️ Résiduel connu (non bloquant)** : un spam console subsiste en condition buffer-plein, d'une source distincte des `cout` retirés en T5 ; le gate de régression (PERFT 1/2 + EVALUATION stables) reste validé. À tracer séparément.

---

## ⬜ Ouverts — par sévérité

### 🔴 #3 — Scores de mat non normalisés au ply dans la TT
- **Fichiers** : `board.cpp:1569` (encodage mate via `_moves_count`), `board.cpp:7014-7042` (`_moves_count` absent de la clé Zobrist)
- **Sévérité** : HAUTE (correctness — cause probable des "fantômes" résiduels)
- **Détail** : mate score = `(-mate_value + _moves_count * mate_ply) * color`. `_moves_count` (n° de coup absolu) n'est pas hashé. Même position physique atteinte via chemins de longueurs différentes ⇒ même clé Zobrist, mate scores différents stockés/relus.
- **⚠️ Régression de visibilité depuis #1 v2** : l'éval de mat est désormais *cohérente* (`uncertainty=0`, `winnable` net). Conséquence : une distance de mat **fausse** issue d'une transposition s'affiche maintenant *avec confiance* (score 1.000, conf 100 %) au lieu d'être visiblement cassée. Borné (mauvaise *distance*, jamais mauvaise *classification* — seuil `10*abs>mate_value` robuste face au terme `_moves_count`), mais le « tell » visuel a disparu → **#3 = prochain item correctness explicite**.
- **Fix proposé** : au `store`, si valeur = mate, normaliser relatif au ply courant (`eval ± _moves_count * mate_ply`) ; au `probe`, dé-normaliser. Cohérent avec l'encodage existant.

### 🔧 #4 — Stand-pat caché en `TT_EXACT`
- **Statut** : 🔧 correctif appliqué, **compile OK** (Release x64, `Grogros_Chess.exe` généré) — gate runtime PERFT 1/2 + EVALUATION en attente (validation utilisateur, comme #1/#2/#12/#13).
- **Fichiers (lignes corrigées — l'ancien doc était décalé par les commits depuis)** : stores `stand_pat` en `TT_EXACT` désormais `exploration.cpp:811` (emergency cutoff) & `:819` (depth≤0) ; au post-loop `:1033-1034`, `stand_pat` monte `alpha` en `:856-858` puis flag `TT_EXACT` si aucun fils ne dépasse le plancher. Probe/consommation des flags centralisée `:764-804`.
- **Sévérité** : MOYENNE (soundness ; bloquant pour #11 plan A).
- **Détail** : `stand_pat` est une **borne inférieure** sur la valeur vraie (le camp au trait peut toujours refuser les captures ; la boucle de captures sélective+élaguée ne la prouve jamais exhaustive). Stocké `TT_EXACT`, le probe coupe **inconditionnellement** (fenêtre-indépendant, `:768`/`:801`) en renvoyant une éval **statique** comme si recherchée → faux cutoffs masquant la tactique, et propagation d'une valeur statique dans #11 plan A.
- **Fix appliqué** : flag dédié `TT_STANDPAT` (`zobrist.h:28`), consommé **comme borne inférieure** (= `TT_BETA` : cutoff seulement si `tt_eval >= beta`, renvoi `beta`). Stores `:811`/`:819` → `TT_STANDPAT` ; au `:1033`, si `alpha > original_alpha` **et** `alpha == stand_pat` → `TT_STANDPAT` (sinon `TT_ALPHA`/`TT_EXACT` inchangés). Égalité exacte fils==stand_pat rare → dégradée `TT_EXACT`→`TT_STANDPAT` : conservatif, toujours sain.
- **Alternative rejetée** : « marquer `TT_ALPHA` » (proposé initialement) est **incorrect** — `TT_ALPHA` = borne *haute* (`zobrist.h:26`), or `stand_pat` est une borne *basse* ; cela aurait introduit des cutoffs fail-low faux (masquage tactique inverse). `TT_STANDPAT`/`TT_BETA` est la sémantique correcte.

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
> #2, #12, #13 corrigés & validés runtime (cf. section ✅ Corrigés). Suite :
1. ~~**#4** (stand-pat ≠ EXACT)~~ → correctif appliqué (🔧 gate runtime en attente) → **#3** (mate scores ply-relatifs) → **#14** (généraliser la cohérence des champs dérivés #1 au cas non-mat) → **#11** — débloquent #11 et corrigent l'incohérence éval/score visible.
2. **#11 plan A** (TT scalaire dans la recherche principale) — **l'objectif** : gain de profondeur. Mesurer après.
3. **#6** puis **#5** (hygiène TT).
4. **#7** (perf path-local + fuite map — re-tester #13) puis **#8** (operator<, rapide, latent).
5. **#11 plan B** (DAG) — seulement si A insuffisant pour le gain visé.
6. **#9** — décision de design à confirmer avec l'utilisateur.
