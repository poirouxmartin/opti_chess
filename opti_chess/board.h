#pragma once
#include <iostream>
#include <vector>
#include <execution>
#include <array>
#include <string>
#include "evaluation.h"
#include "neural_network.h"
#include <cstdint>
#include "raylib.h"
#include <iomanip>
#include <robin_map.h>
#include "useful_functions.h"

using namespace std;
using RepetitionHistory = tsl::robin_map<uint64_t, uint8_t>;


// TODO: use them
// Enumeration of the pieces
constexpr enum piece_type { none = 0, w_pawn = 1, w_knight = 2, w_bishop = 3, w_rook = 4, w_queen = 5, w_king = 6, b_pawn = 7, b_knight = 8, b_bishop = 9, b_rook = 10, b_queen = 11, b_king = 12 };

// Number of half-moves before the game is declared drawn
constexpr uint8_t max_half_moves = 100;

// Maximum number of legal moves per position.
// The all-time record is 218; promotion storms generate 4 moves per pushing/
// capturing pawn, which regularly pushes sharp positions past 100 - silently
// truncating perft counts when the cap binds. 224 keeps a margin over the
// record while staying within uint8_t. Pool sizing adapts to the larger
// stride (see compute_pool_sizing).
constexpr uint8_t max_moves = 224;

// Value of a checkmate
constexpr int mate_value = 1e8;

// Value of a ply (doubled) in the mate search
constexpr int mate_ply = 1e5;

// Possible moves for a knight
constexpr int_fast8_t knight_directions[8][2] = { {1, 2}, {1, -2}, {-1, 2}, {-1, -2}, {2, 1}, {2, -1}, {-2, 1}, {-2, -1} };

// Straight-line moves
constexpr int_fast8_t rect_directions[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };

// Diagonal moves
constexpr int_fast8_t diag_directions[4][2] = { {1, 1}, {1, -1}, {-1, 1}, {-1, -1} };

// Moves in every direction
constexpr int_fast8_t all_directions[8][2] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1}, {-1, 0}, {0, -1}, {0, 1}, {1, 0} };

// ------------------- Colour -------------------
constexpr bool is_white(uint8_t piece) noexcept {
	return piece && piece <= w_king;
}

constexpr bool is_black(uint8_t piece) noexcept {
	return piece >= b_pawn;
}

// Returns the type of the piece (none -> none, w_pawn -> pawn, b_pawn -> pawn, etc...)
constexpr inline uint8_t piece_type(uint8_t piece) noexcept {
	return piece ? ((piece - 1) % 6 + 1) : 0;
}

// ------------------- Type -------------------
constexpr inline bool is_pawn(uint8_t piece) noexcept { return piece == w_pawn || piece == b_pawn; }
constexpr inline bool is_knight(uint8_t piece) noexcept { return piece == w_knight || piece == b_knight; }
constexpr inline bool is_bishop(uint8_t piece) noexcept { return piece == w_bishop || piece == b_bishop; }
constexpr inline bool is_rook(uint8_t piece) noexcept { return piece == w_rook || piece == b_rook; }
constexpr inline bool is_queen(uint8_t piece) noexcept { return piece == w_queen || piece == b_queen; }
constexpr inline bool is_king(uint8_t piece) noexcept { return piece == w_king || piece == b_king; }

// ------------------- Movement -------------------
constexpr bool is_rectilinear(uint8_t piece) noexcept {
	return piece == w_rook || piece == b_rook || piece == w_queen || piece == b_queen;
}

constexpr bool is_diagonal(uint8_t piece) noexcept {
	return piece == w_bishop || piece == b_bishop || piece == w_queen || piece == b_queen;
}

constexpr bool is_sliding(uint8_t piece) noexcept {
	return (piece >= w_bishop && piece <= w_queen) || (piece >= b_bishop && piece <= b_queen);
}

// ------------------- Allies -------------------
constexpr bool is_ally(uint8_t piece, bool player_white) noexcept {
	return piece && ((piece <= w_king) == player_white);
}

constexpr bool is_enemy(uint8_t piece, bool player_white) noexcept {
	return piece && ((piece <= w_king) != player_white);
}

// Promotion piece encoding (2 bits, used when IS_PROMOTION is set)
enum PromoPiece : uint8_t {
	PROMO_QUEEN = 0,
	PROMO_ROOK = 1,
	PROMO_BISHOP = 2,
	PROMO_KNIGHT = 3
};

// Returns the actual piece value for a promotion given the color
constexpr uint8_t promo_to_piece(uint8_t promo, bool player_white) noexcept {
	// Queen=5, Rook=4, Bishop=3, Knight=2 (white); +6 for black
	return (5 - promo) + (player_white ? 0 : 6);
}

// Stalemate?
enum MoveFlags : uint8_t {
	IS_NULL = 1 << 0,
	IS_CAPTURE = 1 << 1,
	IS_PROMOTION = 1 << 2,
	IS_CHECK = 1 << 3,
	IS_MATE = 1 << 4,
	RESULT_MASK = 0b1100000,
	FLAGS_EVALUATED = 1 << 7
};

