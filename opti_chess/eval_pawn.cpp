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
int Board::get_pawn_structure(float display_factor)
{
	// Still to add:
	// Pawn island count
	// Weak pawns
	// Square control
	// Passed pawns
	// Candidate passed pawns

	int pawn_structure = 0;

	// Pawn list by file
	int s_white[8] = { 0 };
	int s_black[8] = { 0 };

	// Pawn placement (6 rows would theoretically do, since pawns cannot sit on the first or last rank)
	bool pawns_white[8][8] = { { 0 } };
	bool pawns_black[8][8] = { { 0 } };

	// Iterate only pawns using bitboards
	uint64_t wp = _bitboards[w_pawn];
	while (wp) {
		const int sq = pop_lsb(wp);
		const uint8_t i = sq >> 3;
		const uint8_t j = sq & 7;
		s_white[j]++;
		pawns_white[i][j] = true;
	}
	uint64_t bp = _bitboards[b_pawn];
	while (bp) {
		const int sq = pop_lsb(bp);
		const uint8_t i = sq >> 3;
		const uint8_t j = sq & 7;
		s_black[j]++;
		pawns_black[i][j] = true;
	}

	// TEST: r4rk1/pp2qppp/3b1n2/8/P2pP3/3B1P2/P2B2PP/2RQ1RK1 b - - 0 17 : pourquoi isolated positif?

	// Isolated pawns
	constexpr int isolated_pawn = -25;
	constexpr float open_row_factor = 1.5f; // An isolated pawn on an open file is much weaker
	constexpr float isolated_adv_factor = 1.0f; // Depending on how far the game has progressed
	const float isolated_adv = eval_from_progress(1, _adv, isolated_adv_factor);
	float isolated_pawns = 0.0f;

	for (uint8_t col = 0; col < 8; col++) {

		// White isolated pawn on the file
		if (s_white[col] > 0 && (col == 0 || s_white[col - 1] == 0) && (col == 7 || s_white[col + 1] == 0)) {
			bool is_open_col = true;

			for (uint8_t row = 7; row > 0; row--) {
				if (_array[row][col] == w_pawn) {
					break;
				}
				else if (_array[row][col] == b_pawn) {
					is_open_col = false;
					break;
				}
			}

			//cout << "W col: " << (int)col << " | " << is_open_col << ", isolated pawns: " << s_white[col] << ", total: " << s_white[col] + is_open_col * (open_row_factor - 1) << endl;

			isolated_pawns += isolated_pawn * (s_white[col] + is_open_col * (open_row_factor - 1)) * isolated_adv;
		}

		// Black isolated pawn on the file
		if (s_black[col] > 0 && (col == 0 || s_black[col - 1] == 0) && (col == 7 || s_black[col + 1] == 0)) {
			bool is_open_col = true;

			for (uint8_t row = 0; row < 7; row++) {
				if (_array[row][col] == b_pawn) {
					break;
				}
				else if (_array[row][col] == w_pawn) {
					is_open_col = false;
					break;
				}
			}

			//cout << "B col: " << (int)col << " | " << is_open_col << ", isolated pawns: " << s_black[col] << ", total: " << s_black[col] + is_open_col * (open_row_factor - 1) << endl;

			isolated_pawns -= isolated_pawn * (s_black[col] + is_open_col * (open_row_factor - 1)) * isolated_adv;
		}
	}

	if (display_factor != 0.0f)
		main_GUI._eval_components += "isolated pawns: " + (isolated_pawns >= 0 ? string("+") : string()) + to_string(static_cast<int>(isolated_pawns * display_factor)) + "\n";

	pawn_structure += isolated_pawns;

	// Doubled (or tripled) pawns

	// TODO: less severe when there is activity
	constexpr int doubled_pawn = -85;
	constexpr float doubled_adv_factor = 1.95f; // Depending on how far the game has progressed
	const float doubled_adv = eval_from_progress(1, _adv, doubled_adv_factor);
	float doubled_pawns = 0.0f;

	for (uint8_t i = 0; i < 8; i++) {
		doubled_pawns += (s_white[i] >= 2) * doubled_pawn * (s_white[i] - 1) * doubled_adv;
		doubled_pawns -= (s_black[i] >= 2) * doubled_pawn * (s_black[i] - 1) * doubled_adv;
	}

	if (display_factor != 0.0f)
		main_GUI._eval_components += "doubled pawns: " + (doubled_pawns >= 0 ? string("+") : string()) + to_string(static_cast<int>(doubled_pawns * display_factor)) + "\n";

	pawn_structure += doubled_pawns;

	// Backward pawns
	// A pawn no other pawn can defend, fixed by an enemy pawn controlling the square ahead
	constexpr int backward_pawn = -35;
	constexpr float backward_adv_factor = 0.5f; // Depending on how far the game has progressed
	constexpr float backward_open_factor = 1.5f; // A backward pawn on an open file is much weaker
	constexpr float blocked_factor = 2.0f; // If the pawn is blocked by an enemy piece, or the square ahead is controlled by an enemy pawn
	const float backward_adv = eval_from_progress(1, _adv, backward_adv_factor);
	float backward_pawns = 0.0f;

	// White pawns
	for (uint8_t col = 0; col < 8; col++) {
		for (uint8_t row = 1; row < 7; row++) {
			if (pawns_white[row][col]) {
				bool is_backward = true;
				
				// Is there a potential defender?
				for (int_fast8_t k = row; k >= 1; k--) {
					if (col > 0 && _array[k][col - 1] == w_pawn) {
						is_backward = false;
						break;
					}

					if (col < 7 && _array[k][col + 1] == w_pawn) {
						is_backward = false;
						break;
					}
				}

				// Is the square ahead controlled by an enemy pawn?
				if (is_backward) {
					bool is_controlled = false;

					if (row < 6 && col > 0 && _array[row + 2][col - 1] == b_pawn) {
						is_controlled = true;
					}
					else if (row < 6 && col < 7 && _array[row + 2][col + 1] == b_pawn) {
						is_controlled = true;
					}

					bool is_blocked = is_black(_array[row + 1][col]);

					// On an open file, the backward pawn is much weaker
					bool is_open_col = true;

					for (uint8_t k = 7; k > row; k--) {
						if (_array[k][col] == b_pawn) {
							is_open_col = false;
							break;
						}
					}

					backward_pawns += backward_pawn * backward_adv * ((is_controlled || is_blocked) ? blocked_factor : 1.0f) * (is_open_col ? backward_open_factor : 1.0f);
				}
			}
		}
	}

	// Black pawns
	for (uint8_t col = 0; col < 8; col++) {
		for (uint8_t row = 6; row > 0; row--) {
			if (pawns_black[row][col]) {
				bool is_backward = true;

				// Is there a potential defender?
				for (int_fast8_t k = row; k <= 6; k++) {
					if (col > 0 && _array[k][col - 1] == b_pawn) {
						is_backward = false;
						break;
					}

					if (col < 7 && _array[k][col + 1] == b_pawn) {
						is_backward = false;
						break;
					}
				}

				// Is the square ahead controlled by an enemy pawn?
				if (is_backward) {
					bool is_controlled = false;

					if (row > 1 && col > 0 && _array[row - 2][col - 1] == w_pawn) {
						is_controlled = true;
					}
					else if (row > 1 && col < 7 && _array[row - 2][col + 1] == w_pawn) {
						is_controlled = true;
					}

					bool is_blocked = is_white(_array[row - 1][col]);

					// On an open file, the backward pawn is much weaker
					bool is_open_col = true;

					for (uint8_t k = 0; k < row; k++) {
						if (_array[k][col] == w_pawn) {
							is_open_col = false;
							break;
						}
					}

					backward_pawns -= backward_pawn * backward_adv * ((is_controlled || is_blocked) ? blocked_factor : 1.0f) * (is_open_col ? backward_open_factor : 1.0f);
				}
			}
		}
	}

	if (display_factor != 0.0f)
		main_GUI._eval_components += "backward pawns: " + (backward_pawns >= 0 ? string("+") : string()) + to_string(static_cast<int>(backward_pawns * display_factor)) + "\n";

	pawn_structure += backward_pawns;

	// Passed pawns
	// r5k1/P3Rpp1/7p/8/3p4/8/2P2PPP/6K1 b - - 1 30: position to test
	// 8/P5kp/8/q7/8/8/5B1P/5K2 w - - 1 57: the x-ray controlled square has to be taken into account

	// 8/5bB1/8/5PP1/8/4K3/p7/1k6 w - - 1 9: winning for White; the square is controlled for the other passed pawn

	// Passed pawn value table, indexed by how far the pawn has advanced
	static constexpr int passed_pawns[8] = { 0, 175, 175, 280, 450, 750, 1250, 0 };

	// Divisor per controlling piece (knight, bishop, rook, queen, king)
	//static constexpr float control_division_per_piece[5] = { 1.5f, 1.75f, 1.35f, 1.2f, 1.35f };
	static constexpr float control_division = 1.75f;

	// Divisor per blocking piece (knight, bishop, rook, queen, king)
	static constexpr float block_division_per_piece[5] = { 2.75f, 2.15f, 2.0f, 1.55f, 2.3f };

	// Blocked by a friendly piece
	static constexpr float self_block_division = 1.5f;

	// Bonus for connected passed pawns
	constexpr float connected_passed_pawn_bonus = 1.65f;


	// Passed pawn whose path is controlled by an enemy piece
	//static constexpr int controlled_passed_pawn[8] = { 0, 40, 40, 70, 130, 235, 350, 0 };

	// Blocked passed pawn
	//static constexpr int blocked_passed_pawn[8] = { 0, 20, 30, 55, 110, 200, 300, 0 };


	constexpr float passed_adv_factor = 1.0f; // Depending on how far the game has progressed
	const float passed_adv = eval_from_progress(1, _adv, passed_adv_factor);
	float passed_pawns_value = 0.0f;

	//print_array(s_white, 8);
	//print_array(s_black, 8);

	//Map white_controls_map = get_white_controls_map();
	//Map black_controls_map = get_black_controls_map();

	// Bonus when the king is outside the square of the passed pawn
	constexpr int out_of_square_bonus[8] = { 0, 1000, 1050, 1100, 1200, 1325, 1500, 0 };
	//constexpr int out_of_square_bonus[8] = { 0, 500, 500, 500, 500, 500, 500, 0 };

	// Are we in a pawn endgame?
	bool pawn_endgame = is_pawn_endgame();

	// Does White still have pieces?
	bool has_white_pieces = has_pieces(true);

	// Does Black still have pieces?
	bool has_black_pieces = has_pieces(false);

	// For each file
	for (uint8_t col = 0; col < 8; col++) {

		// Only the most advanced pawn on the file counts, since the others would be stuck behind it

		// White pawns
		if (s_white[col] > 0) {

			// Scan from the rank closest to promotion down to the first
			for (uint8_t row = 6; row > 0; row--) {

				// If there is a potentially passed pawn
				if (pawns_white[row][col]) {

					// No pawn on the same or an adjacent file at a strictly higher rank
					bool is_passed_pawn = true;
					for (uint8_t k = row + 1; k < 7; k++) {
						if ((col > 0 && _array[k][col - 1] == b_pawn) || _array[k][col] == b_pawn || (col < 7 && _array[k][col + 1] == b_pawn)) {
							is_passed_pawn = false;
							break;
						}
					}

					// If this is a passed pawn
					if (is_passed_pawn) {

						// Controls and blocks
						float division_factor = 1.0f;

						// Look for a blocker
						for (uint8_t k = row + 1; k <= 7; k++) {
							const uint8_t blocker = _array[k][col];
							if (blocker == b_pawn) {
								// Enemy pawn: no table entry (knight..king only),
								// and the most permanent blocker (pawns never move
								// backward) -> strongest divisor. Indexing with
								// b_pawn(7)-8 = -1 used to read out of bounds.
								division_factor += block_division_per_piece[0] - 1.0f;
							}
							else if (is_black(blocker)) {
								division_factor += block_division_per_piece[blocker - 8] - 1.0f;
							}
							else if (is_white(blocker)) {
								division_factor += self_block_division - 1.0f;
							}
						}

					// Remove the pawn to test x-ray control over the square
					_array[row][col] = none;

					// The cached control maps still see the removed pawn. A stale map
					// only matters when a rook/queen sits BELOW the pawn with a clear
					// file (the diff loop reads squares above it on the same file):
					// recompute honestly only in that case.
					bool vertical_xray = false;
					for (int j = row - 1; j >= 0; --j) {
						const uint8_t pj = _array[j][col];
						if (pj != none) {
							vertical_xray = (pj == w_rook || pj == w_queen || pj == b_rook || pj == b_queen);
							break;
						}
					}

					if (vertical_xray)
						_controls_map_valid = false;

					SquareMap white_controls_map = get_white_controls_map();
					SquareMap black_controls_map = get_black_controls_map();

					for (uint8_t k = row + 1; k <= 7; k++) {
						int controls_diff = max(0, black_controls_map._array[k][col] - white_controls_map._array[k][col]);
						division_factor += (control_division - 1.0f) * controls_diff;
					}

					// Put the pawn back
					_array[row][col] = w_pawn;

					// Only invalidate when the maps were rebuilt WITHOUT the pawn;
					// otherwise the cache still describes the restored position
					if (vertical_xray)
						_controls_map_valid = false;


						int passed_value = passed_pawns[row] * (!has_black_pieces ? 1.5f : 1.0f);

						// Is it connected to another pawn?
						if ((col > 0 && (pawns_white[row][col - 1] || pawns_white[row - 1][col - 1])) || (col < 7 && (pawns_white[row][col + 1] || pawns_white[row - 1][col + 1]))) {
							passed_value *= connected_passed_pawn_bonus;
						}

						// Pawn endgame -> is the king inside the square of the passed pawn?
						bool out_of_square = !has_black_pieces && !in_king_square(Pos(row, col), false);

						//cout << "Passed pawn: " << square_name(row, col) << ", Is pawn endgame: " << pawn_endgame << ", Out of square: " << out_of_square << ", bonus: " << out_of_square * out_of_square_bonus[row] << endl;

						// Add the passed pawn value
						passed_pawns_value += (passed_value / division_factor + out_of_square * out_of_square_bonus[row]) * passed_adv;
						//cout << "Passed pawn: " << square_name(row, col) << ", Value: " << (passed_value / division_factor + out_of_square * out_of_square_bonus[row]) * passed_adv << " (passed_value: " << passed_value << ", division_factor: " << division_factor << ", out_of_square bonus: " << out_of_square * out_of_square_bonus[row] << ") * passed_adv: " << passed_adv << endl;

						// Only the most advanced pawn on the file counts: the ones behind it are stuck
						break;
					}

				}
			}
		}

		// rnb2rk1/p5pp/3qp3/1P1p4/3P1p2/4PP2/1pPBB1PP/R2Q1RK1 w - - 0 16

		// Black pawns
		if (s_black[col] > 0) {

			// Scan from the rank closest to promotion down to the first
			for (uint8_t row = 1; row < 7; row++) {

				// If there is a potentially passed pawn
				if (pawns_black[row][col]) {

					// No pawn on the same or an adjacent file at a strictly higher rank
					bool is_passed_pawn = true;
					for (uint8_t k = row - 1; k > 0; k--) {
						if ((col > 0 && _array[k][col - 1] == w_pawn) || _array[k][col] == w_pawn || (col < 7 && _array[k][col + 1] == w_pawn)) {
							is_passed_pawn = false;
							break;
						}
					}

					// If this is a passed pawn
					if (is_passed_pawn) {

						// Controls and blocks
						float division_factor = 1.0f;

						// Look for a blocker
						for (int_fast8_t k = row - 1; k >= 0; k--) {
							const uint8_t blocker = _array[k][col];
							if (blocker == w_pawn) {
								// Mirror of the white side: enemy pawn has no
								// table entry; w_pawn(1)-2 = -1 used to read
								// out of bounds.
								division_factor += block_division_per_piece[0] - 1.0f;
							}
							else if (is_white(blocker)) {
								division_factor += block_division_per_piece[blocker - 2] - 1.0f;
							}
							else if (is_black(blocker)) {
								division_factor += self_block_division - 1.0f;
							}
						}

					// Remove the pawn to test x-ray control over the square
					_array[row][col] = none;

					// Mirror of the white side: only a rook/queen ABOVE with a clear
					// file can x-ray the squares read below
					bool vertical_xray = false;
					for (int j = row + 1; j < 8; ++j) {
						const uint8_t pj = _array[j][col];
						if (pj != none) {
							vertical_xray = (pj == w_rook || pj == w_queen || pj == b_rook || pj == b_queen);
							break;
						}
					}

					if (vertical_xray)
						_controls_map_valid = false;

					SquareMap white_controls_map = get_white_controls_map();
					SquareMap black_controls_map = get_black_controls_map();

					for (int_fast8_t k = row - 1; k >= 0; k--) {
						int controls_diff = max(0, white_controls_map._array[k][col] - black_controls_map._array[k][col]);
						division_factor += (control_division - 1.0f) * controls_diff;
					}

					// Put the pawn back
					_array[row][col] = b_pawn;

					// Only invalidate when the maps were rebuilt WITHOUT the pawn;
					// otherwise the cache still describes the restored position
					if (vertical_xray)
						_controls_map_valid = false;

						int passed_value = passed_pawns[7 - row] * (!has_white_pieces ? 1.5f : 1.0f);

						// Is it connected to another pawn?
						if ((col > 0 && (pawns_black[row][col - 1] || pawns_black[row + 1][col - 1])) || (col < 7 && (pawns_black[row][col + 1] || pawns_black[row + 1][col + 1]))) {
							passed_value *= connected_passed_pawn_bonus;
						}

						//8/8/4p2p/1R6/pPpP1k2/K6P/8/8 b - - 0 43
						//cout << "Passed pawn: " << square_name(row, col) << " (" << passed_value << " ) | " << division_factor << ": " << passed_value / division_factor * passed_adv << endl;

						// Pawn endgame -> is the king inside the square of the passed pawn?
						bool out_of_square = !has_white_pieces && !in_king_square(Pos(row, col), true);

						//cout << "Passed pawn: " << square_name(row, col) << ", Is pawn endgame: " << pawn_endgame << ", Out of square: " << out_of_square << ", bonus: " << out_of_square * out_of_square_bonus[7 - row] << endl;

						// 8/8/7P/6p1/pP1p4/P7/3Kpk2/8 w - - 2 8

						// 8/8/8/8/8/1p5P/p5k1/K7 w - - 0 54

						// Add the passed pawn value
						passed_pawns_value -= (passed_value / division_factor + out_of_square * out_of_square_bonus[7 - row]) * passed_adv;
						//cout << "Passed pawn: " << square_name(row, col) << ", Value: " << -(passed_value / division_factor + out_of_square * out_of_square_bonus[7 - row]) * passed_adv << " (passed_value: " << passed_value << ", division_factor: " << division_factor << ", out_of_square bonus: " << out_of_square * out_of_square_bonus[7 - row] << ") * passed_adv: " << passed_adv << endl;

						// Only the most advanced pawn on the file counts: the ones behind it are stuck
						break;
					}

				}
			}
		}
	}

	//cout << "Passed pawns total value: " << passed_pawns_value << endl;

	if (display_factor != 0.0f)
		main_GUI._eval_components += "passed pawns: " + (passed_pawns_value >= 0 ? string("+") : string()) + to_string(static_cast<int>(passed_pawns_value * display_factor)) + "\n";

	pawn_structure += passed_pawns_value;


	// Connected pawns
	// A pawn is connected when a same-coloured pawn sits on an adjacent file, on the same or the lower rank
	constexpr int connected_pawns[8] = { 0, 10, 65, 135, 150, 200, 275, 0 };
	constexpr float connected_pawns_adv_factor = 0.2f; // Depending on how far the game has progressed
	const float connected_pawns_adv = eval_from_progress(1, _adv, connected_pawns_adv_factor);
	constexpr float column_connection_value[8] = { 0.15f, 0.25f, 0.55f, 1.25f, 1.25f, 0.55f, 0.25f, 0.15f };
	constexpr float connected_pawns_factor = 0.85f;

	float connected_pawns_value = 0.0f;

	constexpr float multiple_connections[5] = { 0.0, 1.0f, 1.25f, 1.35f, 1.4f }; // Bonus when the pawn is connected on several sides

	// TODO: to favour "A" structures over "V" ones:
	// Multiple connection+?
	// Should connected pawns be stronger in the centre?
	// Backward pawns, more heavily weighted

	// Bounds-checked pawn-map accessors for the contested/connection probes
	// below: several of them index col +/- 2 and row +/- 2 under guards that
	// do not always cover BOTH indices (pawns_black[row + 2][col + 2] was
	// reached with col=6 -> flat offset 64, one past the 64-byte array -
	// ASAN stack-buffer-overflow in Puzzle.Wac001QueenG6). Off-board means
	// "no pawn there", i.e. false, which is also the correct semantic for
	// every caller (contest/backing checks treat it as absent).
	auto pw = [&](int r, int c) -> bool {
		return r >= 0 && r < 8 && c >= 0 && c < 8 && pawns_white[r][c];
	};
	auto pb = [&](int r, int c) -> bool {
		return r >= 0 && r < 8 && c >= 0 && c < 8 && pawns_black[r][c];
	};

	// For each file
	for (uint8_t col = 0; col < 8; col++) {
		for (uint8_t row = 1; row < 7; row++) {

			// White pawns
			if (pw(row, col)) {

				// Connection through the left file

				// Pawn connected behind
				bool is_left_connected_behind = (col > 0 && pw(row - 1, col - 1));

			// Contested by an enemy pawn and not backed by a friendly pawn: drop the bonus
			//rnbqkbnr/pp3ppp/4p3/2ppP3/3P4/8/PPP2PPP/RNBQKBNR w KQkq - 0 4: c3 is the move, to indirectly reconnect e5
			// (row < 2 guards: no backing pawn can exist off-board -> treat as unbacked)
			if (is_left_connected_behind && (col > 1 && pb(row, col - 2)) && (row < 2 || !pw(row - 2, col - 2)) && (row < 2 || !pw(row - 2, col))) {
				is_left_connected_behind = false;
			}

				// Pawn connected on the same rank
				bool is_left_connected_side = (col > 0 && pw(row, col - 1));

			// Contested by an enemy pawn and not backed by a friendly pawn: drop the bonus
			if (is_left_connected_side && (col > 1 && pb(row + 1, col - 2) || pb(row + 1, col)) && (row < 2 || !pw(row - 2, col - 2)) && (row < 2 || !pw(row - 2, col))) {
				is_left_connected_side = false;
			}


				// Connection through the right file
				
				// Pawn connected behind
				bool is_right_connected_behind = (col < 7 && pw(row - 1, col + 1));

			// Contested by an enemy pawn and not backed by a friendly pawn: drop the bonus
			if (is_right_connected_behind && (col < 6 && pb(row, col + 2)) && (row < 2 || !pw(row - 2, col + 2)) && (row < 2 || !pw(row - 2, col))) {
				is_right_connected_behind = false;
			}

				// Pawn connected on the same rank
				bool is_right_connected_side = (col < 7 && pw(row, col + 1));

			// Contested by an enemy pawn and not backed by a friendly pawn: drop the bonus
			if (is_right_connected_side && (col < 6 && pb(row + 1, col + 2) || pb(row + 1, col)) && (row < 2 || !pw(row - 2, col + 2)) && (row < 2 || !pw(row - 2, col))) {
				is_right_connected_side = false;
			}

				int behind_connections = is_left_connected_behind + is_right_connected_behind;
				int side_connections = is_left_connected_side + is_right_connected_side;

				// If connected on at least one side
				if (behind_connections + side_connections > 0) {

					bool is_contested = (col > 0 && pb(row + 1, col - 1)) || (col < 7 && pb(row + 1, col + 1));
					float value = connected_pawns[row] * connected_pawns_adv * column_connection_value[col] * multiple_connections[behind_connections + side_connections - is_contested];
					connected_pawns_value += value;
					//std::cout << square_name(row, col) << ", behind: " << behind_connections << ", side: " << side_connections << ", contests: " << is_contested << " = " << behind_connections + side_connections - is_contested << ": " << value << endl;
					//rnbqkb1r/pp1n2pp/4pp2/2ppP3/3P1P2/2NB1N2/PPP3PP/R1BQK2R b KQkq - 1 7: confirm that c4 really is strategically bad (it reconnects e5)
				}
			}

			// Black pawns
			else if (pb(row, col)) {

				// Connection through the left file
				
				// Pawn connected behind
				bool is_left_connected_behind = (col > 0 && pb(row + 1, col - 1));

			// Contested by an enemy pawn and not backed by a friendly pawn: drop the bonus
			//rnbqkb1r/ppp2ppp/5n2/3p4/2PPp3/4P3/PP3PPP/RNBQKBNR b KQkq - 0 5: c6 is the move
			// (row > 5 guards: no backing pawn can exist off-board -> treat as unbacked)
			if (is_left_connected_behind && (col > 1 && pw(row, col - 2) && (row > 5 || !pb(row + 2, col - 2)) && (row > 5 || !pb(row + 2, col)))) {
				is_left_connected_behind = false;
			}

				// Pawn connected on the same rank
				bool is_left_connected_side = (col > 0 && pb(row, col - 1));

			// Contested by an enemy pawn and not backed by a friendly pawn: drop the bonus
			if (is_left_connected_side && (col > 1 && pw(row - 1, col - 2) || pw(row - 1, col)) && (row > 5 || !pb(row + 2, col - 2)) && (row > 5 || !pb(row + 2, col))) {
				is_left_connected_side = false;
			}

				// Connection through the right file
				
				// Pawn connected behind
				bool is_right_connected_behind = (col < 7 && pb(row + 1, col + 1));

			// Contested by an enemy pawn and not backed by a friendly pawn: drop the bonus
			if (is_right_connected_behind && (col < 6 && pw(row, col + 2) && (row > 5 || !pb(row + 2, col + 2)) && (row > 5 || !pb(row + 2, col)))) {
				is_right_connected_behind = false;
			}

				// Pawn connected on the same rank
				bool is_right_connected_side = (col < 7 && pb(row, col + 1));

			// Contested by an enemy pawn and not backed by a friendly pawn: drop the bonus
			if (is_right_connected_side && (col < 6 && pw(row - 1, col + 2) || pw(row - 1, col)) && (row > 5 || !pb(row + 2, col + 2)) && (row > 5 || !pb(row + 2, col))) {
				is_right_connected_side = false;
			}

				int behind_connections = is_left_connected_behind + is_right_connected_behind;
				int side_connections = is_left_connected_side + is_right_connected_side;

				// If connected on at least one side
				if (behind_connections + side_connections > 0) {

					bool is_contested = col > 0 && pw(row - 1, col - 1) || col < 7 && pw(row - 1, col + 1);
					float value = connected_pawns[7 - row] * connected_pawns_adv * column_connection_value[col] * multiple_connections[behind_connections + side_connections - is_contested];
					connected_pawns_value -= value;
					//std::cout << square_name(row, col) << ", behind: " << behind_connections << ", side: " << side_connections << ", contests: " << is_contested << " = " << behind_connections + side_connections - is_contested << ": " << value << endl;
				}
			}
		}
	}

	if (display_factor != 0.0f)
		main_GUI._eval_components += "connected pawns: " + (connected_pawns_value >= 0 ? string("+") : string()) + to_string(static_cast<int>(connected_pawns_value * display_factor)) + "\n";

	pawn_structure += connected_pawns_value * connected_pawns_factor;

	// TODO: the per-progress multipliers should be applied at the end

	// We still need to check whether the enemy pawn is in front of or behind the other pawn

	return pawn_structure;
}

