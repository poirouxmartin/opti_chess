#include "board.h"
#include "gui.h"
#include "useful_functions.h"
#include "windows_tests.h"
#include "buffer.h"
#include "game_tree.h"
#include "zobrist.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <sstream>
#include <thread>
#include <cmath>
#include <utility>
#include <iomanip>
#include <future>
#include <vector>


// Parallelisation experiments
//vector<thread> threads;

// Default constructor
Board::Board() {
	_positions_history.reserve(16);
}

// Copy constructor
Board::Board(const Board& b, bool full, bool copy_history) {
	copy_data(b, full, copy_history);
}

// Copies the bare minimum of the position
void Board::minimal_copy_data(const Board& b) {

	// Copy the pieces, side to move, castling rights, move numbers, en passant and king positions
	memcpy(_array, b._array, sizeof(_array));
	_player = b._player;
	_castling_rights = b._castling_rights;
	_half_moves_count = b._half_moves_count;
	_moves_count = b._moves_count;
	_en_passant_col = b._en_passant_col;
	_white_king_pos = b._white_king_pos;
	_black_king_pos = b._black_king_pos;

	// Copy the bitboards
	memcpy(_bitboards, b._bitboards, sizeof(_bitboards));
	memcpy(_occupancies, b._occupancies, sizeof(_occupancies));
}

// Copies the attributes of a board
void Board::copy_data(const Board& b, bool full, bool copy_history) {

	// Copy the board
	memcpy(_array, b._array, sizeof(_array));
	_got_moves = b._got_moves;
	_player_in_check = b._player_in_check;
	_player = b._player;
	if (full) {
		memcpy(_moves, b._moves, sizeof(_moves));
		_sorted_moves = b._sorted_moves;
	}
	//_moves_flags_assigned = b._moves_flags_assigned;
	_castling_rights = b._castling_rights;
	_half_moves_count = b._half_moves_count;
	_moves_count = b._moves_count;
	_white_king_pos = b._white_king_pos;
	_black_king_pos = b._black_king_pos;
	_en_passant_col = b._en_passant_col;
	_zobrist_key = b._zobrist_key;

	// Copy the bitboards
	memcpy(_bitboards, b._bitboards, sizeof(_bitboards));
	memcpy(_occupancies, b._occupancies, sizeof(_occupancies));

	if (copy_history) {
		_positions_history = b._positions_history;
	}
	else {
		_positions_history.clear();
	}

	if (full) {
		_is_active = b._is_active;
		_adv = b._adv;
		_advancement = b._advancement;
		_game_over_checked = b._game_over_checked;
		_game_over_value = b._game_over_value;
		_displayed_components = b._displayed_components;
	}
}

// Adds a move to a move list
inline bool Board::add_move(const Move move, uint8_t &iterator, const uint8_t piece) noexcept
{
	// If we exceed the move count we assumed possible in a position
	//if (*iterator >= max_moves) {
	//	cout << "Error: too many moves generated (" << (int)(*iterator) << ")!" << endl;
	//	return false;
	//}

	//const Move m(start_row, start_col, end_row, end_col); // Without the flags, skip the useless work
	//const Move m(i, j, k, l, _array[k][l] != 0, (piece == 1 && i == 7) || (piece == 7 && i == 1));
	//_moves[iterator] = Move(start_row, start_col, end_row, end_col);
	_moves[iterator] = move;

	// Increment the move count
	iterator++;

	return true;
}

// Adds every king move, accounting for controlled squares and castling
inline bool Board::add_king_moves(const bool player, const Pos king_pos, const uint16_t controls_around_king, uint8_t& iterator, const bool kingside_castle_check, const bool queenside_castle_check) noexcept {

	// Piece
	const uint8_t piece = _player ? w_king : b_king;
	const uint8_t row = king_pos.row;
	const uint8_t col = king_pos.col;

	// All the ordinary king moves
	for (uint8_t m = 0; m < 8; m++) {

		// Direction
		const int8_t new_row = row + all_directions[m][0];
		if (!is_in_fast(new_row, 0, 7))
			continue;

		const int8_t new_col = col + all_directions[m][1];
		if (!is_in_fast(new_col, 0, 7))
			continue;

		// If the square is not controlled
		if (!is_controlled_around_king(controls_around_king, all_directions[m][0], all_directions[m][1])) {
			!is_ally(_array[new_row][new_col], player) && add_move(Move(row, col, new_row, new_col), iterator, piece);
		}
	}

	// Roques

	// Kingside castling
	if (kingside_castle_check) {
		if ((!is_controlled_around_king(controls_around_king, 0, 0) && !is_controlled_around_king(controls_around_king, 0, 1) && !is_controlled_around_king(controls_around_king, 0, 2)) && _array[row][col + 1] == none && _array[row][col + 2] == none)
			add_move(Move(row, col, row, col + 2), iterator, piece);
	}

	// Queenside castling
	if (queenside_castle_check) {
		if ((!is_controlled_around_king(controls_around_king, 0, 0) && !is_controlled_around_king(controls_around_king, 0, -1) && !is_controlled_around_king(controls_around_king, 0, -2)) && _array[row][col - 1] == none && _array[row][col - 2] == none && _array[row][col - 3] == none)
			add_move(Move(row, col, row, col - 2), iterator, piece);
	}

	return true;
}

// Adds the moves of a pawn, accounting for pins and checks
inline bool Board::add_pawn_moves(const bool player, const uint8_t row, const uint8_t col, uint8_t& iterator, const PinnedSquare& pin, const bool in_check, const uint64_t& interposition_mask) noexcept {

	// Piece
	const uint8_t piece = player ? w_pawn : b_pawn;
	const int8_t direction = player ? 1 : -1;

	// A horizontally pinned pawn cannot move
	if (pin.pinned && pin.dir.d_row == 0 && pin.dir.d_col != 0)
		return true;

	// Pushes, when the pawn is unpinned or pinned vertically
	if (!pin.pinned || (pin.dir.d_row != 0 && pin.dir.d_col == 0)) {

		// Push by 1
		uint8_t new_row = row + direction;

		// If there is no piece ahead
		if (_array[new_row][col] == none) {

			// Promotion: generate all 4 under/over promotions
			if (new_row == (player ? 7 : 0)) {
				for (uint8_t pp = PROMO_QUEEN; pp <= PROMO_KNIGHT; pp++) {
					Move m(row, col, new_row, col);
					m.set_promo_piece(pp);
					(!in_check || is_in_interpose_mask(interposition_mask, new_row, col)) && add_move(m, iterator, piece);
				}
			}
			else {
				// While in check, the move must interpose
				(!in_check || is_in_interpose_mask(interposition_mask, new_row, col)) && add_move(Move(row, col, new_row, col), iterator, piece);

				// Push by 2
				if (row == 1 + 5 * !player) {
					new_row += direction;

					if (_array[new_row][col] == none) {
						(!in_check || is_in_interpose_mask(interposition_mask, new_row, col)) && add_move(Move(row, col, new_row, col), iterator, piece);
					}
				}
			}
		}
	}

	// Captures

	// Capture to the left
	if (col > 0 && (!pin.pinned || is_aligned(direction, -1, pin.dir))) {

		// Nouvelle position
		int new_row = row + direction;
		int new_col = col - 1;

		// If there is an enemy piece, or this is an en passant capture
		bool take = is_enemy(_array[new_row][new_col], player);

		// If this is a normal capture
		if (take) {
			// Promotion on capture
			if (new_row == (player ? 7 : 0)) {
				for (uint8_t pp = PROMO_QUEEN; pp <= PROMO_KNIGHT; pp++) {
					Move m(row, col, new_row, new_col);
					m.set_promo_piece(pp);
					(!in_check || is_in_interpose_mask(interposition_mask, new_row, new_col)) && add_move(m, iterator, piece);
				}
			}
			else {
				// While in check, the move must interpose
				(!in_check || is_in_interpose_mask(interposition_mask, new_row, new_col)) && add_move(Move(row, col, new_row, new_col), iterator, piece);
			}
		}

		// En passant availability
		else {
			bool en_passant = (row == 3 + player && _en_passant_col == new_col);

			if (en_passant) {
				// A lateral discovered check is the one case en passant can expose, and it is
				// handled below. Test: rnbqkbn1/ppppp3/6p1/3KPp1r/8/7p/PPPP1PPP/RNBQ1BNR w q f6 0 7
				// Rank: ... straight slider ... enemy pawn, friendly pawn ... king ...

				// If the king is on the same rank, to the right of the pawn
				Pos king_pos = player ? _white_king_pos : _black_king_pos;
				if (king_pos.row == row && king_pos.col > col) {

					// Iterate leftwards
					for (int c = new_col - 1; c >= 0; c--) {
						uint8_t p = _array[row][c];

						// If we hit a piece
						if (p != none) {

							// Stop on a friendly piece
							if (is_ally(p, player))
								break;

							// Hitting an enemy straight-line slider means a discovered check
							if (is_rectilinear(p)) {
								en_passant = false;
							}

							break;
						}
					}
				}


				// While in check, the move must interpose (or capture the checking pawn).
				// The last clause covers being checked by a pawn that just advanced two
				// squares, which en passant may capture.
				// Test: 8/8/n1p1kp2/PpP1p2p/2K1p1pP/4P3/2P2PP1/6B1 w - b6 0 39
				if (en_passant)
					(!in_check || is_in_interpose_mask(interposition_mask, new_row, new_col) || (king_pos.row == row - direction && abs(king_pos.col - new_col) == 1)) && add_move(Move(row, col, new_row, new_col), iterator, piece);
			}
		}
	}

	// Capture to the right
	if (col < 7 && (!pin.pinned || is_aligned(direction, 1, pin.dir))) {

		// Nouvelle position
		int new_row = row + direction;
		int new_col = col + 1;

		// If there is an enemy piece, or this is an en passant capture
		bool take = is_enemy(_array[new_row][new_col], player);

		// If this is a normal capture
		if (take) {
			// Promotion on capture
			if (new_row == (player ? 7 : 0)) {
				for (uint8_t pp = PROMO_QUEEN; pp <= PROMO_KNIGHT; pp++) {
					Move m(row, col, new_row, new_col);
					m.set_promo_piece(pp);
					(!in_check || is_in_interpose_mask(interposition_mask, new_row, new_col)) && add_move(m, iterator, piece);
				}
			}
			else {
				// While in check, the move must interpose
				(!in_check || is_in_interpose_mask(interposition_mask, new_row, new_col)) && add_move(Move(row, col, new_row, new_col), iterator, piece);
			}
		}
		// En passant availability
		else {
			bool en_passant = (row == 3 + player && _en_passant_col == new_col);

			if (en_passant) {
				// Mirror of the case above, with the king on the other side of the pawn.
				// Seul cas possible:
				// Rank: ... king ... friendly pawn, enemy pawn ... straight slider ...
				
				// If the king is on the same rank, to the left of the pawn
				Pos king_pos = player ? _white_king_pos : _black_king_pos;
				if (king_pos.row == row && king_pos.col < col) {

					// Iterate rightwards
					for (int c = new_col + 1; c < 8; c++) {
						uint8_t p = _array[row][c];

						// If we hit a piece
						if (p != none) {

							// Stop on a friendly piece
							if (is_ally(p, player))
								break;

							// Hitting an enemy straight-line slider means a discovered check
							if (is_rectilinear(p)) {
								en_passant = false;
							}

							break;
						}
					}
				}

				// While in check, the move must interpose
				if (en_passant)
					(!in_check || is_in_interpose_mask(interposition_mask, new_row, new_col) || (king_pos.row == row - direction && abs(king_pos.col - new_col) == 1)) && add_move(Move(row, col, new_row, new_col), iterator, piece);
			}
		}
	}

	return true;
}

// Adds the moves of a knight, accounting for pins and checks
inline bool Board::add_knight_moves(const bool player, const uint8_t row, const uint8_t col, uint8_t& iterator, const PinnedSquare& pin, const bool in_check, const uint64_t& interposition_mask) noexcept {

	// Piece
	const uint8_t piece = player ? w_knight : b_knight;
	bool is_pinned = pin.pinned;

	// A pinned knight cannot move
	if (is_pinned)
		return true;

	// Every possible move
	for (uint8_t m = 0; m < 8; m++) {

		// Direction
		const uint8_t new_row = row + knight_directions[m][0];
		if (!is_in_fast(new_row, 0, 7))
			continue;

		const uint8_t new_col = col + knight_directions[m][1];
		if (!is_in_fast(new_col, 0, 7))
			continue;

		// If the square is not occupied by a friendly piece
		if (!is_ally(_array[new_row][new_col], player)) {
			// While in check, the move must interpose
			(!in_check || is_in_interpose_mask(interposition_mask, new_row, new_col)) && add_move(Move(row, col, new_row, new_col), iterator, piece);
		}
	}

	return true;
}

