#include "buffer.h"
#include "useful_functions.h"

// Constructeur par défaut
Buffer::Buffer() {
	// Crée un gros buffer, de 4GB
	constexpr unsigned long int _size_buffer = 4000000000;
	_length = _size_buffer / sizeof(Board);
	_length = 0;

	_heap_boards = new Board[_length];
}

// Constructeur utilisant la taille max (en bits) du buffer
Buffer::Buffer(const unsigned long int size) {
	_length = size / sizeof(Board);
	_heap_boards = new Board[_length];
}

// Initialize l'allocation de n plateaux
void Buffer::init(const int length) {
	if (_init)
		cout << "already initialized" << endl;
	else {
		cout << "initializing buffer..." << endl;
		_length = length;
		_heap_boards = new Board[_length];
		_init = true;
		cout << "buffer initialized :" << endl;
		cout << "board size : " << int_to_round_string(sizeof(Board)) << "b" << endl;
		cout << "length : " << int_to_round_string(_length) << endl;
		cout << "approximate buffer size : " << long_int_to_round_string(monte_buffer._length * sizeof(Board)) << "b" << endl;
	}
}

// Fonction qui donne l'index du premier plateau de libre dans le buffer
int Buffer::get_first_free_index() {
	for (int i = 0; i < _length; i++) {
		_iterator++;
		if (_iterator >= _length)
			_iterator -= _length;
		if (!_heap_boards[_iterator]._is_active)
			return _iterator;
	}

	return -1;
}

// Fonction qui désalloue toute la mémoire
void Buffer::remove() {
	delete[] _heap_boards;
	_init = false;
}

// Buffer pour l'algo de Monte-Carlo
Buffer monte_buffer;