// Returns how long the engine should spend on the next move, in ms, from a factor k and the remaining clocks

// Computes and returns the net attack/defence balance
int Board::get_pawn_push_threats() const {

	// 1r3r2/1nqb2bk/3p2pp/3Ppp2/p1P5/2BB1N1P/3Q1PP1/R3R1K1 w - - 0 29: e4 is a serious threat
	// r4rk1/qp1bbppp/p3pn2/2Pp4/PP1Q1B2/2P2N2/3N1PPP/R3K2R w KQ - 3 15: neither e5 nor g5 is really playable here

	// For each pawn, look for pieces that would come under attack if the pawn advanced
	// The push must be unobstructed, by a piece or by enemy control
	// TODO: improve the control handling; as it stands a bishop is never threatened, even with the pawn protected
	
	SquareMap white_controls = get_white_controls_map();
	SquareMap black_controls = get_black_controls_map();

	int w_threats = 0;
	int b_threats = 0;

	for (uint8_t row = 1; row < 7; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			const uint8_t p = _array[row][col];

			// White pawn
			if (p == w_pawn && row < 6) {

				// Can the pawn advance safely, and by how many squares?
				int safe_push = 0;

				// If the square ahead is empty and not controlled by an enemy pawn, and (when controlled by the opponent) also controlled by a friendly pawn
				bool can_push = _array[row + 1][col] == none;
				bool opponent_pawn_control = (col > 0 && _array[row + 2][col - 1] == b_pawn) || (col < 7 && _array[row + 2][col + 1] == b_pawn);
				bool opponent_control = black_controls._array[row + 1][col] > 0;
				bool ally_pawn_control = (col > 0 && _array[row][col - 1] == w_pawn) || (col < 7 && _array[row][col + 1] == w_pawn);

				if (can_push && !opponent_pawn_control && (!opponent_control || ally_pawn_control)) {
					safe_push = 1;

					// Double push
					bool can_double_push = row == 1 && _array[row + 2][col] == none;
					bool opponent_pawn_control_double = (col > 0 && _array[row + 3][col - 1] == b_pawn) || (col < 7 && _array[row + 3][col + 1] == b_pawn);
					bool opponent_control_double = black_controls._array[row + 2][col] > 0;
					bool ally_pawn_control_double = (col > 0 && _array[row + 1][col - 1] == w_pawn) || (col < 7 && _array[row + 1][col + 1] == w_pawn);

					if (can_double_push && !opponent_pawn_control_double && (!opponent_control_double || ally_pawn_control_double)) {
						safe_push = 2;
					}
				}

				// Do the pushes threaten any piece?
				if (safe_push > 0) {
					if (col > 0 && is_black(_array[row + 2][col - 1])) {
						w_threats++;
					}
					if (col < 7 && is_black(_array[row + 2][col + 1])) {
						w_threats++;
					}

					if (safe_push > 1) {
						if (col > 0 && is_black(_array[row + 3][col - 1])) {
							w_threats++;
						}
						if (col < 7 && is_black(_array[row + 3][col + 1])) {
							w_threats++;
						}
					}
				}
			}

			// Black pawn
			else if (p == b_pawn && row > 1) {

				// Can the pawn advance safely, and by how many squares?
				int safe_push = 0;

				// If the square ahead is empty and not controlled by an enemy pawn, and (when controlled by the opponent) also controlled by a friendly pawn
				bool can_push = _array[row - 1][col] == none;
				bool opponent_pawn_control = (col > 0 && _array[row - 2][col - 1] == w_pawn) || (col < 7 && _array[row - 2][col + 1] == w_pawn);
				bool opponent_control = white_controls._array[row - 1][col] > 0;
				bool ally_pawn_control = (col > 0 && _array[row][col - 1] == b_pawn) || (col < 7 && _array[row][col + 1] == b_pawn);

				if (can_push && !opponent_pawn_control && (!opponent_control || ally_pawn_control)) {
					safe_push = 1;

					// Double push
					bool can_double_push = row == 6 && _array[row - 2][col] == none;
					bool opponent_pawn_control_double = (col > 0 && _array[row - 3][col - 1] == w_pawn) || (col < 7 && _array[row - 3][col + 1] == w_pawn);
					bool opponent_control_double = white_controls._array[row - 2][col] > 0;
					bool ally_pawn_control_double = (col > 0 && _array[row - 1][col - 1] == b_pawn) || (col < 7 && _array[row - 1][col + 1] == b_pawn);

					if (can_double_push && !opponent_pawn_control_double && (!opponent_control_double || ally_pawn_control_double)) {
						safe_push = 2;
					}
				}

				// Do the pushes threaten any piece?
				if (safe_push > 0) {
					if (col > 0 && is_white(_array[row - 2][col - 1])) {
						b_threats++;
					}
					if (col < 7 && is_white(_array[row - 2][col + 1])) {
						b_threats++;
					}

					if (safe_push > 1) {
						if (col > 0 && is_white(_array[row - 3][col - 1])) {
							b_threats++;
						}
						if (col < 7 && is_white(_array[row - 3][col + 1])) {
							b_threats++;
						}
					}
				}
			}
		}
	}

	// Based on game progress
	constexpr float advancement_factor = 0.25f;

	return eval_from_progress(100 * (w_threats - b_threats), _adv, advancement_factor);
}