// Adds the moves of a straight-line slider, accounting for pins and checks
inline bool Board::add_rect_moves(const bool player, const uint8_t row, const uint8_t col, uint8_t& iterator, const PinnedSquare& pin, const bool in_check, const uint64_t& interposition_mask) noexcept {

	// Piece
	const uint8_t piece = player ? w_rook : b_rook;
	bool is_pinned = pin.pinned;
	Direction pin_dir = pin.dir;
	const uint8_t ally_min = player ? w_pawn : b_pawn;
	const uint8_t ally_max = player ? w_king : b_king;

	// Iterate over the 4 directions
	for (uint8_t m = 0; m < 4; m++) {

		// Direction
		const int8_t d_row = rect_directions[m][0];
		const int8_t d_col = rect_directions[m][1];

		// A pinned piece is only examined along the pin direction
		if (is_pinned && !is_aligned(d_row, d_col, pin_dir))
			continue;

		// Iterate along the direction
		uint8_t current_row = row + d_row;
		uint8_t current_col = col + d_col;

		while (current_row >= 0 && current_row < 8 && current_col >= 0 && current_col < 8) {

			// Piece on the square
			const uint8_t p2 = _array[current_row][current_col];

			// Stop on a friendly piece
			if (is_ally(p2, player))
				break;

			// While in check, the move must interpose
			if (!in_check || is_in_interpose_mask(interposition_mask, current_row, current_col)) {
				add_move(Move(row, col, current_row, current_col), iterator, piece);
			}

			// Stop on an enemy piece
			if (p2 != none)
				break;

			current_row += d_row;
			current_col += d_col;
		}
	}

	return true;
}

// Adds the moves of a diagonal slider, accounting for pins and checks
inline bool Board::add_diag_moves(const bool player, const uint8_t row, const uint8_t col, uint8_t& iterator, const PinnedSquare& pin, const bool in_check, const uint64_t& interposition_mask) noexcept {

	// Piece
	const uint8_t piece = player ? w_bishop : b_bishop;
	bool is_pinned = pin.pinned;
	Direction pin_dir = pin.dir;
	const uint8_t ally_min = player ? w_pawn : b_pawn;
	const uint8_t ally_max = player ? w_king : b_king;

	// Iterate over the 4 directions
	for (uint8_t m = 0; m < 4; m++) {

		// Direction
		const int8_t d_row = diag_directions[m][0];
		const int8_t d_col = diag_directions[m][1];

		// A pinned piece is only examined along the pin direction
		if (is_pinned && !is_aligned(d_row, d_col, pin_dir))
			continue;

		// Iterate along the direction
		uint8_t current_row = row + d_row;
		uint8_t current_col = col + d_col;

		while (current_row >= 0 && current_row < 8 && current_col >= 0 && current_col < 8) {

			// Piece on the square
			const uint8_t p2 = _array[current_row][current_col];

			// Stop on a friendly piece
			if (is_ally(p2, player))
				break;

			// While in check, the move must interpose
			if (!in_check || is_in_interpose_mask(interposition_mask, current_row, current_col)) {
				add_move(Move(row, col, current_row, current_col), iterator, piece);
			}

			// Stop on an enemy piece
			if (p2 != none)
				break;

			current_row += d_row;
			current_col += d_col;
		}
	}

	return true;
}

static constexpr uint64_t SQUARE_MASKS[64] = {
	1ULL << 0,  1ULL << 1,  1ULL << 2,  1ULL << 3,  1ULL << 4,  1ULL << 5,  1ULL << 6,  1ULL << 7,
	1ULL << 8,  1ULL << 9,  1ULL << 10, 1ULL << 11, 1ULL << 12, 1ULL << 13, 1ULL << 14, 1ULL << 15,
	1ULL << 16, 1ULL << 17, 1ULL << 18, 1ULL << 19, 1ULL << 20, 1ULL << 21, 1ULL << 22, 1ULL << 23,
	1ULL << 24, 1ULL << 25, 1ULL << 26, 1ULL << 27, 1ULL << 28, 1ULL << 29, 1ULL << 30, 1ULL << 31,
	1ULL << 32, 1ULL << 33, 1ULL << 34, 1ULL << 35, 1ULL << 36, 1ULL << 37, 1ULL << 38, 1ULL << 39,
	1ULL << 40, 1ULL << 41, 1ULL << 42, 1ULL << 43, 1ULL << 44, 1ULL << 45, 1ULL << 46, 1ULL << 47,
	1ULL << 48, 1ULL << 49, 1ULL << 50, 1ULL << 51, 1ULL << 52, 1ULL << 53, 1ULL << 54, 1ULL << 55,
	1ULL << 56, 1ULL << 57, 1ULL << 58, 1ULL << 59, 1ULL << 60, 1ULL << 61, 1ULL << 62, 1ULL << 63
};

// Returns the list of legal moves
bool Board::get_moves() noexcept {

	// Logique
	// 1: evaluate the pins -> return the direction list for each pin (make it a struct?)
	// 2: build an array of the squares controlled around the king (and on the king)
	// 3: are we in check? (-> king square controls > 0)
	//	3a: not in check
	//		moves = every piece move, restricted to the pin direction for pinned pieces
	//	3b: in check
	//		moves = king moves at distance 1 onto empty AND uncontrolled squares
	//		2: single check (not double or more)
	//			moves += capture the attacking piece (how do we find it?)
	//			moves += interpose the check (unless it comes from a knight) -> quick to spot from the pins?

	// Still to optimise:
	// - move generation for every piece, worth revisiting
	// - find the most efficient way to build the control array around the king
	// - keep an array of empty or non-friendly squares to resolve reachability faster?
	// - cut down the number of board square lookups (they add up)
	// - use the Piece struct for faster access to piece types and colours
	// - fold get_square_attacker into get_controls_around_king?
	// - reuse data from the previous position? (pins, controls, attackers)
	// - among the controlled squares around the king, skip those occupied by a friendly piece
	// - remember where the pieces are to avoid sweeping the whole board (costly otherwise)
	// - iterate bottom-up for White and top-down for Black, cutting at the 16th piece, to avoid scanning the whole board (may be moot once friendly piece positions are stored)
	// - use a different structure for pins? there are only 8 possible ones, and the calls can be costly
	// - precomputed attack tables for knight, pawn and king (see BBC)
	// - precomputed control tables, stored per position for reuse

	// BITBOARDS:
	// - is _king_pos still needed now that it is stored elsewhere?
	// - avoid full-board iteration, look only where the pieces are
	// - avoid hitting _array[][] constantly (costly)
	// - Magic bitboards

	// MICRO-OPTIMIZATIONS:
	// - Const -> slower?
	// - most predictable branches first
	// - minimise the cycle count
	// - separate white/black functions

	// Cas tests:
	// r1bq1b1r/pp4pp/2p1k3/3np3/1nBP4/2N2Q2/PPP2PPP/R1B2RK1 b - - 0 10
	// 2Qr3k/pp2R1pr/2p2N2/4p3/4N3/1P2B3/P4PPP/6K1 b - - 2 31: queen on c8 pinned against the rook on d8
	// rnbqkbnr/pp1ppppp/8/8/2p5/3P4/PPPKPPPP/RNBQ1BNR w kq - 0 3: misses that capturing on c4 is possible

	// Side to move
	const bool player = _player;

	// King position
	Pos king_pos = player ? _white_king_pos : _black_king_pos;

	// Pins in the position, for the side to move
	PinsMap pins = get_pins(player);

	// Look at the king's attackers, if any
	int n_attackers = 0;
	PieceSquare attacker = get_square_attacker(king_pos, &n_attackers);

	// We are in check iff there is at least one attacker
	bool in_check = n_attackers > 0;

	// While in check, castling is not considered
	bool check_kingside = !in_check && (player ? _castling_rights.k_w : _castling_rights.k_b);
	bool check_queenside = !in_check && (player ? _castling_rights.q_w : _castling_rights.q_b);

	// Generate the controls around the king (and the pieces attacking it)
	uint16_t controls_around_king = get_controls_around_king(king_pos, player, check_kingside, check_queenside);

	// Iterator
	uint8_t iterator = 0;

	// Add the king moves onto empty, uncontrolled squares
	add_king_moves(player, king_pos, controls_around_king, iterator, check_kingside, check_queenside);

	// With one attacker we can store the squares between king and attacker, for interpositions
	uint64_t interposition_mask = in_check ? get_interpose_mask(king_pos, attacker) : 0;

	// Outside of a double check, the other pieces can be considered
	if (n_attackers < 2) {

		// Iterate over the friendly pieces
		uint64_t bb = _occupancies[!player];

		while (bb) {

			// Fetch a square holding a friendly piece
			int sq = pop_lsb(bb);  // 0..63
			const uint64_t sq_mask = SQUARE_MASKS[sq];

			// Determine the piece type by looking through the bitboards
			uint8_t piece = 0;
			int first_type = player ? w_pawn : b_pawn;
			for (int type = first_type; type <= first_type + 4; type++) {
				if (_bitboards[type] & sq_mask) {
					piece = type;
					break;
				}
			}

			// Conversion en row/col
			uint8_t row = sq >> 3;
			uint8_t col = sq & 7;

			// Pawn
			if (is_pawn(piece)) {
				add_pawn_moves(player, row, col, iterator, pins.pins[row][col], in_check, interposition_mask);
				continue;
			}

			// Knight
			if (is_knight(piece)) {
				add_knight_moves(player, row, col, iterator, pins.pins[row][col], in_check, interposition_mask);
				continue;
			}

			// Straight-line slider
			if (is_rectilinear(piece)) {
				add_rect_moves(player, row, col, iterator, pins.pins[row][col], in_check, interposition_mask);
			}

			// Diagonal slider
			if (is_diagonal(piece)) {
				add_diag_moves(player, row, col, iterator, pins.pins[row][col], in_check, interposition_mask);
			}
		}
	}

	_got_moves = iterator;
	_player_in_check = in_check;

	return true;
}

// Prints the control map around the king
void print_controls(uint16_t controls) {
	// The grid is 3x5
	cout << "Controls around the king:" << endl;

	for (int row = 2; row >= 0; --row) {
		for (int col = 0; col < 5; ++col) {
			int bit_index = row * 5 + col;
			bool controlled = (controls & (1 << bit_index)) != 0;
			cout << (controlled ? "X " : ". ");
		}
		cout << endl;
	}

	cout << endl;
}

