// Legal move generation: pins and check masks first, then direct legal moves per piece.
// Split from board.cpp (same code); the add_* helpers are only used by get_moves() here.

#include "board.h"

#include <algorithm>
#include <bit>
#include <cstdlib>

namespace {

	// Precomputed attack tables (square index = row * 8 + col, row 0 = rank 1)
	struct AttackTables {
		uint64_t knight[64];
		uint64_t king[64];
		uint64_t pawn[2][64];	// [0]: squares a white pawn on sq captures, [1]: same for a black pawn
		uint64_t ray[8][64];	// empty-board rays, in all_directions order (0-3 diagonal, 4-7 straight)
		uint64_t lines[64];		// union of the 8 rays (every square a queen on an empty board reaches)
	};

	constexpr AttackTables make_attack_tables() {
		AttackTables t{};
		for (int sq = 0; sq < 64; sq++) {
			const int r = sq >> 3, c = sq & 7;
			auto set = [](uint64_t& bb, int rr, int cc) { if (rr >= 0 && rr < 8 && cc >= 0 && cc < 8) bb |= 1ULL << (rr * 8 + cc); };
			for (int k = 0; k < 8; k++)
				set(t.knight[sq], r + knight_directions[k][0], c + knight_directions[k][1]);
			for (int d = 0; d < 8; d++) {
				const int dr = all_directions[d][0], dc = all_directions[d][1];
				set(t.king[sq], r + dr, c + dc);
				for (int rr = r + dr, cc = c + dc; rr >= 0 && rr < 8 && cc >= 0 && cc < 8; rr += dr, cc += dc)
					t.ray[d][sq] |= 1ULL << (rr * 8 + cc);
				t.lines[sq] |= t.ray[d][sq];
			}
			set(t.pawn[0][sq], r + 1, c - 1);
			set(t.pawn[0][sq], r + 1, c + 1);
			set(t.pawn[1][sq], r - 1, c - 1);
			set(t.pawn[1][sq], r - 1, c + 1);
		}
		return t;
	}

	constexpr AttackTables ATTACKS = make_attack_tables();

	// Squares reached along one ray, stopping on (and including) the first blocker
	inline uint64_t ray_attacks(const int d, const int sq, const uint64_t occ) noexcept {
		const uint64_t ray = ATTACKS.ray[d][sq];
		const uint64_t blockers = ray & occ;
		if (!blockers)
			return ray;
		// Directions 2, 3, 6, 7 increase the square index: nearest blocker = lowest bit
		const bool increasing = (d & 2) != 0;
		const int b = increasing ? std::countr_zero(blockers) : 63 - std::countl_zero(blockers);
		return ray ^ ATTACKS.ray[d][b];
	}

	inline uint64_t diag_attacks(const int sq, const uint64_t occ) noexcept {
		return ray_attacks(0, sq, occ) | ray_attacks(1, sq, occ) | ray_attacks(2, sq, occ) | ray_attacks(3, sq, occ);
	}

	inline uint64_t rect_attacks(const int sq, const uint64_t occ) noexcept {
		return ray_attacks(4, sq, occ) | ray_attacks(5, sq, occ) | ray_attacks(6, sq, occ) | ray_attacks(7, sq, occ);
	}

	// Enemy pieces attacking square sq, the side to move being player (true = white)
	inline uint64_t enemy_attackers(const uint64_t* bitboards, const uint64_t occ, const bool player, const int sq) noexcept {
		const uint64_t* enemy = bitboards + (player ? 6 : 0);	// enemy[1..6] = pawn..king
		return (ATTACKS.knight[sq] & enemy[2])
			| (ATTACKS.pawn[player ? 0 : 1][sq] & enemy[1])
			| (ATTACKS.king[sq] & enemy[6])
			| (diag_attacks(sq, occ) & (enemy[3] | enemy[5]))
			| (rect_attacks(sq, occ) & (enemy[4] | enemy[5]));
	}

