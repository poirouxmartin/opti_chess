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
#include "useful_functions.h"

using namespace std;


// TODO: les utiliser
// Enumération des pièces
constexpr enum piece_type { none = 0, w_pawn = 1, w_knight = 2, w_bishop = 3, w_rook = 4, w_queen = 5, w_king = 6, b_pawn = 7, b_knight = 8, b_bishop = 9, b_rook = 10, b_queen = 11, b_king = 12 };

// Nombre de demi-coups avant de déclarer la partie nulle
constexpr uint8_t max_half_moves = 100;

// Nombre maximum de coups légaux par position estimé
// const int max_moves = 218;
constexpr uint8_t max_moves = 100; // ça n'arrivera quasi jamais que ça dépasse ce nombre

// Valeur d'un échec et mat
constexpr int mate_value = 1e8;

// Valeur d'un ply (double) dans la recherche de mat
constexpr int mate_ply = 1e5;

// Coups possibles pour un cavalier
constexpr int_fast8_t knight_directions[8][2] = { {1, 2}, {1, -2}, {-1, 2}, {-1, -2}, {2, 1}, {2, -1}, {-2, 1}, {-2, -1} };

// Coups rectilignes
constexpr int_fast8_t rect_directions[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };

// Coups diagonaux
constexpr int_fast8_t diag_directions[4][2] = { {1, 1}, {1, -1}, {-1, 1}, {-1, -1} };

// Coups dans toutes les directions
constexpr int_fast8_t all_directions[8][2] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1}, {-1, 0}, {0, -1}, {0, 1}, {1, 0} };

// ------------------- Couleur -------------------
constexpr bool is_white(uint8_t piece) noexcept {
	return piece && piece <= w_king;
}

constexpr bool is_black(uint8_t piece) noexcept {
	return piece >= b_pawn;
}

// ------------------- Type -------------------
constexpr inline bool is_pawn(uint8_t piece) noexcept { return piece == w_pawn || piece == b_pawn; }
constexpr inline bool is_knight(uint8_t piece) noexcept { return piece == w_knight || piece == b_knight; }
constexpr inline bool is_bishop(uint8_t piece) noexcept { return piece == w_bishop || piece == b_bishop; }
constexpr inline bool is_rook(uint8_t piece) noexcept { return piece == w_rook || piece == b_rook; }
constexpr inline bool is_queen(uint8_t piece) noexcept { return piece == w_queen || piece == b_queen; }
constexpr inline bool is_king(uint8_t piece) noexcept { return piece == w_king || piece == b_king; }

// ------------------- Mouvement -------------------
constexpr bool is_rectilinear(uint8_t piece) noexcept {
	return piece == w_rook || piece == b_rook || piece == w_queen || piece == b_queen;
}

constexpr bool is_diagonal(uint8_t piece) noexcept {
	return piece == w_bishop || piece == b_bishop || piece == w_queen || piece == b_queen;
}

constexpr bool is_sliding(uint8_t piece) noexcept {
	return (piece >= w_bishop && piece <= w_queen) || (piece >= b_bishop && piece <= b_queen);
}

// ------------------- Alliés -------------------
constexpr bool is_ally(uint8_t piece, bool player_white) noexcept {
	return piece && ((piece <= w_king) == player_white);
}

constexpr bool is_enemy(uint8_t piece, bool player_white) noexcept {
	return piece && ((piece <= w_king) != player_white);
}

