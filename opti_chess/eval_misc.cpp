#include "board.h"
#include "gui.h"
#include "useful_functions.h"
#include "zobrist.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <sstream>
#include <cmath>
#include <utility>
#include <iomanip>
#include <vector>
#include "board.h"
#include "gui.h"
#include "useful_functions.h"
#include "zobrist.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <sstream>
#include <cmath>
#include <utility>
#include <iomanip>
#include <vector>
float Board::get_attacks_and_defenses() const
{
	// TODO: revisit, to model real threats more faithfully
	// FIXME: penalty for unprotected pieces?

	// Attack values by piece (0 = pawn, 1 = knight, 2 = bishop, 3 = rook, 4 = queen, 5 = king)
	static constexpr int attacks_array[6][6] = {
	//   P    N    B     R    Q    K
		{50, 130, 135, 150, 200, 100}, // P
		{40,  60,  70, 150, 175, 100}, // N
		{40,  50,  60, 125, 160, 100}, // B
		{35,  40,  50,  50, 135, 100}, // R
		{25,  30,  35,  45,  50,  50}, // Q
		{15,  10,  10,  15,   0,   0}, // K
	};

	// Defence values by piece (0 = pawn, 1 = knight, 2 = bishop, 3 = rook, 4 = queen, 5 = king)
	static constexpr int defenses_array[6][6] = {
	//   P    N    B     R    Q    K
		{100, 45,  45,  35,  30,   0}, // P
		{25,  30,  30,  30,  30,   0}, // N
		{25,  30,  30,  30,  30,   0}, // B
		{20,  20,  20,  35,  25,   0}, // R
		{10,   5,   5,   5,   5,   0}, // Q
		{10,   5,   5,   5,   5,   0}, // K
	};

	// TODO: stop summing the defence of every piece? look only at undefended ones, otherwise the engine turns timid
	// FIXME: x-ray attacks and defences are not accounted for

	// Attack table for White
	int attacks_white[8][8] = { 0 };

	// Attack table for Black
	int attacks_black[8][8] = { 0 };


	// Defence table for White
	int defenses_white[8][8] = { 0 };

	// Defence table for Black
	int defenses_black[8][8] = { 0 };


	// TODO: use constants for the redundant computations


	// TODO: replace the ifs with &&

	uint64_t occ = _occupancies[2];
	while (occ) {
		const int sq = pop_lsb(occ);
		const int row = sq >> 3;
		const int col = sq & 7;

		// Piece on the square
		const uint8_t p = _array[row][col];

		switch (p) {
			// White pawn
			case w_pawn:
					if (col > 0) {
						// Up-left square of the white pawn
						uint8_t new_row = row + 1;
						uint8_t new_col = col - 1;
						uint8_t p2 = _array[new_row][new_col];

						if (is_black(p2))
							attacks_white[new_row][new_col] += attacks_array[0][p2 - 7];
						else if (is_white(p2))
							defenses_white[new_row][new_col] += defenses_array[0][p2 - 1];
					}
					if (col < 7) {
						// Up-right square of the white pawn
						uint8_t new_row = row + 1;
						uint8_t new_col = col + 1;
						uint8_t p2 = _array[new_row][new_col];

						if (is_black(p2))
							attacks_white[new_row][new_col] += attacks_array[0][p2 - 7];
						else if (is_white(p2))
							defenses_white[new_row][new_col] += defenses_array[0][p2 - 1];
					}

					break;

				// White knight
				case w_knight:
					for (int k = 0; k < 8; k++) {
						uint8_t new_row = row + knight_directions[k][0];
						uint8_t new_col = col + knight_directions[k][1];

						if (is_in_fast(new_row, 0, 7) && is_in_fast(new_col, 0, 7)) {
							uint8_t p2 = _array[new_row][new_col];

							if (is_black(p2))
								attacks_white[new_row][new_col] += attacks_array[1][p2 - 7];
							else if (is_white(p2))
								defenses_white[new_row][new_col] += defenses_array[1][p2 - 1];
						}
					}

					break;

				// White bishop
				case w_bishop:
					for (uint8_t k = 0; k < 4; k++) {
						const uint8_t d_row = diag_directions[k][0];
						const uint8_t d_col = diag_directions[k][1];
						uint8_t new_row = row + d_row;
						uint8_t new_col = col + d_col;

						while (new_row >= 0 && new_row < 8 && new_col >= 0 && new_col < 8) {
							const uint8_t p2 = _array[new_row][new_col];

							if (is_black(p2))
								attacks_white[new_row][new_col] += attacks_array[2][p2 - 7];
							else if (is_white(p2))
								defenses_white[new_row][new_col] += defenses_array[2][p2 - 1];

							// Stop on any piece
							if (p2 != none)
								break;

							new_row += d_row;
							new_col += d_col;
						}
					}

					break;

				// White rook
				case w_rook:
					for (uint8_t k = 0; k < 4; k++) {
						const uint8_t d_row = rect_directions[k][0];
						const uint8_t d_col = rect_directions[k][1];
						uint8_t new_row = row + d_row;
						uint8_t new_col = col + d_col;

						while (new_row >= 0 && new_row < 8 && new_col >= 0 && new_col < 8) {
							const uint8_t p2 = _array[new_row][new_col];

							if (is_black(p2))
								attacks_white[new_row][new_col] += attacks_array[3][p2 - 7];
							else if (is_white(p2))
								defenses_white[new_row][new_col] += defenses_array[3][p2 - 1];

							// Stop on any piece
							if (p2 != none)
								break;

							new_row += d_row;
							new_col += d_col;
						}
					}
					break;

				// White queen
				case w_queen:
					for (uint8_t k = 0; k < 8; k++) {
						const uint8_t d_row = all_directions[k][0];
						const uint8_t d_col = all_directions[k][1];
						uint8_t new_row = row + d_row;
						uint8_t new_col = col + d_col;

						while (new_row >= 0 && new_row < 8 && new_col >= 0 && new_col < 8) {
							const uint8_t p2 = _array[new_row][new_col];

							if (is_black(p2))
								attacks_white[new_row][new_col] += attacks_array[4][p2 - 7];
							else if (is_white(p2))
								defenses_white[new_row][new_col] += defenses_array[4][p2 - 1];

							// Stop on any piece
							if (p2 != none)
								break;

							new_row += d_row;
							new_col += d_col;
						}
					}

					break;

				// White king
				case w_king:
					for (uint8_t k = 0; k < 8; k++) {
						uint8_t new_row = row + all_directions[k][0];
						uint8_t new_col = col + all_directions[k][1];

						if (is_in_fast(new_row, 0, 7) && is_in_fast(new_col, 0, 7)) {
							uint8_t p2 = _array[new_row][new_col];

							if (is_black(p2))
								attacks_white[new_row][new_col] += attacks_array[5][p2 - 7];
							else if (is_white(p2))
								defenses_white[new_row][new_col] += defenses_array[5][p2 - 1];
						}
					}

					break;

				// Black pawn
				case b_pawn:
					if (col > 0) {
						// Down-left square of the black pawn
						uint8_t new_row = row - 1;
						uint8_t new_col = col - 1;
						uint8_t p2 = _array[new_row][new_col];

						if (is_white(p2))
							attacks_black[new_row][new_col] += attacks_array[0][p2 - 1];
						else if (is_black(p2))
							defenses_black[new_row][new_col] += defenses_array[0][p2 - 7];
					}
					if (col < 7) {
						// Down-right square of the black pawn
						uint8_t new_row = row - 1;
						uint8_t new_col = col + 1;
						uint8_t p2 = _array[new_row][new_col];

						if (is_white(p2))
							attacks_black[new_row][new_col] += attacks_array[0][p2 - 1];
						else if (is_black(p2))
							defenses_black[new_row][new_col] += defenses_array[0][p2 - 7];
					}

					break;
			
				// Black knight
				case b_knight:
					for (int k = 0; k < 8; k++) {
						uint8_t new_row = row + knight_directions[k][0];
						uint8_t new_col = col + knight_directions[k][1];

						if (is_in_fast(new_row, 0, 7) && is_in_fast(new_col, 0, 7)) {
							uint8_t p2 = _array[new_row][new_col];

							if (is_white(p2))
								attacks_black[new_row][new_col] += attacks_array[1][p2 - 1];
							else if (is_black(p2))
								defenses_black[new_row][new_col] += defenses_array[1][p2 - 7];
						}
					}

					break;

				// Black bishop
				case b_bishop:
					for (uint8_t k = 0; k < 4; k++) {
						const uint8_t d_row = diag_directions[k][0];
						const uint8_t d_col = diag_directions[k][1];
						uint8_t new_row = row + d_row;
						uint8_t new_col = col + d_col;

						while (new_row >= 0 && new_row < 8 && new_col >= 0 && new_col < 8) {
							const uint8_t p2 = _array[new_row][new_col];

							if (is_white(p2))
								attacks_black[new_row][new_col] += attacks_array[2][p2 - 1];
							else if (is_black(p2))
								defenses_black[new_row][new_col] += defenses_array[2][p2 - 7];

							// Stop on any piece
							if (p2 != none)
								break;

							new_row += d_row;
							new_col += d_col;
						}
					}

					break;

				// Black rook
				case b_rook:
					for (uint8_t k = 0; k < 4; k++) {
						const uint8_t d_row = rect_directions[k][0];
						const uint8_t d_col = rect_directions[k][1];
						uint8_t new_row = row + d_row;
						uint8_t new_col = col + d_col;

						while (new_row >= 0 && new_row < 8 && new_col >= 0 && new_col < 8) {
							const uint8_t p2 = _array[new_row][new_col];

							if (is_white(p2))
								attacks_black[new_row][new_col] += attacks_array[3][p2 - 1];
							else if (is_black(p2))
								defenses_black[new_row][new_col] += defenses_array[3][p2 - 7];

							// Stop on any piece
							if (p2 != none)
								break;

							new_row += d_row;
							new_col += d_col;
						}
					}

					break;

				// Black queen
				case b_queen:
					for (uint8_t k = 0; k < 8; k++) {
						const uint8_t d_row = all_directions[k][0];
						const uint8_t d_col = all_directions[k][1];
						uint8_t new_row = row + d_row;
						uint8_t new_col = col + d_col;

						while (new_row >= 0 && new_row < 8 && new_col >= 0 && new_col < 8) {
							const uint8_t p2 = _array[new_row][new_col];

							if (is_white(p2))
								attacks_black[new_row][new_col] += attacks_array[4][p2 - 1];
							else if (is_black(p2))
								defenses_black[new_row][new_col] += defenses_array[4][p2 - 7];

							// Stop on any piece
							if (p2 != none)
								break;

							new_row += d_row;
							new_col += d_col;
						}
					}

					break;

				// Black king
				case b_king:
					for (uint8_t k = 0; k < 8; k++) {
						uint8_t new_row = row + all_directions[k][0];
						uint8_t new_col = col + all_directions[k][1];

						if (is_in_fast(new_row, 0, 7) && is_in_fast(new_col, 0, 7)) {
							uint8_t p2 = _array[new_row][new_col];

							if (is_white(p2))
								attacks_black[new_row][new_col] += attacks_array[5][p2 - 1];
							else if (is_black(p2))
								defenses_black[new_row][new_col] += defenses_array[5][p2 - 7];
						}
					}

					break;
		}
	}


	// Sum every positive value of the attack tables, for each side
	int white_attacks_eval = 0;
	int black_attacks_eval = 0;

	// Maximum defence value
	constexpr int max_defense = 10;

	// Penalty for undefended pieces (pawns, knights, bishops, rooks, queens, kings)
	//int undefended_malus[6] = { 0, 20, 8, 10, 5, 0 };
	constexpr int undefended_malus[6] = { 0, 0, 0, 0, 0, 0 };

	// TESTS
	//rnbqkbnr/ppp2ppp/4p3/3p4/3P4/2N5/PPP1PPPP/R1BQKBNR w KQkq - 0 3
	//4r2k/2q3bp/R4pp1/1R3p2/3P4/1PP2N1n/1P1Q1P1P/7K w - - 1 30

	uint64_t occ2 = _occupancies[2];
	while (occ2) {
		const int sq = pop_lsb(occ2);
		const uint8_t row = sq >> 3;
		const uint8_t col = sq & 7;
		const uint8_t p = _array[row][col];

		if (is_white(p)) {
			int black_attack = attacks_black[row][col] - defenses_white[row][col] + (defenses_white[row][col] == 0 ? undefended_malus[p - 1] : 0);

			if (black_attack < 0) {
				float division_factor = 1.0f - static_cast<float>(black_attack) / max_defense;
				black_attack = static_cast<int>(black_attack / division_factor);
			}

			black_attacks_eval += black_attack;
		}
		else {
			int white_attack = attacks_white[row][col] - defenses_black[row][col] + (defenses_black[row][col] == 0 ? undefended_malus[p - 7] : 0);

			if (white_attack < 0) {
				float division_factor = 1.0f - static_cast<float>(white_attack) / max_defense;
				white_attack = static_cast<int>(white_attack / division_factor);
			}

			white_attacks_eval += white_attack;
		}
	}

	// r6r/p1kb2p1/1p2p3/3pP3/2p2P2/2P1R1P1/PP1N2P1/R5K1 b - - 0 20: Black attacks nothing yet still gets a bonus?

	// 4R3/p4pkp/2p2pb1/qp1p4/2nP2PN/PRPB3P/2P2P2/6K1 b - - 2 21

	// Based on game progress
	constexpr float advancement_factor = 0.5f;

	//cout << "\nWhite attacks: " << white_attacks_eval << " | Black attacks: " << black_attacks_eval << endl;

	return eval_from_progress(white_attacks_eval - black_attacks_eval, _adv, advancement_factor);
}

