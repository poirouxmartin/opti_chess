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
	_player = b._player;
	memcpy(_moves, b._moves, sizeof(_moves));
	//_moves_flags_assigned = b._moves_flags_assigned;
	_sorted_moves = b._sorted_moves;
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
			// While in check, the move must interpose
			(!in_check || is_in_interpose_mask(interposition_mask, new_row, new_col)) && add_move(Move(row, col, new_row, new_col), iterator, piece);
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
			// While in check, the move must interpose
			(!in_check || is_in_interpose_mask(interposition_mask, new_row, new_col)) && add_move(Move(row, col, new_row, new_col), iterator, piece);
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
void Board::display_moves() {
	if (_got_moves == -1)
		get_moves();

	if (_got_moves == 0) {
		cout << "no legal moves" << endl;
		return;
	}

	sort_moves();
	cout << "total moves : " << (int)_got_moves << endl;
	cout << "moves : [|";
	for (int i = 0; i < _got_moves; i++)
		cout << " " << move_label(_moves[i]) << " |";
	cout << "]" << endl;
}

// Prints the board
void Board::display() const {
	cout << "  +-----------------+" << endl;
	for (int row = 7; row >= 0; row--) {
		cout << row + 1 << " | ";
		for (int col = 0; col < 8; col++) {
			cout << short_piece_name(_array[row][col]) << " ";
		}
		cout << "|" << endl;
	}
	cout << "  +-----------------+" << endl;
	cout << "    a b c d e f g h" << endl;
}

// Plays a move
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

	// Promotion (queen only for now)
	(p == w_pawn && row2 == 7) && (_array[row2][col2] = w_queen);
	(p == b_pawn && row2 == 0) && (_array[row2][col2] = b_queen);

	// Clear the origin square
	_array[row1][col1] = none;

	// Update the bitboards
	_player ? update_bitboards_white(row1, col1, row2, col2, p, p_last) : update_bitboards_black(row1, col1, row2, col2, p, p_last);

	// Flip the side to move
	_player = !_player;

	// Increment the move counter
	_player && _moves_count++;

	// Reset the possible move count
	_got_moves = -1;

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

		get_zobrist_key();
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
void Board::game_advancement() {
	if (_advancement)
		return;

	_adv = 0;

	// Local definition of game progress: (p_tot - p) / p_tot, where p_tot is the starting material total (opponent only? both?) and p the current one
	static constexpr int adv_pawn = 2;
	static constexpr int adv_knight = 10;
	static constexpr int adv_bishop = 10;
	static constexpr int adv_rook = 10;
	static constexpr int adv_queen = 50;
	static constexpr int adv_castle = 5;

	// Threshold above which the position counts as an endgame
	static constexpr int endgame_adv = 35;

	static constexpr int p_tot = 2 * (8 * adv_pawn + 2 * adv_knight + 2 * adv_bishop + 2 * adv_rook + 1 * adv_queen + 2 * adv_castle);
	int p = 0;

	static constexpr int values[6] = { 0, adv_pawn, adv_knight, adv_bishop, adv_rook, adv_queen };

	// Pieces
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			const uint8_t piece = _array[row][col];
			piece && (p += values[piece % 6]);
		}
	}

	// Roques
	p += (_castling_rights.k_w + _castling_rights.q_w + _castling_rights.k_b + _castling_rights.q_b) * adv_castle;

	_adv = min(1.0f, static_cast<float>(p_tot - p) / (p_tot - endgame_adv));

	return;
}

// Counts the material on the board and returns its value
int Board::count_material(const Evaluator* eval, float closed_factor) const
{
	int material_count = 0;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			if (const uint8_t piece = _array[row][col]) {
				const int piece_number = (piece - 1) % 6;
				const int piece_begin_value = (1.0f - closed_factor) * eval->_pieces_value_begin_open[piece_number] + closed_factor * eval->_pieces_value_begin_closed[piece_number];
				const int piece_end_value = (1.0f - closed_factor) * eval->_pieces_value_end_open[piece_number] + closed_factor * eval->_pieces_value_end_closed[piece_number];

				const int value = static_cast<int>(static_cast<float>(piece_begin_value) * (1.0f - _adv) + static_cast<float>(piece_end_value) * _adv);

				material_count += (piece < 7) ? value : -value;
			}
		}
	}

	return material_count;
}

// Counts the bishop pairs and returns their value
int Board::count_bishop_pairs() const
{
	//rnnqk2r/ppp1nppp/4p1n1/3pP3/3P1P2/8/PPP3PP/RBBQKBBR w KQkq - 1 5: two bishop pairs for White

	uint8_t w_bishop_light = 0; uint8_t w_bishop_dark = 0;
	uint8_t b_bishop_light = 0; uint8_t b_bishop_dark = 0;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			const uint8_t p = _array[row][col];
			const bool is_light = (row + col) % 2 == 0;
			if (p == w_bishop) {
				if (is_light)
					w_bishop_light++;
				else
					w_bishop_dark++;
			}
			else if (p == b_bishop) {
				if (is_light)
					b_bishop_light++;
				else
					b_bishop_dark++;
			}
		}
	}

	//cout << "w_bishop_light: " << (int)w_bishop_light << " w_bishop_dark: " << (int)w_bishop_dark << endl;
	//cout << "b_bishop_light: " << (int)b_bishop_light << " b_bishop_dark: " << (int)b_bishop_dark << endl;

	return min(w_bishop_light, w_bishop_dark) - min(b_bishop_light, b_bishop_dark);
}

// Counts and returns the penalty value for doubled pieces
int Board::count_doubled_pieces(const Evaluator* eval) const
{
	int penalties = 0;

	// Piece counters by type
	uint8_t piece_counts[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	for (uint8_t col = 0; col < 8; col++) {
		for (uint8_t row = 0; row < 8; row++) {
			if (const uint8_t piece = _array[row][col]) {
				piece_counts[piece - 1]++;
			}
		}
	}

	for (uint8_t i = 0; i < 12; i++) {
		if (piece_counts[i] > 1) {
			const int piece_number = i % 6;
			const int penalty = eval->_doubled_piece_penalty[piece_number];
			penalties += (piece_counts[i] - 1) * penalty * ((i < 6) ? -1 : 1);
		}
	}

	return penalties;
}

// Computes and returns the piece placement value
int Board::pieces_positioning(const Evaluator* eval) const
{
	int pos = 0;

	for (uint8_t i = 0; i < 8; i++) {
		for (uint8_t j = 0; j < 8; j++) {
			if (const uint8_t piece = _array[i][j]) {
				const int value = static_cast<int>(static_cast<float>(eval->_pieces_pos_begin[(piece - 1) % 6][(piece < 7) ? 7 - i : i][j]) * (1.0f - _adv) + static_cast<float>(eval->_pieces_pos_end[(piece - 1) % 6][(piece < 7) ? 7 - i : i][j]) * _adv);
				pos += (piece < 7) ? value : -value;
			}
		}
	}

	return pos;
}

// Evaluates the position using heuristics
void Board::evaluate(Evaluation* eval, Evaluator* evaluator, bool display, Network* n, bool check_game_over)
{
	/*if (_evaluated)
		return false;*/

	// If the transposition table already has it evaluated
	//if (_zobrist_key == 0) {
	//	cout << "Zobrist key is 0" << endl;
	//	get_zobrist_key();
	//}

	//if (transposition_table.contains(_zobrist_key)) {
	//	cout << "Transposition table hit" << endl;
	//	if (transposition_table._hash_table[_zobrist_key]._node->_board->_evaluated) {
	//		_evaluation = transposition_table._hash_table[_zobrist_key]._node->_deep_evaluation._value;
	//		cout << "Evaluation: " << _evaluation << endl;
	//		_evaluated = true;
	//		return true;
	//	}
	//}

	//if (_evaluated) {
	//	cout << "already evaluated: " << to_fen() << ", eval: " << _evaluation << endl;
	//}

	if (check_game_over) {
		//is_game_over();

		// Draw
		if (_game_over_value == draw) {
			eval->_value = 0;
			eval->_evaluated = true;

			if (display)
				main_GUI._eval_components = "DRAW\n";

			eval->_uncertainty = 0;
			eval->_winnable_black = 0;
			eval->_winnable_white = 0;
			eval->get_WDL();
			eval->get_average_score();

			return;
		}

		// Mat
		if (_game_over_value != unterminated) {
			eval->_value = (-mate_value + _moves_count * mate_ply) * get_color();
			eval->_evaluated = true;

			if (display)
				main_GUI._eval_components = "CHECKMATE\n";

			eval->_uncertainty = 0;
			eval->_winnable_white = (_game_over_value == white_win) ? 1 : 0;
			eval->_winnable_black = (_game_over_value == black_win) ? 1 : 0;
			eval->get_WDL();
			eval->get_average_score();

			return;
		}
	}

	// If a neural network is available
	if (n != nullptr) {
		n->input_from_fen(to_fen());
		n->calculate_output();
		//_evaluation = n->_output;
		eval->_value = n->output_eval(mate_value);

		eval->_uncertainty = 0;
		eval->_winnable_black = 1;
		eval->_winnable_white = 1;
		eval->get_WDL();
		eval->get_average_score();

		// The evaluation has been performed
		eval->_evaluated = true;

		// Game not over
		return;
	}

	_displayed_components = display;
	if (display)
		main_GUI._eval_components = "";

	// Reset the evaluation
	eval->_value = 0;

	// Game progress
	game_advancement();
	if (display)
		main_GUI._eval_components += "ADVANCEMENT: " + to_string(static_cast<int>(round(100 * _adv))) + "%\n";

	// Nature of the position (open/closed)
	const float position_nature = get_position_nature();
	if (display)
		main_GUI._eval_components += "CLOSED: " + to_string(static_cast<int>(position_nature * 100.0f)) + "%\n";

	// *** MATERIEL ***

	if (display)
		main_GUI._eval_components += "\nMATERIAL\n";

	int total_material = 0;

	// Material
	if (evaluator->_piece_value != 0.0f) {
		const int material = count_material(evaluator, position_nature) * evaluator->_piece_value;
		if (display)
			main_GUI._eval_components += "material: " + (material >= 0 ? string("+") : string()) + to_string(material) + "\n";
		total_material += material;
	}	

	// Paire de oufs
	if (evaluator->_bishop_pair != 0.0f) {
		const int bishop_pair = count_bishop_pairs() * evaluator->_bishop_pair * (1 - position_nature);
		if (display)
			main_GUI._eval_components += "bishop pair: " + (bishop_pair >= 0 ? string("+") : string()) + to_string(bishop_pair) + "\n";
		total_material += bishop_pair;
	}

	// Doubled pieces
	if (evaluator->_doubled_pieces != 0.0f) {
		const int doubled_pieces = count_doubled_pieces(evaluator) * evaluator->_doubled_pieces;
		if (display)
			main_GUI._eval_components += "doubled pieces: " + (doubled_pieces >= 0 ? string("+") : string()) + to_string(doubled_pieces) + "\n";
		total_material += doubled_pieces;
	}

	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_material >= 0 ? string("+") : string()) + to_string(total_material) + " ---\n";

	eval->_value += total_material;

	// *** POSITIONNEMENT ***

	if (display)
		main_GUI._eval_components += "\nPOSITIONING\n";

	int total_positioning = 0;

	// Piece placement
	if (evaluator->_piece_positioning != 0.0f) {
		const int positioning = pieces_positioning(evaluator) * evaluator->_piece_positioning;
		if (display)
			main_GUI._eval_components += "piece positioning: " + (positioning >= 0 ? string("+") : string()) + to_string(positioning) + "\n";
		total_positioning += positioning;
	}

	// Rooks on open and semi-open files
	if (evaluator->_open_files != 0.0f) {
		const int rook_open = get_sliders_on_open_file() * evaluator->_open_files;
		if (display)
			main_GUI._eval_components += "sliders on open/semi files: " + (rook_open >= 0 ? string("+") : string()) + to_string(rook_open) + "\n";
		total_positioning += rook_open;
	}

	// Fianchettoed bishops
	if (evaluator->_fianchetto != 0.0f) {
		const int fianchetto = get_fianchetto_value() * evaluator->_fianchetto * (1.0f - position_nature);
		if (display)
			main_GUI._eval_components += "fianchetto bishops: " + (fianchetto >= 0 ? string("+") : string()) + to_string(fianchetto) + "\n";
		total_positioning += fianchetto;
	}

	// Piece alignments (bishop-rook / queen-king)
	if (evaluator->_alignments != 0.0f)
	{
		const int pieces_alignment = get_alignments() * evaluator->_alignments;
		if (display)
			main_GUI._eval_components += "pieces alignment: " + (pieces_alignment >= 0 ? string("+") : string()) + to_string(pieces_alignment) + "\n";
		total_positioning += pieces_alignment;
	}

	// Trapped pieces
	if (evaluator->_trapped_pieces != 0.0f) {
		const int trapped_pieces = get_trapped_pieces() * evaluator->_trapped_pieces;
		if (display)
			main_GUI._eval_components += "trapped pieces: " + (trapped_pieces >= 0 ? string("+") : string()) + to_string(trapped_pieces) + "\n";
		total_positioning += trapped_pieces;
	}

	// Pawn push threatening an enemy piece
	if (evaluator->_pawn_push_threats != 0.0f) {
		const int pawn_push_threat = get_pawn_push_threats() * evaluator->_pawn_push_threats;
		if (display)
			main_GUI._eval_components += "pawn push threats: " + (pawn_push_threat >= 0 ? string("+") : string()) + to_string(pawn_push_threat) + "\n";
		total_positioning += pawn_push_threat;
	}

	// Queen safety
	if (evaluator->_queen_safety != 0.0f) {
		const int queen_safety = (get_queen_safety(true) - get_queen_safety(false)) * evaluator->_queen_safety;
		if (display)
			main_GUI._eval_components += "queen safety: " + (queen_safety >= 0 ? string("+") : string()) + to_string(queen_safety) + "\n";
		total_positioning += queen_safety;
	}

	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_positioning >= 0 ? string("+") : string()) + to_string(total_positioning) + " ---\n";

	eval->_value += total_positioning;


	// *** ACTIVITE ***

	if (display)
		main_GUI._eval_components += "\nACTIVITY\n";

	int total_activity = 0;

	// Piece mobility
	//if (eval->_piece_mobility) {
	//	const int piece_mobility = get_piece_mobility() * eval->_piece_mobility;
	//	if (display)
	//		main_GUI._eval_components += "piece mobility: " + (piece_mobility >= 0 ? string("+") : string()) + to_string(piece_mobility) + "\n";
	//	total_activity += piece_mobility;
	//}

	// Long-term piece mobility
	if (evaluator->_long_term_piece_mobility != 0.0f) {
		const int long_term_mobility = get_long_term_piece_mobility() * evaluator->_long_term_piece_mobility;
		if (display)
			main_GUI._eval_components += "long-term piece mobility: " + (long_term_mobility >= 0 ? string("+") : string()) + to_string(long_term_mobility) + "\n";
		total_activity += long_term_mobility;
	}

	// Short-term piece mobility
	if (evaluator->_short_term_piece_mobility != 0.0f) {
		const int short_term_mobility = get_short_term_piece_mobility() * evaluator->_short_term_piece_mobility;
		if (display)
			main_GUI._eval_components += "short-term piece mobility: " + (short_term_mobility >= 0 ? string("+") : string()) + to_string(short_term_mobility) + "\n";
		total_activity += short_term_mobility;
	}

	// Piece activity
	if (evaluator->_piece_activity != 0.0f) {
		const int piece_activity = get_piece_activity() * evaluator->_piece_activity;
		if (display)
			main_GUI._eval_components += "piece activity: " + (piece_activity >= 0 ? string("+") : string()) + to_string(piece_activity) + "\n";
		total_activity += piece_activity;
	}

	// Knight activity
	if (evaluator->_knight_activity != 0.0f) {
		const int knight_activity = get_knight_activity() * evaluator->_knight_activity;
		if (display)
			main_GUI._eval_components += "knight activity: " + (knight_activity >= 0 ? string("+") : string()) + to_string(knight_activity) + "\n";
		total_activity += knight_activity;
	}

	// Bishop activity
	if (evaluator->_bishop_activity != 0.0f) {
		const int bishop_activity = get_bishop_activity() * evaluator->_bishop_activity;
		if (display)
			main_GUI._eval_components += "bishop activity: " + (bishop_activity >= 0 ? string("+") : string()) + to_string(bishop_activity) + "\n";
		total_activity += bishop_activity;
	}

	// Rook activity
	if (evaluator->_rook_activity != 0.0f) {
		const int rook_activity = get_rook_activity() * evaluator->_rook_activity;
		if (display)
			main_GUI._eval_components += "rook activity: " + (rook_activity >= 0 ? string("+") : string()) + to_string(rook_activity) + "\n";
		total_activity += rook_activity;
	}

	// Piece attacks and defences
	if (evaluator->_attacks != 0.0f) {
		const int pieces_attacks_and_defenses = get_attacks_and_defenses() * evaluator->_attacks;
		if (display)
			main_GUI._eval_components += "attacks/defenses: " + (pieces_attacks_and_defenses >= 0 ? string("+") : string()) + to_string(pieces_attacks_and_defenses) + "\n";
		total_activity += pieces_attacks_and_defenses;
	}

	// Side to move
	if (evaluator->_player_trait != 0.0f) {
		const int player_trait = evaluator->_player_trait * get_color() * (1 - position_nature);
		//const int player_trait = eval->_player_trait * get_color() * (1 - position_nature) * (1.0f + 1.0f * _adv);
		if (display)
			main_GUI._eval_components += "player trait: " + (player_trait >= 0 ? string("+") : string()) + to_string(player_trait) + "\n";
		total_activity += player_trait;
	}

	//if (display)
	//	main_GUI._eval_components += "SUB-TOTAL: " + (total_activity >= 0 ? string("+") : string()) + to_string(total_activity) + "\n";

	//// Adjustment based on the nature of the position
	//if (display)
	//	main_GUI._eval_components += "position nature: x" + to_string((int)(100 - 100 * position_nature)) + "%\n";
	//total_activity *= 1 - position_nature;


	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_activity >= 0 ? string("+") : string()) + to_string(total_activity) + " ---\n";

	eval->_value += total_activity;

	// *** TACTIQUE ***

	if (display)
		main_GUI._eval_components += "\nTACTICS\n";

	int total_tactics = 0;

	// Fourchettes
	if (display)
		main_GUI._eval_components += "forks: TODO\n";
	//if (eval->_forks != 0.0f) {
	//	const int forks = get_forks() * eval->_forks;
	//	if (display)
	//		main_GUI._eval_components += "forks: " + (forks >= 0 ? string("+") : string()) + to_string(forks) + "\n";
	//	total_tactics += forks;
	//}

	// Pins (TODO: move them here)

	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_tactics >= 0 ? string("+") : string()) + to_string(total_tactics) + " ---\n";

	// *** PAWN STRUCTURE ***

	if (display)
		main_GUI._eval_components += "\nPAWN STRUCTURE\n";

	int total_pawn_structure = 0;

	// Square control
	if (evaluator->_square_controls != 0.0f) {
		const int square_controls = get_square_controls() * evaluator->_square_controls;
		if (display)
			main_GUI._eval_components += "square controls: " + (square_controls >= 0 ? string("+") : string()) + to_string(square_controls) + "\n";
		total_pawn_structure += square_controls;
	}

	// Avantage d'espace
	if (evaluator->_space_advantage != 0.0f)
	{
		const int space = get_space() * evaluator->_space_advantage * position_nature;
		if (display)
			main_GUI._eval_components += "space: " + (space >= 0 ? string("+") : string()) + to_string(space) + "\n";
		total_pawn_structure += space;
	}

	// Pawn structure
	if (evaluator->_pawn_structure != 0.0f) {
		const int pawn_structure = get_pawn_structure(display * evaluator->_pawn_structure) * evaluator->_pawn_structure;
		//if (display)
		//	main_GUI._eval_components += "pawn structure: " + (pawn_structure >= 0 ? string("+") : string()) + to_string(pawn_structure) + "\n";
		total_pawn_structure += pawn_structure;
	}

	// Good and bad bishops
	if (evaluator->_bishop_pawns != 0.0f) {
		const int bishop_pawns = get_bishop_pawns() * evaluator->_bishop_pawns;
		if (display)
			main_GUI._eval_components += "bishop pawns: " + (bishop_pawns >= 0 ? string("+") : string()) + to_string(bishop_pawns) + "\n";
		total_pawn_structure += bishop_pawns;
	}

	// Weak squares and outposts
	if (evaluator->_weak_squares != 0.0f) {
		const int weak_squares = (-get_weak_squares(true) + get_weak_squares(false)) * evaluator->_weak_squares * (1.0f + position_nature);
		if (display)
			main_GUI._eval_components += "weak squares: " + (weak_squares >= 0 ? string("+") : string()) + to_string(weak_squares) + "\n";
		total_pawn_structure += weak_squares;
	}

	// Evaluation with all of its components
	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_pawn_structure >= 0 ? string("+") : string()) + to_string(total_pawn_structure) + " ---\n";

	eval->_value += total_pawn_structure;


	// *** KING ***

	if (display)
		main_GUI._eval_components += "\nKING\n";

	int total_king = 0;

	// King safety
	if (evaluator->_king_safety != 0.0f) {
		const int king_safety = get_king_safety(total_activity, display * evaluator->_king_safety) * evaluator->_king_safety;
		if (display)
			main_GUI._eval_components += "king safety: " + (king_safety >= 0 ? string("+") : string()) + to_string(king_safety) + "\n";
		total_king += king_safety;
	}

	// Droits de roques
	if (evaluator->_castling_rights != 0.0f) {
		const int castling_rights = evaluator->_castling_rights * (_castling_rights.k_w + _castling_rights.q_w - _castling_rights.k_b - _castling_rights.q_b) * (1 - _adv);
		if (display)
			main_GUI._eval_components += "castling rights: " + (castling_rights >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(castling_rights))) + "\n";
		total_king += castling_rights;
	}

	// Distance to castling
	//if (eval->_castling_distance != 0.0f) {
	//	const int castling_distance = get_castling_distance() * eval->_castling_distance;
	//	if (display)
	//		main_GUI._eval_components += "castling distance: " + (castling_distance >= 0 ? string("+") : string()) + to_string(castling_distance) + "\n";
	//	total_king += castling_distance;
	//}

	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_king >= 0 ? string("+") : string()) + to_string(total_king) + " ---\n";

	eval->_value += total_king;


	// *** FINALES ***

	if (display)
		main_GUI._eval_components += "\nENDGAME\n";

	int total_endgame = 0;

	// King opposition
	if (evaluator->_kings_opposition != 0.0f) {
		const int kings_opposition = get_kings_opposition() * evaluator->_kings_opposition;
		if (display)
			main_GUI._eval_components += "king opposition: " + (kings_opposition >= 0 ? string("+") : string()) + to_string(kings_opposition) + "\n";
		total_endgame += kings_opposition;
	}

	// King proximity to the pawns in the endgame
	if (evaluator->_king_proximity != 0.0f) {
		const int king_proximity = get_king_proximity() * evaluator->_king_proximity;
		if (display)
			main_GUI._eval_components += "king proximity: " + (king_proximity >= 0 ? string("+") : string()) + to_string(king_proximity) + "\n";
		total_endgame += king_proximity;
	}

	// King centralisation
	if (evaluator->_king_centralization != 0.0f) {
		const int king_centralization = (get_king_centralization(true) - get_king_centralization(false)) * evaluator->_king_centralization;
		if (display)
			main_GUI._eval_components += "king centralization: " + (king_centralization >= 0 ? string("+") : string()) + to_string(king_centralization) + "\n";
		total_endgame += king_centralization;
	}

	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_endgame >= 0 ? string("+") : string()) + to_string(total_endgame) + " ---\n";

	eval->_value += total_endgame;

	// *** NATURE OF THE POSITION ***

	if (display)
		main_GUI._eval_components += "\nPOSITION NATURE\n";

	int total_nature = 0;

	// Forteresse
	if (evaluator->_push != 0.0f) {
		const float push = 1 - static_cast<float>(_half_moves_count) * evaluator->_push / max_half_moves;
		const int fortress = 100.0f - push * 100.0f;
		const int fortress_value = eval->_value * (push - 1);
		if (display)
			main_GUI._eval_components += "fortress: " + to_string(fortress) + "% (" + (fortress_value >= 0 ? string("+") : string()) + to_string(fortress_value) + ")\n";
		total_nature += fortress_value;
	}

	// Evaluation uncertainty
	get_uncertainty(eval, total_material);
	const int uncertainty_percent = (int)(100 * eval->_uncertainty);
	if (display)
		main_GUI._eval_components += "uncertainty: " + to_string(uncertainty_percent) + "%\n";

	// Is the position winnable?
	get_winnable_values(eval, position_nature);

	if (display)
		main_GUI._eval_components += "winnable: " + to_string(static_cast<int>(eval->_winnable_white * 100)) + "% / " + to_string(static_cast<int>(eval->_winnable_black * 100)) + "%\n";

	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_nature >= 0 ? string("+") : string()) + to_string(total_nature) + " ---\n";

	eval->_value += total_nature;


	// *** TOTAL ***
	if (display) {
		main_GUI._eval_components += "\nTOTAL COMPONENTS\n";
		main_GUI._eval_components += "Material: " + (total_material >= 0 ? string("+") : string()) + to_string(total_material) + "\n";
		main_GUI._eval_components += "Positioning: " + (total_positioning >= 0 ? string("+") : string()) + to_string(total_positioning) + "\n";
		main_GUI._eval_components += "Activity: " + (total_activity >= 0 ? string("+") : string()) + to_string(total_activity) + "\n";
		main_GUI._eval_components += "Tactics: " + (total_tactics >= 0 ? string("+") : string()) + to_string(total_tactics) + "\n";
		main_GUI._eval_components += "Pawn structure: " + (total_pawn_structure >= 0 ? string("+") : string()) + to_string(total_pawn_structure) + "\n";
		main_GUI._eval_components += "King: " + (total_king >= 0 ? string("+") : string()) + to_string(total_king) + "\n";
		main_GUI._eval_components += "Endgame: " + (total_endgame >= 0 ? string("+") : string()) + to_string(total_endgame) + "\n";
		main_GUI._eval_components += "Nature: " + (total_nature >= 0 ? string("+") : string()) + to_string(total_nature) + "\n";

		main_GUI._eval_components += "_______________\nTOTAL: " + (eval->_value >= 0 ? string("+") : string()) + to_string(eval->_value) + "\n";

	}

	// Chances de gain
	//const float win_chance = get_winning_chances_from_eval(_evaluation, true);
	//if (display)
	//	main_GUI._eval_components += "W/D/L: " + to_string(static_cast<int>(100 * win_chance)) + "/" + to_string(static_cast<int>(100 * 0)) + "/" + to_string(static_cast<int>(100 * (1.0f - win_chance))) + "%\n";

	eval->get_WDL();
	eval->get_average_score();

	if (display) {
		main_GUI._eval_components += "Confidence: " + to_string(100 - uncertainty_percent) + "%\n";
		main_GUI._eval_components += eval->_wdl.to_string() + "\n";
		main_GUI._eval_components += "Score: " + score_string(eval->_avg_score) + "\n";
	}

	// The evaluation has been performed
	eval->_evaluated = true;

	// EXPERIMENTAL
	//Node node(this);
	//transposition_table._hash_table[_zobrist_key] = &node;


	// Game not over
	return;
}