// TODO *** il reste 1 bit de libre, à utiliser pour un flag supplémentaire si besoin
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
	uint8_t start_row : 3;
	uint8_t start_col : 3;
	uint8_t end_row : 3;
	uint8_t end_col : 3;
	uint8_t flags;

	// --- Flag Accessors ---
	inline bool is_null() const { return flags & IS_NULL; }
	inline bool is_capture() const { return flags & IS_CAPTURE; }
	inline bool is_promotion() const { return flags & IS_PROMOTION; }
	inline bool is_check() const { return flags & IS_CHECK; }
	inline bool is_checkmate() const { return flags & IS_MATE; }
	inline bool has_flags() const { return flags & FLAGS_EVALUATED; }

	inline uint8_t game_result() const { return (flags & RESULT_MASK) >> 5; }

	inline void set_game_result(uint8_t result) {
		flags = (flags & ~RESULT_MASK) | ((result & 0b11) << 5);
	}

	// --- Helpers ---
	inline void set_flag(MoveFlags f) { flags |= f; }
	inline void clear_flag(MoveFlags f) { flags &= ~f; }
	inline bool has_flag(MoveFlags f) const { return flags & f; }

	inline void set_result(uint8_t result) {
		flags = (flags & ~RESULT_MASK) | ((result & 0x03) << 5);
	}

	inline uint8_t get_result() const {
		return (flags & RESULT_MASK) >> 5;
	}

	// --- Comparisons ---
	inline bool operator==(const Move& other) const {
		return start_row == other.start_row &&
			start_col == other.start_col &&
			end_row == other.end_row &&
			end_col == other.end_col;
	}

	inline bool operator<(const Move& other) const {
		if (start_row != other.start_row) return start_row < other.start_row;
		if (start_col != other.start_col) return start_col < other.start_col;
		if (end_row != other.end_row)   return end_row < other.end_row;
		return end_col < other.end_col;
	}

	// Renvoie si c'est un coup nul
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
			uint16_t key = (m.start_row << 0) | (m.start_col << 3) | (m.end_row << 6) | (m.end_col << 9);
			return hash<uint16_t>()(key);
		}
	};
}

// Droits de roque (pour optimiser la place mémoire)
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

// Tests pour une représentation plus compacte d'un plateau
// TODO *** à utiliser
#pragma pack(push, 1)
struct Piece
{
	int type : 3;
	bool color : 1;

	// Constructeurs
	Piece() : type(0), color(0) {}
	Piece(int type, bool color) : type(type), color(color) {}

	// Opérateurs

	// Méthodes
};

// TODO: utiliser!!
// Ligne du plateau d'échec (horizontale)
// 40 bytes (Minimum possible pour une représentation board-centric : 32 bytes)
struct Array
{
	Piece pieces[8][8];
};
#pragma pack(pop)

// Position sur le plateau
struct Pos
{
	int row : 4; // TODO: 3 suffit?
	int col : 4;

	// Opérateur d'égalité
	bool operator== (const Pos& other) const {
		return (row == other.row) && (col == other.col);
	}

	// Renvoie la notation de la case
	// TODO *** mettre des constantes statiques pour que ça soit précalculé
	string square() const {
		return string(1, 'a' + col) + string(1, '1' + row);
	}
};

// Map d'un plateau (pour stocker les cases controllées, etc...)
struct SquareMap
{
	int _array[8][8];

	// Constructeurs
	SquareMap() {
		for (uint8_t row = 0; row < 8; row++) {
			for (uint8_t col = 0; col < 8; col++) {
				_array[row][col] = 0;
			}
		}
	}

	// Opérateurs

	// Soustraction
	SquareMap operator- (const SquareMap& other) const {
		SquareMap result;
		for (int row = 0; row < 8; row++) {
			for (int col = 0; col < 8; col++) {
				result._array[row][col] = _array[row][col] - other._array[row][col];
			}
		}
		return result;
	}


	// Méthodes

	// Affichage de façon alignée
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

	// Constructeurs
	BoolMap() {
		for (uint8_t row = 0; row < 8; row++) {
			for (uint8_t col = 0; col < 8; col++) {
				_array[row][col] = false;
			}
		}
	}

	// Opérateurs
	// 
	// Soustraction
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
	float win_chance; // Entre 0.0f et 1.0f
	float draw_chance; // Entre 0.0f et 1.0f
	float lose_chance; // Entre 0.0f et 1.0f

	// Affichage
	string to_string() const {
		return "WDL: " + int_to_round_string(1000 * win_chance) + "/" + int_to_round_string(1000 * draw_chance) + "/" + int_to_round_string(1000 * lose_chance);
	}

	void print() const {
		cout << "WDL: " << (int)(1000 * win_chance) << "/" << (int)(1000 * draw_chance) << "/" << (int)(1000 * lose_chance) << endl;
	}
};

// Fins de partie
static constexpr enum game_termination { unterminated = 0, white_win = 1, draw = 2, black_win = 3 };

// Directions
struct Direction {
	int8_t d_row;
	int8_t d_col;
};

// Types de direction
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

// Case clouée ou non, et dans quelle direction?
struct PinnedSquare {
	bool pinned = false;
	Direction dir;
};

