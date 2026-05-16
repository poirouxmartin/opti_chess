#include "buffer.h"
#include "useful_functions.h"
#include "exploration.h"     // sizeof(Node)
#include "zobrist.h"         // sizeof(ZobristEntry)
#include "windows_tests.h"   // get_available_physical_memory() — probe RAM isolée
                             // (pas de <windows.h> ici : board.h tire raylib.h)

// Défini dans exploration.cpp : log « buffer plein » une seule fois.
// Remis à false ici dès qu'un reset/remove de BoardBuffer libère de la place.
extern bool g_buffers_full_logged;

// Constructeur par défaut : n'alloue rien, init() est obligatoire
BoardBuffer::BoardBuffer() {
	_boards = nullptr;
	_length = 0;
}

// Constructeur taille (octets) : alloue immédiatement
BoardBuffer::BoardBuffer(const size_t size_bytes) {
	init(static_cast<int>(size_bytes / sizeof(Board)), false);
}

// Initialize l'allocation de n plateaux
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

// Dépile un index libre — O(1). Pile vide => -1 (buffer plein)
int BoardBuffer::get_first_free_index() {
	if (_free_indices.empty())
		return -1;
	const int index = _free_indices.back();
	_free_indices.pop_back();
	return index;
}

// Fonction qui désalloue toute la mémoire
void BoardBuffer::remove() {
	g_buffers_full_logged = false;
	delete[] _boards;
	_boards = nullptr;
	_init = false;
	_length = 0;
	_iterator = -1;
	_free_indices.clear();
}

// Reset global du buffer : reconstruit uniquement la pile d'indices libres.
// #12: NE PAS rebalayer _length avec reset_board() (chaque appel clearait un
// robin_map => coût O(capacité) qui rendait load_FEN lent/infini). Le contenu
// objet est réinitialisé paresseusement à la réutilisation. Sans appelant
// depuis le fix #12 (reset_buffers ne l'appelle plus) — gardé cohérent.
bool BoardBuffer::reset() {
	g_buffers_full_logged = false;
	_free_indices.clear();
	_free_indices.reserve(_length);
	for (int i = _length - 1; i >= 0; i--)
		_free_indices.push_back(i);

	return true;
}

// Fonction qui renvoie le premier plateau disponible dans le buffer
Board* BoardBuffer::get_first_free_board() {
	const int index = get_first_free_index();
	if (index == -1)
		return nullptr;

	Board* board = &_boards[index];
	board->_is_active = true;
	return board;
}

// DEBUG *** fonction qui affiche l'état du buffer (combien de plateaux sont utilisés)
void BoardBuffer::display_buffer_state() const {
	int used_boards = 0;
	for (int i = 0; i < _length; i++) {
		if (_boards[i]._is_active)
			used_boards++;

		// Affiche le plateau en question
		cout << "Board " << i << ", active: " << (_boards[i]._is_active ? "yes" : "no") << endl;
		_boards[i].display();
	}
	cout << "Board buffer state: " << used_boards << " / " << _length << " boards used (" << (used_boards * 100.0 / _length) << "%)" << endl;
}

// Buffer pour l'algo de Monte-Carlo
BoardBuffer monte_board_buffer;

// Dimensionnement adaptatif des pools depuis la RAM physique disponible.
// budget = min(ram_fraction × RAM dispo, hard_cap) ; TT plafonnée à budget/4 ;
// reste réparti à parts égales en NOMBRE d'entrées Node et Board.
// Tout le calcul en unsigned long long (portable Win32/x64).
PoolSizing compute_pool_sizing(double ram_fraction, unsigned long long hard_cap_bytes, int tt_max_entries, double rss_overhead_factor) {
	const unsigned long long avail = get_available_physical_memory();

	// Budget = cible de RSS TOTAL du process.
	unsigned long long budget = (unsigned long long)((double)avail * ram_fraction);
	if (budget > hard_cap_bytes)
		budget = hard_cap_bytes;

	// Le RSS réel ≈ rss_overhead_factor × (tableaux plats + TT plate), car
	// les robin_map _children/_positions_history/TT grossissent en heap hors
	// sizeof. On alloue donc les structures plates à budget/facteur : le RSS
	// total (plat + heap dynamique) converge vers `budget`. Borné sur toute
	// machine, pas seulement les tableaux (vrai correctif #13).
	if (rss_overhead_factor < 1.0)
		rss_overhead_factor = 1.0;
	budget = (unsigned long long)((double)budget / rss_overhead_factor);

	// Coût approché d'une entrée robin_map<uint64_t, ZobristEntry> (overhead ~2x)
	const unsigned long long tt_entry_bytes = (unsigned long long)(sizeof(uint64_t) + sizeof(ZobristEntry)) * 2;
	unsigned long long tt_bytes = (unsigned long long)tt_max_entries * tt_entry_bytes;
	if (tt_bytes > budget / 4)
		tt_bytes = budget / 4;
	const int tt_length = (int)(tt_bytes / tt_entry_bytes);

	// Reste réparti : autant d'entrées Node que Board (un Board ≈ un Node expansé)
	const unsigned long long rest = budget - tt_bytes;
	const unsigned long long pair_bytes = (unsigned long long)(sizeof(Board) + sizeof(Node));
	const int count = (int)(rest / pair_bytes);

	PoolSizing ps;
	ps.board_length = count;
	ps.node_length = count;
	ps.tt_length = tt_length;
	return ps;
}