// Computes and returns the king's proximity to the pawns
int Board::get_bishop_pawns() const {

	// 3rr3/1kpn1p1p/p1p1qbb1/2Pp2p1/QP1Pp3/P1N1P1NP/3B1PP1/1R2R1K1 b - - 0 24: evaluation is not symmetric

	// Add a bonus/penalty depending on the colour of the opposing pawns too?
	float ally_bishop_pawn_malus = 1.0f;
	float enemy_bishop_pawn_bonus = 1.0f * (1.0f - _adv); // En fin de partie, on veut pouvoir attaquer les pions adverses
	// r1b1k2r/1p1n1p2/p1pBp1pp/2Pp1q1n/PP1P4/4P3/3N1PPP/R2Q1RK1 w kq - 0 5

	// White pawns on light squares
	int white_pawns_w = 0;

	// White pawns on dark squares
	int white_pawns_b = 0;

	// Black pawns on light squares
	int black_pawns_w = 0;

	// Black pawns on dark squares
	int black_pawns_b = 0;

	// Number of white pawns blocked on the central files (C, D, E, F)
	int white_central_pawns_blocked = 0;

	// Number of black pawns blocked on the central files (C, D, E, F)
	int black_central_pawns_blocked = 0;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			const uint8_t p = _array[row][col];

			// White pawns
			if (p == w_pawn) {
				if ((row + col) % 2)
					white_pawns_w++;
				else
					white_pawns_b++;

				if (_array[row + 1][col] != none && is_in(col, 2, 5))
					white_central_pawns_blocked++;
			}

			// Black pawns
			else if (p == b_pawn) {
				if ((row + col) % 2)
					black_pawns_w++;
				else
					black_pawns_b++;

				if (_array[row - 1][col] != none && is_in(col, 2, 5))
					black_central_pawns_blocked++;
			}
		}
	}

	//cout << "white_pawns_w: " << white_pawns_w << endl;
	//cout << "white_pawns_b: " << white_pawns_b << endl;
	//cout << "black_pawns_w: " << black_pawns_w << endl;
	//cout << "black_pawns_b: " << black_pawns_b << endl;
	//cout << "white_pawns_blocked: " << white_central_pawns_blocked << endl;
	//cout << "black_pawns_blocked: " << black_central_pawns_blocked << endl;

	float bishop_pawns_value = 0.0f;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			const uint8_t p = _array[row][col];

			// White bishop
			if (p == w_bishop) {
				if ((row + col) % 2) // Light square
					bishop_pawns_value -= (white_pawns_w - white_pawns_b) * (2 + white_central_pawns_blocked) * ally_bishop_pawn_malus + (black_pawns_w - black_pawns_b) * enemy_bishop_pawn_bonus;
				else // Dark square
					bishop_pawns_value -= (white_pawns_b - white_pawns_w) * (2 + white_central_pawns_blocked) * ally_bishop_pawn_malus + (black_pawns_b - black_pawns_w) * enemy_bishop_pawn_bonus;
			}

			// Black bishop
			else if (p == b_bishop) {
				if ((row + col) % 2) // Light square
					bishop_pawns_value += (black_pawns_w - black_pawns_b) * (2 + black_central_pawns_blocked) * ally_bishop_pawn_malus + (white_pawns_w - white_pawns_b) * enemy_bishop_pawn_bonus;
				else // Dark square
					bishop_pawns_value += (black_pawns_b - black_pawns_w) * (2 + black_central_pawns_blocked) * ally_bishop_pawn_malus + (white_pawns_b - white_pawns_w) * enemy_bishop_pawn_bonus;
			}
		}
	}

	//cout << "bishop_pawns_value: " << bishop_pawns_value << endl;

	// Multiplier based on how far the game has progressed
	float advancement_factor = 0.8f;

	return eval_from_progress(bishop_pawns_value, _adv, advancement_factor);
}