// Tableau des clouages de la position
struct PinsMap {
	PinnedSquare pins[8][8];

	// Affichage de façon alignée
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

// Les directions sont-elles alignées
inline bool is_aligned(int d_row, int d_col, Direction dir) {
	return (d_row * dir.d_col == d_col * dir.d_row);
}

void print_controls(uint16_t controls);

// Disposition de base des cases autour du roi
// 0   1   2   3   4
// 5   6   7   8   9
// 10  11  12  13  14

// On utilise un entier 16 bits pour stocker les contrôles autour du roi
inline uint16_t control_bit(int8_t rel_row, int8_t rel_col) {
	return 1u << ((rel_row + 1) * 5 + (rel_col + 2));
}

// Renvoie si une case autour du roi est contrôlée
inline bool is_controlled_around_king(uint16_t controls, int8_t rel_row, int8_t rel_col) {
	return (controls & control_bit(rel_row, rel_col)) != 0;
}

// row et col doivent être dans [0,7]
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


// Plateau
class Board {
public:

	// Attributs

	// Plateau
	// 64 bytes
	uint8_t _array[8][8]{	{	w_rook,		w_knight,	w_bishop,   w_queen,    w_king,		w_bishop,   w_knight,   w_rook	},
							{   w_pawn,		w_pawn,		w_pawn,		w_pawn,		w_pawn,		w_pawn,		w_pawn,		w_pawn	},
							{	none,		none,		none,		none,		none,		none,		none,		none	},
							{	none,		none,		none,		none,		none,		none,		none,		none	},
							{	none,		none,		none,		none,		none,		none,		none,		none	},
							{	none,		none,		none,		none,		none,		none,		none,		none	},
							{   b_pawn,		b_pawn,		b_pawn,		b_pawn,		b_pawn,		b_pawn,		b_pawn,		b_pawn	},
							{   b_rook,		b_knight,   b_bishop,   b_queen,    b_king,		b_bishop,   b_knight,   b_rook	} };

	//Array _array; // TODO utiliser

	// Bitboard!! (TODO)
	// none -> w_pawn -> b_king
	uint64_t _bitboards[13];

	// Pièces blanches, noires, et toutes
	uint64_t _occupancies[3];

	// TODO *** rajouter des masks pour les contrôles
	// TODO *** magic bitboards à utiliser

	// TODO *** Optionnel : roi en cache (remplacera les _white_king_pos ?)
	//int _square_king[2];

	// Coups possibles
	// Nombre max de coups légaux dans une position : 218

	// TODO: réduire le nombre de bytes utilisés
	// 200 bytes
	Move _moves[max_moves];

	// Les coups sont-ils actualisés? Si non : -1, sinon, _got_moves représente le nombre de coups jouables
	// En supposant que le nombre de coups n'excède pas 127
	// 1 byte
	int_fast8_t _got_moves = -1;

	// Les flags des coups ont-ils été assignés?
	//bool _moves_flags_assigned = false;

	// Les coups sont-ils triés?
	bool _sorted_moves = false;

	// Tour du joueur (true pour les blancs, false pour les noirs)
	bool _player = true;

	// Peut-être utile pour les optimisations?
	int _evaluation = 0;

	// Droits de roque
	CastlingRights _castling_rights;

	// Colonne d'en passant
	int_fast8_t _en_passant_col = -1;

	// Nombre de demi-coups (depuis le dernier déplacement de pion, ou la dernière capture, et reste nul à chacun de ces coups)
	uint8_t _half_moves_count = 0;

	// Nombre de coups de la partie
	uint_fast16_t _moves_count = 1;

	// Plateau libre ou actif? (pour le buffer)
	bool _is_active = false;

	// Est-ce que le plateau a été évalué?
	bool _evaluated = false;

	// Pour l'affichage 
	// FIXME: ça prend de la place, à voir si on peut s'en passer
	int _static_evaluation = 0;

	// Avancement de la partie
	// TODO *** à mettre dans les noeuds plutôt que plateaux?
	float _adv = 0.0f;
	bool _advancement = false;

	// Est-ce que le calcul de game over a déjà été fait?
	bool _game_over_checked = false;
	int_fast8_t _game_over_value = unterminated;

	// On stocke les positions des rois
	Pos _white_king_pos = { 0, 4 };
	Pos _black_king_pos = { 7, 4 };

