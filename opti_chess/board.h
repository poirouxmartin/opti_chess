#pragma once
#include <iostream>
#include <vector>
#include <execution>
#include <array>
#include <string>
#include "evaluation.h"
#include "neural_network.h"
#include <vector>
#include <map>
#include <cstdint>
#include "raylib.h"
#include <iomanip>
#include "useful_functions.h"
//#include "zobrist.h"

using namespace std;


// TODO: les utiliser
// Enumération des pièces
enum piece_type { none = 0, w_pawn = 1, w_knight = 2, w_bishop = 3, w_rook = 4, w_queen = 5, w_king = 6, b_pawn = 7, b_knight = 8, b_bishop = 9, b_rook = 10, b_queen = 11, b_king = 12 };

// Nombre de demi-coups avant de déclarer la partie nulle
constexpr int max_half_moves = 100;

// Nombre maximum de coups légaux par position estimé
// const int max_moves = 218;
constexpr int max_moves = 100; // ça n'arrivera quasi jamais que ça dépasse ce nombre

// Valeur d'un échec et mat
constexpr int mate_value = 1e8;

// Valeur d'un ply (double) dans la recherche de mat
constexpr int mate_ply = 1e5;

// Coups possibles pour un cavalier
constexpr int knight_moves[8][2] = { {1, 2}, {1, -2}, {-1, 2}, {-1, -2}, {2, 1}, {2, -1}, {-2, 1}, {-2, -1} };

// Coups rectilignes
constexpr int rect_moves[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };

// Coups diagonaux
constexpr int diag_moves[4][2] = { {1, 1}, {1, -1}, {-1, 1}, {-1, -1} };

// Coups dans toutes les directions
constexpr int all_directions[8][2] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1}, {-1, 0}, {0, -1}, {0, 1}, {1, 0} };

// Renvoie si la pièce est blanche
static bool is_white(uint_fast8_t piece) {
	return piece >= w_pawn && piece <= w_king;
}

// Renvoie si la pièce est noire
static bool is_black(uint_fast8_t piece) {
	return piece >= b_pawn && piece <= b_king;
}

// Renvoie si la pièce est un pion
static bool is_pawn(uint_fast8_t piece) {
	return piece == w_pawn || piece == b_pawn;
}

// Renvoie si la pièce est un cavalier
static bool is_knight(uint_fast8_t piece) {
	return piece == w_knight || piece == b_knight;
}

// Renvoie si la pièce est un fou
static bool is_bishop(uint_fast8_t piece) {
	return piece == w_bishop || piece == b_bishop;
}

// Renvoie si la pièce est une tour
static bool is_rook(uint_fast8_t piece) {
	return piece == w_rook || piece == b_rook;
}

// Renvoie si la pièce est une dame
static bool is_queen(uint_fast8_t piece) {
	return piece == w_queen || piece == b_queen;
}

// Renvoie si la pièce est un roi
static bool is_king(uint_fast8_t piece) {
	return piece == w_king || piece == b_king;
}

// Renvoie si la pièce a un mouvement rectiligne
static bool is_rectilinear(uint_fast8_t piece) {
	return is_rook(piece) || is_queen(piece);
}

// Renvoie si la pièce a un mouvement diagonal
static bool is_diagonal(uint_fast8_t piece) {
	return is_bishop(piece) || is_queen(piece);
}