struct Move {
	// Zero-initialized by default: `Move m;` MUST be a well-formed null move.
	// Uninitialized bitfields used to hand back garbage keys that passed the
	// is_null_move() checks and got inserted as phantom children (null _node).
	uint8_t start_row : 3 = 0;
	uint8_t start_col : 3 = 0;
	uint8_t end_row : 3 = 0;
	uint8_t end_col : 3 = 0;
	uint8_t promo_piece : 2 = PROMO_QUEEN;
	uint8_t flags = 0;

	// --- Flag Accessors ---
	inline bool is_null() const { return flags & IS_NULL; }
	inline bool is_capture() const { return flags & IS_CAPTURE; }
	inline bool is_promotion() const { return flags & IS_PROMOTION; }
	inline bool is_check() const { return flags & IS_CHECK; }
	inline bool is_checkmate() const { return flags & IS_MATE; }
	inline bool has_flags() const { return flags & FLAGS_EVALUATED; }
	inline uint8_t get_promo_piece() const { return promo_piece; }
	inline void set_promo_piece(uint8_t p) { promo_piece = p; }

	//inline uint8_t game_result() const { return (flags & RESULT_MASK) >> 5; }

	//inline void set_game_result(uint8_t result) {
	//	flags = (flags & ~RESULT_MASK) | ((result & 0b11) << 5);
	//}

	// --- Helpers ---
	inline void set_flag(MoveFlags f) { flags |= f; }
	inline void clear_flag(MoveFlags f) { flags &= ~f; }
	inline bool has_flag(MoveFlags f) const { return flags & f; }

	//inline void set_result(uint8_t result) {
	//	flags = (flags & ~RESULT_MASK) | ((result & 0x03) << 5);
	//}

	//inline uint8_t get_result() const {
	//	return (flags & RESULT_MASK) >> 5;
	//}

	// --- Comparisons ---
	inline bool operator==(const Move& other) const {
		return start_row == other.start_row &&
			start_col == other.start_col &&
			end_row == other.end_row &&
			end_col == other.end_col &&
			promo_piece == other.promo_piece;
	}

	inline bool operator<(const Move& other) const {
		if (start_row != other.start_row) return start_row < other.start_row;
		if (start_col != other.start_col) return start_col < other.start_col;
		if (end_row != other.end_row)   return end_row < other.end_row;
		if (end_col != other.end_col) return end_col < other.end_col;
		return promo_piece < other.promo_piece;
	}

	// Returns whether this is a null move
	bool is_null_move() const {
		return (is_null() || (start_row == 0 && start_col == 0 && end_row == 0 && end_col == 0));
	}

	// --- Display (debug only) ---
	std::string to_string() const {
		return "(" + std::to_string(start_row) + ", " + std::to_string(start_col) +
			") -> (" + std::to_string(end_row) + ", " + std::to_string(end_col) + ")";
	}

	void display() const {
		cout << to_string() << endl;
	}
};

namespace std {
	template <>
	struct hash<Move> {
		size_t operator()(const Move& m) const noexcept {
			uint16_t key = (m.start_row << 0) | (m.start_col << 3) | (m.end_row << 6) | (m.end_col << 9) | (m.promo_piece << 12);
			return hash<uint16_t>()(key);
		}
	};
}

// Castling rights (to save memory)
// 1 byte
struct CastlingRights {
	bool k_w : 1; // Kingside - White
	bool q_w : 1; // Queenside - White
	bool k_b : 1; // Kingside - Black
	bool q_b : 1; // Queenside - Black

	CastlingRights() :
		k_w(true),
		q_w(true),
		k_b(true),
		q_b(true)
	{}

	bool operator== (const CastlingRights& other) const {
		return (k_w == other.k_w) && (q_w == other.q_w) && (k_b == other.k_b) && (q_b == other.q_b);
	}
};

// Experiments towards a more compact board representation
// TODO *** to be used
#pragma pack(push, 1)
struct Piece
{
	int type : 3;
	bool color : 1;

	// Constructors
	Piece() : type(0), color(0) {}
	Piece(int type, bool color) : type(type), color(color) {}

	// Operators

	// Methods
};

// TODO: use this!!
// Rank of the chessboard (horizontal)
// 40 bytes (the minimum possible for a board-centric representation is 32 bytes)
struct Array
{
	Piece pieces[8][8];
};
#pragma pack(pop)

// Position on the board
struct Pos
{
	int row : 4; // TODO: would 3 be enough?
	int col : 4;

	// Equality operator
	bool operator== (const Pos& other) const {
		return (row == other.row) && (col == other.col);
	}

	// Returns the notation of the square
	// TODO *** use static constants so that this is precomputed
	string square() const {
		return string(1, 'a' + col) + string(1, '1' + row);
	}
};

// Map of a board (to store the controlled squares, etc...)
struct SquareMap
{
	int _array[8][8];