// Loads the board from a FEN
// TODO: rewrite this
void Board::from_fen(string fen)
{
	string pgn;
	//reset_all();
	reset_board();

	// PGN
	main_GUI._initial_fen = fen;
	main_GUI._pgn = "";

	// Iterator walking the character string
	int iterator = 0;

	// Board position being iterated
	int row = 7;
	int col = 0;

	char c;

	// Piece placement
	while (row >= 0) {
		c = fen[iterator];
		switch (c) {
		case '/': case ' ': row -= 1; col = 0; break;
		case 'P': _array[row][col] = w_pawn; col++; break;
		case 'N': _array[row][col] = w_knight; col++; break;
		case 'B': _array[row][col] = w_bishop; col++; break;
		case 'R': _array[row][col] = w_rook; col++; break;
		case 'Q': _array[row][col] = w_queen; col++; break;
		case 'K': _array[row][col] = w_king; col++; break;
		case 'p': _array[row][col] = b_pawn; col++; break;
		case 'n': _array[row][col] = b_knight; col++; break;
		case 'b': _array[row][col] = b_bishop; col++; break;
		case 'r': _array[row][col] = b_rook; col++; break;
		case 'q': _array[row][col] = b_queen; col++; break;
		case 'k': _array[row][col] = b_king; col++; break;
		default:
			if (isdigit(c)) {
				const int digit = (static_cast<int>(c)) - (static_cast<int>('0'));
				for (int k = col; k < col + digit; k++) {
					_array[row][k] = 0;
				}
				col += digit;
				break;
			}

			else {
				cout << "invalid FEN" << endl;
				return;
			}
		}

		iterator++;
	}

	// Side to move
	c = fen[iterator];

	_player = c == 'w';

	iterator += 2;

	bool next = true;

	// Roques
	_castling_rights.k_w = false; _castling_rights.q_w = false; _castling_rights.k_b = false; _castling_rights.q_b = false;

	while (next) {
		c = fen[iterator];

		switch (c) {
		case '-': iterator += 1; next = false; break;
		case 'K': _castling_rights.k_w = true; break;
		case 'Q': _castling_rights.q_w = true; break;
		case 'k': _castling_rights.k_b = true; break;
		case 'q': _castling_rights.q_b = true; iterator += 1; next = false; break;
		default: next = false; break;
		}

		iterator++;
	}

	c = fen[iterator];

	// En passant
	if (c == '-')
		_en_passant_col = -1;
	else {
		_en_passant_col = fen[iterator] - 'a';
		iterator++;
	}

	iterator += 2;
	string s;
	while (fen[iterator] != ' ') {
		s += fen[iterator];
		iterator++;
	}
	_half_moves_count = stoi(s);

	iterator++;
	fen += ' ';
	s = "";
	while (fen[iterator] != ' ') {
		s += fen[iterator];
		iterator++;
	}
	_moves_count = stoi(s);

	_got_moves = -1;

	reset_eval();
	reset_positions_history();
	get_zobrist_key();
	_positions_history[_zobrist_key] = 1;

	update_kings_pos();

	update_bitboards();

	// Orient the board for the side to move
	main_GUI._board_orientation = _player;

	// Update the position FEN in the GUI
	main_GUI._initial_fen = fen;

	main_GUI._game_tree.new_tree(*this);
}

// Returns the FEN of the board
string Board::to_fen() const
{
	string s;
	int it = 0;

	for (int i = 7; i >= 0; i--) {
		for (int j = 0; j < 8; j++) {
			if (const uint8_t p = _array[i][j]; p == none)
				it++;
			else {
				constexpr auto piece_letters = "PNBRQKpnbrqk";

				if (it > 0) {
					s += static_cast<char>(it + 48);
					it = 0;
				}

				s += piece_letters[p - 1];
			}
		}

		if (it > 0) {
			s += static_cast<char>(it + 48);
			it = 0;
		}

		if (i > 0)
			s += "/";
	}

	if (_player)
		s += " w ";
	else
		s += " b ";

	if (_castling_rights.k_w)
		s += "K";
	if (_castling_rights.q_w)
		s += "Q";
	if (_castling_rights.k_b)
		s += "k";
	if (_castling_rights.q_b)
		s += "q";
	if (!(_castling_rights.k_w || _castling_rights.q_w || _castling_rights.k_b || _castling_rights.q_b))
		s += "-";

	string en_passant = "-";
	if (_en_passant_col != -1)
		en_passant = main_GUI._abc8[_en_passant_col] + static_cast<string>(_player ? "6" : "3");

	s += " " + en_passant + " " + to_string(_half_moves_count) + " " + to_string(_moves_count);

	return s;
}

// Returns the winner when the game is over
// Also generates the legal moves, when there are any
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
string Board::move_label(Move move, bool use_uft8)
{
	assign_move_flags(&move);

	const uint8_t start_row = move.start_row;
	const uint8_t start_col = move.start_col;
	const uint8_t end_row = move.end_row;
	const uint8_t end_col = move.end_col;

	const uint8_t p1 = _array[start_row][start_col]; // Piece being moved
	const uint8_t p2 = _array[end_row][end_col];

	// To tell whether another similar piece can reach the same square
	bool spec_col = false;
	bool spec_row = false;

	if (_got_moves == -1) {
		get_moves();
	}

	uint8_t new_start_row; uint8_t new_start_col; uint8_t new_end_row; uint8_t new_end_col; uint8_t new_p;
	for (int m = 0; m < _got_moves; m++) {
		new_start_row = _moves[m].start_row;
		new_start_col = _moves[m].start_col;
		new_end_row = _moves[m].end_row;
		new_end_col = _moves[m].end_col;
		new_p = _array[new_start_row][new_start_col];

		// A different piece of the same type that can reach the same square
		if ((new_start_row != start_row || new_start_col != start_col) && new_p == p1 && new_end_row == end_row && new_end_col == end_col) {

			// Same file, so the rank has to be spelled out
			if (new_start_col == start_col)
				spec_row = true;
			else {
				spec_col = true;
			}
		}
	}

	string s;

	bool is_castle = false;

	switch (p1)	{
	case w_knight: case b_knight: s += use_uft8 ? (_player ? main_GUI.N_symbol : main_GUI.n_symbol) : "N"; if (spec_col) s += main_GUI._abc8[start_col]; if (spec_row) s += static_cast<char>(start_row + 1 + 48); break;
	case w_bishop: case b_bishop: s += use_uft8 ? (_player ? main_GUI.B_symbol : main_GUI.b_symbol) : "B"; if (spec_col) s += main_GUI._abc8[start_col]; if (spec_row) s += static_cast<char>(start_row + 1 + 48); break;
	case w_rook: case b_rook: s += use_uft8 ? (_player ? main_GUI.R_symbol : main_GUI.r_symbol) : "R"; if (spec_col) s += main_GUI._abc8[start_col]; if (spec_row) s += static_cast<char>(start_row + 1 + 48); break;
	case w_queen: case b_queen: s += use_uft8 ? (_player ? main_GUI.Q_symbol : main_GUI.q_symbol) : "Q"; if (spec_col) s += main_GUI._abc8[start_col]; if (spec_row) s += static_cast<char>(start_row + 1 + 48); break;
	case w_king: case b_king:
		if (end_col - start_col == 2) {
			s += "O-O"; is_castle = true; break;
		}
		if (start_col - end_col == 2) {
			s += "O-O-O"; is_castle = true; break;
		}
		s += use_uft8 ? (_player ? main_GUI.K_symbol : main_GUI.k_symbol) : "K"; break;
	}

	// Capture (or en passant)
	if (move.is_capture()) {
		if (is_pawn(p1))
			s += main_GUI._abc8[start_col];
		s += "x";
		s += main_GUI._abc8[end_col];
		s += static_cast<char>(end_row + 1 + 48);
	}

	else if (!is_castle) {
		s += main_GUI._abc8[end_col];
		s += static_cast<char>(end_row + 1 + 48);
	}

	// Promotion (queen only for now)
	if ((p1 == w_pawn && end_row == 7) || (p1 == b_pawn && end_row == 0)) {
		s += "=";
		s += use_uft8 ? (_player ? main_GUI.Q_symbol : main_GUI.q_symbol) : "Q";
	}

	if (move.is_checkmate())
		return _player ? s + "# 1-0" : s + "# 0-1";

	if (move.is_check())
		return s + "+";

	Board temp_board = *this;
	temp_board.make_move(move, false);

	if (temp_board.is_game_over() == draw) {
		return s + " 1/2-1/2";
	}

	return s;
}

// Draws a text inside a given area
void Board::draw_text_rect(const string& s, const float pos_x, const float pos_y, const float width, const float height, const float size) {

	// Text splitting
	const int sub_div = (1.5f * width) / size;

	if (width <= 0 || height <= 0 || sub_div <= 0)
		return;

	const Rectangle rect_text = { pos_x, pos_y, width, height };
	DrawRectangleRec(rect_text, main_GUI._background_text_color);

	const size_t string_size = s.length();
	int i = 0;
	while (sub_div * i <= string_size) {
		const char* c = s.substr(i * sub_div, sub_div).c_str();
		DrawTextEx(main_GUI._text_font, c, { pos_x, pos_y + i * size }, size, main_GUI._font_spacing * size, main_GUI._text_color);
		i++;
	}
}

// Plays the sound of a move
void Board::play_move_sound(Move move) {
	assign_move_flags(&move);

	//cout << "Flags: " << move.is_capture << " " << move.is_check << " " << move.is_promotion << endl;

	const uint8_t i = move.start_row;
	const uint8_t j = move.start_col;
	const uint8_t k = move.end_row;
	const uint8_t l = move.end_col;

	// Pieces
	const uint8_t p1 = _array[i][j];
	const uint8_t p2 = _array[k][l];


	// End-of-game sounds

	// Mat
	if (move.is_checkmate())
		PlaySound(main_GUI._checkmate_sound);

	// Check
	else if (move.is_check()) {
		PlaySound(main_GUI._check_sound);
	}

	else {
		Board temp_board = *this;
		temp_board.make_move(move, false);

		if (temp_board.is_game_over() == draw) {
			PlaySound(main_GUI._stalemate_sound);
		}
	}

	// Promotion
	if (move.is_promotion())
		PlaySound(main_GUI._promotion_sound);

	// Capture
	if (move.is_capture()) {
		PlaySound(main_GUI._capture_sound);
	}

	// Roques
	if (p1 == w_king && abs(j - l) == 2 || (p1 == b_king && abs(j - l) == 2))
		PlaySound(main_GUI._castle_sound);

	// "Normal" move
	if (!move.is_check()) {
		PlaySound(main_GUI._move_sound);
	}

	return;
}

