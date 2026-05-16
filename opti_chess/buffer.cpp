#include "buffer.h"
#include "useful_functions.h"

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