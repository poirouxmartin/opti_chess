#pragma once
#include "board.h"

class Buffer {
public:

	bool _init = false;
	int _length = 0;
	Board* _heap_boards;

	// Itérateur pour rechercher moins longtemps un index de plateau libre
	int _iterator = -1;

	// Constructeur par défaut
	Buffer();

	// Constructeur utilisant la taille max (en bits) du buffer
	explicit Buffer(unsigned long int);

	// Initialize l'allocation de n plateaux
	void init(int length = 5000000, bool display = true);

	// Fonction qui donne l'index du premier plateau de libre dans le buffer
	int get_first_free_index();

	// Fonction qui désalloue toute la mémoire
	void remove();

	// Fonction qui reset le buffer
	[[nodiscard]] bool reset() const;
};

// Buffer pour monte-carlo
extern Buffer monte_buffer;