// Resets the board to its base state, for the buffer
// FIXME: would allocating a fresh board be faster, and safer memory-wise?
void Board::reset_board(const bool display) {
	_got_moves = -1;
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
int Board::get_king_safety(int activity_diff, float display_factor) {

	// ----------------------
	// *** POSITIONS TEST ***
	// ----------------------

	// TODO: factor mobility into king safety
	// Rework the whole potential as a non-linear function, with 0 = cannot mate and 1 = can mate?

	// r1bq1b1r/pp4pp/2p1k3/3np3/1nB5/2N2Q2/PPPP1PPP/R1B2RK1 w - - 0 10 vs r1b2bnr/pppp1k1p/2n2q2/8/5B2/2N2Q2/PPP3PP/R4RK1 b - - 2 12
	// 4rb1r/pp3kpp/2p1b3/3nB3/2BP4/P7/1PP2PPP/4RRK1 w - - 0 18
	// 4r3/p3bkp1/r7/1pPpBP1p/1P1P4/P2b2P1/5R1P/4R1K1 w - - 1 28: the king should be safe
	// r1bq1b1r/ppp3pp/2n1k3/3np3/2B5/5Q2/PPPP1PPP/RNB1K2R w KQ - 2 8
	// r1b2b1r/ppp3pp/8/3kp3/8/8/PPPP1PPP/R1B1K2R w KQ - 0 12
	// 8/2p1k1pp/p1Qb4/3P3q/4p3/N1P1BnPb/P4P2/5R1K w - - 1 25
	// 5rk1/6p1/pq1b3p/3p4/2p1n3/PP3N1P/4p1P1/RQR4K w - - 2 31: white king very weak (mated)
	// 3r1rk1/pp1bbp2/1qp1pn1Q/4N3/3P4/2PB4/PP3PPP/R3R1K1 b - - 0 16
	// 2k3r1/p1b4p/2p5/3P3r/8/5bP1/PP3P2/2R2RK1 w - - 0 7
	// r1bq1rk1/ppppnpp1/8/2bNp1PQ/1nB1P3/2P5/PP1P1PP1/R1B1K2R b KQ - 2 3
	// r1bq1rk1/pp2npp1/2n1p3/2ppP1NQ/3P4/P1P5/2P2PPP/R1B1K2R b KQ - 3 3
	// r3kb1r/pR2pppp/2p5/3p4/3P2b1/B3RN2/q1P2PPP/3Q2K1 b kq - 1 14 : overload++
	// r4k1r/pRQ3pp/2p1pp2/3p1b2/3P4/R4N1P/2q2PP1/6K1 b - - 2 20 : mat imparable
	// r1b1k2r/p1p2ppp/2p5/8/5P1q/3B1R1P/PBP3P1/Q5K1 w kq - 3 17: the black king is the weaker one
	// 1r4k1/p2n1pp1/2p1b2p/3p3P/4pQ2/2q1P3/P1P1BPP1/2KR3R w - - 1 23: Black is mating
	// rnbr2k1/ppq2p2/2pb1npQ/6N1/7R/3B2P1/PPP2P1P/2KR4 b - - 2 17: White is mating
	// 3rk2r/ppp2ppq/2p1b3/2P5/4P1P1/2P3P1/PPQ1B3/RNB2RK1 w k - 1 7: nearly equal
	// 3rk2r/ppp2pp1/2p5/2P5/4P3/2P3P1/PPQN1KR1/R1B4q b k - 2 12: Rh2 then perpetual
	// 2k2r2/ppp3pp/1bp1b3/8/4Pp1q/1N1B1Pn1/PP3RPP/R2QB1K1 w - - 8 6: white king not very safe
	// 2k2r2/ppp3pp/1bp1b3/8/4Pp1q/1N1B1Pn1/PPQ2RPP/R3B1K1 b - - 9 6 : Dxh2+!! #5
	// 2k5/ppp3pp/1bp1b2r/8/4Pp2/1N1B1Pn1/PPQ2RP1/R3B1K1 w - - 3 9 : #1 imparable
	// 8/p7/r3pk2/8/1P2Kp2/P1R2P2/5P2/8 b - - 3 39: white king not in danger
	// 2rk3q/1pp5/p4n2/1P1p1bp1/2PQ1b2/N2p4/P2P2PP/R1B1R2K w - - 0 23: white king is lost
	// r1b1k2r/pppp2pp/2n5/4Pp2/8/BB3N2/P1PQ2PP/5K2 b kq - 0 15: the king is actually in trouble
	// r1bq1b1r/pp4pp/2p1k3/3np3/1nBP4/2N2Q2/PPP2PPP/R1B2RK1 b - - 0 10: +2.5 / +5 for king safety
	// r3r1k1/2p2pp1/1p1p3p/pPn4q/2PN3n/P3PP1P/2Q2P1K/B2R2R1 w - - 7 6: already completely winning for White
	// r1bq1rk1/pp1nbpn1/2p1p3/8/2pP4/2N1PN2/PPQ2P1P/2KR1BR1 b - - 1 6: winning for White -> black king too weak, open files and diagonals, no pawns in front either; all 6 white pieces can attack while only 4 black pieces can defend
	// 1r1qr1k1/p2n1pn1/b1p1pb1Q/4N3/1ppPN3/4P3/PP3P1P/2KR1BR1 b - - 9 12: lost for Black
	// r1b3kr/pppp3p/2n2Q2/8/5N2/4p3/PPP3PP/6K1 b - - 2 19: winning for White
	// r1b3kr/ppp4p/2np1Q2/7N/8/4p3/PPP3PP/6K1 b - - 1 20 : #1 imparable
	// rnb2bnr/pppp1k1p/8/8/5p2/4BQ2/PqP3PP/RN3RK1 w - - 0 11: winning for White
	// 6k1/5pp1/5r2/7K/P5PP/2Nr1n2/1P6/8 b - - 0 38 : #1 imparable...
	// r3k2r/pp1n1pp1/2n1b2p/2p1P3/5P2/P4NP1/1PPKBB1P/3R3R w kq - 0 18: here the king is better on c1 than e3
	// r1b3kr/pppp3p/2n2Q2/3N4/8/4p3/PPP3PP/6K1 w - - 1 19: winning for White
	// r1b1r2k/pp3pp1/2n4n/3qp3/2Np4/3B1N1P/PP3PP1/RQ2R1K1 w - - 0 17: black king not that endangered; the bishop/queen battery achieves nothing, the queen is only half attacking
	// 8/1rp3p1/4k2p/8/7P/2R2KP1/5P2/8 w - - 6 58: king is fine
	// 6R1/5p2/5kp1/2q5/pp4B1/2n1R3/5PKP/8 b - - 5 45: black king is fine
	// 1r6/7p/p1P1p3/4kp2/1P1Rp3/4KPP1/8/8 b - - 0 49 : pareil...
	// 2bk1r2/4b1Qp/8/1p6/P2P4/1qp5/4NPPP/R1K2B1R w - - 1 25: winning for Black

	// 8/6PK/5k2/8/8/8/8/8 b - - 0 8


	// Update the king positions
	update_kings_pos();

	// Number of files between the kings, to detect opposite-side castling for instance
	const int king_columns_diff = abs(_white_king_pos.col - _black_king_pos.col);


	// King weaknesses
	int w_king_weakness = 0;
	int b_king_weakness = 0;

	// ---------------------------
	// *** POTENTIEL D'ATTAQUE ***
	// ---------------------------

	// rnb2bnr/pppp1k1p/5q2/8/5B2/5Q2/PPP3PP/RN3RK1 b - - 0 11
	// 8/8/8/2r2pp1/1k5p/2b4P/4K3/1Q6 b - - 81 133: the queen has more potential than rook and bishop combined

	// Attacking potential of each piece (pawn, knight, bishop, rook, queen)
	static constexpr int attack_potentials[6] = { 5, 30, 35, 55, 125, 0 };
	constexpr int reference_attack_potential = 405; // If every starting piece is still on the board

	// Defensive potential
	static constexpr int defense_potentials[6] = { 5, 25, 20, 15, 10, 0 };
	constexpr int reference_defense_potential = 170; // If every starting piece is still on the board

	// Attacking potential required to mate comfortably
	// r1b3nr/ppppk2p/2n5/8/5N2/1Q6/PPP3PP/R6K w - - 1 18
	constexpr int needed_potential = 40;
	//constexpr int needed_potential = 0;

	// Attacking potential values
	int w_total_attack_potential = 0;
	int b_total_attack_potential = 0;

	// Defensive potential values
	int w_total_defense_potential = 0;
	int b_total_defense_potential = 0;

	// Bishops of each side
	bool w_bishop_w = false;
	bool w_bishop_b = false;
	bool b_bishop_w = false;
	bool b_bishop_b = false;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			if (const uint8_t p = _array[row][col]; p > 0) {
				if (is_white(p)) {
					w_total_attack_potential += attack_potentials[p - 1];
					w_total_defense_potential += defense_potentials[p - 1];
				}
				else {
					b_total_attack_potential += attack_potentials[(p - 1) % 6];
					b_total_defense_potential += defense_potentials[(p - 1) % 6];
				}

				if (p == w_bishop) {
					if ((row + col) % 2 == 0)
						w_bishop_w = true;
					else
						w_bishop_b = true;
				}
				else if (p == b_bishop) {
					if ((row + col) % 2 == 0)
						b_bishop_w = true;
					else
						b_bishop_b = true;
				}
			}
		}
	}

	// With an opposite-coloured bishop, add attacking potential proportional to the current one
	//constexpr float opposite_bishop_potential = 1.25f;

	//cout << "bishops: " << w_bishop_w << " " << w_bishop_b << " " << b_bishop_w << " " << b_bishop_b << endl;

	if (((w_bishop_w && !w_bishop_b) && (!b_bishop_w && b_bishop_b)) || ((!w_bishop_w && w_bishop_b) && (b_bishop_w && !b_bishop_b))) {
		//cout << "opposite bishop" << endl;
		//w_total_attack_potential *= 1 + (opposite_bishop_potential - 1) * w_total_attack_potential / reference_attack_potential;
		//b_total_attack_potential *= 1 + (opposite_bishop_potential - 1) * b_total_attack_potential / reference_attack_potential;
		w_total_defense_potential -= defense_potentials[2] * 0.5f;
		b_total_defense_potential -= defense_potentials[2] * 0.5f;
	}

	//r1b3k1/pp3ppp/5q2/2Pr4/4p3/1NQ1K1N1/PP2B1PP/R7 b - - 1 24
	//8/8/2p4k/2Pp4/3P1n2/8/3K4/8 w - - 0 58

	//cout << "w_total_attack_potential: " << w_total_attack_potential << endl;
	//cout << "b_total_attack_potential: " << b_total_attack_potential << endl;

	//cout << "w_total_defense_potential: " << w_total_defense_potential << endl;
	//cout << "b_total_defense_potential: " << b_total_defense_potential << endl;

	// Normalised attacking potential
	const float w_attack_potential_normalized = (float)(w_total_attack_potential - needed_potential) / (reference_attack_potential - needed_potential);
	const float b_attack_potential_normalized = (float)(b_total_attack_potential - needed_potential) / (reference_attack_potential - needed_potential);

	//cout << "w_attack_potential_normalized: " << w_attack_potential_normalized << endl;
	//cout << "b_attack_potential_normalized: " << b_attack_potential_normalized << endl;

	// Normalised defensive potential
	const float w_defense_potential_normalized = (float)w_total_defense_potential / reference_defense_potential;
	const float b_defense_potential_normalized = (float)b_total_defense_potential / reference_defense_potential;

	//cout << "w_defense_potential_normalized: " << w_defense_potential_normalized << endl;
	//cout << "b_defense_potential_normalized: " << b_defense_potential_normalized << endl;

	// Potentiel minimum d'attaque
	//const float min_attack_potential = -needed_potential / (float)reference_attack_potential;

	// Potentiel final d'attaque
	float w_attacking_potential = 2.0f * max(0.0f, w_attack_potential_normalized) / (1.0f + w_defense_potential_normalized);
	float b_attacking_potential = 2.0f * max(0.0f, b_attack_potential_normalized) / (1.0f + b_defense_potential_normalized);

	//// Potentiel total
	//const int w_total_potential = max(0, w_total_attack_potential - b_total_defense_potential - needed_potential);
	//const int b_total_potential = max(0, b_total_attack_potential - w_total_defense_potential - needed_potential);

	//cout << "w_total_potential: " << w_total_potential << endl;
	//cout << "b_total_potential: " << b_total_potential << endl;

	//// Reference total potential
	//const int reference_potential = reference_attack_potential - reference_defense_potential - needed_potential;

	//// Normalised potential
	//float w_attacking_potential = (float)w_total_potential / reference_potential;
	//float b_attacking_potential = (float)b_total_potential / reference_potential;

	//cout << "w_attacking_potential: " << w_attacking_potential << endl;
	//cout << "b_attacking_potential: " << b_attacking_potential << endl;


	//1r1q1k2/2n5/p2p4/2pp4/6QN/8/1PP1N1PP/7K b - - 0 29
	// r3r1k1/5p2/2p2b1B/p2bpP1Q/8/1Pq4P/6PK/4RR2 w - - 2 29: capturing on b3 lowers White's potential???

	// 3rr1k1/2p2ppp/1bp2n2/pp6/4PB2/2PPN2q/PPQ1BP2/R4RK1 b - - 3 9: Black still has drawing potential here

	// Non-linear function
	constexpr double alpha = 2.0;
	w_attacking_potential = pow(w_attacking_potential, alpha);
	b_attacking_potential = pow(b_attacking_potential, alpha);

	//cout << "w_attacking_potential: " << w_attacking_potential << endl;
	//cout << "b_attacking_potential: " << b_attacking_potential << endl;

	// Constant keeping a floor of potential
	constexpr float min_potential = 0.0f;

	// 2k5/ppp3Bp/2p4r/8/b3Pp2/3B1Pn1/PP3KP1/RQ6 b - - 0 1

	// Always add a minimum potential, so any weakening is accounted for
	w_attacking_potential = max(w_attacking_potential, min_potential * pow((float)w_total_attack_potential / reference_attack_potential, 0.35f));
	b_attacking_potential = max(b_attacking_potential, min_potential * pow((float)b_total_attack_potential / reference_attack_potential, 0.35f));


	// Potentiel d'attaque
	//const float w_attacking_potential = ((float)w_total_attack_potential / reference_potential + min_potential) / (1 + min_potential);
	//const float b_attacking_potential = ((float)b_total_attack_potential / reference_potential + min_potential) / (1 + min_potential);

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "----------\n";
		main_GUI._eval_components += "Attacking potential: " + to_string(w_attacking_potential) + " / " + to_string(b_attacking_potential) + "\n";
	}

	// Facteurs multiplicatifs
	constexpr float piece_attack_factor = 1.0f;
	constexpr float piece_defense_factor = 1.0f;
	constexpr float pawn_protection_factor = 0.6f;

	// When the resultant is positive or negative
	constexpr float piece_overload_multiplicator = 1.0f; // TODO: put this to use
	constexpr float piece_defense_multiplicator = 1.0f;

	// --------------------------
	// *** ESPACE DE MOBILITE ***
	// --------------------------

	// TEST (unsure)
	constexpr float space_safety_factor = 0.35f;

	const int space = get_space();

	const int w_space = space_safety_factor * space;
	const int b_space = -space_safety_factor * space;

	// ---------------------------
	// *** PIECE ACTIVITY ***
	// ---------------------------

	// TEST (unsure)
	constexpr float activity_attacking_factor = 1.0f;
	constexpr float activity_protection_factor = 0.5f;

	const int activity = activity_diff > 0 ? pow(activity_diff / 100.0, 0.5) * 100 : -pow(-activity_diff / 100.0, 0.5) * 100;

	const int w_activity = activity > 0 ? activity * activity_protection_factor : activity * activity_attacking_factor;
	const int b_activity = activity < 0 ? -activity * activity_protection_factor : -activity * activity_attacking_factor;

	//rnbqkbnr/ppp2ppp/3p4/4p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w kq - 3 7: g8 and similar moves are wrong even with more activity

	// -------------------------------------
	// *** POWER COMPUTATION ***
	// * ATTAQUES - DEFENSES - PROTECTIONS *
	// -------------------------------------

	// King shielding
	int w_king_protection = get_pawn_shield_protection(true, b_attacking_potential, w_space) * pawn_protection_factor;
	int b_king_protection = get_pawn_shield_protection(false, w_attacking_potential, b_space) * pawn_protection_factor;

	// Attaquants
	int w_attacking_power = get_king_attackers(true);
	int b_attacking_power = get_king_attackers(false);

	// The more attack there is, the harder it is to defend even with many defenders -> exponential?
	// Threshold above which attacking power counts double
	constexpr int doubled_attack = 800;
	float w_mult_attack = 1.0f + w_attacking_power * w_attacking_potential / static_cast<float>(doubled_attack);
	float b_mult_attack = 1.0f + b_attacking_power * b_attacking_potential/ static_cast<float>(doubled_attack);

	w_attacking_power *= w_mult_attack;
	b_attacking_power *= b_mult_attack;

	w_attacking_power *= piece_attack_factor;
	b_attacking_power *= piece_attack_factor;

	// Defenders
	//int w_defending_power = get_king_defenders(true) * piece_defense_factor * (1.0f + 0.5f * (1.0f - b_attacking_potential));
	//int b_defending_power = get_king_defenders(false) * piece_defense_factor * (1.0f + 0.5f * (1.0f - w_attacking_potential));

	int w_defending_power = get_king_defenders(true) * piece_defense_factor * (1.0f + 0.35f * (1.0f - b_attacking_potential));
	int b_defending_power = get_king_defenders(false) * piece_defense_factor * (1.0f + 0.35f * (1.0f - w_attacking_potential));

	// Defence by the king alone
	//constexpr int king_defense = 200;

	//w_defending_power += king_defense;
	//b_defending_power += king_defense;

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "Attacking power: " + to_string(w_attacking_power) + " / " + to_string(b_attacking_power) + "\n";
		main_GUI._eval_components += "Defending power: " + to_string(w_defending_power) + " / " + to_string(b_defending_power) + "\n";
		//main_GUI._eval_components += "King protection: " + to_string(w_king_protection) + " / " + to_string(b_king_protection) + "\n";
	}


	// -----------------
	// *** OVERLOADS ***
	// -----------------

	// TODO: extract a function for this
	//rnq1k2r/pp2bp2/2p5/3p4/5Pb1/P2P1NPp/1PP4K/R1BQ1R1N b kq - 0 17: overload on our own h3 pawn??
	// r3k2r/ppqn3n/3b1p2/2ppp1p1/4P2p/P2P1P1P/1PPBBN1K/R1NQ1R2 b kq - 5 22 : overload +495???

	// Fetch the square control maps
	SquareMap white_controls_map = get_white_controls_map();
	SquareMap black_controls_map = get_black_controls_map();

	// Is this useful?
	//white_controls_map.print();
	//black_controls_map.print();

	// Net control
	SquareMap controls_map = white_controls_map - black_controls_map;

	//controls_map.print();

	// Overload danger: squares controlled in our favour near the enemy king
	constexpr uint8_t overloard_distance_dangers[8] = { 50, 35, 5, 1, 0, 0, 0, 0 };


	// Overload on the white king
	int w_king_overloaded = 0;

	// Controlled squares near the king
	for (uint8_t i = 0; i < 8; i++) {
		for (uint8_t j = 0; j < 8; j++) {
			// Piece on this square
			uint8_t p = _array[i][j];

			if (controls_map._array[i][j] < 0 && p <= w_king)
			{
				const uint8_t distance = max(abs(i - _white_king_pos.row), abs(j - _white_king_pos.col));
				w_king_overloaded -= overloard_distance_dangers[distance] * controls_map._array[i][j]; // minus, because the value is negative
				//cout << "square: " << square_name(i, j) << ", piece: " << piece_name(p) << " / distance: " << (int)distance << " / overload: " << overloard_distance_dangers[distance] * controls_map._array[i][j] << endl;
			}
		}
	}
	
	// Overload on the black king
	int b_king_overloaded = 0;

	// Attacks across the board
	for (uint8_t i = 0; i < 8; i++) {
		for (uint8_t j = 0; j < 8; j++) {
			// Piece on this square
			uint8_t p = _array[i][j];

			if (controls_map._array[i][j] > 0 && (p >= b_pawn || p == none))
			{
				const uint8_t distance = max(abs(i - _black_king_pos.row), abs(j - _black_king_pos.col));
				b_king_overloaded += overloard_distance_dangers[distance] * controls_map._array[i][j];
				//cout << "square: " << square_name(i, j) << ", piece: " << piece_name(p) << " / distance: " << (int)distance << " / overload: " << overloard_distance_dangers[distance] * controls_map._array[i][j] << endl;
			}
		}
	}

	const float overload_factor = 2.5f;
	//const float overload_factor = 0.0f;

	w_king_overloaded *= overload_factor * b_attacking_potential;
	b_king_overloaded *= overload_factor * w_attacking_potential;

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "Overloaded: " + to_string(w_king_overloaded) + " / " + to_string(b_king_overloaded) + "\n";
	//}


	// -------------------
	// *** PROTECTIONS ***
	// -------------------

	//rnq1k2r/pp2bpp1/2p1bn2/3pp3/7p/P2PP2P/1PPNBPP1/R1BQ1RKN w kq - 2 11: the king on h2 is not that bad
	//r3k2r/ppq2p2/2p1bP2/3pn3/8/P2PPB2/1PPNK2p/R1BQ3R b kq - 7 21: bug on black queenside castling???
	//r3k3/p1q2p1r/4b3/1p1p4/6P1/P2PPQb1/1P1NB1P1/R1B2RK1 b q - 6 22: castling is needed to bring another rook into the attack
	//Nnb2b1r/1p1k1p1p/p4p2/8/3p4/8/PP2PPPP/R3KB1R b KQ - 0 12

	// -----------------------
	// *** POSITION DU ROI ***
	// -----------------------

	// Proximity to the edge
	// Progress threshold beyond which sitting on an edge becomes more dangerous
	//constexpr float edge_adv = 0.85f;
	//constexpr float mult_endgame = 25.0f;
	//constexpr float safe_zone = 0.25f;

	//// Additive version, suited to the endgame
	//constexpr int edge_defense = 75;
	
	//8/8/1k6/3Q4/4K3/8/8/8 w - - 19 136
	//r1k2b1r/p5p1/2p4p/8/4p1b1/4B3/PPP2P1P/2KR2R1 w - - 0 21: 0 before taking the bishop, 200+ after

	// r1bq1b1r/ppp3pp/4k3/3np3/1nB5/2N3Q1/PPPP1PPP/R1B1K2R b KQ - 5 9 : Rf7 vs Rf5... analyser...

	// Distances to the edges
	/*int w_col_dist = min(_white_king_pos.col, 7 - _white_king_pos.col);
	int w_row_dist = min(_white_king_pos.row, 7 - _white_king_pos.row);
	int b_col_dist = min(_black_king_pos.col, 7 - _black_king_pos.col);
	int b_row_dist = min(_black_king_pos.row, 7 - _black_king_pos.row);

	int w_placement_weakness = edge_defense * ((edge_adv - _adv) * ((_adv < edge_adv) ? (max(0, w_col_dist - 1) + _white_king_pos.row * _white_king_pos.row / 2.0f) : (mult_endgame / (edge_adv - 1.0f) * (1.0f / ((w_col_dist + 1) * (w_row_dist + 1)) - safe_zone))));
	int b_placement_weakness = edge_defense * ((edge_adv - _adv) * ((_adv < edge_adv) ? (max(0, b_col_dist - 1) + (7 - _black_king_pos.row) * (7 - _black_king_pos.row) / 2.0f) : (mult_endgame / (edge_adv - 1.0f) * (1.0f / ((b_col_dist + 1) * (b_row_dist + 1)) - safe_zone))));*/

	const int w_placement_weakness = get_king_placement_weakness(true);
	const int b_placement_weakness = get_king_placement_weakness(false);

	//cout << b_placement_weakness << endl;

	// While castling is available, placement problems are ignored
	//if (_castling_rights.k_b || _castling_rights.q_b)
	//	b_placement_weakness = 0;
	//if (_castling_rights.k_w || _castling_rights.q_w)
	//	w_placement_weakness = 0;

	//const float placement_factor = 1.0f;

	//w_placement_weakness *= placement_factor;
	//b_placement_weakness *= placement_factor;

	//2k5/8/8/3QK3/8/8/8/8 b - - 26 139

	//cout << "distances: " << w_col_dist << " " << w_row_dist << " / " << b_col_dist << " " << b_row_dist << endl;
	//cout << (edge_adv - _adv) * (endgame_safe_zone - (b_col_dist + 1) * (b_row_dist + 1)) * mult_endgame / (edge_adv - 1.0f) << endl;
	//cout << "placement: " << w_placement_weakness << " / " << b_placement_weakness << endl;

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "King placement weakness: " + to_string(w_placement_weakness) + " / " + to_string(b_placement_weakness) + "\n";
	//}

	// ---------------------------------
	// *** VIRTUAL KING MOBILITY ***
	// ---------------------------------
	
	// FIXME: is this actually useful? it may break more than it fixes
	//constexpr int virtual_mobility_danger = 20;
	constexpr int virtual_mobility_danger = 0;

	// Mobility beyond which the king is in danger
	constexpr int virtual_mobility_threshold = 3;

	const int w_virtual_mobility = virtual_mobility_danger * (max(0, get_king_virtual_mobility(true) - virtual_mobility_threshold)) * (1 - _adv);
	const int b_virtual_mobility = virtual_mobility_danger * (max(0, get_king_virtual_mobility(false) - virtual_mobility_threshold)) * (1 - _adv);

	// ---------------------
	// *** RANK WEAKNESS ***
	// ---------------------

	// TODO *********
	const int w_rank_weakness = get_king_row_weakness(true);
	const int b_rank_weakness = get_king_row_weakness(false);

	// TODO

	// --------------------
	// *** WEAK SQUARES ***
	// --------------------

	const int w_weak_squares = get_weak_squares(true, true) * b_attacking_potential;
	const int b_weak_squares = get_weak_squares(false, true) * w_attacking_potential;

	// ------------------
	// *** MATING NET ***
	// ------------------

	// TODO *********

	// ------------------
	// *** PAWN STORM ***
	// ------------------

	constexpr float pawn_storm_danger = 1.5f;

	int w_pawn_storm = get_pawn_storm(true) * pawn_storm_danger * w_attacking_potential;
	int b_pawn_storm = get_pawn_storm(false) * pawn_storm_danger * b_attacking_potential;

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "Pawn storms: " + to_string(w_pawn_storm) + " / " + to_string(b_pawn_storm) + "\n";
	//}

	// ------------------
	// *** OPEN LINES ***
	// ------------------

	//5rk1/5ppp/4bn2/q2p4/2p5/b1P1BN2/3Q1PPP/1K1RRB2 w - - 2 9
	//r1b2k1r/ppp1qpp1/3bP1pn/3P4/4N3/8/PPPBQ1PP/2KR1R2 b - - 6 18

	constexpr float open_lines_danger = 2.25f;

	int w_open_lines = get_open_files_on_opponent_king(true) * open_lines_danger;
	int b_open_lines = get_open_files_on_opponent_king(false) * open_lines_danger;

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "Open lines: " + to_string(w_open_lines) + " / " + to_string(b_open_lines) + "\n";
	//}


	// ----------------------
	// *** OPEN DIAGONALS ***
	// ----------------------

	constexpr float open_diagonals_danger = 0.0f;

	int w_open_diagonals = get_open_diagonals_on_opponent_king(true) * open_diagonals_danger;
	int b_open_diagonals = get_open_diagonals_on_opponent_king(false) * open_diagonals_danger;

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "Open diagonals: " + to_string(w_open_diagonals) + " / " + to_string(b_open_diagonals) + "\n";
	//}


	// --------------
	// *** CHECKS ***
	// --------------

	const int w_checks = get_checks_value(&white_controls_map, &black_controls_map, true) * w_attacking_potential;
	const int b_checks = get_checks_value(&white_controls_map, &black_controls_map, false) * b_attacking_potential;

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "Checks: " + to_string(w_checks) + " / " + to_string(b_checks) + "\n";
	//}

	// -------------------------------------
	// *** KING WEAKNESS COMPUTATION ***
	// -------------------------------------


	// Long-term weaknesses grow when the kings are far apart
	const float king_distance_factor = 1.0f * (1 - _adv);

	constexpr float col_diff_factors[8] = { 0.0f, 0.02f, 0.12f, 0.35f, 0.75f, 0.85f, 0.90f, 0.95f };

	// Weakness amplified when the kings are far apart
	const float long_term_weakness_distance_factor = 0.75f * (1 + king_distance_factor * col_diff_factors[king_columns_diff]);

	// Short-term attack amplified when the kings are far apart
	const float short_term_weakness_distance_factor = 0.75f * (1 + king_distance_factor * col_diff_factors[king_columns_diff]);



	// Multiplier on a negative weakness, balancing short against long term
	const float negative_long_term_factor = 1.0f;

	// Compensation
	// TESTS
	// In theory the short term can never fully repay the long-term weaknesses, so a small share always remains
	// When the short term turns positive, should it grow with the long-term weaknesses too?

	//const float base_compensation = 0.20f / (short_term_weakness_distance_factor * short_term_weakness_distance_factor);
	//const float w_negative_short_term_factor = base_compensation + (1 - b_attacking_potential) * 0.25f;
	//const float b_negative_short_term_factor = base_compensation + (1 - w_attacking_potential) * 0.25f;

	// Short term = k, short term = max(0, short_term) - factor * long_term / short_term
	// Short term = 0 -> factor = 0
	// Short term = -500 -> factor = -0.33
	// Short term = -1000 -> factor = -0.5
	// Short term = 1000 -> factor = 0.5

	// Value at which the compensation equals 0.5
	constexpr int short_term_compensation_value = 500;

	//const float w_short_term_compensation_factor = 1 - 1 / (1 + abs(w_short_term_weakness) / short_term_compensation_value);

	// rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w kq - 4 4

	// Black king (White attacking)

	// Attack/Defense overload
	int w_attacking_overload = w_attacking_power - b_defending_power;
	if (w_attacking_overload > 0) {
		w_attacking_overload *= piece_overload_multiplicator;
	}
	else {
		w_attacking_overload *= piece_defense_multiplicator;
	}
	

	// Faiblesses long terme:
	int b_long_term_weakness = w_pawn_storm + w_open_lines + w_open_diagonals - b_king_protection + b_placement_weakness + b_virtual_mobility + b_weak_squares + b_rank_weakness - b_space;

	// Weakness amplified when the kings are far apart
	b_long_term_weakness *= long_term_weakness_distance_factor;


	//if (b_long_term_weakness > 0) {
	//	b_long_term_weakness *= w_attacking_potential;
	//}

	// Reduce when castling is still available?
	if (_castling_rights.k_b || _castling_rights.q_b) {
		//b_long_term_weakness *= 0.5f;
	}

	if (b_long_term_weakness < 0) {
		b_long_term_weakness *= negative_long_term_factor;
	}

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "B LONG TERM WEAKNESS: (";
		main_GUI._eval_components += "Storm: " + to_string(w_pawn_storm);
		main_GUI._eval_components += " + Lines: " + to_string(w_open_lines);
		main_GUI._eval_components += " + Diags (REDO): " + to_string(w_open_diagonals);
		main_GUI._eval_components += " - Protec: " + to_string(b_king_protection);
		main_GUI._eval_components += " + Placement: " + to_string(b_placement_weakness);
		main_GUI._eval_components += " + Exposure (?): " + to_string(b_virtual_mobility);
		main_GUI._eval_components += " + Weak squares: " + to_string(b_weak_squares);
		main_GUI._eval_components += " + Rank weakness (TODO): " + to_string(b_rank_weakness);
		main_GUI._eval_components += " + Mating nets (TODO): " + to_string(0);
		main_GUI._eval_components += " - Space: " + to_string(b_space);
		main_GUI._eval_components += ") * Kings distance : " + to_string(long_term_weakness_distance_factor);
		main_GUI._eval_components += " = " + to_string(b_long_term_weakness) + "\n";
	}

	// Attaque court terme:
	int b_short_term_weakness = w_checks + w_attacking_overload + b_king_overloaded - b_activity;

	// Weakness amplified when the kings are far apart
	b_short_term_weakness *= short_term_weakness_distance_factor;

	// Short-term / long-term compensation, between 0 and 1
	const float b_short_term_compensation_factor = w_attacking_potential <= 0.0f ? 1.0f : 1.0f - 1.0f / (1.0f + abs(b_short_term_weakness) / static_cast<float>(short_term_compensation_value) / w_attacking_potential);
	//short term = max(0, short_term) - factor * long_term / short_term

	//cout << "b short term: " << b_short_term_weakness << " / b long term: " << b_long_term_weakness << " / b factor: " << b_short_term_compensation_factor << endl;

	b_short_term_weakness = max(0, b_short_term_weakness) + (b_short_term_weakness > 0 ? 0 : -1) * b_short_term_compensation_factor * max(0, b_long_term_weakness);

	//cout << "b final short term: " << b_short_term_weakness << endl;

	//if (b_short_term_weakness < 0) {
	//	b_short_term_weakness *= b_negative_short_term_factor;
	//}

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "B SHORT TERM WEAKNESS: (";
		main_GUI._eval_components += "Checks: " + to_string(w_checks);
		main_GUI._eval_components += " + Attack : " + to_string(w_attacking_overload);
		main_GUI._eval_components += " + Overload : " + to_string(b_king_overloaded);
		main_GUI._eval_components += " - Activity: " + to_string(b_activity);
		main_GUI._eval_components += ") * Kings distance : " + to_string(short_term_weakness_distance_factor);
		main_GUI._eval_components += " = " + to_string(b_short_term_weakness) + "\n";
	}

	b_king_weakness = b_long_term_weakness + b_short_term_weakness;

	// Based on the attacking potential
	//b_king_weakness *= w_attacking_potential;


	// White king (Black attacking)

	// Attack/Defense overload
	int b_attacking_overload = b_attacking_power - w_defending_power;
	if (b_attacking_overload > 0) {
		b_attacking_overload *= piece_overload_multiplicator;
	}
	else {
		b_attacking_overload *= piece_defense_multiplicator;
	}

	// Faiblesses long terme:
	int w_long_term_weakness = b_pawn_storm + b_open_lines + b_open_diagonals - w_king_protection + w_placement_weakness + w_virtual_mobility + w_weak_squares + w_rank_weakness - w_space;

	// Weakness amplified when the kings are far apart
	w_long_term_weakness *= long_term_weakness_distance_factor;

	//if (w_long_term_weakness > 0) {
	//	w_long_term_weakness *= b_attacking_potential;
	//}

	// Reduce when castling is still available?
	if (_castling_rights.k_w || _castling_rights.q_w) {
		//w_long_term_weakness *= 0.5f;
	}

	if (w_long_term_weakness < 0) {
		w_long_term_weakness *= negative_long_term_factor;
	}

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "W LONG TERM WEAKNESS: (";
		main_GUI._eval_components += "Storm: " + to_string(b_pawn_storm);
		main_GUI._eval_components += " + Lines: " + to_string(b_open_lines);
		main_GUI._eval_components += " + Diags (REDO): " + to_string(b_open_diagonals);
		main_GUI._eval_components += " - Protec: " + to_string(w_king_protection);
		main_GUI._eval_components += " + Placement: " + to_string(w_placement_weakness);
		main_GUI._eval_components += " + Exposure (?): " + to_string(w_virtual_mobility);
		main_GUI._eval_components += " + Weak squares: " + to_string(w_weak_squares);
		main_GUI._eval_components += " + Rank weakness (TODO): " + to_string(w_rank_weakness);
		main_GUI._eval_components += " + Mating nets (TODO): " + to_string(0);
		main_GUI._eval_components += " - Space: " + to_string(w_space);
		main_GUI._eval_components += ") * Kings distance : " + to_string(long_term_weakness_distance_factor);
		main_GUI._eval_components += " = " + to_string(w_long_term_weakness) + "\n";
	}

	// Attaque court terme:
	int w_short_term_weakness = b_checks + b_attacking_overload + w_king_overloaded - w_activity;

	// Weakness amplified when the kings are far apart
	w_short_term_weakness *= short_term_weakness_distance_factor;

	const float w_short_term_compensation_factor = b_attacking_potential <= 0.0f ? 1.0f : 1.0f - 1.0f / (1.0f + abs(w_short_term_weakness) / static_cast<float>(short_term_compensation_value) / b_attacking_potential);
	//short term = max(0, short_term) - factor * long_term / short_term

	//cout << "w short term: " << w_short_term_weakness << " / w long term: " << w_long_term_weakness << " / w factor: " << w_short_term_compensation_factor << endl;

	// 2k5/ppp3Bp/2p4r/8/b3Pp2/3B1Pn1/PP3KP1/RQ6 b - - 0 12 ?????

	w_short_term_weakness = max(0, w_short_term_weakness) + (w_short_term_weakness > 0 ? 0 : -1) * w_short_term_compensation_factor * max(0, w_long_term_weakness);

	//cout << "w final short term: " << w_short_term_weakness << endl;

	//if (w_short_term_weakness < 0) {
	//	w_short_term_weakness *= w_negative_short_term_factor;
	//}

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "W SHORT TERM WEAKNESS: (";
		main_GUI._eval_components += "Checks: " + to_string(b_checks);
		main_GUI._eval_components += " + Attack : " + to_string(b_attacking_overload);
		main_GUI._eval_components += " + Overload : " + to_string(w_king_overloaded);
		main_GUI._eval_components += " - Activity: " + to_string(w_activity);
		main_GUI._eval_components += ") * Kings distance : " + to_string(short_term_weakness_distance_factor);
		main_GUI._eval_components += " = " + to_string(w_short_term_weakness) + "\n";
	}

	w_king_weakness = w_long_term_weakness + w_short_term_weakness;

	// Based on the attacking potential
	//w_king_weakness *= b_attacking_potential;


	// Add the king shielding. King weakness cannot go negative; worth revisiting, but over-protection sometimes produced absurd values
	//float overprotection_factor = 0.15f;

	// TEST: non-linear function damping the differences around 0 and avoiding excessive over-protection

	if (w_king_weakness < 0) {
		float w_overprotection = 1.0f / (1.0f - w_king_weakness / 20.0f);
		w_king_weakness *= w_overprotection;
		//w_king_weakness = 0;
	}

	if (b_king_weakness < 0) {
		float b_overprotection = 1.0f / (1.0f - b_king_weakness / 20.0f);
		b_king_weakness *= b_overprotection;
		//b_king_weakness = 0;
	}


	// rnb1kb1r/pp2pppp/2p1q3/3n4/8/2N2N2/PPPPBPPP/R1BQ1RK1 w kq - 4 7 : test h3

	//w_king_weakness = max_int(0, w_king_weakness);
	//b_king_weakness = max_int(0, b_king_weakness);


	if (display_factor != 0.0f) {
		main_GUI._eval_components += "King weakness: " + to_string((int)(w_king_weakness)) + " / " + to_string((int)(b_king_weakness)) + "\n----------\n";
	}

	// Returns the weakness difference between the kings
	const int king_safety = b_king_weakness - w_king_weakness;

	return king_safety;
}

// Tells whether a piece can be captured by the enemy, for GUI display
bool Board::is_capturable(const int row, const int col) {
	_got_moves == -1 && get_moves();

	// FIXME *** manque l'en-passant

	for (int k = 0; k < _got_moves; k++)
		if (_moves[k].end_row == row && _moves[k].end_col == col)
			return true;

	return false;
}

// Prints the PGN
void Board::display_pgn() const
{
	cout << "\n***** PGN *****\n" << main_GUI._pgn << "\n***** PGN *****" << endl;
}

// Tells from an evaluation whether it is a mate: 0 if not, otherwise the move count to mate, positive for White and negative for Black
int Board::is_eval_mate(const int e) const
{
	int abs_eval = abs(e);

	if (10 * abs_eval > mate_value) {
		int mate_moves = static_cast<int>(mate_value - abs_eval - _moves_count * mate_ply) * (e > 0 ? 1 : -1) / mate_ply + (_player && e > 0);
		return mate_moves != 0 ? mate_moves : 1;
	}
	else
		return 0;
}