// Coup (défini par ses coordonnées)
// TODO : l'utiliser !!
// Utilise 2 bytes (16 bits)
// On peut utiliser 2 bits en plus, car on en utilise seulement 14 là (promotion flag -> sur 4 bits, et on retire capture flag?) (castling flag * 2? en passant?)
// check flag?
// TODO : Faut-il mettre la pièce concérnée? ça éviterait de devoir la rechercher dans le plateau (coûteux)
typedef struct Move {
	uint_fast8_t i1 : 3;
	uint_fast8_t j1 : 3;
	uint_fast8_t i2 : 3;
	uint_fast8_t j2 : 3;

	//Pos init; utiliser ça?
	//bool capture_flag : 1;
	//bool promotion_flag : 1;
	bool is_null : 1;
	// Reste 2 bytes à utiliser : check? castling? en passant? is null?

	bool operator== (const Move& other) const {
		return (i1 == other.i1) && (j1 == other.j1) && (i2 == other.i2) && (j2 == other.j2);
	}

	// Il faut pouvoir trier les coups dans un dictionnaire
	bool operator<(const Move& other) const {
		if (i1 != other.i1) return i1 < other.i1;
		if (j1 != other.j1) return j1 < other.j1;
		if (i2 != other.i2) return i2 < other.i2;
		return j2 < other.j2;
	}

	// Fonction de hash
	size_t hash() const {
		size_t hashValue = 0;
		hashValue ^= std::hash<uint_fast8_t>()(i1);
		hashValue ^= std::hash<uint_fast8_t>()(j1) << 1;
		hashValue ^= std::hash<uint_fast8_t>()(i2) << 2;
		hashValue ^= std::hash<uint_fast8_t>()(j2) << 3;
		return hashValue;
	}


	void display() const
	{
		cout << "(" << static_cast<int>(i1) << ", " << static_cast<int>(j1) << ") -> (" << static_cast<int>(i2) << ", " << static_cast<int>(j2) << ")" << endl;
	}

	string to_string() const
	{
		return "(" + std::to_string(i1) + ", " + std::to_string(j1) + ") -> (" + std::to_string(i2) + ", " + std::to_string(j2) + ")";
	}

	// Renvoie si c'est un coup nul
	bool is_null_move() const {
		return (is_null || (i1 == 0 && j1 == 0 && i2 == 0 && j2 == 0));
	}
};


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
	string square() const {
		return string(1, 'a' + col) + string(1, '1' + row);
	}
};

// Map d'un plateau (pour stocker les cases controllées, etc...)
struct Map
{
	int _array[8][8];

	// Constructeurs
	Map() {
		for (uint_fast8_t i = 0; i < 8; i++) {
			for (uint_fast8_t j = 0; j < 8; j++) {
				_array[i][j] = 0;
			}
		}
	}

	// Opérateurs

	// Soustraction
	Map operator- (const Map& other) const {
		Map result;
		for (int i = 0; i < 8; i++) {
			for (int j = 0; j < 8; j++) {
				result._array[i][j] = _array[i][j] - other._array[i][j];
			}
		}
		return result;
	}


	// Méthodes

	// Affichage de façon alignée
	void print() const {
		cout << "Map : " << endl;
		for (int i = 7; i >= 0; i--) {
			for (int j = 0; j < 8; j++) {
				cout << setw(3) << _array[i][j] << " ";
			}
			cout << endl;
		}
		cout << endl;
	}
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
enum game_termination { black_win = -1, unterminated = 0, white_win = 1, draw = 2 };


// Plateau
class Board {
public:

	// Attributs

	// Plateau
	// 64 bytes
	uint_fast8_t _array[8][8]{	{	w_rook,		w_knight,	w_bishop,   w_queen,    w_king,		w_bishop,   w_knight,   w_rook	},
								{   w_pawn,		w_pawn,		w_pawn,		w_pawn,		w_pawn,		w_pawn,		w_pawn,		w_pawn	},
								{	none,		none,		none,		none,		none,		none,		none,		none	},
								{	none,		none,		none,		none,		none,		none,		none,		none	},
								{	none,		none,		none,		none,		none,		none,		none,		none	},
								{	none,		none,		none,		none,		none,		none,		none,		none	},
								{   b_pawn,		b_pawn,		b_pawn,		b_pawn,		b_pawn,		b_pawn,		b_pawn,		b_pawn	},
								{   b_rook,		b_knight,   b_bishop,   b_queen,    b_king,		b_bishop,   b_knight,   b_rook	} };

	//Array _array; // TODO utiliser

	// Coups possibles
	// Nombre max de coups légaux dans une position : 218

