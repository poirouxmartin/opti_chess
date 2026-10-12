// Legal move generation: pins and check masks first, then direct legal moves per piece.
// Split from board.cpp (same code); the add_* helpers are only used by get_moves() here.

#include "board.h"

#include <algorithm>
#include <cstdlib>

// Adds a move to a move list
inline bool Board::add_move(const Move move, uint8_t &iterator, const uint8_t piece) noexcept
{
	// Overflow guard: silently drop instead of overflowing _moves[] into
	// adjacent Board members (no cout: this is a generation-hot path; the cap
	// is a deliberate memory optimization and almost never binds).
	if (iterator >= max_moves) {
		return false;
	}

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
		int new_row = row + direction;
		if (!is_in(new_row, 0, 7))
			return true;

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
				// A lateral discovered check is the one case en passant can expose (both pawns
				// leave the rank). Test: rnbqkbn1/ppppp3/6p1/3KPp1r/8/7p/PPPP1PPP/RNBQ1BNR w q f6 0 7
				const Pos king_pos = player ? _white_king_pos : _black_king_pos;
				if (en_passant_exposes_king(player, row, col, new_col))
					en_passant = false;

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
				// Same lateral discovered-check test as the left capture
				const Pos king_pos = player ? _white_king_pos : _black_king_pos;
				if (en_passant_exposes_king(player, row, col, new_col))
					en_passant = false;

				// While in check, the move must interpose
				if (en_passant)
					(!in_check || is_in_interpose_mask(interposition_mask, new_row, new_col) || (king_pos.row == row - direction && abs(king_pos.col - new_col) == 1)) && add_move(Move(row, col, new_row, new_col), iterator, piece);
			}
		}
	}

	return true;
}

// En passant removes two pawns from the same rank at once, which the pin detection cannot see.
// Walk the rank from the king towards the pawns, skipping both pawn squares: the first other
// piece met decides (enemy rook/queen = discovered check). Covers both king sides, both
// capture directions, and blockers between the king and the pawns.
// Tests: 4k3/8/8/K2pP2r/8/8/8/8 w - d6 0 1 (illegal), 4k3/8/8/r2pP1BK/8/8/8/8 w - d6 0 1 (legal)
bool Board::en_passant_exposes_king(const bool player, const uint8_t row, const uint8_t col, const int new_col) const noexcept {
	const Pos king_pos = player ? _white_king_pos : _black_king_pos;
	if (king_pos.row != row)
		return false;

	const int step = king_pos.col < col ? 1 : -1;
	for (int c = king_pos.col + step; c >= 0 && c < 8; c += step) {
		if (c == col || c == new_col)
			continue;
		const uint8_t p = _array[row][c];
		if (p != none)
			return !is_ally(p, player) && is_rectilinear(p);
	}

	return false;
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

			// Conversion en row/col
			uint8_t row = sq >> 3;
			uint8_t col = sq & 7;

			// Piece type: a single array read instead of scanning up to 5 bitboards
			// (the king, absent from that scan, falls through every branch below)
			const uint8_t piece = _array[row][col];

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