// Returns the long-term weakness value of the pawn shield
int Board::get_pawn_shield() {
	// Prendre en compte:
	// - pawns in front of the king: done
	// - TODO: semi-open files in front of the king
	// - TODO: penalties for doubled or isolated pawns in front of the king
	// - open files/diagonals: TODO

	// castled (can no longer castle): look at the 3 pawns in front of it
	// can castle on one side only: look at pawns f, g, h or b, c, d depending on the side
	// can castle both sides: average the f/g/h set, the b/c/d set and the 3 pawns in front of it

	// Exemples:
	// r3k2r/1ppq2pp/p1pbbpn1/8/3PP3/2N1BN1P/PP3PP1/R2Q1RK1 w kq - 3 13: h4 wrecks the king structure here. Never play it, especially with opposite castling


	int pawn_shield_value = 0;

	update_kings_pos();

	// White king

	// pawns f, g and h
	int w_kingside_pawns = 25 * ((_array[1][5] == 1) + (_array[1][6] == 1) + (_array[1][7] == 1)) + 15 * ((_array[2][5] == 1) + (_array[2][6] == 1) + (_array[2][7] == 1)) + 5 * ((_array[3][5] == 1) + (_array[3][6] == 1) + (_array[3][7] == 1));

	// pawns b, c and d
	int w_queenside_pawns = 25 * ((_array[1][1] == 1) + (_array[1][2] == 1) + (_array[1][3] == 1)) + 15 * ((_array[2][1] == 1) + (_array[2][2] == 1) + (_array[2][3] == 1)) + 5 * ((_array[3][1] == 1) + (_array[3][2] == 1) + (_array[3][3] == 1));

	// pawns in front of the king
	int w_front_pawns = 25 * (_array[1][_white_king_pos.col] == 1) + 15 * (_array[2][_white_king_pos.col] == 1) + 5 * (_array[3][_white_king_pos.col] == 1);
	if (_white_king_pos.col > 0)
		w_front_pawns += 25 * (_array[1][_white_king_pos.col - 1] == 1) + 15 * (_array[2][_white_king_pos.col - 1] == 1) + 5 * (_array[3][_white_king_pos.col - 1] == 1);
	if (_white_king_pos.col < 7)
		w_front_pawns += 25 * (_array[1][_white_king_pos.col + 1] == 1) + 15 * (_array[2][_white_king_pos.col + 1] == 1) + 5 * (_array[3][_white_king_pos.col + 1] == 1);

	int w_castles_count = _castling_rights.k_w + _castling_rights.q_w;

	int w_pawn_shield_value = (w_front_pawns + _castling_rights.k_w * w_kingside_pawns + _castling_rights.q_w * w_queenside_pawns) / (1 + w_castles_count);


	// Black king

	// pawns f, g and h
	int b_kingside_pawns = 25 * ((_array[6][5] == 7) + (_array[6][6] == 7) + (_array[6][7] == 7)) + 15 * ((_array[5][5] == 7) + (_array[5][6] == 7) + (_array[5][7] == 7)) + 5 * ((_array[4][5] == 7) + (_array[4][6] == 7) + (_array[4][7] == 7));

	// pawns b, c and d
	int b_queenside_pawns = 25 * ((_array[6][1] == 7) + (_array[6][2] == 7) + (_array[6][3] == 7)) + 15 * ((_array[5][1] == 7) + (_array[5][2] == 7) + (_array[5][3] == 7)) + 5 * ((_array[4][1] == 7) + (_array[4][2] == 7) + (_array[4][3] == 7));

	// pawns in front of the king
	int b_front_pawns = 25 * (_array[6][_black_king_pos.col] == 7) + 15 * (_array[5][_black_king_pos.col] == 7) + 5 * (_array[4][_black_king_pos.col] == 7);
	if (_black_king_pos.col > 0)
		b_front_pawns += 25 * (_array[6][_black_king_pos.col - 1] == 7) + 15 * (_array[5][_black_king_pos.col - 1] == 7) + 5 * (_array[4][_black_king_pos.col - 1] == 7);
	if (_black_king_pos.col < 7)
		b_front_pawns += 25 * (_array[6][_black_king_pos.col + 1] == 7) + 15 * (_array[5][_black_king_pos.col + 1] == 7) + 5 * (_array[4][_black_king_pos.col + 1] == 7);

	int b_castles_count = _castling_rights.k_b + _castling_rights.q_b;

	int b_pawn_shield_value = (b_front_pawns + _castling_rights.k_b * b_kingside_pawns + _castling_rights.q_b * b_queenside_pawns) / (1 + b_castles_count);

	// Holes on the files


	// Compute the pawn shield value
	pawn_shield_value = w_pawn_shield_value - b_pawn_shield_value;

	// Game progress value beyond which this stops mattering, decreasing linearly
	float pawn_shield_advancement_threshold = 0.7f;

	return eval_from_progress(pawn_shield_value, _adv, pawn_shield_advancement_threshold);
}