	// Constructors
	SquareMap() {
		for (uint8_t row = 0; row < 8; row++) {
			for (uint8_t col = 0; col < 8; col++) {
				_array[row][col] = 0;
			}
		}
	}

	// Operators

	// Subtraction
	SquareMap operator- (const SquareMap& other) const {
		SquareMap result;
		for (int row = 0; row < 8; row++) {
			for (int col = 0; col < 8; col++) {
				result._array[row][col] = _array[row][col] - other._array[row][col];
			}
		}
		return result;
	}


	// Methods

	// Aligned display
	void print() const {
		cout << "Map : " << endl;
		for (int row = 7; row >= 0; row--) {
			for (int col = 0; col < 8; col++) {
				cout << setw(3) << _array[row][col] << " ";
			}
			cout << endl;
		}
		cout << endl;
	}
};

struct BoolMap
{
	bool _array[8][8];

	// Constructors
	BoolMap() {
		for (uint8_t row = 0; row < 8; row++) {
			for (uint8_t col = 0; col < 8; col++) {
				_array[row][col] = false;
			}
		}
	}

	// Operators
	// 
	// Subtraction
	BoolMap operator- (const BoolMap& other) const {
		BoolMap result;
		for (int row = 0; row < 8; row++) {
			for (int col = 0; col < 8; col++) {
				result._array[row][col] = _array[row][col] && !other._array[row][col];
			}
		}
		return result;
	}
};

struct PieceSquare {
	uint8_t piece;
	Pos square;
};
	
// WDL chance
struct WDL {
	float win_chance; // Between 0.0f and 1.0f
	float draw_chance; // Between 0.0f and 1.0f
	float lose_chance; // Between 0.0f and 1.0f

	// Display
	string to_string() const {
		return "WDL: " + int_to_round_string(1000 * win_chance) + "/" + int_to_round_string(1000 * draw_chance) + "/" + int_to_round_string(1000 * lose_chance);
	}

	void print() const {
		cout << "WDL: " << (int)(1000 * win_chance) << "/" << (int)(1000 * draw_chance) << "/" << (int)(1000 * lose_chance) << endl;
	}
};

// Game endings
static constexpr enum game_termination { unterminated = 0, white_win = 1, draw = 2, black_win = 3 };

// Directions
struct Direction {
	int8_t d_row;
	int8_t d_col;
};

// Direction types
inline bool is_vertical(Direction d) {
	return (d.d_row != 0 && d.d_col == 0);
}

inline bool is_horizontal(Direction d) {
	return (d.d_row == 0 && d.d_col != 0);
}

inline bool is_diagonal(Direction d) {
	return (abs(d.d_row) == abs(d.d_col));
}

inline bool is_straight(Direction d) {
	return is_vertical(d) || is_horizontal(d);
}

// Is the square pinned, and in which direction?
struct PinnedSquare {
	bool pinned = false;
	Direction dir;
};

// Table of the pins of the position
struct PinsMap {
	PinnedSquare pins[8][8];

	// Aligned display
	void print() const {
		cout << "Pins map : " << endl;
		for (int row = 7; row >= 0; row--) {
			for (int col = 0; col < 8; col++) {
				cout << setw(3) << pins[row][col].pinned << " ";
			}
			cout << endl;
		}
		cout << endl;
	}
};

// Are the directions aligned?
inline bool is_aligned(int d_row, int d_col, Direction dir) {
	return (d_row * dir.d_col == d_col * dir.d_row);
}

void print_controls(uint16_t controls);

// Base layout of the squares around the king
// 0   1   2   3   4
// 5   6   7   8   9
// 10  11  12  13  14

// A 16-bit integer stores the controls around the king
inline uint16_t control_bit(int8_t rel_row, int8_t rel_col) {
	return 1u << ((rel_row + 1) * 5 + (rel_col + 2));
}

// Returns whether a square around the king is controlled
inline bool is_controlled_around_king(uint16_t controls, int8_t rel_row, int8_t rel_col) {
	return (controls & control_bit(rel_row, rel_col)) != 0;
}

// row and col must be in [0,7]
inline bool is_in_interpose_mask(uint64_t interpose_mask, uint8_t row, uint8_t col) {
	return (interpose_mask & (1ULL << (row * 8 + col))) != 0;
}

inline constexpr bool on_board(int row, int col) noexcept {
	return static_cast<unsigned>(row | col) < 8;
}

inline constexpr bool on_board_short(int8_t row, int8_t col) noexcept {
	return static_cast<unsigned>(row | col) < 8;
}

inline constexpr bool on_board_unsigned_short(uint8_t row, uint8_t col) noexcept {
	return (row | col) < 8;
}

inline constexpr bool on_board(int coord) noexcept {
    return static_cast<unsigned>(coord) < 8;
}

inline constexpr bool on_board_short(int8_t coord) noexcept {
	return static_cast<unsigned>(coord) < 8;
}