// Builds the control map around the king
uint16_t Board::get_controls_around_king(Pos king_pos, bool player, bool kingside_castle_check, bool queenside_castle_check) const noexcept {

	// Logique:
	// Iterate over the enemy pieces
	// Cut early the moves and directions that cannot reach the squares of interest
	// Fill in the control map

	// Action zone (the squares we care about)
	const uint8_t min_row = max(0, king_pos.row - 1);
	const uint8_t max_row = min(7, king_pos.row + 1);
	const uint8_t min_col = max(0, king_pos.col - 1 - queenside_castle_check);
	const uint8_t max_col = min(7, king_pos.col + 1 + kingside_castle_check);

	// Control map around the king
	uint16_t controls = 0;

	// The king under consideration
	const uint8_t piece_king = player ? w_king : b_king;

	// Pawn direction by side, for the control zone
	const int8_t pawn_dir = player ? -1 : 1;

	// Iterate over the enemy pieces
	uint64_t bb = _occupancies[player];

	while (bb) {

		// Fetch a square holding a friendly piece
		int sq = pop_lsb(bb);  // 0..63
		const uint64_t sq_mask = SQUARE_MASKS[sq];

		// Determine the piece type by looking through the bitboards
		uint8_t piece = 0;
		int first_type = player ? b_pawn : w_pawn;
		for (int type = first_type; type <= first_type + 5; type++) {
			if (_bitboards[type] & sq_mask) {
				piece = type;
				break;
			}
		}

		// Conversion en row/col
		uint8_t row = sq >> 3;
		uint8_t col = sq & 7;

		// Pawn
		if (is_pawn(piece)) {

			uint8_t control_row = row + pawn_dir;

			// Skip when the pawn cannot control the zone
			if (!is_in_fast(control_row, min_row, max_row))
				continue;

			// Left control
			if (is_in_fast(col - 1, min_col, max_col))
				controls |= control_bit(control_row - king_pos.row, col - 1 - king_pos.col);

			// Right control
			if (is_in_fast(col + 1, min_col, max_col))
				controls |= control_bit(control_row - king_pos.row, col + 1 - king_pos.col);

			continue;
		}

		// Knight
		if (is_knight(piece)) {

			// Iterate over the directions
			for (int k = 0; k < 8; k++) {
					
				// FIXME *** possible d'optimiser en coupant certaines directions?

				// Skip when the knight cannot control the zone
				const uint8_t new_row = row + knight_directions[k][0];
				if (!is_in_fast(new_row, min_row, max_row))
					continue;

				const uint8_t new_col = col + knight_directions[k][1];
				if (!is_in_fast(new_col, min_col, max_col))
					continue;

				controls |= control_bit(new_row - king_pos.row, new_col - king_pos.col);
			}

			continue;
		}

		// Slider rectiligne
		if (is_rectilinear(piece)) {

			// Iterate over the directions
			for (int d = 0; d < 4; d++) {

				// Direction
				const uint8_t d_row = rect_directions[d][0];
				const uint8_t d_col = rect_directions[d][1];

				uint8_t current_row = row + d_row;
				uint8_t current_col = col + d_col;

				// Skip when the direction cannot control the zone
				if ((d_row == -1 && current_row < min_row) || (d_row == 1 && current_row > max_row) || (d_col == -1 && current_col < min_col) || (d_col == 1 && current_col > max_col))
					continue;

				// Advance along the direction
				while (current_row >= 0 && current_row < 8 && current_col >= 0 && current_col < 8) {

					// Inside the zone, add the control
					if (is_in_fast(current_row, min_row, max_row) && is_in_fast(current_col, min_col, max_col))
						controls |= control_bit(current_row - king_pos.row, current_col - king_pos.col);

					// Stop on a piece (except the king, where we carry on)
					const uint8_t p2 = _array[current_row][current_col];
					if (p2 != none && p2 != piece_king)
						break;

					current_row += d_row;
					current_col += d_col;
				}
			}
		}

		// Slider diagonal
		if (is_diagonal(piece)) {

			// Iterate over the directions
			for (int d = 0; d < 4; d++) {

				// Direction
				const uint8_t d_row = diag_directions[d][0];
				const uint8_t d_col = diag_directions[d][1];

				uint8_t current_row = row + d_row;
				uint8_t current_col = col + d_col;

				// Skip when the direction cannot control the zone
				if ((d_row == -1 && current_row < min_row) || (d_row == 1 && current_row > max_row) || (d_col == -1 && current_col < min_col) || (d_col == 1 && current_col > max_col))
					continue;

				// Advance along the direction
				while (current_row >= 0 && current_row < 8 && current_col >= 0 && current_col < 8) {

					// Inside the zone, add the control
					if (is_in(current_row, min_row, max_row) && is_in(current_col, min_col, max_col))
						controls |= control_bit(current_row - king_pos.row, current_col - king_pos.col);

					// Stop on a piece (except the king, where we carry on)
					const uint8_t p2 = _array[current_row][current_col];
					if (p2 != none && p2 != piece_king)
						break;

					current_row += d_row;
					current_col += d_col;
				}
			}

			continue;
		}

		// King
		if (is_king(piece)) {

			// Iterate over the directions
			for (int k = 0; k < 8; k++) {

				// Skip when the king cannot control the zone
				const uint8_t new_row = row + all_directions[k][0];

				if (!is_in_fast(new_row, min_row, max_row))
					continue;

				const uint8_t new_col = col + all_directions[k][1];
				if (!is_in_fast(new_col, min_col, max_col))
					continue;

				controls |= control_bit(new_row - king_pos.row, new_col - king_pos.col);
			}

			continue;
		}
	}

	return controls;
}

// Returns one of the pieces attacking the square, when there are several
PieceSquare Board::get_square_attacker(Pos square, int* n_attackers) const noexcept {
	const uint8_t square_row = square.row;
	const uint8_t square_col = square.col;

	PieceSquare attacker = PieceSquare(none, { -1, -1 });

	// Pawn direction by side, for captures
	const int pawn_dir = _player ? 1 : -1;
	const uint8_t knight = 2 + _player * 6;

	// Knight check
	for (int k = 0; k < 8; k++) {
		const uint8_t row_knight = square_row + knight_directions[k][0];
		const uint8_t col_knight = square_col + knight_directions[k][1];

		// Make sure we are still on the board
		if (!on_board_unsigned_short(row_knight, col_knight))
			continue;

		if (_array[row_knight][col_knight] == knight) {
			(*n_attackers)++;
			if (*n_attackers == 2) return attacker;
			attacker = PieceSquare(knight, { row_knight, col_knight });
		}
	}

	// Straight and diagonal slider check
	for (int d = 0; d < 8; d++) {
		const int8_t drow = all_directions[d][0];
		const int8_t dcol = all_directions[d][1];

		uint8_t row = square_row + drow;
		uint8_t col = square_col + dcol;

		while (row >= 0 && row < 8 && col >= 0 && col < 8) {
			const uint8_t piece = _array[row][col];

			if (piece != none) {
				if (!is_ally(piece, _player)) {
					bool valid_attack = false;

					// King at distance 1
					if (is_king(piece) && abs(row - square_row) <= 1 && abs(col - square_col) <= 1)
						valid_attack = true;

					// Enemy pawn capturing onto the square
					if ((piece == w_pawn && !_player && row - square_row == pawn_dir && abs(col - square_col) == 1)
						|| (piece == b_pawn && _player && row - square_row == pawn_dir && abs(col - square_col) == 1))
						valid_attack = true;

					// Slider matching the direction
					if ((abs(drow) + abs(dcol) == 1) && is_rectilinear(piece)) valid_attack = true;
					if ((abs(drow) == 1 && abs(dcol) == 1) && is_diagonal(piece)) valid_attack = true;

					if (valid_attack) {
						(*n_attackers)++;
						if (*n_attackers == 2) return attacker;
						attacker = PieceSquare(piece, { row, col });
					}
				}

				break; // Stop as soon as a piece is met
			}

			row += drow;
			col += dcol;
		}
	}

	return attacker;
}

// Returns a bitboard of the interposition squares between king and attacker, attacker included
uint64_t Board::get_interpose_mask(Pos king_pos, const PieceSquare &attacker) const noexcept {
	uint64_t mask = 0ULL;
	const uint8_t attacker_piece = attacker.piece;
	const uint8_t row = attacker.square.row;
	const uint8_t col = attacker.square.col;

	// For a knight attacker, only its own square counts
	if (is_knight(attacker_piece)) {
		mask |= 1ULL << (row * 8 + col);
		return mask;
	}

	// Determine the slider direction
	int d_row = (row == king_pos.row) ? 0 : (row > king_pos.row ? 1 : -1);
	int d_col = (col == king_pos.col) ? 0 : (col > king_pos.col ? 1 : -1);

	int r = king_pos.row + d_row;
	int c = king_pos.col + d_col;

	// Add every square up to the attacker
	while (r != row || c != col) {
		mask |= 1ULL << (r * 8 + c);
		r += d_row;
		c += d_col;
	}

	// Add the attacker square
	mask |= 1ULL << (row * 8 + col);

	return mask;
}

// Returns the pin list for the given side
PinsMap Board::get_pins(bool player) const noexcept {

	// Pinned pieces by direction (at most 8: 2 per direction, on either side of the king)
	PinsMap pins;

	// Logic: start from the player's king
	// Look in each possible direction

	// For each direction, walk until a piece is found or the board ends (break)
	// If the piece is not friendly, move on to the next direction
	// Record the square, pinned or not
	// Keep walking that direction to the next piece, or off the board (break)
	// The recorded square is pinned iff the second piece found is an enemy slider matching the direction

	// King position of the side concerned
	uint8_t king_row = player ? _white_king_pos.row : _black_king_pos.row;
	uint8_t king_col = player ? _white_king_pos.col : _black_king_pos.col;

	// Walk the directions
	for (int d = 0; d < 8; d++) {

		// Direction being visited
		int8_t d_row = all_directions[d][0];
		int8_t d_col = all_directions[d][1];

		// Position, to be incremented
		int row = king_row + d_row;
		int col = king_col + d_col;

		// Have we already found a pinnable piece?
		bool found_candidate = false;

		// Position of the pinnable piece
		Pos candidate_pos;

		// Iterate over the board
		while (row >= 0 && row < 8 && col >= 0 && col < 8) {
			uint8_t piece = _array[row][col];

			// We hit a piece
			if (piece != none) {

				// If no pinnable piece was found yet
				if (!found_candidate) {

					// An enemy piece cannot be pinned, move on to the next direction
					if (!is_ally(piece, player)) {
						break;
					}

					// The piece is pinnable
					candidate_pos = { row, col };
					found_candidate = true;
				}

				// We already have a pinnable piece
				else {

					// No pin possible when covered by a friendly piece
					if (is_ally(piece, player)) {
						break;
					}

					// Is the movement straight (otherwise it is diagonal)?
					bool is_rect = abs(d_row) + abs(d_col) == 1;

					if ((is_rect && is_rectilinear(piece)) || (!is_rect && is_diagonal(piece))) {
						pins.pins[candidate_pos.row][candidate_pos.col].pinned = true;
						pins.pins[candidate_pos.row][candidate_pos.col].dir = { d_row, d_col };
					}

					break; // Stop searching this direction, pin or no pin
				}
			}

			row += d_row;
			col += d_col;
		}
	}


	return pins;
}

// Tells whether the side to move is in check
bool Board::in_check(bool update_king_pos) noexcept
{
	if (update_king_pos)
		update_kings_pos();

	const int king_row = _player ? _white_king_pos.row : _black_king_pos.row;
	const int king_col = _player ? _white_king_pos.col : _black_king_pos.col;

	// Faster approach: start from the king to find the potential attackers:
	// Walk the diagonals, ranks and files, and see whether an enemy piece attacks the king along them

	// TODO: merge with the equivalents in the other functions?

	const int enemy_knight = 2 + _player * 6;

	for (int k = 0; k < 8; k++) {

		// Skip a knight square that is off the board
		const int nrow = king_row + knight_directions[k][0];
		if (!is_in(nrow, 0, 7))
			continue;

		const int ncol = king_col + knight_directions[k][1];
		if (!is_in(ncol, 0, 7))
			continue;

		// An attacking knight means check, so return true
		if (_array[nrow][ncol] == enemy_knight)
			return true;
	}

	// TODO: is there a cheaper order to scan the lines in, given the opponent is more likely to attack through the centre?

	// Look along ranks and files

	// Gauche
	for (int col = king_col - 1; col >= 0; col--)
	{
		// If there is a piece
		if (const uint8_t piece = _array[king_row][col]; piece != none)
		{
			// If the piece is not ours, check for a rook, a queen, or a king at distance 1
			if (piece < 7 != _player)
				if (is_rectilinear(piece) || (is_king(piece) && col == king_col - 1))
					return true;

			break;
		}
	}

	// Droite
	for (int col = king_col + 1; col < 8; col++)
	{
		if (const uint8_t piece = _array[king_row][col]; piece != none)
		{
			if (piece < 7 != _player)
				if (is_rectilinear(piece) || (is_king(piece) && col == king_col + 1))
					return true;

			break;
		}
	}

	// Haut
	for (int row = king_row - 1; row >= 0; row--)
	{
		if (const uint8_t piece = _array[row][king_col]; piece != none)
		{
			if (piece < 7 != _player)
				if (is_rectilinear(piece) || (is_king(piece) && row == king_row - 1))
					return true;

			break;
		}
	}

	// Bas
	for (int row = king_row + 1; row < 8; row++)
	{
		if (const uint8_t piece = _array[row][king_col]; piece != none)
		{
			if (piece < 7 != _player)
				if (is_rectilinear(piece) || (is_king(piece) && row == king_row + 1))
					return true;

			break;
		}
	}

	// Look along the diagonals

	// Diagonale bas-gauche
	for (int row = king_row - 1, col = king_col - 1; row >= 0 && col >= 0; row--, col--)
	{
		if (const uint8_t piece = _array[row][col]; piece != none)
		{
			if (piece < 7 != _player)
			{
				if (is_diagonal(piece) || (is_king(piece) && (abs(king_row - row) == 1)))
					return true;

				// Special case for pawns
				if (piece == w_pawn && abs(king_col - col) == 1)
					return true;
			}

			break;
		}
	}

	// Diagonal bas-droite
	for (int row = king_row - 1, col = king_col + 1; row >= 0 && col < 8; row--, col++)
	{
		if (const uint8_t piece = _array[row][col]; piece != none)
		{
			if ((piece < 7) != _player)
			{
				if (is_diagonal(piece) || (is_king(piece) && (abs(king_row - row) == 1)))
					return true;

				// Special case for pawns
				if (piece == w_pawn && abs(king_col - col) == 1)
					return true;
			}

			break;
		}
	}

	// Diagonale haut-gauche
	for (int row = king_row + 1, col = king_col - 1; row < 8 && col >= 0; row++, col--)
	{
		if (const uint8_t piece = _array[row][col]; piece != none)
		{
			if ((piece < 7) != _player)
			{
				if (is_diagonal(piece) || (is_king(piece) && (abs(king_row - row) == 1)))
					return true;

				// Pawns

				if (piece == b_pawn && abs(king_col - col) == 1)
					return true;
			}

			break;
		}
	}

	// Diagonale haut-droite
	for (int row = king_row + 1, col = king_col + 1; row < 8 && col < 8; row++, col++)
	{
		if (const uint8_t piece = _array[row][col]; piece != none)
		{
			if ((piece < 7) != _player)
			{
				if (is_diagonal(piece) || (is_king(piece) && (abs(king_row - row) == 1)))
					return true;

				// Pawns
				if (piece == b_pawn && abs(king_col - col) == 1)
					return true;
			}

			break;
		}
	}

	return false;
}

