#pragma once
#include "board.h"
//#include "exploration.h"

class BoardBuffer {
public:

	// Le buffer est-il initialisé ?
	bool _init = false;

	// Longueur du buffer
	int _length = 0;

	// Tableau de plateaux
	Board* _heap_boards;

	// Itérateur pour rechercher moins longtemps un index de plateau libre
	int _iterator = -1;

	// Constructeur par défaut
	BoardBuffer();

	// Constructeur utilisant la taille max (en bits) du buffer
	explicit BoardBuffer(unsigned long int);

	// Initialize l'allocation de n plateaux
	void init(int length = 5000000, bool display = true);

	// Fonction qui donne l'index du premier plateau de libre dans le buffer
	int get_first_free_index();

	// Fonction qui désalloue toute la mémoire
	void remove();

	// Fonction qui reset le buffer
	bool reset() const;
};

// Buffer pour monte-carlo
extern BoardBuffer monte_board_buffer;

//class NodeBuffer {
//public:
//
//	// Le buffer est-il initialisé ?
//	bool _init = false;
//
//	// Longueur du buffer
//	int _length = 0;
//
//	// Tableau de plateaux
//	Node* _heap_boards;
//
//	// Itérateur pour rechercher moins longtemps un index de plateau libre
//	int _iterator = -1;
//
//	// Constructeur par défaut
//	NodeBuffer();
//
//	// Constructeur utilisant la taille max (en bits) du buffer
//	explicit NodeBuffer(unsigned long int);
//
//	// Initialize l'allocation de n plateaux
//	void init(int length = 10000000, bool display = true);
//
//	// Fonction qui donne l'index du premier plateau de libre dans le buffer
//	int get_first_free_index();
//
//	// Fonction qui désalloue toute la mémoire
//	void remove();
//
//	// Fonction qui reset le buffer
//	bool reset() const;
//};
//
//extern NodeBuffer monte_node_buffer;