inline constexpr bool on_board_usigned_short(uint8_t coord) noexcept {
	return coord < 8;
}

inline constexpr int square_index(const int row, const int col) noexcept {
	return row * 8 + col; // 0..63
}

inline constexpr void set_bit(uint64_t& bb, const int square) noexcept {
	bb |= (1ULL << square);
}

inline constexpr void clear_bit(uint64_t& bb, const int square) noexcept {
	bb &= ~(1ULL << square);
}

//inline int pop_lsb(uint64_t& bb) noexcept {
//	const int sq = __builtin_ctzll(bb); // 1 cycle (BSF/TZCNT)
//	bb &= bb - 1;                       // 1 cycle
//	return sq;                          // total ≈ 3–4 cycles
//}


#include <immintrin.h>

inline int pop_lsb(uint64_t& bb) noexcept {
	int sq = _tzcnt_u64(bb);  // maps to TZCNT (AMD/Intel)
	bb &= bb - 1;
	return sq;
}

struct Evaluation {

	// Variables

	// Value of the evaluation
	int _value;

	// Uncertainty of the evaluation
	float _uncertainty;

	// Winnable value
	float _winnable_white;
	float _winnable_black;

	// WDL
	WDL _wdl;

	// Average score
	float _avg_score;

	// TODO *** add total value (-> move score?)

	// Evaluated?
	bool _evaluated = false;


	// Copy of the evaluation
	Evaluation& operator=(const Evaluation& other) {
		// Copies the evaluation parameters
		_value = other._value;
		_uncertainty = other._uncertainty;
		_winnable_white = other._winnable_white;
		_winnable_black = other._winnable_black;
		_wdl = other._wdl;
		_avg_score = other._avg_score;
		_evaluated = other._evaluated;

		return *this;
	}

	// Comparator
	bool operator>(Evaluation& other) {
		if (!other._evaluated)
			return true;

		if (!_evaluated)
			return false;

		return _value > other._value;
	}

	bool operator<(Evaluation& other) {
		if (!other._evaluated)
			return false;

		if (!_evaluated)
			return true;

		return _value < other._value;
	}

	// Reset
	void reset() {
		_value = 0;
		_uncertainty = 0.0f;
		_winnable_white = 1.0f;
		_winnable_black = 1.0f;
		_wdl = WDL();
		_avg_score = 0.0f;
		_evaluated = false;
	}

	// Returns the WDL of the position
	void get_WDL(int winning_eval = 110, float beta = 0.75f);

	// Returns the expected gain (in points) of the position for White (based on the WDL probabilities)
	void get_average_score(float draw_score = 0.5f);
};


// Board
class Board {
public:

	// Attributes

	// Board
	// 64 bytes
	uint8_t _array[8][8]{	{	w_rook,		w_knight,	w_bishop,   w_queen,    w_king,		w_bishop,   w_knight,   w_rook	},
							{   w_pawn,		w_pawn,		w_pawn,		w_pawn,		w_pawn,		w_pawn,		w_pawn,		w_pawn	},
							{	none,		none,		none,		none,		none,		none,		none,		none	},
							{	none,		none,		none,		none,		none,		none,		none,		none	},
							{	none,		none,		none,		none,		none,		none,		none,		none	},
							{	none,		none,		none,		none,		none,		none,		none,		none	},
							{   b_pawn,		b_pawn,		b_pawn,		b_pawn,		b_pawn,		b_pawn,		b_pawn,		b_pawn	},
							{   b_rook,		b_knight,   b_bishop,   b_queen,    b_king,		b_bishop,   b_knight,   b_rook	} };

	//Array _array; // TODO use this

	// Bitboard!! (TODO)
	// none -> w_pawn -> b_king
	uint64_t _bitboards[13];

	// White pieces, black pieces, and all of them
	uint64_t _occupancies[3];

	// TODO *** add masks for the controls
	// TODO *** magic bitboards to be used

	// TODO *** optional: cached king (will it replace the _white_king_pos?)
	//int _square_king[2];

	// Possible moves
	// Maximum number of legal moves in a position: 218

	// TODO: reduce the number of bytes used
	// 200 bytes
	Move _moves[max_moves];

	// Are the moves up to date? If not: -1, otherwise _got_moves holds the number of playable moves
	// Assuming the number of moves does not exceed 127
	// 1 byte
	int_fast8_t _got_moves = -1;

	// Whether the side to move is in check (valid whenever _got_moves >= 0)
	bool _player_in_check = false;

	// Have the move flags been assigned?
	//bool _moves_flags_assigned = false;

	// Are the moves sorted?
	bool _sorted_moves = false;

	// Side to move (true for White, false for Black)
	bool _player = true;

	// Castling rights
	CastlingRights _castling_rights;

	// En passant file
	int_fast8_t _en_passant_col = -1;

	// Number of half-moves (since the last pawn move or capture, and reset to zero on each of those)
	uint8_t _half_moves_count = 0;

	// Number of moves of the game
	uint_fast16_t _moves_count = 1;