// Generates the opening book
void Board::generate_opening_book(int nodes) {
	//// Lit le livre d'ouvertures actuel
	//string book = LoadFileText("resources/data/opening_book.txt");
	//cout << "Book : " << book << endl;

	//// Seek to the relevant spot in the book ----> store FENs in the book and search them?
	//to_fen();
	//size_t pos = book.find(_fen); // What if there are several? Build an array of positions, split the book into more parts, then insert in the middle
	//const string book_part_1;
	//const string book_part_2;

	//const string add_to_book = "()";

	//// Check whether every move has been tested; otherwise test one of the remaining ones with `nodes` nodes

	//const string new_book = book_part_1 + add_to_book + book_part_2;

	//SaveFileText("resources/data/opening_book.txt", const_cast<char*>(new_book.c_str()));
}

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

	for (uint8_t i = 0; i < 8; i++) {
		for (uint8_t j = 0; j < 8; j++) {
			s_white[j] += (_array[i][j] == w_pawn);
			s_black[j] += (_array[i][j] == b_pawn);
			pawns_white[i][j] = (_array[i][j] == w_pawn);
			pawns_black[i][j] = (_array[i][j] == b_pawn);
		}
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
							if (is_black(_array[k][col])) {
								division_factor += block_division_per_piece[_array[k][col] - 8] - 1.0f;
							}
							else if (is_white(_array[k][col])) {
								division_factor += self_block_division - 1.0f;
							}
						}

						// Remove the pawn to test x-ray control over the square
						_array[row][col] = none;

						SquareMap white_controls_map = get_white_controls_map();
						SquareMap black_controls_map = get_black_controls_map();

						for (uint8_t k = row + 1; k <= 7; k++) {
							int controls_diff = max(0, black_controls_map._array[k][col] - white_controls_map._array[k][col]);
							division_factor += (control_division - 1.0f) * controls_diff;
						}

						// Put the pawn back
						_array[row][col] = w_pawn;


						int passed_value = passed_pawns[row] * (!has_black_pieces ? 1.0f : 1.0f);

						// Is it connected to another pawn?
						if ((col > 0 && (pawns_white[row][col - 1] || pawns_white[row - 1][col - 1])) || (col < 7 && (pawns_white[row][col + 1] || pawns_white[row - 1][col + 1]))) {
							passed_value *= connected_passed_pawn_bonus;
						}

						//cout << "Passed pawn: " << square_name(row, col) << " (" << passed_value << " ) | " << division_factor << ": " << passed_value / division_factor * passed_adv << endl;

						// Pawn endgame -> is the king inside the square of the passed pawn?
						bool out_of_square = !has_black_pieces && !in_king_square(Pos(row, col), false);

						//cout << "Passed pawn: " << square_name(row, col) << ", Is pawn endgame: " << pawn_endgame << ", Out of square: " << out_of_square << ", bonus: " << out_of_square * out_of_square_bonus[row] << endl;

						// Add the passed pawn value
						passed_pawns_value += (passed_value / division_factor + out_of_square * out_of_square_bonus[row]) * passed_adv;

						//cout << "Passed pawn: " << square_name(row, col) << ", Value: " << (passed_value / division_factor + out_of_square * out_of_square_bonus[row]) * passed_adv << " (passed_value: " << passed_value << ", division_factor: " << division_factor << ", out_of_square bonus: " << out_of_square * out_of_square_bonus[row] << ") * passed_adv: " << passed_adv << endl;
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
							if (is_white(_array[k][col])) {
								division_factor += block_division_per_piece[_array[k][col] - 2] - 1.0f;
							}
							else if (is_black(_array[k][col])) {
								division_factor += self_block_division - 1.0f;
							}
						}

						// Remove the pawn to test x-ray control over the square
						_array[row][col] = none;

						SquareMap white_controls_map = get_white_controls_map();
						SquareMap black_controls_map = get_black_controls_map();

						for (int_fast8_t k = row - 1; k >= 0; k--) {
							int controls_diff = max(0, white_controls_map._array[k][col] - black_controls_map._array[k][col]);
							division_factor += (control_division - 1.0f) * controls_diff;
						}

						// Put the pawn back
						_array[row][col] = b_pawn;

						int passed_value = passed_pawns[7 - row] * (!has_white_pieces ? 1.0f : 1.0f);

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

	// For each file
	for (uint8_t col = 0; col < 8; col++) {
		for (uint8_t row = 1; row < 7; row++) {

			// White pawns
			if (pawns_white[row][col]) {

				// Connection through the left file

				// Pawn connected behind
				bool is_left_connected_behind = (col > 0 && pawns_white[row - 1][col - 1]);

				// Contested by an enemy pawn and not backed by a friendly pawn: drop the bonus
				//rnbqkbnr/pp3ppp/4p3/2ppP3/3P4/8/PPP2PPP/RNBQKBNR w KQkq - 0 4: c3 is the move, to indirectly reconnect e5
				if (is_left_connected_behind && (col > 1 && pawns_black[row][col - 2]) && !pawns_white[row - 2][col - 2] && !pawns_white[row - 2][col]) {
					is_left_connected_behind = false;
				}

				// Pawn connected on the same rank
				bool is_left_connected_side = (col > 0 && pawns_white[row][col - 1]);

				// Contested by an enemy pawn and not backed by a friendly pawn: drop the bonus
				if (is_left_connected_side && (col > 1 && pawns_black[row + 1][col - 2] || pawns_black[row + 1][col]) && !pawns_white[row - 2][col - 2] && !pawns_white[row - 2][col]) {
					is_left_connected_side = false;
				}


				// Connection through the right file
				
				// Pawn connected behind
				bool is_right_connected_behind = (col < 7 && pawns_white[row - 1][col + 1]);

				// Contested by an enemy pawn and not backed by a friendly pawn: drop the bonus
				if (is_right_connected_behind && (col < 6 && pawns_black[row][col + 2]) && !pawns_white[row - 2][col + 2] && !pawns_white[row - 2][col]) {
					is_right_connected_behind = false;
				}

				// Pawn connected on the same rank
				bool is_right_connected_side = (col < 7 && pawns_white[row][col + 1]);

				// Contested by an enemy pawn and not backed by a friendly pawn: drop the bonus
				if (is_right_connected_side && (col < 6 && pawns_black[row + 1][col + 2] || pawns_black[row + 1][col]) && !pawns_white[row - 2][col + 2] && !pawns_white[row - 2][col]) {
					is_right_connected_side = false;
				}

				int behind_connections = is_left_connected_behind + is_right_connected_behind;
				int side_connections = is_left_connected_side + is_right_connected_side;

				// If connected on at least one side
				if (behind_connections + side_connections > 0) {

					bool is_contested = (col > 0 && pawns_black[row + 1][col - 1]) || (col < 7 && pawns_black[row + 1][col + 1]);
					float value = connected_pawns[row] * connected_pawns_adv * column_connection_value[col] * multiple_connections[behind_connections + side_connections - is_contested];
					connected_pawns_value += value;
					//std::cout << square_name(row, col) << ", behind: " << behind_connections << ", side: " << side_connections << ", contests: " << is_contested << " = " << behind_connections + side_connections - is_contested << ": " << value << endl;
					//rnbqkb1r/pp1n2pp/4pp2/2ppP3/3P1P2/2NB1N2/PPP3PP/R1BQK2R b KQkq - 1 7: confirm that c4 really is strategically bad (it reconnects e5)
				}
			}

			// Black pawns
			else if (pawns_black[row][col]) {

				// Connection through the left file
				
				// Pawn connected behind
				bool is_left_connected_behind = (col > 0 && pawns_black[row + 1][col - 1]);

				// Contested by an enemy pawn and not backed by a friendly pawn: drop the bonus
				//rnbqkb1r/ppp2ppp/5n2/3p4/2PPp3/4P3/PP3PPP/RNBQKBNR b KQkq - 0 5: c6 is the move
				if (is_left_connected_behind && (col > 1 && pawns_white[row][col - 2] && !pawns_black[row + 2][col - 2] && !pawns_black[row + 2][col])) {
					is_left_connected_behind = false;
				}

				// Pawn connected on the same rank
				bool is_left_connected_side = (col > 0 && pawns_black[row][col - 1]);

				// Contested by an enemy pawn and not backed by a friendly pawn: drop the bonus
				if (is_left_connected_side && (col > 1 && pawns_white[row - 1][col - 2] || pawns_white[row - 1][col]) && !pawns_black[row + 2][col - 2] && !pawns_black[row + 2][col]) {
					is_left_connected_side = false;
				}

				// Connection through the right file
				
				// Pawn connected behind
				bool is_right_connected_behind = (col < 7 && pawns_black[row + 1][col + 1]);

				// Contested by an enemy pawn and not backed by a friendly pawn: drop the bonus
				if (is_right_connected_behind && (col < 6 && pawns_white[row][col + 2]) && !pawns_black[row + 2][col + 2] && !pawns_black[row + 2][col]) {
					is_right_connected_behind = false;
				}

				// Pawn connected on the same rank
				bool is_right_connected_side = (col < 7 && pawns_black[row][col + 1]);

				// Contested by an enemy pawn and not backed by a friendly pawn: drop the bonus
				if (is_right_connected_side && (col < 6 && pawns_white[row - 1][col + 2] || pawns_white[row - 1][col]) && !pawns_black[row + 2][col + 2] && !pawns_black[row + 2][col]) {
					is_right_connected_side = false;
				}

				int behind_connections = is_left_connected_behind + is_right_connected_behind;
				int side_connections = is_left_connected_side + is_right_connected_side;

				// If connected on at least one side
				if (behind_connections + side_connections > 0) {

					bool is_contested = col > 0 && pawns_white[row - 1][col - 1] || col < 7 && pawns_white[row - 1][col + 1];
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
int time_to_play_move(const int t1, int t2, const float k) {
	return t1 * k;

	// Still to improve:
	// Prendre en compte le temps de l'adversaire
	// account for the number of moves left in the game, or an approximation -> spend longer when mating or nearly lost
	// account for evaluation swings, or rising moves
	// increments are still unhandled
	// a minimum node count before moving?
}

// Computes and returns the net attack/defence balance
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

	for (int row = 0; row < 8; row++) {
		for (int col = 0; col < 8; col++) {

			// Piece on the square
			uint8_t p = _array[row][col];

			// Skip an empty square
			if (p == none)
				continue;

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

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t p = _array[row][col];

			if (p == none) {
				continue;
			}


			if (is_white(p)) {
				int black_attack = attacks_black[row][col] - defenses_white[row][col] + (defenses_white[row][col] == 0 ? undefended_malus[p - 1] : 0);

				if (black_attack < 0) {
					float division_factor = 1.0f - static_cast<float>(black_attack) / max_defense;
					black_attack = static_cast<int>(black_attack / division_factor);
				}

				black_attacks_eval += black_attack;

				//cout << "W: " << square_name(row, col) << " (" << piece_name(p) << ") : " << attacks_black[row][col] << "/" << defenses_white[row][col] << " - undefended: " << (defenses_white[row][col] == 0 ? undefended_malus[p - 1] : 0) << " - Value: " << black_attack << " | B TOTAL: " << black_attacks_eval << endl;
			}
			else {
				int white_attack = attacks_white[row][col] - defenses_black[row][col] + (defenses_black[row][col] == 0 ? undefended_malus[p - 7] : 0);

				if (white_attack < 0) {
					float division_factor = 1.0f - static_cast<float>(white_attack) / max_defense;
					white_attack = static_cast<int>(white_attack / division_factor);
				}

				white_attacks_eval += white_attack;

				//cout << "B: " << square_name(row, col) << " (" << piece_name(p) << ") : " << attacks_white[row][col] << "/" << defenses_black[row][col] << " - undefended: " << (defenses_black[row][col] == 0 ? undefended_malus[p - 7] : 0) << " - Value: " << white_attack << " | W TOTAL: " << white_attacks_eval << endl;
			}
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
int Board::material_difference() const
{
	int mat = 0;
	int w_material[6] = { 0, 0, 0, 0, 0, 0 };
	int b_material[6] = { 0, 0, 0, 0, 0, 0 };

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			const uint8_t p = _array[row][col];
			if (p > 0) {
				if (p < 6)
					w_material[p]++;
				else
					b_material[p % 6]++;
			}

			mat += main_GUI._piece_GUI_values[p % 6] * (1 - (p / 6) * 2);
		}
	}

	for (uint8_t i = 0; i < 6; i++) {
		main_GUI._missing_w_material[i] = max(0, main_GUI._base_material[i] - w_material[i]);
		main_GUI._missing_b_material[i] = max(0, main_GUI._base_material[i] - b_material[i]);
	}

	return mat;
}

// Resets the evaluation components
void Board::reset_eval() {
	_displayed_components = false;
	_advancement = false; _adv = 0;
}

// Counts the rooks on open and semi-open files and returns the value
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
int Board::get_square_controls() const
{
	// TODO: add values for square control by pieces?
	// rnbqkbnr/pp3ppp/2p5/3pp3/8/1P4P1/PBPPPPBP/RN1QK1NR b KQkq - 1 4: neither e4 nor d4 is right

	// Control value of each square, for pawns
	static constexpr int square_controls[8][8] = {
		{10,  10,  10,  10,  10,  10,  10,  10},
		{20,  20,  30,  40,  40,  30,  20,  20},
		{10,  25,  45,  65,  65,  45,  25,  10},
		{5,   35,  65, 100, 100,  65,  35,   5},
		{0,   15,  25,  50,  50,  25,  15,   0},
		{5,    5,  10,  20,  20,  10,   5,   5},
		{0,    0,   0,   0,   0,   0,   0,   0},
		{0,    0,   0,   0,   0,   0,   0,   0}
	};

	int total_control = 0;

	// Compute the squares controlled by each side's pawns
	// TODO: check whether double control matters
	bool white_controls[8][8] = { {false} };
	bool black_controls[8][8] = { {false} };

	// Add the pawn control over the squares
	for (uint8_t i = 0; i < 8; i++) {
		for (uint8_t j = 0; j < 8; j++) {
			const int p = _array[i][j];
			(j - 1 >= 0 && 7 - i - 1 >= 0) && (white_controls[7 - i - 1][j - 1] |= (p == 1));
			(j + 1 < 8 && 7 - i - 1 >= 0) && (white_controls[7 - i - 1][j + 1] |= (p == 1));
			(j - 1 >= 0 && i - 1 >= 0) && (black_controls[i - 1][j - 1] |= (p == 7));
			(j + 1 < 8 && i - 1 >= 0) && (black_controls[i - 1][j + 1] |= (p == 7));
		}
	}

	// Sum the pawn control over the squares
	for (uint8_t i = 0; i < 8; i++)
		for (uint8_t j = 0; j < 8; j++)
			total_control += (white_controls[i][j] - black_controls[i][j]) * square_controls[i][j];

	// The weight of this term depends on game progress: space matters more the more pieces remain
	constexpr float control_adv_factor = 0.2f; // Depending on how far the game has progressed

	return eval_from_progress(total_control, _adv, control_adv_factor);
}

// Returns the UCT value
float uct(const float win_chance, const float c, const int nodes_parent, const int nodes_child) {
	// cout << win_chance << ", " << nodes_parent << ", " << nodes_child << " = " << win_chance + c * sqrt(log(nodes_parent) / nodes_child) << endl;
	return win_chance + c * static_cast<float>(sqrt(log(nodes_parent) / nodes_child));
}

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
bool Board::click_m_move(const Move m, const bool orientation) const
{
	simulate_mouse_release();
	click_move(m.start_row, m.start_col, m.end_row, m.end_col , main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom, orientation, m.is_promotion());
	SetWindowFocused();

	return true;
}

// Updates a text box
void update_text_box(TextBox& text_box) {
	// If a click happens
	if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
		const Vector2 mouse_pos = GetMousePosition();
		const Rectangle rect = { text_box.x, text_box.y, text_box.width, text_box.height };

		// Over the text box
		if (CheckCollisionPointRec(mouse_pos, rect)) {
			// Activate, and show the value
			if (!text_box.active) {
				text_box.active = true;
				text_box.text = to_string(text_box.value);
			}
		}

		else {
			// Deactivate, and update the value
			if (text_box.active) {
				text_box.value = stoi(text_box.text);
				text_box.active = false;
			}
		}
	}

	// If it is active
	if (text_box.active) {
		// Take the keyboard input
		const int key = GetKeyPressed();

		// Only the relevant keys, the numeric ones
		if ((key >= 320 && key <= 329) || (key == KEY_BACKSPACE) || (key == KEY_ENTER)) {
			if (key == KEY_BACKSPACE) {
				if (!text_box.text.empty())
					text_box.text.pop_back();
			}
			else if (key == KEY_ENTER) {
				text_box.value = stoi(text_box.text);
				text_box.active = false;
			}

			else {
				const int pressed_value = key - 320;
				text_box.text += static_cast<char>(pressed_value + 48);
			}
		}
	}
}

// Draws a text box
void draw_text_box(const TextBox& text_box) {
	const Rectangle rect = { text_box.x, text_box.y, text_box.width, text_box.height };
	DrawRectangleRec(rect, text_box.main_color);

	//Vector2 text_dimensions = MeasureTextEx(textBox.text_font, textBox.text.c_str(), textBox.text_size, font_spacing * textBox.text_size); // for centred drawing
	const float text_x = text_box.x + text_box.text_size / 2;
	const float text_y = text_box.y + text_box.text_size / 4;
	DrawTextEx(text_box.text_font, text_box.text.c_str(), { text_x, text_y }, text_box.text_size, main_GUI._font_spacing * text_box.text_size, text_box.text_color);
}

// Returns the colour of the side to move (1 for White, -1 for Black)
int Board::get_color() const
{
	return _player ? 1 : -1;
}

// Computes the space advantage
int Board::get_space() const
{
	// Multiply by a weight
	// The space advantage depends on how many pieces remain

	int w_pieces = 0; int b_pieces = 0;

	for (uint8_t i = 0; i < 8; i++) {
		for (uint8_t j = 0; j < 8; j++) {
			if (is_in(_array[i][j], 2, 6))
				w_pieces++;
			else if (is_in(_array[i][j], 8, 12))
				b_pieces++;
		}
	}
		

	// Open file count
	int open_rows = 0;
	for (uint8_t j = 0; j < 8; j++) {
		bool open = true;
		for (uint8_t i = 0; i < 8; i++) {
			if (_array[i][j] == 1 || _array[i][j] == 7) {
				open = false;
				break;
			}
		}
		if (open)
			open_rows++;
	}

	// Poids
	const int w_weight = max(0, w_pieces - 2 * open_rows);
	const int b_weight = max(0, b_pieces - 2 * open_rows);

	// Avantage d'espace
	int space_area = 0;

	// Space advantage value of each central square
	int w_space[3][4]{};
	int b_space[3][4]{};

	// Assign the values for each square
	for (uint8_t j = 2; j < 6; j++) {
		// No point computing this when the weight is zero
		if (w_weight != 0) {
			// White
			for (uint8_t i = 1; i <= 3; i++) {
				// A friendly pawn on the square, or enemy pawn control over it, sets the value to 0
				if (_array[i][j] == 1 || (_array[i + 1][j - 1] == 7) || (_array[i + 1][j + 1] == 7))
					w_space[i - 1][j - 2] = 0;
				// Otherwise, with a friendly pawn less than 3 squares ahead, set the value to 2
				else if (_array[i + 1][j] == 1 || _array[i + 2][j] == 1 || _array[i + 3][j] == 1)
					w_space[i - 1][j - 2] = 2;
				else
					w_space[i - 1][j - 2] = 1;
			}
		}

		if (b_weight != 0)
		{
			// Black
			for (uint8_t i = 6; i >= 4; i--) {
				// A friendly pawn on the square, or enemy pawn control over it, sets the value to 0
				if (_array[i][j] == 7 || (_array[i - 1][j - 1] == 1) || (_array[i - 1][j + 1] == 1))
					b_space[6 - i][j - 2] = 0;
				// Otherwise, with a friendly pawn less than 3 squares ahead, set the value to 2
				else if (_array[i - 1][j] == 7 || _array[i - 2][j] == 7 || _array[i - 3][j] == 7)
					b_space[6 - i][j - 2] = 2;
				else
					b_space[6 - i][j - 2] = 1;
			}
		}
	}

	if (w_weight != 0 || b_weight != 0) {
		// Compute the space advantage
		for (uint8_t i = 0; i < 3; i++) {
			for (uint8_t j = 0; j < 4; j++) {
				space_area += w_space[i][j] * w_weight - b_space[i][j] * b_weight;
			}
		}
	}

	constexpr float space_adv_factor = 0.5f; // Depending on how far the game has progressed

	return space_area * max(0.0f, (1 + (space_adv_factor - 1) * _adv));
}

// Computes and returns an evaluation of the alignments
int Board::get_alignments() const
{
	// TODO: a piece can sometimes fully unpin another one (a bishop unpinning a diagonal)
	// 8/2p2kp1/3bp2p/4p3/1pP1P2P/1P1Q1NP1/qr2RPK1/8 w - - 5 34: here the rook on b2 unpins the queen on a2
	// rnb1kbnr/pp1ppppp/2p5/q7/8/2NP4/PPPBPPPP/R2QKBNR b KQkq - 3 3
	// r2qk2r/pb1pbpp1/1pn4n/2p1P2p/8/2NB1N1P/PP1BQPP1/3RR1K1 b kq - 2 13: rook on d1 facing the queen

	// r1b2b1r/pp3kpp/5q2/3pP3/8/P7/1PP1QPPP/R1B2RK1 b - - 0 16 : ????

	// Values of the pinned pieces (TODO: take the other values from the evaluation?)
	constexpr int pinned_king = 120;
	constexpr int pinned_queen = 100;
	constexpr int pinned_rook = 25;
	constexpr int pinned_bishop = 10;
	constexpr int pinned_knight = 8;
	constexpr int pinned_pawn = 3;

	constexpr int pieces_values[6] = { pinned_pawn, pinned_knight, pinned_bishop, pinned_rook, pinned_queen, pinned_king };

	// Values of the pieces pressuring a pin against higher-valued pieces
	// To factor into the computation: 
	// - the lower value of the two significant pieces involved in the pin
	// - the value of the pinning piece, when the pinned piece can capture it
	// - the value of the piece pressuring the pinned piece, when it is worth less (it threatens the capture)
	constexpr int pressuring_values[6] = { 100, 300, 300, 500, 900, 10000};
	constexpr float pressuring_factor = 1.5f; // FIXME: too much?

	// Blocking power of the friendly pieces
	constexpr float ally_block_values[6] = { 0.8f, 0.35f, 0.45f, 0.5f, 0.6f, 0.75f };

	// Values for the friendly pieces
	constexpr int ally_piece_value = 15;
	constexpr int ally_pawn_value = 5;

	// Pin strength, per piece
	constexpr float bishop_power = 1.00f;
	constexpr float rook_power = 1.0f;
	constexpr float queen_power = 0.3f;

	// Possible pin directions

	// Alignment value
	int w_pins = 0;
	int b_pins = 0;

	// Walk the board
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {

			// If the square holds a piece
			const uint8_t pinning_piece = _array[row][col];

			// Skip anything that is not a sliding piece
			if (!is_sliding(pinning_piece))
				continue;

			// Piece colour
			const bool pinning_piece_color = pinning_piece < b_pawn;
			const int pinning_int_color = pinning_piece_color ? 1 : -1;

			// Pressure value of the pinning piece
			const int pinning_pressure_value = pressuring_values[(pinning_piece - 1) % 6];

			// For each direction
			for (uint8_t d = 0; d < 8; d++) {

				// first 4 directions: diagonals
				// last 4 directions: straight lines

				if (!is_diagonal(pinning_piece) && d < 4)
					continue;

				if (!is_rectilinear(pinning_piece) && d >= 4)
					continue;

				// Direction
				int_fast8_t d_row = all_directions[d][0];
				int_fast8_t d_col = all_directions[d][1];

				// Piece position
				uint8_t current_row = row + d_row;
				uint8_t current_col = col + d_col;

				// List of the pieces met along the way
				uint8_t pieces[7] = { 0 };

				// Pressure values on the pins
				int pressures[7] = { 0 };

				// Progress along the direction
				uint8_t i = 0;


				// While the square is on the board
				while (current_row >= 0 && current_row <= 7 && current_col >= 0 && current_col <= 7) {
					// If the square holds a piece
					const uint8_t piece2 = _array[current_row][current_col];

					// Add the piece to the list
					if (piece2 != none) {
						pieces[i] = piece2;

						// Pressure value on the pin: the value of the cheapest piece pressuring this one
						// For now, only the pawns are tested
						//cout << (int)current_row << ", " << (int)current_col << ": " << square_name(current_row, current_col) << ", pawns for " << (pinning_piece_color ? "white?" : "black?") << endl;
						if ((current_col > 0 && _array[current_row - pinning_int_color][current_col - 1] == (pinning_piece_color ? w_pawn : b_pawn)) || (current_col < 7 && _array[current_row - pinning_int_color][current_col + 1] == (pinning_piece_color ? w_pawn : b_pawn))) {
							pressures[i] = pressuring_values[0];
							//cout << "pawn !" << endl;
						}

						i++;
					}

					// On continue
					current_row += d_row;
					current_col += d_col;
				}

				// Compute the total pin value for this direction, from the pieces met
				int total_value = 0;

				// Value of the pinning piece
				const uint8_t pinning_piece_value = pieces_values[(pinning_piece - 1) % 6];

				// Strength of the pinning piece
				const float pinning_piece_power = is_bishop(pinning_piece) ? bishop_power : (is_rook(pinning_piece) ? rook_power : queen_power);

				//cout << "color: " << pinning_piece_color << ", square: " << square_name(i, j) << endl;

				// TODO: trouver formule...
				//rnb1kbnr/1p2pppp/p1qp4/2p5/B3P3/P1N2N2/1PPP1PPP/R1BQK2R b KQkq - 1 6

				//r1bqk2r/ppp2ppp/2nb4/8/2B1n3/3P1N2/PP3PPP/RNBQR1K1 b kq - 0 9
				//r3n3/1p1q4/1Qp2pk1/8/P1B1P1br/2P1K3/1P3P2/R5R1 w - - 2 31

				// The first pieces count more than the last ones

				// ATTEMPT: for each piece, multiply by the previous one's value and divide by a distance constant
				uint8_t previous_piece = 0;
				bool is_previous_ally = true;
				uint8_t distance = 1;
				float ally_blocking_factor = 1.0f;

				for (int j = 0; j < i; j++) {
					uint8_t pinned_piece = pieces[j];
					const bool pinned_piece_color = pinned_piece < b_pawn;
					const bool ally_piece = pinned_piece_color == pinning_piece_color;

					// Stop on an enemy piece of the same type (FIXME: this should key on movement type, not necessarily the same piece)
					if ((pinned_piece - 1) % 6 == (pinning_piece - 1) % 6 && !ally_piece) {
						break;
					}

					const float division_factor = pow(distance, 4);
					const int pinned_piece_value = ally_piece ? ((pinned_piece - 1) % 6 == 0 ? ally_pawn_value : ally_piece_value) : pieces_values[(pinned_piece - 1) % 6];
					ally_blocking_factor *= ally_piece ? (1 - ally_block_values[(pinned_piece - 1) % 6]) : 1.0f;

					if (!ally_piece || !is_previous_ally) {
						const int pin_value = pinning_piece_power * pinned_piece_value * previous_piece / division_factor * ally_blocking_factor;

						// Look for a cheaper piece pressuring this pin
						// For now only pawns are tested, otherwise it gets far too slow
						// TODO: add the pressure from the other pieces
						// TODO: add the natural pressure of the pin when the pinned piece is undefended

						// Raw pressure value on the piece, were it to be captured
						const int pressure_value = pressures[j] != 0 ? max(0, pressuring_values[(pinned_piece - 1) % 6] - pressures[j]) : 0;

						// Pressure value on the piece behind in the pin, were the pinned piece to move away
						uint8_t secondary_piece = pieces[j + 1];
						const int secondary_pressure = (j >= 6 || !secondary_piece) ? 0 : max(0, pressuring_values[(pieces[j + 1] - 1) % 6] - pinning_pressure_value);

						// Final pressure = the lower of the two
						int result_pressure = min(pressure_value, secondary_pressure);

						// Seulement en pression principale?
						result_pressure *= distance == 1;

						// N1b3R1/p2kp3/1pnp4/1B6/3PP3/8/P1P2PP1/b3K3 w - - 1 18

						//cout << piece_name(pinned_piece) << (pinned_piece_color ? " (white) " : " (black) ") << " pinned by " << piece_name(pinning_piece) << (pinning_piece_color ? " (white)" : " (black)") << " on " << square_name(row, col) << ", base pressure: " << pressure_value << ", secondary pressure: " << secondary_pressure << " = " << result_pressure << " * " << pressuring_factor << " * d == 1 (d = " << (int)d << ") = " << pressuring_factor * result_pressure * (d == 1) << " + pin value: " << pin_value << endl;

						// FIXME: a pin should never be worth more than the pinned piece
						total_value += pin_value + pressuring_factor * result_pressure * !ally_piece;
					}

					//2brr3/kpQn2p1/p4p2/5P1p/6nP/1P3B2/2P5/1K1R4 w - - 6 35 : bug??

					previous_piece = pinned_piece_value;
					is_previous_ally = ally_piece;
					distance++;
				}

				// DEBUG:
				if (total_value < 0) {
					cout << "negative value (overflow) in piece alignments" << endl;
				}

				if (pinning_piece_color) {
					w_pins += total_value;
				}
				else {
					b_pins += total_value;
				}
			}
		}
	}

	// Depending on how far the game has progressed
	constexpr float alignment_adv_factor = 1.0f;

	return eval_from_progress(w_pins - b_pins, _adv, alignment_adv_factor);
}

// Updates the king positions
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
int Board::get_piece_activity() const
{
	SquareMap white_map = get_white_controls_map();
	SquareMap black_map = get_black_controls_map();

	// 2r2b2/q4p1k/P1P3p1/1p1rBpPp/1P1NbP1P/2Q5/4R3/R4K2 w - - 4 37: not better for White?

	// Strength of controlling an enemy square
	// Controlled once, twice, and so on
	static constexpr float controlled_power[8] = { 1.0f, 1.25f, 1.35f, 1.4f, 1.45f, 1.5f, 1.55f, 1.6f };

	// Control strength of each square
	static constexpr uint8_t activity_controlled_squares[8][8] = {
	{30, 30, 30, 30, 30, 30, 30, 30},
	{30, 30, 40, 60, 60, 40, 30, 30},
	{30, 30, 35, 50, 50, 35, 30, 30},
	{30, 30, 30, 30, 30, 30, 30, 30},
	{0,  0,  10, 20, 20, 10,  0,  0},
	{0,  0,  0,   5,  5,  0,  0,  0},
	{0,  0,  0,   0,  0,  0,  0,  0},
	{0,  0,  0,   0,  0,  0,  0,  0} };
	
	// Total piece activity
	int white_activity = 0;
	int black_activity = 0;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			white_activity += activity_controlled_squares[7 - row][col] * controlled_power[min(7, white_map._array[row][col])];
			black_activity += activity_controlled_squares[row][col] * controlled_power[min(7, black_map._array[row][col])];
		}
	}

	// Multiplier based on game progress
	float advancement_factor = 0.3f;

	return eval_from_progress(white_activity - black_activity, _adv, advancement_factor);
}

// Returns the map of control counts for each square, for White
SquareMap Board::get_white_controls_map() const
{
	// Control map
	SquareMap controls_map;

	// Iterate over every piece and add its control to each square
	for (uint8_t row = 0; row < 8; row++)
		for (uint8_t col = 0; col < 8; col++) {
			const uint8_t piece = _array[row][col];
			if (is_white(piece)) {
				add_piece_controls(&controls_map, row, col, piece);
			}
		}

	return controls_map;
}

// Returns the map of control counts for each square, for Black
SquareMap Board::get_black_controls_map() const
{
	// FIXME: with a rook behind another, the squares should count twice, and they do not

	// Control map
	SquareMap controls_map;

	// Iterate over every piece and add its control to each square
	for (uint8_t row = 0; row < 8; row++)
		for (uint8_t col = 0; col < 8; col++) {
			const uint8_t piece = _array[row][col];
			if (is_black(piece)) {
				add_piece_controls(&controls_map, row, col, piece);
			}
		}

	return controls_map;
}

// Adds the control of one piece to a map
bool Board::add_piece_controls(SquareMap* map, int row, int col, int piece) const
{
	if (piece == none)
		return false;

	// White pawn
	if (piece == w_pawn) {
		col > 0 && (map->_array[row + 1][col - 1]++);
		col < 7 && (map->_array[row + 1][col + 1]++);
		return true;
	}

	// Black pawn
	if (piece == b_pawn) {
		col > 0 && (map->_array[row - 1][col - 1]++);
		col < 7 && (map->_array[row - 1][col + 1]++);
		return true;
	}

	// Cavaliers
	if (is_knight(piece)) {
		for (uint8_t k = 0; k < 8; k++) {
			const uint8_t i2 = row + knight_directions[k][0];
			const uint8_t j2 = col + knight_directions[k][1];
			i2 >= 0 && i2 <= 7 && j2 >= 0 && j2 <= 7 && (map->_array[i2][j2]++);
		}
		return true;
	}

	// Fous
	if (is_bishop(piece)) {
		for (uint8_t k = 0; k < 4; k++) {
			const uint8_t mi = diag_directions[k][0];
			const uint8_t mj = diag_directions[k][1];
			uint8_t i2 = row + mi;
			uint8_t j2 = col + mj;
			while (i2 >= 0 && i2 <= 7 && j2 >= 0 && j2 <= 7) {
				map->_array[i2][j2]++;
				// Stop on an occupied square, unless the piece is a friendly bishop, queen or pawn
				//if (_array[i2][j2] != 0 && _array[i2][j2] != piece && _array[i2][j2] != (piece + 2) && _array[i2][j2] != piece - 2)
				if (_array[i2][j2] != 0 && _array[i2][j2] != piece && _array[i2][j2] != (piece + 2)) // Stop even on a friendly pawn
					break;
				i2 += mi;
				j2 += mj;
			}
		}
		return true;
	}

	// Tours
	if (is_rook(piece)) {
		for (uint8_t k = 0; k < 4; k++) {
			const uint8_t mi = rect_directions[k][0];
			const uint8_t mj = rect_directions[k][1];
			uint8_t i2 = row + mi;
			uint8_t j2 = col + mj;
			while (i2 >= 0 && i2 <= 7 && j2 >= 0 && j2 <= 7) {
				map->_array[i2][j2]++;
				// Stop on an occupied square, unless the piece is a friendly rook or queen
				if (_array[i2][j2] != 0 && _array[i2][j2] != piece && _array[i2][j2] != (piece + 1)) 
					break;
				i2 += mi;
				j2 += mj;
			}
		}
		return true;
	}

	// Dames
	if (is_queen(piece)) {
		for (uint8_t k = 0; k < 8; k++) {
			const uint8_t mi = all_directions[k][0];
			const uint8_t mj = all_directions[k][1];
			uint8_t i2 = row + mi;
			uint8_t j2 = col + mj;
			while (i2 >= 0 && i2 <= 7 && j2 >= 0 && j2 <= 7) {
				map->_array[i2][j2]++;
				bool diagonal = mi != 0 && mj != 0;
				// Stop on an occupied square, unless it is a friendly queen, a friendly bishop or pawn on a diagonal move, or a friendly rook on a non-diagonal one
				//if (_array[i2][j2] != 0 && _array[i2][j2] != piece && ((_array[i2][j2] != (piece - 2) && _array[i2][j2] != (piece - 4)) || !diagonal) && (_array[i2][j2] != (piece - 1) || diagonal))
				if (_array[i2][j2] != 0 && _array[i2][j2] != piece && ((_array[i2][j2] != (piece - 2)) || !diagonal) && (_array[i2][j2] != (piece - 1) || diagonal)) // Stop even on a friendly pawn
					break;
				i2 += mi;
				j2 += mj;
			}
		}
		return true;
	}

	// Kings
	if (is_king(piece)) {
		for (uint8_t k = 0; k < 8; k++) {
			uint8_t i2 = row + all_directions[k][0];
			uint8_t j2 = col + all_directions[k][1];
			i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8 && (map->_array[i2][j2]++);
		}
		return true;
	}

	

	return false;
}