// Computes the king opposition, in pawn endgames
int Board::get_sliders_on_open_file() const
{
	// TEST: 5rk1/6p1/p2qp2p/1b1p4/3P4/1P2BP2/3Q2PP/2R3K1 w - - 0 28

	// Bonus
	constexpr int rook_open_bonus = 50;
	constexpr int rook_semi_open_bonus = 30;
	constexpr int queen_open_bonus = 30;
	constexpr int queen_semi_open_bonus = 20;

	int w_open_value = 0;
	int b_open_value = 0;

	int w_semi_open_value = 0;
	int b_semi_open_value = 0;

	// Pawns, rooks and queens per file
	uint8_t w_pawns[8] = { 0 };
	uint8_t b_pawns[8] = { 0 };
	uint8_t w_rooks[8] = { 0 };
	uint8_t b_rooks[8] = { 0 };
	uint8_t w_queens[8] = { 0 };
	uint8_t b_queens[8] = { 0 };


	// Count the pawns, rooks and queens on each file
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			const uint8_t p = _array[row][col];

			if (p == w_pawn) {
				w_pawns[col]++;
			}
			else if (p == w_rook) {
				w_rooks[col]++;
			}
			else if (p == w_queen) {
				w_queens[col]++;
			}
			else if (p == b_pawn) {
				b_pawns[col]++;
			}
			else if (p == b_rook) {
				b_rooks[col]++;
			}
			else if (p == b_queen) {
				b_queens[col]++;
			}
		}
	}

	// Open files
	uint8_t open_files = 0;

	for (uint8_t col = 0; col < 8; col++) {
		bool is_open = !w_pawns[col] && !b_pawns[col];

		// If the file is open
		if (is_open) {
			open_files++;

			w_open_value += w_rooks[col] * rook_open_bonus;
			b_open_value += b_rooks[col] * rook_open_bonus;

			w_open_value += w_queens[col] * queen_open_bonus;
			b_open_value += b_queens[col] * queen_open_bonus;
		}
	}

	// Semi-open files
	uint8_t w_semi_open_files = 0;
	uint8_t b_semi_open_files = 0;

	for (uint8_t col = 0; col < 8; col++)
	{
		bool w_semi_open = !w_pawns[col] && b_pawns[col];
		bool b_semi_open = w_pawns[col] && !b_pawns[col];

		// If the file is semi-open for White
		if (w_semi_open) {
			w_semi_open_files++;
			w_semi_open_value += w_rooks[col] * rook_semi_open_bonus;
			w_semi_open_value += w_queens[col] * queen_semi_open_bonus;
		}

		// If the file is semi-open for Black
		if (b_semi_open) {
			b_semi_open_files++;
			b_semi_open_value += b_rooks[col] * rook_semi_open_bonus;
			b_semi_open_value += b_queens[col] * queen_semi_open_bonus;
		}
	}

	//cout << "Open files: " << (int)open_files << " | Semi-open files: " << (int)w_semi_open_files << " / " << (int)b_semi_open_files << endl;
	//cout << "W open value: " << w_open_value << " | B open value: " << b_open_value << endl;
	//cout << "W semi-open value: " << w_semi_open_value << " | B semi-open value: " << b_semi_open_value << endl;

	// It matters less when several files are open
	w_open_value = w_open_value == 0 ? 0 : w_open_value / open_files;
	b_open_value = b_open_value == 0 ? 0 : b_open_value / open_files;

	// It matters less when several files are semi-open
	w_semi_open_value = w_semi_open_value == 0 ? 0 : w_semi_open_value / (w_semi_open_files + open_files * 2);
	b_semi_open_value = b_semi_open_value == 0 ? 0 : b_semi_open_value / (b_semi_open_files + open_files * 2);

	//cout << "W open value: " << w_open_value << " | B open value: " << b_open_value << endl;
	//cout << "W semi-open value: " << w_semi_open_value << " | B semi-open value: " << b_semi_open_value << endl;

	// Total value
	return w_open_value - b_open_value + w_semi_open_value - b_semi_open_value;
}

