#pragma once

#include "player.h"


// Match between two players
class Match {
	public:

		// Attributes

		// Player with the white pieces
		Player* _w_player;

		// Player with the black pieces
		Player* _b_player;

		// Initial position of the match
		string _initial_position;



		// Constructors

		// Constructor with the two players
		Match(Player* w_player, Player* b_player);


		// Methods

		// Play the match and returns the result of the match (1 if white wins, 0 if draw, -1 if black wins)
		int play(string initial_position = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", bool display = false) const;
};