	// Board free or active? (for the buffer)
	bool _is_active = false;

	// Index in monte_board_buffer (-1 = object outside the buffer: do not recycle)
	int _buffer_index = -1;

	// Advancement of the game
	// TODO *** put this in the nodes rather than in the boards?
	float _adv = 0.0f;
	bool _advancement = false;

	// Has the game-over computation already been done?
	bool _game_over_checked = false;
	int_fast8_t _game_over_value = unterminated;

	// The positions of the kings are stored
	Pos _white_king_pos = { 0, 4 };
	Pos _black_king_pos = { 7, 4 };

	// Have the components of the board been displayed?
	bool _displayed_components = false;

	// Zobrist key of the position
	uint64_t _zobrist_key = 0;

	// History of the positions since the last irreversible move
	RepetitionHistory _positions_history = {};

	// FIXME *** dummy variables for memory alignment, otherwise it creates very slow new vectors on every board creation
	float _dummy1 = 1.0f;
	float _dummy2 = 1.0f;

	// Cached control maps — computed lazily by get_white/black_controls_map(),
	// invalidated by reset_eval() (called at the end of make_move).
	mutable SquareMap _cached_white_controls;
	mutable SquareMap _cached_black_controls;
	mutable bool _controls_map_valid = false;

	// Cached pawn control maps — computed lazily by get_pawns_controls(),
	// invalidated by reset_eval().
	mutable SquareMap _cached_white_pawns_controls;
	mutable SquareMap _cached_black_pawns_controls;
	mutable bool _pawns_controls_valid = false;



	// Default constructor
	Board();

	// Copy constructor
	Board(const Board&, bool full = false, bool copy_history = false);

	// Equality operator (compares only the piece placement, the castling rights and the move count)
	bool operator== (const Board&) const;

	// Copies the strict minimum of the position
	void minimal_copy_data(const Board& b);

	// Copies the attributes of a board (full copy: everything is copied)
	void copy_data(const Board&, bool full = false, bool copy_history = false);

	// Adds a move to the move list
	bool add_move(const Move move, uint8_t& iterator, const uint8_t piece) noexcept;

	// Adds every king move, accounting for the controls and the castling rights
	bool add_king_moves(const bool player, const Pos king_pos, const uint16_t controls_around_king, uint8_t &iterator, const bool kingside_castle_check, const bool queenside_castle_check) noexcept;

	// Adds the moves of a pawn, accounting for the pins and the checks
	bool add_pawn_moves(const bool player, const uint8_t row, const uint8_t col, uint8_t &iterator, const PinnedSquare &pin, const bool in_check, const uint64_t &interposition_mask) noexcept;

	// Adds the moves of a knight, accounting for the pins and the checks
	bool add_knight_moves(const bool player, const uint8_t row, const uint8_t col, uint8_t &iterator, const PinnedSquare &pin, const bool in_check, const uint64_t& interposition_mask) noexcept;

	// Adds the moves of a straight-line piece, accounting for the pins and the checks
	bool add_rect_moves(const bool player, const uint8_t row, const uint8_t col, uint8_t &iterator, const PinnedSquare &pin, const bool in_check, const uint64_t& interposition_mask) noexcept;

	// Adds the moves of a diagonal piece, accounting for the pins and the checks
	bool add_diag_moves(const bool player, const uint8_t row, const uint8_t col, uint8_t &iterator, const PinnedSquare &pin, const bool in_check, const uint64_t& interposition_mask) noexcept;

	// Returns the list of legal moves
	bool get_moves() noexcept;

	// Returns the map of the controls around the king of the given player (and the castling squares, if needed)
	uint16_t get_controls_around_king(Pos king_pos, bool player, bool kingside_castle_check, bool queenside_castle_check) const noexcept;

	// Returns one of the pieces controlling the square (if there is more than one)
	PieceSquare get_square_attacker(Pos square, int* n_attackers) const noexcept;

	// Returns a bitboard of the interposition squares between the king and the attacker (including the attacker)
	uint64_t get_interpose_mask(Pos king_pos, const PieceSquare& attacker) const noexcept;

	// Returns the list of pins for the given player
	PinsMap get_pins(bool player) const noexcept;

	// Returns whether there is a check
	bool in_check(bool update_king_pos = true) noexcept;

	// Displays the list of moves
	void display_moves();

	// Displays the board
	void display() const;

	// Plays a move
	void make_move(const Move& move, const bool pgn = false, const bool add_to_history = false) noexcept;

	// Undoes a move
	void unmake_move(Move move, uint8_t p1, uint8_t p2, int en_passant_col, int prev_half_count, bool k_castle, bool q_castle, bool is_castle, bool is_promotion, bool is_en_passant);

	// Returns the advancement of the game (0 = opening, 1 = endgame)
	void game_advancement();

	// Counts the material on the board and returns its value
	int count_material(const Evaluator* e = nullptr, float closed_factor = 0.0f) const;