// Computes the value of the controlled squares on the board
int Board::get_fianchetto_value() const
{
	int_fast8_t fianchetti = 0;

	// r1bqk2r/ppp2ppp/2nbpn2/8/2pP4/5NP1/PP1NPPBP/R1BQ1RK1 b kq - 1 7: this should rise after b5

	// r1bq1rk1/pppnbppp/4p3/3pP3/3P4/6P1/PPP1P1BP/RNBQ1RK1 w - - 1 9 vs r1bq1rk1/pppnbppp/4p3/3pP3/3P4/5BP1/PPP1P2P/RNBQ1RK1 w - - 2 9 vs r1bq1rk1/pppnbppp/4p3/3pP3/3P4/6P1/PPP1P2P/RNBQ1RKB w - - 3 9

	// Value of a "normal" fianchetto: at least 3 reachable squares on the diagonal
	constexpr int base_fianchetto_squares = 1;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {

			// Piece
			const uint8_t piece = _array[row][col];

			// If this is not a bishop
			if (!is_bishop(piece))
				continue;

			// If the bishop is off the long diagonal
			if (row != col && row + col != 7)
				continue;

			int current_row = row, current_col = col;

			// Assume the bishop is not in the centre of the board
			if (min(current_row, 7 - current_row) > 2)
				continue;

			// Which of the two diagonals is the bishop on?
			bool diagonal = row == col;

			// Direction de la diagonale
			int d_row = 1;
			int d_col = diagonal ? 1 : -1;

			bool piece_color = piece < 7;

			// Remove the base value of the fianchetto
			piece_color ? fianchetti -= base_fianchetto_squares : fianchetti += base_fianchetto_squares;

			// Count the squares along the diagonal, stopping at the first pawn

			// Upwards
			for (int k = current_row + d_row; k < 8; k += d_row) {
				current_row = k;
				current_col += d_col;
				
				// Stop on a pawn
				if (_array[current_row][current_col] % 6 == w_pawn)
					break;

				//cout << current_row << ", " << current_col << ": " << square_name(current_row, current_col) << " ";

				piece_color ? fianchetti++ : fianchetti--;
			}

			// Downwards
			current_row = row;
			current_col = col;

			for (int k = current_row - d_row; k >= 0; k -= d_row) {
				current_row = k;
				current_col -= d_col;

				// Stop on a pawn
				if (_array[current_row][current_col] % 6 == w_pawn)
					break;

				//cout << current_row << ", " << current_col << ": " << square_name(current_row, current_col) << " ";

				piece_color ? fianchetti++ : fianchetti--;
			}
		}
	}

	return fianchetti * 25.0f * (1.0f - _adv);
}

