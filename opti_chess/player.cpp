#include "player.h"

// Default constructor
Player::Player() {
	_network = new Network();
	_network->generate_random_weights();
}

// Constructor with a network
Player::Player(Network* network) {
	_network = network;
}

// Return the best move for the given position
Move Player::best_move(Board* board) const {
	// TODO : improve using an algorithm? for now: no search

	// For now, evaluates all possible moves and returns the best one

	int best_score = -INT_MAX;
	Move best_move{};

	if (board->_got_moves == -1)
		board->get_moves();

	//cout << "toto" << endl;

	for (int i = 0; i < board->_got_moves; i++) {
		Board b(*board, false, true);
		b.make_move(board->_moves[i], false, false, true);
		// FIXME: comment évaluer un plateau avec les réseaux de neurones?
		//b.evaluate(nullptr, false, _network, false); // We don't look for checkmates, because we want it to learn to play such moves by itself
		_network->input_from_fen(b.to_fen());
		_network->calculate_output();

		int colored_evaluation = _network->_output * b.get_color();
		//cout << colored_evaluation << endl;

		if (colored_evaluation > best_score) {
			best_score = colored_evaluation;
			best_move = board->_moves[i];
		}
	}

	return best_move;
}