	// Est-ce qu'on a affiché les composantes du plateau?
	bool _displayed_components = false;

	// Clé de Zobrist de la position
	uint_fast64_t _zobrist_key = 0;

	// Historique des positions
	vector<uint64_t> _positions_history = {};
	//unordered_map<uint_fast64_t, int> _positions_history = {};

	// WDL
	// TODO *** faudra les retirer à terme...
	WDL _wdl;

	// Incertitude
	float _uncertainty = 0.0f;

	// Winnable
	float _winnable_white = 1.0f;
	float _winnable_black = 1.0f;


	// Constructeur par défaut
	Board();

	// Constructeur de copie
	Board(const Board&, bool full = false, bool copy_history = false);

	// Opérateur d'égalité (compare seulement le placement des pièces, droits de roques, et nombre de coups)
	bool operator== (const Board&) const;

	// Fonction qui copie le strict minimum de la position
	void minimal_copy_data(const Board& b);

	// Fonction qui copie les attributs d'un plateau (full copy: on copie tout)
	void copy_data(const Board&, bool full = false, bool copy_history = false);

	// Fonction qui ajoute un coup dans la liste de coups
	bool add_move(const Move move, uint8_t& iterator, const uint8_t piece) noexcept;

	// Fonction qui ajoute tous les coups de roi, en tenant compte des contrôles et des roques
	bool add_king_moves(const bool player, const Pos king_pos, const uint16_t controls_around_king, uint8_t &iterator, const bool kingside_castle_check, const bool queenside_castle_check) noexcept;

	// Fonction qui ajoute les coups d'un pion, en tenant compte des clouages, et des échecs
	bool add_pawn_moves(const bool player, const uint8_t row, const uint8_t col, uint8_t &iterator, const PinnedSquare &pin, const bool in_check, const uint64_t &interposition_mask) noexcept;

	// Fonction qui ajoute les coups d'un cavalier, en tenant compte des clouages, et des échecs
	bool add_knight_moves(const bool player, const uint8_t row, const uint8_t col, uint8_t &iterator, const PinnedSquare &pin, const bool in_check, const uint64_t& interposition_mask) noexcept;

	// Fonction qui ajoute les coups d'une pièce rectiligne, en tenant compte des clouages, et des échecs
	bool add_rect_moves(const bool player, const uint8_t row, const uint8_t col, uint8_t &iterator, const PinnedSquare &pin, const bool in_check, const uint64_t& interposition_mask) noexcept;

	// Fonction qui ajoute les coups d'une pièce diagonale, en tenant compte des clouages, et des échecs
	bool add_diag_moves(const bool player, const uint8_t row, const uint8_t col, uint8_t &iterator, const PinnedSquare &pin, const bool in_check, const uint64_t& interposition_mask) noexcept;

	// Fonction qui renvoie la liste des coups légaux
	bool get_moves() noexcept;

	// Fonction qui renvoie la map des contrôles autour du roi du joueur donné (et les cases pour le roque, si besoin)
	uint16_t get_controls_around_king(Pos king_pos, bool player, bool kingside_castle_check, bool queenside_castle_check) const noexcept;

	// Fonction qui renvoie une des pièces qui la case (si plus d'une)
	PieceSquare get_square_attacker(Pos square, int* n_attackers) const noexcept;

	// Retourne un bitboard des cases d'interposition entre le roi et l'attaquant (incluant l'attaquant)
	uint64_t get_interpose_mask(Pos king_pos, const PieceSquare& attacker) const noexcept;

	// Fonction qui renvoie la liste des clouages pour le joueur donné
	PinsMap get_pins(bool player) const noexcept;

	// Fonction qui dit s'il y'a échec
	bool in_check(bool update_king_pos = true) noexcept;

	// Fonction qui affiche la liste des coups
	void display_moves();

	// Fonction qui joue un coup
	void make_move(const Move& move, const bool pgn = false, const bool add_to_history = false) noexcept;

	inline void make_move_fast(const Move move);

	// Fonction qui annule un coup
	void unmake_move(Move move, uint8_t p1, uint8_t p2, int en_passant_col, int prev_half_count, bool k_castle, bool q_castle, bool is_castle, bool is_promotion, bool is_en_passant);

	// Fonction qui renvoie l'avancement de la partie (0 = début de partie, 1 = fin de partie)
	void game_advancement();