// Returns the virtual mobility of a king
int Board::get_king_virtual_mobility(bool color) {
	// FIXME: base this on the pawns only?

	// The king is replaced by a queen, and the number of possible moves is counted
	update_kings_pos();
	const int i = color ? _white_king_pos.row : _black_king_pos.row;
	const int j = color ? _white_king_pos.col : _black_king_pos.col;
	
	// Count the number of possible moves for the new queen
	int mobility = 0;

	for (uint8_t k = 0; k < 8; k++) {
		const uint8_t mi = all_directions[k][0];
		const uint8_t mj = all_directions[k][1];
		uint8_t i2 = i + mi;
		uint8_t j2 = j + mj;

		while (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8) {
			if (_array[i2][j2] != 0)
				break;
			mobility++;
			i2 += mi;
			j2 += mj;
		}
	}


	return mobility;
}

// Returns the number of safe checks in the position, for both sides
int Board::get_checks_value(SquareMap* white_controls, SquareMap* black_controls, bool color)
{
	constexpr int initial_safe_check_value = 250;
	constexpr int initial_unsafe_check_value = 25;
	constexpr float no_escape_multiplier = 2.5f;
	constexpr float inital_division = 1.0f;
	constexpr float king_escape_division_add = 0.35f;
	constexpr float piece_block_division_add = 1.00f;

	// Raise the value when the side is to move? To be tested
	//constexpr float has_trait_multiplier = 2.0f;
	constexpr float has_trait_multiplier = 1.0f;

	//3r2k1/pp3r2/2q2pp1/3n3P/7Q/7R/1B5P/4R2K b - - 0 33: an enormous number of discoveries here
	//rnb2bnr/pppp1k1p/5q2/8/5B2/5Q2/PPP3PP/RN3RK1 b - - 0 11: why are the checks better for Black?

	int safe_checks_value = 0;
	int unsafe_checks_value = 0;

	// Position of the opposing king
	update_kings_pos();
	const Pos king_pos = color ? _black_king_pos : _white_king_pos;

	// Look at every possible move for the player
	Board b(*this);
	b._player = !color;
	if (b.in_check()) // FIXME?
		return 0;


	// FIXME *** why is another board used here?
	// plus: use the move flags to tell whether it gives check
	b._player = color;
	b.get_moves();

	for (uint8_t i = 0; i < b._got_moves; i++) {
		
		// Move
		const Move& move = b._moves[i];

		// Destination square of the move
		const uint8_t i2 = move.end_row;
		const uint8_t j2 = move.end_col;

		// Control counts of the destination square, for White and for Black
		//const uint8_t controls_ally = color ? white_controls._array[i2][j2] : black_controls._array[i2][j2];
		//const uint8_t controls_enemy = color ? black_controls._array[i2][j2] : white_controls._array[i2][j2];

		// If the destination is uncontrolled by White, or controlled only by the white king plus at least one black piece
		// FIXME: pas ouf
		//if (controls_enemy == 0 || (controls_enemy == 1 && controls_ally > 1 && abs(king_pos.i - i2) <= 1 && abs(king_pos.j - j2) <= 1)) {
		if (true) {
			// Play the move and see whether it gives check
			Board b_check(b);
			//cout << "color: " << color << ", move: " << b_check.move_label(move) << endl;
			b_check.make_move(move);


			// TODO: replace with "does the move attack the king"?
			if (b_check.in_check()) {
				b_check.get_moves(); // FIMXE: BOF

				// Number of escape squares for the king
				int king_escapes = 0;

				// Number of pieces able to block the check
				int piece_blocks = 0;

				// Is the check safe?
				bool is_safe_check = true;

				for (uint8_t j = 0; j < b_check._got_moves; j++) {
					// Any capture available to the opponent makes the check unsafe (should this be restricted to the checking piece?) 
					// FIXME: bof
					uint8_t eaten_piece = b_check._array[b_check._moves[j].end_row][b_check._moves[j].end_col];
					if (eaten_piece != none) {
						is_safe_check = false;
						break;
					}

					// Piece able to prevent the check
					uint8_t piece = b_check._array[b_check._moves[j].start_row][b_check._moves[j].start_col];

					// Escape square for the king
					if (piece == (color ? b_king : w_king)) {
						king_escapes++;
					}
					else {
						piece_blocks++;
					}
				}

				// Value of the division
				float division = inital_division + king_escapes * king_escape_division_add + piece_blocks * piece_block_division_add;

				// Value of the multiplication
				float multiplier = (king_escapes == 0 && piece_blocks == 0) ? no_escape_multiplier : 1.0f;

				//cout << "is safe check: " << is_safe_check;

				if (is_safe_check) {
					// Add the safe check value
					//cout << "color: " << color << ", king_escapes : " << king_escapes << ", piece_blocks : " << piece_blocks << ", division : " << division << ", value : " << initial_safe_check_value / division << endl;
					safe_checks_value += max(multiplier * initial_safe_check_value / division, (float)initial_unsafe_check_value); // A safe check always beats an unsafe one
				}
				else {
					// Add the unsafe check value
					//cout << "color: " << color << "value : " << initial_unsafe_check_value << endl;
					unsafe_checks_value += initial_unsafe_check_value;
				}

			}
		}
	}

	return (safe_checks_value + unsafe_checks_value) * (_player == color ? has_trait_multiplier : 1.0f);
}

// Returns the move generation speed
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
int Board::get_king_proximity()
{
	// TEST: 8/8/8/1k1K3p/6p1/6P1/7P/8 w - - 0 25
	// TEST: 8/8/3k2b1/1p5p/1P1K2p1/1B4P1/7P/8 w - - 6 12
	// 8/8/8/3k3p/5Kp1/6P1/7P/8 w - - 4 4: Kg5 should bring it closer to the unprotected pawn
	// 4k3/2p5/1p1p4/pP1Pp1p1/P3P1P1/2P1P1K1/8/8 b - - 0 41: the king cannot reach a single enemy pawn here

	// TODO: use the controlled squares to find the fastest route; hard to do, but it could help a lot

	// TODO: take into account whether the pawn is passed

	// Update the king positions
	update_kings_pos();

	// King proximity
	float proximity = 0.0f;

	// Proximity bonus for enemy pawns unprotected by another pawn
	// TODO
	constexpr float unprotected_pawn_bonus = 1.0f;

	// Proximity bonus for passed pawns
	// TODO
	//constexpr float passed_pawn_bonus = 0.5f;

	// 8/8/8/4K2p/2k3p1/6P1/7P/8 w - - 2 26 : ??
	// 8/8/8/5k2/8/8/2p1p1p1/2R3K1 w - - 0 1 : ??

	// Progress percentage from which this starts to count
	const float min_advancement = 0.65f;

	if (_adv <= min_advancement)
		return 0;


	//int w_best_bonus = 0;
	//int b_best_bonus = 0;

	// 8/7p/p3K1k1/P4p2/5P1p/2p5/2P3P1/8 b - - 9 13: why did Kg7 raise the black king proximity? (FIXED)

	// 6k1/p3r2p/3R3p/2p2N2/5P2/2P5/P5PP/6K1 b - - 0 31: after Re2 and Rd7

	constexpr float innaccessibility_multiplier = 0.5f;

	constexpr float self_pawn_multiplier = 0.25f;

	SquareMap white_king_distances = get_king_squares_distance(true);
	SquareMap black_king_distances = get_king_squares_distance(false);

	int n_pawns = 0;

	for (uint8_t row = 1; row < 7; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			const uint8_t p = _array[row][col];

			// White pawn
			if (p == w_pawn) {
				//int w_distance = max(abs(row - _white_king_pos.row), abs(col - _white_king_pos.col));
				//int b_distance = max(abs(row - _black_king_pos.row), abs(col - _black_king_pos.col));

				float w_distance = white_king_distances._array[row][col];
				float b_distance = black_king_distances._array[row][col];

				// Is the square reachable?
				if (w_distance < 0) {
					w_distance = -w_distance;
				}

				if (b_distance < 0) {
					b_distance = -b_distance / innaccessibility_multiplier;
				}

				// Pawn value (rises with advancement, when unprotected, and when passed)
				float w_pawn_value = pow(row, 1.1) * self_pawn_multiplier;
				float b_pawn_value = 1;
				//float b_pawn_value = sqrt(row);

				// Larger penalty by rank; needs revisiting. Use a true proximity rather than a distance?
				//8/8/8/4K2p/2k3p1/6P1/7P/8 w - - 2 26

				// Is the pawn unprotected by another pawn?
				bool protected_pawn = (col > 0 && _array[row - 1][col - 1] == w_pawn) || (col < 7 && _array[row - 1][col + 1] == w_pawn);
				if (!protected_pawn) {
					//w_pawn_value *= unprotected_pawn_bonus;
					b_pawn_value *= unprotected_pawn_bonus;
				}

				float w_proximity = w_distance == 0.0f ? 0.0f : w_pawn_value / w_distance; // FIXME: subtract instead of divide?
				float b_proximity = b_distance == 0.0f ? 0.0f : b_pawn_value / b_distance;

				//cout << endl << "w_pawn on " << square_name(row, col) << ": " << endl
				//	<< "WHITE: value: " << w_pawn_value << " / distance: " << w_distance << ": proximity = " << w_proximity << endl
				//	<< "BLACK: value: " << b_pawn_value << " / distance: " << b_distance << ": proximity = " << b_proximity << endl;

				//8/8/8/6Kp/3k2p1/6P1/7P/8 b - - 5 27
				
				proximity += w_proximity;
				proximity -= b_proximity;

				//8/8/8/7p/3k1Kp1/6P1/7P/8 w - - 4 27

				// 8/3k4/3p1K2/p2P1p2/P2P1P2/8/8/8 b - - 27 14: the white king is the closer one

				n_pawns++;
			}

			// Black pawn
			else if (p == b_pawn) {
				//int w_distance = max(abs(row - _white_king_pos.row), abs(col - _white_king_pos.col));
				//int b_distance = max(abs(row - _black_king_pos.row), abs(col - _black_king_pos.col));

				float w_distance = white_king_distances._array[row][col];
				float b_distance = black_king_distances._array[row][col];

				// Is the square reachable?
				if (w_distance < 0) {
					w_distance = -w_distance / innaccessibility_multiplier;
				}

				if (b_distance < 0) {
					b_distance = -b_distance;
				}

				// Pawn value (rises with advancement, when unprotected, and when passed)
				//float w_pawn_value = sqrt(7 - row);
				float b_pawn_value = pow(7 - row, 1.1) * self_pawn_multiplier;
				float w_pawn_value = 1;

				// Is the pawn unprotected by another pawn?
				bool protected_pawn = (col > 0 && _array[row + 1][col - 1] == b_pawn) || (col < 7 && _array[row + 1][col + 1] == b_pawn);
				if (!protected_pawn) {
					w_pawn_value *= unprotected_pawn_bonus;
					//b_pawn_value *= unprotected_pawn_bonus;
				}

				float w_proximity = w_distance == 0.0f ? 0.0f : w_pawn_value / w_distance; // FIXME: subtract instead of divide?
				float b_proximity = b_distance == 0.0f ? 0.0f : b_pawn_value / b_distance;

				//cout << endl << "b_pawn on " << square_name(row, col) << ": " << endl
				//	<< "WHITE: value: " << w_pawn_value << " / distance: " << w_distance << ": proximity = " << w_proximity << endl
				//	<< "BLACK: value: " << b_pawn_value << " / distance: " << b_distance << ": proximity = " << b_proximity << endl;

				proximity += w_proximity;
				proximity -= b_proximity;

				n_pawns++;
			}

		}
	}

	// Delete the maps
	white_king_distances.~SquareMap();
	black_king_distances.~SquareMap();

	const int multiplier = 300;
	const double average_proximity = n_pawns == 0 ? 0.0f : proximity / pow(n_pawns / 2.0, 0.75f);
	
	return multiplier * average_proximity * (_adv - min_advancement) / (1.0f - min_advancement);
}


// Computes rook activity and mobility
int Board::get_rook_activity() const
{
	// Buggy or badly evaluated positions:
	// 2bq1k1r/br3p2/p2p2n1/npp1p2p/4P1pP/2PPN1B1/PPBN1PP1/R3QR1K w - - 0 1: Kg1 raises the rook activity
	// r2qr1k1/pp3ppp/2nb1n2/4p3/2P3P1/1PNb3P/PB1PNPB1/R2QK2R w KQ - 0 12: Rh2 raises its activity
	// rnbqkb1r/ppp2ppp/4pn2/3p4/3P4/4PN2/PPP2PPP/RNBQKB1R w KQkq - 0 4 : il veut h4

	// r2qk2r/pb1pbpp1/1pn4n/2p1P2p/8/2NB1N1P/PP1BQPP1/2R1R1K1 b kq - 2 13 vs r2qk2r/pb1pbpp1/1pn4n/2p1P2p/8/2NB1N1P/PP1BQPP1/3RR1K1 b kq - 2 13

	// Cas de figure:
	// 1. Rook trapped by the king: mobility < 4 -> penalty, larger still when the king cannot castle, divided by mobility
	// 2. Activity depends mostly on vertical mobility, the distance to the nearest pawn ahead

	// this should decrease with the number of open files

	constexpr int vertical_mobility_bonus = 50;
	constexpr int horizontal_mobility_bonus = 35;

	// Bonus when it attacks the opposing camp
	constexpr float row_bonus[8] = { 1.0f, 1.0f, 1.2f, 1.5f, 1.75f, 2.0f, 3.5f, 2.5f };

	// Penalty for lack of mobility
	constexpr float bad_mobility_min = 2.5f;
	constexpr int bad_mobility_malus = 1000;

	// Baseline value of rook activity
	constexpr int normal_activity = 5 * vertical_mobility_bonus + 7 * horizontal_mobility_bonus;

	// r2q3r/ppp2kpp/2n2n2/2b1p3/4P1b1/2N2N2/PPPP2PP/R1BQ1R1K b - - 3 9: after Kg8 the rook ends up stuck on h8


	//1r5k/3n2p1/5nbp/1Np5/P4b2/1P1P4/1BP2PP1/R3R1K1 w - - 0 25

	float activity = 0.0f;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			const uint8_t p = _array[row][col];

			// White rook
			if (p == w_rook) {

				int rook_activity = -normal_activity;
				
				// Horizontal mobility
				float h_mobility = 0.0f;

				// To the right
				for (uint8_t k = col + 1; k < 8; k++) {
					const uint8_t p2 = _array[row][k];
					if (p2 != w_pawn && p2 != b_pawn && p2 != w_king)
						h_mobility += row_bonus[row];
					else
						break;
				}

				// To the left
				for (int_fast8_t k = col - 1; k >= 0; k--) {
					const uint8_t p2 = _array[row][k];
					if (p2 != w_pawn && p2 != b_pawn && p2 != w_king)
						h_mobility += row_bonus[row];
					else
						break;
				}

				// Vertical mobility
				float v_mobility = 0.0f;

				// Upwards
				for (uint8_t k = row + 1; k < 8; k++) {
					const uint8_t p2 = _array[k][col];
					if (p2 != w_pawn && p2 != b_pawn && p2 != w_king)
						v_mobility += row_bonus[k];
					else
						break;
				}

				// Downwards
				for (int_fast8_t k = row - 1; k >= 0; k--) {
					const uint8_t p2 = _array[k][col];
					if (p2 != w_pawn && p2 != b_pawn && p2 != w_king)
						v_mobility += row_bonus[k];
					else
						break;
				}

				// Bonus for vertical mobility
				rook_activity += vertical_mobility_bonus * v_mobility;

				// Bonus for horizontal mobility
				rook_activity += horizontal_mobility_bonus * h_mobility;

				// Penalty for lack of mobility
				const int total_mobility = h_mobility + v_mobility;

				// FIXME: make this a more linear function?
				if (total_mobility < bad_mobility_min) {
					//rook_activity -= bad_mobility_malus;
					rook_activity -= bad_mobility_malus * (bad_mobility_min - total_mobility);
				}

				//cout << "White rook (" << square_name(row, col) << "): h_mobility = " << h_mobility << ", v_mobility = " << v_mobility << ", total_mobility = " << total_mobility << "/" << bad_mobility_min << " (-" << to_string((total_mobility < bad_mobility_min) ? bad_mobility_malus : 0) << "), activity = " << rook_activity << endl;
			
				activity += rook_activity;
			}

			//1r2q2k/4p1b1/p1n1Q1p1/1pp2p1p/3p4/3P1N1P/PPP1RPP1/4R1K1 b - - 3 26

			// Black rook
			else if (p == b_rook) {

				int rook_activity = -normal_activity;

				// Horizontal mobility
				float h_mobility = 0.0f;

				// To the right
				for (uint8_t k = col + 1; k < 8; k++) {
					const uint8_t p2 = _array[row][k];
					if (p2 != w_pawn && p2 != b_pawn && p2 != b_king)
						h_mobility += row_bonus[7 - row];
					else
						break;
				}

				// To the left
				for (int_fast8_t k = col - 1; k >= 0; k--) {
					const uint8_t p2 = _array[row][k];
					if (p2 != w_pawn && p2 != b_pawn && p2 != b_king)
						h_mobility += row_bonus[7 - row];
					else
						break;
				}

				// Vertical mobility
				float v_mobility = 0.0f;

				// Upwards
				for (uint8_t k = row + 1; k < 8; k++) {
					const uint8_t p2 = _array[k][col];
					if (p2 != w_pawn && p2 != b_pawn && p2 != b_king)
						v_mobility += row_bonus[7 - k];
					else
						break;
				}

				// Downwards
				for (int_fast8_t k = row - 1; k >= 0; k--) {
					const uint8_t p2 = _array[k][col];
					if (p2 != w_pawn && p2 != b_pawn && p2 != b_king)
						v_mobility += row_bonus[7 - k];
					else
						break;
				}

				// Bonus for vertical mobility
				rook_activity += vertical_mobility_bonus * v_mobility;

				// Bonus for horizontal mobility
				rook_activity += horizontal_mobility_bonus * h_mobility;

				// Penalty for lack of mobility
				const int total_mobility = h_mobility + v_mobility;

				// FIXME: make this a more linear function?
				if (total_mobility < bad_mobility_min) {
					//rook_activity -= bad_mobility_malus;
					rook_activity -= bad_mobility_malus * (bad_mobility_min - total_mobility);
				}

				//cout << "Black rook (" << square_name(row, col) << "): h_mobility = " << h_mobility << ", v_mobility = " << v_mobility << ", total_mobility = " << total_mobility << "/" << bad_mobility_min << " (-" << to_string((total_mobility < bad_mobility_min) ? bad_mobility_malus : 0) << "), activity = " << rook_activity << endl;

				activity -= rook_activity;
			}
		}
	}

	// Multiplier based on how far the game has progressed
	float advancement_factor = 1.0f;

	//cout << "Final rook activity: " << activity << ", advancement factor: " << advancement_factor << " => " << eval_from_progress(activity, _adv, advancement_factor) << endl;

	return eval_from_progress(activity, _adv, advancement_factor);
}


// Equality operator: compares only piece placement, castling rights and en passant
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
						if (weak)
							cout << "shouldn't be weak here.. ?" << endl;
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
						if (weak)
							cout << "shouldn't be weak here.. ?" << endl;
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
						if (weak)
							cout << "shouldn't be weak here.. ?" << endl;
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
						if (weak)
							cout << "shouldn't be weak here.. ?" << endl;
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
string Board::algebric_notation(Move move) const {
	string move_notation = main_GUI._abc8[move.start_col] + to_string(move.start_row + 1) + main_GUI._abc8[move.end_col] + to_string(move.end_row + 1);

	// Promotion
	if (move.end_row == 7 && _array[move.start_row][move.start_col] == 1)
		move_notation += "q";
	else if (move.end_row == 0 && _array[move.start_row][move.start_col] == 7)
		move_notation += "q";

	return move_notation;
}

// Converts an algebraic notation into a move
Move Board::move_from_algebric_notation(string notation) {
	return Move(notation[1] - '1', notation[0] - 'a', notation[3] - '1', notation[2] - 'a');
}

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
void Board::get_zobrist_key()
{
	// We assume the key is never 0
	/*if (_zobrist_key != 0)
		return;*/

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
	const float pawns_factor = n_pawns_winnable[pawns_count];

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
	winnable_value *= 1.0f - pow(position_nature, 1.0) * closed_position_draw_factor;
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
	const int passed_pawns_count = get_passed_pawns_count(color);
	//cout << "passed pawns count: " << passed_pawns_count << endl;
	if (passed_pawns_count > 8) {
		cout << "too many passed pawns" << endl;
	}

	winnable_value = 1.0f - (1.0f - winnable_value) * (1.0f - passed_pawn_winnable_bonuses[passed_pawns_count]);

	if (display) {
		cout << "- passed pawns count: " << (int)passed_pawns_count << endl;
		cout << "- final winnable value after passed pawns: " << winnable_value << endl;
	}


	return winnable_value;
}

// Computes the winning-chance values for each side
void Board::get_winnable_values(Evaluation* eval, float position_nature) const {
	eval->_winnable_white = get_winnable(eval, true, position_nature);
	eval->_winnable_black = get_winnable(eval, false, position_nature);
}

// Returns the activity of the bishops along the diagonals
int Board::get_bishop_activity() const {
	// Bishop mobility = number of non-pawn squares along its diagonals

	// Baseline bishop activity
	constexpr int normal_bishop_activity = 3;

	// Bonus for the white bishop
	int w_bishop_activity = 0;

	// Bonus for the black bishop
	int b_bishop_activity = 0;

	for (int row = 0; row < 8; row++) {
		for (int col = 0; col < 8; col++) {
			uint8_t p = _array[row][col];

			// White bishop
			if (p == w_bishop) {

				w_bishop_activity -= normal_bishop_activity;

				// Diagonale haut-gauche
				for (uint8_t k = 1; k < min(row, col) + 1; k++) {
					if (!is_pawn(_array[row - k][col - k]))
						w_bishop_activity++;
					else
						break;
				}

				// Diagonale haut-droite
				for (uint8_t k = 1; k < min(row, 7 - col) + 1; k++) {
					if (!is_pawn(_array[row - k][col + k]))
						w_bishop_activity++;
					else
						break;
				}

				// Diagonale bas-gauche
				for (uint8_t k = 1; k < min(7 - row, col) + 1; k++) {
					if (!is_pawn(_array[row + k][col - k]))
						w_bishop_activity++;
					else
						break;
				}

				// Diagonale bas-droite
				for (uint8_t k = 1; k < min(7 - row, 7 - col) + 1; k++) {
					if (!is_pawn(_array[row + k][col + k]))
						w_bishop_activity++;
					else
						break;
				}
			}

			// Black bishop
			else if (p == b_bishop) {

				b_bishop_activity -= normal_bishop_activity;

				// Diagonale haut-gauche
				for (uint8_t k = 1; k < min(row, col) + 1; k++) {
					if (!is_pawn(_array[row - k][col - k]))
						b_bishop_activity++;
					else
						break;
				}

				// Diagonale haut-droite
				for (uint8_t k = 1; k < min(row, 7 - col) + 1; k++) {
					if (!is_pawn(_array[row - k][col + k]))
						b_bishop_activity++;
					else
						break;
				}

				// Diagonale bas-gauche
				for (uint8_t k = 1; k < min(7 - row, col) + 1; k++) {
					if (!is_pawn(_array[row + k][col - k]))
						b_bishop_activity++;
					else
						break;
				}

				// Diagonale bas-droite
				for (uint8_t k = 1; k < min(7 - row, 7 - col) + 1; k++) {
					if (!is_pawn(_array[row + k][col + k]))
						b_bishop_activity++;
					else
						break;
				}
			}
		}
	}

	// Multiplier based on game progress
	float bishop_activity_advancement_factor = 0.5f;

	// TODO: make this non-linear?

	return eval_from_progress(w_bishop_activity - b_bishop_activity, _adv, bishop_activity_advancement_factor);
}

// Tells whether a move is legal
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

// Prints the position history, as Zobrist keys
//void Board::display_positions_history() const
//{
//	cout << "Positions history:" << endl;
//
//	for (auto const& x : _positions_history)
//	{
//		cout << x << endl;
//	}
//}

// Returns the evaluation display
string Board::evaluation_to_string(int eval) const {
	string eval_string = "";

	if (eval > 0)
		eval_string += "+";

	// Is this a mate?
	if (int mate = is_eval_mate(eval); mate != 0) {
		if (eval < 0)
			eval_string += "-";

		eval_string += "M";
		eval_string += to_string(abs(mate));
	}
	else {
		eval_string += to_string(eval);
	}

	return eval_string;
}

// Returns the trapped-piece evaluation
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
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t p = _array[row][col];

			if (p == none)
				continue;

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
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t p = _array[row][col];

			if (p == none)
				continue;

			// If this is a white piece
			if (is_white(p)) {

				// Distance to the centre of mass
				float base_distance = sqrt(pow(row - w_center_of_mass_i, 2) + pow(col - w_center_of_mass_j, 2));

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
				float base_distance = sqrt(pow(row - b_center_of_mass_i, 2) + pow(col - b_center_of_mass_j, 2));

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
	}

	//cout << "w_trapped_pieces: " << w_trapped_pieces << endl;
	//cout << "b_trapped_pieces: " << b_trapped_pieces << endl;

	return (b_trapped_pieces - w_trapped_pieces);
}

// Adjusts the piece values, penalty or bonus, according to the type of position
int Board::get_updated_piece_values() const {
	// Rook penalty based on the number of non-open files
	// Penalty for bishops in a closed position, with the diagonals shut
	// Same for the queen. Bonus in the opposite cases

	// *** TODO ***
	return 0;
}

// Returns the nature of the position as a number: 0 = open, 1 = closed
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

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t p = _array[row][col];

			if (p == w_pawn || p == b_pawn) {
				pawns++;

				// If the pawn is blocked by another pawn
				if (p == w_pawn && (_array[row + 1][col] == w_pawn || _array[row + 1][col] == b_pawn))
					blocked_pawns++;
				else if (p == b_pawn && (_array[row - 1][col] == w_pawn || _array[row - 1][col] == b_pawn))
					blocked_pawns++;
			}
		}
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
	int queenside_file_weakness = can_kingside_castle ? get_open_files_on_opponent_king_at_column(player, 2) : 0;

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
int Board::get_king_escape_squares(bool color) {

	// Square control by the enemy pieces
	SquareMap control_map = color ? get_black_controls_map() : get_white_controls_map();

	// King position
	update_kings_pos();

	Pos king_pos = color ? _white_king_pos : _black_king_pos;

	// Number of retreat squares
	int escape_squares = 0;

	// For each square around the king
	for (int i = -1; i < 2; i++) {
		for (int j = -1; j < 2; j++) {

			// Square coordinates
			uint8_t new_i = king_pos.row + i;
			uint8_t new_j = king_pos.col + j;

			// If the square is off the board
			if (new_i < 0 || new_i > 7 || new_j < 0 || new_j > 7)
				continue;

			// If a friendly piece stands on the square
			uint8_t p = _array[new_i][new_j];
			if (p != 0 && (color ? p <= w_king : p <= b_king))
				continue;


			// If the square is controlled by an enemy piece
			if (control_map._array[new_i][new_j] != 0)
				continue;

			escape_squares++;
		}
	}

	return escape_squares;
}

