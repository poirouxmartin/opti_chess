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

using namespace std;


// TODO : à la place de 1, .. 12, mettre des enums (P, N, B, R, Q, K, p, n, b, r, q, k)

/*

Plateau :
	Pièces :
		Blancs :
			Pion = 1
			Cavalier = 2
			Fou = 3
			Tour = 4
			Dame = 5
			Roi = 6

		Noirs :
			Pion = 7
			Cavalier = 8
			Fou = 9
			Tour = 10
			Dame = 11
			Roi = 12

*/

// Enumération des pièces
//enum piece_type { P = 1, N = 2, B = 3, R = 4, Q = 5, K = 6, p = 7, n = 8, b = 9, r = 10, q = 11, k = 12 };


// Nombre de demi-coups avant de déclarer la partie nulle
constexpr int max_half_moves = 100;


// Nombre maximum de coups légaux par position estimé
// const int max_moves = 218;
constexpr int max_moves = 100; // ça n'arrivera quasi jamais que ça dépasse ce nombre

// Valeur d'un échec et mat
constexpr int mate_value = 1e8;

// Valeur d'un ply (double) dans la recherche de mat
constexpr int mate_ply = 1e5;


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
	//bool capture_flag : 1;
	//bool promotion_flag : 1;
	bool is_null : 1;
	// Reste 2 bytes à utiliser : check? castling? en passant? is null?

	bool operator== (const Move& other) const {
		return (i1 == other.i1) && (j1 == other.j1) && (i2 == other.i2) && (j2 == other.j2);
	}

	void display() const
	{
		cout << "(" << static_cast<int>(i1) << ", " << static_cast<int>(j1) << ") -> (" << static_cast<int>(i2) << ", " << static_cast<int>(j2) << ")" << endl;
	}

	string to_string() const
	{
		return "(" + std::to_string(i1) + ", " + std::to_string(j1) + ") -> (" + std::to_string(i2) + ", " + std::to_string(j2) + ")";
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
	int i : 4;
	int j : 4;

	// Opérateur d'égalité
	bool operator== (const Pos& other) const {
		return (i == other.i) && (j == other.j);
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
	

// Plateau
class Board {
public:

	// Attributs

	// Plateau
	// 64 bytes
	uint_fast8_t _array[8][8]{	{ 4, 2, 3, 5, 6, 3, 2, 4 },
								{ 1, 1, 1, 1, 1, 1, 1, 1 },
								{ 0, 0, 0, 0, 0, 0, 0, 0 },
								{ 0, 0, 0, 0, 0, 0, 0, 0 },
								{ 0, 0, 0, 0, 0, 0, 0, 0 },
								{ 0, 0, 0, 0, 0, 0, 0, 0 },
								{ 7, 7, 7, 7, 7, 7, 7, 7 },
								{10, 8, 9,11,12, 9, 8,10 } };

	// Coups possibles
	// Nombre max de coups légaux dans une position : 218

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

	// Liste des index des plateaux fils dans le buffer (changer la structure de données?)
	int* _index_children = nullptr;

	// Nombre de coups déjà testés
	uint_fast8_t _tested_moves = 0;

	// Coup auquel il en est dans sa recherche
	uint_fast8_t _current_move = 0;

	// Evaluation des fils
	int* _eval_children = nullptr;

	// Nombre de noeuds
	int _nodes = 0;

	// Noeuds des enfants
	int* _nodes_children = nullptr;

	// Est-ce que le plateau a été évalué?
	bool _evaluated = false;

	// Le plateau a t-il été initialisé?
	bool _new_board = true;

	// Pour l'affichage
	int _static_evaluation = 0;

	// Temps passé sur l'anayse de Monte-Carlo
	clock_t _time_monte_carlo = 0;

	// Avancement de la partie
	// TODO changer en uint_fast8_t
	float _adv = 0.0f;
	bool _advancement = false;

	// Est-ce que le calcul de game over a déjà été fait?
	bool _game_over_checked = false;
	int _game_over_value = 0;

	// Nombre de noeuds regardés par le quiescence search
	int _quiescence_nodes = 0;

	// On stocke les positions des rois
	Pos _white_king_pos = { 0, 4 };
	Pos _black_king_pos = { 7, 4 };

	// Est-ce qu'on a affiché les composantes du plateau?
	bool _displayed_components = false;

	// Constructeur par défaut
	Board();

	// Constructeur de copie
	Board(const Board&);

	// Opérateur d'égalité (compare seulement le placement des pièces, droits de roques, et nombre de coups)
	bool operator== (const Board&) const;

	// Fonction qui copie les attributs d'un plateau
	void copy_data(const Board&);

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
	bool get_moves(const bool forbide_check = false);

	// Fonction qui dit s'il y'a échec
	[[nodiscard]] bool in_check();

	// Fonction qui affiche la liste des coups
	void display_moves(bool pseudo = false);

	// Fonction qui joue un coup
	void make_move(Move, bool pgn = false, bool new_board = false);

	// Fonction qui joue le coup i de la liste des coups possibles
	void make_index_move(int, bool pgn = false);

	// Fonction qui renvoie l'avancement de la partie (0 = début de partie, 1 = fin de partie)
	void game_advancement();

	// Fonction qui compte le matériel sur l'échiquier et renvoie sa valeur
	int count_material(const Evaluator* e = nullptr) const;

	// Fonction qui compte les paires de fous et renvoie la valeur
	int count_bishop_pairs() const;

	// Fonction qui calcule et renvoie la valeur de positionnement des pièces sur l'échiquier
	int pieces_positioning(const Evaluator* eval = nullptr) const;

	// Fonction qui évalue la position à l'aide d'heuristiques
	bool evaluate(Evaluator* eval = nullptr, bool display = false, Network* n = nullptr);

	// Fonction qui joue le coup d'une position, renvoyant la meilleure évaluation à l'aide d'un negamax (similaire à un minimax)
	int negamax(int, int, int, bool, Evaluator*, bool play = false, bool display = false, int quiescence_depth = 8, int null_depth = 2);

	// Grogrosfish
	bool grogrosfish(int, Evaluator*, bool);

	// Fonction qui récupère le plateau d'un FEN
	void from_fen(string);

	// Fonction qui renvoie le FEN du tableau
	[[nodiscard]] string to_fen() const;

	// Fonction qui renvoie le gagnant si la partie est finie (-1/1), et 0 sinon
	int is_game_over();

	// Fonction qui renvoie le label d'un coup
	string move_label(Move move);

	// Fonction qui renvoie le label d'un coup en fonction de son index
	string move_label_from_index(int);

	// Fonction qui affiche un texte dans une zone donnée
	static void draw_text_rect(const string&, float, float, float, float, float);

	// Fonction qui dessine le plateau
	bool draw();

	// Fonction qui joue le son d'un coup
	void play_move_sound(Move) const;

	// Fonction qui joue le son d'un coup à partir de son index
	void play_index_move_sound(int) const;

	// Fonction qui calcule et renvoie la mobilité des pièces
	[[nodiscard]] int get_piece_mobility(bool legal = false) const;

	// Fonction qui renvoie le meilleur coup selon l'analyse faite par l'algo de Monte-Carlo
	[[nodiscard]] int best_monte_carlo_move() const;

	// Fonction qui joue le coup après analyse par l'algo de Monte-Carlo, et qui garde en mémoire les infos du nouveau plateau
	bool play_monte_carlo_move_keep(Move move, bool keep = true, bool keep_display = false, bool display = false, bool add_to_list = false);

	// Pas très opti pour l'affichage, mais bon... Fonction qui cherche la profondeur la plus grande dans la recherche de Monté-Carlo
	[[nodiscard]] int max_monte_carlo_depth() const;

	// Algo de grogros_zero
	void grogros_zero(Evaluator* eval = nullptr, int nodes = 1, float beta = 0.035f, float k_add = 50.0f, int quiescence_depth = 4, bool explore_checks = true, bool display = false, int depth = 0, Network* net = nullptr, int correction = 0);

	// Fonction qui réinitialise le plateau dans son état de base (pour le buffer)
	void reset_board(bool display = false);

	// Fonction qui réinitialise tous les plateaux fils dans le buffer
	void reset_all(bool self = true, bool display = false);

	// Fonction qui renvoie le nombre de noeuds calculés par GrogrosZero
	[[nodiscard]] int total_nodes() const;

	// Fonction qui calcule et renvoie la valeur correspondante à la sécurité des rois
	int get_king_safety();

	// Fonction qui dit si une pièce est capturable par l'ennemi (pour les affichages GUI)
	bool is_capturable(int, int);

	// Fonction qui affiche le PGN
	void display_pgn() const;

	// Fonction qui renvoie en chaîne de caractères la meilleure variante selon monte carlo
	string get_monte_carlo_variant(bool evaluate_final_pos = false);

	// Fonction qui trie les index des coups par nombre de noeuds décroissant
	[[nodiscard]] vector<int> sort_by_nodes(bool ascending = false) const;

	// Fonction qui renvoie selon l'évaluation si c'est un mat ou non
	[[nodiscard]] int is_eval_mate(int) const;

	// Fonction qui génère le livre d'ouvertures
	void generate_opening_book(int nodes = 100000);

	// Fonction qui renvoie une représentation simple et rapide de la position
	[[nodiscard]] string simple_position() const;

	// Fonction qui calcule la structure de pions et renvoie sa valeur
	[[nodiscard]] int get_pawn_structure(float display_factor = 0.0f) const;

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
	[[nodiscard]] int get_rooks_on_open_file() const;

	// Fonction qui renvoie la profondeur de calcul de la variante principale
	[[nodiscard]] int grogros_main_depth() const;

	// Fonction qui calcule la valeur des cases controllées sur l'échiquier
	[[nodiscard]] int get_square_controls() const;

	// Fonction qui sélectionne et renvoie le coup avec le meilleur UCT
	[[nodiscard]] int select_uct(float c = 0.5f) const;

	// Fonction qui fait un tri rapide des coups (en plaçant les captures en premier)
	bool sort_moves();

	// Fonction qui fait un quiescence search
	int quiescence(Evaluator* eval, int alpha = -2147483647, int beta = 2147483647, int depth = 4, bool explore_checks = true, bool main_player = true);

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

	// Fonction qui renvoie la puissance d'attaque d'une pièce sur le roi adverse
	[[nodiscard]] int get_piece_attack_power(int i, int j) const;

	// Fonction qui renvoie la puissance de défense d'une pièce pour le roi allié
	[[nodiscard]] int get_piece_defense_power(int i, int j) const;

	// Fonction qui renvoie l'activité des pièces
	[[nodiscard]] int get_piece_activity() const;

	// Fonction qui renvoie la map correspondante au nombre de contrôles pour chaque case de l'échiquier pour le joueur blanc
	[[nodiscard]] Map get_white_controls_map() const;

	// Fonction qui renvoie la map correspondante au nombre de contrôles pour chaque case de l'échiquier pour le joueur noir
	[[nodiscard]] Map get_black_controls_map() const;

	// Fonction qui ajoute à une map les contrôles d'une pièce
	[[nodiscard]] bool add_piece_controls(Map* m, int i, int j, int piece) const;

	// Fonction qui renvoie la mobilité virtuelle d'un roi
	[[nodiscard]] int get_king_virtual_mobility(bool color);

	// Fonction qui renvoie le nombre d'échecs 'safe' dans la position pour les deux joueurs
	[[nodiscard]] pair<uint_fast8_t, uint_fast8_t> get_safe_checks(Map white_controls, Map black_controls) const;

	// Fonction qui renvoie la vitesse de génération des coups
	[[nodiscard]] int moves_generation_benchmark(uint_fast8_t depth, bool main_call = true);

	// Fonction qui renvoie la valeur des fous en fianchetto
	[[nodiscard]] int get_fianchetto_value() const;

	// Fonction qui renvoie si la case est controlée par le joueur adverse
	[[nodiscard]] bool is_controlled(int square_i, int square_j, bool player) const;

	// Fonction qui calcule et renvoie la valeur des menaces d'avance de pion
	[[nodiscard]] int get_pawn_push_threats() const;

	// Fonction qui calcule et renvoie la proximité du roi avec les pions
	[[nodiscard]] int get_king_proximity();

	// Fonction qui calcule et renvoie l'activité/mobilité des tours
	[[nodiscard]] int get_rook_activity() const;

	// Fonction qui calcule et renvoie la valeur des pions qui bloquent les fous
	[[nodiscard]] int get_bishop_pawns() const;

	// Fonction qui renvoie la valeur d'une marrée de pions
	[[nodiscard]] int get_pawn_storm() const;

	// Fonction qui renvoie la valeur des faiblesses long terme du bouclier de pions
	[[nodiscard]] int get_pawn_shield();

	// Fonction qui renvoie la caleur des cases faibles
	[[nodiscard]] int get_weak_squares() const;

	// Fonction qui convertit un coup en sa notation algébrique
	string algebric_notation(Move move);

	// Fonction qui convertit une notation algébrique en un coup
	Move move_from_algebric_notation(string notation);
};

// Fonction qui obtient la case correspondante à la position sur la GUI
Pos get_pos_from_GUI(float, float);

// Fonction qui permet de changer l'orientation du plateau
void switch_orientation();

// Fonction aidant à l'affichage du plateau (renvoie i si board_orientation, et 7 - i sinon)
int orientation_index(int);

// Fonction qui joue un match entre deux IA utilisant GrogrosZero, et une évaluation par réseau de neurones ou des évaluateurs, avec un certain nombre de noeuds de calcul
int match(Evaluator* e_white = nullptr, Evaluator* e_black = nullptr, Network* n_white = nullptr, Network* n_black = nullptr, int nodes = 1000, bool display = false, int max_moves = 100);

// Fonction qui organise un tournoi entre les IA utilisant évaluateurs et réseaux de neurones des listes et renvoie la liste des scores (dépendant des nombres par victoires/nulles, et leur valeur)
int* tournament(Evaluator**, Network**, int, int nodes = 1000, int victory = 3, int draw = 1, bool display_full = false, int max_moves = 100);

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
bool compare_move_arrows(int m1, int m2);