// Returns the value of the weak squares
int Board::get_pawn_storm_at_col(bool color, uint8_t king_row, uint8_t king_col) const {
	// FIXME: should there be a bonus once the pawn is past the enemy king? That effectively puts the king on an "open" file without it truly being open
	// FIXME: does an enemy piece block the pawn storm, or only pawns?

	// 3rr3/1kpn1p1p/p1p1qbb1/2Pp2p1/QP1Pp3/P1N1P1NP/3B1PP1/1R2R1K1 b - - 0 24
	// 1k1r2nr/p7/Pqpb2p1/4p2p/5p2/2NP4/1PP1QPKP/R1B1R3 b - - 0 22 : +333??

	// r2qk2r/pp1n1ppp/3bpn2/2pp4/3P4/2N1PN2/PPP2PPP/R1BQ1RK1 b kq - 3 8: castling comes next move, so this is not really a storm

	// 2k1rb1r/p1ppqppp/bnp5/4P3/2P5/1P6/PQ2BPPP/RNB1K2R w KQ - 5 12: there is a storm, but c5 has no point of contact

	// Bonus depending on the vertical distance between the pawns and the king
	int bonus[7] = { 85, 75, 55, 38, 21, 0, 0};

	// Bonus for the pawns on a nearly adjacent file
	float semi_adjacent_bonus = 0.25f;

	int total_bonus = 0;

	// Look at the three files adjacent to the king
	for (int_fast8_t col = king_col - 2; col <= king_col + 2; col++) {
		if (col < 0 || col > 7) {
			continue;
		}

		for (uint8_t row = 1; row < 7; row++) {

			// If there is a friendly pawn
			if (_array[row][col] == (color ? w_pawn : b_pawn)) {
				//cout << (int)i << ", " << opponent_king_pos.i << endl;
				//cout << abs(i - opponent_king_pos.i) << endl;

				// If no enemy pawn blocks it
				int dy = (color ? 1 : -1);

				uint8_t p = _array[row + dy][col];

				bool blocked = p == w_pawn || p == b_pawn;
				//cout << "piece : " << (int)p << endl;

				// Make sure nothing frees the pawn, such as an available capture
				if (blocked) {
					if (col > 1) {
						uint8_t p1 = _array[row + dy][col - 1];
						if (color && is_black(p1) || !color && is_white(p1)) {
							blocked = false;
						}
					}

					if (col < 7) {
						uint8_t p2 = _array[row + dy][col + 1];
						if (color && is_black(p2) || !color && is_white(p2)) {
							blocked = false;
						}
					}
				}

				//cout << square_name(row, col) << ", blocked: " << blocked << endl;

				// 5r2/5pk1/r1pp1qn1/4p2p/4P3/2P1QpB1/BP2b1PP/R5RK w - - 6 29: is f3 counted as a semi storm?
				//1rbq1r2/2p2pk1/p2p1nn1/4p1N1/p3P2p/2PPP3/RPB3PP/3QBRK1 b - - 1 7: h3 makes a storm here

				// FIXME??
				if (!blocked) { // In theory the index stays within [0, 7], since pawns cannot sit on the outer ranks
					total_bonus += bonus[abs(row - king_row)] * (abs(col - king_col) < 2 ? 1.0f : semi_adjacent_bonus);
				}
			}
		}
	}

	// Based on game progress
	float pawn_storm_advancement_factor = 0.0f;

	return eval_from_progress(total_bonus, _adv, pawn_storm_advancement_factor);
}