// Returns a value for the pieces attacking the enemy king
int Board::get_king_attackers(bool color) {
	// Sliding pieces: just walk the rank/file/diagonal. If a pawn blocks, is it a pawn near the king? Otherwise, does it control squares around the king?

	// FIXME: should only the piece count matter?
	// Should each piece type carry its own value?
	// Should this be weighted by the distance to the king?

	// TODO: prendre en compte distance 2??

	//8/8/8/2r2pp1/1k5p/2b4P/4K3/1Q6 b - - 81 133
	//rnbr3k/ppp1qppB/4p2p/1P2P3/2Pn4/P4N2/2Q2PPP/RN2K2R w KQ - 3 15
	//6rk/1p3p1p/2nN1q2/2Q2p2/3p4/PP5P/5PP1/2R3K1 b - - 1 28: the queen attacks when it is placed on e6??
	//6rk/1p3p1p/2nNq3/2Q2p2/3p4/PP5P/5PP1/2R3K1 w - - 2 29 : bug?
	//r1bqk2r/pppp1ppp/2n5/2b1p3/2BPP1n1/5N2/PPP2P1P/RNBQ1RK1 b kq - 0 6 ??
	//r1br2k1/pp2Rp2/6nB/7Q/3p4/8/5PP1/6K1 b - - 0 5: 300 here?
	//6R1/5p2/5kp1/2q5/pp4B1/2n1R3/5PKP/8 b - - 5 45: 700 here?
	//1r6/7p/p1P1p3/4kp2/1P1Rp3/4KPP1/8/8 b - - 0 49 ...
	//rnb4r/ppppbk1p/5n2/6Q1/8/2N1p3/PPP3PP/5RK1 w - - 4 16: Nd5 brings a major attacker in
	//1rbq1r2/2p2pk1/p2p1nn1/4p1N1/p3P2p/2PPP3/RPB3PP/3QBRK1 b - - 1 2: h3 lowers Black's attack? because it denies the bishop the h3 square?
	// 1rbq3r/b4pk1/p1p3n1/4p1PQ/P3P3/1BP3P1/3N1P2/R4K1R w - - 1 8

	// rn1q1rkn/pb2bpp1/1ppp4/5P2/3P4/2N2B2/PPP3PP/R1BQR1K1 b - - 4 14 vs rn1q1rkn/pb2bpp1/1ppp4/5P2/3P4/2N2B1R/PPP3PP/R1BQ2K1 b - - 4 14

	// 2r5/3r4/p1p1pk2/PpRnR3/3P2pp/4P3/7P/1B5K w - - 0 38

	// Value of a piece attacking the enemy king
	constexpr int attacking_value[7] = { 0, 100, 110, 114, 117, 119, 120 };

	// Attack value of a piece hitting the outer ring around the king
	constexpr int semi_attack_value = 40;

	// Attack factor per piece (pawn, knight, bishop, rook, queen, king)
	constexpr float piece_attack_factor[6] = { 0.60f, 1.00f, 0.95f, 1.10f, 1.30f, 0.85f };

	// Semi-attack factor per piece (pawn, knight, bishop, rook, queen, king)
	constexpr float piece_semi_attack_factor[6] = { 0.75f, 1.00f, 0.95f, 1.10f, 1.01f, 0.70f };

	// Attack factor based on the distance to the king


	// Update the king positions
	update_kings_pos();

	// King position
	Pos king_pos = color ? _black_king_pos : _white_king_pos;
	Pos opponent_king_pos = color ? _white_king_pos : _black_king_pos;

	// Number of controls around the king
	int king_attackers = 0;

	//1k1rr3/1pp1q3/pnn1b3/4p3/3pP1p1/PP1P3p/1BPNN2K/R3QR1B b - - 1 46

	// Look at every friendly piece on the board
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t p = _array[row][col];

			uint8_t attacks = 0;
			uint8_t semi_attacks = 0;

			// Pawn
			if (p == (color ? w_pawn : b_pawn)) {

				// Squares controlled by the pawn
				uint8_t di = abs(row + (color ? 1 : -1) - king_pos.row);
				uint8_t dj1 = abs(col - 1 - king_pos.col);
				uint8_t dj2 = abs(col + 1 - king_pos.col);

				uint8_t p2a = _array[row + (color ? 1 : -1)][col - 1];

				// If the pawn controls a square around the king
				if (col > 0 && di <= 2 && dj1 <= 2 && p2a != (color ? w_pawn : b_pawn)) {
					if (di <= 1 && dj1 <= 1) {
						attacks++;
					}
					else {
						semi_attacks++;
					}
				}

				uint8_t p2b = _array[row + (color ? 1 : -1)][col + 1];

				if (col < 7 && di <= 2 && dj2 <= 2 && p2b != (color ? w_pawn : b_pawn)) {
					if (di <= 1 && dj2 <= 1) {
						attacks++;
					}
					else {
						semi_attacks++;
					}
				}
			}

			// Knight
			if (p == (color ? w_knight : b_knight)) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_i = row + knight_directions[m][0];
					int new_j = col + knight_directions[m][1];

					if (!is_in(new_i, 0, 7) || !is_in(new_j, 0, 7))
						continue;

					uint8_t p2 = _array[new_i][new_j];

					// The square cannot be attacked
					if (p2 == (color ? w_pawn : b_pawn))
						continue;

					uint8_t di = abs(new_i - king_pos.row);
					uint8_t dj = abs(new_j - king_pos.col);

					// If the knight controls a square around the king
					if (di <= 2 && dj <= 2) {
						if (di <= 1 && dj <= 1) {
							attacks++;
						}
						else {
							semi_attacks++;
						}
					}
				}
			}

			// Straight-line sliders
			if ((p == (color ? w_rook : b_rook)) || (p == (color ? w_queen : b_queen))) {

				for (uint8_t m = 0; m < 4; m++) {

					// Is the piece obstructed by another piece in this direction?
					bool blocked = false;

					int mi = rect_directions[m][0];
					int mj = rect_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint8_t p2 = _array[new_i][new_j];

						uint8_t di = abs(new_i - king_pos.row);
						uint8_t dj = abs(new_j - king_pos.col);

						// The square cannot be attacked
						if (p2 == (color ? w_pawn : b_pawn))
							break;

						// If the piece controls a square around the king
						if (di <= 2 && dj <= 2) {
							if (di <= 1 && dj <= 1 && !blocked) {
								attacks++;
							}
							else {
								semi_attacks++;
							}
						}

						// If a piece blocks the square
						if (p2 != none) {
							blocked = true;
						}

						// If a pawn blocks the square
						if (p2 == w_pawn || p2 == b_pawn)
							break;

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// Diagonal sliders
			if ((p == (color ? w_bishop : b_bishop)) || (p == (color ? w_queen : b_queen))) {

				for (uint8_t m = 0; m < 4; m++) {

					// Is the piece obstructed by another piece in this direction?
					bool blocked = false;

					int mi = diag_directions[m][0];
					int mj = diag_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint8_t p2 = _array[new_i][new_j];

						uint8_t di = abs(new_i - king_pos.row);
						uint8_t dj = abs(new_j - king_pos.col);

						// The square cannot be attacked
						if (p2 == (color ? w_pawn : b_pawn))
							break;

						// If the piece controls a square around the king
						if (di <= 2 && dj <= 2) {
							if (di <= 1 && dj <= 1 && !blocked) {
								attacks++;
							}
							else {
								semi_attacks++;
							}
						}

						// If a piece blocks the square
						if (p2 != none) {
							blocked = true;
						}

						// If a pawn blocks the square
						if (p2 == w_pawn || p2 == b_pawn)
							break;

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// King
			if (p == (color ? w_king : b_king)) {
				for (int i = -1; i < 2; i++) {
					for (int j = -1; j < 2; j++) {
						int new_i = i + opponent_king_pos.row;
						int new_j = j + opponent_king_pos.col;

						if (!is_in(new_i, 0, 7) || !is_in(new_j, 0, 7))
							continue;

						uint8_t p2 = _array[new_i][new_j];

						// The square cannot be attacked
						if (p2 == (color ? w_pawn : b_pawn))
							break;

						uint8_t di = abs(new_i - king_pos.row);
						uint8_t dj = abs(new_j - king_pos.col);

						// If the king controls a square around the king
						if (di <= 2 && dj <= 2) {
							if (di <= 1 && dj <= 1) {
								attacks++;
							}
							else {
								semi_attacks++;
							}
						}
					}
				}
			}

			if (attacks > 6) {
				cout << "BUG: too many attacks from a single piece... check get_king_attackers()" << endl;
			}
			else {
				if (attacks > 0) {
					king_attackers += attacking_value[attacks] * piece_attack_factor[(p - 1) % 6];
					//cout << "color: " << color << ", piece: " << piece_name(p) << "(" << square_name(row, col) << "), attacks : " << (int)attacks << ", value : " << attacking_value[attacks] << ", piece factor : " << piece_attack_factor[(p - 1) % 6] << ", total : " << attacking_value[attacks] * piece_attack_factor[(p - 1) % 6] << endl;
				}
				else if (semi_attacks > 0) {
					king_attackers += semi_attack_value * piece_semi_attack_factor[(p - 1) % 6] * pow(semi_attacks, 0.3);
					//cout << "color: " << color << ", piece: " << piece_name(p) << "(" << square_name(row, col) << "), semi-attacks : " << (int)semi_attacks << ", value : " << semi_attack_value * piece_semi_attack_factor[(p - 1) % 6] * pow(semi_attacks, 0.3) << endl;
				}
			}
		}
	}

	//cout << "king_attackers: " << king_attackers << endl;

	//1r1qr3/5p1k/3p1Ppb/p2N3p/2pPP2P/2Pn3B/P3QR2/5RK1 w - - 1 22

	return king_attackers;
}

int Board::get_king_defenders(bool color) {
	// Sliding pieces: just walk the rank/file/diagonal. If a pawn blocks, is it a pawn near the king? Otherwise, does it control squares around the king?

	// r1b2b1r/ppN3pp/1k6/2p5/3Q1B2/8/PP3PPP/n1R3K1 w - - 0 20: Black has few defenders here

	// 5r2/1pk3p1/2pr3p/p1n2P2/2PN4/2P1pBP1/6KP/1R1R4 b - - 0 29 : y'a r

	// Value of a piece defending the king
	constexpr int defending_value[9] = { 0, 100, 110, 115, 119, 122, 125, 128, 130 };

	// Defence value of a piece covering the outer ring around the king
	constexpr int semi_defense_value = 30;

	// Defence factor per piece (pawn, knight, bishop, rook, queen, king)
	constexpr float piece_defense_factor[6] = { 0.5f, 1.5f, 1.35f, 1.2f, 0.75f, 1.0f };

	// The queen is a poor defender, being so easily exposed

	// Update the king positions
	update_kings_pos();

	// King position
	Pos king_pos = color ? _white_king_pos : _black_king_pos;

	// Number of controls around the king
	int king_defenders = 0;

	// Look at every friendly piece on the board
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t p = _array[row][col];

			uint8_t defenses = 0;
			uint8_t semi_defenses = 0;

			// Pawn: TODO, revisit
			if (p == (color ? w_pawn : b_pawn)) {

				// Squares controlled by the pawn
				uint8_t di = abs(row + (color ? 1 : -1) - king_pos.row);
				uint8_t dj1 = abs(col - 1 - king_pos.col);
				uint8_t dj2 = abs(col + 1 - king_pos.col);
				bool front_square = color ? row >= king_pos.row : row <= king_pos.row;

				// If the pawn controls a square around the king
				if (col > 0 && di <= 2 && dj1 <= 2) {
					if (front_square && di <= 1 && dj1 <= 1) {
						defenses++;
					}
					else {
						semi_defenses++;
					}
				}

				if (col < 7 && di <= 2 && dj2 <= 2) {
					if (di <= 1 && dj2 <= 1) {
						defenses++;
					}
					else {
						semi_defenses++;
					}
				}
			}

			// Knight
			if (p == (color ? w_knight : b_knight)) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_i = row + knight_directions[m][0];
					int new_j = col + knight_directions[m][1];

					if (!is_in(new_i, 0, 7) || !is_in(new_j, 0, 7))
						continue;

					uint8_t di = abs(new_i - king_pos.row);
					uint8_t dj = abs(new_j - king_pos.col);
					bool front_square = color ? new_i > king_pos.row : new_i < king_pos.row;

					// If the knight controls a square around the king
					if (di <= 2 && dj <= 2) {
						if (front_square && di <= 1 && dj <= 1) {
							defenses++;
						}
						else {
							semi_defenses++;
						}
					}
				}
			}

			// Straight-line sliders
			if ((p == (color ? w_rook : b_rook)) || (p == (color ? w_queen : b_queen))) {

				for (uint8_t m = 0; m < 4; m++) {
					int mi = rect_directions[m][0];
					int mj = rect_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint8_t p2 = _array[new_i][new_j];

						uint8_t di = abs(new_i - king_pos.row);
						uint8_t dj = abs(new_j - king_pos.col);
						bool front_square = color ? new_i > king_pos.row : new_i < king_pos.row;

						// If the piece controls a square around the king
						if (di <= 2 && dj <= 2) {
							if (front_square && di <= 1 && dj <= 1) {
								defenses++;
							}
							else {
								semi_defenses++;
							}
						}

						// If a piece blocks the square
						if (p2 != none)
							break;

						// Si 

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// Diagonal sliders
			if ((p == (color ? w_bishop : b_bishop)) || (p == (color ? w_queen : b_queen))) {

				for (uint8_t m = 0; m < 4; m++) {
					int mi = diag_directions[m][0];
					int mj = diag_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint8_t p2 = _array[new_i][new_j];

						uint8_t di = abs(new_i - king_pos.row);
						uint8_t dj = abs(new_j - king_pos.col);
						bool front_square = color ? new_i > king_pos.row : new_i < king_pos.row;

						// If the piece controls a square around the king
						if (di <= 2 && dj <= 2) {
							if (front_square && di <= 1 && dj <= 1) {
								defenses++;
							}
							else {
								semi_defenses++;
							}
						}

						// If a pawn blocks the square
						if (p2 == w_pawn || p2 == b_pawn)
							break;

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// King
			if (p == (color ? w_king : b_king)) {
				for (int i = -1; i < 2; i++) {
					for (int j = -1; j < 2; j++) {

						int new_i = i + king_pos.row;
						int new_j = j + king_pos.col;

						if (i == 0 && j == 0)
							continue;

						if (!is_in(new_i, 0, 7) || !is_in(new_j, 0, 7))
							continue;

						uint8_t di = abs(new_i - king_pos.row);
						uint8_t dj = abs(new_j - king_pos.col);
						bool front_square = color ? new_i > king_pos.row : new_i < king_pos.row;

						// If the king controls a square around the king
						if (di <= 2 && dj <= 2) {
							if (front_square && di <= 1 && dj <= 1) {
								defenses++;
							}
							else {
								semi_defenses++;
							}
						}
					}
				}
			}

			if (defenses > 8) {
				cout << "BUG: too many defenses from a single piece... check get_king_defenders()" << endl;
			}
			else {
				if (defenses > 0) {
					king_defenders += defending_value[defenses] * piece_defense_factor[(p - 1) % 6];
					//cout << "color: " << color << ", piece: " << piece_name(p) << "(" << square_name(row, col) << "), defenses : " << (int)defenses << ", value : " << defending_value[defenses] << ", piece factor : " << piece_defense_factor[(p - 1) % 6] << ", total : " << defending_value[defenses] * piece_defense_factor[(p - 1) % 6] << endl;
				}
				else if (semi_defenses > 0) { // TODO: improve by counting the semi-defences?
					king_defenders += semi_defense_value * piece_defense_factor[(p - 1) % 6];
					//cout << "color: " << color << ", piece: " << piece_name(p) << "(" << square_name(row, col) << "), semi-defenses : " << (int)semi_defenses << ", value : " << semi_defense_value * piece_defense_factor[(p - 1) % 6] << endl;
				}
			}
		}
	}

	//cout << "total defenders: " << king_defenders << endl;

	return king_defenders;
}

// Returns the pawn storm bonus against the enemy king on a given file
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
string square_name(uint8_t row, uint8_t col) {
	return string(1, 'a' + col) + string(1, '1' + row);
}

// Returns the name of a piece
string piece_name(uint8_t piece) {
	switch (piece) {
	case w_pawn:
		return "w_pawn";
	case w_knight:
		return "w_knight";
	case w_bishop:
		return "w_bishop";
	case w_rook:
		return "w_rook";
	case w_queen:
		return "w_queen";
	case w_king:
		return "w_king";
	case b_pawn:
		return "b_pawn";
	case b_knight:
		return "b_knight";
	case b_bishop:
		return "b_bishop";
	case b_rook:
		return "b_rook";
	case b_queen:
		return "b_queen";
	case b_king:
		return "b_king";
	default:
		return "none";
	}
}

// Returns the name of a piece
string short_piece_name(uint8_t piece) {
	switch (piece) {
	case w_pawn:
		return "P";
	case w_knight:
		return "N";
	case w_bishop:
		return "B";
	case w_rook:
		return "R";
	case w_queen:
		return "Q";
	case w_king:
		return "K";
	case b_pawn:
		return "p";
	case b_knight:
		return "n";
	case b_bishop:
		return "b";
	case b_rook:
		return "r";
	case b_queen:
		return "q";
	case b_king:
		return "k";
	default:
		return ".";
	}
}

// Returns an activity bonus for the knights
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
				if ((col > 0 && (pawns[row][col - 1] || pawns[row - dir][col - 1] || pawns[row + dir][col - 1])) || (col < 4 && (pawns[row][col + 1] || pawns[row - dir][col + 1] || pawns[row + dir][col + 1]))) {
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
int Board::get_piece_mobility(bool display) const {
	// Points to account for:
	// - number of virtually reachable squares, counting pawns only
	// - the squares actually reachable, given the pieces in the way
	// - blocked pieces: blocked by pieces? by pawns? permanently? are the blocking pawns themselves blocked?
	// - should reachable squares be valued by their location, or does that belong in piece activity?

	// Cas tests:
	//2b1r1k1/qp3ppn/1r1p1n1p/pP1Pp3/P3P1P1/1P1QP1NP/5RB1/5RK1 w - - 1 3: the rook on b6
	//r1bqk1nr/p4ppp/2nbp3/3p4/Pp1P4/1Pp1PN2/2P1BPPP/RNBQK2R w KQkq - 0 10: rook and knight on a1 and b1
	//8/1p4pk/1r1p1n1p/pP1Pp2P/P1R1P3/1P2P3/6K1/8 b - - 2 18: winning for White? Leela says yes, Stockfish 17 at low depth does not
	//2Rnk3/3p2p1/3Pp2r/1b2P1Np/7P/5P1K/6P1/8 b - - 1 43: no moves for Black -> completely lost
	//3k4/1pp5/p1n2p2/2P1p2p/P3p1pP/1P2P1B1/1KP2PP1/8 w - - 0 30: bishop on g3 trapped for good
	//r1bq1knr/pp1p1pp1/1bnP3p/1p2P3/8/P1N2N2/1P3PPP/R1BQ1RK1 b - - 2 12: nearly winning for White
	//rn3rk1/1b1p1ppp/p1pP4/1pP5/P7/8/3B1PPP/1R1K1B1R w - - 0 18: a5 simply wins strategically, the three black queenside pieces are stuck for good
	//r2qk2r/1bp1ppnp/p5p1/3pP3/6P1/2N2P2/PPPB3P/R2Q1RK1 w kq - 1 14: Black is better...

	//display = true;

	// TODO: how should pinned pieces be handled?
	// TODO: for a bishop, 7 versus 8 available moves barely differ any more

	// Weight of virtual mobility, passing "through" pieces but not pawns
	static constexpr int pawn_virtual_mobility[5] = { 0, 0, 0, 0, 0 };
	static constexpr int knight_virtual_mobility[9] = { -1350, -150, 0, 65, 110, 125, 135, 143, 150 };
	static constexpr int bishop_virtual_mobility[15] = { -1350, -500, 0, 65, 100, 125, 138, 145, 150, 155, 160, 165, 170, 175, 180 };
	static constexpr int rook_virtual_mobility[15] = { -1750, -750, 0, 35, 50, 65, 75, 85, 95, 105, 115, 125, 135, 145, 155 };
	static constexpr int queen_virtual_mobility[29] = { -3000, -1500, 0, 65, 100, 125, 138, 145, 150, 155, 160, 165, 170, 175, 180, 185, 190, 195, 200, 205, 210, 215, 220, 225, 230, 235, 240, 245, 250 };
	static constexpr int king_virtual_mobility[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    static const int* virtual_mobilities[6] = { pawn_virtual_mobility, knight_virtual_mobility, bishop_virtual_mobility, rook_virtual_mobility, queen_virtual_mobility, king_virtual_mobility };

	// Weight of real mobility (based on actually legal moves; never passes through a piece)
	static constexpr int pawn_real_mobility[5] = { 0, 0, 0, 0, 0 };
	static constexpr int knight_real_mobility[9] = { -250, -100, -35, 0, 28, 37, 43, 47, 50 };
	static constexpr int bishop_real_mobility[15] = { -350, -180, -85, -10, 15, 20, 25, 30, 35, 37, 40, 42, 45, 47, 50 };
	static constexpr int rook_real_mobility[15] = { -135, -85, -50, 10, 15, 20, 25, 30, 35, 37, 40, 42, 45, 47, 50 };
	static constexpr int queen_real_mobility[29] = { -300, -135, -70, -15, 10, 14, 18, 21, 24, 26, 28, 29, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 };
	static constexpr int king_real_mobility[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };


	static const int* real_mobilities[6] = { pawn_real_mobility, knight_real_mobility, bishop_real_mobility, rook_real_mobility, queen_real_mobility, king_real_mobility };


	// Squares controlled by enemy pawns are not counted
	bool b_pawn_controls[8][8] = { false };
	bool w_pawn_controls[8][8] = { false };

	for (uint8_t row = 1; row < 7; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			if (_array[row][col] == w_pawn) {
				(col > 0) && (w_pawn_controls[row + 1][col - 1] = true);
				(col < 7) && (w_pawn_controls[row + 1][col + 1] = true);
			}
			else if (_array[row][col] == b_pawn) {
				(col > 0) && (b_pawn_controls[row - 1][col - 1] = true);
				(col < 7) && (b_pawn_controls[row - 1][col + 1] = true);
			}
		}
	}


	// White piece mobility
	int white_mobility = 0;

	// Black piece mobility
	int black_mobility = 0;

	// Piece mobility
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t piece = _array[row][col];

			if (piece == none) {
				continue;
			}

			int virtual_mobility = 0;
			int real_mobility = 0;
			int blocking_pawn_moves = 0;

			// White pawn
			if (piece == w_pawn) {

				// Avance de 1
				uint8_t front_piece = _array[row + 1][col];

				if (front_piece == none) {
					virtual_mobility++;
					real_mobility++;

					// Avance de 2
					if (row == 1) {
						uint8_t front_piece2 = _array[row + 2][col];

						if (!is_pawn(front_piece2)) {
							virtual_mobility++;

							if (front_piece2 == none) {
								real_mobility++;
							}
						}
					}
				}
				else if (!is_pawn(front_piece)) {
					virtual_mobility++;

					// Avance de 2
					if (row == 1) {
						virtual_mobility += !is_pawn(_array[row + 2][col]);
					}
				}

				// Captures

				// Capture en diagonale gauche
				if (col > 0) {
					uint8_t left_piece = _array[row + 1][col - 1];

					if (is_black(left_piece)) {
						virtual_mobility++;
						real_mobility++;
					}
				}

				// Capture en diagonale droite
				if (col < 7) {
					uint8_t right_piece = _array[row + 1][col + 1];

					if (is_black(right_piece)) {
						virtual_mobility++;
						real_mobility++;
					}
				}
			}

			// White knight
			if (piece == w_knight) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_i = row + knight_directions[m][0];
					int new_j = col + knight_directions[m][1];

					if (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint8_t p = _array[new_i][new_j];

						if (!b_pawn_controls[new_i][new_j]) {
							if (p != w_pawn && p != w_king) {
								virtual_mobility++;

								if (!is_white(p)) {
									real_mobility++;
								}
							}

							// Friendly pawn blocking the piece
							else {
								// Can the pawn move?
								if (pawn_can_move(new_i, new_j, true)) {
									blocking_pawn_moves++;
								}
							}
						}
					}
				}
			}

			// White straight-line slider
			if (is_rectilinear(piece) && is_white(piece)) {
				for (uint8_t m = 0; m < 4; m++) {
					int mi = rect_directions[m][0];
					int mj = rect_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					bool blocked = false;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint8_t p = _array[new_i][new_j];

						if (!b_pawn_controls[new_i][new_j]) {
							if (p != w_pawn && p != w_king) {
								virtual_mobility++;

								if (!is_white(p) && !blocked) {
									real_mobility++;
								}
							}
							else {
								if (pawn_can_move(new_i, new_j, true)) {
									blocking_pawn_moves++;
								}
							}
						}

						// A pawn blocks the piece
						if (is_pawn(p) || p == w_king) {
							break;
						}

						if (p != none) {
							blocked = true;
						}

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// White diagonal slider
			if (is_diagonal(piece) && is_white(piece)) {
				for (uint8_t m = 0; m < 4; m++) {
					int mi = diag_directions[m][0];
					int mj = diag_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					bool blocked = false;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint8_t p = _array[new_i][new_j];

						if (!b_pawn_controls[new_i][new_j]) {
							if (p != w_pawn && p != w_king) {
								virtual_mobility++;

								if (!is_white(p) && !blocked) {
									real_mobility++;
								}
							}
							else {
								if (pawn_can_move(new_i, new_j, true)) {
									blocking_pawn_moves++;
								}
							}
						}

						// A pawn blocks the piece
						if (is_pawn(p) || p == w_king) {
							break;
						}

						if (p != none) {
							blocked = true;
						}

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// White king
			if (piece == w_king) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_i = row + all_directions[m][0];
					int new_j = col + all_directions[m][1];

					if (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint8_t p = _array[new_i][new_j];

						if (!b_pawn_controls[new_i][new_j]) {
							if (p != w_pawn) {
								virtual_mobility++;

								if (!is_white(p)) {
									real_mobility++;
								}
							}
							else {
								if (pawn_can_move(new_i, new_j, true)) {
									blocking_pawn_moves++;
								}
							}
						}
					}
				}
			}

			// Black pawn
			if (piece == b_pawn) {

				// Avance de 1
				uint8_t front_piece = _array[row - 1][col];

				if (front_piece == none) {
					virtual_mobility++;
					real_mobility++;

					// Avance de 2
					if (row == 6) {
						uint8_t front_piece2 = _array[row - 2][col];

						if (!is_pawn(front_piece2)) {
							virtual_mobility++;

							if (front_piece2 == none) {
								real_mobility++;
							}
						}
					}
				}
				else if (!is_pawn(front_piece)) {
					virtual_mobility++;

					// Avance de 2
					if (row == 6) {
						virtual_mobility += !is_pawn(_array[row - 2][col]);
					}
				}

				// Captures

				// Capture en diagonale gauche
				if (col > 0) {
					uint8_t left_piece = _array[row - 1][col - 1];

					if (is_white(left_piece)) {
						virtual_mobility++;
						real_mobility++;
					}
				}

				// Capture en diagonale droite
				if (col < 7) {
					uint8_t right_piece = _array[row - 1][col + 1];

					if (is_white(right_piece)) {
						virtual_mobility++;
						real_mobility++;
					}
				}
			}

			// Black knight
			if (piece == b_knight) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_i = row + knight_directions[m][0];
					int new_j = col + knight_directions[m][1];

					if (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint8_t p = _array[new_i][new_j];

						if (!w_pawn_controls[new_i][new_j]) {
							if (p != b_pawn && p != b_king) {
								virtual_mobility++;

								if (!is_black(p)) {
									real_mobility++;
								}
							}

							// Friendly pawn blocking the piece
							else {
								// Can the pawn move?
								if (pawn_can_move(new_i, new_j, false)) {
									blocking_pawn_moves++;
								}
							}
						}
					}
				}
			}

			// Black straight-line slider
			if (is_rectilinear(piece) && is_black(piece)) {
				for (uint8_t m = 0; m < 4; m++) {
					int mi = rect_directions[m][0];
					int mj = rect_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					bool blocked = false;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint8_t p = _array[new_i][new_j];

						if (!w_pawn_controls[new_i][new_j]) {
							if (p != b_pawn && p != b_king) {
								virtual_mobility++;

								if (!is_black(p) && !blocked) {
									real_mobility++;
								}
							}
							else {
								if (pawn_can_move(new_i, new_j, false)) {
									blocking_pawn_moves++;
								}
							}
						}

						// A pawn blocks the piece
						if (is_pawn(p) || p == b_king) {
							break;
						}

						if (p != none) {
							blocked = true;
						}

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// Black diagonal slider
			if (is_diagonal(piece) && is_black(piece)) {
				for (uint8_t m = 0; m < 4; m++) {
					int mi = diag_directions[m][0];
					int mj = diag_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					bool blocked = false;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint8_t p = _array[new_i][new_j];

						if (!w_pawn_controls[new_i][new_j]) {
							if (p != b_pawn && p != b_king) {
								virtual_mobility++;

								if (!is_black(p) && !blocked) {
									real_mobility++;
								}
							}
							else {
								if (pawn_can_move(new_i, new_j, false)) {
									blocking_pawn_moves++;
								}
							}
						}

						// A pawn blocks the piece
						if (is_pawn(p) || p == b_king) {
							break;
						}

						if (p != none) {
							blocked = true;
						}

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// Black king
			if (piece == b_king) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_i = row + all_directions[m][0];
					int new_j = col + all_directions[m][1];

					if (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint8_t p = _array[new_i][new_j];

						if (!w_pawn_controls[new_i][new_j]) {
							if (p != b_pawn && p != b_king) {
								virtual_mobility++;

								if (!is_black(p)) {
									real_mobility++;
								}
							}
							else {
								if (pawn_can_move(new_i, new_j, false)) {
									blocking_pawn_moves++;
								}
							}
						}
					}
				}
			}

			if (display)
				cout << piece_name(piece) << ", " << square_name(row, col) << ", virtual-mob : " << virtual_mobility << ", real-mob : " << real_mobility << ", pawn moves : " << blocking_pawn_moves << " | ";

			// Add the mobility
			if (is_white(piece)) {
				int virtual_mobility_value = virtual_mobilities[piece - 1][virtual_mobility];

				if (display)
					cout << virtual_mobility_value;

				if (virtual_mobility_value < 0) {
					virtual_mobility_value /= (1 + blocking_pawn_moves) * (1 + blocking_pawn_moves);

					if (display)
						cout << " / " << (1 + blocking_pawn_moves) * (1 + blocking_pawn_moves) << " = " << virtual_mobility_value;
				}

				if (display)
					cout << " + " << real_mobilities[piece - 1][real_mobility];

				int piece_mobility = virtual_mobility_value + real_mobilities[piece - 1][real_mobility];

				if (display)
					cout << " = " << piece_mobility << endl;

				white_mobility += piece_mobility;
			}
			else {
				int virtual_mobility_value = virtual_mobilities[piece - 7][virtual_mobility];

				if (display)
					cout << virtual_mobility_value;

				if (virtual_mobility_value < 0) {
					virtual_mobility_value /= (1 + blocking_pawn_moves) * (1 + blocking_pawn_moves);

					if (display)
						cout << " / " << (1 + blocking_pawn_moves) * (1 + blocking_pawn_moves) << " = " << virtual_mobility_value;
				}

				if (display)
					cout << " + " << real_mobilities[piece - 7][real_mobility];

				int piece_mobility = virtual_mobility_value + real_mobilities[piece - 7][real_mobility];

				if (display)
					cout << " = " << piece_mobility << endl;

				black_mobility += piece_mobility;
			}
		}
	}

	if (display) {
		cout << "White mobility: " << white_mobility << ", black mobility: " << black_mobility << endl;
	}

	// Multiplier based on game progress
	float piece_mobility_advancement_factor = 1.0f;

	return eval_from_progress(white_mobility - black_mobility, _adv, piece_mobility_advancement_factor);
}