	// TODO: réduire le nombre de bytes utilisés
	// 200 bytes
	Move _moves[max_moves];

	// Les coups sont-ils actualisés? Si non : -1, sinon, _got_moves représente le nombre de coups jouables
	// En supposant que le nombre de coups n'excède pas 127
	// 1 byte
	int_fast8_t _got_moves = -1;

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
	uint_fast8_t _half_moves_count = 0;

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
	// TODO changer en uint_fast8_t
	float _adv = 0.0f;
	bool _advancement = false;

	// Est-ce que le calcul de game over a déjà été fait?
	bool _game_over_checked = false;
	int_fast8_t _game_over_value = 0;

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


	// Constructeur par défaut
	Board();

	// Constructeur de copie
	Board(const Board&, bool full = false, bool copy_history = false);

	// Opérateur d'égalité (compare seulement le placement des pièces, droits de roques, et nombre de coups)
	bool operator== (const Board&) const;

	// Fonction qui copie les attributs d'un plateau (full copy: on copie tout)
	void copy_data(const Board&, bool full = false, bool copy_history = false);

	// Fonction qui ajoute un coup dans la liste de coups
	bool add_move(uint_fast8_t, uint_fast8_t, uint_fast8_t, uint_fast8_t, int*, uint_fast8_t);

	// Fonction qui ajoute les coups "pions" dans la liste de coups
	bool add_pawn_moves(uint_fast8_t, uint_fast8_t, int*);

	// Fonction qui ajoute les coups "cavaliers" dans la liste de coups
	bool add_knight_moves(uint_fast8_t, uint_fast8_t, int*, uint_fast8_t);

	// Fonction qui ajoute les coups diagonaux dans la liste de coups
	bool add_diag_moves(uint_fast8_t, uint_fast8_t, int*, uint_fast8_t);

	// Fonction qui ajoute les coups horizontaux et verticaux dans la liste de coups
	bool add_rect_moves(uint_fast8_t, uint_fast8_t, int*, uint_fast8_t);

	// Fonction qui ajoute les coups "roi" dans la liste de coups
	bool add_king_moves(uint_fast8_t, uint_fast8_t, int*, uint_fast8_t);

	// Renvoie la liste des coups possibles
	bool get_moves(const bool forbide_check = true);

	// Fonction qui dit s'il y'a échec
	[[nodiscard]] bool in_check();

	// Fonction qui affiche la liste des coups
	void display_moves(bool pseudo = false);

	// Fonction qui joue un coup
	void make_move(Move, const bool pgn = false, const bool new_board = false, const bool add_to_history = false);

	// Fonction qui joue le coup i de la liste des coups possibles
	void make_index_move(int, const bool pgn = false, const bool new_board = false, const bool add_to_history = false);

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
	[[nodiscard]] string to_fen() const;

	// Fonction qui renvoie le gagnant si la partie est finie (-1/1, et 2 pour nulle), et 0 sinon
	int game_over(int max_repetitions);

	// Fonction qui renvoie le gagnant si la partie est finie (-1/1, et 2 pour nulle), et 0 sinon -> et stocke la valeur dans _game_over_value
	int is_game_over(int max_repetitions = 1);

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
	[[nodiscard]] int is_eval_mate(int) const;

	// Fonction qui génère le livre d'ouvertures
	void generate_opening_book(int nodes = 100000);

	// Fonction qui renvoie une représentation simple et rapide de la position
	[[nodiscard]] string simple_position() const;

	// Fonction qui calcule la structure de pions et renvoie sa valeur
	[[nodiscard]] int get_pawn_structure(float display_factor = 0.0f);

	// Fonction qui calcule la résultante des attaques et des défenses et la renvoie
	[[nodiscard]] float get_attacks_and_defenses() const;

	// Fonction qui calcule et renvoie l'opposition des rois (en finales de pions)
	int get_kings_opposition();

	// Fonction qui renvoie le type de pièce sélectionnée
	[[nodiscard]] uint_fast8_t selected_piece() const;