// Prints the move list given as an argument
inline void Board::make_move(const Move& move, const bool pgn, const bool add_to_history) noexcept
{
	// TODO *** optimiser
	// Split into separate white/black functions?

	// TODO: check whether this actually makes it faster
	const int row1 = move.start_row;
	const int col1 = move.start_col;
	const int row2 = move.end_row;
	const int col2 = move.end_col;
	const int p = _array[row1][col1];
	const int p_last = _array[row2][col2];
	const bool irreversible_move = add_to_history && is_irreversible_move(move);

	// Save old state for incremental Zobrist update
	const uint8_t old_castling = _castling_rights.k_w + _castling_rights.q_w * 2 + _castling_rights.k_b * 4 + _castling_rights.q_b * 8;
	const int old_en_passant = _en_passant_col;

	// TODO *** rendre plus efficace
	if (pgn) {
		if (_moves_count != 0 || _half_moves_count != 0)
			main_GUI._pgn += " ";
		if (_player) {
			stringstream ss;
			ss << _moves_count;
			string s;
			ss >> s;
			main_GUI._pgn += s;
			main_GUI._pgn += ". ";
		}
		main_GUI._pgn += move_label(move);
	}

	// Reset the halfmove clock on a pawn move or a capture
	if (is_pawn(p) || p_last) {
		_half_moves_count = 0;
	}
	else {
		// Increment the halfmove clock
		_half_moves_count++;
	}

	// Moves that make en passant available
	_en_passant_col = -1;

	// Pawn advancing 2 squares with an enemy pawn left or right -> en passant becomes available
	(p == w_pawn && row2 == row1 + 2 && ((col2 > 0 && _array[row2][col2 - 1] == b_pawn) || (col2 < 7 && _array[row2][col2 + 1] == b_pawn))) && (_en_passant_col = col1);
	(p == b_pawn && row2 == row1 - 2 && ((col2 > 0 && _array[row2][col2 - 1] == w_pawn) || (col2 < 7 && _array[row2][col2 + 1] == w_pawn))) && (_en_passant_col = col1);

	// En passant
	(p == w_pawn && col1 != col2 && p_last == none) && (_array[row2 - 1][col2] = none);
	(p == b_pawn && col1 != col2 && p_last == none) && (_array[row2 + 1][col2] = none);


	// White king
	if (p == w_king) {
		_castling_rights.q_w = false;
		_castling_rights.k_w = false;
		_white_king_pos = { row2, col2 }; // Update the king position

		(col2 == col1 + 2) && ((_array[0][7] = none), (_array[0][5] = w_rook)); // Petit roque
		(col2 == col1 - 2) && ((_array[0][0] = none), (_array[0][3] = w_rook)); // Grand roque
	}

	// Black king
	else if (p == b_king) {
		_castling_rights.q_b = false;
		_castling_rights.k_b = false;
		_black_king_pos = { row2, col2 };

		(col2 == col1 + 2) && ((_array[7][7] = none), (_array[7][5] = b_rook)); // Petit roque
		(col2 == col1 - 2) && ((_array[7][0] = none), (_array[7][3] = b_rook)); // Grand roque
	}

	// Loss of castling rights

	// White rook
	(p == w_rook) && ((col1 == 0) && (row1 == 0) && (_castling_rights.q_w = false) || (col1 == 7) && (row1 == 0) && (_castling_rights.k_w = false));

	// Black rook
	(p == b_rook) && ((col1 == 0) && (row1 == 7) && (_castling_rights.q_b = false) || (col1 == 7) && (row1 == 7) && (_castling_rights.k_b = false));

	// White rook captured
	(p_last == w_rook) && ((col2 == 0) && (row2 == 0) && (_castling_rights.q_w = false) || (col2 == 7) && (row2 == 0) && (_castling_rights.k_w = false));

	// Black rook captured
	(p_last == b_rook) && ((col2 == 0) && (row2 == 7) && (_castling_rights.q_b = false) || (col2 == 7) && (row2 == 7) && (_castling_rights.k_b = false));

	// Update the destination square
	_array[row2][col2] = p;

	// Promotion (uses the promotion piece from the move)
	if (move.is_promotion())
		_array[row2][col2] = promo_to_piece(move.get_promo_piece(), _player);

	// Clear the origin square
	_array[row1][col1] = none;

	// Update the bitboards
	const uint8_t promo = move.is_promotion() ? move.get_promo_piece() : 0;
	_player ? update_bitboards_white(row1, col1, row2, col2, p, p_last, promo) : update_bitboards_black(row1, col1, row2, col2, p, p_last, promo);

	// Flip the side to move
	_player = !_player;

	// Increment the move counter
	_player && _moves_count++;

	// Reset the possible move count
	_got_moves = -1;
	_player_in_check = false;

	// The move flags are reset
	//_moves_flags_assigned = false;

	// The moves are no longer sorted
	_sorted_moves = false;

	// Game over has to be re-checked
	_game_over_checked = false;

	reset_eval();

	if (add_to_history) {
		if (irreversible_move) {
			reset_positions_history();
		}

		// Incremental Zobrist update (O(1) instead of O(64) full recompute)
		if (!transposition_table._zobrist._keys_generated)
			transposition_table._zobrist.generate_zobrist_keys();
		const Zobrist& zobrist = transposition_table._zobrist;

		// XOR out moved piece from start, XOR in at end (or promoted piece)
		const int start_sq = row1 * 8 + col1;
		const int end_sq = row2 * 8 + col2;
		const bool piece_color = !_player; // _player was already flipped; use original side
		const int promo = move.is_promotion() ? promo_to_piece(move.get_promo_piece(), piece_color) : p;

		_zobrist_key ^= zobrist._board_keys[start_sq][p - 1];
		_zobrist_key ^= zobrist._board_keys[end_sq][promo - 1];

		// XOR out captured piece (if any)
		if (p_last != none)
			_zobrist_key ^= zobrist._board_keys[end_sq][p_last - 1];

		// Castling: move the rook in Zobrist too
		if (p == w_king && col2 == col1 + 2) {
			_zobrist_key ^= zobrist._board_keys[0 * 8 + 7][w_rook - 1];
			_zobrist_key ^= zobrist._board_keys[0 * 8 + 5][w_rook - 1];
		}
		else if (p == w_king && col2 == col1 - 2) {
			_zobrist_key ^= zobrist._board_keys[0 * 8 + 0][w_rook - 1];
			_zobrist_key ^= zobrist._board_keys[0 * 8 + 3][w_rook - 1];
		}
		else if (p == b_king && col2 == col1 + 2) {
			_zobrist_key ^= zobrist._board_keys[7 * 8 + 7][b_rook - 1];
			_zobrist_key ^= zobrist._board_keys[7 * 8 + 5][b_rook - 1];
		}
		else if (p == b_king && col2 == col1 - 2) {
			_zobrist_key ^= zobrist._board_keys[7 * 8 + 0][b_rook - 1];
			_zobrist_key ^= zobrist._board_keys[7 * 8 + 3][b_rook - 1];
		}

		// En passant capture: XOR out the captured pawn
		if (is_pawn(p) && col1 != col2 && p_last == none) {
			int ep_pawn_row = (p == w_pawn) ? row2 - 1 : row2 + 1;
			int ep_pawn = (p == w_pawn) ? b_pawn : w_pawn;
			_zobrist_key ^= zobrist._board_keys[ep_pawn_row * 8 + col2][ep_pawn - 1];
		}

		// Castling rights
		uint8_t new_castling = _castling_rights.k_w + _castling_rights.q_w * 2 + _castling_rights.k_b * 4 + _castling_rights.q_b * 8;
		if (old_castling != new_castling) {
			_zobrist_key ^= zobrist._castling_keys[old_castling];
			_zobrist_key ^= zobrist._castling_keys[new_castling];
		}

		// En passant
		if (old_en_passant != _en_passant_col) {
			if (old_en_passant != -1)
				_zobrist_key ^= zobrist._en_passant_keys[old_en_passant];
			if (_en_passant_col != -1)
				_zobrist_key ^= zobrist._en_passant_keys[_en_passant_col];
		}

		// Side to move
		_zobrist_key ^= zobrist._player_key;

		_positions_history[_zobrist_key]++;
	}


	return;
}

// Unmakes a move
void Board::unmake_move(Move move, uint8_t p1, uint8_t p2, int en_passant_col, int prev_half_count, bool k_castle, bool q_castle, bool is_castle, bool is_promotion, bool is_en_passant) {
	// TODO

	// TODO: decide whether to mirror everything make_move does (eval reset and so on)
	// TODO: would a faster make_move_minimal be worth it?

	// Side that played the move
	_player = !_player;

	// Put the pieces back
	_array[move.start_row][move.start_col] = p1;

	// TODO: handle the special cases (castling, en passant, promotion?)
	if (is_castle) {

		// The rook must be restored too, then the pre-unmake king square cleared
		int direction = (move.end_col - move.start_col) / 2;
		_array[move.end_row][move.end_col + direction] = w_rook + 6 * _player;
		_array[move.end_row][move.end_col] = none;
	}
	else if (is_en_passant) {

		// The enemy pawn has to go back to the right square
		_array[move.start_row][move.end_col] = w_pawn + 6 * _player;

		// Clear the capture square
		_array[move.end_row][move.end_col] = none;
	}

	// By default, put the captured piece back on its square
	else {
		_array[move.end_row][move.end_col] = p2;
	}


	if (_player) {
		_castling_rights.k_w = k_castle;
		_castling_rights.q_w = q_castle;
	}
	else {
		_castling_rights.k_b = k_castle;
		_castling_rights.q_b = q_castle;
	}

	_half_moves_count = prev_half_count;
	!_player && _moves_count--;
	_en_passant_col = en_passant_col;

	if (p1 == w_king) {
		_white_king_pos.row = move.start_row;
		_white_king_pos.col = move.start_col;
	}
	else if (p1 == b_king) {
		_black_king_pos.row = move.start_row;
		_black_king_pos.col = move.start_col;
	}
}

// Returns how far the game has progressed (0 = opening, 1 = endgame)
int Board::game_over(int max_repetitions) {

	// Do not recompute when already done
	if (_game_over_checked)
		return _game_over_value;
		
	// So that it is not recomputed
	_game_over_checked = true;

	if (repetition_count() >= max_repetitions)
		return draw;

	// Compute the legal moves
	if (_got_moves == -1)
		get_moves();

	// No legal move means either checkmate or stalemate
	if (_got_moves == 0) {
		
		// Mat
		if (in_check())
			return _player ? black_win : white_win;

		// Pat
		return draw;
	}

	// Fifty-move rule
	if (_half_moves_count >= max_half_moves)
		return draw;

	// Insufficient material
	uint8_t count_w_knight = 0;
	uint8_t count_w_bishop = 0;
	uint8_t count_b_knight = 0;
	uint8_t count_b_bishop = 0;

	for (uint8_t i = 0; i < 8; i++) {
		for (uint8_t j = 0; j < 8; j++) {
			const uint8_t p = _array[i][j];
			if (p == w_knight)
				count_w_knight++;
			else if (p == w_bishop)
				count_w_bishop++;
			else if (p == b_knight)
				count_b_knight++;
			else if (p == b_bishop)
				count_b_bishop++;
			// Major pieces or a pawn -> mate is possible
			else if (p != w_king && p != b_king && p != none)
				return unterminated;

			// At least 1 bishop plus a knight/bishop -> no longer a draw by insufficient material
			if ((count_w_bishop > 0) && (count_w_knight > 0 || count_w_bishop > 1))
				return unterminated;
		}
	}

	// Draw possibilities through insufficient material
	if (count_w_knight + count_w_bishop < 2 && count_b_knight + count_b_bishop < 2)
		return draw;

	// Two knights alone cannot force mate
	// TODO: is the game actually declared drawn?
	/*if (count_w_knight == 2 || count_b_knight == 2) {
		_game_over_value = 2;
		return 2;
	}*/

	return unterminated;
}