	// Counts the bishop pairs and returns the value
	int count_bishop_pairs() const;

	// Counts and returns the value of the penalties tied to doubled pieces
	int count_doubled_pieces(const Evaluator* eval) const;

	// Computes and returns the positioning value of the pieces on the board
	int pieces_positioning(const Evaluator* eval = nullptr) const;

	// Evaluates the position with heuristics
	void evaluate(Evaluation* eval, Evaluator* evaluator = nullptr, bool display = false, Network* n = nullptr, bool check_game_over = false);

	// Loads the board from a FEN
	void from_fen(string);

	// Returns the FEN of the board
	string to_fen() const;

	// Returns the winner if the game is over (-1/1, and 2 for a draw), 0 otherwise
	int game_over(int max_repetitions);

	// Returns the winner if the game is over (-1/1, and 2 for a draw), 0 otherwise -> and stores the value in _game_over_value
	int is_game_over(int max_repetitions = 2);
	//int is_game_over(int max_repetitions = 3);

	// Returns the label of a move
	string move_label(Move move, bool use_uft8 = false);

	// Displays a text inside a given area
	static void draw_text_rect(const string&, float, float, float, float, float);

	// Plays the sound of a move
	void play_move_sound(Move);

	// Resets the board to its base state (for the buffer)
	void reset_board(bool display = false);

	// Computes and returns the value matching the safety of the kings
	int get_king_safety(int activity_diff, float display_factor = 0.0f);

	// Returns whether a piece can be captured by the enemy (for the GUI display)
	bool is_capturable(int, int);

	// Displays the PGN
	void display_pgn() const;

	// Returns, from the evaluation, whether this is a mate or not
	int is_eval_mate(int) const;

	// Generates the opening book
	void generate_opening_book(int nodes = 100000);

	// Returns a simple and fast representation of the position
	string simple_position() const;

	// Computes the pawn structure and returns its value
	int get_pawn_structure(float display_factor = 0.0f);

	// Computes the resultant of the attacks and defences and returns it
	float get_attacks_and_defenses() const;

	// Computes and returns the opposition of the kings (in pawn endgames)
	int get_kings_opposition();

	// Returns the type of the selected piece
	uint8_t selected_piece() const;

	// Returns the type of the piece the mouse has just clicked on
	uint8_t clicked_piece() const;

	// Returns whether the selected piece belongs to the side to move
	bool selected_piece_has_trait() const;

	// Returns whether the clicked piece belongs to the side to move
	bool clicked_piece_has_trait() const;

	// Resets the clocks "to zero" (to the base time)
	static void reset_timers();

	// Resets the board to its initial position
	void restart();

	// Returns the material difference between the two sides
	int material_difference() const;

	// Resets the components of the evaluation
	void reset_eval();

	// Counts the rooks on open and semi-open files and returns the value
	int get_sliders_on_open_file() const;

	// Computes the value of the controlled squares on the board
	int get_square_controls() const;

	// Sorts the moves quickly (putting the captures first)
	bool sort_moves();

	// Clicks the move m
	bool click_m_move(Move i, bool orientation) const;

	// Returns the colour of the side to move (1 for White, -1 for Black)
	int get_color() const;

	// Computes and returns the space advantage
	int get_space() const;

	// Computes and returns an evaluation of the alignments
	int get_alignments() const;

	// Updates the position of the kings
	bool update_kings_pos();

	// Returns the activity of the pieces
	int get_piece_activity() const;

	// Returns the map of the number of controls on each square of the board for White
	SquareMap get_white_controls_map() const;

	// Returns the map of the number of controls on each square of the board for Black
	SquareMap get_black_controls_map() const;

	// Adds the controls of a piece to a map
	bool add_piece_controls(SquareMap* map, int i, int j, int piece) const;

	// Returns the virtual mobility of a king
	int get_king_virtual_mobility(bool color);

	// Returns the number of 'safe' checks in the position for both players
	int get_checks_value(SquareMap* white_controls, SquareMap* black_controls, bool color);

	// Returns the move generation speed
	int moves_generation_benchmark(uint8_t depth, bool main_call = true);

	// Returns the value of the fianchettoed bishops
	int get_fianchetto_value() const;

	// Returns whether the square is controlled by a player
	bool is_controlled(int square_i, int square_j, bool player) const;

	// Computes and returns the value of the pawn push threats
	int get_pawn_push_threats() const;

	// Computes and returns the proximity of the king to the pawns
	int get_king_proximity();

	// Computes and returns the activity/mobility of the rooks
	int get_rook_activity() const;

	// Computes and returns the value of the pawns blocking the bishops
	int get_bishop_pawns() const;

	// Returns the value of the long-term weaknesses of the pawn shield
	int get_pawn_shield();

	// Returns the value of the weak squares
	int get_weak_squares(bool color, bool around_king = false);

	// Converts a move into its algebraic notation
	string algebric_notation(Move move) const;

	// Converts an algebraic notation into a move
	Move move_from_algebric_notation(string notation);