// Tells whether the square is controlled by a side -> by the opponent? to be confirmed
int Board::get_weak_squares(bool color, bool around_king) {
	// Weak square: one no pawn can protect any more (no pawns on a lower rank of the adjacent files), when no pawn stands on it
	// Bonus for enemy pawn control over the weak square
	// Bonus for the outpost of a knight, a bishop or a rook

	// TODO: scale this by the pieces able to occupy it
	// Try a bonus based on a piece's distance to the square?

	// r1bq2rk/2n4p/3p4/pNpPnp2/P1P1pN2/2Q5/4BPPP/1R3RK1 w - - 1 25: can e5 count as a weak square for White here?
	// rnbqkb1r/ppp2ppp/4pn2/3p4/3P4/4PN2/PPP2PPP/RNBQKB1R w KQkq - 0 4: h4 adds a lot to weak_squares; why?
	// 2r1brk1/6bp/p2pPpp1/qppN4/8/1P3NP1/P1Q2PP1/4RBK1 b - - 1 25 : grosse case faible en d5!!
	// r1bqkbnr/pppp1ppp/8/4p3/2PnP3/7P/PP1P1PP1/RNBQKBNR w KQkq - 1 4 : d4
	// r2qkb1r/pp1b1ppp/4p3/1PPp4/3QnP2/4P3/P1P3PP/RNB1KB1R w KQkq - 1 12 : e4 gratos...
	// 2r1brk1/6bp/p2pPpp1/qppN4/8/1P3NP1/P1Q2PP1/4RBK1 b - - 1 25 vs 2r1brk1/6bp/p2p1pp1/qppN4/8/1P3NP1/P1Q2PP1/4RBK1 b - - 1 25
	// 2r5/pp6/3pkp2/2p1p1p1/2P3Pp/3PPP1P/PP2K3/2R5 w - - 0 1: many weak squares, none of them usable
	// r1bq1rk1/pp1n1ppp/2pbpn2/3p4/3P4/2NBPN2/PPP2PPP/R1BQ1RK1 w - - 4 9 : pas vraiment de case faible...

	// r1bqkbnr/5ppp/p1np4/4p3/1p2P3/N1N5/PPP1BPPP/R1BQK2R w KQkq - 0 9: after Nd5, then after bxa3?

	//https://chessmood.com/blog/weak-squares-in-chess

	bool display = false;

	// Weak square value, from the point of view of the side that owns them
	constexpr static int weak_square_values[8][8] = {
		{ 0,  0,  0,  0,  0,  0,  0,  0},
		{ 0,  0,  5, 10, 10,  5,  0,  0}, 
		{ 0,  5, 10, 20, 20, 10,  5,  0},
		{ 5, 20, 35, 50, 50, 35, 20,  5},
		{ 5, 20, 40, 60, 60, 40, 20,  5},
		{ 5, 10, 15, 25, 25, 15, 10,  5},
		{ 0,  0,  0,  0,  0,  0,  0,  0},
		{ 0,  0,  0,  0,  0,  0,  0,  0}
	};

	// Outpost value on the weak squares, from the point of view of the side that owns them
	constexpr static int outpost_square_values[8][8] = {
		{ 0,  0,   0,  0,  0,  0,  0,  0 },
		{ 0,  0,   0,  0,  0,  0,  0,  0 },
		{ 0,  0,   5, 20, 20,  5,  0,  0 },
		{ 0,  10, 30, 45, 45, 30, 10,  0 },
		{ 0,  15, 45, 50, 50, 45, 15,  0 },
		{ 0,  25, 30, 40, 40, 30, 25,  0 },
		{ 0,  5,  15, 25, 25, 15,  5,  0 },
		{ 0,  0,  0,   0,  0,  0,  0,  0 }
	};

	// Value of the weak squares near the king
	constexpr static int king_weak_square[3] = { 20, 18, 8 };

	// Outpost value near the king, decreasing with distance
	constexpr static int king_outpost_square[3] = { 35, 30, 15 };

	// Outpost values (pawn, knight, bishop, rook, queen, king)
	constexpr static float outpost_values[6] = { 0.0f, 1.45f, 0.65f, 0.5f, 0.35f, 0.24f };

	// When the square is merely "safe" rather than weak as such
	constexpr static float safe_square_bonus = 0.3f;

	// Bonus when there is an enemy pawn right in front
	constexpr static float blocked_pawn_bonus = 1.55f;


	// Weak square value
	int weak_squares_value = 0;

	// Square controls
	SquareMap white_controls = get_white_controls_map();
	SquareMap black_controls = get_black_controls_map();


	if (around_king) {
		update_kings_pos();
	}

	// King position
	Pos king_pos = color ? _white_king_pos : _black_king_pos;

	// White's weak squares
	if (color) {

		if (display)
			cout << endl << "White weak squares:" << endl;

		// For each square
		for (uint8_t row = 1; row < 7; row++) {
			for (uint8_t col = 0; col < 8; col++) {

				if (_array[row][col] == w_pawn || _array[row][col] == b_pawn)
					continue;

				// No pawn on the square, so for now assume it is a weak and safe square
				bool weak = true;
				bool safe = true;

				// Unless this is the left edge
				if (col > 0) {

					// Look at the rank of the nearest white pawn able to control the square
					int can_control = -1;
					bool blocking_pawn = false;

					for (int_fast8_t k = row - 1; k > 0; k--) {
						if (_array[k][col - 1] == w_pawn) {
							weak = false;
							if (!blocking_pawn) {
								can_control = k;
								break;
							}
						}

						// If a black pawn blocks the white pawn
						if (_array[k][col - 1] == b_pawn) {
							blocking_pawn = true;
						}
					}

					// Can a black pawn stop the white pawn from controlling the weak square?
					bool no_control = false;

					if (can_control != -1) {
						for (int_fast8_t k = row; k > can_control + 1; k--) {
							if (_array[k][col] == b_pawn || (col > 1 && _array[k][col - 2] == b_pawn)) {
								no_control = true;
								break;
							}
						}
					}

				// A white pawn can control the weak square and no black pawn can attack it, so it is not a weak square
				if (can_control != -1 && !no_control) {
					//if (weak)
					//	cout << "shouldn't be weak here.. ?" << endl;
					weak = false;
					safe = false;
				}
			}

			if (!weak && !safe)
				continue;

			// Unless this is the left edge
			if (col < 7) {

					// Look at the rank of the nearest white pawn able to control the square
					int can_control = -1;
					bool blocking_pawn = false;

					for (int_fast8_t k = row - 1; k > 0; k--) {
						if (_array[k][col + 1] == w_pawn) {
							weak = false;
							if (!blocking_pawn) {
								can_control = k;
								break;
							}
						}

						// If a black pawn blocks the white pawn
						if (_array[k][col + 1] == b_pawn) {
							blocking_pawn = true;
						}
					}

					// Can a black pawn stop the white pawn from controlling the weak square?
					bool no_control = false;

					if (can_control != -1) {
						for (int_fast8_t k = row; k > can_control + 1; k--) {
							if (_array[k][col] == b_pawn || (col < 6 && _array[k][col + 2] == b_pawn)) {
								no_control = true;
								break;
							}
						}
					}

				// A white pawn can control the weak square and no black pawn can attack it, so it is not a weak square
				if (can_control != -1 && !no_control) {
					//if (weak)
					//	cout << "shouldn't be weak here.. ?" << endl;
					weak = false;
					safe = false;
				}
			}

			if (!weak && !safe)
				continue;

				if (display)
					cout << "W weak square: " << Pos(row, col).square() << ": " << (weak ? "weak " : "safe");

				// This is a weak square
				int square_value = 0;

				if (around_king) {
					int dist = max(abs(king_pos.row - row), abs(king_pos.col - col));
					if (dist > 0 && dist <= 3)
						square_value = king_weak_square[dist - 1];

					if (display)
						cout << " | d_king: " << dist;
				}
				else {
					square_value = weak_square_values[7 - row][col];
				}

				if (display)
					cout << " | value: " << square_value;

				// Control of the square by enemy pawn(s)
				const int pawn_controls = (col > 0 && _array[row + 1][col - 1] == b_pawn) + (col < 7 && _array[row + 1][col + 1] == b_pawn);

				if (display)
					cout << " | pawn controls: " << pawn_controls;

				// Outposts
				if (pawn_controls > 0 || true) {

					// Value of the enemy outpost
					int outpost_value = 0;

					if (around_king) {
						int dist = max(abs(king_pos.row - row), abs(king_pos.col - col));
						if (dist > 0 && dist <= 3)
							outpost_value = king_outpost_square[dist - 1];
					}
					else {
						outpost_value = outpost_square_values[7 - row][col];
					}

					const uint8_t p = _array[row][col];

					// Value depending on the piece
					float outpost_value_multiplier = is_black(p) ? outpost_values[p - 7] : 0.0f;

					square_value += outpost_value * outpost_value_multiplier;

					if (display)
						cout << " | outpost: " << outpost_value << " for piece: " << piece_name(p) << " -> *= " << outpost_value_multiplier;
				}

				// Value of an enemy pawn controlling the weak square
				square_value *= (1.00f + 0.75f * (pawn_controls > 0));

				square_value *= (weak ? 1.0f : safe_square_bonus);

				// Lower the weakness value when White controls it more
				//int control_diff = max(0, white_controls._array[row][col] - black_controls._array[row][col]);
				int control_diff = white_controls._array[row][col];

				if (display)
					cout << " | control diff: " << control_diff;

				square_value /= (1.0f + control_diff / 1.0f);

				if (display)
					cout << " | with controls: " << square_value;

				// Bonus when it blocks a pawn
				if (_array[row - 1][col] == w_pawn)
					square_value *= blocked_pawn_bonus;

				if (display)
					cout << " | with blocked pawn: " << square_value << endl;

				weak_squares_value += square_value;
			}
		}
	}

	// r1bq1r2/pnp1ppbk/1p1p1npp/3P4/1P1NP3/2NBB3/P1PQ1PPP/R4RK1 b - - 2 12

	// Black's weak squares
	else {

		if (display)
			cout << endl << "Black weak squares:" << endl << endl;

		// For each square
		for (uint8_t row = 1; row < 7; row++) {
			for (uint8_t col = 0; col < 8; col++) {

				if (_array[row][col] == w_pawn || _array[row][col] == b_pawn)
					continue;

				// No pawn on the square, so for now assume it is a weak and safe square
				bool weak = true;
				bool safe = true;

				// Unless this is the left edge
				if (col > 0) {

					// Look at the rank of the nearest black pawn able to control the square
					int can_control = -1;
					bool blocking_pawn = false;

					for (int_fast8_t k = row + 1; k < 7; k++) {
						if (_array[k][col - 1] == b_pawn) {
							weak = false;
							if (!blocking_pawn) {
								can_control = k;
								break;
							}
						}

						// If a white pawn blocks the black pawn
						if (_array[k][col - 1] == w_pawn) {
							blocking_pawn = true;
						}
					}

					// Can a white pawn stop the black pawn from controlling the weak square?
					bool no_control = false;

					if (can_control != -1) {
						for (int_fast8_t k = row; k < can_control - 1; k++) {
							if (_array[k][col] == w_pawn || (col > 1 && _array[k][col - 2] == w_pawn)) {
								no_control = true;
								break;
							}
						}
					}

					// A black pawn can control the weak square and no white pawn can attack it, so it is not a weak square
					if (can_control != -1 && !no_control) {
						//if (weak)
						//	cout << "shouldn't be weak here.. ?" << endl;
						weak = false;
						safe = false;
					}
				}

				if (!weak && !safe)
					continue;

				// Unless this is the left edge
				if (col < 7) {

					// Look at the rank of the nearest black pawn able to control the square
					int can_control = -1;
					bool blocking_pawn = false;

					for (int_fast8_t k = row + 1; k < 7; k++) {
						if (_array[k][col + 1] == b_pawn) {
							weak = false;
							if (!blocking_pawn) {
								can_control = k;
								break;
							}
						}

						// If a white pawn blocks the black pawn
						if (_array[k][col + 1] == w_pawn) {
							blocking_pawn = true;
						}
					}

					// Can a white pawn stop the black pawn from controlling the weak square?
					bool no_control = false;

					if (can_control != -1) {
						for (int_fast8_t k = row; k < can_control - 1; k++) {
							if (_array[k][col] == w_pawn || (col < 6 && _array[k][col + 2] == w_pawn)) {
								no_control = true;
								break;
							}
						}
					}

					// A black pawn can control the weak square and no white pawn can attack it, so it is not a weak square
					if (can_control != -1 && !no_control) {
						//if (weak)
						//	cout << "shouldn't be weak here.. ?" << endl;
						weak = false;
						safe = false;
					}
				}

				if (!weak && !safe)
					continue;

				if (display)
					cout << "B weak square: " << Pos(row, col).square() << ": " << (weak ? "weak " : "safe");

				// This is a weak square
				int square_value = 0;

				if (around_king) {
					int dist = max(abs(king_pos.row - row), abs(king_pos.col - col));
					if (dist > 0 && dist <= 3)
						square_value = king_weak_square[dist - 1];

					if (display)
						cout << " | d_king: " << dist;
				}
				else {
					square_value = weak_square_values[row][col];
				}

				if (display)
					cout << " | value: " << square_value;

				// Control of the square by enemy pawn(s)
				const int pawn_controls = (col > 0 && _array[row - 1][col - 1] == w_pawn) + (col < 7 && _array[row - 1][col + 1] == w_pawn);

				if (display)
					cout << " | pawn controls: " << pawn_controls;

				// Outposts
				if (pawn_controls > 0 || true) {

					// Value of the enemy outpost
					int outpost_value = 0;

					if (around_king) {
						int dist = max(abs(king_pos.row - row), abs(king_pos.col - col));
						if (dist > 0 && dist <= 3)
							outpost_value = king_outpost_square[dist - 1];
					}
					else {
						outpost_value = outpost_square_values[row][col];
					}

					const uint8_t p = _array[row][col];

					// Value depending on the piece
					float outpost_value_multiplier = is_white(p) ? outpost_values[p - 1] : 0.0f;

					square_value += outpost_value * outpost_value_multiplier;

					if (display)
						cout << " | outpost: " << outpost_value << " for piece: " << piece_name(p) << " -> *= " << outpost_value_multiplier;
				}

				// Value of an enemy pawn controlling the weak square
				square_value *= (1.00f + 0.75f * (pawn_controls > 0));

				square_value *= (weak ? 1.0f : safe_square_bonus);

				// Lower the weakness value when Black controls it more
				//int control_diff = max(0, black_controls._array[row][col] - white_controls._array[row][col]);
				int control_diff = black_controls._array[row][col];

				if (display)
					cout << " | control diff: " << control_diff;

				square_value /= (1.0f + control_diff / 1.0f);

				if (display)
					cout << " | with controls: " << square_value;

				// Bonus when it blocks a pawn
				if (_array[row + 1][col] == b_pawn)
					square_value *= blocked_pawn_bonus;

				if (display)
					cout << " | with blocked pawn: " << square_value << endl;

				weak_squares_value += square_value;
			}
		}
	}

	

	// Open file count
	int open_files = 0;

	// For each file
	for (uint8_t j = 0; j < 8; j++) {
		bool open = true;

		// For each square
		for (uint8_t i = 0; i < 8; i++) {
			if (_array[i][j] == w_pawn || _array[i][j] == b_pawn) {
				open = false;
				break;
			}
		}

		open_files += open;
	}

	//cout << "open files: " << open_files << endl;
	//cout << "total weak squares value: " << weak_squares_value << endl;
	//cout << "final value: " << weak_squares_value / (open_files / 2 + 1) << endl;

	// Depending on how far the game has progressed
	float advancement_factor = around_king ? 0.0f : 0.35f;

	//2r5/3r4/p1p1pk2/PpRnR2p/3P2p1/4P3/7P/1B5K b - - 5 37: weak squares around the king, 200+?

	if (display)
		cout << "weak squares value: " << weak_squares_value << ", open files: " << open_files << ", advancement factor: " << advancement_factor << ", total: " << eval_from_progress(weak_squares_value / (open_files / 2.0f + 1), _adv, advancement_factor) << endl;
	
	return eval_from_progress(weak_squares_value / (open_files / 2.0f + 1), _adv, advancement_factor);
}

