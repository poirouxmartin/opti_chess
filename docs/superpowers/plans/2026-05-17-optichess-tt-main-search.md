# TT scalaire dans la recherche principale (#11 Plan A) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Réutiliser une valeur de TT fiable comme feuille gelée dans la recherche principale (`explore_new_move`) et écrire en retour la `_deep_evaluation` raffinée des nœuds principaux, pour casser la redondance combinatoire des transpositions — derrière un toggle runtime OFF par défaut.

**Architecture:** Probe TT dans `explore_new_move` avant la quiescence du nouveau fils ; sur hit fiable (`TT_EXACT` + profondeur dans la bande write-back) on construit une feuille non expansible (`_nodes=1`, `_iterations=1`, `_can_explore=false`) portant la valeur TT synthétisée (mêmes conventions #1/#14 que le cutoff quiescence, factorisées en helper partagé). Write-back `TT_EXACT` aux deux points où `_deep_evaluation` est copiée du meilleur fils, avec une profondeur-proxy `QDEPTH_BAND + floor(log2(_nodes+1))` au-dessus de la bande quiescence.

**Tech Stack:** C++17, MSVC (Visual Studio, `opti_chess.sln`), raylib (GUI), tsl::robin_map (TT). Pas de framework de test CLI : la **vérification = (a) compilation MSBuild propre, (b) gate runtime lancé par l'utilisateur** (PERFT 1/2 + EVALUATION stables, puis bench OFF vs ON nœuds/profondeur sur la FEN de référence). Spec : `docs/superpowers/specs/2026-05-17-optichess-tt-main-search-design.md`.

---

## Testing model (lire avant de commencer)

Ce moteur n'a pas de runner de tests unitaires câblé en CLI pour la recherche ; sa correction se vérifie au runtime par l'utilisateur (PERFT 1/2 + EVALUATION) et sa performance est l'objectif même. La granularité TDD « test rouge d'abord » est donc adaptée ainsi :

- **Step build** : compiler la solution et vérifier 0 erreur. Commande :
  `MSBuild opti_chess.sln /p:Configuration=Release /p:Platform=x64 /m /nologo /verbosity:minimal`
  (l'exécutant lance cette commande ; si MSBuild n'est pas au PATH, utiliser le « Developer PowerShell for VS » ; ne jamais prétendre que ça compile sans avoir vu le code de sortie de retour 0).
- **Step gate runtime** : marqué **[USER]** — c'est l'utilisateur qui lance le binaire et confirme PERFT 1/2 + EVALUATION inchangés (toggle OFF) puis le bench OFF/ON. Ne pas cocher tant que l'utilisateur n'a pas confirmé.
- Commits fréquents, atomiques, style utilisateur : conventional commits, sujet ASCII (pas d'accents), **aucune attribution IA**.

---

## File Structure

- **`opti_chess/exploration.cpp`** — cœur : constantes/helpers TT-main (près des `tt_normalize_mate` existants l.17-26), refacto du bloc de synthèse cutoff (l.820-829), `init_tt_leaf_child`, probe dans `explore_new_move`, write-back dans `explore_new_move`/`explore_random_child`, définition du global toggle.
- **`opti_chess/exploration.h`** — déclaration `extern bool g_tt_main_search;` (près de `g_buffers_full_logged` l.262).
- **`opti_chess/gui.h`** — membre GUI `bool _tt_main_search = false;` (près de `_explore_checks` l.108).
- **`opti_chess/gui.cpp`** — miroir membre→global avant l'appel `grogros_zero` (l.1055) ; ligne HUD (près de `_explore_checks` l.1417).
- **`opti_chess/main_gui.h`** — keybind runtime de bascule (zone inputs l.115-374).

Pas de nouveau fichier : le changement est localisé et suit les patterns existants (helpers file-local `inline`, global façon `g_buffers_full_logged`, leaf façon `init_terminal_draw_child`).

---

## Task 1: Constantes, helpers TT-main et global toggle

**Files:**
- Modify: `opti_chess/exploration.cpp:17-26` (après les helpers `tt_*_mate`)
- Modify: `opti_chess/exploration.cpp:1673` (près de `bool g_buffers_full_logged = false;`)
- Modify: `opti_chess/exploration.h:262` (près de `extern bool g_buffers_full_logged;`)

- [ ] **Step 1: Ajouter constantes + helpers après `tt_denormalize_mate` (exploration.cpp, juste après la ligne 26)**

Contexte existant (l.22-26) :
```cpp
inline int tt_denormalize_mate(int eval, int moves_count) {
	if (10 * abs(eval) > mate_value)
		return eval - (eval > 0 ? 1 : -1) * moves_count * mate_ply;
	return eval;
}
```

Insérer juste après la `}` de `tt_denormalize_mate` :
```cpp
// #11 Plan A — TT scalaire dans la recherche principale.
// QDEPTH_BAND : décale toute profondeur de write-back MCTS au-dessus de la
// bande de quiescence (depth quiescence <= _quiescence_depth, ~10). Le
// remplacement depth-preferred (zobrist.cpp:113) garde ainsi toujours une
// entrée raffinée plutôt qu'une feuille de quiescence, et la porte de
// réutilisation peut exiger un vrai sous-arbre raffiné.
constexpr int QDEPTH_BAND = 256;
// MIN_REUSE_LOG2 : on ne réutilise que les entrées représentant >= 2^N noeuds
// raffinés (jamais une simple feuille de quiescence). Ajustable au gate.
constexpr int MIN_REUSE_LOG2 = 4;

// log2 entier de (nodes+1), borné. Pas de float (hot-path-friendly, MSVC).
inline int tt_writeback_depth(int nodes) {
	int v = nodes + 1;
	int log2v = 0;
	while (v > 1) { v >>= 1; ++log2v; }
	return QDEPTH_BAND + log2v;
}

// Cohérence des champs dérivés d'une Evaluation dont _value vient de la TT.
// Factorisation EXACTE du bloc de synthèse du cutoff quiescence (#1 v2 / #14) :
// _value est supposé déjà posé (white-relative). On force _uncertainty=0 (une
// valeur TT fiable ne doit pas être filtrée par l'incertitude statique), on
// remet _winnable_* par signe si mat (évite le scaling sur un score de mat
// géant), puis on redérive _wdl / _avg_score depuis _value.
inline void tt_fixup_derived(Evaluation& e) {
	e._uncertainty = 0.0f;
	if (10 * abs(e._value) > mate_value) {
		e._winnable_white = e._value > 0 ? 1.0f : 0.0f;
		e._winnable_black = e._value < 0 ? 1.0f : 0.0f;
	}
	e.get_WDL();
	e.get_average_score();
}
```

- [ ] **Step 2: Déclarer le global toggle dans exploration.h (près de la ligne 262)**

Contexte existant (l.262) : `extern bool g_buffers_full_logged;`

Ajouter juste en dessous :
```cpp
// #11 Plan A — active probe + write-back TT dans la recherche principale.
// Défaut OFF : comportement actuel au byte près, A/B même binaire.
extern bool g_tt_main_search;
```

- [ ] **Step 3: Définir le global dans exploration.cpp (près de la ligne 1673)**

Contexte existant (l.1673) : `bool g_buffers_full_logged = false;`

Ajouter juste en dessous :
```cpp
bool g_tt_main_search = false;
```

- [ ] **Step 4: Build**

Run: `MSBuild opti_chess.sln /p:Configuration=Release /p:Platform=x64 /m /nologo /verbosity:minimal`
Expected: build réussi, 0 erreur (helpers non encore appelés → warnings « unused » possibles et acceptables).

- [ ] **Step 5: Commit**

```bash
git add opti_chess/exploration.cpp opti_chess/exploration.h
git commit -m "feat(search): constantes, helpers et toggle pour TT en recherche principale (#11)"
```

---

## Task 2: Refacto behavior-preserving du bloc de synthèse du cutoff quiescence

But : remplacer les lignes 820-829 de `quiescence()` par un appel à `tt_fixup_derived` (DRY, source unique de la convention #1/#14). Aucun changement de comportement attendu.

**Files:**
- Modify: `opti_chess/exploration.cpp:820-829`

- [ ] **Step 1: Remplacer le bloc inline par l'appel au helper**

Bloc existant à remplacer (exploration.cpp, l.820-829) :
```cpp
			_deep_evaluation._uncertainty = 0.0f;
			// Mat : on garde l'idiome terminal (0/1 par signe) qui évite le
			// scaling winning_eval/_winnable sur un score de mat géant. Hors mat
			// on conserve les _winnable_* statiques (propriété de position).
			if (10 * abs(_deep_evaluation._value) > mate_value) {
				_deep_evaluation._winnable_white = _deep_evaluation._value > 0 ? 1.0f : 0.0f;
				_deep_evaluation._winnable_black = _deep_evaluation._value < 0 ? 1.0f : 0.0f;
			}
			_deep_evaluation.get_WDL();
			_deep_evaluation.get_average_score();
```

Le remplacer par (commentaire conservé pour la traçabilité #1/#14) :
```cpp
			// #1 v2 / #14 : champs dérivés cohérents depuis le _value de la TT.
			// Logique factorisée dans tt_fixup_derived (réutilisée par la
			// feuille TT de la recherche principale, #11 Plan A).
			tt_fixup_derived(_deep_evaluation);
```

- [ ] **Step 2: Build**

Run: `MSBuild opti_chess.sln /p:Configuration=Release /p:Platform=x64 /m /nologo /verbosity:minimal`
Expected: build réussi, 0 erreur.

- [ ] **Step 3: [USER] Gate runtime — non-régression du refacto**

Demander à l'utilisateur de confirmer, **toggle non encore câblé donc comportement = baseline** : PERFT 1/2 OK et EVALUATION inchangée sur ses positions habituelles (le bloc 820-829 et `tt_fixup_derived` doivent être strictement équivalents). Ne pas cocher tant que l'utilisateur n'a pas confirmé « OK, identique ».

- [ ] **Step 4: Commit**

```bash
git add opti_chess/exploration.cpp
git commit -m "refactor(search): factoriser la synthese d'eval du cutoff TT en helper partage"
```

---

## Task 3: `init_tt_leaf_child` — feuille TT gelée

But : helper analogue à `init_terminal_draw_child` (exploration.cpp:65-73) qui construit une feuille non expansible portant la valeur TT. Si la position est en réalité terminale (mat/pat détecté par l'éval statique), on garde l'éval terminale exacte au lieu d'écraser par le scalaire TT.

**Files:**
- Modify: `opti_chess/exploration.cpp` (insérer juste après `init_terminal_draw_child`, c.-à-d. après la ligne 73)

- [ ] **Step 1: Ajouter le helper après `init_terminal_draw_child` (après la `}` de la ligne 73)**

Contexte existant (l.65-73) :
```cpp
void init_terminal_draw_child(Node* child, Board* board, Evaluator* eval, Network* network) {
	mark_position_as_draw(*board);

	child->_board = board;
	child->evaluate_position(eval, false, network, true);
	child->_fully_explored = true;
	child->_can_explore = false;
	child->_is_terminal = true;
}
```

Insérer juste après :
```cpp
// #11 Plan A — feuille gelée portant une valeur fiable issue de la TT.
// Structurellement calquée sur init_terminal_draw_child. Détection terminale
// d'abord : Board::evaluate(check_game_over=true) ne CALCULE pas la fin de
// partie (is_game_over() y est commenté, board.cpp:1548) ; il ne fait que LIRE
// _game_over_value. On le calcule donc explicitement comme init_node
// (get_moves puis is_game_over) avant d'évaluer. Si la position est réellement
// terminale, Board::evaluate a déjà posé l'éval exacte (mat/nulle), meilleure
// que le scalaire TT : on la garde et on pose _is_terminal=true. Sinon on
// remplace _deep_evaluation par la valeur TT (white-relative) avec les champs
// dérivés cohérents (#1/#14). Compteurs identiques à la feuille de nulle
// (_nodes=1, _iterations=1) : le terme d'exploration UCT (voir
// pick_random_child, terme d'exploration sur _iterations) se comporte alors
// comme pour les feuilles terminales/nulles existantes.
void init_tt_leaf_child(Node* child, Board* board, Evaluator* eval, Network* network, int white_relative_value) {
	child->_board = board;

	// Fin de partie : Board::evaluate ne la calcule pas (cf. ci-dessus), on
	// reproduit la séquence de init_node avant d'évaluer.
	board->get_moves();
	board->is_game_over();

	child->evaluate_position(eval, false, network, true);

	if (board->_game_over_value != unterminated) {
		// Réellement terminale : éval exacte (mat/nulle) déjà posée par
		// Board::evaluate, supérieure au scalaire TT — on ne l'écrase pas.
		child->_is_terminal = true;
	}
	else {
		child->_deep_evaluation = child->_static_evaluation;
		child->_deep_evaluation._value = white_relative_value;
		child->_deep_evaluation._evaluated = true;
		tt_fixup_derived(child->_deep_evaluation);
	}

	child->_nodes = 1;
	child->_iterations = 1;
	child->_is_stand_pat_eval = false;
	child->_fully_explored = true;
	child->_can_explore = false;
}
```

- [ ] **Step 2: Build**

Run: `MSBuild opti_chess.sln /p:Configuration=Release /p:Platform=x64 /m /nologo /verbosity:minimal`
Expected: build réussi, 0 erreur (helper non encore appelé → warning « unused » acceptable).

- [ ] **Step 3: Commit**

```bash
git add opti_chess/exploration.cpp
git commit -m "feat(search): init_tt_leaf_child, feuille gelee portant une valeur TT (#11)"
```

---

## Task 4: Probe TT + feuille gelée dans `explore_new_move`

But : sur création d'un nouveau nœud non-nul, si le toggle est ON et qu'une entrée `TT_EXACT` fiable existe, court-circuiter la quiescence par une feuille TT. La quiescence du fils est dans `if (!child->_fully_explored)` (l.316) : `init_tt_leaf_child` posant `_fully_explored=true`, ce bloc est naturellement sauté (même mécanisme que la feuille de nulle).

**Files:**
- Modify: `opti_chess/exploration.cpp:267-314` (branche création nouveau nœud + garde avant la quiescence)

- [ ] **Step 1: Marquer la création d'un nouveau nœud non-nul**

Contexte existant (l.296-308), branche `else` (création normale) :
```cpp
		// Sinon, on crée un nouveau noeud normalement
		else {
			record_position_in_history(child_path_history, *new_board);
			new_board->get_zobrist_key();

			// Création du noeud fils (pas de partage via TT — un noeud partagé entre
			// plusieurs parents casse _nodes, backpropagation et crée des cycles)
			child = monte_node_buffer.get_first_free_node();

			if (child == nullptr)
				return;

			child->_board = new_board;
		}
```

Remplacer ce bloc `else { ... }` par (ajout d'un flag local `created_new_node`) :
```cpp
		// Sinon, on crée un nouveau noeud normalement
		else {
			record_position_in_history(child_path_history, *new_board);
			new_board->get_zobrist_key();

			// Création du noeud fils (pas de partage via TT — un noeud partagé entre
			// plusieurs parents casse _nodes, backpropagation et crée des cycles)
			child = monte_node_buffer.get_first_free_node();

			if (child == nullptr)
				return;

			child->_board = new_board;
			created_new_node = true;
		}
```

Et déclarer `created_new_node` avec les autres locales. Contexte existant (l.251-255) :
```cpp
	// Noeud fils
	Node *child = nullptr;

	// Si on a déjà exploré ce coup, mais pas complètement
	bool already_explored = _children.contains(move);
	ChildLink* child_link = already_explored ? &_children[move] : nullptr;
```

Le remplacer par :
```cpp
	// Noeud fils
	Node *child = nullptr;

	// #11 Plan A — vrai pour le seul cas « nouveau noeud, non nulle » :
	// seul cas où la feuille TT peut court-circuiter la quiescence.
	bool created_new_node = false;

	// Si on a déjà exploré ce coup, mais pas complètement
	bool already_explored = _children.contains(move);
	ChildLink* child_link = already_explored ? &_children[move] : nullptr;
```

- [ ] **Step 2: Insérer le probe + feuille TT avant le bloc quiescence**

Contexte existant (l.311-316) :
```cpp
    if (child == nullptr) {
        cout << "null child and shouldn't be (explore_new_move)" << endl;
        return;
    }

	if (!child->_fully_explored) {
```

Insérer entre la `}` du null-check (l.314) et le `if (!child->_fully_explored)` (l.316) :
```cpp
	// #11 Plan A — Probe TT (toggle OFF par défaut). Seulement sur un noeud
	// fraîchement créé non-nulle : les nulles par répétition sont path-dependent
	// (déjà court-circuitées plus haut) et ne doivent jamais être lues comme une
	// valeur de position. Bornes (STANDPAT/BETA/ALPHA) jamais réutilisées comme
	// valeur de noeud (sémantique de borne sans fenêtre en MCTS). On exige
	// _depth dans la bande write-back : un vrai sous-arbre raffiné, pas une
	// feuille de quiescence.
	if (g_tt_main_search && created_new_node) {
		const ZobristEntry* tt_entry = transposition_table.probe(new_board->_zobrist_key);
		if (tt_entry != nullptr
			&& tt_entry->_flag == TT_EXACT
			&& tt_entry->_depth >= QDEPTH_BAND + MIN_REUSE_LOG2) {
			const int tt_eval = tt_denormalize_mate(tt_entry->_eval, new_board->_moves_count); // #3
			init_tt_leaf_child(child, new_board, eval, network, tt_eval * new_board->get_color());
		}
	}

	if (!child->_fully_explored) {
```

Note : `transposition_table.probe` incrémente `_lookups`/`_hits` (zobrist.cpp:100-106) — visible dans les stats, attendu.

- [ ] **Step 3: Build**

Run: `MSBuild opti_chess.sln /p:Configuration=Release /p:Platform=x64 /m /nologo /verbosity:minimal`
Expected: build réussi, 0 erreur.

- [ ] **Step 4: Commit**

```bash
git add opti_chess/exploration.cpp
git commit -m "feat(search): probe TT et feuille gelee dans explore_new_move (#11, toggle OFF)"
```

---

## Task 5: Write-back `TT_EXACT` aux deux points de backup

But : après chaque copie de `_deep_evaluation` depuis le meilleur fils, écrire en retour dans la TT (toggle ON, hors terminal/stand-pat, éval évaluée), profondeur-proxy `tt_writeback_depth(_nodes)`.

**Files:**
- Modify: `opti_chess/exploration.cpp:379-382` (`explore_new_move`)
- Modify: `opti_chess/exploration.cpp:410-412` (`explore_random_child`)

- [ ] **Step 1: Write-back dans `explore_new_move`**

Contexte existant (l.376-382) :
```cpp
	// Standpat = le meilleur
	if (best_move.is_null_move()) {
		_is_stand_pat_eval = true;
	}
	else {
		_is_stand_pat_eval = false;
		_deep_evaluation = _children[best_move]._node->_deep_evaluation;
	}
```

Le remplacer par :
```cpp
	// Standpat = le meilleur
	if (best_move.is_null_move()) {
		_is_stand_pat_eval = true;
	}
	else {
		_is_stand_pat_eval = false;
		_deep_evaluation = _children[best_move]._node->_deep_evaluation;

		// #11 Plan A — write-back de la valeur raffinée (le levier réel).
		// Garde : toggle ON, pas terminal (exclut les nulles path-dependent),
		// pas stand-pat (borne inf déjà gérée en TT_STANDPAT par la quiescence),
		// éval évaluée. Profondeur-proxy au-dessus de la bande quiescence.
		if (g_tt_main_search && !_is_terminal && !_is_stand_pat_eval && _deep_evaluation._evaluated) {
			transposition_table.store(_board->_zobrist_key,
				tt_normalize_mate(_deep_evaluation._value, _board->_moves_count), // #3
				tt_writeback_depth(_nodes), TT_EXACT);
		}
	}
```

- [ ] **Step 2: Write-back dans `explore_random_child`**

Contexte existant (l.410-412) :
```cpp
	// Met à jour l'évaluation du plateau avec le meilleur coup
	_deep_evaluation = _children[get_best_score_move(alpha, beta)]._node->_deep_evaluation;

	// Augmente le nombre de noeuds
```

Le remplacer par :
```cpp
	// Met à jour l'évaluation du plateau avec le meilleur coup
	_deep_evaluation = _children[get_best_score_move(alpha, beta)]._node->_deep_evaluation;

	// #11 Plan A — write-back de la valeur raffinée (le levier réel).
	// Mêmes gardes que dans explore_new_move.
	if (g_tt_main_search && !_is_terminal && !_is_stand_pat_eval && _deep_evaluation._evaluated) {
		transposition_table.store(_board->_zobrist_key,
			tt_normalize_mate(_deep_evaluation._value, _board->_moves_count), // #3
			tt_writeback_depth(_nodes), TT_EXACT);
	}

	// Augmente le nombre de noeuds
```

Note : `_is_stand_pat_eval` n'est pas réassigné dans `explore_random_child` ; on réutilise sa valeur courante (cohérent : un nœud stand-pat ne doit pas être écrit en `TT_EXACT`).

- [ ] **Step 3: Build**

Run: `MSBuild opti_chess.sln /p:Configuration=Release /p:Platform=x64 /m /nologo /verbosity:minimal`
Expected: build réussi, 0 erreur.

- [ ] **Step 4: Commit**

```bash
git add opti_chess/exploration.cpp
git commit -m "feat(search): write-back TT_EXACT de la valeur raffinee (#11, toggle OFF)"
```

---

## Task 6: Câblage du toggle runtime (GUI)

But : exposer le toggle à l'utilisateur pour l'A/B même binaire (membre GUI + miroir vers le global au site d'appel + HUD + keybind).

**Files:**
- Modify: `opti_chess/gui.h:108` (près de `_explore_checks`)
- Modify: `opti_chess/gui.cpp:1055` (avant l'appel `grogros_zero`)
- Modify: `opti_chess/gui.cpp:1417` (chaîne HUD près de `_explore_checks`)
- Modify: `opti_chess/main_gui.h` (zone inputs, près de la l.303 `KEY_F`)

- [ ] **Step 1: Membre GUI**

Contexte existant (gui.h:108) :
```cpp
	bool _explore_checks = true; // FIXME? faut-il vraiment explorer les échecs?
```

Ajouter juste en dessous :
```cpp
	bool _tt_main_search = false; // #11 Plan A — TT dans la recherche principale (A/B runtime, defaut OFF)
```

- [ ] **Step 2: Miroir membre→global avant l'appel grogros_zero**

Contexte existant (gui.cpp:1055) :
```cpp
	_root_exploration_node->grogros_zero(&monte_board_buffer, _grogros_eval, _alpha, _beta, _gamma, iterations == -1 ? iterations_to_explore : iterations, _quiescence_depth); // TODO: nombre de noeuds à paramétrer
```

Insérer la ligne miroir juste avant :
```cpp
	g_tt_main_search = _tt_main_search; // #11 Plan A — propage le toggle au global lu dans exploration.cpp
	_root_exploration_node->grogros_zero(&monte_board_buffer, _grogros_eval, _alpha, _beta, _gamma, iterations == -1 ? iterations_to_explore : iterations, _quiescence_depth); // TODO: nombre de noeuds à paramétrer
```

Vérifier que `exploration.h` est inclus dans `gui.cpp` (il l'est déjà : `grogros_zero` y est appelé). Aucun include à ajouter.

- [ ] **Step 3: Ligne HUD**

Contexte existant (gui.cpp:1417) — fin de la chaîne :
```cpp
	string monte_carlo_text = static_cast<string>(_grogros_analysis ? "STOP GrogrosZero-Auto (CTRL-H)" : "RUN GrogrosZero-Auto (CTRL-G)") + "\nCONTROLS (H)" + "\n\nSEARCH PARAMETERS\nalpha: " + to_string(_alpha) + "\nbeta: " + to_string(_beta) + "\ngamma : " + to_string(_gamma) + "\nq_depth : " + to_string(_quiescence_depth) + "\nexplore checks : " + (_explore_checks ? "true" : "false");
```

Ajouter le segment TT en fin de chaîne (avant le `;`) :
```cpp
	string monte_carlo_text = static_cast<string>(_grogros_analysis ? "STOP GrogrosZero-Auto (CTRL-H)" : "RUN GrogrosZero-Auto (CTRL-G)") + "\nCONTROLS (H)" + "\n\nSEARCH PARAMETERS\nalpha: " + to_string(_alpha) + "\nbeta: " + to_string(_beta) + "\ngamma : " + to_string(_gamma) + "\nq_depth : " + to_string(_quiescence_depth) + "\nexplore checks : " + (_explore_checks ? "true" : "false") + "\nTT main search : " + (_tt_main_search ? "true" : "false") + " (I)";
```

- [ ] **Step 4: Keybind de bascule (`I`, non utilisé)**

Contexte existant (main_gui.h:302-305) :
```cpp
		// F - Retourne le plateau
		if (IsKeyPressed(KEY_F)) {
			main_GUI.switch_orientation();
		}
```

Insérer juste après le `}` (l.305) :
```cpp
		// I - #11 Plan A : bascule TT dans la recherche principale (A/B runtime)
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_I)) {
			main_GUI._tt_main_search = !main_GUI._tt_main_search;
			cout << "TT main search : " << (main_GUI._tt_main_search ? "true" : "false") << endl;
		}
```

- [ ] **Step 5: Build**

Run: `MSBuild opti_chess.sln /p:Configuration=Release /p:Platform=x64 /m /nologo /verbosity:minimal`
Expected: build réussi, 0 erreur.

- [ ] **Step 6: Commit**

```bash
git add opti_chess/gui.h opti_chess/gui.cpp opti_chess/main_gui.h
git commit -m "feat(gui): toggle runtime TT recherche principale + HUD + keybind I (#11)"
```

---

## Task 7: [USER] Gate runtime final — non-régression + bench OFF/ON

But : valider la spec §6. Aucune modif de code ; étapes lancées par l'utilisateur.

- [ ] **Step 1: [USER] Non-régression toggle OFF**

L'utilisateur lance le binaire **sans toucher la touche `I`** (toggle OFF par défaut, HUD doit afficher `TT main search : false`). Confirmer : PERFT 1/2 OK, EVALUATION inchangée vs `main` sur ses positions habituelles (comportement attendu au byte près quand OFF).

- [ ] **Step 2: [USER] Bench OFF vs ON sur la FEN de référence**

Position de référence : finale roi + pions bloqués (réf. vidéo Sebastian Lague). Budget en **itérations fixes** (pas wall-time), même FEN, même binaire. Mesurer pour OFF puis ON (basculer avec `I`, HUD le confirme) :
- `_root_exploration_node->_nodes`
- `get_main_depth(_alpha, _beta)` (profondeur de variante principale)
- meilleur coup
- temps

Attendu ON : **moins de nœuds ET profondeur supérieure** à itérations égales, **même meilleur coup** sur positions à transpositions. Inspecter les stats TT (`stats_string()`) : `stores`/`overwrites` non nuls, `cutoffs` en hausse vs baseline.

- [ ] **Step 3: [USER] Verdict**

Si gain confirmé sans régression de jeu → succès #11 Plan A. Sinon, ajuster `MIN_REUSE_LOG2` / `QDEPTH_BAND` (Task 1 Step 1) et reboucler le bench. Si A insuffisant pour le gain visé → envisager Plan B (DAG), hors périmètre de ce plan.

- [ ] **Step 4: Mettre à jour le suivi des bugs**

Dans `opti_chess/BUGFIXES.md`, passer #11 de 🔴 à ✅ (ou 🔧 selon le verdict utilisateur), en résumant le résultat du bench (nœuds/profondeur OFF vs ON) et la valeur retenue des constantes.

```bash
git add opti_chess/BUGFIXES.md
git commit -m "docs(bugfixes): #11 plan A - resultat bench TT recherche principale"
```

---

## Self-Review (rempli par l'auteur du plan)

- **Couverture spec** : §1 gating → Task 1 (global) + Task 6 (GUI/keybind, défaut OFF). §2 probe → Task 4 (gate `TT_EXACT` + bande, denormalize #3, feuille via Task 3). §3 write-back → Task 5 (deux sites, gardes terminal/stand-pat, `tt_writeback_depth`, normalize #3). §4 constantes → Task 1 (`QDEPTH_BAND=256`, `MIN_REUSE_LOG2=4`). §5 invariants → Task 3 (`_nodes=1`/`_iterations=1`/`_can_explore=false`, branche terminale) + helper `tt_fixup_derived` partagé (Task 1/2). §6 bench → Task 7. Refacto DRY de la synthèse #1/#14 → Task 2. Aucune section spec sans tâche.
- **Placeholders** : aucun TBD/TODO ajouté ; tout le code des steps est complet.
- **Cohérence des types** : `tt_fixup_derived(Evaluation&)`, `tt_writeback_depth(int)`, `init_tt_leaf_child(Node*, Board*, Evaluator*, Network*, int)`, `g_tt_main_search` (bool), `_tt_main_search` (membre GUI), `QDEPTH_BAND`/`MIN_REUSE_LOG2` (constexpr int) — noms et signatures identiques entre Task 1 (définition) et Tasks 2/3/4/5/6 (usage). `Evaluation` expose `_value/_evaluated/_uncertainty/_winnable_white/_winnable_black/get_WDL()/get_average_score()` (déjà utilisés au bloc 820-829 d'origine). `ZobristEntry._flag/_depth/_eval` et `TT_EXACT` conformes à zobrist.h.