	// Fonction qui compte le matériel sur l'échiquier et renvoie sa valeur
	int count_material(const Evaluator* e = nullptr, float closed_factor = 0.0f) const;

	// Fonction qui compte les paires de fous et renvoie la valeur
	int count_bishop_pairs() const;

	// Fonction qui calcule et renvoie la valeur de positionnement des pièces sur l'échiquier
	int pieces_positioning(const Evaluator* eval = nullptr) const;

	// Fonction qui évalue la position à l'aide d'heuristiques
	bool evaluate(Evaluator* eval = nullptr, bool display = false, Network* n = nullptr, bool check_game_over = false);

	// Fonction qui récupère le plateau d'un FEN
	void from_fen(string);

	// Fonction qui renvoie le FEN du tableau
	string to_fen() const;

	// Fonction qui renvoie le gagnant si la partie est finie (-1/1, et 2 pour nulle), et 0 sinon
	int game_over(int max_repetitions);

	// Fonction qui renvoie le gagnant si la partie est finie (-1/1, et 2 pour nulle), et 0 sinon -> et stocke la valeur dans _game_over_value
	int is_game_over(int max_repetitions = 2);

	// Fonction qui renvoie le label d'un coup
	string move_label(Move move, bool use_uft8 = false);

	// Fonction qui affiche un texte dans une zone donnée
	static void draw_text_rect(const string&, float, float, float, float, float);

	// Fonction qui joue le son d'un coup
	void play_move_sound(Move) const;

	// Fonction qui réinitialise le plateau dans son état de base (pour le buffer)
	void reset_board(bool display = false);

	// Fonction qui calcule et renvoie la valeur correspondante à la sécurité des rois
	int get_king_safety(float display_factor = 0.0f);

	// Fonction qui dit si une pièce est capturable par l'ennemi (pour les affichages GUI)
	bool is_capturable(int, int);

	// Fonction qui affiche le PGN
	void display_pgn() const;

	// Fonction qui renvoie selon l'évaluation si c'est un mat ou non
	int is_eval_mate(int) const;

	// Fonction qui génère le livre d'ouvertures
	void generate_opening_book(int nodes = 100000);

	// Fonction qui renvoie une représentation simple et rapide de la position
	string simple_position() const;

	// Fonction qui calcule la structure de pions et renvoie sa valeur
	int get_pawn_structure(float display_factor = 0.0f);

	// Fonction qui calcule la résultante des attaques et des défenses et la renvoie
	float get_attacks_and_defenses() const;

	// Fonction qui calcule et renvoie l'opposition des rois (en finales de pions)
	int get_kings_opposition();

	// Fonction qui renvoie le type de pièce sélectionnée
	uint8_t selected_piece() const;

	// Fonction qui renvoie le type de pièce où la souris vient de cliquer
	uint8_t clicked_piece() const;

	// Fonction qui renvoie si la pièce sélectionnée est au joueur ayant trait ou non
	bool selected_piece_has_trait() const;

	// Fonction qui renvoie si la pièce cliquée est au joueur ayant trait ou non
	bool clicked_piece_has_trait() const;

	// Fonction qui remet les compteurs de temps "à zéro" (temps de base)
	static void reset_timers();

	// Fonction qui remet le plateau dans sa position initiale
	void restart();

	// Fonction qui renvoie la différence matérielle entre les deux camps
	int material_difference() const;

	// Fonction qui réinitialise les composantes de l'évaluation
	void reset_eval();

	// Fonction qui compte les tours sur les colonnes ouvertes et semi-ouvertes et renvoie la valeur
	int get_sliders_on_open_file() const;

	// Fonction qui calcule la valeur des cases controllées sur l'échiquier
	int get_square_controls() const;

	// Fonction qui fait un tri rapide des coups (en plaçant les captures en premier)
	bool sort_moves();

	// Fonction qui fait cliquer le coup m
	bool click_m_move(Move i, bool orientation) const;

	// Fonction qui récupère et renvoie la couleur du joueur au trait (1 pour les blancs, -1 pour les noirs)
	int get_color() const;

	// Fonction qui calcule et renvoie l'avantage d'espace
	int get_space() const;

	// Fonction qui calcule et renvoie une évaluation des vis-à-vis
	int get_alignments() const;