// Converts a move into its algebraic notation
// Returns the value of the distance to being able to castle
int Board::get_castling_distance() const {
	// TODO: merge with other functions, so it stops handing out a flat bonus even when castling is terrible

	// Look for pieces blocking the castle (and whether they can move? TODO), or enemy pieces controlling the squares

	// Penalty for the minimum distance to castling
	int castling_distance_malus = 30;

	// White

	// Kingside castling
	uint8_t w_kingside_castle_distance = 0;

	// If kingside castling is still available
	if (_castling_rights.k_w) {
		// Are any pieces blocking the castle? With the bishop still on f1 (confirm it is the bishop), add a penalty if something blocks its exit
		// TODO: improve this, there are special cases
		w_kingside_castle_distance += (_array[0][5] != 0) + (_array[0][6] != 0) + (_array[0][5] == w_bishop && _array[1][4] != 0 && _array[1][6] != 0);
		//cout << "w_kingside_castle_distance: " << (int)w_kingside_castle_distance << endl;

		// Do any enemy pieces control the squares?
		w_kingside_castle_distance += is_controlled(0, 5, true) + is_controlled(0, 6, true);
	}
	else {
		w_kingside_castle_distance = 2;
	}

	// Queenside castling
	uint8_t w_queenside_castle_distance = 0;

	// If queenside castling is still available
	if (_castling_rights.q_w) {
		// Are any pieces blocking the castle?
		w_queenside_castle_distance += (_array[0][1] != 0) + (_array[0][2] != 0) + (_array[0][3] != 0) + (_array[0][2] == w_bishop && _array[1][1] != 0 && _array[1][3] != 0);
		//cout << "w_queenside_castle_distance: " << (int)w_queenside_castle_distance << endl;

		// Do any enemy pieces control the squares?
		w_queenside_castle_distance += is_controlled(0, 2, true) + is_controlled(0, 3, true);
	}
	else {
		w_queenside_castle_distance = 2;
	}
	
	// Minimum distance to castle (2 once already castled)
	uint8_t w_castling_distance = (_castling_rights.k_w || _castling_rights.q_w) ? min(w_kingside_castle_distance, w_queenside_castle_distance) : 1;


	// Black

	// Kingside castling
	uint8_t b_kingside_castle_distance = 0;

	// If kingside castling is still available
	if (_castling_rights.k_b) {
		// Are any pieces blocking the castle?
		b_kingside_castle_distance += (_array[7][5] != 0) + (_array[7][6] != 0) + (_array[7][5] == b_bishop && _array[6][4] != 0 && _array[6][6] != 0);
		//cout << "b_kingside_castle_distance: " << (int)b_kingside_castle_distance << endl;

		// Do any enemy pieces control the squares?
		b_kingside_castle_distance += is_controlled(7, 5, false) + is_controlled(7, 6, false);
	}
	else {
		b_kingside_castle_distance = 2;
	}

	// Queenside castling
	uint8_t b_queenside_castle_distance = 0;

	// If queenside castling is still available
	if (_castling_rights.q_b) {
		// Are any pieces blocking the castle?
		b_queenside_castle_distance += (_array[7][1] != 0) + (_array[7][2] != 0) + (_array[7][3] != 0) + (_array[7][2] == b_bishop && _array[6][1] != 0 && _array[6][3] != 0);
		//cout << "b_queenside_castle_distance: " << (int)b_queenside_castle_distance << endl;

		// Do any enemy pieces control the squares?
		b_queenside_castle_distance += is_controlled(7, 2, false) + is_controlled(7, 3, false);
	}
	else {
		b_queenside_castle_distance = 2;
	}

	// Minimum distance to castle (2 once already castled)
	uint8_t b_castling_distance = (_castling_rights.k_b || _castling_rights.q_b) ? min(b_kingside_castle_distance, b_queenside_castle_distance) : 1;

	//cout << "w_castling_distance: " << (int)w_castling_distance << endl;
	//cout << "b_castling_distance: " << (int)b_castling_distance << endl;

	return castling_distance_malus * (b_castling_distance - w_castling_distance) * (1 - _adv);
}