// Returns the shielding power of the king's pawn structure
int Board::get_pawn_storm(bool color) {

	// King position
	update_kings_pos();

	// TODO: handle the uncastled case: estimate the possible post-castling squares (file c or g) and derive the shielding power of the pawn structure from them

	Pos opponent_king_pos = color ? _black_king_pos : _white_king_pos;

	bool can_kingside_castle = color ? _castling_rights.k_w : _castling_rights.k_b;
	bool can_queenside_castle = color ? _castling_rights.q_w : _castling_rights.q_b;

	const int main_malus = get_pawn_storm_at_col(color, opponent_king_pos.row, opponent_king_pos.col);
	const int kingside_malus = can_kingside_castle ? get_pawn_storm_at_col(color, opponent_king_pos.row, 6) : 0;
	const int queenside_malus = can_queenside_castle ? get_pawn_storm_at_col(color, opponent_king_pos.row, 2) : 0;

	//cout << "main malus: " << main_malus << ", kingside malus: " << kingside_malus << ", queenside malus: " << queenside_malus << endl;

	int total_bonus = get_long_term_king_weakness(!color, main_malus, kingside_malus, queenside_malus);

	return total_bonus;
}

// Returns the name of the square
int Board::get_knight_activity() const {

	// Bonus per controlled square
	constexpr int control_bonus[8][8] = 
	{	{ 3, 4, 6, 6, 6, 6, 4, 3 },
		{ 4, 6, 8,10,10, 8, 6, 4 },
		{ 5, 7,10,12,12,10, 7, 5 },
		{ 4, 6, 8,10,10, 8, 6, 4 },
		{ 3, 5, 6, 8, 8, 6, 5, 3 },
		{ 2, 3, 4, 5, 5, 4, 3, 2 },
		{ 1, 2, 3, 4, 4, 3, 2, 1 },
		{ 1, 2, 2, 3, 3, 2, 2, 1 } };

	int white_knight_bonus = 0;
	int black_knight_bonus = 0;

	// Activity threshold above which it counts as positive, renormalising the activity
	constexpr int knight_activity_base = 20;

	for (uint8_t i = 0; i < 8; i++) {
		for (uint8_t j = 0; j < 8; j++) {

			if (_array[i][j] == w_knight) {
				white_knight_bonus -= knight_activity_base;
				for (uint8_t m = 0; m < 8; m++) {
					int new_i = i + knight_directions[m][0];
					int new_j = j + knight_directions[m][1];

					if (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						white_knight_bonus += control_bonus[7 - new_i][new_j];
					}
				}
			}

			if (_array[i][j] == b_knight) {
				black_knight_bonus -= knight_activity_base;
				for (uint8_t m = 0; m < 8; m++) {
					int new_i = i + knight_directions[m][0];
					int new_j = j + knight_directions[m][1];

					if (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						black_knight_bonus += control_bonus[new_i][new_j];
					}
				}
			}
		}
	}

	// Multiplier based on game progress
	float knight_activity_advancement_factor = 0.5f;

	return eval_from_progress(white_knight_bonus - black_knight_bonus, _adv, knight_activity_advancement_factor);
}

