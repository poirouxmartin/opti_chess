#include "buffer.h"
#include "useful_functions.h"

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
	_init = true;

	if (display) {
		cout << "board buffer initialized" << endl;
		cout << "board size: " << int_to_round_string(sizeof(Board)) << "b" << endl;
		cout << "length: " << int_to_round_string(_length) << endl;
		cout << "approximate buffer size: " << long_int_to_round_string(monte_board_buffer._length * sizeof(Board)) << "b\n\n";
	}
}

// Fonction qui donne l'index du premier plateau de libre dans le buffer
int BoardBuffer::get_first_free_index() {
	for (int i = 0; i < _length; i++) {
		_iterator++;
		if (_iterator >= _length)
			_iterator -= _length;
		if (!_boards[_iterator]._is_active)
			return _iterator;
	}

	return -1;
}

// Fonction qui désalloue toute la mémoire
void BoardBuffer::remove() {
	delete[] _boards;
	_init = false;
	_length = 0;
	_iterator = -1;
}

// Fonction qui reset le buffer
bool BoardBuffer::reset()
{
	for (int i = 0; i < _length; i++)
		_boards[i].reset_board();

	return true;
}

// Fonction qui renvoie le premier noeud disponible dans le buffer
Board* BoardBuffer::get_first_free_board() {
	const int index = get_first_free_index();

	if (index == -1) {
		cout << "Board buffer is full!" << endl;
		return nullptr;
	}

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