// Generates the Zobrist key of the board (debug helper)
float Board::get_winnable(Evaluation* eval, bool color, float position_nature) const {
	// TODO *** ajouter win conditions
	// Mats
	// Passed pawns
	
	// rnbqkbnr/8/p3p3/Pp1pPp1p/1PpP1PpP/2P3P1/8/RNBQKBNR w KQkq - 0 11: completely closed position -> winnable should be zero, but uncertainty took over

	// MIDDLEGAME:
	// Factors contributing to winning chances:
	// - material imbalance
	// - Incertitude
	// - opposite-side castling
	
	// Factors contributing to drawing chances:
	// - Fermeture de la position
	// - symmetry of the position
	
	// ENDGAME:
	// 
	// Factors contributing to winning chances:
	// - number of pawns left
	// - whether mate is possible
	
	// Factors contributing to drawing chances:
	// - Finales de tours -> +0.25?
	// - opposite-coloured bishops -> 0.5?

	// EXEMPLES:
	// 2q5/k3bp2/p1p1b1p1/P1PpPp1p/1Pp2P1P/2Q3P1/6K1/3RR3 w - - 0 1: this is a draw (closed position)
	// 6k1/2p5/N7/8/4K3/8/8/8 b - - 0 61: unwinnable for White, no pawns left and not enough material to mate
	// Rook against knight or bishop -> draw
	// Rook against queen -> it depends?
	// Rook against a lone king -> win
	// Rook against rook -> draw
	// Knight, lone bishop, or two knights -> draw
	// Pawn endgames -> some are drawn, even with a bishop on top of the pawn when it is the wrong colour


	// Implementations:
	// - pawn count
	// - can mate (pawns left, or material beyond 2 knights)
	// - stalemates? rampant rook and the like, but hard to evaluate and very rare
	// - drawish material ratios: rook against knight and so on


	bool display = false;

	// Constantes
	constexpr float mating_potentials[6] = { 1.0f, 0.4f, 0.6f, 1.2f, 2.0f, 0.0f };


	// Pawn count of the side
	uint8_t pawns_count = 0;

	// Potentiel de mat
	float mating_potential = 0.0f;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			const uint8_t p = _array[row][col];

			// If the square is empty
			if (p == none)
				continue;

			if (is_white(p) == color) {
				if (p == (color ? w_pawn : b_pawn))
					pawns_count++;

				if (p == (color ? w_queen : b_queen)) {
					pawns_count += 2;
				}

				mating_potential += mating_potentials[_array[row][col] - (color ? 1 : 7)];
			}
		}
	}

	bool can_mate = mating_potential >= 1.0f;

	if (display) {
		cout << "Winnable eval for " << (color ? "white" : "black") << ":" << endl;
		cout << "- pawns count: " << (int)pawns_count << endl;
		cout << "- mating potential: " << mating_potential << endl;
		cout << "- can mate: " << (can_mate ? "yes" : "no") << endl;
	}

	// If we cannot mate, we cannot win.. makes sense
	if (!can_mate) {
		return 0.0f;
	}

	// Here we can mate -> complex endgames or middlegame

	// 8/5ppk/3R3p/8/8/1r5P/6PK/8 w - - 0 32: theoretical draw, rook v rook with pawns 3v2
	// 2r5/2kb3K/8/8/8/1Q6/8/8 w - - 0 92: theoretical draw as well

	// rn1qkbnr/ppp2ppp/4b3/8/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 4 : bug?

	// Winnable value for a pawnless endgame
	constexpr float no_pawns_winnable = 0.15f;

	// The more pawns there are, the more winnable it gets
	constexpr float n_pawns_winnable[11] = { 0.0f, 0.20f, 0.37f, 0.53f, 0.67f, 0.79f, 0.85f, 0.90f, 0.94f, 0.98f, 1.0f };
	// Queens count double: promotion storms push the count past the table - saturate.
	const float pawns_factor = n_pawns_winnable[min<uint8_t>(pawns_count, 10)];

	// Base winnable value
	float winnable_value = (1 - no_pawns_winnable) * pawns_factor + no_pawns_winnable;

	if (display) {
		cout << "- pawns factor: " << pawns_factor << endl;
		cout << "- base winnable value: " << winnable_value << endl;
	}

	// 8/PK6/3k4/8/8/8/8/8 w - - 0 79 : FIXME *** a8 ne devrait pas baisser le winnable!
	// K1k5/P7/3N4/8/8/8/8/8 b - - 2 77: Kc7 is the only move that holds


	// Some endgames are drawish than others, even with pawns on the board:
	// - opposite-coloured bishops: 0.62
	// - rook against rook: 0.45
	// - queen against queen: 0.5
	// - same-coloured bishops: 0.28
	// - knight against knight: 0.28

	// Likewise, rook+bishop against rook+bishop -> ~0.5

	// Drawishness value per piece
	constexpr float draw_potentials[6] = { 0.0f, 0.17f, 0.17f, 0.34f, 0.42f, 0.0f };

	// Drawishness value of opposite-coloured bishops
	constexpr float opposite_bishops_draw = 0.62f;

	// For now only endgames with equal pieces are judged
	// Average the potential of each piece

	// Piece counts
	uint8_t white_pieces[6] = { 0, 0, 0, 0, 0, 0 };
	uint8_t black_pieces[6] = { 0, 0, 0, 0, 0, 0 };

	// Are there opposite-coloured bishops?
	int light_white_bishops = 0;
	int dark_white_bishops = 0;
	int light_black_bishops = 0;
	int dark_black_bishops = 0;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			const uint8_t p = _array[row][col];

			// If the square is empty
			if (p == none)
				continue;

			if (is_white(p)) {
				white_pieces[p - 1]++;
				if (p == w_bishop) {
					if ((row + col) % 2 == 0)
						light_white_bishops++;
					else
						dark_white_bishops++;
				}
			}
			else {
				black_pieces[p - 7]++;
				if (p == b_bishop) {
					if ((row + col) % 2 == 0)
						light_black_bishops++;
					else
						dark_black_bishops++;
				}
			}
		}
	}

	// Are we in an opposite-coloured bishop situation, and if so how many such pairs? Should almost never happen
	int opposite_bishops = (light_white_bishops * dark_white_bishops == 0 && light_black_bishops * dark_black_bishops == 0 &&
		light_white_bishops * light_black_bishops == 0 && dark_white_bishops * dark_black_bishops == 0) ?
		min(light_white_bishops, dark_black_bishops) + min(dark_white_bishops, light_black_bishops) : 0;

	// Is the material equal, pawns excluded?
	bool equal_material = true;

	for (uint8_t i = 1; i < 5; i++) {
		if (white_pieces[i] != black_pieces[i]) {
			equal_material = false;
			break;
		}
	}

	//8/2k5/4Bp2/2b1p1p1/4K2p/7P/8/8 w - - 0 1

	//print_array(white_pieces, 6);
	//print_array(black_pieces, 6);
	//cout << "equal material: " << equal_material << endl;
	//cout << "opposite bishops: " << opposite_bishops << endl;

	if (equal_material) {
		// Average drawishness value of the opponent's pieces
		float draw_value = 0.0f;
		int draw_pieces = 0;

		for (uint8_t i = 1; i < 5; i++) {
			if (!opposite_bishops || i != 2) {
				draw_value += draw_potentials[i] * (color ? black_pieces[i] : white_pieces[i]);
				draw_pieces += (color ? black_pieces[i] : white_pieces[i]);
			}
		}

		// With opposite-coloured bishops, add the drawing potential
		if (opposite_bishops > 0) {
			draw_value += opposite_bishops_draw * opposite_bishops;
			draw_pieces += opposite_bishops;
		}

		//cout << "draw value: " << draw_value << ", draw pieces: " << draw_pieces << endl;

		draw_value /= (draw_pieces == 0) ? 1.0f : (draw_pieces + pawns_count / 2.0f);

		// Update the winnable value from the drawishness and the remaining winning chances
		winnable_value *= (1.0f - draw_value * _adv);

		if (display) {
			cout << "- draw value: " << draw_value << endl;
			cout << "- final winnable value after draw value: " << winnable_value << endl;
		}
	}

	// r6r/1p1k3p/p2p1npb/4p3/4P3/4NP2/PPP1K1PP/R1B4R w - - 2 17: the more pawns there are, the better the winning chances
	
	// TODO: implement drawing chances for unbalanced material ratios in the endgame
	// TODO: bishop against knight

	// TODO: special pawn endgame case: king and pawn against lone king is a theoretical draw when the king stands in front of the enemy pawn


	// TODO: derive the factors from the game progress

	// TODO: implement the rest of the logic for middlegame winning chances

	// rnbqkb1r/4n3/p3p1p1/Pp1pPpPp/1PpP1P1P/2P5/8/RNBQKBNR b KQkq - 0 11: closed position

	// Depending on the nature of the position
	constexpr float closed_position_draw_factor = 1.0f;
	winnable_value *= 1.0f - position_nature;
	// FIXME: weight this even more, since it rarely reaches 0% or 100%?

	if (display) {
		cout << "- position nature: " << position_nature << endl;
		cout << "- final winnable value after position nature: " << winnable_value << endl;
	}

	// TODO: take the uncertainty into account (raising winnable?)
	winnable_value = 1.0f - (1.0f - winnable_value) * (1.0f - eval->_uncertainty * 0.35f);

	if (display) {
		cout << "- uncertainty: " << eval->_uncertainty << endl;
		cout << "- final winnable value after uncertainty: " << winnable_value << endl;
	}


	// TODO *** win conditions
	// - Mat
	// - passed pawns
	constexpr float passed_pawn_winnable_bonuses[8] = { 0.0f, 0.50f, 0.60f, 0.70f, 0.75f, 0.80f, 0.85f, 0.90f };
	// Clamp: doubled passed pawns can push the count past the table (the old
	// "if (> 8)" guard was an EMPTY stub and the unclamped count still indexed
	// the array - ASAN stack-buffer-overflow on a corpus promotion position).
	const int passed_pawns_count = min(7, get_passed_pawns_count(color));

	winnable_value = 1.0f - (1.0f - winnable_value) * (1.0f - passed_pawn_winnable_bonuses[passed_pawns_count]);

	return winnable_value;
}

// Computes the winning-chance values for each side
void Board::get_winnable_values(Evaluation* eval, float position_nature) const {
	eval->_winnable_white = get_winnable(eval, true, position_nature);
	eval->_winnable_black = get_winnable(eval, false, position_nature);
}