// Returns the winner when the game is over
int Board::is_game_over(int max_repetitions) {
	_game_over_value = game_over(max_repetitions);
	//cout << "Game over value: " << (int)_game_over_value << endl;
	return _game_over_value;
}

// Returns the label of a move
// En passant missing, checks too, then castling, promotions, mate/stalemate

// Resets the board to its base state, for the buffer
// FIXME: would allocating a fresh board be faster, and safer memory-wise?
void Board::reset_board(const bool display) {
	_got_moves = -1;
	_player_in_check = false;
	_is_active = false;
	_game_over_checked = false;
	_game_over_value = unterminated;
	//_moves_flags_assigned = false;
	_en_passant_col = -1;
	_sorted_moves = false;
	_zobrist_key = 0;
	reset_positions_history();

	if (display)
		cout << "board reset done" << endl;

	return;
}

// Computes and returns the king safety value
bool Board::is_capturable(const int row, const int col) {
	_got_moves == -1 && get_moves();

	// FIXME *** manque l'en-passant

	for (int k = 0; k < _got_moves; k++)
		if (_moves[k].end_row == row && _moves[k].end_col == col)
			return true;

	return false;
}

// Prints the PGN

// Tells whether two positions, given as FENs, are the same
bool equal_fen(const string& fen_a, const string& fen_b) {
	size_t k = fen_a.find(' ');
	k = fen_a.find(' ', k + 1);
	k = fen_a.find(' ', k + 1);
	k = fen_a.find(' ', k + 1);
	const string simple_fen_a = fen_a.substr(0, k);

	k = fen_b.find(' ');
	k = fen_b.find(' ', k + 1);
	k = fen_b.find(' ', k + 1);
	k = fen_b.find(' ', k + 1);
	const string simple_fen_b = fen_b.substr(0, k);

	return (simple_fen_a == simple_fen_b);
}

// Tells whether two positions, given as FENs, are the same, for repetition detection
bool equal_positions(const Board& a, const Board& b) {
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++)
			if (a._array[i][j] != b._array[i][j])
				return false;

	return (a._player == b._player && a._castling_rights == b._castling_rights && a._en_passant_col == b._en_passant_col);
}

// Returns a simple, cheap representation of the position
string Board::simple_position() const
{
	string s;
	for (int row = 0; row < 8; row++)
		for (int col = 0; col < 8; col++)
			s += _array[row][col];

	s += _player + _castling_rights.k_b + _castling_rights.k_w + _castling_rights.q_b + _castling_rights.q_w;
	s += _en_passant_col;

	return s;
}

// Computes the pawn structure and returns its value
int Board::get_kings_opposition() {

	// FIXME: should this only count in pawn endgames?

	// Update the king positions
	update_kings_pos();

	// Are the kings in opposition?
	const int d_row = abs(_white_king_pos.row - _black_king_pos.row);
	const int d_col = abs(_white_king_pos.col - _black_king_pos.col);
	if (!((d_row == 0 || d_row == 2) && (d_col == 0 || d_col == 2)))
		return 0;

	// In opposition, the side holding it is the one not to move
	int opposition = -get_color();

	// Opposition only starts to matter in the endgame
	float adv_begin = 0.9f;

	return opposition * (_adv - adv_begin) / (1.0f - adv_begin);
}

// Returns the type of the selected piece
uint8_t Board::selected_piece() const
{
	// Should this be cached to avoid recomputing it?
	if (main_GUI._selected_pos.row == -1 || main_GUI._selected_pos.col == -1)
		return 0;
	return _array[main_GUI._selected_pos.row][main_GUI._selected_pos.col];
}

// Returns the type of the piece just clicked
uint8_t Board::clicked_piece() const
{
	if (main_GUI._clicked_pos.row == -1 || main_GUI._clicked_pos.col == -1)
		return 0;
	return _array[main_GUI._clicked_pos.row][main_GUI._clicked_pos.col];
}

// Tells whether the selected piece belongs to the side to move
bool Board::selected_piece_has_trait() const
{
	return ((_player && is_in_fast(selected_piece(), 1, 6)) || (!_player && is_in_fast(selected_piece(), 7, 12)));
}

// Tells whether the clicked piece belongs to the side to move
bool Board::clicked_piece_has_trait() const
{
	return ((_player && is_in_fast(clicked_piece(), 1, 6)) || (!_player && is_in_fast(clicked_piece(), 7, 12)));
}

// Resets the clocks to their base time
void Board::reset_timers() {
	// Time per player, in ms
	main_GUI._time_white = main_GUI._initial_time_white;
	main_GUI._time_black = main_GUI._initial_time_black;
}

// Resets the board to its initial position
void Board::restart() {
	// Plenty of room to optimise this
	from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	// _pgn = "";
	reset_timers();
}

// Returns the material difference between the two sides

// Counts the rooks on open and semi-open files and returns the value
bool Board::sort_moves() {
	if (_sorted_moves)
		return false;

	// Piece values
	static constexpr int piece_values[13] = { 0, 100, 320, 330, 500, 900, 10000, 100, 320, 330, 500, 900, 10000 };

	// Enemy controls
	const SquareMap& opponent_controls = _player ? get_black_controls_map() : get_white_controls_map();

	// Static stack-local scratch array for the indices and scores
	struct MoveScore { int index; int score; };
	MoveScore scored_moves[128]; // 128 is far more than a chess position needs
	int move_count = 0;

	for (int i = 0; i < _got_moves; ++i) {
		Move& move = _moves[i];
		int from = _array[move.start_row][move.start_col];
		int to = _array[move.end_row][move.end_col];

		assign_move_flags(&move); // when assign_move_flags is light or bitboard-based

		int score = 0;
		int piece_value = piece_values[from];
		int captured_value = piece_values[to];

		// MVV-LVA
		if (to != none)
			score += 100 * captured_value - piece_value;

		// Promotion
		if (move.is_promotion())
			score += 80000;

		// Check
		else if (move.is_check()) {
			score += 5000;

			// Et mat
			if (move.is_checkmate())
				score += 1000000;
		}

		// The piece is threatened
		int opponent_control = opponent_controls._array[move.end_row][move.end_col];
		if (opponent_control > 0)
			score -= min(50.0 * piece_value, sqrt(opponent_control) * 35 * piece_value);

		// Castling / king
		if (is_king(from)) {
			if (abs(move.end_col - move.start_col) == 2)
				score += 4000; // Roque
			else
				score -= 10000 * (1.0f - _adv);
		}

		scored_moves[move_count++] = { i, score };
	}

	// Sort by descending score
	std::sort(scored_moves, scored_moves + move_count,
		[](const MoveScore& a, const MoveScore& b) { return a.score > b.score; });

	// Write the sorted _moves back
	Move temp_moves[max_moves];
	for (int i = 0; i < move_count; ++i)
		temp_moves[i] = _moves[scored_moves[i].index];

	for (int i = 0; i < move_count; ++i)
		_moves[i] = temp_moves[i];

	_sorted_moves = true;
	return true;
}


// Clicks the move m

// Returns the colour of the side to move (1 for White, -1 for Black)
int Board::get_color() const
{
	return _player ? 1 : -1;
}

// Computes the space advantage
bool Board::update_kings_pos()
{
	// Check whether the king positions are already known
	bool search_white = _array[_white_king_pos.row][_white_king_pos.col] != w_king;
	bool search_black = _array[_black_king_pos.row][_black_king_pos.col] != b_king;

	// Print the assumed king positions, along with the piece value on that square
	/*cout << "White king pos : " << _white_king_pos.i << " " << _white_king_pos.j << " " << (int)_array[_white_king_pos.i][_white_king_pos.j] << endl;
	cout << "Black king pos : " << _black_king_pos.i << " " << _black_king_pos.j << " " << (int)_array[_black_king_pos.i][_black_king_pos.j] << endl;
	cout << search_white << " " << search_black << endl;*/

	if (!search_white && !search_black)
		return true;

	// Walk the board
	for (uint8_t i = 0; i < 8; i++) {
		for (uint8_t j = 0; j < 8; j++) {
			if (const uint8_t piece = _array[i][j]; search_white && piece == w_king) {
				_white_king_pos = { i, j };
				if (!search_black)
					return true;
				search_white = false;
			}
			else if (search_black && piece == b_king) {
				_black_king_pos = { i, j };
				if (!search_white)
					return true;
				search_black = false;
			}
		}
	}

	return false;
}

// Returns the piece activity
int Board::moves_generation_benchmark(uint8_t depth, bool main_call)
{
	if (depth == 0)
		return 1;

	if (main_call) {
		for (uint8_t i = 1; i <= depth; i++) {
			clock_t start = clock();
			cout << "Depth " << static_cast<int>(i) << ": ";
			int nodes_main = moves_generation_benchmark(i, false);
			int time = clock() - start;
			time = time == 0 ? 1 : time;
			cout << nodes_main << " nodes in " << time << "ms (" << int_to_round_string(nodes_main / time * 1000) << "N/s)" << endl;
		}

		return 0;
	}

	int nodes = 0;
	get_moves();

	for (uint8_t i = 0; i < _got_moves; i++) {
		Move& m = _moves[i];
		Board b(*this);
		b.make_move(m);
		nodes += b.moves_generation_benchmark(depth - 1, false);
	}

	return nodes;
}

// Returns the value of fianchettoed bishops, or bishops on the long diagonal within 3 squares of the edge
bool Board::is_controlled(int square_i, int square_j, bool player) const
{
	// Look for potential knight attacks
	for (int k = 0; k < 8; k++) {
		const int ni = square_i + knight_directions[k][0];

		// An attacking knight means check, so return true
		if (const int nj = square_j + knight_directions[k][1]; ni >= 0 && ni < 8 && nj >= 0 && nj < 8 && _array[ni][nj] == (2 + player * 6))
			return true;
	}

	// Look along ranks and files

	// Gauche
	for (int j = square_j - 1; j >= 0; j--)
	{
		// If there is a piece
		if (const uint8_t piece = _array[square_i][j]; piece != none)
		{
			// If the piece is not ours, check for a rook, a queen, or a king at distance 1
			if (piece < 7 != player)
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 4 || simple_piece == 5 || (simple_piece == 6 && j == square_j - 1))
					return true;

			break;
		}
	}

	// Droite
	for (int j = square_j + 1; j < 8; j++)
	{
		if (const uint8_t piece = _array[square_i][j]; piece != 0)
		{
			if (piece < 7 != player)
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 4 || simple_piece == 5 || (simple_piece == 6 && j == square_j + 1))
					return true;

			break;
		}
	}

	// Haut
	for (int i = square_i - 1; i >= 0; i--)
	{
		if (const uint8_t piece = _array[i][square_j]; piece != 0)
		{
			if (piece < 7 != player)
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 4 || simple_piece == 5 || (simple_piece == 6 && i == square_i - 1))
					return true;

			break;
		}
	}

	// Bas
	for (int i = square_i + 1; i < 8; i++)
	{
		if (const uint8_t piece = _array[i][square_j]; piece != 0)
		{
			if (piece < 7 != player)
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 4 || simple_piece == 5 || (simple_piece == 6 && i == square_i + 1))
					return true;

			break;
		}
	}

	// Look along the diagonals

	// Diagonale bas-gauche
	for (int i = square_i - 1, j = square_j - 1; i >= 0 && j >= 0; i--, j--)
	{
		if (const uint8_t piece = _array[i][j]; piece != 0)
		{
			if (piece < 7 != player)
			{
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 3 || simple_piece == 5 || (simple_piece == 6 && (abs(square_i - i) == 1)))
					return true;

				// Special case for pawns
				if (piece == 1 && abs(square_j - j) == 1)
					return true;
			}

			break;
		}
	}

	// Diagonal bas-droite
	for (int i = square_i - 1, j = square_j + 1; i >= 0 && j < 8; i--, j++)
	{
		if (const uint8_t piece = _array[i][j]; piece != 0)
		{
			if ((piece < 7) != player)
			{
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 3 || simple_piece == 5 || (simple_piece == 6 && abs(square_i - i) == 1))
					return true;

				// Special case for pawns
				if (piece == 1 && abs(square_j - j) == 1)
					return true;
			}
			break;
		}
	}

	// Diagonale haut-gauche
	for (int i = square_i + 1, j = square_j - 1; i < 8 && j >= 0; i++, j--)
	{
		if (const uint8_t piece = _array[i][j]; piece != 0)
		{
			if ((piece < 7) != player)
			{
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 3 || simple_piece == 5 || (simple_piece == 6 && abs(square_i - i) == 1))
					return true;

				// Special case for pawns
				if (piece == 7 && abs(square_j - j) == 1)
					return true;
			}
			break;
		}
	}

	// Diagonale haut-droite
	for (int i = square_i + 1, j = square_j + 1; i < 8 && j < 8; i++, j++)
	{
		if (const uint8_t piece = _array[i][j]; piece != 0)
		{
			if ((piece < 7) != player)
			{
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 3 || simple_piece == 5 || (simple_piece == 6 && abs(square_j - j) == 1))
					return true;

				// Special case for pawns
				if (piece == 7 && abs(square_j - j) == 1)
					return true;
			}
			break;
		}
	}

	return false;
}