	// Fonction qui met à jour la position des rois
	bool update_kings_pos();

	// Fonction qui renvoie l'activité des pièces
	int get_piece_activity() const;

	// Fonction qui renvoie la map correspondante au nombre de contrôles pour chaque case de l'échiquier pour le joueur blanc
	SquareMap get_white_controls_map() const;

	// Fonction qui renvoie la map correspondante au nombre de contrôles pour chaque case de l'échiquier pour le joueur noir
	SquareMap get_black_controls_map() const;

	// Fonction qui ajoute à une map les contrôles d'une pièce
	bool add_piece_controls(SquareMap* map, int i, int j, int piece) const;

	// Fonction qui renvoie la mobilité virtuelle d'un roi
	int get_king_virtual_mobility(bool color);

	// Fonction qui renvoie le nombre d'échecs 'safe' dans la position pour les deux joueurs
	int get_checks_value(SquareMap white_controls, SquareMap black_controls, bool color);

	// Fonction qui renvoie la vitesse de génération des coups
	int moves_generation_benchmark(uint8_t depth, bool main_call = true);

	// Fonction qui renvoie la valeur des fous en fianchetto
	int get_fianchetto_value() const;

	// Fonction qui renvoie si la case est controlée par un joueur
	bool is_controlled(int square_i, int square_j, bool player) const;

	// Fonction qui calcule et renvoie la valeur des menaces d'avance de pion
	int get_pawn_push_threats() const;

	// Fonction qui calcule et renvoie la proximité du roi avec les pions
	int get_king_proximity();

	// Fonction qui calcule et renvoie l'activité/mobilité des tours
	int get_rook_activity() const;

	// Fonction qui calcule et renvoie la valeur des pions qui bloquent les fous
	int get_bishop_pawns() const;

	// Fonction qui renvoie la valeur des faiblesses long terme du bouclier de pions
	int get_pawn_shield();

	// Fonction qui renvoie la caleur des cases faibles
	int get_weak_squares(bool color, bool around_king = false);

	// Fonction qui convertit un coup en sa notation algébrique
	string algebric_notation(Move move) const;

	// Fonction qui convertit une notation algébrique en un coup
	Move move_from_algebric_notation(string notation);

	// Fonction qui renvoie la valeur de la distance à la possibilité de roque
	int get_castling_distance() const;

	// Fonction qui génère la clé de Zobrist du plateau
	void get_zobrist_key();

	// Fonction qui renvoie à quel point la partie est gagnable (de 0 à 1), pour une couleur donnée
	float get_winnable(bool color, float position_nature) const;

	// Fonction qui calcule les valeurs de possibilités de gain pour chaque côté
	void get_winnable_values(float position_nature = 0.0f);

	// Fonction qui renvoie l'activité des fous sur les diagonales
	int get_bishop_activity() const;

	// Fonction qui renvoie si un coup est légal ou non
	bool is_legal(Move move);

	// Fonction qui reset l'historique des positions
	void reset_positions_history();

	// Fonction qui renvoie combien de fois la position actuelle a été répétée
	int repetition_count();

	// Affiche l'histoirque des positions (les clés de Zobrist)
	void display_positions_history() const;

	// Quiescence search pour l'algo de GrogrosZero
	//int grogros_quiescence(Evaluator* eval, int alpha = -2147483647, int beta = 2147483647, int depth = 4, bool explore_checks = true, bool main_player = true);

	// Fonction qui renvoie l'affichage de l'évaluation
	string evaluation_to_string(int eval) const;

	// Fonction qui renvoie l'évaluation des pièces enfermées
	int get_trapped_pieces() const;

	// Fonction qui ajuste les valeurs des pièces (malus/bonus), en fonction du type de position
	int get_updated_piece_values() const;

	// Fonction qui renvoie la nature de la position de manière chiffrée: 0 = ouverte, 1 = fermée
	float get_position_nature() const;

	// Fonction qui renvoie la valeur des bonus liés aux colonnes ouvertes et semi-ouvertes sur le roi adverse
	int get_open_files_on_opponent_king(bool color);

	// Fonction qui renvoie la valeur des bonus liés aux diagonales ouvertes et semi-ouvertes sur le roi adverse
	int get_open_diagonals_on_opponent_king(bool color);