	// Returns the value of the distance to a possible castle
	int get_castling_distance() const;

	// Generates the Zobrist key of the board
	void get_zobrist_key();

	// Returns how winnable the game is (from 0 to 1), for a given colour
	float get_winnable(Evaluation* eval, bool color, float position_nature) const;

	// Computes the winning-chance values for each side
	void get_winnable_values(Evaluation* eval, float position_nature = 0.0f) const;

	// Returns the activity of the bishops on the diagonals
	int get_bishop_activity() const;

	// Returns whether a move is legal or not
	bool is_legal(Move move);

	// Resets the position history
	void reset_positions_history();

	// Returns how many times the current position has been repeated
	int repetition_count();

	// Displays the position history (the Zobrist keys)
	//void display_positions_history() const;

	// Quiescence search for the GrogrosZero algorithm
	//int grogros_quiescence(Evaluator* eval, int alpha = -2147483647, int beta = 2147483647, int depth = 4, bool explore_checks = true, bool main_player = true);

	// Returns the display of the evaluation
	string evaluation_to_string(int eval) const;

	// Returns the evaluation of the trapped pieces
	int get_trapped_pieces() const;

	// Adjusts the piece values (penalty/bonus), depending on the type of position
	int get_updated_piece_values() const;

	// Returns the nature of the position as a number: 0 = open, 1 = closed
	float get_position_nature() const;

	// Returns the value of the bonuses tied to the open and semi-open files on the opposing king
	int get_open_files_on_opponent_king(bool color);

	// Returns the value of the bonuses tied to the open and semi-open diagonals on the opposing king
	int get_open_diagonals_on_opponent_king(bool color);

	// Returns the number of retreat squares for the king
	int get_king_escape_squares(bool color);

	// Returns a value matching the pieces attacking the opposing king
	int get_king_attackers(bool color);

	// Returns a value matching the pieces defending the king
	int get_king_defenders(bool color);

	// Returns a bonus matching the pawn storm on the opposing king on a given file
	int get_pawn_storm_at_col(bool color, uint8_t king_row, uint8_t king_col) const;

	// Returns the protective strength of the pawn structure of the king
	int get_pawn_storm(bool color);

	// Returns an activity bonus for the knights
	int get_knight_activity() const;

	// Returns the protective strength of the pawn structure of the king
	int get_pawn_shield_protection(bool color, float opponent_attacking_potential, int space);

	// Returns the protective strength of the pawn structure of the king, were it on the given file
	int get_pawn_shield_protection_at_column(bool color, int column, float opponent_attacking_potential, bool add_column_bonus = false, int space = 0);

	// Computes every move up to a certain depth, and returns the total node count
	long long int count_nodes_at_depth(int depth, bool display = true, bool main = false);

	// Parallelized version
	long long int count_nodes_at_depth_parallelized(int depth, bool display, bool main = false);

	// Returns whether the node count computed for a position at a certain depth matches the expected one
	bool validate_nodes_count_at_depth(string fen, int depth, vector<long long int> expected_nodes, bool display = false, bool display_full = false, bool parallel = false);

	// Runs the same validation several times, and displays the average, minimum and maximum time, the standard deviation...
	void benchmark_nodes_count_at_depth(string fen, int depth, vector<long long int> expected_nodes, int iterations = 10, bool display = false, bool parallel = false);

	// Test function: new piece mobility
	int get_piece_mobility(bool display = false) const;

	// Returns whether the pawn can move
	bool pawn_can_move(uint8_t row, uint8_t col, bool color) const;

	// Returns the uncertainty of the position
	void get_uncertainty(Evaluation *eval, int material_eval, int winning_eval = 110) const;

	// Swaps the colours of the players (including the side to move)
	void switch_colors();

	// Iterates over the distance map from a given position, and returns the newly controlled squares
	vector<Pos> get_next_king_squares(SquareMap& map, Pos start_pos, int distance, bool color) const;

	// Returns a map of the distances between the king and every point of the board (the number of moves needed to get there, given the current controls of the board)
	SquareMap get_king_squares_distance(bool color);

	// Returns the weakness on the ranks of the king
	int get_king_row_weakness(bool color);

	// Returns the centralization value of the king in the endgame
	int get_king_centralization(bool color);

	// Returns the value of the undefended pieces
	int get_unprotected_pieces(bool color) const;

	// Returns whether the king is inside the square of the pawn
	bool in_king_square(Pos pos, bool color);

	// Returns whether this is a pawn endgame
	bool is_pawn_endgame() const;

	// Returns whether the player still has pieces (other than the king and the pawns)
	bool has_pieces(bool color) const;

	// Returns the real value of a long-term king weakness parameter, depending on the castling options and how close they are
	int get_long_term_king_weakness(bool player, int current_weakness, int kingside_weakness, int queenside_weakness);

	// Returns the value of the bonuses tied to the open and semi-open files on the opposing king, were it on a certain file
	int get_open_files_on_opponent_king_at_column(bool player, int king_col) const;

