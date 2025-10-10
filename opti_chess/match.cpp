#include "match.h"
#include "board.h"

// Constructor with the two players
Match::Match(Player* w_player, Player* b_player) {
	_w_player = w_player;
	_b_player = b_player;
}

// Play the match and returns the result of the match (1 if white wins, 0 if draw, -1 if black wins)
// So far, it is a no-search match
int Match::play(string initial_position, bool display) const {

	if (display)
		cout << "Match !" << endl;

	// Create the board with the initial position
	Board board;
	board.from_fen(initial_position);

	while (board.is_game_over() == 0) {
		//cout << board.to_fen() << endl;
		Move move = board._player ? _w_player->best_move(&board) : _b_player->best_move(&board);

		if (display)
			cout << " " << board.move_label(move);

		board.make_move(move, false, true);

	}

	if (display)
		cout << "\nresult: " << (int)board._game_over_value << endl;

	return board._game_over_value;
}