// Returns the activity of the bishops along the diagonals
int Board::get_trapped_pieces() const {
	// Isolated piece: one far from the other friendly pieces

	// TODO: adapt this to endgames too, so the king closes on the pawns? same for the knights
	// TODO: add a variable radius for the centre, to tell whether it is extended and judge isolation better

	// TESTS
	//rn2kbnr/pp3ppp/4p3/2ppPb2/2PP4/4BN2/Pq2BPPP/RN1QK2R w KQkq - 0 8
	//8/1p4p1/p4p2/2k1p1p1/P1n1P3/2BK1P1P/6P1/8 b - - 0 42

	// 1r3rk1/p3bppp/2p3b1/1pPqN3/3P2P1/1PB1pP2/P5BP/R3Q1K1 w - - 0 23 : -200??

	// Weight per piece type
	const float pawn_weight = 1.0f;
	const float knight_weight = 4.0f; // Heavy weight, since this is a short-range piece
	const float bishop_weight = 3.0f;
	const float rook_weight = 3.0f;
	const float queen_weight = 5.0f;
	const float king_weight = 0.0f; // Unclear whether a weight belongs here: it may pile every piece into defence

	// Base total weight, per colour
	const float total_weight = 8.0f * pawn_weight + 2.0f * knight_weight + 2.0f * bishop_weight + 2.0f * rook_weight + queen_weight + king_weight;

	// White pieces

	// Compute the centre of mass
	float w_center_of_mass_i = 0.0f;
	float w_center_of_mass_j = 0.0f;
	float w_total_weight = 0.0f;

	// Black pieces

	// Compute the centre of mass
	float b_center_of_mass_i = 0.0f;
	float b_center_of_mass_j = 0.0f;
	float b_total_weight = 0.0f;

	// For each piece
	uint64_t occ = _occupancies[2];
	while (occ) {
		const int sq = pop_lsb(occ);
		const uint8_t row = sq >> 3;
		const uint8_t col = sq & 7;
		const uint8_t p = _array[row][col];

		// If this is a white piece
		if (p <= w_king) {
				// Piece weight
				float weight = p == w_pawn ? pawn_weight : (p == w_knight ? knight_weight : (p == w_bishop ? bishop_weight : (p == w_rook ? rook_weight : (p == w_queen ? queen_weight : king_weight))));

				//cout << "i: " << i << ", j: " << j << ", weight: " << weight << endl;

				// Centre de masse
				w_center_of_mass_i += row * weight;
				w_center_of_mass_j += col * weight;
				w_total_weight += weight;
			}

			// If this is a black piece
			else {
				// Piece weight
				float weight = p == b_pawn ? pawn_weight : (p == b_knight ? knight_weight : (p == b_bishop ? bishop_weight : (p == b_rook ? rook_weight : (p == b_queen ? queen_weight : king_weight))));
				
				//cout << "i: " << i << ", j: " << j << ", weight: " << weight << endl;

				// Centre de masse
				b_center_of_mass_i += row * weight;
				b_center_of_mass_j += col * weight;
				b_total_weight += weight;
			}
	}

	// Computation of the centre of mass
	w_center_of_mass_i /= w_total_weight;
	w_center_of_mass_j /= w_total_weight;

	b_center_of_mass_i /= b_total_weight;
	b_center_of_mass_j /= b_total_weight;

	//cout << "w_center_of_mass_i: " << w_center_of_mass_i << endl;
	//cout << "w_center_of_mass_j: " << w_center_of_mass_j << endl;
	//cout << "b_center_of_mass_i: " << b_center_of_mass_i << endl;
	//cout << "b_center_of_mass_j: " << b_center_of_mass_j << endl;


	// Distance from the pieces to the centre of mass
	// Pieces far from the centre have to be penalised
	float min_distance = 3.0f;

	float w_trapped_pieces = 0.0f;
	float b_trapped_pieces = 0.0f;

	// Linear penalty for now?

	// Penalty per piece type
	const float pawn_malus = 2.0f;
	const float knight_malus = 7.0f; // Heavy weight, since this is a short-range piece
	const float bishop_malus = 7.0f;
	const float rook_malus = 9.0f;
	const float queen_malus = 15.0f;
	const float king_malus = 0.0f;

	const float attacked_factor = 3.0f;
	const float trapped_factor = 15.0f;

	// Progress beyond which isolation stops counting, though trapped pieces still do
	const float max_adv = 0.6f;

	// Distance beyond which the piece counts as effectively "attacked"
	const int attacked_distance = 10;

	// Damping factor when the piece can capture an enemy one back (TODO)

	const SquareMap w_controls = get_white_controls_map();
	const SquareMap b_controls = get_black_controls_map();

	//w_controls.print();
	//b_controls.print();
	//5rk1/Qbp2pp1/1pq1p2p/3p4/8/P1P1P3/2P2PPP/R3R1K1 w - - 0 21
	//rn2kbnr/pp3ppp/4p3/2ppP3/2PP4/1Q2BN2/P3BPPP/qR4K1 b kq - 0 10

	// For each piece
	uint64_t occ2 = _occupancies[2];
	while (occ2) {
		const int sq = pop_lsb(occ2);
		const uint8_t row = sq >> 3;
		const uint8_t col = sq & 7;
		const uint8_t p = _array[row][col];

		// If this is a white piece
		if (is_white(p)) {

				// Distance to the centre of mass
				const float di = row - w_center_of_mass_i;
				const float dj = col - w_center_of_mass_j;
				float base_distance = sqrt(di * di + dj * dj);

				float distance = max(0.0f, base_distance - min_distance);

				// Plus gros malus quand proche du camp advserse
				distance *= row + 1;

				// Penalty
				const float malus = p == w_pawn ? pawn_malus : (p == w_knight ? knight_malus : (p == w_bishop ? bishop_malus : (p == w_rook ? rook_malus : (p == w_queen ? queen_malus : king_malus))));

				// Penalty based on distance
				//float isolated_piece = distance * malus * max(1.0f - _adv / max_adv, 0.0f);
				float isolated_piece = distance * malus * w_total_weight / total_weight * 0.5f;

				//cout << piece_name(p) << " on " << square_name(row, col) << ", distance: " << distance << ", isolated_piece: " << isolated_piece << endl;
				//float isolated_piece = 0.0f;

				//cout << Pos(row, col).square() << ", distance: " << distance << ", trapped_piece: " << trapped_piece;

				// Penalty increased by how few uncontrolled squares the piece can reach

				int safe_squares = 0;

				// Pawn
				if (p == w_pawn) {
				}

				// Knight
				if (p == w_knight) {
					for (int k = 0; k < 8; k++) {
						int i2 = row + knight_directions[k][0];
						int j2 = col + knight_directions[k][1];

						if (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8)
							safe_squares += (b_controls._array[i2][j2] < w_controls._array[i2][j2]) && !is_white(_array[i2][j2]);
					}
				}
				
				// Straight-line sliders
				if (p == w_rook || p == w_queen) {
					for (int k = 0; k < 4; k++) {
						int i2 = row + rect_directions[k][0];
						int j2 = col + rect_directions[k][1];

						while (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8) {
							// If the square is uncontrolled and no friendly piece blocks it
							safe_squares += (b_controls._array[i2][j2] < w_controls._array[i2][j2]) && !is_white(_array[i2][j2]);

							if (_array[i2][j2] != none) {
								break;
							}

							i2 += rect_directions[k][0];
							j2 += rect_directions[k][1];
						}
					}
				}

				// Diagonal sliders
				if (p == w_bishop || p == w_queen) {
					for (int k = 0; k < 4; k++) {
						int i2 = row + diag_directions[k][0];
						int j2 = col + diag_directions[k][1];

						while (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8) {
							// If the square is uncontrolled and no friendly piece blocks it
							safe_squares += (b_controls._array[i2][j2] < w_controls._array[i2][j2]) && !is_white(_array[i2][j2]);

							if (_array[i2][j2] != none) {
								break;
							}

							i2 += diag_directions[k][0];
							j2 += diag_directions[k][1];
						}
					}
				}

				// King
				if (p == w_king) {
					for (int k = 0; k < 8; k++) {
						int i2 = row + all_directions[k][0];
						int j2 = col + all_directions[k][1];

						if (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8)
							safe_squares += (b_controls._array[i2][j2] < w_controls._array[i2][j2]) && !is_white(_array[i2][j2]);
					}
				}

				// How trapped the piece is
				const float trapped_malus = trapped_factor / ((safe_squares + 1) * (safe_squares + 1));

				bool attacked = b_controls._array[row][col] != 0;

				// An isolated piece is almost equivalent to an attacked one
				int attacked_bonus = min(attacked ? max(0.0f, base_distance * (row - 2)) : distance / (float)attacked_distance * attacked_factor / 2.0f, attacked_factor);

				if (p != w_pawn) {
					//cout << piece_name(p) << " on " << square_name(row, col) << ", safe_squares: " << safe_squares << ", trapped malus: " << trapped_malus << ", isolated_piece: " << isolated_piece << ", attacked: " << (b_controls._array[row][col] != 0) << ", distance: " << distance << ", attacked_bonus: " << attacked_bonus;

					// A pawn is never considered trappable
					isolated_piece += malus * trapped_malus * attacked_bonus;

					//cout << " *** total: " << isolated_piece << endl;
				}

				// TODO: can the piece trade itself off? If it can, against an equal or higher value, it is not really trapped
				// e.g. 1r3rk1/p3bppp/2p3b1/1pPqN3/3P2P1/1PB1pP2/P5BP/R3Q1K1 w - - 0 23: the knight on e5 can take the bishop on g6, so f6 is not a threat

				w_trapped_pieces += isolated_piece;

				//2rq1rk1/1p1b1pp1/p1n2b1p/P2p3n/1P4P1/2PB1N1P/1Q1N1P1B/R2R2K1 b - g3 0 21: knight trapped on h5
				//rn2kbnr/pp3ppp/1q2p3/2ppPb2/2PP4/4BN2/PP2BPPP/RN1QK2R b KQkq - 0 7: tests with a trapped queen

				// r1bqkb1Q/ppp5/2n1p1p1/3pP3/8/2P5/PP3PPP/RNB1K2R b KQq - 0 11: the white queen is not trapped here
			}

			// If this is a black piece
			else {

				// Distance to the centre of mass
				const float di = row - b_center_of_mass_i;
				const float dj = col - b_center_of_mass_j;
				float base_distance = sqrt(di * di + dj * dj);

				float distance = max(0.0f, base_distance - min_distance);

				// Larger penalty closer to the enemy camp
				distance *= 8 - row;

				// Penalty
				const float malus = p == b_pawn ? pawn_malus : (p == b_knight ? knight_malus : (p == b_bishop ? bishop_malus : (p == b_rook ? rook_malus : (p == b_queen ? queen_malus : king_malus))));

				// Penalty based on distance
				//float isolated_piece = distance * malus * max(1.0f - _adv / max_adv, 0.0f);
				float isolated_piece = distance * malus * b_total_weight / total_weight * 0.5f;
				//float isolated_piece = 0.0f;

				//cout << piece_name(p) << " on " << square_name(row, col) << ", distance: " << distance << ", isolated_piece: " << isolated_piece << endl;

				//cout << Pos(row, col).square() << ", distance: " << distance << ", trapped_piece: " << trapped_piece;

				// Penalty increased by how few uncontrolled squares the piece can reach

				int safe_squares = 0;

				// Pawn
				if (p == b_pawn) {
				}

				// Knight
				if (p == b_knight) {
					for (int k = 0; k < 8; k++) {
						int i2 = row + knight_directions[k][0];
						int j2 = col + knight_directions[k][1];

						if (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8)
							safe_squares += (w_controls._array[i2][j2] < b_controls._array[i2][j2]) && !is_black(_array[i2][j2]);
					}
				}

				// Straight-line sliders
				if (p == b_rook || p == b_queen) {
					for (int k = 0; k < 4; k++) {
						int i2 = row + rect_directions[k][0];
						int j2 = col + rect_directions[k][1];

						while (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8) {
							// If the square is uncontrolled and no friendly piece blocks it
							safe_squares += (w_controls._array[i2][j2] < b_controls._array[i2][j2]) && !is_black(_array[i2][j2]);

							if (_array[i2][j2] != none) {
								break;
							}

							i2 += rect_directions[k][0];
							j2 += rect_directions[k][1];
						}
					}
				}

				// Diagonal sliders
				if (p == b_bishop || p == b_queen) {
					for (int k = 0; k < 4; k++) {
						int i2 = row + diag_directions[k][0];
						int j2 = col + diag_directions[k][1];

						while (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8) {
							// If the square is uncontrolled and no friendly piece blocks it
							safe_squares += (w_controls._array[i2][j2] < b_controls._array[i2][j2]) && !is_black(_array[i2][j2]);

							if (_array[i2][j2] != none) {
								break;
							}

							i2 += diag_directions[k][0];
							j2 += diag_directions[k][1];
						}
					}
				}

				// King
				if (p == b_king) {
					for (int k = 0; k < 8; k++) {
						int i2 = row + all_directions[k][0];
						int j2 = col + all_directions[k][1];

						if (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8)
							safe_squares += (w_controls._array[i2][j2] < b_controls._array[i2][j2]) && !is_black(_array[i2][j2]);
					}
				}

				const float trapped_malus = trapped_factor / ((safe_squares + 1) * (safe_squares + 1));

				bool attacked = w_controls._array[row][col] != 0;

				// An isolated piece is almost equivalent to an attacked one
				int attacked_bonus = min(attacked ? max(0.0f, base_distance * (5 - row)) : distance / (float)attacked_distance * attacked_factor / 2.0f, attacked_factor);

				if (p != b_pawn) {
					//cout << piece_name(p) << " on " << square_name(row, col) << ", safe_squares: " << safe_squares << ", trapped malus: " << trapped_malus << ", isolated_piece: " << isolated_piece << ", attacked: " << (b_controls._array[row][col] != 0) << ", distance: " << distance << ", attacked_bonus: " << attacked_bonus;

					// A pawn is never considered trappable
					isolated_piece += malus * trapped_malus * attacked_bonus;

					//cout << " *** total: " << isolated_piece << endl;
				}

				b_trapped_pieces += isolated_piece;

				//2kr1b2/pp2pp2/8/8/3p4/3P2B1/PPP1KPr1/R6R b - - 1 21
			}
	}

	//cout << "w_trapped_pieces: " << w_trapped_pieces << endl;
	//cout << "b_trapped_pieces: " << b_trapped_pieces << endl;

	return (b_trapped_pieces - w_trapped_pieces);
}