// Returns the shielding power of the king's pawn structure
int Board::get_pawn_shield_protection(bool color, float opponent_attacking_potential, int space) {

	// King position
	update_kings_pos();

	// TODO: handle the uncastled case: estimate the possible post-castling squares (file c or g) and derive the shielding power of the pawn structure from them

	Pos king_pos = color ? _white_king_pos : _black_king_pos;
	
	bool can_kingside_castle = color ? _castling_rights.k_w : _castling_rights.k_b;
	bool can_queenside_castle = color ? _castling_rights.q_w : _castling_rights.q_b;

	const int main_bonus = get_pawn_shield_protection_at_column(color, king_pos.col, opponent_attacking_potential, true, space);
	const int kingside_bonus = can_kingside_castle ? get_pawn_shield_protection_at_column(color, 6, opponent_attacking_potential, true, space) : 0;
	const int queenside_bonus = can_queenside_castle ? get_pawn_shield_protection_at_column(color, 2, opponent_attacking_potential, true, space) : 0;

	// 4k1r1/2pp4/1p2pq2/6r1/p2P2Pp/2PB1b1P/PPQ5/R4RK1 b - g3 0 24
	// r3k2r/pppnbppp/2n5/3N1q2/5B2/8/PPPQBP1P/1K1R2R1 b kq - 2 15
	// rnb1kb1r/ppp2ppp/5n2/3qp3/8/5N2/PPPPBPPP/RNBQK2R w KQkq - 0 5

	//cout << "main: " << main_bonus << ", kingside: " << kingside_bonus << ", queenside: " << queenside_bonus << endl;

	// TODO: take the distances to castling into account
	// FIXME: not ideal, since only the structure is considered when judging whether castling is a good idea

	//uint8_t king_row = color ? 0 : 7;
	//uint8_t above_row = color ? 1 : 6;
	//uint8_t blocking_bishop = color ? w_bishop : b_bishop;

	//// Distance to kingside castling
	//uint8_t kingside_castle_distance = 0;

	//if (can_kingside_castle) {
	//	kingside_castle_distance = (_array[king_row][5] != none) + (_array[king_row][6] != none)
	//		+ (_array[king_row][5] == blocking_bishop && _array[above_row][4] != none && _array[above_row][6] != none)
	//		+ is_controlled(king_row, 5, color) + is_controlled(king_row, 6, color);
	//}

	//// Distance to queenside castling
	//uint8_t queenside_castle_distance = 0;

	//if (can_queenside_castle) {
	//	queenside_castle_distance = (_array[king_row][3] != none) + (_array[king_row][2] != none) + (_array[king_row][1] != none)
	//		+ (_array[king_row][2] == blocking_bishop && _array[above_row][1] != none && _array[above_row][3] != none)
	//		+ is_controlled(king_row, 2, color) + is_controlled(king_row, 3, color);
	//}
	//
	////cout << "kingside: " << (int)kingside_castle_distance << ", queenside: " << (int)queenside_castle_distance << endl;

	//// TODO: could be improved when pieces other than the original bishop block the castle

	//// TOTAL: main / C + K / (Dk + c) + Q / (Dq + c)

	//int total_bonus = main_bonus;

	//// Potential gain from castling
	//const int kingside_bonus_diff = max(0, kingside_bonus - main_bonus);
	//const int queenside_bonus_diff = max(0, queenside_bonus - main_bonus);
	//
	//// FIXME: fairly arbitrary. 0.9 when castling is available, decaying linearly to 0 otherwise, assuming the distance never exceeds 8
	//// Plenty of room to improve this
	//double kingside_castling_factor = can_kingside_castle ? 1.0 - (2.0 + kingside_castle_distance) / 20.0 : 0.0;
	//double queenside_castling_factor = can_queenside_castle ? 1.0 - (2.0 + queenside_castle_distance) / 20.0 : 0.0;

	//int total_kingside_bonus = kingside_bonus_diff * kingside_castling_factor;
	//int total_queenside_bonus = queenside_bonus_diff * queenside_castling_factor;

	//int best_castle_bonus = max(total_kingside_bonus, total_queenside_bonus);

	//total_bonus += best_castle_bonus;

	//// 2rqk2r/ppb4p/2n1ppn1/3p4/1P1P4/P3BN1P/4BPP1/R2Q1RK1 b k - 2 16 : ??

	//// FIXME: one special case makes this bad:
	//// With both the kingside and queenside structures broken, the king is better off centrally, so it will try to give up its castling rights
	////const int total_bonus = (main_bonus + kingside_bonus + queenside_bonus) / (1 + kingside_castle + queenside_castle);
	//// FIXED??

	//cout << "total: " << total_bonus << endl;

	//cout << "main: " << main_bonus << ", kingside: " << kingside_bonus << ", queenside: " << queenside_bonus << endl;

	int total_bonus = -get_long_term_king_weakness(color, -main_bonus, -kingside_bonus, -queenside_bonus);

	//cout << "total: " << total_bonus << endl;

	//rnbqkb1r/pppp2pp/4pn2/5p2/4P3/5N2/PPPPBPPP/RNBQK2R w KQkq - 0 4

	return total_bonus;
}