	// Fonction qui renvoie le type de pièce où la souris vient de cliquer
	[[nodiscard]] uint_fast8_t clicked_piece() const;

	// Fonction qui renvoie si la pièce sélectionnée est au joueur ayant trait ou non
	[[nodiscard]] bool selected_piece_has_trait() const;

	// Fonction qui renvoie si la pièce cliquée est au joueur ayant trait ou non
	[[nodiscard]] bool clicked_piece_has_trait() const;

	// Fonction qui remet les compteurs de temps "à zéro" (temps de base)
	static void reset_timers();

	// Fonction qui remet le plateau dans sa position initiale
	void restart();

	// Fonction qui renvoie la différence matérielle entre les deux camps
	[[nodiscard]] int material_difference() const;

	// Fonction qui réinitialise les composantes de l'évaluation
	void reset_eval();

	// Fonction qui compte les tours sur les colonnes ouvertes et semi-ouvertes et renvoie la valeur
	[[nodiscard]] int get_sliders_on_open_file() const;

	// Fonction qui calcule la valeur des cases controllées sur l'échiquier
	[[nodiscard]] int get_square_controls() const;

	// Fonction qui fait un tri rapide des coups (en plaçant les captures en premier)
	bool sort_moves();

	// Fonction qui fait cliquer le coup m
	[[nodiscard]] bool click_m_move(Move i, bool orientation) const;

	// Fonction qui récupère et renvoie la couleur du joueur au trait (1 pour les blancs, -1 pour les noirs)
	[[nodiscard]] int get_color() const;

	// Fonction qui calcule et renvoie l'avantage d'espace
	[[nodiscard]] int get_space() const;

	// Fonction qui calcule et renvoie une évaluation des vis-à-vis
	[[nodiscard]] int get_alignments() const;

	// Fonction qui met à jour la position des rois
	bool update_kings_pos();

	// Fonction qui renvoie la puissance de défense d'une pièce pour le roi allié
	[[nodiscard]] int get_piece_defense_power(int i, int j) const;

	// Fonction qui renvoie l'activité des pièces
	[[nodiscard]] int get_piece_activity() const;

	// Fonction qui renvoie la map correspondante au nombre de contrôles pour chaque case de l'échiquier pour le joueur blanc
	[[nodiscard]] Map get_white_controls_map() const;

	// Fonction qui renvoie la map correspondante au nombre de contrôles pour chaque case de l'échiquier pour le joueur noir
	[[nodiscard]] Map get_black_controls_map() const;

	// Fonction qui ajoute à une map les contrôles d'une pièce
	[[nodiscard]] bool add_piece_controls(Map* map, int i, int j, int piece) const;

	// Fonction qui renvoie la mobilité virtuelle d'un roi
	[[nodiscard]] int get_king_virtual_mobility(bool color);

	// Fonction qui renvoie le nombre d'échecs 'safe' dans la position pour les deux joueurs
	[[nodiscard]] int get_checks_value(Map white_controls, Map black_controls, bool color);

	// Fonction qui renvoie la vitesse de génération des coups
	[[nodiscard]] int moves_generation_benchmark(uint_fast8_t depth, bool main_call = true);

	// Fonction qui renvoie la valeur des fous en fianchetto
	[[nodiscard]] int get_fianchetto_value() const;

	// Fonction qui renvoie si la case est controlée par un joueur
	[[nodiscard]] bool is_controlled(int square_i, int square_j, bool player) const;

	// Fonction qui calcule et renvoie la valeur des menaces d'avance de pion
	[[nodiscard]] int get_pawn_push_threats() const;

	// Fonction qui calcule et renvoie la proximité du roi avec les pions
	[[nodiscard]] int get_king_proximity();

	// Fonction qui calcule et renvoie l'activité/mobilité des tours
	[[nodiscard]] int get_rook_activity() const;

	// Fonction qui calcule et renvoie la valeur des pions qui bloquent les fous
	[[nodiscard]] int get_bishop_pawns() const;