// Tells whether the pawn can move
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
void Board::get_uncertainty(Evaluation* eval, int material_eval, int winning_eval) const {

	// TODO ***
	// Number of checks available in the position?
	// Contre jeu

	// r2r4/8/1p1q2P1/2b5/3k1pP1/pP2pP2/2Q4P/1K6 w - - 9 11: example of a very uncertain position
	// r2r4/4q3/1p4P1/2b5/5pPk/pP2pP2/8/1K6 w - - 0 18 : plus du tout d'incertitude !!
	// rnb2bnr/pppp1k1p/5q2/8/5p2/4BQ2/PPP3PP/RN3RK1 w - - 2 11 : grosse incertitude
	// r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4 : assez peu...
	// r1bq1b1r/ppp3pp/2n1k3/3np3/2B5/5Q2/PPPP1PPP/RNB1K2R w KQ - 2 8 : grosse incertitude
	// 8/8/7P/1p2Kp2/3P4/P2k2P1/P7/8 b - - 0 41 : plus aucune incertitude

	// rnb2bnr/pppp1k1p/5q2/8/5p2/4BQ2/PPP3PP/RN3RK1 w - - 2 11: how does uncertainty jump from 14% to 92% on Bxf4?
	// et avg score 0.7 -> 0.42??


	// TODO: should the search GrogrosZero has already done be taken into account?
	// For instance, when the evaluation swings quickly
	// Depends on how many pieces remain

	// TODO: should the number of available captures be taken into account?

	// FIXME: improvable. When behind both materially and non-materially, it avoids regaining activity and pulling the non-material term back to 0, because that would lower the uncertainty
	//5rk1/1B4p1/7p/3p4/5n2/4n2P/1R4PK/8 b - - 3 36: Black has activity here, hence extra uncertainty, which is wrong

	// rnb3r1/ppp2k1p/1b1p1N2/4P3/1P6/2P3P1/P3PP1P/RN1QK2R w KQ - 2 13: should be nearly 100%

	// 4r1k1/p1p2ppp/1p2bn2/8/1r6/1PN1B3/P3BPPP/R2R2K1 b - - 1 16: it only reaches 50% after c5?

	float raw_incertitude = 0.0f;

	// TODO: a proper formula for the uncertainty is still needed
	int non_material_eval = eval->_value - material_eval;

	if (non_material_eval != 0) {
		int abs_non_material_eval = abs(non_material_eval);

		// How far the non-material evaluation opposes the material one (0 = not at all, 1 = completely)
		//float opposite_material_factor = 1.0f / (1.0f + abs(_evaluation) / (abs_non_material_eval));

		// Note: normalised dot product. 1 = same direction, -1 = opposite direction
		int max_eval = max(abs(material_eval), abs_non_material_eval);
		float colinear_evals = static_cast<float>(material_eval) * non_material_eval / (max_eval * max_eval);

		// Value of the opposite material factor (0 = not at all, 1 = completely)
		float opposite_material_factor = 0.5f - colinear_evals / 2.0f;

		//// Value of the opposite material factor when the material evaluation is zero
		constexpr float thresold = 0.5f;

		// New opposite material factor
		float new_opposite_material_factor = opposite_material_factor - thresold;

		//cout << "old value: " << new_opposite_material_factor << endl;

		// Pull the value towards the bounds (-0.5, 0.5)
		//float rapprochement = 3.0f;
		//const float rapprochement = abs(material_eval) / 100.0f;
		const float rapprochement = abs(non_material_eval) / 100.0f;
		new_opposite_material_factor = new_opposite_material_factor >= 0 ? pow(new_opposite_material_factor * 2, 1 / rapprochement) / 2 : -pow(-new_opposite_material_factor * 2, 1 / rapprochement) / 2;

		new_opposite_material_factor += thresold;

		// TODO: opposed should reinforce itself, and non-opposed likewise

		// Constant giving a baseline uncertainty of 0.5 in the messiest case
		constexpr int half_uncertainty_constant = 50;

		// Normalise this non-material factor to [0, 1] through a non-linear function
		//float norm_non_material_eval = abs_non_material_eval / (static_cast<float>(half_uncertainty_constant) + abs_non_material_eval + abs(material_eval) / 2.0f);
		float norm_non_material_eval = abs_non_material_eval / (static_cast<float>(half_uncertainty_constant) + abs_non_material_eval + abs(eval->_value));
		//float norm_non_material_eval = abs_non_material_eval / (static_cast<float>(half_uncertainty_constant) + abs_non_material_eval);


		// 5rk1/1B4p1/7p/3p4/5n2/4n2P/1R4PK/8 b - - 3 36

		// Combine the two factors
		float new_incertitude = norm_non_material_eval * new_opposite_material_factor;
		//float new_incertitude = norm_non_material_eval * opposite_material_factor;

		// Bring the uncertainty back into [0, 1]
		//raw_incertitude = new_incertitude + thresold;
		raw_incertitude = new_incertitude;

		//cout << "eval: " << _evaluation << ", material eval: " << material_eval << ", non-material eval: " << non_material_eval << ", opposite material factor: " << opposite_material_factor << ", new opposite material factor: " << new_opposite_material_factor << ", norm non-material eval: " << norm_non_material_eval << ", raw incertitude: " << raw_incertitude << endl;
	}

	// Weight of the non-material uncertainty
	//float non_material_factor = 0.75f;


	// Uncertainty per piece type
	constexpr int piece_uncertitudes[6] = { 1, 5, 7, 10, 50, 0 };

	// Incertitude max (environ... si y'a plusieurs dames?)
	//constexpr int max_piece_uncertainty = 204;
	constexpr int max_piece_uncertainty_per_side = 102;

	// Uncertainty per piece
	//int total_piece_uncertitude = 0;
	int white_piece_uncertainty = 0;
	int black_piece_uncertainty = 0;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t piece = _array[row][col];
			if (piece == none) {
				continue;
			}

			//total_piece_uncertitude += piece_uncertitudes[(piece - 1) % 6];

			if (is_white(piece)) {
				white_piece_uncertainty += piece_uncertitudes[(piece - 1) % 6];
			}
			else {
				black_piece_uncertainty += piece_uncertitudes[(piece - 1) % 6];
			}
		}
	}

	// Uncertainty as a function of the piece count
	//float piece_uncertainty = total_piece_uncertitude / static_cast<float>(max_piece_uncertainty);
	float piece_uncertainty = eval->_value > 0 ? black_piece_uncertainty / static_cast<float>(max_piece_uncertainty_per_side) : white_piece_uncertainty / static_cast<float>(max_piece_uncertainty_per_side);

	// Weight of this uncertainty
	int eval_val = abs(eval->_value);
	//float piece_uncertainty_factor = 0.25f;
	//float piece_uncertainty_factor = 0.5 * raw_incertitude;
	float piece_uncertainty_factor = 0.65f / (1 + static_cast<float>(eval_val) / winning_eval);


	// Other factors to account for:
	// - number of pieces left (raises uncertainty)
	// - complexity of the position (raises uncertainty)
	// - material imbalance (raises uncertainty)
	// - symmetry of the position (lowers uncertainty)

	// Account for the game progress

	float alpha = 1 + abs(eval->_value) / static_cast<float>(winning_eval) / 2.0f;
	float new_raw_incertitude = pow(raw_incertitude, alpha);

	//float value = raw_incertitude * piece_uncertainty;
	//float value = raw_incertitude * non_material_factor + piece_uncertainty * piece_uncertainty_factor;
	float value = new_raw_incertitude * (1 - piece_uncertainty_factor) + piece_uncertainty * piece_uncertainty_factor;
	
	//float alpha = 1 + abs(_evaluation) / static_cast<float>(winning_eval) / 10.0f;
	//float relative_uncertainty = pow(value, alpha);


	eval->_uncertainty = value;
}

// Stores the WDL of the position (for White)
void Evaluation::get_WDL(int winning_eval, float beta) {
	
	// r2r4/8/1p1q2P1/2b5/3k1pP1/pP2pP2/2Q4P/1K6 w - - 9 11: down the drawing lines this should read 0, 1000, 0, not 333, 333, 333

	// Winning eval: the evaluation at which the winning and drawing chances are equal, at zero uncertainty.

	// beta controls how slowly it converges.
	// For beta = 0.25, f(2 * winning_eval) = 0.84
	// for beta = 0.5,  f(2 * winning_eval) = 0.707
	// for beta = 0.75,  f(2 * winning_eval) = 0.62

	// TEST
	//const float up_beta = 0.35f;
	//const float down_beta = 1.0f;

	// rnbqkb1r/ppp1pp1p/5p2/3p4/3P4/8/PPP2PPP/RNBQKBNR b KQkq - 0 4: close to a 100% win rate at maximum confidence

	// Winning eval = the evaluation at which the certain winning chance equals the certain drawing chance (0.5)

	bool is_eval_positive = _value > 0;
	float eval = abs(_value);

	//cout << "eval: " << eval << ", winning_eval: " << winning_eval << ", beta: " << beta << endl;

	constexpr float beta_up = 2.5f;
	constexpr float beta_down = 1.5f;

	// TEST
	const float white_winning_eval = _winnable_white == 0.0f ? FLT_MAX : winning_eval / _winnable_white;
	const float black_winning_eval = _winnable_black == 0.0f ? FLT_MAX : winning_eval / _winnable_black;


	const float base_win_chance_factor = is_eval_positive ? eval / white_winning_eval : eval / black_winning_eval;
	//const float base_win_chance_factor = eval / winning_eval;
	const float win_chance_factor = base_win_chance_factor > 1.0f ? pow(base_win_chance_factor, beta_up) : pow(base_win_chance_factor, beta_down);
	const float base_win_chance = 1.0f - 1.0f / (1.0f + win_chance_factor);

	//const float base_win_chance = (eval / (eval + winning_eval));

	// Flatten it slightly when completely winning
	const float threshold_win_chance = base_win_chance - 0.5f;
	//const float updated_threshold = threshold_win_chance > 0 ? pow(abs(threshold_win_chance) * 2, up_beta) / 2 : -pow(abs(threshold_win_chance) * 2, down_beta) / 2;
	const float updated_threshold = threshold_win_chance;

	const float certain_win_chance = updated_threshold + 0.5f;

	//float certain_win_chance = pow((eval / (eval + winning_eval)), beta);
	const float certain_draw_chance = 1 - certain_win_chance;

	//cout << "certain win chance: " << certain_win_chance << ", certain draw chance: " << certain_draw_chance << endl;

	const float white_win_chance = (is_eval_positive ? certain_win_chance : 0.0f);
	const float white_lose_chance = (is_eval_positive ? 0.0f : certain_win_chance);

	// Non-linear function of the uncertainty
	//float alpha = 1 + eval / winning_eval / 25.0f;
	constexpr float alpha = 1.0f;
	const float relative_uncertainty = pow(_uncertainty, alpha);

	//cout << "relative uncertainty: " << relative_uncertainty << endl;

	//const float win_chance = white_win_chance * (1 - relative_uncertainty) + relative_uncertainty / 3.0f;
	//const float lose_chance = white_lose_chance * (1 - relative_uncertainty) + relative_uncertainty / 3.0f;
	//const float draw_chance = certain_draw_chance * (1 - relative_uncertainty) + relative_uncertainty / 3.0f;

	const float win_chance = white_win_chance * (1 - relative_uncertainty) + relative_uncertainty * _winnable_white / 3.0f;
	const float lose_chance = white_lose_chance * (1 - relative_uncertainty) + relative_uncertainty * _winnable_black / 3.0f;
	const float draw_chance = 1 - win_chance - lose_chance;

	//7R/8/1p2kp2/pKr5/P7/1P3P2/5P2/8 w - - 0 49

	// Evaluation of the real winning chances
	//const float 
	// _white = get_winnable(true);
	//const float winnable_black = get_winnable(false);

	// FIXME: this may need to be applied before the uncertainty
	//const float total_win_chance = win_chance * _winnable_white;
	//const float total_lose_chance = lose_chance * _winnable_black;
	//const float total_draw_chance = 1 - total_win_chance - total_lose_chance;

	//cout << "win chance: " << win_chance << ", draw chance: " << draw_chance << ", lose chance: " << lose_chance << endl;

	//_uncertainty = uncertainty;
	_wdl = WDL(win_chance, draw_chance, lose_chance);
	//_wdl = WDL(total_win_chance, total_draw_chance, total_lose_chance);
}

// Returns the expected score of the position, in points, from the WDL probabilities
void Evaluation::get_average_score(float draw_score) {
	// TODO: pick the move from this score rather than from the evaluation?

	_avg_score = _wdl.win_chance + draw_score * _wdl.draw_chance;
}

// Returns the evaluation renormalised against the average score
string get_renormalized_evaluation(float avg_score, float winning_eval, float winning_score) {

	// avg_score = 0.5 -> eval = 0
	// avg_score = 1.0 -> eval = +inf
	// avg_score = 0.0 -> eval = -inf
	// avg_score = 0.67 -> eval = winning_eval
	// avg_score = 0.33 -> eval = -winning_eval

	if (avg_score == 1.0f) {
		return "+Inf";
	}
	if (avg_score == 0.0f) {
		return "-Inf";
	}

	// Function symmetric about 0.5
	const float winning_score_diff = 1.0f - winning_score;
	const float score_diff = min(avg_score, 1.0f - avg_score);

	float eval = winning_eval * (avg_score - 0.5f) / (winning_score - 0.5f) * pow(winning_score_diff / score_diff, 0.5f);

	stringstream stream;
	stream << fixed << setprecision(1) << eval;
	return eval > 0 ? "+" + stream.str() : stream.str();
}

// Returns the score of a WDL triple, to a precision of 0.01
string score_string(float avg_score) {
	//float score = get_average_score(wdl, draw_score);
	//stringstream stream;
	//stream << fixed << setprecision(3) << avg_score;
	//return stream.str();

	char buffer[32];
	int len = snprintf(buffer, sizeof(buffer), "%.3f", avg_score);
	return std::string(buffer, len);
}

// Swaps the colours of the two sides, side to move and castling rights included
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
	transposition_table.clear();
	node_map.clear(); // #11 Plan B - purge the DAG along with the TT (no dangling pointer between searches)
	main_GUI._root_exploration_node->reset();
}

// Iterates over the distance map from a given position and returns the newly controlled squares
vector<Pos> Board::get_next_king_squares(SquareMap& map, Pos start_pos, int distance, bool color) const {

	// Initialise the list of newly controlled squares
	vector<Pos> new_controlled_squares;

	// Look at the 8 possible directions
	for (uint8_t m = 0; m < 8; m++) {

		// Nouvelle position
		int new_i = start_pos.row + all_directions[m][0];
		int new_j = start_pos.col + all_directions[m][1];

		// If the square is on the board
		if (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
			
			// If this is a newly explored square
			if (map._array[new_i][new_j] == 0) {
				map._array[new_i][new_j] = distance + 1;
				new_controlled_squares.push_back(Pos(new_i, new_j));
			}

			// If the square is controlled by the opponent
			else if (map._array[new_i][new_j] == -64) {
				map._array[new_i][new_j] = -distance - 1;
			}
		}
	}

	return new_controlled_squares;
}

// Returns a map of the distances from the king to every square, as the number of moves needed given the current controls
SquareMap Board::get_king_squares_distance(bool color) {
	// TODO: tedious, but probably very strong

	//8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7: the black king can reach neither a4, d5 nor d4, short of going all the way round

	// 8/8/3k2b1/1p5p/1P1K2p1/1B4P1/7P/8 w - - 6 12
	// White king: distance 3 to h5, via e3 f4 g5
	// Cannot reach b5 as things stand

	// 8/8/6K1/1p2k2p/1P4p1/6P1/7P/8 b - - 0 19

	// 8/5b2/8/1p1k1BKp/1P4p1/6P1/7P/8 b - - 17 17: the white king is closer


	// Enemy control map
	SquareMap control_map = color ? get_black_controls_map() : get_white_controls_map();

	// Map initialisation

	// -64 = unreachable square (friendly piece, or controlled by the opponent)
	// k = square at distance k from the king
	// -k = square controlled by the opponent, or a friendly piece, at distance k
	SquareMap distance_map;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {

			uint8_t piece = _array[row][col];

			// The king is assumed unable to pass through:
			// a square controlled by the opponent
			// a friendly piece
			// an enemy non-pawn piece, on the assumption that it can step back and keep control of the square (TEST)
			if (control_map._array[row][col] || !(piece == none || (is_pawn(piece) && is_white(piece) != color))) {
				distance_map._array[row][col] = -64;
			}
		}
	}

	// Supprime la map
	control_map.~SquareMap();

	// King position
	update_kings_pos();
	Pos king_pos = color ? _white_king_pos : _black_king_pos;

	// Build the distance map iteratively
	vector<Pos> current_controlled_squares = { king_pos };
	int distance = 0;

	// FIXME: something other than vectors may be needed, this is very slow

	while (!current_controlled_squares.empty()) {
		vector<Pos> new_controlled_squares;
		for (const Pos& pos : current_controlled_squares) {
			vector<Pos> current_new_controlled_squares = get_next_king_squares(distance_map, pos, distance, color);
			new_controlled_squares.insert(new_controlled_squares.end(), current_new_controlled_squares.begin(), current_new_controlled_squares.end());
		}
		current_controlled_squares = new_controlled_squares;
		distance++;
	}

	// Reset the -64 entries to 0
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			if (distance_map._array[row][col] == -64) {
				distance_map._array[row][col] = 0;
			}
		}
	}

	// King square
	distance_map._array[king_pos.row][king_pos.col] = 0;

	return distance_map;
}


// Returns the weakness along the king's ranks
int Board::get_king_row_weakness(bool color) {
	update_kings_pos();

	return 0;
}

// Returns the king centralisation value in the endgame
int Board::get_king_centralization(bool color) {

	update_kings_pos();

	// King position
	Pos king_pos = color ? _white_king_pos : _black_king_pos;

	// Distance from the king to the centre
	int row_distance = min(abs(king_pos.row - 3), abs(king_pos.row - 4));
	int col_distance = min(abs(king_pos.col - 3), abs(king_pos.col - 4));

	int distance = row_distance * row_distance + col_distance * col_distance;

	// The value grows with the number of pawns left
	int pawns_count = 0;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t piece = _array[row][col];
			if (is_pawn(piece)) {
				pawns_count++;
			}
		}
	}

	float pawns_factor = (2.0f + pawns_count) / 10.0f;

	// Progress at which this starts to matter
	constexpr float begin_adv = 0.65f;

	// Penalty based on the distance
	return (10 - distance) * pawns_factor * max(0.0f, (_adv - begin_adv) / (1 - begin_adv));
}

// Returns the value of the undefended pieces
int Board::get_unprotected_pieces(bool color) const {
	// TODO
	return 0;
}

// Tells whether the king is inside the square of the pawn
bool Board::in_king_square(Pos pos, bool king_color) {

	// FIXME: this does not cover everything; the king may fail to reach the pawn for other reasons, controlled squares among them

	// King position
	update_kings_pos();
	Pos king_pos = king_color ? _white_king_pos : _black_king_pos;

	// TODO: account for the +1 at the boundaries when the king is to move

	// color: colour of the king, which differs from the pawn's
	bool pawn_color = !king_color;

	// Distance to promotion
	int distance = pawn_color ? 7 - pos.row + !_player : pos.row + _player;

	//cout << "distance: " << distance << endl;

	// Promotion square
	Pos promotion_pos = Pos(pawn_color ? 7 : 0, pos.col);

	//cout << "king pos: " << king_pos.row << " " << king_pos.col << endl;
	//cout << "promotion pos: " << promotion_pos.row << " " << promotion_pos.col << endl;

	// King distance to the promotion square
	int row_distance = abs(king_pos.row - promotion_pos.row);
	int col_distance = abs(king_pos.col - promotion_pos.col);

	//cout << "row distance: " << row_distance << ", col distance: " << col_distance << endl;

	// If the king is inside the square
	return row_distance <= distance && col_distance <= distance;
}

// Returns whether this is a pawn endgame
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
int Board::get_long_term_king_weakness(bool player, int current_weakness, int kingside_weakness, int queenside_weakness) {

	// Update the king positions
	update_kings_pos();

	// King position
	Pos king_pos = player ? _white_king_pos : _black_king_pos;

	// Castling rights
	bool can_kingside_castle = player ? _castling_rights.k_w : _castling_rights.k_b;
	bool can_queenside_castle = player ? _castling_rights.q_w : _castling_rights.q_b;

	uint8_t king_row = player ? 0 : 7;
	uint8_t above_row = player ? 1 : 6;
	uint8_t blocking_bishop = player ? w_bishop : b_bishop;

	// TODO: dedicated functions for the distances to castling

	// Distance added when the opponent controls the square
	constexpr int control_distance_add = 3;

	// Distance to kingside castling
	uint8_t kingside_castle_distance = 0;

	if (can_kingside_castle) {
		kingside_castle_distance = (_array[king_row][5] != none) + (_array[king_row][6] != none)
			+ (_array[king_row][5] == blocking_bishop && _array[above_row][4] != none && _array[above_row][6] != none)
			+ is_controlled(king_row, 5, player) * control_distance_add + is_controlled(king_row, 6, player) * control_distance_add;
	}

	// r1bqk2r/pppp1ppp/1bn2n2/8/4P3/1N3P2/PPP3PP/RNBQKB1R w KQkq - 3 7: kingside castling is controlled here -> much larger distance

	// Distance to queenside castling
	uint8_t queenside_castle_distance = 0;

	if (can_queenside_castle) {
		queenside_castle_distance = (_array[king_row][3] != none) + (_array[king_row][2] != none) + (_array[king_row][1] != none)
			+ (_array[king_row][2] == blocking_bishop && _array[above_row][1] != none && _array[above_row][3] != none)
			+ is_controlled(king_row, 2, player) * control_distance_add + is_controlled(king_row, 3, player) * control_distance_add;
	}

	//cout << "kingside: " << (int)kingside_castle_distance << ", queenside: " << (int)queenside_castle_distance << endl;
	//cout << "castling distance factor, kingside: " << max(0.0, 1.0 - (1.0 + kingside_castle_distance) / 7.0) << ", queenside: " << max(0.0, 1.0 - (1.0 + queenside_castle_distance) / 7.0) << endl;

	// TODO: could be improved when pieces other than the original bishop block the castle

	// TOTAL: main / C + K / (Dk + c) + Q / (Dq + c)

	int total_weakness = current_weakness;

	// Potential gain from castling
	const int kingside_castling_bonus = max(0, current_weakness - kingside_weakness);
	const int queenside_castling_bonus = max(0, current_weakness - queenside_weakness);

	//3qkb1r/p1pbnppp/2p5/4N3/Q7/2N5/Pr3PPP/3RR1K1 b k - 1 14: distance to castling is at least 3

	// FIXME: fairly arbitrary. 0.9 when castling is available, decaying linearly to 0 otherwise, assuming the distance never exceeds 8
	// Plenty of room to improve this
	double kingside_castling_factor = can_kingside_castle ? max(0.0, 1.0 - (1.5 + kingside_castle_distance) / 6.0) : 0.0;
	double queenside_castling_factor = can_queenside_castle ? max(0.0, 1.0 - (1.5 + queenside_castle_distance) / 6.0) : 0.0;

	int total_kingside_bonus = kingside_castling_bonus * kingside_castling_factor;
	int total_queenside_bonus = queenside_castling_bonus * queenside_castling_factor;

	int best_castle_bonus = max(total_kingside_bonus, total_queenside_bonus);

	total_weakness -= best_castle_bonus;

	//cout << "current weakness: " << current_weakness << ", kingside weakness: " << kingside_weakness << ", queenside weakness: " << queenside_weakness << endl;
	//cout << "kingside distance: " << (int)kingside_castle_distance << ", queenside distance: " << (int)queenside_castle_distance << endl;
	//cout << "kingside bonus: " << total_kingside_bonus << ", queenside bonus: " << total_queenside_bonus << endl;
	//cout << "kingside factor: " << kingside_castling_factor << ", queenside factor: " << queenside_castling_factor << endl;
	//cout << "best castle bonus: " << best_castle_bonus << endl;
	//cout << "total weakness: " << total_weakness << endl;

	return total_weakness;
}