// Returns the shielding power of the king's pawn structure, were it on the given file
int Board::get_pawn_shield_protection_at_column(bool color, int column, float opponent_attacking_potential, bool add_column_bonus, int space) {

	// r1bqr1k1/pp3pp1/2pp1b1p/8/4P3/2N1Q3/PPP2PPP/1K2RB1R b - - 1 15 ??????

	// Shielding level above which the king counts as safe
	//const int king_base_protection = 600 * (1 - _adv) - 200;
	int needed_protection = 450 * opponent_attacking_potential - 100 - (space > 0 ? space * 2 : space * 0.5);

	// r3r3/P2k2pp/1Bb5/6N1/3P4/3n4/PP3PPP/1KR5 w - - 0 28

	// Protection bonus depending on the file
	constexpr int column_protection_bonus[8] = { 50, 50, 25, 0, 0, 25, 50, 50 };

	if (add_column_bonus)
		needed_protection -= column_protection_bonus[column];

	// King position
	update_kings_pos();

	Pos king_pos = color ? _white_king_pos : _black_king_pos;
	king_pos.col = column;

	// Pawn structure on the adjacent files
	bool pawns[8][5] = { false };

	// No pawn on the king's file
	bool open_files[5] = { false, false, false, false, false };

	// For each file adjacent to the king
	for (int_fast8_t col = king_pos.col - 2; col <= king_pos.col + 2; col++) {
		if (col < 0 || col > 7)
			continue;

		open_files[col - king_pos.col + 2] = true;

		// For each friendly pawn on the file. Test: 1rbq1r1k/b4p1p/p1p2pn1/4p3/P3P2P/1BP2NP1/3NQP2/R4K1R b - - 0 4
		for (uint8_t row = 0; row < 8; row++) {
			if (_array[row][col] == (color ? w_pawn : b_pawn)) {
				pawns[row][col - king_pos.col + 2] = true;
				open_files[col - king_pos.col + 2] = false;
			}
		}
	}

	// r1b1qk1r/ppppb1pp/5n2/n3NQ2/3PP3/2P5/P4PPP/RNB1K2R w KQ - 5 11

	// Bonus for the connection between the pawns
	constexpr int connected_pawns_bonus = 75;

	// Penalty for an open file
	constexpr int open_file_malus = 50;

	// Bonus for every pawn close to the king
	constexpr int pawn_bonus = 75;

	// Multiplier based on the vertical distance
	constexpr float distance_factors[7] = { 1.0f, 1.0f, 0.55f, 0.25f, 0.10f, 0.05f, 0.02f };

	// Multiplier when the king stands in front of the pawns
	constexpr float king_in_front_factor = 0.5f;

	// Multiplier for pawns two files away
	constexpr float semi_adjacent_factor = 0.25f;


	// Bonus for connected pawns
	float connected_pawns_bonus_total = 0.0f;

	// For each pawn
	float pawns_bonus_total = 0.0f;

	// b2r2k1/2p3pn/p7/3p2Pp/2P4P/2N1B2q/P3Q3/5RK1 w - - 2 32


	// As a rule, any push in front of the king weakens it
	//r1bqr1k1/ppp1bppp/2np1n2/4p3/2B1P1P1/3P3P/PPP2P2/RNBQNRK1 b - - 2 8
	// vs
	//r1bqr1k1/ppp1bppp/2np1n2/4p3/2B1P1P1/3P1P1P/PPP5/RNBQNRK1 b - - 1 10
	//rnbq1b1r/ppp2kpp/3p1n2/8/4P3/2N5/PPPP1PPP/R1BQKB1R b KQ - 1 5: g7 still shields

	//r2qr1k1/pp1n1pp1/2pbbp1p/3p4/3P4/P1NBPN1P/1PPQ1PP1/R4RK1 w - - 2 12: it changes when h4 is played
	// r2qr1k1/pp1n1pp1/2pbbp1p/3p4/3P3P/P1NBPNP1/1PPQ1PK1/R4R2 b - - 2 14

	// r1bq1rk1/ppp2ppp/2n2n2/3p4/1bPP4/1PN3P1/P3NPBP/R1BQ1RK1 b - - 2 10 : ??

	// r2r4/pp1b1pkp/1qn1p1n1/3pP3/3P2P1/2N2N2/PP1QBP2/1RR4K b - - 1 20: why is f2 still worth 75 here?

	for (uint8_t col = 0; col < 5; col++) {
		for (uint8_t row = 0; row < 8; row++) {

			if (pawns[row][col]) {

				// Pawn direction
				int dir = color ? 1 : -1;
				int distance = abs(row - king_pos.row);

				float distance_factor = distance_factors[distance];

				// If the king stands in front of the pawns
				if (king_pos.row * dir > row * dir) {
					distance_factor *= king_in_front_factor;
				}

				// Factor (semi-adjacent or adjacent)
				float adjacent_factor = ((col == 0 || col == 4) ? semi_adjacent_factor : 1.0f);


				// REVIEW: unsupported pawns attached to another are still counted as connected
				// En fait non?
				// row +/- dir can leave the board at ranks 0/7: bound-check every index.
				const int row_behind = row - dir;
				const int row_ahead = row + dir;
				auto has_pawn_near = [&](int c) {
					if (c < 0 || c > 4)
						return false;
					if (pawns[row][c])
						return true;
					if (row_behind >= 0 && row_behind <= 7 && pawns[row_behind][c])
						return true;
					if (row_ahead >= 0 && row_ahead <= 7 && pawns[row_ahead][c])
						return true;
					return false;
				};
				if (has_pawn_near(col - 1) || has_pawn_near(col + 1)) {
				//if ((col > 0 && (pawns[row][col - 1] || pawns[row - dir][col - 1])) || (col < 2 && (pawns[row][col + 1] || pawns[row - dir][col + 1]))) {
					const int connected_bonus = connected_pawns_bonus * distance_factor * adjacent_factor;

					//cout << "connected pawns on " << square_name(row, col + king_pos.col - 2) << " : " << connected_bonus << endl;
					connected_pawns_bonus_total += connected_bonus;
				}


				// Adjacent or semi-adjacent file
				const int bonus = pawn_bonus * distance_factor * adjacent_factor;

				//cout << "pawn bonus on " << square_name(row, col + king_pos.col - 2) << " : " << bonus << endl;
				pawns_bonus_total += bonus;
			}
		}
	}

	// r2r4/pp1b1pkp/1qn1p1n1/3pP3/3P2P1/2N2N2/PP1QBP2/1RR3K1 w - - 0 20
	// rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w kq - 4 4

	//cout << "connected pawns: " << connected_pawns_bonus_total << endl;
	//cout << "pawns: " << pawns_bonus_total << endl;

	// Open files
	int open_files_total = 0;

	for (uint8_t col = 0; col < 5; col++) {
		if (open_files[col]) {
			//cout << "open file on " << square_name(king_pos.row, col + king_pos.col - 2) << endl;
			open_files_total += open_file_malus * ((col == 0 || col == 4) ? semi_adjacent_factor : 1.0f);
		}
	}

	open_files_total *= opponent_attacking_potential;

	// Shielding from the board edges

	// Bord vertical
	constexpr int v_edge_protection = 0;

	// Bord horizontal
	constexpr int h_edge_protection = 15;

	const int edge_protection = opponent_attacking_potential * (v_edge_protection * (min(king_pos.row, 7 - king_pos.row) == 0) + h_edge_protection * (min(king_pos.col, 7 - king_pos.col) == 0));

	//cout << "edges: " << edge_protection << endl;

	int total_bonus = connected_pawns_bonus_total + pawns_bonus_total - open_files_total + edge_protection;

	//cout << "total: " << total_bonus << endl;

	return total_bonus - needed_protection;
}

// Computes every move to a given depth and returns the total node count
int Board::get_passed_pawns_count(bool color) const {

	int passed_pawns_count = 0;

	// For each square of the board

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t piece = _array[row][col];

			// If this is a pawn of the right colour
			if ((color && piece == w_pawn) || (!color && piece == b_pawn)) {
				bool is_passed = true;

				// Check the adjacent files and the pawn's own file
				for (int dc = -1; dc <= 1; dc++) {
					int adj_col = col + dc;

					// Check the ranks ahead of the pawn
					for (int dr = 1; dr <= (color ? (7 - row) : row); dr++) {
						int adj_row = color ? (row + dr) : (row - dr);

						// If an enemy piece is found in the passed pawn zone
						if (is_in(adj_row, 0, 7) && is_in(adj_col, 0, 7)) {
							uint8_t adj_piece = _array[adj_row][adj_col];
							if ((color && is_black(adj_piece) && is_pawn(adj_piece)) ||
								(!color && is_white(adj_piece) && is_pawn(adj_piece))) {
								is_passed = false;
								break;
							}
						}
					}

					if (!is_passed) {
						break;
					}
				}
				if (is_passed) {
					passed_pawns_count++;
				}
			}
		}
	}

	return passed_pawns_count;
}

// Returns the evaluation value tied to the passed pawns
int Board::get_passed_pawns_value(bool color) const {
	// TODO ***

	return 0;
}