	// Returns the value of the bonuses tied to the placement of the king, were it on a certain file
	int get_king_placement_weakness_at_column(bool player, Pos king_pos) const;

	// Returns the value of the bonuses tied to the placement of the king
	int get_king_placement_weakness(bool player);

	// Returns a map of every blocked pawn
	SquareMap get_blocked_pawns(bool color) const;

	// Takes a map of blocked pawns/pieces, updates it with the newly blocked pieces, and returns whether one or more pieces were added to it
	bool update_blocked_pieces(SquareMap& blocked_pieces, bool color, SquareMap opponent_controls) const;

	// Returns the map of every blocked piece
	SquareMap get_all_blocked_pieces(bool color, SquareMap opponent_controls) const;

	// Updates the map of every currently blocked piece (1) and newly unblocked piece (-1), and returns whether one or more pieces were added to or removed from the map
	bool get_blocked_and_unblocked_pieces(SquareMap &pieces_states, bool color, SquareMap opponent_controls) const;

	// Returns the map of the squares controlled by the pawns
	SquareMap get_pawns_controls(bool color) const;

	// Returns the real mobility of the pieces (short term)
	int get_short_term_piece_mobility(bool display = false) const;

	// Returns the virtual mobility of the pieces (long term)
	int get_long_term_piece_mobility(bool display = false) const;

	// Returns the value matching the safety of the queens of the given player
	int get_queen_safety(bool color) const;

	// Returns how quiet a position is: the number of available captures, of available checks and of available promotions
	int get_quietness();

	// Assigns the flags to the possible moves
	void assign_all_move_flags();

	// Switches the side to move
	void switch_trait();

	// Assigns the flags to a given move
	void assign_move_flags(Move *move);

	// Resets the bitboards to 0
	void reset_bitboards();

	// Updates the bitboards
	void update_bitboards();

	// Displays every bitboard
	void print_all_bitboards() const;

	// Updates the bitboards after a move played by White
	void update_bitboards_white(int row1, int col1, int row2, int col2, int p, int p_last, uint8_t promo_piece = 0) noexcept;

	// Updates the bitboards after a move played by Black
	void update_bitboards_black(int row1, int col1, int row2, int col2, int p, int p_last, uint8_t promo_piece = 0) noexcept;

	// Returns the number of passed pawns for a given colour
	int get_passed_pawns_count(bool color) const;

	// Returns the evaluation value tied to the passed pawns
	// TODO
	int get_passed_pawns_value(bool color) const;

	// Returns whether a move is irreversible (for the fast repetition detection)
	// Irreversible = pawn move, capture (including en passant), promotion, or a move losing the castling rights
	bool is_irreversible_move(const Move& move) const noexcept;

	// Returns whether a move changes (loses) the castling rights
	bool does_move_change_castling_rights(const Move& move) const noexcept;

	// TODO *** write a more generic piece_safety?

	// TODO *** faster move generation
};

// Returns whether two positions (in FEN form) are the same
bool equal_fen(const string&, const string&);

// Returns whether two positions (in FEN form) are the same (for the repetitions)
bool equal_positions(const Board&, const Board&);

// Returns the time the AI should spend on the next move (in ms), given a factor k and the times left
int time_to_play_move(int t1, int t2, float k = 0.05f);

// Displays every value of a bitboard
void print_bitboard(uint64_t bitboard);

// std::map<string, int> _positions_history = {
//     { "A", 1 },
//     { "B", 1 },
//     { "C", 2 }
// };

// Returns the UCT value
float uct(float, float, int, int);

// Text box
struct TextBox {
	float x;
	float y;
	float width;
	float height;
	string text;
	int value;
	bool active;
	float text_size;
	Color main_color;
	Color text_color;
	Font text_font;

	// Default constructor
	TextBox() {}

	TextBox(const float pos_x, const float pos_y, const float box_width, const float box_height, string initial_text, const int initial_value) :
		x(pos_x),
		y(pos_y),
		width(box_width),
		height(box_height),
		text(std::move(initial_text)),
		value(initial_value),
		active(false) {}

	void set_rect(const float pos_x, const float pos_y, const float box_width, const float box_height) {
		x = pos_x;
		y = pos_y;
		width = box_width;
		height = box_height;
	}
};

// Updates a text box
void update_text_box(TextBox& text_box);

// Draws a text box
void draw_text_box(const TextBox& text_box);

// Compares two moves to know which one to display first
//bool compare_move_arrows(int m1, int m2);
// 
// Returns the name of the square
string square_name(uint8_t i, uint8_t j);

// Returns the name of a piece
string piece_name(uint8_t piece);

// Returns the name of a piece
string short_piece_name(uint8_t piece);

// Returns the evaluation renormalized by the average score
string get_renormalized_evaluation(float avg_score, float winning_eval = 1, float winning_score = 0.70f);

// Returns the score of a WDL with a precision of 0.01
string score_string(float avg_score);