// Returns the bonus value for open and semi-open files bearing on the enemy king, were it on a given file
int Board::get_open_files_on_opponent_king_at_column(bool player, int king_col) const {

	// Bonus for the open and semi-open files
	constexpr int open_file_bonus = 25;
	constexpr int semi_open_file_bonus = 15;

	// Factor based on proximity to the enemy king's file
	// If the king is on the file, the bonus is maximal
	constexpr float king_file_bonus = 1.0f;

	// On an adjacent file, the bonus is reduced
	constexpr float king_adjacent_file_bonus = 0.65f;

	// Extra bonus for the pieces standing on it (rooks, queen)
	constexpr int rook_open_bonus = 35;
	constexpr int queen_open_bonus = 35;

	constexpr int rook_semi_open_bonus = 25;
	constexpr int queen_semi_open_bonus = 20;

	constexpr int opponent_guarding_malus = 20;

	// Bonus for the player
	int total_bonus = 0;

	// Friendly pawn
	const int player_pawn = player ? w_pawn : b_pawn;

	// Enemy pawn
	const int opponent_pawn = player ? b_pawn : w_pawn;

	//r1r1b1k1/pp3p1p/1q2p1nQ/3pP1N1/3n2P1/2N5/PP2BPK1/1R5R w - - 2 24

	// For each file adjacent to the black king
	for (uint8_t col = king_col - 1; col < king_col + 2; col++) {

		// If the file is off the board
		if (col < 0 || col > 7)
			continue;

		// Nature of the file
		bool semi_open = true;
		bool open = true;

		for (uint8_t row = 0; row < 8; row++) {
			uint8_t p = _array[row][col];

			if (p == player_pawn) {
				semi_open = false;
				open = false;
				break;
			}
			else if (p == opponent_pawn) {
				open = false;
			}
		}

		// Bonus
		int bonus = (open ? open_file_bonus : (semi_open ? semi_open_file_bonus : 0));

		// Bonus for the pieces standing on the file
		if (open || semi_open) {
			for (uint8_t row = 0; row < 8; row++) {
				uint8_t p = _array[row][col];

				if (p == (player ? w_rook : b_rook))
					bonus += open ? rook_open_bonus : rook_semi_open_bonus;
				else if (p == (player ? w_queen : b_queen))
					bonus += open ? queen_open_bonus : queen_semi_open_bonus;
				else if (is_rectilinear(p))
					bonus -= opponent_guarding_malus;
			}

			//cout << "col: " << (int)col << ", open: " << open << ", semi_open: " << semi_open << ", bonus: " << bonus << endl;
		}

		// 4k1r1/2pp4/1p2pq2/6r1/p2P3p/2PB1b1P/PPQ3P1/R4RK1 w - - 2 24
		// 4k2r/2pp3B/1p2pq2/6r1/p2P4/2P2b1P/PPQ2Rp1/4R1K1 b - - 3 27
		// r2q1rk1/pb2bppp/1pn1p3/2p4n/4P3/2NBBN2/PPP1QPPP/2KR3R b - - 11 11

		// Bonus based on proximity to the king
		bonus *= (col == king_col ? king_file_bonus : king_adjacent_file_bonus);

		total_bonus += bonus;
	}

	// Depending on how far the game has progressed
	constexpr float advancement_factor = 0.0f;

	return eval_from_progress(max(0, total_bonus), _adv, advancement_factor);
}

// Returns the king placement bonus, were it on a given file
int Board::get_king_placement_weakness_at_column(bool player, Pos king_pos) const {

	// Tuning
	constexpr float edge_adv = 0.7f;
	constexpr float mult_endgame = 0.25f;

	// Additive version, suited to the endgame
	constexpr int edge_defense = 50;

	const int col_dist = min(king_pos.col, 7 - king_pos.col);
	const int row_dist = min(king_pos.row, 7 - king_pos.row);

	const int center_col_dist = min(abs(king_pos.col - 3), abs(king_pos.col - 4));
	const int center_row_dist = min(abs(king_pos.row - 3), abs(king_pos.row - 4));

	const double base_factor = edge_defense * (edge_adv - _adv);

	const double row_malus = 0.5f;
	const double row_factor = row_malus * (player ? (king_pos.row * king_pos.row) : ((7 - king_pos.row) * (7 - king_pos.row)));

	const double placement_weakness = base_factor * ((_adv < edge_adv) ? (max(0.0, col_dist - 1.35) + row_factor / 2.0f) : (mult_endgame / (edge_adv - 1.0f) * (center_col_dist * center_col_dist + center_row_dist * center_row_dist)));

	//cout << "king pos: " << (int)king_pos.row << " " << (int)king_pos.col << ", row dist: " << row_dist << ", col dist: " << col_dist << ", row factor: " << row_factor << ", placement weakness: " << placement_weakness << endl;

	return placement_weakness;
}


// Returns the king placement bonus
int Board::get_king_placement_weakness(bool player) {

	// King position
	update_kings_pos();
	Pos king_pos = player ? _white_king_pos : _black_king_pos;

	// Castling rights
	bool can_kingside_castle = player ? _castling_rights.k_w : _castling_rights.k_b;
	bool can_queenside_castle = player ? _castling_rights.q_w : _castling_rights.q_b;
	
	int current_weakness = get_king_placement_weakness_at_column(player, king_pos);
	int kingside_weakness = can_kingside_castle ? get_king_placement_weakness_at_column(player, { player ? 0 : 7, 6 }) : 0;
	int queenside_weakness = can_queenside_castle ? get_king_placement_weakness_at_column(player, { player ? 0 : 7, 2 }) : 0;

	return get_long_term_king_weakness(player, current_weakness, kingside_weakness, queenside_weakness);
	//return current_weakness;
}

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
int Board::get_short_term_piece_mobility(bool display) const {

	// 3n1krr/1p1bq2p/1Pp1p1pP/2Pp1pP1/3P1P1N/3BP3/Q4K1R/R7 w - - 9 8: Black's mobility really is terrible here
	
	// Weight of real mobility (based on actually legal moves; never passes through a piece)
	static constexpr int pawn_real_mobility[5] = { -10, 0, 0, 0, 0 };
	static constexpr int knight_real_mobility[9] = { -250, -100, -35, 0, 28, 37, 43, 47, 50 };
	static constexpr int bishop_real_mobility[15] = { -300, -120, -50, -10, 15, 20, 25, 30, 35, 37, 40, 42, 45, 47, 50 };
	static constexpr int rook_real_mobility[15] = { -230, -95, -50, 10, 15, 20, 25, 30, 35, 37, 40, 42, 45, 47, 50 };
	static constexpr int queen_real_mobility[29] = { -300, -135, -70, -15, 10, 14, 18, 21, 24, 26, 28, 29, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 };
	static constexpr int king_real_mobility[9] = { -100, -40, 0, 5, 10, 12, 13, 14, 15 };

	static const int* real_mobilities[6] = { pawn_real_mobility, knight_real_mobility, bishop_real_mobility, rook_real_mobility, queen_real_mobility, king_real_mobility };

	// Pawn control map
	SquareMap white_pawns_controls = get_pawns_controls(true);
	SquareMap black_pawns_controls = get_pawns_controls(false);

	// Piece mobility
	int white_mobility = 0;
	int black_mobility = 0;

	// For each piece
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t piece = _array[row][col];

			if (piece == none) {
				continue;
			}

			int piece_mobility = 0;

			// White pawn
			if (piece == w_pawn) {
				piece_mobility = _array[row + 1][col] == none; // Single push
				piece_mobility += col > 0 && is_black(_array[row + 1][col - 1]); // Capture to the left
				piece_mobility += col < 7 && is_black(_array[row + 1][col + 1]); // Capture to the right
				piece_mobility += row == 1 && _array[row + 1][col] == none && _array[row + 2][col] == none; // Double push
			}

			// White knight
			if (piece == w_knight) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_row = row + knight_directions[m][0];
					int new_col = col + knight_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !is_white(_array[new_row][new_col]) && !black_pawns_controls._array[new_row][new_col]) {
						piece_mobility++;
					}
				}
			}

			// White straight-line slider
			if (is_rectilinear(piece) && is_white(piece)) {
				for (uint8_t m = 0; m < 4; m++) {
					int d_row = rect_directions[m][0];
					int d_col = rect_directions[m][1];

					int new_row = row + d_row;
					int new_col = col + d_col;

					while (is_in(new_row, 0, 7) && is_in(new_col, 0, 7)) {
						uint8_t p = _array[new_row][new_col];
						
						if (!is_white(p) && !black_pawns_controls._array[new_row][new_col]) {
							piece_mobility++;
						}

						if (p != none) {
							break;
						}

						new_row += d_row;
						new_col += d_col;
					}
				}
			}

			// White diagonal slider
			if (is_diagonal(piece) && is_white(piece)) {
				for (uint8_t m = 0; m < 4; m++) {
					int d_row = diag_directions[m][0];
					int d_col = diag_directions[m][1];

					int new_row = row + d_row;
					int new_col = col + d_col;

					while (is_in(new_row, 0, 7) && is_in(new_col, 0, 7)) {
						uint8_t p = _array[new_row][new_col];

						if (!is_white(p) && !black_pawns_controls._array[new_row][new_col]) {
							piece_mobility++;
						}

						if (p != none) {
							break;
						}

						new_row += d_row;
						new_col += d_col;
					}
				}
			}

			// White king
			if (piece == w_king) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_row = row + all_directions[m][0];
					int new_col = col + all_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !is_white(_array[new_row][new_col]) && !black_pawns_controls._array[new_row][new_col]) {
						piece_mobility++;
					}
				}
			}

			// Black pawn
			if (piece == b_pawn) {
				piece_mobility = _array[row - 1][col] == none; // Single push
				piece_mobility += col > 0 && is_white(_array[row - 1][col - 1]); // Capture to the left
				piece_mobility += col < 7 && is_white(_array[row - 1][col + 1]); // Capture to the right
				piece_mobility += row == 6 && _array[row - 1][col] == none && _array[row - 2][col] == none; // Double push
			}

			// Black knight
			if (piece == b_knight) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_row = row + knight_directions[m][0];
					int new_col = col + knight_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !is_black(_array[new_row][new_col]) && !white_pawns_controls._array[new_row][new_col]) {
						piece_mobility++;
					}
				}
			}

			// Black straight-line slider
			if (is_rectilinear(piece) && is_black(piece)) {
				for (uint8_t m = 0; m < 4; m++) {
					int d_row = rect_directions[m][0];
					int d_col = rect_directions[m][1];

					int new_row = row + d_row;
					int new_col = col + d_col;

					while (is_in(new_row, 0, 7) && is_in(new_col, 0, 7)) {
						uint8_t p = _array[new_row][new_col];

						if (!is_black(p) && !white_pawns_controls._array[new_row][new_col]) {
							piece_mobility++;
						}

						if (p != none) {
							break;
						}

						new_row += d_row;
						new_col += d_col;
					}
				}
			}

			// Black diagonal slider
			if (is_diagonal(piece) && is_black(piece)) {
				for (uint8_t m = 0; m < 4; m++) {
					int d_row = diag_directions[m][0];
					int d_col = diag_directions[m][1];

					int new_row = row + d_row;
					int new_col = col + d_col;

					while (is_in(new_row, 0, 7) && is_in(new_col, 0, 7)) {
						uint8_t p = _array[new_row][new_col];

						if (!is_black(p) && !white_pawns_controls._array[new_row][new_col]) {
							piece_mobility++;
						}

						if (p != none) {
							break;
						}

						new_row += d_row;
						new_col += d_col;
					}
				}
			}

			// Black king
			if (piece == b_king) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_row = row + all_directions[m][0];
					int new_col = col + all_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !is_black(_array[new_row][new_col]) && !white_pawns_controls._array[new_row][new_col]) {
						piece_mobility++;
					}
				}
			}

			if (display) {
				cout << piece_name(piece) << " on " << square_name(row, col) << ", mobility: " << piece_mobility << endl;
			}

			// Add the piece mobility
			if (is_white(piece)) {
				white_mobility += real_mobilities[piece - 1][piece_mobility];
			}
			else if (is_black(piece)) {
				black_mobility += real_mobilities[piece - 7][piece_mobility];
			}
		}
	}

	if (display) {
		cout << "white mobility: " << white_mobility << ", black mobility: " << black_mobility << endl;
	}

	// Depending on how far the game has progressed
	float advancement_factor = 1.0f;

	return eval_from_progress(white_mobility - black_mobility, _adv, advancement_factor);
}

// Returns the virtual piece mobility, long term
int Board::get_long_term_piece_mobility(bool display) const {

	// TEST: b1N3kr/7p/6pB/4p3/8/8/PP3P1P/4K3 w - - 0 32, after Nd6 the king and rook are blocked
	// 2r2rk1/1pqbb3/p3pp2/3pP1p1/7p/P1N1Q2P/1PP2PP1/3R1KBR w - - 0 24: dreadful activity for White
	// r1bqkbnr/pp3pp1/4p2p/3pP3/8/2N5/PPP2PPP/R1BQKB1R w KQkq - 0 8: not that bad for Black
	// r1bq1rk1/pp3ppp/2n2n2/1Bb1p3/8/2N2N2/PPPP1PPP/R1BQ1RK1 w - - 6 8
	// r1bqkb1r/pppp1ppp/2n2n2/8/4P3/4QP2/PPP3PP/RNB1KBNR b KQkq - 0 5: slightly better for Black
	//display = true;
	// Weight of virtual mobility, through every unblocked piece
	static constexpr int pawn_virtual_mobility[5] = { 0, 0, 0, 0, 0 }; // 2 de base
	static constexpr int knight_virtual_mobility[9] = { -1350, -300, -80, 70, 110, 125, 135, 143, 150 }; // 2.35
	static constexpr int bishop_virtual_mobility[15] = { -1350, -450, 0, 65, 100, 125, 138, 145, 150, 155, 160, 165, 170, 175, 180 }; // 2.1
	static constexpr int rook_virtual_mobility[15] = { -2350, -750, 0, 20, 50, 65, 75, 85, 95, 105, 115, 125, 135, 145, 155 }; // 2.7
	static constexpr int queen_virtual_mobility[29] = { -4500, -2000, -1100, -500, -250, -50, 65, 100, 130, 145, 158, 165, 170, 175, 180, 185, 190, 195, 200, 205, 210, 215, 220, 225, 230, 235, 240, 245, 250 }; // 5.3
	static constexpr int king_virtual_mobility[9] = { -1000, -300, 0, 25, 45, 50, 55, 60, 65 }; // 2.35

	static const int* virtual_mobilities[6] = { pawn_virtual_mobility, knight_virtual_mobility, bishop_virtual_mobility, rook_virtual_mobility, queen_virtual_mobility, king_virtual_mobility };

	// Mobility counted after meeting a piece
	static constexpr float blocking_piece_mult[7] = { 1.0f, 0.4f, 0.65f, 0.55f, 0.45f, 0.35f, 0.1f }; // By the type of piece met (none, pawn, knight, bishop, rook, queen, king)

	// Piece control map, for the king
	SquareMap white_pieces_controls = get_white_controls_map();
	SquareMap black_pieces_controls = get_black_controls_map();

	// Blocked piece map
	SquareMap white_blocked_pieces = get_all_blocked_pieces(true, black_pieces_controls);
	SquareMap black_blocked_pieces = get_all_blocked_pieces(false, white_pieces_controls);

	// Pawn control map
	SquareMap white_pawns_controls = get_pawns_controls(true);
	SquareMap black_pawns_controls = get_pawns_controls(false);

	// Piece mobility
	int white_mobility = 0;
	int black_mobility = 0;

	// For each piece
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t piece = _array[row][col];

			if (piece == none) {
				continue;
			}

			float piece_mobility = 0.0f;

			// White pawn
			if (piece == w_pawn) {
				piece_mobility = white_blocked_pieces._array[row + 1][col] == 0; // Single push
				piece_mobility += col > 0 && is_black(_array[row + 1][col - 1]); // Capture to the left
				piece_mobility += col < 7 && is_black(_array[row + 1][col + 1]); // Capture to the right
				piece_mobility += row == 1 && white_blocked_pieces._array[row + 1][col] == 0 && white_blocked_pieces._array[row + 2][col] == 0; // Double push
			}

			// White knight
			if (piece == w_knight) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_row = row + knight_directions[m][0];
					int new_col = col + knight_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && white_blocked_pieces._array[new_row][new_col] == 0 && !black_pawns_controls._array[new_row][new_col]) {
						uint8_t target_piece = _array[new_row][new_col];
						piece_mobility += blocking_piece_mult[piece_type(target_piece)];
					}
				}
			}

			// White straight-line slider
			if (is_rectilinear(piece) && is_white(piece)) {
				for (uint8_t m = 0; m < 4; m++) {
					int d_row = rect_directions[m][0];
					int d_col = rect_directions[m][1];

					int new_row = row + d_row;
					int new_col = col + d_col;

					float cumulative_blocking_factor = 1.0f;

					while (is_in(new_row, 0, 7) && is_in(new_col, 0, 7)) {
						uint8_t p = _array[new_row][new_col];

						if (white_blocked_pieces._array[new_row][new_col] == 1 || (p == b_pawn && black_pawns_controls._array[new_row][new_col])) {
							break;
						}

						if (p != none) {
							cumulative_blocking_factor *= blocking_piece_mult[piece_type(p)];
						}

						if (!black_pawns_controls._array[new_row][new_col]) {
							piece_mobility += cumulative_blocking_factor;
						}

						new_row += d_row;
						new_col += d_col;
					}
				}
			}

			// White diagonal slider
			if (is_diagonal(piece) && is_white(piece)) {
				for (uint8_t m = 0; m < 4; m++) {
					int d_row = diag_directions[m][0];
					int d_col = diag_directions[m][1];

					int new_row = row + d_row;
					int new_col = col + d_col;

					float cumulative_blocking_factor = 1.0f;

					while (is_in(new_row, 0, 7) && is_in(new_col, 0, 7)) {
						uint8_t p = _array[new_row][new_col];

						if (white_blocked_pieces._array[new_row][new_col] == 1 || (p == b_pawn && black_pawns_controls._array[new_row][new_col])) {
							break;
						}

						if (p != none) {
							cumulative_blocking_factor *= blocking_piece_mult[piece_type(p)];
						}

						if (!black_pawns_controls._array[new_row][new_col]) {
							piece_mobility += cumulative_blocking_factor;
						}

						new_row += d_row;
						new_col += d_col;
					}
				}
			}

			// White king
			if (piece == w_king) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_row = row + all_directions[m][0];
					int new_col = col + all_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && white_blocked_pieces._array[new_row][new_col] == 0 && !black_pieces_controls._array[new_row][new_col]) {
						uint8_t target_piece = _array[new_row][new_col];
						piece_mobility += blocking_piece_mult[piece_type(target_piece)];
					}
				}
			}

			// Black pawn
			if (piece == b_pawn) {
				piece_mobility = black_blocked_pieces._array[row - 1][col] == 0; // Single push
				piece_mobility += col > 0 && is_white(_array[row - 1][col - 1]); // Capture to the left
				piece_mobility += col < 7 && is_white(_array[row - 1][col + 1]); // Capture to the right
				piece_mobility += row == 6 && black_blocked_pieces._array[row - 1][col] == 0 && black_blocked_pieces._array[row - 2][col] == 0; // Double push
			}

			// Black knight
			if (piece == b_knight) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_row = row + knight_directions[m][0];
					int new_col = col + knight_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && black_blocked_pieces._array[new_row][new_col] == 0 && !white_pawns_controls._array[new_row][new_col]) {
						uint8_t target_piece = _array[new_row][new_col];
						piece_mobility += blocking_piece_mult[piece_type(target_piece)];
					}
				}
			}

			// Black straight-line slider
			if (is_rectilinear(piece) && is_black(piece)) {
				for (uint8_t m = 0; m < 4; m++) {
					int d_row = rect_directions[m][0];
					int d_col = rect_directions[m][1];

					int new_row = row + d_row;
					int new_col = col + d_col;

					float cumulative_blocking_factor = 1.0f;

					while (is_in(new_row, 0, 7) && is_in(new_col, 0, 7)) {
						uint8_t p = _array[new_row][new_col];

						if (black_blocked_pieces._array[new_row][new_col] == 1 || (p == w_pawn && white_pawns_controls._array[new_row][new_col])) {
							break;
						}

						if (p != none) {
							cumulative_blocking_factor *= blocking_piece_mult[piece_type(p)];
						}

						if (!white_pawns_controls._array[new_row][new_col]) {
							piece_mobility += cumulative_blocking_factor;
						}

						new_row += d_row;
						new_col += d_col;
					}
				}
			}

			// Black diagonal slider
			if (is_diagonal(piece) && is_black(piece)) {
				for (uint8_t m = 0; m < 4; m++) {
					int d_row = diag_directions[m][0];
					int d_col = diag_directions[m][1];

					int new_row = row + d_row;
					int new_col = col + d_col;

					float cumulative_blocking_factor = 1.0f;

					while (is_in(new_row, 0, 7) && is_in(new_col, 0, 7)) {
						uint8_t p = _array[new_row][new_col];

						if (black_blocked_pieces._array[new_row][new_col] == 1 || (p == w_pawn && white_pawns_controls._array[new_row][new_col])) {
							break;
						}

						if (p != none) {
							cumulative_blocking_factor *= blocking_piece_mult[piece_type(p)];
						}

						if (!white_pawns_controls._array[new_row][new_col]) {
							piece_mobility += cumulative_blocking_factor;
						}

						new_row += d_row;
						new_col += d_col;
					}
				}
			}

			// Black king
			if (piece == b_king) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_row = row + all_directions[m][0];
					int new_col = col + all_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && black_blocked_pieces._array[new_row][new_col] == 0 && !white_pieces_controls._array[new_row][new_col]) {
						uint8_t target_piece = _array[new_row][new_col];
						piece_mobility += blocking_piece_mult[piece_type(target_piece)];
					}
				}
			}

			//if (display) {
			//	cout << piece_name(piece) << " on " << square_name(row, col) << ", mobility: " << piece_mobility << endl;
			//}

			const int low_bound = static_cast<int>(piece_mobility);
			const int high_bound = low_bound + 1;
			const float fractional_part = piece_mobility - low_bound;


			// Add the piece mobility
			if (is_white(piece)) {

				// Average of the before and after values
				const int lower_bound_value = virtual_mobilities[piece - 1][low_bound];
				const int upper_bound_value = virtual_mobilities[piece - 1][high_bound];
				const int interpolated_value = static_cast<int>(lower_bound_value + (upper_bound_value - lower_bound_value) * fractional_part);

				white_mobility += interpolated_value;

				if (display) {
					cout << "WHITE: " << piece_name(piece) << " on " << square_name(row, col) << ", mobility: " << piece_mobility << ", bounds: [" << lower_bound_value << ", " << upper_bound_value << "], value: " << interpolated_value << endl;
				}
			}
			else if (is_black(piece)) {

				// Average of the before and after values
				const int lower_bound_value = virtual_mobilities[piece - 7][low_bound];
				const int upper_bound_value = virtual_mobilities[piece - 7][high_bound];
				const int interpolated_value = static_cast<int>(lower_bound_value + (upper_bound_value - lower_bound_value) * fractional_part);

				black_mobility += interpolated_value;

				if (display) {
					cout << "BLACK: " << piece_name(piece) << " on " << square_name(row, col) << ", mobility: " << piece_mobility << ", bounds: [" << lower_bound_value << ", " << upper_bound_value << "], value: " << interpolated_value << endl;
				}
			}
		}
	}

	if (display) {
		cout << "white mobility: " << white_mobility << ", black mobility: " << black_mobility << endl;
	}

	// Depending on how far the game has progressed
	float advancement_factor = 1.0f;

	return eval_from_progress(white_mobility - black_mobility, _adv, advancement_factor);
}

// Returns the queen safety value for the given side
int Board::get_queen_safety(bool color) const {

	// Factors to evaluate:
	// - tempi that can be gained against the queen
	// - isolated queen
	// - queen surrounded by enemy pieces
	// - trapped queen, or one with few escape squares

	// Total value
	int queen_safety_value = 0;

	// Plural, since there can theoretically be several
	int queens_safety = 0;

	// Queen positions
	// Theoretical maximum of 9 queens per side
	Pos queens_pos[9]{};
	uint8_t queens_count = 0;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t piece = _array[row][col];

			if (piece == (color ? w_queen : b_queen)) {
				queens_pos[queens_count] = { row, col };
				queens_count++;
			}
		}
	}

	// Look at every opponent move
	Board b(*this);
	b._player = !color;
	b.get_moves();

	// Value of a tempo per piece type (pawn, knight, bishop, rook, queen, king)
	constexpr int tempo_values[6] = { 35, 100, 65, 50, 0, 0 };

	// REVIEW: to simplify every capture computation
	constexpr int unsafe_attack_values[6] = { 15, 12, 10, 5, 0, 0 };

	// Total value of the moves able to attack each queen
	int queens_attacks_value[9] = { 0 };

	// TODO: consider only the safe moves
	//Map base_controls = color ? get_black_controls_map() : get_white_controls_map();

	SquareMap opponent_controls = color ? get_white_controls_map() : get_black_controls_map();

	// Count the number of moves attacking the queen
	for (uint8_t m = 0; m < b._got_moves; m++) {
		Move& move = b._moves[m];

		// Play the move
		Board b2(b);
		b2.make_move(move);

		// Piece played
		uint8_t piece = b2._array[move.end_row][move.end_col];

		// Look at the side's controls after the move
		SquareMap controls;
		add_piece_controls(&controls, move.end_row, move.end_col, piece);

		//Map controls = color ? b2.get_black_controls_map() : b2.get_white_controls_map();

		// Check whether any queen is attacked
		for (uint8_t q = 0; q < queens_count; q++) {
			//if (controls._array[queens_pos[q].row][queens_pos[q].col] > (color == _player ? base_controls._array[queens_pos[q].row][queens_pos[q].col] : 0)) {
			if (controls._array[queens_pos[q].row][queens_pos[q].col]) {
				queens_attacks_value[q] += opponent_controls._array[move.end_row][move.end_col] ? unsafe_attack_values[(piece - 1) % 6] : tempo_values[(piece - 1) % 6];
			}
		}
	}

	// For now this stays linear in the number of moves able to attack the queen
	// TODO: use a richer function accounting for the queen's placement and the enemy pieces around it
	//constexpr int tempo_on_queen_malus = 50;

	// Value of an exact tempo, when the queen is attacked
	//constexpr int max_tempo = 100;

	int queens_tempo_penalty = 0;

	for (uint8_t q = 0; q < queens_count; q++) {
		//queens_tempo_penalty += queens_attacks_value[q] * tempo_on_queen_malus;
		queens_tempo_penalty += queens_attacks_value[q];
		//queens_tempo_penalty += max_tempo * (1.0f - 1.0f / (queens_attacks_value[q] + 1.0f));
	}

	// Add the term's value
	queen_safety_value -= queens_tempo_penalty * (1 - _adv);


	// TODO: proximity to the king should carry a penalty
	// Much larger on the same file at close range, and somewhat on a diagonal too


	return queen_safety_value;
}

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
	//_moves_flags_assigned = false;
	_sorted_moves = false;
	_game_over_checked = false;
}

// Assigns the flags of a given move
void Board::assign_move_flags(Move* move) const {

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
	}

	// Capture normale ou en passant
	if (dest != none || (is_pawn(piece) && move->start_col != move->end_col)) {
		move->set_flag(IS_CAPTURE);
	}

	// Evaluate on a copy
	Board b;
	b.copy_data(*this);
	b.make_move(*move);

	// TODO: optimise this
	// TODO *** see whether a stalemate can be detected quickly

	// Check
	if (b.in_check()) {
		move->set_flag(IS_CHECK);

		// Mat
		b.is_game_over();
		if ((b._game_over_value == white_win && _player) ||
			(b._game_over_value == black_win && !_player)) {
			move->set_flag(IS_MATE);
		}
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
inline void Board::update_bitboards_white(int row1, int col1, int row2, int col2, int p, int p_last) noexcept {

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

	// Promotion (peu probable)
	_bitboards[p + (p == w_pawn) * (row2 == 7) * 4] |= to_mask;

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
inline void Board::update_bitboards_black(int row1, int col1, int row2, int col2, int p, int p_last) noexcept {

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

	// Promotion (peu probable)
	_bitboards[p + (p == b_pawn) * (row2 == 0) * 4] |= to_mask;

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