	// Fonction qui renvoie la valeur des faiblesses long terme du bouclier de pions
	[[nodiscard]] int get_pawn_shield();

	// Fonction qui renvoie la caleur des cases faibles
	[[nodiscard]] int get_weak_squares() const;

	// Fonction qui convertit un coup en sa notation algébrique
	string algebric_notation(Move move) const;

	// Fonction qui convertit une notation algébrique en un coup
	Move move_from_algebric_notation(string notation);

	// Fonction qui renvoie la valeur de la distance à la possibilité de roque
	[[nodiscard]] int get_castling_distance() const;

	// Fonction qui génère la clé de Zobrist du plateau
	[[nodiscard]] void get_zobrist_key();

	// Fonction qui renvoie à quel point la partie est gagnable (de 0 à 1), pour une couleur
	[[nodiscard]] float get_winnable(bool color) const;

	// Fonction qui renvoie l'activité des fous sur les diagonales
	[[nodiscard]] int get_bishop_activity() const;

	// Fonction qui renvoie si un coup est légal ou non
	[[nodiscard]] bool is_legal(Move move);

	// Fonction qui reset l'historique des positions
	void reset_positions_history();

	// Fonction qui renvoie combien de fois la position actuelle a été répétée
	[[nodiscard]] int repetition_count();

	// Affiche l'histoirque des positions (les clés de Zobrist)
	void display_positions_history() const;

	// Quiescence search pour l'algo de GrogrosZero
	//int grogros_quiescence(Evaluator* eval, int alpha = -2147483647, int beta = 2147483647, int depth = 4, bool explore_checks = true, bool main_player = true);

	// Fonction qui renvoie l'affichage de l'évaluation
	[[nodiscard]] string evaluation_to_string(int eval) const;

	// Fonction qui renvoie l'évaluation des pièces enfermées
	[[nodiscard]] int get_trapped_pieces() const;

	// Fonction qui ajuste les valeurs des pièces (malus/bonus), en fonction du type de position
	[[nodiscard]] int get_updated_piece_values() const;

	// Fonction qui renvoie la nature de la position de manière chiffrée: 0 = ouverte, 1 = fermée
	[[nodiscard]] float get_position_nature() const;

	// Fonction qui renvoie la probabilité de nulle de la position
	[[nodiscard]] float get_draw_chance() const;

	// Fonction qui renvoie la valeur des bonus liés aux colonnes ouvertes et semi-ouvertes sur le roi adverse
	[[nodiscard]] int get_open_files_on_opponent_king(bool color);

	// Fonction qui renvoie la valeur des bonus liés aux diagonales ouvertes et semi-ouvertes sur le roi adverse
	[[nodiscard]] int get_open_diagonals_on_opponent_king(bool color);

	// Fonction qui renvoie le nombre de cases de retrait pour le roi
	[[nodiscard]] int get_king_escape_squares(bool color);

	// Fonction qui renvoie une valeur correspondante aux pièces attaquant le roi adverse
	[[nodiscard]] int get_king_attackers(bool color);

	// Fonction qui renvoie une valeur correspondante aux pièces défendant le roi
	[[nodiscard]] int get_king_defenders(bool color);

	// Fonction qui renvoie un bonus correspondant au pawn storm sur le roi adverse
	[[nodiscard]] int get_pawn_storm(bool color);

	// Fonction qui renvoie un bonus d'activité pour les cavaliers
	[[nodiscard]] int get_knight_activity() const;

	// Fonction qui renvoie la puissance de protection de la structure de pions du roi
	[[nodiscard]] int get_pawn_shield_protection(bool color, float opponent_attacking_potential);

	// Fonction qui renvoie la puissance de protection de la structure de pions du roi, s'il est sur la colonne donnée
	[[nodiscard]] int get_pawn_shield_protection_at_column(bool color, int column, float opponent_attacking_potential, bool add_column_bonus = false);

	// Fonction qui calcule tous les coups à une certaine profondeur, et renvoie le nombre de noeuds total
	int count_nodes_at_depth(int depth, bool display = true);

