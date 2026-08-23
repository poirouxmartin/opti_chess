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

	// Add the pawn control over the squares using pawn bitboards
	uint64_t wp = _bitboards[w_pawn];
	while (wp) {
		const int sq = pop_lsb(wp);
		const uint8_t i = sq >> 3;
		const uint8_t j = sq & 7;
		// White pawn at (i,j) controls (7-i-1, j-1) and (7-i-1, j+1) in the square_controls table
		const uint8_t di = 7 - i - 1;
		if (j - 1 >= 0 && di >= 0)
			white_controls[di][j - 1] = true;
		if (j + 1 < 8 && di >= 0)
			white_controls[di][j + 1] = true;
	}
	uint64_t bp = _bitboards[b_pawn];
	while (bp) {
		const int sq = pop_lsb(bp);
		const uint8_t i = sq >> 3;
		const uint8_t j = sq & 7;
		// Black pawn at (i,j) controls (i-1, j-1) and (i-1, j+1)
		if (j - 1 >= 0 && i - 1 >= 0)
			black_controls[i - 1][j - 1] = true;
		if (j + 1 < 8 && i - 1 >= 0)
			black_controls[i - 1][j + 1] = true;
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

int Board::get_space() const
{
	// Multiply by a weight
	// The space advantage depends on how many pieces remain

	int w_pieces = 0; int b_pieces = 0;

	w_pieces = __popcnt64(_occupancies[0] & ~_bitboards[w_pawn]);
	b_pieces = __popcnt64(_occupancies[1] & ~_bitboards[b_pawn]);
		

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
	uint64_t all_sliders = _bitboards[w_bishop] | _bitboards[b_bishop] | _bitboards[w_rook] | _bitboards[b_rook] | _bitboards[w_queen] | _bitboards[b_queen];
	while (all_sliders) {
		const int sq = pop_lsb(all_sliders);
		const uint8_t row = sq >> 3;
		const uint8_t col = sq & 7;

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

					const float d2 = distance * distance;
					const float division_factor = d2 * d2;
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
					//cout << "negative value (overflow) in piece alignments" << endl;
				}

				if (pinning_piece_color) {
					w_pins += total_value;
				}
				else {
					b_pins += total_value;
				}
		}
	}

	// Depending on how far the game has progressed
	constexpr float alignment_adv_factor = 1.0f;

	return eval_from_progress(w_pins - b_pins, _adv, alignment_adv_factor);
}

// Updates the king positions
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
	if (_controls_map_valid)
		return _cached_white_controls;

	// Compute both maps together (always called as a pair)
	_cached_white_controls = SquareMap();
	_cached_black_controls = SquareMap();

	uint64_t occ = _occupancies[2];
	while (occ) {
		const int sq = pop_lsb(occ);
		const uint8_t row = sq >> 3;
		const uint8_t col = sq & 7;
		const uint8_t piece = _array[row][col];
		if (is_white(piece)) {
			add_piece_controls(&_cached_white_controls, row, col, piece);
		}
		else if (is_black(piece)) {
			add_piece_controls(&_cached_black_controls, row, col, piece);
		}
	}

	_controls_map_valid = true;
	return _cached_white_controls;
}