// Computes and returns the value of the pawn advance threats
bool Board::operator== (const Board& b) const
{
	// Compare the pieces
	for (uint8_t i = 0; i < 8; i++)
		for (uint8_t j = 0; j < 8; j++)
			if (_array[i][j] != b._array[i][j])
				return false;

	// Compare the castling rights
	/*if (_castling_rights != b._castling_rights)
		return false;*/

	// Comparaison de l'en passant
	/*if (_en_passant_col != b._en_passant_col)
		return false;*/

	return true;
}

// Computes and returns the value of the pawns blocking the bishops
void Board::get_zobrist_key()
{
	// FIXME: is it computed several times?

	// #2: no copy of the Zobrist struct (~6 KB) per call - take a reference.
	// The keys are generated once at startup (TranspositionTable::init).
	// Idempotent guard (generate_zobrist_keys() early-returns when already done):
	// it operates on the real object, never on a copy.
	if (!transposition_table._zobrist._keys_generated)
		transposition_table._zobrist.generate_zobrist_keys();

	const Zobrist& zobrist = transposition_table._zobrist;
	
	// (keys guaranteed generated above - #2)

	// Zobrist key
	uint_fast64_t zobrist_key = zobrist._initial_key;

	// Zobrist key for the pieces
	for (uint8_t i = 0; i < 8; i++) {
		for (uint8_t j = 0; j < 8; j++) {
			// Piece number
			uint8_t p = _array[i][j];

			// If the square is not empty
			if (p != 0) {
				// Square number of the piece
				uint8_t square = i * 8 + j;

				// Matching Zobrist entry (p is 1..12, array indices 0..11)
				zobrist_key ^= zobrist._board_keys[square][p - 1];
			}
		}
	}

	// Zobrist key for the castling rights
	uint8_t castling_rights = _castling_rights.k_w + _castling_rights.q_w * 2 + _castling_rights.k_b * 4 + _castling_rights.q_b * 8;
	zobrist_key ^= zobrist._castling_keys[castling_rights];

	// Zobrist key for en passant
	if (_en_passant_col != -1)
		zobrist_key ^= zobrist._en_passant_keys[_en_passant_col];

	// Zobrist key for the side to move
	if (_player == 1)
		zobrist_key ^= zobrist._player_key;

	_zobrist_key = zobrist_key;
}

// Returns how winnable the game is, from 0 to 1, for a given colour
long long int Board::count_nodes_at_depth(int depth, bool display, bool main) {

	if (depth == 0) {
		return 1;
	}

	get_moves();
	long long int nodes_count = 0;

	Board b;

	for (int m = 0; m < _got_moves; m++) {
		b.minimal_copy_data(*this);

		if (display) {
			cout << move_label(_moves[m]) << ", ";
		}

		b.make_move(_moves[m]);
		//b.make_move_fast(_moves[m]);
		//b.make_move(_moves[m], false, false, true);

		long long int below_nodes = b.count_nodes_at_depth(depth - 1, false);

		if (display) {
			cout << below_nodes << endl;
		}

		nodes_count += below_nodes;
	}

	return nodes_count;
}

// Parallelised version
long long int Board::count_nodes_at_depth_parallelized(int depth, bool display, bool main) {
	if (depth == 0)
		return 1;

	get_moves();
	long long int nodes_count = 0;

	if (main) {
		vector<future<long long int>> futures;
		futures.reserve(_got_moves);

		for (int m = 0; m < _got_moves; m++) {
			Board copy;
			copy.minimal_copy_data(*this);
			copy.make_move(_moves[m]);

			futures.emplace_back(async(launch::async, [copy, depth]() mutable {
				return copy.count_nodes_at_depth_parallelized(depth - 1, false);
				}));
		}

		for (auto& f : futures)
			nodes_count += f.get();

		return nodes_count;
	}

	Board b;
	for (int m = 0; m < _got_moves; m++) {
		b.minimal_copy_data(*this);
		b.make_move(_moves[m]);
		nodes_count += b.count_nodes_at_depth(depth - 1, false);
	}

	return nodes_count;
}

// Tells whether the node count computed for a position at a given depth matches the expected one
bool Board::validate_nodes_count_at_depth(string fen, int depth, vector<long long int> expected_nodes, bool display, bool display_full, bool parallel) {

	// Set up the position
	if (fen != "") {
		if (display) {
			cout << endl << (parallel ? "*** parallel execution ***" : "*** single thread execution ***") << endl;
			cout << fen << endl;
		}

		from_fen(fen);
	}

	clock_t begin_time;
	bool success = true;
	bool validation_existence = true;

	for (int d = 0; d <= depth; d++) {

		if (display) {
			begin_time = clock();
			cout << "Depth " << d << "...\r";
		}

		//int nodes = count_nodes_at_depth(d);
		long long int nodes = parallel ? count_nodes_at_depth_parallelized(d, display_full, true) : count_nodes_at_depth(d, display_full, true);
		
		if (expected_nodes.size() <= d) {
			cout << "Missing expected nodes in nodes count validation" << endl;
			//return false;
			validation_existence = false;
		}

		long long int expected_nodes_at_depth = validation_existence ? expected_nodes[d] : 0;

		if (display) {
			int total_time = clock() - begin_time;
			string speed = total_time == 0 ? "N/A" : (int_to_round_string(nodes / total_time * CLOCKS_PER_SEC) + "N / s");

			cout << "Depth " << d << ", expected nodes : " << expected_nodes_at_depth << " / actual nodes : " << nodes << " | " << ((nodes == expected_nodes_at_depth) ? "OK" : "FAIL") << ", time : " << total_time << " ms, speed : " << speed << endl;
		}


		if (nodes != expected_nodes_at_depth) {
			success = false;
			break;
			//return false;
		}
	}

	if (display) {
		cout << "Validation " << (success ? "succeeded" : "failed") << endl;
	}

	return success;
}

// Runs the same validation repeatedly and prints the mean, min, max and standard deviation of the timings
void Board::benchmark_nodes_count_at_depth(string fen, int depth, vector<long long int> expected_nodes, int iterations, bool display, bool parallel) {

	// Set up the position
	if (fen != "") {
		cout << endl << "*** Benchmarking nodes count at depth " << depth << " over " << iterations << " iterations ***" << endl;
		cout << (parallel ? "*** parallel execution ***" : "*** single thread execution ***") << endl;
		cout << fen << endl;
		from_fen(fen);
	}

	vector<double> times;
	times.reserve(iterations);

	for (int it = 0; it < iterations; it++) {
		clock_t begin_time = clock();

		// Validation
		validate_nodes_count_at_depth("", depth, expected_nodes, display, false, parallel);
		double total_time = double(clock() - begin_time) / CLOCKS_PER_SEC;
		times.push_back(total_time);
		cout << "Iteration " << (it + 1) << " / " << iterations << " completed in " << total_time << " s" << endl;
	}

	// Compute the statistics
	double sum = accumulate(times.begin(), times.end(), 0.0);
	double mean = sum / times.size();
	double sq_sum = inner_product(times.begin(), times.end(), times.begin(), 0.0);
	double stdev = sqrt(sq_sum / times.size() - mean * mean);
	auto [min_it, max_it] = minmax_element(times.begin(), times.end());
	double min_time = *min_it;
	double max_time = *max_it;

	cout << "Benchmark results over " << iterations << " iterations :" << endl;
	cout << "Average time: " << mean << " s" << endl;
	cout << "Minimum time: " << min_time << " s" << endl;
	cout << "Maximum time: " << max_time << " s" << endl;
	cout << "Standard deviation: " << stdev << " s" << endl;
}

// Test function: new piece mobility
bool Board::pawn_can_move(uint8_t row, uint8_t col, bool color) const {

	// Make sure this is a pawn
	if (color) {
		if (_array[row][col] != w_pawn) {
			return false;
		}
	}
	else {
		if (_array[row][col] != b_pawn) {
			return false;
		}
	}

	if (color) {
		if (_array[row + 1][col] == none) {
			return true;
		}
		if (col > 0 && is_black(_array[row + 1][col - 1])) {
			return true;
		}
		if (col < 7 && is_black(_array[row + 1][col + 1])) {
			return true;
		}
	}
	else {
		if (_array[row - 1][col] == none) {
			return true;
		}
		if (col > 0 && is_white(_array[row - 1][col - 1])) {
			return true;
		}
		if (col < 7 && is_white(_array[row - 1][col + 1])) {
			return true;
		}
	}

	return false;
}

// Returns the uncertainty of the position
void Board::switch_colors() {
	//rnbqkbnr/pppp1ppp/8/4p3/8/8/PPPPPPPP/RNBQKBNR b KQkq - 1 2
	//rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1

	// Inversion du trait
	_player = !_player;

	// Inversion of the castling rights
	bool b_king_castling = _castling_rights.k_b;
	bool b_queen_castling = _castling_rights.q_b;
	bool w_king_castling = _castling_rights.k_w;
	bool w_queen_castling = _castling_rights.q_w;

	_castling_rights.k_b = w_king_castling;
	_castling_rights.q_b = w_queen_castling;
	_castling_rights.k_w = b_king_castling;
	_castling_rights.q_w = b_queen_castling;

	_got_moves = -1;
	_player_in_check = false;
	//_moves_flags_assigned = false;
	_sorted_moves = false;

	// TODO

	// King pos

	// Zobrist


	Board switched_board = *this;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			const uint8_t piece = _array[row][col];

			const uint8_t opposite_piece = is_white(piece) ? piece + 6 : is_black(piece) ? piece - 6 : piece;

			switched_board._array[7 - row][col] = opposite_piece;
		}
	}

	*this = switched_board;


	// Reset the search
	main_GUI.reset_buffers(); // #6: systematic TT/node_map clear
	main_GUI._root_exploration_node->reset();
}

// Iterates over the distance map from a given position and returns the newly controlled squares
bool Board::is_pawn_endgame() const {
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t piece = _array[row][col];
			if (piece != none && !is_pawn(piece) && !is_king(piece)) {
				return false;
			}
		}
	}

	return true;
}

// Tells whether the side still has pieces, king and pawns aside
bool Board::has_pieces(bool color) const {
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t piece = _array[row][col];
			if (piece != none && !is_pawn(piece) && !is_king(piece) && (is_white(piece) == color)) {
				return true;
			}
		}
	}

	return false;
}

// Returns the true value of a long-term king weakness term, from the castling options and their proximity

// Returns a map of every blocked pawn
SquareMap Board::get_blocked_pawns(bool color) const {

	// The king counts as blocking, whatever moves it has
	// FIXME: perhaps not. It makes some sense in the opening, but not in the endgame where the king is mobile

	// Map initialisation
	SquareMap blocked_pawns;

	// For each pawn
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {

			uint8_t piece = _array[row][col];

			// White pawn
			if (color && piece == w_pawn) {
				// If the pawn is blocked, by an enemy piece or friendly pawn ahead, with no capture available
				if ((is_black(_array[row + 1][col]) || _array[row + 1][col] == w_pawn) && !(col > 0 && is_black(_array[row + 1][col - 1])) && !(col < 7 && is_black(_array[row + 1][col + 1]))) {
					blocked_pawns._array[row][col] = 1;
				}
			}

			// White king
			//else if (color && piece == w_king) {
			//	blocked_pawns._array[row][col] = 1;
			//}

			// Black pawn
			else if (!color && piece == b_pawn) {
				// If the pawn is blocked
				if ((is_white(_array[row - 1][col]) || _array[row - 1][col] == b_pawn) && !(col > 0 && is_white(_array[row - 1][col - 1])) && !(col < 7 && is_white(_array[row - 1][col + 1]))) {
					blocked_pawns._array[row][col] = 1;
				}
			}

			// Black king
			//else if (!color && piece == b_king) {
			//	blocked_pawns._array[row][col] = 1;
			//}
		}
	}

	return blocked_pawns;
}