	// Fonction qui renvoie si le nombre de noeuds calculés pour une position à une certaine profondeur correspond au nombre attendu
	bool validate_nodes_count_at_depth(string fen, int depth, vector<int> expected_nodes, bool display = false);

	// Fonction test: nouvelle mobilité des pièces
	[[nodiscard]] int get_piece_mobility(bool display = false) const;

	// Fonction qui renvoie si le pion peut bouger
	[[nodiscard]] bool pawn_can_move(uint_fast8_t row, uint_fast8_t col, bool color) const;

	// Fonction qui renvoie l'incertiude de la position
	[[nodiscard]] float get_uncertainty(int material_eval);

	// Fonction qui renvoie le WDL de la position
	[[nodiscard]] void get_WDL(float uncertainity = 0.0f, int winning_eval = 150, float beta = 0.75f);

	// Fonction qui renvoie l'espérance de gain (en points) de la position (fondé sur les probas de WDL) pour les blancs
	[[nodiscard]] float get_average_score(float draw_score = 0.5f) const;

	// Fonction qui inverse les couleurs des joueurs (y compris le trait)
	void switch_colors();

	// Fonction qui itère sur la map des distances à partir d'une position donnée, et renvoie les nouvelles cases contrôlées
	[[nodiscard]] vector<Pos> get_next_king_squares(Map& map, Pos start_pos, int distance, bool color) const;

	// Fonction qui renvoie une map des distances entre le roi et chaque point de l'échiquier (nombre de coups pour y arriver, en fonction des contrôles actuels du plateau)
	[[nodiscard]] Map get_king_squares_distance(bool color);

	// Fonction qui renvoie la faiblesse sur les rangées du roi
	[[nodiscard]] int get_king_row_weakness(bool color);

	// Fonction qui renvoie la valeur de centralisation du roi en fin de partie
	[[nodiscard]] int get_king_centralization(bool color);

	// Fonction qui renvoie la valeur des pièces non défendues
	[[nodiscard]] int get_unprotected_pieces(bool color) const;

	// Fonction qui renvoie si le roi est dans le carré du pion
	[[nodiscard]] bool in_king_square(Pos pos, bool color);

	// Fonction qui renvoie si on est en finale de pions
	[[nodiscard]] bool is_pawn_endgame() const;

	// Fonction qui renvoie si le joueur a encore des pièces (autres que roi et pions)
	[[nodiscard]] bool has_pieces(bool color) const;

	// TODO: génération plus rapide des coups

	// TODO: test de puzzles et problèmes stratégiques, évaluations... avec un certain temps imparti
	// Donner un score par catégorie: problèmes tactiques -> avec thèmes -> avec différentes cadences
	// Problèmes stratégiques -> avec thèmes -> avec différentes cadences
	// Test de positions pour évaluer le jeu; évaluation du coup joué et différence avec le meilleur coup
	// Test d'évaluation: comparaison avec l'évaluation profonde de Stockfish/Leela -> test de l'évaluation statique + dynamique avec différentes cadences
};

// Fonction qui renvoie si deux positions (en format FEN) sont les mêmes
bool equal_fen(const string&, const string&);

// Fonction qui renvoie si deux positions (en format FEN) sont les mêmes (pour les répétitions)
bool equal_positions(const Board&, const Board&);

// Fonction qui renvoie le temps que l'IA doit passer sur le prochain coup (en ms), en fonction d'un facteur k, et des temps restant
int time_to_play_move(int t1, int t2, float k = 0.05f);

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
string square_name(uint_fast8_t i, uint_fast8_t j);

// Fonction qui renvoie le nom d'une pièce
string piece_name(uint_fast8_t piece);

// Fonction qui renvoie l'espérance de gain d'un WDL
float get_average_score(WDL wdl, float draw_score = 0.5f);

// Fonction qui renvoie le score d'un WDL avec une précision de 0.01
string score_string(float avg_score);