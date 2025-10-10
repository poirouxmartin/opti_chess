#pragma once
#include "board.h"

class Buffer {
public:

	// Le buffer est-il initialis� ?
	bool _init = false;

	// Longueur du buffer
	int _length = 0;

	// Tableau de plateaux
	Board* _heap_boards;

	// It�rateur pour rechercher moins longtemps un index de plateau libre
	int _iterator = -1;

	// Constructeur par d�faut
	Buffer();

	// Constructeur utilisant la taille max (en bits) du buffer
	explicit Buffer(unsigned long int);

	// Initialize l'allocation de n plateaux
	void init(int length = 10000000, bool display = true);

	// Fonction qui donne l'index du premier plateau de libre dans le buffer
	int get_first_free_index();

	// Fonction qui d�salloue toute la m�moire
	void remove();

	// Fonction qui reset le buffer
	bool reset() const;
};

// Buffer pour monte-carlo
extern Buffer monte_buffer;