	// Fonction qui renvoie le nombre de cases de retrait pour le roi
	int get_king_escape_squares(bool color);

	// Fonction qui renvoie une valeur correspondante aux pièces attaquant le roi adverse
	int get_king_attackers(bool color);

	// Fonction qui renvoie une valeur correspondante aux pièces défendant le roi
	int get_king_defenders(bool color);

	// Fonction qui renvoie un bonus correspondant au pawn storm sur le roi adverse à une colonne donnée
	int get_pawn_storm_at_col(bool color, uint8_t king_row, uint8_t king_col) const;

	// Fonction qui renvoie la puissance de protection de la structure de pions du roi
	int get_pawn_storm(bool color);

	// Fonction qui renvoie un bonus d'activité pour les cavaliers
	int get_knight_activity() const;

	// Fonction qui renvoie la puissance de protection de la structure de pions du roi
	int get_pawn_shield_protection(bool color, float opponent_attacking_potential, int space);

	// Fonction qui renvoie la puissance de protection de la structure de pions du roi, s'il est sur la colonne donnée
	int get_pawn_shield_protection_at_column(bool color, int column, float opponent_attacking_potential, bool add_column_bonus = false, int space = 0);

	// Fonction qui calcule tous les coups à une certaine profondeur, et renvoie le nombre de noeuds total
	long long int count_nodes_at_depth(int depth, bool display = true, bool main = false);

	// Version parallelisée
	long long int count_nodes_at_depth_parallelized(int depth, bool display, bool main = false);

	// Fonction qui renvoie si le nombre de noeuds calculés pour une position à une certaine profondeur correspond au nombre attendu
	bool validate_nodes_count_at_depth(string fen, int depth, vector<long long int> expected_nodes, bool display = false, bool display_full = false, bool parallel = false);

	// Fonction qui execute une même validation plusieurs fois, et affiche le temps moyen, min, max, écart-type...
	void benchmark_nodes_count_at_depth(string fen, int depth, vector<long long int> expected_nodes, int iterations = 10, bool display = false, bool parallel = false);

	// Fonction test: nouvelle mobilité des pièces
	int get_piece_mobility(bool display = false) const;

	// Fonction qui renvoie si le pion peut bouger
	bool pawn_can_move(uint8_t row, uint8_t col, bool color) const;

	// Fonction qui renvoie l'incertiude de la position
	float get_uncertainty(int material_eval, int winning_eval = 110);

	// Fonction qui renvoie le WDL de la position
	void get_WDL(int winning_eval = 110, float beta = 0.75f);

	// Fonction qui renvoie l'espérance de gain (en points) de la position (fondé sur les probas de WDL) pour les blancs
	float get_average_score(float draw_score = 0.5f) const;

	// Fonction qui inverse les couleurs des joueurs (y compris le trait)
	void switch_colors();

	// Fonction qui itère sur la map des distances à partir d'une position donnée, et renvoie les nouvelles cases contrôlées
	vector<Pos> get_next_king_squares(SquareMap& map, Pos start_pos, int distance, bool color) const;

	// Fonction qui renvoie une map des distances entre le roi et chaque point de l'échiquier (nombre de coups pour y arriver, en fonction des contrôles actuels du plateau)
	SquareMap get_king_squares_distance(bool color);

	// Fonction qui renvoie la faiblesse sur les rangées du roi
	int get_king_row_weakness(bool color);

	// Fonction qui renvoie la valeur de centralisation du roi en fin de partie
	int get_king_centralization(bool color);

	// Fonction qui renvoie la valeur des pièces non défendues
	int get_unprotected_pieces(bool color) const;

	// Fonction qui renvoie si le roi est dans le carré du pion
	bool in_king_square(Pos pos, bool color);

	// Fonction qui renvoie si on est en finale de pions
	bool is_pawn_endgame() const;

	// Fonction qui renvoie si le joueur a encore des pièces (autres que roi et pions)
	bool has_pieces(bool color) const;

	// Fonction qui renvoie la réelle valeur d'un paramètre de faiblesse long terme du roi, en fonction des possibilités et proximités des roques
	int get_long_term_king_weakness(bool player, int current_weakness, int kingside_weakness, int queenside_weakness);

	// Fonction qui renvoie la valeur des bonus liés aux colonnes ouvertes et semi-ouvertes sur le roi adverse, s'il était sur une certaine colonne
	int get_open_files_on_opponent_king_at_column(bool player, int king_col) const;

