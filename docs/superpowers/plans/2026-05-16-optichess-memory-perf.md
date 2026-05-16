# OptiChess Memory/Perf Cleanup — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Borner la mémoire et passer l'allocation de Node/Board en O(1) pour corriger #12 (chargement FEN lent/infini), #13 (gel système) et #2 (copie ~6 Ko/appel de `get_zobrist_key`), sans changer le comportement de jeu.

**Architecture :** Free-list (pile d'indices `std::vector<int>`) dans `BoardBuffer`/`NodeBuffer` remplaçant le scan O(n) ; chaque `Board`/`Node` connaît son index de buffer (`_buffer_index`) ; libération branchée sur les points de reset existants ; dimensionnement des pools calculé au démarrage depuis la RAM physique (Windows `GlobalMemoryStatusEx`) ; buffer plein → reroutage propre dans la boucle GrogrosZero.

**Tech Stack :** C++17, MSVC (Visual Studio `opti_chess.sln`), raylib (GUI), tsl::robin_map, `<windows.h>` (déjà utilisé dans `windows_tests.cpp`).

**Référence spec :** `docs/superpowers/specs/2026-05-16-optichess-memory-perf-design.md`

**Branche de travail :** `fix/buffer-memory-perf` (déjà créée et courante).

---

## Contexte testing (lire avant de commencer)

Le projet **n'a pas de framework de tests unitaires**. La validation se fait via le harnais intégré `Tests::run_all_tests()` (`tests.cpp:159`) :

- **Build :** ouvrir `opti_chess.sln` dans Visual Studio et compiler (config habituelle, p.ex. Release|x64), ou en ligne de commande :
  `msbuild opti_chess.sln /p:Configuration=Release /p:Platform=x64 /m /nologo`
  (utiliser la config/plateforme que l'utilisateur compile d'habitude).
- **Lancer la régression :** exécuter le binaire, puis dans la fenêtre GUI **appuyer sur la touche `T`** (sans Ctrl) — cela déclenche `Tests::run_all_tests()` (`main_gui.h:131` → `:236`).
- **Gate de correctness (invariant absolu) :** la console doit afficher `*** PERFT RESULTS: 2/2 ***`. Le perft compte exactement les nœuds de génération de coups : s'il reste à 2/2, la génération de coups, `make_move`, FEN et Zobrist sont intacts. Toute autre valeur = régression à corriger avant de continuer.
- **Note Zobrist (Task 1) :** vérifier en plus que les scores de la section `*** EVALUATION TESTS ***` sont **identiques** à la baseline (capturée en Task 0).

Chaque task se termine par : build OK → `T` → `PERFT RESULTS: 2/2` → commit.

---

## File Structure

| Fichier | Responsabilité | Action |
|---|---|---|
| `opti_chess/board.cpp:6996` | `get_zobrist_key` : référence au lieu de copie (#2) | Modifier |
| `opti_chess/board.h` | `Board` : ajouter `_buffer_index` | Modifier |
| `opti_chess/board.cpp:2494` | `Board::reset_board` : hook free-list | Modifier |
| `opti_chess/buffer.h` | `BoardBuffer` : free-list, `is_full`, sizing API | Modifier |
| `opti_chess/buffer.cpp` | `BoardBuffer` impl O(1) + `compute_pool_sizing` | Modifier |
| `opti_chess/exploration.h` | `Node` (`_buffer_index`), `NodeBuffer` free-list, flag log | Modifier |
| `opti_chess/exploration.cpp` | `NodeBuffer` impl O(1) ; `Node::reset` hook ; reroutage `grogros_zero` ; retrait des `cout` spam | Modifier |
| `opti_chess/gui.h:415-416` | Retrait des constantes magiques `1E7` | Modifier |
| `opti_chess/gui.cpp:110,142,1898-1903` | Sites d'init buffers → sizing adaptatif | Modifier |
| `opti_chess/main_gui.h:84-87` | TT size → sizing adaptatif | Modifier |
| `opti_chess/BUGFIXES.md` | #2/#12/#13 → corrigés + preuve | Modifier |

---

## Task 0: Baseline

**Files:** aucun (mesures de référence).

- [ ] **Step 1 : Build de référence**

Compiler la branche `fix/buffer-memory-perf` telle quelle.
Run : `msbuild opti_chess.sln /p:Configuration=Release /p:Platform=x64 /m /nologo`
Expected : build réussi (0 erreur).

- [ ] **Step 2 : Capturer la baseline**

Lancer le binaire, appuyer sur `T`. Noter dans un fichier scratch (hors git) :
- la ligne `*** PERFT RESULTS: x/2 ***` (doit être `2/2`),
- tous les scores des sections `EVALUATION TESTS` (valeurs `x/1` par catégorie),
- charger une FEN de finale (p.ex. `8/pppbn2r/3p4/4k1p1/1P2P3/P1P1RP2/6P1/3R2K1 b - - 1 27`) et chronométrer subjectivement le temps de chargement (instantané ? plusieurs secondes ?),
- ouvrir le Gestionnaire des tâches, lancer une réflexion (`Entrée`) 60 s, noter le pic de RSS du process.

Expected : référence écrite. `PERFT RESULTS: 2/2`.

- [ ] **Step 3 : Commit (point de départ propre)**

```bash
git status
```
Expected : working tree clean (rien à committer ; baseline notée hors git). Ne pas committer.

---

## Task 1: Fix #2 — `get_zobrist_key` par référence

**Files:**
- Modify: `opti_chess/board.cpp:6996-7043`

`TranspositionTable::init` génère déjà les clés au démarrage (`zobrist.cpp:82`) et `generate_zobrist_keys()` est idempotent (`zobrist.cpp:13-14`). On supprime la copie du struct `Zobrist` (~6 Ko : `_board_keys[64][12]`) à chaque appel ; on garde un garde-fou idempotent qui n'opère **jamais** de copie.

- [ ] **Step 1 : Remplacer la copie par une référence**

Dans `opti_chess/board.cpp`, remplacer exactement ce bloc (lignes 7002-7008) :

```cpp
	// FIXME: elle est calculée plusieurs fois?

	Zobrist zobrist = transposition_table._zobrist;
	
	// Génération des clés de Zobrist si ce n'est pas déjà fait
	if (!zobrist._keys_generated)
		zobrist.generate_zobrist_keys();
```

par :

```cpp
	// #2: pas de copie du struct Zobrist (~6 Ko) à chaque appel — référence.
	// Les clés sont générées une fois au démarrage (TranspositionTable::init).
	// Garde-fou idempotent (generate_zobrist_keys() early-return si déjà fait) :
	// opère sur l'objet réel, jamais une copie.
	if (!transposition_table._zobrist._keys_generated)
		transposition_table._zobrist.generate_zobrist_keys();

	const Zobrist& zobrist = transposition_table._zobrist;
```

Le reste de la fonction (`zobrist._initial_key`, `zobrist._board_keys[...]`, etc.) est inchangé : `zobrist` est désormais une référence const, toutes les lectures restent valides.

- [ ] **Step 2 : Build**

Run : `msbuild opti_chess.sln /p:Configuration=Release /p:Platform=x64 /m /nologo`
Expected : build réussi.

- [ ] **Step 3 : Régression perft + Zobrist inchangé**

Lancer le binaire, appuyer sur `T`.
Expected : `*** PERFT RESULTS: 2/2 ***` ET tous les scores `EVALUATION TESTS` **identiques** à la baseline Task 0 (la clé Zobrist ne doit rien changer fonctionnellement).

- [ ] **Step 4 : Vérifier le symptôme #12**

Charger la FEN de finale de Task 0 Step 2. 
Expected : chargement nettement plus rapide qu'en baseline (le coût 6 Ko/appel sur le chemin d'historique disparaît).

- [ ] **Step 5 : Commit**

```bash
git add opti_chess/board.cpp
git commit -m "perf(zobrist): get_zobrist_key par reference, supprime la copie 6Ko/appel (#2)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Task 2: Free-list O(1) — infrastructure `BoardBuffer` & `NodeBuffer`

**Files:**
- Modify: `opti_chess/board.h` (ajouter `_buffer_index` à `Board`)
- Modify: `opti_chess/exploration.h` (ajouter `_buffer_index` à `Node` ; free-list `NodeBuffer`)
- Modify: `opti_chess/buffer.h` (free-list `BoardBuffer` + `is_full`)
- Modify: `opti_chess/buffer.cpp` (impl O(1))
- Modify: `opti_chess/exploration.cpp:1505-1597` (impl O(1) `NodeBuffer`)

On ajoute à chaque buffer une pile d'indices libres et un drapeau `_bulk_resetting` (pour que le reset global ne double-pousse pas via les hooks de Task 3).

- [ ] **Step 1 : `Board::_buffer_index`**

Dans `opti_chess/board.h`, dans la classe `Board`, à côté du champ `_is_active` (chercher `_is_active`), ajouter :

```cpp
	// Index dans monte_board_buffer (-1 = objet hors buffer : ne pas recycler)
	int _buffer_index = -1;
```

- [ ] **Step 2 : `Node::_buffer_index`**

Dans `opti_chess/exploration.h`, classe `Node`, juste après la ligne `bool _is_active = false;` (ligne ~93) ajouter :

```cpp
	// Index dans monte_node_buffer (-1 = objet hors buffer : ne pas recycler)
	int _buffer_index = -1;
```

- [ ] **Step 3 : API free-list `BoardBuffer` (buffer.h)**

Dans `opti_chess/buffer.h`, classe `BoardBuffer`, ajouter `#include <vector>` en tête du fichier (après `#include "board.h"`), puis dans la classe ajouter les membres et méthodes (après `int _iterator = -1;`) :

```cpp
	// Free-list : pile d'indices de plateaux libres (allocation/libération O(1))
	std::vector<int> _free_indices;

	// Vrai pendant reset()/init() : les hooks de libération ne repoussent pas
	bool _bulk_resetting = false;

	// Le buffer est-il plein ? (O(1))
	bool is_full() const { return _free_indices.empty(); }

	// Repousse un index libéré (appelé depuis Board::reset_board)
	void free_index(int index) { _free_indices.push_back(index); }
```

- [ ] **Step 4 : Impl `BoardBuffer` O(1) (buffer.cpp)**

Dans `opti_chess/buffer.cpp`, remplacer **intégralement** le contenu des fonctions suivantes.

Constructeur par défaut (lignes 4-12) — remplacer :

```cpp
// Constructeur par défaut
BoardBuffer::BoardBuffer() {
	// Crée un gros buffer, de 4GB
	constexpr unsigned long int _size_buffer = 4000000000;
	_length = _size_buffer / sizeof(Board);
	_length = 0;

	_boards = new Board[_length];
}

// Constructeur utilisant la taille max (en bits) du buffer
BoardBuffer::BoardBuffer(const unsigned long int size) {
	_length = size / sizeof(Board);
	_boards = new Board[_length];
}
```

par :

```cpp
// Constructeur par défaut : n'alloue rien, init() est obligatoire
BoardBuffer::BoardBuffer() {
	_boards = nullptr;
	_length = 0;
}

// Constructeur taille (octets) : alloue immédiatement
BoardBuffer::BoardBuffer(const size_t size_bytes) {
	init(static_cast<int>(size_bytes / sizeof(Board)), false);
}
```

`init` (lignes 21-41) — remplacer le corps par :

```cpp
void BoardBuffer::init(const int length, bool display) {
	if (_init) {
		if (display)
			cout << "board buffer already initialized" << endl;
		return;
	}

	if (display)
		cout << "\ninitializing board buffer..." << endl;

	_length = length;
	_boards = new Board[_length];

	// Chaque plateau connaît son index ; free-list = tous les indices libres
	_free_indices.clear();
	_free_indices.reserve(_length);
	for (int i = _length - 1; i >= 0; i--) {
		_boards[i]._buffer_index = i;
		_free_indices.push_back(i);
	}

	_init = true;

	if (display) {
		cout << "board buffer initialized" << endl;
		cout << "board size: " << int_to_round_string(sizeof(Board)) << "b" << endl;
		cout << "length: " << int_to_round_string(_length) << endl;
		cout << "approximate buffer size: " << long_int_to_round_string((long long int)_length * sizeof(Board)) << "b\n\n";
	}
}
```

`get_first_free_index` (lignes 44-54) — remplacer le corps par une dépile O(1) :

```cpp
int BoardBuffer::get_first_free_index() {
	if (_free_indices.empty())
		return -1;
	const int index = _free_indices.back();
	_free_indices.pop_back();
	return index;
}
```

`remove` (lignes 57-62) — remplacer par :

```cpp
void BoardBuffer::remove() {
	delete[] _boards;
	_boards = nullptr;
	_init = false;
	_length = 0;
	_iterator = -1;
	_free_indices.clear();
}
```

`reset` (lignes 65-71) — remplacer par (rebuild complet de la free-list, sans double-push via les hooks) :

```cpp
bool BoardBuffer::reset() {
	_bulk_resetting = true;
	for (int i = 0; i < _length; i++)
		_boards[i].reset_board();
	_bulk_resetting = false;

	_free_indices.clear();
	_free_indices.reserve(_length);
	for (int i = _length - 1; i >= 0; i--)
		_free_indices.push_back(i);

	return true;
}
```

`get_first_free_board` (lignes 74-86) — remplacer par (plus de `cout` à chaque appel) :

```cpp
Board* BoardBuffer::get_first_free_board() {
	const int index = get_first_free_index();
	if (index == -1)
		return nullptr;

	Board* board = &_boards[index];
	board->_is_active = true;
	return board;
}
```

- [ ] **Step 5 : Signature ctor taille dans buffer.h**

Dans `opti_chess/buffer.h`, remplacer la déclaration :

```cpp
	explicit BoardBuffer(unsigned long int);
```

par :

```cpp
	explicit BoardBuffer(size_t);
```

- [ ] **Step 6 : API free-list `NodeBuffer` (exploration.h)**

Dans `opti_chess/exploration.h`, en tête ajouter `#include <vector>` (après les includes existants), puis dans la classe `NodeBuffer`, après `int _iterator = -1;` (ligne ~216) ajouter :

```cpp
	std::vector<int> _free_indices;
	bool _bulk_resetting = false;
	bool is_full() const { return _free_indices.empty(); }
	void free_index(int index) { _free_indices.push_back(index); }
```

- [ ] **Step 7 : Impl `NodeBuffer` O(1) (exploration.cpp)**

Dans `opti_chess/exploration.cpp`, appliquer les **mêmes** transformations qu'en Step 4, sur les fonctions `NodeBuffer` (lignes 1505-1597), `Node` au lieu de `Board`, `_nodes` au lieu de `_boards`, `reset(false)` au lieu de `reset_board()` :

Ctors (1505-1519) → :

```cpp
// Constructeur par défaut : n'alloue rien, init() est obligatoire
NodeBuffer::NodeBuffer() {
	_nodes = nullptr;
	_length = 0;
}

// Constructeur taille (octets) : alloue immédiatement
NodeBuffer::NodeBuffer(const size_t size_bytes) {
	init(static_cast<int>(size_bytes / sizeof(Node)), false);
}
```

`init` (1522-1542) → corps :

```cpp
void NodeBuffer::init(const int length, bool display) {
	if (_init) {
		if (display)
			cout << "node buffer already initialized" << endl;
		return;
	}

	if (display)
		cout << "\ninitializing node buffer..." << endl;

	_length = length;
	_nodes = new Node[_length];

	_free_indices.clear();
	_free_indices.reserve(_length);
	for (int i = _length - 1; i >= 0; i--) {
		_nodes[i]._buffer_index = i;
		_free_indices.push_back(i);
	}

	_init = true;

	if (display) {
		cout << "node buffer initialized" << endl;
		cout << "node size: " << int_to_round_string(sizeof(Node)) << "b" << endl;
		cout << "length: " << int_to_round_string(_length) << endl;
		cout << "approximate buffer size: " << long_int_to_round_string((long long int)_length * sizeof(Node)) << "b\n\n";
	}
}
```

`get_first_free_index` (1545-1555) → :

```cpp
int NodeBuffer::get_first_free_index() {
	if (_free_indices.empty())
		return -1;
	const int index = _free_indices.back();
	_free_indices.pop_back();
	return index;
}
```

`remove` (1558-1563) → :

```cpp
void NodeBuffer::remove() {
	delete[] _nodes;
	_nodes = nullptr;
	_init = false;
	_length = 0;
	_iterator = -1;
	_free_indices.clear();
}
```

`reset` (1566-1572) → :

```cpp
bool NodeBuffer::reset() {
	_bulk_resetting = true;
	for (int i = 0; i < _length; i++)
		_nodes[i].reset(false);
	_bulk_resetting = false;

	_free_indices.clear();
	_free_indices.reserve(_length);
	for (int i = _length - 1; i >= 0; i--)
		_free_indices.push_back(i);

	return true;
}
```

`get_first_free_node` (1575-1587) → :

```cpp
Node* NodeBuffer::get_first_free_node() {
	const int index = get_first_free_index();
	if (index == -1)
		return nullptr;

	Node* node = &_nodes[index];
	node->_is_active = true;
	return node;
}
```

- [ ] **Step 8 : Signature ctor taille dans exploration.h**

Dans `opti_chess/exploration.h`, remplacer `explicit NodeBuffer(unsigned long int);` par `explicit NodeBuffer(size_t);`.

- [ ] **Step 9 : Build**

Run : `msbuild opti_chess.sln /p:Configuration=Release /p:Platform=x64 /m /nologo`
Expected : build réussi. (À ce stade les indices ne sont pas encore repoussés à la libération — c'est Task 3 ; le buffer se comporte comme une allocation séquentielle jusqu'à épuisement, ce qui suffit pour valider la régression sur des recherches courtes.)

- [ ] **Step 10 : Régression**

Lancer, appuyer sur `T`.
Expected : `*** PERFT RESULTS: 2/2 ***`, scores EVALUATION identiques baseline.

- [ ] **Step 11 : Commit**

```bash
git add opti_chess/board.h opti_chess/buffer.h opti_chess/buffer.cpp opti_chess/exploration.h opti_chess/exploration.cpp
git commit -m "perf(buffer): free-list O(1) pour BoardBuffer/NodeBuffer, retire le scan O(n) et le code mort

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Task 3: Brancher la libération sur les resets

**Files:**
- Modify: `opti_chess/board.cpp:1` (include) et `:2494` (`Board::reset_board`)
- Modify: `opti_chess/exploration.cpp:421` (`Node::reset`)

Aujourd'hui un Board/Node est « libéré » uniquement quand `_is_active` repasse à `false`, ce qui n'arrive **que** dans `Board::reset_board()` (`board.cpp:2496`) et `Node::reset()` (`exploration.cpp:439`). On y repousse l'index, en ne le faisant que si l'objet appartient au buffer (`_buffer_index >= 0`), s'il était actif, et hors reset global (`!_bulk_resetting`).

- [ ] **Step 1 : Include buffer dans board.cpp**

En tête de `opti_chess/board.cpp`, après les includes existants (chercher la dernière ligne `#include "..."` du bloc d'en-tête), ajouter :

```cpp
#include "buffer.h"   // monte_board_buffer (recyclage free-list dans reset_board)
```

- [ ] **Step 2 : Hook dans `Board::reset_board`**

Dans `opti_chess/board.cpp`, remplacer le début de `reset_board` (lignes 2494-2496) :

```cpp
void Board::reset_board(const bool display) {
	_got_moves = -1;
	_is_active = false;
```

par :

```cpp
void Board::reset_board(const bool display) {
	const bool was_active = _is_active;
	_got_moves = -1;
	_is_active = false;
```

Puis, juste avant le `if (display)` final (après `reset_positions_history();`, ligne ~2503), insérer :

```cpp
	// Recyclage free-list : seulement les plateaux du buffer, actifs, hors reset global
	if (was_active && _buffer_index >= 0 && !monte_board_buffer._bulk_resetting)
		monte_board_buffer.free_index(_buffer_index);
```

- [ ] **Step 3 : Hook dans `Node::reset`**

Dans `opti_chess/exploration.cpp`, remplacer le début de `Node::reset` (lignes 421-422) :

```cpp
void Node::reset(bool recursive) {
	_latest_first_move_explored = -1;
```

par :

```cpp
void Node::reset(bool recursive) {
	const bool was_active = _is_active;
	_latest_first_move_explored = -1;
```

Puis, tout à la fin de la fonction, **après** `_children.clear();` (ligne ~456) et avant l'accolade fermante, insérer :

```cpp
	// Recyclage free-list : seulement les noeuds du buffer, actifs, hors reset global.
	// (_board->reset_board() ci-dessus a déjà recyclé le plateau associé.)
	if (was_active && _buffer_index >= 0 && !monte_node_buffer._bulk_resetting)
		monte_node_buffer.free_index(_buffer_index);
```

- [ ] **Step 4 : Build**

Run : `msbuild opti_chess.sln /p:Configuration=Release /p:Platform=x64 /m /nologo`
Expected : build réussi.

- [ ] **Step 5 : Régression + recyclage effectif**

Lancer, appuyer sur `T` → `*** PERFT RESULTS: 2/2 ***`, EVALUATION identiques.
Puis : lancer une réflexion (`Entrée`), naviguer dans plusieurs coups / charger plusieurs FEN successives pendant ~2 min. 
Expected : pas de message « buffer plein » alors que le buffer n'est pas réellement saturé (les indices sont bien recyclés ; sans le hook, une longue session épuiserait le buffer séquentiellement). Aucun crash.

- [ ] **Step 6 : Commit**

```bash
git add opti_chess/board.cpp opti_chess/exploration.cpp
git commit -m "perf(buffer): recyclage des indices libres sur reset_board/Node::reset

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Task 4: Dimensionnement mémoire adaptatif

**Files:**
- Modify: `opti_chess/buffer.h` (API `compute_pool_sizing`)
- Modify: `opti_chess/buffer.cpp` (impl + `#include <windows.h>`)
- Modify: `opti_chess/gui.h:415-416` (retrait constantes `1E7`)
- Modify: `opti_chess/gui.cpp:110,142,1898-1903` (sites d'init)
- Modify: `opti_chess/main_gui.h:84-87` (TT)

Budget = `min(fraction × RAM physique dispo, plafond_dur)`. TT plafonnée à `min(tt_max, budget/4)`, puis reste réparti à parts égales en **nombre** d'entrées Node et Board (un Board ≈ un Node expansé).

- [ ] **Step 1 : Déclaration API (buffer.h)**

Dans `opti_chess/buffer.h`, après les `#include` (avant `class BoardBuffer`), ajouter :

```cpp
#include <cstddef>

// Résultat du dimensionnement des pools (nombre d'entrées)
struct PoolSizing {
	int board_length;
	int node_length;
	int tt_length;
};

// Calcule les tailles des pools depuis la RAM physique dispo au démarrage.
// ram_fraction : part de la RAM dispo utilisable. hard_cap_bytes : plafond dur.
// tt_max_entries : plafond du nombre d'entrées de la table de transposition.
// NB: hard_cap_bytes en unsigned long long (pas size_t) — 4 GiB déborderait
//     un size_t 32-bit en build Win32 (exactement le bug que ce cleanup tue).
PoolSizing compute_pool_sizing(double ram_fraction = 0.5,
                               unsigned long long hard_cap_bytes = 4ull * 1024 * 1024 * 1024,
                               int tt_max_entries = 5000000);
```

- [ ] **Step 2 : Implémentation (buffer.cpp)**

En tête de `opti_chess/buffer.cpp`, après `#include "useful_functions.h"`, ajouter :

```cpp
#include <windows.h>
#include "exploration.h"   // sizeof(Node)
#include "zobrist.h"       // sizeof(ZobristEntry)
```

Puis, à la fin du fichier (après `BoardBuffer monte_board_buffer;`), ajouter :

```cpp
PoolSizing compute_pool_sizing(double ram_fraction, unsigned long long hard_cap_bytes, int tt_max_entries) {
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);

	// Tout le calcul d'octets en unsigned long long (64-bit, portable Win32/x64)
	unsigned long long budget = (unsigned long long)((double)status.ullAvailPhys * ram_fraction);
	if (budget > hard_cap_bytes)
		budget = hard_cap_bytes;

	// Coût approché d'une entrée robin_map<uint64_t, ZobristEntry> (overhead ~2x)
	const unsigned long long tt_entry_bytes = (unsigned long long)(sizeof(uint64_t) + sizeof(ZobristEntry)) * 2;
	unsigned long long tt_bytes = (unsigned long long)tt_max_entries * tt_entry_bytes;
	if (tt_bytes > budget / 4)
		tt_bytes = budget / 4;
	const int tt_length = (int)(tt_bytes / tt_entry_bytes);

	// Reste réparti : autant d'entrées Node que Board
	const unsigned long long rest = budget - tt_bytes;
	const unsigned long long pair_bytes = (unsigned long long)(sizeof(Board) + sizeof(Node));
	const int count = (int)(rest / pair_bytes);

	PoolSizing ps;
	ps.board_length = count;
	ps.node_length = count;
	ps.tt_length = tt_length;
	return ps;
}
```

> Note inclusion : `buffer.cpp` inclut `exploration.h` (qui inclut `buffer.h`, protégé par `#pragma once`) — pas de cycle car c'est dans le `.cpp`. `buffer.cpp` n'inclut pas raylib → pas de conflit `<windows.h>`.

- [ ] **Step 3 : Retirer les constantes magiques (gui.h)**

Dans `opti_chess/gui.h`, supprimer les lignes 414-416 :

```cpp
	// Taille des buffers à utilser
	const int _board_buffer_length = 1E7;
	const int _node_buffer_length = 1E7;
```

(Seules `gui.cpp:1899` et `:1903` les référençaient ; corrigées au Step 4.)

- [ ] **Step 4 : Sites d'init buffers (gui.cpp)**

Dans `opti_chess/gui.cpp`, remplacer le bloc 1898-1903 :

```cpp
	if (!monte_board_buffer._init)
		monte_board_buffer.init(_board_buffer_length);

	if (!monte_node_buffer._init)
		monte_node_buffer.init(_node_buffer_length);
```

par :

```cpp
	if (!monte_board_buffer._init || !monte_node_buffer._init) {
		const PoolSizing ps = compute_pool_sizing();
		if (!monte_board_buffer._init)
			monte_board_buffer.init(ps.board_length);
		if (!monte_node_buffer._init)
			monte_node_buffer.init(ps.node_length);
	}
```

Puis, aux lignes 109-110 et 141-142, remplacer **chacune** des deux occurrences de :

```cpp
	if (!monte_board_buffer._init)
		monte_board_buffer.init();
```

par :

```cpp
	if (!monte_board_buffer._init || !monte_node_buffer._init) {
		const PoolSizing ps = compute_pool_sizing();
		if (!monte_board_buffer._init)
			monte_board_buffer.init(ps.board_length);
		if (!monte_node_buffer._init)
			monte_node_buffer.init(ps.node_length);
	}
```

(On initialise aussi le node buffer ici : ces chemins lançaient une recherche sans garantir son init.)

- [ ] **Step 5 : TT adaptative (main_gui.h)**

Dans `opti_chess/main_gui.h`, remplacer les lignes 83-87 :

```cpp
	// Taille de la table de transposition
	constexpr int transposition_table_size = 5E6;

	// Initialisation de la table de transposition
	transposition_table.init(transposition_table_size, nullptr, true);
```

par :

```cpp
	// Taille de la table de transposition : dimensionnement adaptatif
	const int transposition_table_size = compute_pool_sizing().tt_length;

	// Initialisation de la table de transposition
	transposition_table.init(transposition_table_size, nullptr, true);
```

Vérifier que `main_gui.h` voit `compute_pool_sizing` : il inclut déjà la GUI ; si l'édition ne compile pas faute de déclaration, ajouter `#include "buffer.h"` en tête de `main_gui.h`.

- [ ] **Step 6 : Build**

Run : `msbuild opti_chess.sln /p:Configuration=Release /p:Platform=x64 /m /nologo`
Expected : build réussi.

- [ ] **Step 7 : Régression + RSS borné**

Lancer, appuyer sur `T` → `*** PERFT RESULTS: 2/2 ***`, EVALUATION identiques.
Vérifier les logs d'init buffers : `length` doit être largement < 1E7 sur une machine RAM modeste, et cohérent avec ~`0.5 × RAM dispo` plafonné à 4 Go.
Lancer une réflexion longue (5 min) + laisser en arrière-plan, surveiller le RSS au Gestionnaire des tâches.
Expected : RSS plafonné (≤ ~budget calculé), **ne croît plus indéfiniment**. Pas de gel système.

- [ ] **Step 8 : Commit**

```bash
git add opti_chess/buffer.h opti_chess/buffer.cpp opti_chess/gui.h opti_chess/gui.cpp opti_chess/main_gui.h
git commit -m "perf(buffer): dimensionnement memoire adaptatif depuis la RAM physique (#13)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Task 5: Buffer plein — reroutage propre dans GrogrosZero

**Files:**
- Modify: `opti_chess/exploration.h` (flag log extern)
- Modify: `opti_chess/exploration.cpp:184-198` (boucle `grogros_zero`), `:240,258,277` et `:926,943,960` (retrait `cout` spam), reset du flag

Aujourd'hui, buffer plein → `explore_new_move` fait `return;` mais la condition d'expansion reste vraie → busy-spin sur toutes les itérations + `cout` par tour. On reroute vers l'exploitation de l'arbre existant, arrêt propre si rien à raffiner, log unique.

- [ ] **Step 1 : Flag log (exploration.h)**

Dans `opti_chess/exploration.h`, après `extern NodeBuffer monte_node_buffer;` (fin du fichier), ajouter :

```cpp
// Log « buffer plein » une seule fois par session de saturation
extern bool g_buffers_full_logged;
```

- [ ] **Step 2 : Définition + reset du flag (exploration.cpp)**

Dans `opti_chess/exploration.cpp`, juste après `NodeBuffer monte_node_buffer;` (ligne 1600), ajouter :

```cpp
bool g_buffers_full_logged = false;
```

Puis, dans `NodeBuffer::reset()` et `BoardBuffer::reset()` (Task 2), ainsi que dans `NodeBuffer::remove()` / `BoardBuffer::remove()`, le flag doit repasser à `false` quand de la place se libère. Ajouter au **début** de `NodeBuffer::reset()` (avant `_bulk_resetting = true;`) et de `NodeBuffer::remove()` :

```cpp
	g_buffers_full_logged = false;
```

(Pour `BoardBuffer::reset()`/`remove()` dans `buffer.cpp`, ajouter `extern bool g_buffers_full_logged;` en tête de `buffer.cpp` puis la même ligne `g_buffers_full_logged = false;` au début de ces deux fonctions.)

- [ ] **Step 3 : Reroutage de la boucle `grogros_zero`**

Dans `opti_chess/exploration.cpp`, remplacer le bloc 186-195 :

```cpp
		// EXPLORATION D'UN NOUVEAU COUP
		if (get_fully_explored_children_count() < _board->_got_moves) {
			explore_new_move(board_buffer, eval, alpha, beta, gamma, quiescence_depth, network, &iteration_path_history);
		}

		// EXPLORATION D'UN COUP DÉJÀ EXPLORÉ
		else {
			explore_random_child(board_buffer, eval, alpha, beta, gamma, quiescence_depth, network, &iteration_path_history);
		}
```

par :

```cpp
		// Si les buffers sont pleins, on n'étend plus : on raffine l'arbre existant.
		const bool can_expand = !monte_board_buffer.is_full() && !monte_node_buffer.is_full();

		// EXPLORATION D'UN NOUVEAU COUP
		if (can_expand && get_fully_explored_children_count() < _board->_got_moves) {
			explore_new_move(board_buffer, eval, alpha, beta, gamma, quiescence_depth, network, &iteration_path_history);
		}

		// EXPLORATION D'UN COUP DÉJÀ EXPLORÉ (raffinage)
		else if (children_count() > 0) {
			explore_random_child(board_buffer, eval, alpha, beta, gamma, quiescence_depth, network, &iteration_path_history);
		}

		// Buffers pleins ET rien à raffiner ici : arrêt propre + log unique
		else {
			if (!g_buffers_full_logged) {
				cout << "buffer plein - arbre plafonne a " << _nodes
				     << " noeuds, on continue a raffiner l'existant" << endl;
				g_buffers_full_logged = true;
			}
			break;
		}
```

- [ ] **Step 4 : Retirer le spam `cout` (explore_new_move)**

Dans `opti_chess/exploration.cpp`, dans `explore_new_move`, retirer les 3 lignes `cout` des gardes buffer-plein (en gardant les `return;`). Remplacer :

```cpp
		if (new_board == nullptr) {
			cout << "null new board in explore_new_move" << endl;
			return;
		}
```
par :
```cpp
		if (new_board == nullptr)
			return;
```

Remplacer (les 2 occurrences) :
```cpp
			if (child == nullptr) {
				cout << "null child in explore_new_move" << endl;
				return;
			}
```
par :
```cpp
			if (child == nullptr)
				return;
```

- [ ] **Step 5 : Retirer le spam `cout` (quiescence)**

Dans `opti_chess/exploration.cpp`, quiescence, appliquer les 3 remplacements exacts ci-dessous (garder `_time_spent` + `return alpha;`, retirer la ligne `cout`).

Bloc 1 (~924-928), remplacer :
```cpp
				if (new_board == nullptr) {
					_time_spent += clock() - begin_monte_time;
					cout << "board buffer full during quiescence!" << endl;
					return alpha;
				}
```
par :
```cpp
				if (new_board == nullptr) {
					_time_spent += clock() - begin_monte_time;
					return alpha;
				}
```

Bloc 2 (~941-945), remplacer :
```cpp
						if (child == nullptr) {
							_time_spent += clock() - begin_monte_time;
							cout << "node buffer full during quiescence!" << endl;
							return alpha;
						}
```
par :
```cpp
						if (child == nullptr) {
							_time_spent += clock() - begin_monte_time;
							return alpha;
						}
```

Bloc 3 (~958-962), remplacer :
```cpp
						if (child == nullptr) {
							_time_spent += clock() - begin_monte_time;
							cout << "node buffer full during quiescence!" << endl;
							return alpha;
						}
```
par :
```cpp
						if (child == nullptr) {
							_time_spent += clock() - begin_monte_time;
							return alpha;
						}
```

- [ ] **Step 6 : Build**

Run : `msbuild opti_chess.sln /p:Configuration=Release /p:Platform=x64 /m /nologo`
Expected : build réussi.

- [ ] **Step 7 : Régression + comportement plein**

Lancer, appuyer sur `T` → `*** PERFT RESULTS: 2/2 ***`, EVALUATION identiques.
Pour forcer le plein : lancer temporairement avec un budget minuscule — dans `gui.cpp` (un seul site, à remettre après) appeler `compute_pool_sizing(0.0005)` ; lancer une réflexion longue.
Expected : un **seul** message « buffer plein - arbre plafonne a N noeuds … », CPU non saturé à vide, l'UI reste fluide, l'évaluation continue de s'affiner (les visites montent). Aucun spam console. Remettre `compute_pool_sizing()` (sans argument) après le test, rebuild.

- [ ] **Step 8 : Commit**

```bash
git add opti_chess/exploration.h opti_chess/exploration.cpp opti_chess/buffer.cpp
git commit -m "fix(search): buffer plein -> raffinage propre de l'arbre existant + log unique (#13)

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

## Task 6: Validation finale + clôture des bugs

**Files:**
- Modify: `opti_chess/BUGFIXES.md`

- [ ] **Step 1 : Régression complète**

Build propre, lancer, appuyer sur `T`.
Expected : `*** PERFT RESULTS: 2/2 ***` ET scores `EVALUATION TESTS` **identiques** à la baseline Task 0 (iso-comportement confirmé).

- [ ] **Step 2 : Mesures #12 / #13**

- Charger 5 FEN variées (dont une finale roi+pions) : chargement quasi-instantané (vs baseline lente). 
- Réflexion 10 min + 10 min en arrière-plan, RSS au Gestionnaire des tâches : reste ≤ budget calculé, stable. Aucun gel système, aucune latence d'input OS.

Expected : #12 et #13 non reproductibles.

- [ ] **Step 3 : Mettre à jour BUGFIXES.md**

Dans `opti_chess/BUGFIXES.md` : déplacer #2, #12, #13 de la section « ⬜ Ouverts » vers « ✅ Corrigés », statut `✅ corrigé — validé runtime`, en résumant : free-list O(1), `get_zobrist_key` par référence, budget mémoire adaptatif, reroutage buffer-plein. Mettre à jour « Ordre de traitement recommandé » (retirer #2/#12/#13, le prochain item devient `#4 → #3 → #14 → #11`).

- [ ] **Step 4 : Commit**

```bash
git add opti_chess/BUGFIXES.md
git commit -m "docs(bugfixes): #2/#12/#13 corriges et valides runtime

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

- [ ] **Step 5 : Récap final**

Vérifier l'historique :
```bash
git log --oneline fix/buffer-memory-perf -8
```
Expected : la suite des commits de Task 1→6. Proposer à l'utilisateur la finalisation (merge / PR) via la skill `superpowers:finishing-a-development-branch`.

---

## Notes de risque (rappel spec §5)

- **Double / oubli de libération** : garde `was_active && _buffer_index >= 0 && !_bulk_resetting` ⇒ un objet n'est repoussé qu'une fois, à la transition actif→libre, jamais pour les objets hors buffer (`_buffer_index == -1`), jamais pendant un reset global.
- **`<windows.h>` dans buffer.cpp** : sûr (pas de raylib dans cette TU) ; mécanisme déjà utilisé tel quel dans `windows_tests.cpp:82-85`.
- **Single-thread assumé** : free-list/`is_full()`/TT non thread-safe — choix assumé (spec §6) ; la future parallélisation devra les rendre thread-safe.
- **Pas de framework unitaire** : la régression repose sur perft 2/2 + iso-scores EVALUATION + mesures RSS/temps FEN (conforme spec §4).