	constexpr uint64_t FILE_A = 0x0101010101010101ULL;
	constexpr uint64_t FILE_H = 0x8080808080808080ULL;
}

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

	// Every square attacked by an enemy piece, built from attack tables.
	// Sliders see through the king under consideration (it cannot hide behind itself).
	const uint64_t* enemy = _bitboards + (player ? 6 : 0);	// enemy[1..6] = pawn..king
	const uint64_t occ = _occupancies[2] & ~_bitboards[player ? w_king : b_king];

	// Pawns, all at once: black pawns capture downwards, white pawns upwards
	uint64_t attacked = player
		? ((enemy[1] >> 7) & ~FILE_A) | ((enemy[1] >> 9) & ~FILE_H)
		: ((enemy[1] << 7) & ~FILE_H) | ((enemy[1] << 9) & ~FILE_A);

	for (uint64_t bb = enemy[2]; bb;)
		attacked |= ATTACKS.knight[pop_lsb(bb)];

	for (uint64_t bb = enemy[3] | enemy[5]; bb;)
		attacked |= diag_attacks(pop_lsb(bb), occ);

	for (uint64_t bb = enemy[4] | enemy[5]; bb;)
		attacked |= rect_attacks(pop_lsb(bb), occ);

	if (enemy[6])
		attacked |= ATTACKS.king[std::countr_zero(enemy[6])];

	// Keep the action zone only, and map it onto the 3x5 control bits
	uint64_t zone = 0;
	const uint64_t zone_row = (0xFFULL >> (7 - max_col + min_col)) << min_col;
	for (int r = min_row; r <= max_row; r++)
		zone |= zone_row << (r * 8);

	uint16_t controls = 0;
	for (uint64_t hits = attacked & zone; hits;) {
		const int sq = pop_lsb(hits);
		controls |= control_bit((sq >> 3) - king_pos.row, (sq & 7) - king_pos.col);
	}

	return controls;
}

// Returns one of the pieces attacking the square, when there are several
PieceSquare Board::get_square_attacker(Pos square, int* n_attackers) const noexcept {
	const uint8_t square_row = square.row;
	const uint8_t square_col = square.col;

	PieceSquare attacker = PieceSquare(none, { -1, -1 });

	// Bitboard pre-check: with no attacker at all (by far the common case) answer at once;
	// otherwise the walk below finds the attacker(s) in its usual order
	if (!enemy_attackers(_bitboards, _occupancies[2], _player, square_row * 8 + square_col))
		return attacker;

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

	// Enemy sliders able to pin along each kind of direction
	const uint64_t* enemy = _bitboards + (player ? 6 : 0);
	const uint64_t diag_pinners = enemy[3] | enemy[5];
	const uint64_t rect_pinners = enemy[4] | enemy[5];
	const int king_sq = king_row * 8 + king_col;

	// Walk the directions
	for (int d = 0; d < 8; d++) {

		// No matching enemy slider anywhere on this ray: no pin possible, skip the walk
		if (!(ATTACKS.ray[d][king_sq] & (d < 4 ? diag_pinners : rect_pinners)))
			continue;

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

	// Attack-table test from the king square: same answer as walking the lines around the king
	const int king_sq = _player ? _white_king_pos.row * 8 + _white_king_pos.col : _black_king_pos.row * 8 + _black_king_pos.col;

	return enemy_attackers(_bitboards, _occupancies[2], _player, king_sq) != 0;
}

// Conservative filter: false only when the move certainly cannot check the king on king_sq.
// A check needs the destination on a line or knight jump from that king (direct check), the
// origin on a line from it (discovered check), castling (the rook) or en passant (captured pawn)
bool Board::move_may_give_check(const Move& move, const int king_sq) const noexcept
{
	const int from = move.start_row * 8 + move.start_col;
	const int to = move.end_row * 8 + move.end_col;

	if ((ATTACKS.lines[king_sq] & SQUARE_MASKS[from]) || ((ATTACKS.lines[king_sq] | ATTACKS.knight[king_sq]) & SQUARE_MASKS[to]))
		return true;

	const uint8_t piece = _array[move.start_row][move.start_col];

	// Castling
	if (is_king(piece) && abs(move.end_col - move.start_col) == 2)
		return true;

	// En passant
	if (is_pawn(piece) && move.end_col != move.start_col && _array[move.end_row][move.end_col] == none)
		return true;

	return false;
}