// Takes a map of blocked pawns and pieces, updates it with the newly blocked ones, and reports whether any were added
bool Board::update_blocked_pieces(SquareMap& blocked_pieces, bool color, SquareMap opponent_controls) const {

	// rn1qkbnr/pbp1p1p1/1p1pPpPp/3P1P2/8/8/PPP4P/RNBQKBNR b KQkq - 0 10

	bool new_blocked = false;

	// Look at every piece
	// A piece with at least one available move is not blocked

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t piece = _array[row][col];

			if (!is_white(piece) == color || piece == none || blocked_pieces._array[row][col]) {
				continue;
			}
			
			// Knight
			if (is_knight(piece)) {
				bool is_blocked = true;

				for (uint8_t m = 0; m < 8 && is_blocked; m++) {
					int new_row = row + knight_directions[m][0];
					int new_col = col + knight_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !blocked_pieces._array[new_row][new_col]) {
						is_blocked = false;
						break;
					}
				}

				if (is_blocked) {
					blocked_pieces._array[row][col] = 1;
					new_blocked = true;
				}
			}

			// Bishop
			else if (is_bishop(piece)) {
				bool is_blocked = true;

				for (uint8_t m = 0; m < 4 && is_blocked; m++) {
					int new_row = row + diag_directions[m][0];
					int new_col = col + diag_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !blocked_pieces._array[new_row][new_col]) {
						is_blocked = false;
						break;
					}
				}

				if (is_blocked) {
					blocked_pieces._array[row][col] = 1;
					new_blocked = true;
				}
			}

			// Rook
			else if (is_rook(piece)) {
				bool is_blocked = true;

				for (uint8_t m = 0; m < 4 && is_blocked; m++) {
					int new_row = row + rect_directions[m][0];
					int new_col = col + rect_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !blocked_pieces._array[new_row][new_col]) {
						is_blocked = false;
						break;
					}
				}

				if (is_blocked) {
					blocked_pieces._array[row][col] = 1;
					new_blocked = true;
				}
			}

			// Queen
			else if (is_queen(piece)) {
				bool is_blocked = true;

				for (uint8_t m = 0; m < 8 && is_blocked; m++) {

					int new_row = row + all_directions[m][0];
					int new_col = col + all_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !blocked_pieces._array[new_row][new_col]) {
						is_blocked = false;
						break;
					}
				}

				if (is_blocked) {
					blocked_pieces._array[row][col] = 1;
					new_blocked = true;
				}
			}

			// King
			else if (is_king(piece)) {
				bool is_blocked = true;

				for (uint8_t m = 0; m < 8 && is_blocked; m++) {

					int new_row = row + all_directions[m][0];
					int new_col = col + all_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !blocked_pieces._array[new_row][new_col] && !opponent_controls._array[new_row][new_col]) {
						is_blocked = false;
						break;
					}
				}
				if (is_blocked) {
					blocked_pieces._array[row][col] = 1;
					new_blocked = true;
				}
			}
		}
	}

	return new_blocked;
}

// Returns the map of every blocked piece
SquareMap Board::get_all_blocked_pieces(bool color, SquareMap opponent_controls) const {

	// FIXME: how do we detect two pieces blocking each other, such as two adjacent rooks or a rook beside a knight? They currently do not count as blocking one another
	// TODO: an alternative approach:
	// - a map of every piece with no legal move, plus a map of the "newly" freed pieces with at least one
	// - while the newly freed map is non-empty, and so is the other one
	// - iterate over the blocked map and check whether any newly freed piece frees a blocked one
	
	// rn1qkbnr/pbp1p1pr/1p1pPpPp/3P1P1P/8/8/PPP5/RNBQKBNR b KQkq - 0 12: blocked here are the f8 bishop, g8 knight, h8 rook, h7 rook and several pawns. The catch: the rooks do not count each other as blocking
	// rn1qkbnr/pbp1p1pn/1p1pPpPp/3P1PpP/6P1/8/PPP5/RNBQKBNR b KQkq - 0 12: blocked here are the f8 bishop, g8 knight, h8 rook and the h7 knight
	// b1N3kr/7p/6pB/4p3/8/8/PP3P1P/4K3 w - - 0 32: king blocked with the rook stuck behind it, after Nd6
	// 6kr/7p/3N3B/3b4/8/8/8/6K1 w - - 0 1: same case, simplified

	//// Blocked pawn map
	//SquareMap blocked_pieces = get_blocked_pawns(color);

	////cout << "initial " << (color ? "white" : "black") << " blocked pawns:" << endl;
	////blocked_pieces.print();

	//// TODO *** to think about: why would we start with the pawns?
	//// Why start from the kings in the update? Because depending on the controlled squares, the king may no longer be able to move

	//// While new blocked pieces keep turning up
	//while (true) {
	//	bool new_blocked = update_blocked_pieces(blocked_pieces, color, opponent_controls);

	//	if (!new_blocked) {
	//		//cout << "done" << endl;
	//		break;
	//	}

	//	//cout << "new blocked pieces:" << endl;
	//	//blocked_pieces.print();
	//}


	SquareMap blocked_pieces;

	bool update = true;

	while (update) {
		update = get_blocked_and_unblocked_pieces(blocked_pieces, color, opponent_controls);

		//cout << "blocked pieces for " << (color ? "white" : "black") << ":" << endl;
		//blocked_pieces.print();
	}

	// -1 -> 0 (not considered)
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			if (blocked_pieces._array[row][col] == -1) {
				blocked_pieces._array[row][col] = 0;
			}
		}
	}

	//cout << "final blocked pieces for " << (color ? "white" : "black") << ":" << endl;
	//blocked_pieces.print();

	return blocked_pieces;
}

// Returns the map of every piece currently blocked (1) and newly freed (-1)
bool Board::get_blocked_and_unblocked_pieces(SquareMap& pieces_states, bool color, SquareMap opponent_controls) const {
	
	// Blocked pieces: pieces with no legal moves, while allowing unblocked pieces' squares

	// Is at least one piece freed?
	bool has_unblocked_pieces = false;
	bool has_blocked_pieces = false;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t piece = _array[row][col];

			if (!is_ally(piece, color) || piece == none || pieces_states._array[row][col] == -1) {
				continue;
			}

			// Pawn
			if (is_pawn(piece)) {
				int forward_row = row + (color ? 1 : -1);

				// If the pawn is blocked
				if ((_array[forward_row][col] != none && pieces_states._array[forward_row][col] != -1) && !(col > 0 && is_enemy(_array[forward_row][col - 1], color)) && !(col < 7 && is_enemy(_array[forward_row][col + 1], color))) {
					pieces_states._array[row][col] = 1;
					has_blocked_pieces = true;
				}
				else {
					pieces_states._array[row][col] = -1;
					has_unblocked_pieces = true;
				}
			}

			// Knight
			else if (is_knight(piece)) {
				pieces_states._array[row][col] = 1;

				for (uint8_t m = 0; m < 8; m++) {
					int new_row = row + knight_directions[m][0];
					int new_col = col + knight_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && (!is_ally(_array[new_row][new_col], color) || pieces_states._array[new_row][new_col] == -1)) {
						pieces_states._array[row][col] = -1;
						has_unblocked_pieces = true;
						break;
					}
				}

				if (!has_blocked_pieces && pieces_states._array[row][col] == 1) {
					has_blocked_pieces = true;
				}
			}

			// Bishop
			else if (is_bishop(piece)) {
				pieces_states._array[row][col] = 1;

				for (uint8_t m = 0; m < 4; m++) {
					int new_row = row + diag_directions[m][0];
					int new_col = col + diag_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && (!is_ally(_array[new_row][new_col], color) || pieces_states._array[new_row][new_col] == -1)) {
						pieces_states._array[row][col] = -1;
						has_unblocked_pieces = true;
						break;
					}
				}


				if (!has_blocked_pieces && pieces_states._array[row][col] == 1) {
					has_blocked_pieces = true;
				}
			}

			// Rook
			else if (is_rook(piece)) {
				pieces_states._array[row][col] = 1;

				for (uint8_t m = 0; m < 4; m++) {
					int new_row = row + rect_directions[m][0];
					int new_col = col + rect_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && (!is_ally(_array[new_row][new_col], color) || pieces_states._array[new_row][new_col] == -1)) {
						pieces_states._array[row][col] = -1;
						has_unblocked_pieces = true;
						break;
					}
				}


				if (!has_blocked_pieces && pieces_states._array[row][col] == 1) {
					has_blocked_pieces = true;
				}
			}

			// Queen
			else if (is_queen(piece)) {
				pieces_states._array[row][col] = 1;

				for (uint8_t m = 0; m < 8; m++) {
					int new_row = row + all_directions[m][0];
					int new_col = col + all_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && (!is_ally(_array[new_row][new_col], color) || pieces_states._array[new_row][new_col] == -1)) {
						pieces_states._array[row][col] = -1;
						has_unblocked_pieces = true;
						break;
					}
				}


				if (!has_blocked_pieces && pieces_states._array[row][col] == 1) {
					has_blocked_pieces = true;
				}
			}

			// King
			else if (is_king(piece)) {
				pieces_states._array[row][col] = 1;

				for (uint8_t m = 0; m < 8; m++) {

					int new_row = row + all_directions[m][0];
					int new_col = col + all_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && (!is_ally(_array[new_row][new_col], color) || pieces_states._array[new_row][new_col] == -1) && !opponent_controls._array[new_row][new_col]) {
						pieces_states._array[row][col] = -1;
						has_unblocked_pieces = true;
						break;
					}
				}

				if (!has_blocked_pieces && pieces_states._array[row][col] == 1) {
					has_blocked_pieces = true;
				}
			}
		}
	}

	return has_unblocked_pieces && has_blocked_pieces;
}

// Returns the map of the squares controlled by the pawns
SquareMap Board::get_pawns_controls(bool color) const {

	// Control map
	SquareMap controls;

	// Friendly pawn
	uint8_t player_pawn = color ? w_pawn : b_pawn;

	// Pawn direction
	const int pawn_direction = color ? 1 : -1;

	// For each pawn
	for (uint8_t row = 1; row < 7; row++) {
		for (uint8_t col = 0; col < 8; col++) {

			// Friendly pawn
			if (_array[row][col] == player_pawn) {
				(col > 0) && (controls._array[row + pawn_direction][col - 1] = true);
				(col < 7) && (controls._array[row + pawn_direction][col + 1] = true);
			}
		}
	}
	return controls;
}


// Returns the real piece mobility, short term

// Returns the virtual piece mobility, long term

// Returns the queen safety value for the given side

// Reports how quiet a position is: the number of available captures, checks and promotions
int Board::get_quietness() {

	int captures = 0;
	int checks = 0;
	int promotions = 0;

	// Look at every move
	for (uint8_t m = 0; m < _got_moves; m++) {

		Move& move = _moves[m];

		// If this is a capture
		if (move.is_capture()) {
			captures++;
		}

		// If this is a promotion
		else if (move.is_promotion()) {
			promotions++;
		}

		// If this is a check
		else if (move.is_check()) {
			checks++;
		}
	}

	int quietness = captures + checks + promotions;

	return quietness;
}

// Assigns the flags of every possible move
void Board::assign_all_move_flags() {

	//if (_moves_flags_assigned) {
	//	return; // The flags have already been assigned
	//}

	for (uint8_t m = 0; m < _got_moves; m++) {
		Move& move = _moves[m];
		assign_move_flags(&move);
	}

	//// The flags have been assigned
	//_moves_flags_assigned = true;
}

// Flips the side to move
void Board::switch_trait() {
	_player = !_player;
	_got_moves = -1;
	_player_in_check = false;
	//_moves_flags_assigned = false;
	_sorted_moves = false;
	_game_over_checked = false;
}

