#pragma once

#include "neural_network.h"
#include "board.h"

// TO ADD
// - algorithm (and parameters to use)
// - time management function
// - evaluator instead of network?
// - default elo value

// Default elo value
#define DEFAULT_ELO 1000

class Player {
	public:

		// Attributes

		// Neural network to evaluate positions
		Network* _network = nullptr;

		// Elo of the player
		int _elo = DEFAULT_ELO;

		// Number of matches played (used to compute the elo)
		int _n_matches = 0;


		// Constructors

		// Default constructor
		Player();

		// Constructor with a network
		Player(Network* network);


		// Methods

		// Return the best move for the given position
		Move best_move(Board* board) const;

};