	// Fonction qui renvoie la valeur des bonus liés au placement du roi, s'il était sur une certaine colonne
	int get_king_placement_weakness_at_column(bool player, Pos king_pos) const;

	// Fonction qui renvoie la valeur des bonus liés au placement du roi
	int get_king_placement_weakness(bool player);

	// Fonction qui renvoie une map de tous les pions bloqués
	SquareMap get_blocked_pawns(bool color) const;

	// Fonction qui prend une map de pions/pièces bloquées, la met à jour en fonction des nouvelles pièces bloquées, et renvoie si une ou plusieurs pièces y ont été ajoutées
	bool update_blocked_pieces(SquareMap& blocked_pieces, bool color) const;

	// Fonction qui renvoie toutes la map de toutes les pièces bloquées
	SquareMap get_all_blocked_pieces(bool color) const;

	// Fonction qui renvoie la map des cases controlées par les pions
	SquareMap get_pawns_controls(bool color) const;

	// Fonction qui renvoie la mobilité réelle des pièces (court terme)
	int get_short_term_piece_mobility(bool display = false) const;

	// Fonction qui renvoie la mobilité virtuelle des pièces (long terme)
	int get_long_term_piece_mobility(bool display = false) const;

	// Fonction qui renvoie la valeur correspondante à la sécurité des dames du joueur donné
	int get_queen_safety(bool color) const;

	// Fonction qui renvoie à quel point une position est quiet ou non: renvoie le nombre de captures disponibles, d'échecs disponibles et de promotions disponibles
	int get_quietness();

	// Fonction qui assigne les flags aux coups possibles
	void assign_all_move_flags();

	// Fonction qui change le trait du joueur
	void switch_trait();

	// Fonction qui assigne les flags à un coup donné
	void assign_move_flags(Move *move) const;

	// Fonction qui remet à 0 les bitboards
	void reset_bitboards();

	// Fonction qui met à jour les bitboards
	void update_bitboards();

	// Fonction qui affiche tous les bitboards
	void print_all_bitboards() const;

	// Fonction qui met à jour les bitboards en fonction d'un coup joué pour les blancs
	void update_bitboards_white(int row1, int col1, int row2, int col2, int p, int p_last) noexcept;

	// Fonction qui met à jour les bitboards en fonction d'un coup joué pour les noirs
	void update_bitboards_black(int row1, int col1, int row2, int col2, int p, int p_last) noexcept;


	// TODO *** faire un piece_safety plus générique?

	// TODO *** génération plus rapide des coups
};

// Fonction qui renvoie si deux positions (en format FEN) sont les mêmes
bool equal_fen(const string&, const string&);

// Fonction qui renvoie si deux positions (en format FEN) sont les mêmes (pour les répétitions)
bool equal_positions(const Board&, const Board&);

// Fonction qui renvoie le temps que l'IA doit passer sur le prochain coup (en ms), en fonction d'un facteur k, et des temps restant
int time_to_play_move(int t1, int t2, float k = 0.05f);

// Fonction qui affiche toutes les valeurs d'un bitboard
void print_bitboard(uint64_t bitboard);

// std::map<string, int> _positions_history = {
//     { "A", 1 },
//     { "B", 1 },
//     { "C", 2 }
// };

// Fonction qui renvoie la valeur UCT
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

	// Constructeur par défaut
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

// Fonction qui met à jour une text box
void update_text_box(TextBox& text_box);

// Fonction qui dessine une text box
void draw_text_box(const TextBox& text_box);

// Fonction qui compare deux coups pour savoir lequel afficher en premier
//bool compare_move_arrows(int m1, int m2);
// 
// Fonction qui renvoie le nom de la case
string square_name(uint8_t i, uint8_t j);

// Fonction qui renvoie le nom d'une pièce
string piece_name(uint8_t piece);

// Fonction qui renvoie l'espérance de gain d'un WDL
float get_average_score(WDL wdl, float draw_score = 0.5f);

// Fonction qui renvoie l'évaluation re-normalisée en fonction du score moyen
string get_renormalized_evaluation(float avg_score, float winning_eval = 1, float winning_score = 0.70f);

// Fonction qui renvoie le score d'un WDL avec une précision de 0.01
string score_string(float avg_score);