// Returns the map of control counts for each square, for Black
SquareMap Board::get_black_controls_map() const
{
	if (_controls_map_valid)
		return _cached_black_controls;

	// Trigger computation (always computed together with white map)
	get_white_controls_map();
	return _cached_black_controls;
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

	// White rooks
	{
		uint64_t bb = _bitboards[w_rook];
		while (bb) {
			const int sq = pop_lsb(bb);
			const uint8_t row = sq >> 3;
			const uint8_t col = sq & 7;

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
	}

	//1r2q2k/4p1b1/p1n1Q1p1/1pp2p1p/3p4/3P1N1P/PPP1RPP1/4R1K1 b - - 3 26

	// Black rooks
	{
		uint64_t bb = _bitboards[b_rook];
		while (bb) {
			const int sq = pop_lsb(bb);
			const uint8_t row = sq >> 3;
			const uint8_t col = sq & 7;

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

	// Multiplier based on how far the game has progressed
	float advancement_factor = 1.0f;

	//cout << "Final rook activity: " << activity << ", advancement factor: " << advancement_factor << " => " << eval_from_progress(activity, _adv, advancement_factor) << endl;

	return eval_from_progress(activity, _adv, advancement_factor);
}


// Equality operator: compares only piece placement, castling rights and en passant
int Board::get_bishop_activity() const {
	// Bishop mobility = number of non-pawn squares along its diagonals

	// Baseline bishop activity
	constexpr int normal_bishop_activity = 3;

	// Bonus for the white bishop
	int w_bishop_activity = 0;

	// Bonus for the black bishop
	int b_bishop_activity = 0;

	// White bishops
	{
		uint64_t bb = _bitboards[w_bishop];
		while (bb) {
			const int sq = pop_lsb(bb);
			const int row = sq >> 3;
			const int col = sq & 7;

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
	}

	// Black bishops
	{
		uint64_t bb = _bitboards[b_bishop];
		while (bb) {
			const int sq = pop_lsb(bb);
			const int row = sq >> 3;
			const int col = sq & 7;

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

	// Multiplier based on game progress
	float bishop_activity_advancement_factor = 0.5f;

	// TODO: make this non-linear?

	return eval_from_progress(w_bishop_activity - b_bishop_activity, _adv, bishop_activity_advancement_factor);
}

// Tells whether a move is legal
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

	{
		uint64_t bb = _bitboards[w_pawn];
		while (bb) {
			const int sq = pop_lsb(bb);
			const uint8_t row = sq >> 3;
			const uint8_t col = sq & 7;
			(col > 0) && (w_pawn_controls[row + 1][col - 1] = true);
			(col < 7) && (w_pawn_controls[row + 1][col + 1] = true);
		}
	}
	{
		uint64_t bb = _bitboards[b_pawn];
		while (bb) {
			const int sq = pop_lsb(bb);
			const uint8_t row = sq >> 3;
			const uint8_t col = sq & 7;
			(col > 0) && (b_pawn_controls[row - 1][col - 1] = true);
			(col < 7) && (b_pawn_controls[row - 1][col + 1] = true);
		}
	}


	// White piece mobility
	int white_mobility = 0;

	// Black piece mobility
	int black_mobility = 0;

	// Piece mobility
	uint64_t occ = _occupancies[2];
	while (occ) {
		const int sq = pop_lsb(occ);
		const uint8_t row = sq >> 3;
		const uint8_t col = sq & 7;
		uint8_t piece = _array[row][col];

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

	if (display) {
		cout << "White mobility: " << white_mobility << ", black mobility: " << black_mobility << endl;
	}

	// Multiplier based on game progress
	float piece_mobility_advancement_factor = 1.0f;

	return eval_from_progress(white_mobility - black_mobility, _adv, piece_mobility_advancement_factor);
}

// Tells whether the pawn can move
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

	// Bound-safe byte access (same rationale as in get_long_term_piece_mobility)
	auto piece_at = [&](int r, int c) -> uint8_t {
		if (r < 0 || r > 7 || c < 0 || c > 7) return none;
		return _array[r][c];
	};

	// For each piece
	uint64_t occ = _occupancies[2];
	while (occ) {
		const int sq = pop_lsb(occ);
		const uint8_t row = sq >> 3;
		const uint8_t col = sq & 7;
		uint8_t piece = _array[row][col];

		int piece_mobility = 0;

		// White pawn
		if (piece == w_pawn) {
			piece_mobility = _array[row + 1][col] == none; // Single push
			piece_mobility += col > 0 && is_black(piece_at(row + 1, col - 1)); // Capture to the left
			piece_mobility += col < 7 && is_black(piece_at(row + 1, col + 1)); // Capture to the right
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
						uint8_t p = piece_at(new_row, new_col);
						
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
						uint8_t p = piece_at(new_row, new_col);

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
				piece_mobility += col > 0 && is_white(piece_at(row - 1, col - 1)); // Capture to the left
				piece_mobility += col < 7 && is_white(piece_at(row - 1, col + 1)); // Capture to the right
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
						uint8_t p = piece_at(new_row, new_col);

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
						uint8_t p = piece_at(new_row, new_col);

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

	if (display) {
		cout << "white mobility: " << white_mobility << ", black mobility: " << black_mobility << endl;
	}

	// Depending on how far the game has progressed
	float advancement_factor = 1.0f;

	return eval_from_progress(white_mobility - black_mobility, _adv, advancement_factor);
}
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

	// Bound-safe byte access for this function's board scans: ASAN flagged a
	// 1-byte read past a caller frame originating here; clamping guarantees an
	// out-of-range row/col can never escape the object.
	auto piece_at = [&](int r, int c) -> uint8_t {
		if (r < 0 || r > 7 || c < 0 || c > 7) return none;
		return _array[r][c];
	};

	// For each piece
	uint64_t occ = _occupancies[2];
	while (occ) {
		const int sq = pop_lsb(occ);
		const uint8_t row = sq >> 3;
		const uint8_t col = sq & 7;
		uint8_t piece = _array[row][col];

		float piece_mobility = 0.0f;

		// White pawn
		if (piece == w_pawn) {
			piece_mobility = white_blocked_pieces._array[row + 1][col] == 0; // Single push
			piece_mobility += col > 0 && is_black(piece_at(row + 1, col - 1)); // Capture to the left
			piece_mobility += col < 7 && is_black(piece_at(row + 1, col + 1)); // Capture to the right
			piece_mobility += row == 1 && white_blocked_pieces._array[row + 1][col] == 0 && white_blocked_pieces._array[row + 2][col] == 0; // Double push
			}

			// White knight
			if (piece == w_knight) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_row = row + knight_directions[m][0];
					int new_col = col + knight_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && white_blocked_pieces._array[new_row][new_col] == 0 && !black_pawns_controls._array[new_row][new_col]) {
						uint8_t target_piece = piece_at(new_row, new_col);
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
						uint8_t p = piece_at(new_row, new_col);

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
						uint8_t p = piece_at(new_row, new_col);

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
						uint8_t target_piece = piece_at(new_row, new_col);
						piece_mobility += blocking_piece_mult[piece_type(target_piece)];
					}
				}
			}

			// Black pawn
			if (piece == b_pawn) {
				piece_mobility = black_blocked_pieces._array[row - 1][col] == 0; // Single push
				piece_mobility += col > 0 && is_white(piece_at(row - 1, col - 1)); // Capture to the left
				piece_mobility += col < 7 && is_white(piece_at(row - 1, col + 1)); // Capture to the right
				piece_mobility += row == 6 && black_blocked_pieces._array[row - 1][col] == 0 && black_blocked_pieces._array[row - 2][col] == 0; // Double push
			}

			// Black knight
			if (piece == b_knight) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_row = row + knight_directions[m][0];
					int new_col = col + knight_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && black_blocked_pieces._array[new_row][new_col] == 0 && !white_pawns_controls._array[new_row][new_col]) {
						uint8_t target_piece = piece_at(new_row, new_col);
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
						uint8_t p = piece_at(new_row, new_col);

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
						uint8_t p = piece_at(new_row, new_col);

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
						uint8_t target_piece = piece_at(new_row, new_col);
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

	if (display) {
		cout << "white mobility: " << white_mobility << ", black mobility: " << black_mobility << endl;
	}

	// Depending on how far the game has progressed
	float advancement_factor = 1.0f;

	return eval_from_progress(white_mobility - black_mobility, _adv, advancement_factor);
}