// Adjusts the piece values, penalty or bonus, according to the type of position
float Board::get_position_nature() const {
	// Examples to test:
	// rnbqkbnr/8/p1p1p1p1/PpPpPpPp/1P1P1P1P/8/8/RNBQKBNR w KQkq - 1 13: completely closed
	// r1bqkb1r/pp1n1ppp/2n1p3/2ppP3/3P1P2/2N1BN2/PPP3PP/R2QKB1R b KQkq - 3 7: French-type structure -> fairly closed
	// rnbq1rk1/ppp2ppp/3b1n2/3p4/3P4/3B1N2/PPP2PPP/RNBQ1RK1 w - - 6 7: exchange French -> fairly balanced?
	// rnb1kb1r/ppp1pppp/3q1n2/8/3P4/2N2N2/PPP2PPP/R1BQKB1R b KQkq - 2 5: Scandinavian -> fairly open

	// Depends on the pawn structure alone

	// Factors making the position more closed
	// - pawn count
	// - number of blocked pawns

	// Facteurs rendant la position plus ouverte
	// - Number of open files
	// - Nombre de diagonales ouvertes

	// Impact on the other evaluation terms:
	// Position ouverte:
	// Piece activity

	// Closed position:
	// Avantage d'espace
	// Pawn structure
	// Piece placement


	// Factor above which the position counts as completely closed 
	constexpr int completely_closed = 1100;

	// Factor below which the position counts as completely open
	constexpr int completely_open = 200;


	// Closure contributed by a pawn
	constexpr int pawn_closed = 30;

	// Closure contributed by a blocked pawn
	constexpr int blocked_pawn_closed = 50;

	// Pawn count
	int pawns = 0;

	// Number of blocked pawns
	int blocked_pawns = 0;

	// Iterate only pawns using bitboards
	uint64_t wp = _bitboards[w_pawn];
	while (wp) {
		const int sq = pop_lsb(wp);
		const uint8_t row = sq >> 3;
		const uint8_t col = sq & 7;
		pawns++;
		// If the pawn is blocked by another pawn
		if (row < 7 && (_array[row + 1][col] == w_pawn || _array[row + 1][col] == b_pawn))
			blocked_pawns++;
	}
	uint64_t bp = _bitboards[b_pawn];
	while (bp) {
		const int sq = pop_lsb(bp);
		const uint8_t row = sq >> 3;
		const uint8_t col = sq & 7;
		pawns++;
		// If the pawn is blocked by another pawn
		if (row > 0 && (_array[row - 1][col] == w_pawn || _array[row - 1][col] == b_pawn))
			blocked_pawns++;
	}

	int range = (completely_closed - completely_open);

	float total_factor = pawn_closed * pawns + blocked_pawn_closed * blocked_pawns;

	float nature = (total_factor - completely_open) / range;

	return min(max(0.0f, nature), 1.0f);
}

// Returns the bonus value for open and semi-open files bearing on the enemy king
int Board::get_open_files_on_opponent_king(bool player) {
	
	// Castling rights
	bool can_kingside_castle = player ? _castling_rights.k_w : _castling_rights.k_b;
	bool can_queenside_castle = player ? _castling_rights.q_w : _castling_rights.q_b;

	int current_file_weakness = get_open_files_on_opponent_king_at_column(player, player ? _black_king_pos.col : _white_king_pos.col);
	int kingside_file_weakness = can_kingside_castle ? get_open_files_on_opponent_king_at_column(player, 6) : 0;
	// Queenside gated by QUEENSIDE rights (was a copy-paste of kingside)
	int queenside_file_weakness = can_queenside_castle ? get_open_files_on_opponent_king_at_column(player, 2) : 0;

	// !player, because one side's bonuses are the other side's weaknesses
	int total_weakness = get_long_term_king_weakness(!player, current_file_weakness, kingside_file_weakness, queenside_file_weakness);

	return total_weakness;
}

// Returns the bonus value for open and semi-open diagonals bearing on the enemy king
int Board::get_open_diagonals_on_opponent_king(bool color) {
	// *** TODO: fix this?

	// Bonus for the open and semi-open diagonals
	constexpr int open_diagonal_bonus = 60;
	constexpr int semi_open_diagonal_bonus = 25;

	// Factor based on proximity to the enemy king's diagonal
	// If the king is on the diagonal, the bonus is maximal
	constexpr float king_diagonal_bonus = 1.0f;

	// On an adjacent diagonal, the bonus is reduced
	constexpr float king_adjacent_diagonal_bonus = 0.5f;

	// Extra bonus for the pieces standing on it (bishops, queen)
	constexpr int bishop_bonus = 20;
	constexpr int queen_bonus = 15;

	update_kings_pos();

	// Diagonal of the enemy king
	uint8_t king_i = color ? _black_king_pos.row : _white_king_pos.row;
	uint8_t king_j = color ? _black_king_pos.col : _white_king_pos.col;

	// Bonus for the player
	int total_bonus = 0;

	// Friendly pawn
	const int player_pawn = color ? w_pawn : b_pawn;

	// Enemy pawn
	const int opponent_pawn = color ? b_pawn : w_pawn;

	// For each diagonal adjacent to the black king
	for (int i = -1; i < 2; i += 2) {
		for (int j = -1; j < 2; j += 2) {

			// Square coordinates
			uint8_t new_i = king_i + i;
			uint8_t new_j = king_j + j;

			// If the square is off the board
			if (new_i < 0 || new_i > 7 || new_j < 0 || new_j > 7)
				continue;

			// Nature de la diagonale
			bool semi_open = true;
			bool open = true;

			// Parcours de la diagonale
			while (new_i >= 0 && new_i < 8 && new_j >= 0 && new_j < 8) {
				uint8_t p = _array[new_i][new_j];

				if (p == player_pawn) {
					semi_open = false;
					open = false;
					break;
				}
				else if (p == opponent_pawn) {
					open = false;
				}

				// Prochaine case
				new_i += i;
				new_j += j;
			}

			//cout << "diagonal: " << (int)i << ", " << (int)j << ", open: " << open << ", semi_open: " << semi_open << endl;

			// Bonus
			int bonus = (open ? open_diagonal_bonus : (semi_open ? semi_open_diagonal_bonus : 0));

			// Bonus for the pieces standing on the diagonal
			if (open || semi_open) {
				new_i = king_i + i;
				new_j = king_j + j;

				while (new_i >= 0 && new_i < 8 && new_j >= 0 && new_j < 8) {
					uint8_t p = _array[new_i][new_j];

					if (p == (color ? w_bishop : b_bishop))
						bonus += bishop_bonus * (1 + open);
					else if (p == (color ? w_queen : b_queen))
						bonus += queen_bonus * (1 + open);

					new_i += i;
					new_j += j;
				}
			}

			// Bonus based on proximity to the king
			bonus *= (i == 0 && j == 0 ? king_diagonal_bonus : king_adjacent_diagonal_bonus);

			total_bonus += bonus;
		}
	}

	// Depending on how far the game has progressed
	constexpr float advancement_factor = 0.0f;

	return eval_from_progress(total_bonus, _adv, advancement_factor);
}

// Returns the number of retreat squares for the king