// Assigns the flags of a given move
void Board::assign_move_flags(Move* move) {

	// r3k3/p1p2pp1/2p5/1p2P3/1qPK4/7r/PP1PR3/R1BQ4 b q - 0 22 : Dxc4# ?

	// If the flags have already been computed
	if (move->has_flags()) {
		return;
	}

	move->flags = 0;  // Reset the flags

	const uint8_t piece = _array[move->start_row][move->start_col];
	const uint8_t dest = _array[move->end_row][move->end_col];

	// Promotion
	if ((piece == w_pawn && move->end_row == 7) ||
		(piece == b_pawn && move->end_row == 0)) {
		move->set_flag(IS_PROMOTION);
		move->set_promo_piece(PROMO_QUEEN);
	}

	// Capture normale ou en passant
	if (dest != none || (is_pawn(piece) && move->start_col != move->end_col)) {
		move->set_flag(IS_CAPTURE);
	}

	// Evaluate on a copy using save/restore (cheaper than Board copy)
	uint8_t saved_array[8][8];
	memcpy(saved_array, _array, sizeof(saved_array));
	const bool saved_player = _player;
	const Pos saved_wk = _white_king_pos;
	const Pos saved_bk = _black_king_pos;
	const CastlingRights saved_castling = _castling_rights;
	const int saved_ep = _en_passant_col;
	const int saved_half = _half_moves_count;
	const int saved_moves_count = _moves_count;
	const int8_t saved_got_moves = _got_moves;
	const bool saved_player_in_check = _player_in_check;
	const bool saved_game_over_checked = _game_over_checked;
	const int saved_game_over_value = _game_over_value;
	uint64_t saved_bitboards[sizeof(_bitboards) / sizeof(uint64_t)];
	memcpy(saved_bitboards, _bitboards, sizeof(saved_bitboards));
	uint64_t saved_occupancies[sizeof(_occupancies) / sizeof(uint64_t)];
	memcpy(saved_occupancies, _occupancies, sizeof(saved_occupancies));
	const bool saved_sorted_moves = _sorted_moves;
	const uint64_t saved_zobrist_key = _zobrist_key;

	make_move(*move);

	// Check and mate detection — save results, apply AFTER restore.
	bool detected_check = false;
	bool detected_mate = false;
	if (in_check()) {
		detected_check = true;

		// Save _moves[] before get_moves() overwrites it (only needed here)
		Move saved_moves[max_moves];
		memcpy(saved_moves, _moves, sizeof(saved_moves));

		get_moves();
		if (_got_moves == 0) {
			detected_mate = true;
		}

		// Restore _moves[] after get_moves() corrupted it
		memcpy(_moves, saved_moves, sizeof(saved_moves));
	}

	// Restore state
	memcpy(_array, saved_array, sizeof(saved_array));
	_player = saved_player;
	_white_king_pos = saved_wk;
	_black_king_pos = saved_bk;
	_castling_rights = saved_castling;
	_en_passant_col = saved_ep;
	_half_moves_count = saved_half;
	_moves_count = saved_moves_count;
	_got_moves = saved_got_moves;
	_player_in_check = saved_player_in_check;
	_game_over_checked = saved_game_over_checked;
	_game_over_value = saved_game_over_value;
	memcpy(_bitboards, saved_bitboards, sizeof(saved_bitboards));
	memcpy(_occupancies, saved_occupancies, sizeof(saved_occupancies));
	_sorted_moves = saved_sorted_moves;
	_zobrist_key = saved_zobrist_key;

	// Apply flags AFTER restore so they go onto the correct move
	if (detected_check) {
		move->set_flag(IS_CHECK);
	}
	if (detected_mate) {
		move->set_flag(IS_MATE);
	}



	// Game result
	//move->set_result(static_cast<uint8_t>(b._game_over_value));

	move->set_flag(FLAGS_EVALUATED);
}

// Zeroes the bitboards
void Board::reset_bitboards() {
	for (int i = 0; i < 12; i++)
		_bitboards[i] = 0ULL;
	for (int i = 0; i < 3; i++)
		_occupancies[i] = 0ULL;
}

// Prints every bitboard
void Board::update_bitboards() {
	reset_bitboards();

	for (int row = 0; row < 8; row++) {
		for (int col = 0; col < 8; col++) {
			int piece = _array[row][col];

			int square = row * 8 + col;
			_bitboards[piece] |= 1ULL << square;

			if (piece) {
				int color = (piece > 6) ? 1 : 0;
				_occupancies[color] |= 1ULL << square;
				_occupancies[2] |= 1ULL << square;
			}
		}
	}
}

// Prints every value of a bitboard
void print_bitboard(uint64_t bitboard) {
	for (int row = 7; row >= 0; row--) {
		for (int col = 0; col < 8; col++) {
			int square = row * 8 + col;
			cout << ((bitboard >> square) & 1ULL) << " ";
		}
		cout << "\n";
	}
	cout << "\n";
}

// Prints every bitboard
void Board::print_all_bitboards() const {
	auto print_row = [&](int row, int offset, int count) {
		for (int i = 0; i < count; ++i) {
			uint64_t bb = _bitboards[offset + i];
			for (int col = 0; col < 8; ++col) {
				int sq = row * 8 + col;
				cout << (((bb >> sq) & 1ULL) ? "1 " : ". ");
			}
			cout << "   ";
		}
		cout << endl;
		};

	auto print_occ_row = [&](int row) {
		for (int i = 0; i < 3; ++i) {
			uint64_t bb = _occupancies[i];
			for (int col = 0; col < 8; ++col) {
				int sq = row * 8 + col;
				cout << (((bb >> sq) & 1ULL) ? "1 " : ". ");
			}
			cout << "   ";
		}
		cout << endl;
		};

	cout << "\n=== BITBOARDS (none + white pieces) ===" << endl;
	cout << left << setw(19) << "None"
		<< setw(19) << "w_pawn"
		<< setw(19) << "w_knight"
		<< setw(19) << "w_bishop"
		<< setw(19) << "w_rook"
		<< setw(19) << "w_queen"
		<< setw(19) << "w_king" << endl;

	for (int row = 7; row >= 0; --row)
		print_row(row, 0, 7);

	cout << "\n=== BITBOARDS (black pieces) ===" << endl;
	cout << left << setw(19) << "b_pawn"
		<< setw(19) << "b_knight"
		<< setw(19) << "b_bishop"
		<< setw(19) << "b_rook"
		<< setw(19) << "b_queen"
		<< setw(19) << "b_king" << endl;

	for (int row = 7; row >= 0; --row)
		print_row(row, 7, 6);

	cout << "\n=== OCCUPANCIES ===" << endl;
	cout << left << setw(19) << "White"
		<< setw(19) << "Black"
		<< setw(19) << "Both" << endl;

	for (int row = 7; row >= 0; --row)
		print_occ_row(row);
}

static constexpr uint64_t w_kcastle_mask = SQUARE_MASKS[7] ^ SQUARE_MASKS[5];
static constexpr uint64_t w_qcastle_mask = SQUARE_MASKS[0] ^ SQUARE_MASKS[3];
static constexpr uint64_t b_kcastle_mask = SQUARE_MASKS[63] ^ SQUARE_MASKS[61];
static constexpr uint64_t b_qcastle_mask = SQUARE_MASKS[56] ^ SQUARE_MASKS[59];

// Updates the bitboards for a move
inline void Board::update_bitboards_white(int row1, int col1, int row2, int col2, int p, int p_last, uint8_t promo_piece) noexcept {

	// 0-63 coordinates
	const int from = row1 * 8 + col1;
	const int to = row2 * 8 + col2;

	const uint64_t from_mask = SQUARE_MASKS[from];
	const uint64_t to_mask = SQUARE_MASKS[to];

	// Remove the moved piece from its former square
	_bitboards[p] &= ~from_mask;
	_occupancies[0] &= ~from_mask;
	_occupancies[2] &= ~from_mask;

	// Move the piece within the occupancies
	_occupancies[0] |= to_mask;
	_occupancies[2] |= to_mask;

	// En passant (least likely)
	if (p == w_pawn && col1 != col2 && p_last == none) {
		const uint64_t captured_square_mask = SQUARE_MASKS[from + (col2 - col1)];
		_bitboards[b_pawn] &= ~captured_square_mask;
		_occupancies[1] &= ~captured_square_mask;
		_occupancies[2] &= ~captured_square_mask;
	}

	// Promotion (uses the correct piece from the move)
	if (p == w_pawn && row2 == 7)
		_bitboards[w_queen - promo_piece] |= to_mask;
	else
		_bitboards[p] |= to_mask;

	// Roques (assez peu probables)

	// White kingside castle
	if (p == w_king && col2 == col1 + 2) {
		_bitboards[w_rook] ^= w_kcastle_mask;
		_occupancies[0] ^= w_kcastle_mask;
		_occupancies[2] ^= w_kcastle_mask;
	}

	// White queenside castle
	if (p == w_king && col2 == col1 - 2) {
		_bitboards[w_rook] ^= w_qcastle_mask;
		_occupancies[0] ^= w_qcastle_mask;
		_occupancies[2] ^= w_qcastle_mask;
	}

	// If a piece is captured, remove it as well
	if (p_last != none) {
		_bitboards[p_last] &= ~to_mask;
		_occupancies[1] &= ~to_mask;
	}
}

// Updates the bitboards for a move
inline void Board::update_bitboards_black(int row1, int col1, int row2, int col2, int p, int p_last, uint8_t promo_piece) noexcept {

	// 0-63 coordinates
	const int from = row1 * 8 + col1;
	const int to = row2 * 8 + col2;

	const uint64_t from_mask = SQUARE_MASKS[from];
	const uint64_t to_mask = SQUARE_MASKS[to];

	// Remove the moved piece from its former square
	_bitboards[p] &= ~from_mask;
	_occupancies[1] &= ~from_mask;
	_occupancies[2] &= ~from_mask;

	// Move the piece within the occupancies
	_occupancies[1] |= to_mask;
	_occupancies[2] |= to_mask;

	// En passant (least likely)
	if (p == b_pawn && col1 != col2 && p_last == none) {
		const uint64_t captured_square_mask = SQUARE_MASKS[from + (col2 - col1)];
		_bitboards[w_pawn] &= ~captured_square_mask;
		_occupancies[0] &= ~captured_square_mask;
		_occupancies[2] &= ~captured_square_mask;
	}

	// Promotion (uses the correct piece from the move)
	if (p == b_pawn && row2 == 0)
		_bitboards[b_queen - promo_piece] |= to_mask;
	else
		_bitboards[p] |= to_mask;

	// Roques (assez peu probables)

	// Black kingside castle
	if (p == b_king && col2 == col1 + 2) {
		_bitboards[b_rook] ^= b_kcastle_mask;
		_occupancies[1] ^= b_kcastle_mask;
		_occupancies[2] ^= b_kcastle_mask;
	}

	// Black queenside castle
	if (p == b_king && col2 == col1 - 2) {
		_bitboards[b_rook] ^= b_qcastle_mask;
		_occupancies[1] ^= b_qcastle_mask;
		_occupancies[2] ^= b_qcastle_mask;
	}

	// If a piece is captured, remove it as well
	if (p_last != none) {
		_bitboards[p_last] &= ~to_mask;
		_occupancies[0] &= ~to_mask;
	}
}

// Returns the number of passed pawns for a given colour

// Tells whether a move changes, that is forfeits, the castling rights
bool Board::does_move_change_castling_rights(const Move& move) const noexcept {
	// Moving piece
	const uint8_t p = _array[move.start_row][move.start_col];

	// King move: only matters if that side still has any castling rights
	if (is_king(p)) {
		if ((p == w_king) && (_castling_rights.k_w || _castling_rights.q_w)) return true;
		if ((p == b_king) && (_castling_rights.k_b || _castling_rights.q_b)) return true;
		return false;
	}

	// Rook move from original square
	if (is_rook(p)) {
		if (p == w_rook) {
			if (move.start_row == 0 && move.start_col == 0 && _castling_rights.q_w) return true;
			if (move.start_row == 0 && move.start_col == 7 && _castling_rights.k_w) return true;
		}
		else if (p == b_rook) {
			if (move.start_row == 7 && move.start_col == 0 && _castling_rights.q_b) return true;
			if (move.start_row == 7 && move.start_col == 7 && _castling_rights.k_b) return true;
		}
	}

	// Capture of rook on original square (normal captures)
	if (move.is_capture()) {
		const uint8_t captured = _array[move.end_row][move.end_col];
		if (captured == w_rook) {
			if (move.end_row == 0 && move.end_col == 0 && _castling_rights.q_w) return true;
			if (move.end_row == 0 && move.end_col == 7 && _castling_rights.k_w) return true;
		}
		else if (captured == b_rook) {
			if (move.end_row == 7 && move.end_col == 0 && _castling_rights.q_b) return true;
			if (move.end_row == 7 && move.end_col == 7 && _castling_rights.k_b) return true;
		}
	}

	return false;
}

// Tells whether a move is irreversible, for fast repetition detection
bool Board::is_irreversible_move(const Move& move) const noexcept {
	const uint8_t p = _array[move.start_row][move.start_col];

	// Any pawn move
	if (is_pawn(p)) return true;

	// Any capture
	if (move.is_capture()) return true;

	// Any move that changes castling rights
	if (does_move_change_castling_rights(move)) return true;

	return false;
}

bool Board::is_legal(Move move) {

	// Fetch the moves when needed
	if (_got_moves == -1)
		get_moves();

	// Find the index of the move
	for (int i = 0; i < _got_moves; i++)
		if (move == _moves[i])
			return true;

	return false;
}

void Board::reset_positions_history() {
	_positions_history.clear();
}

int Board::repetition_count() {
	get_zobrist_key();

	const auto it = _positions_history.find(_zobrist_key);
	return it == _positions_history.end() ? 1 : it->second;
}
