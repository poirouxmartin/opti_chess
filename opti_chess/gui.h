#pragma once

#include "time.h"
#include <string>
#include "raylib.h"
#include <iostream>
#include <vector>
#include "board.h"
#include "game_tree.h"
#include "exploration.h"

using namespace std;

// TODO: rajouter des const un peu partout

// GUI
class GUI {
public:
	// Variables

	// Dimensions de la fenêtre
	int _screen_width = 1800;
	int _screen_height = 945;

	// Plateau affiché
	Board _board;

	// Faut-il faire l'affichage?
	bool _draw = true;

	// Faut-il que les coups soient cliqués en binding chess.com?
	bool _click_bind = false;

	// Mode de jeu automatique en binding avec chess.com
	bool _binding_full = false; // Pour récupérer tous les coups de la partie
	bool _binding_solo = false; // Pour récupérer seulement les coups de la couleur du joueur du bas

	// Intervalle de tmeps pour check chess.com
	int _binding_interval_check = 100;

	// Moment du dernier check
	clock_t _last_binding_check = clock();

	// Coup récupéré par le binding
	uint_fast8_t* _binding_move = new uint_fast8_t[4];

	// Coordonnées du plateau sur chess.com
	int _binding_left = 108; // (+10 si barre d'éval)
	int _binding_top = 219;
	int _binding_right = 851;
	int _binding_bottom = 962;

	// Coordonées du plateau pour le binding
	//SimpleRectangle _binding_coord;

	// Temps initial des joueurs
	clock_t _initial_time_white = 180000;
	clock_t _initial_time_black = 180000;

	// Temps des joueurs
	clock_t _time_white;
	clock_t _time_black;

	// Incrément
	clock_t _time_increment_white = -100;
	clock_t _time_increment_black = -100;

	// Mode analyse de Grogros
	bool _grogros_analysis = false;

	// Le temps est-il activé?
	bool _time = false;

	// Pour la gestion du temps
	clock_t _last_move_clock;

	// Joueur au trait lors du dernier check (pour les temps)
	bool _last_player = true;

	// Affichage des flèches : affiche les chances de gain (true), l'évaluation (false)
	bool _display_win_chances = true;

	// Texte pour les timers
	TextBox _white_time_text_box;
	TextBox _black_time_text_box;

	// Paramètres pour la recherche de Monte-Carlo
	float _beta = 0.25f;
	float _k_add = 10.0f;
	int _quiescence_depth = 40;

	//float _beta = 0.1f;
	//float _k_add = 25.0f;
	//int _quiescence_depth = 8;

	bool _explore_checks = true; // FIXME? faut-il vraiment explorer les échecs?

	// Paramètres de brute
	//float _beta = 0.25f;
	//float _k_add = 1.0f;
	//int _quiescence_depth = 4;

	// Est-ce que les noms des joueurs ont été ajoutés au PGN
	bool _named_pgn = false;
	bool _timed_pgn = false;

	// Affichage du PGN

	// Joueurs
	string _white_player = "White";
	string _black_player = "Black";

	// FEN de la position initiale
	string _initial_fen;

	// FEN de la position actuelle
	string _current_fen;

	// PGN de la partie
	string _pgn;

	// Cadence
	string _time_control;

	// PGN global
	string _global_pgn;

	// Titres des joueurs
	string _white_title;
	string _black_title;

	// Elo des joueurs
	string _white_elo;
	string _black_elo;

	// URL des joueurs (pour les images)
	string _white_url;
	string _black_url;

	// Pays des joueurs
	string _white_country;
	string _black_country;

	// Date de la partie
	string _date;

	// Elo de GrogrosZero
	string _grogros_zero_elo = "2300";

	// Evaluateur pour l'affichage de l'évaluation
	Evaluator* _grogros_eval = new Evaluator();
	//Evaluator* _grogros_eval = new Evaluator(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

	// Faut-il actualiser la GUI au niveau des variantes?
	bool _update_variants = false;

	// Position sélectionnée sur la GUI
	Pos _selected_pos = Pos(-1, -1);

	// Position cliquée sur la GUI
	Pos _clicked_pos = Pos(-1, -1);

	// Position de la souris
	Vector2 _mouse_pos;

	// Position de la case cliquée (droit)
	Pos _right_clicked_pos = { -1, -1 };

	// La souris est-elle cliquée
	bool _clicked = false;

	// Vecteur des flèches de GrogrosZero : vecteur de coups
	vector<Move> _grogros_arrows;

	// TODO : Threads (pour la parallélisation)
	// Thread de GUI

	// Thread de GrogrosZero
	thread _thread_grogros_zero;

	// Threads pour les plateaux fils de GrogrosZero
	vector<thread> _threads_grogros_zero;

	// Arbre de recherche, variantes jouées dans le PGN
	GameTree _game_tree = GameTree(_board);

	// Nombre de FPS
	int _fps = 144;
	clock_t _last_drawing_time = 0;

	// Couleur de fond
	Color _background_color = { 25, 25, 25, 255 };

	// Couleur du rectangle de texte
	Color _background_text_color = { 0, 0, 0, 255 };

	// Couleurs du texte
	Color _text_color = { 255, 75, 75, 255 };
	Color _text_color_dark = { 200, 50, 50, 255 };
	Color _text_color_light = { 200, 200, 200, 255 };
	Color _text_color_blue = { 150, 150, 200, 255 };
	Color _text_color_info = { 140, 140, 140, 255 };

	// Couleurs du plateau
	Color _board_color_light = { 180, 150, 115, 255 };
	Color _board_color_dark = { 109, 75, 54, 255 };

	// Couleur de surlignage de cases
	Color _highlight_color = { 255, 255, 100, 150 };

	// Couleur de selection de cases
	Color _select_color = { 50, 225, 50, 100 };

	// Couleur des cases du dernier coup joué
	Color _last_move_color = { 220, 150, 50, 125 };

	// Couleur de la case de pre-move
	Color _pre_move_color = { 220, 30, 30, 125 };

	// Couleur des flèches
	Color _arrow_color = { 255, 225, 0, 150 };

	// Couleur des sliders
	Color _slider_color = { 200, 200, 200, 100 };
	Color _slider_background_color = { 100, 100, 100, 75 };

	// Couleurs de la barre d'évaluation
	Color _eval_bar_color_light = { 224, 206, 186, 255 };
	Color _eval_bar_color_dark = { 57, 50, 47, 255 };

	// Epaisseur des flèches (par rapport à la taille d'une case)
	float _arrow_scale = 0.125f;
	float _arrow_thickness = 50.0f;

	// Est-ce qu'on affiche les flèches (non par exemple si l'utilisateur veut jouer contre l'IA)
	bool _drawing_arrows = true;

	// Pourcentage de noeuds à partir duquel on montre le coup
	float _arrow_rate = 0.05f;

	// Variable qui indique si l'initialisation a été faite
	bool _loaded_resources = false;

	// Textures et images
	Image _piece_images[12];
	Texture2D _piece_textures[12];

	// Icône
	Image _icon;

	// Tête de Grogros
	Image _grogros_image;
	Texture2D _grogros_texture;
	float _grogros_size;

	// Curseur
	Image _cursor_image;
	Texture2D _cursor_texture;
	float _cursor_size = 64.0f;

	// Mini-pièces
	Image _mini_piece_images[12];
	Texture2D _mini_piece_textures[12];
	int _mini_piece_size;

	// Sons
	Sound _move_1_sound;
	Sound _move_2_sound;
	Sound _castle_1_sound;
	Sound _castle_2_sound;
	Sound _check_1_sound;
	Sound _check_2_sound;
	Sound _capture_1_sound;
	Sound _capture_2_sound;
	Sound _checkmate_sound;
	Sound _stealmate_sound;
	Sound _game_begin_sound;
	Sound _game_end_sound;
	Sound _promotion_sound;

	// Taille du plateau par rapport à la fenêtre
	float _board_scale = 0.7f;
	float _board_size;
	float _board_padding_x;
	float _board_padding_y;

	// Taille des pièces
	float _tile_size;
	float _piece_size;
	float _piece_scale = 0.8f;

	// Taille standard du texte
	float _text_size;

	// Police du texte
	Font _text_font;

	// Espacement entre les caractères
	float _font_spacing = 0.05f;

	// Orientation du plateau
	bool _board_orientation = true;

	// Cases surlignées
	int _highlighted_array[8][8]
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0} 
	};

	// Calcul du nombre de noeuds visités
	int _visited_nodes;

	// Calcul de temps
	clock_t _begin_time;

	// Valeurs des sliders
	float _pgn_slider = 0.0f;
	float _monte_carlo_slider = 0.0f;
	float _variants_slider = 0.0f;

	// Pre-move
	//int pre_move[4] = { -1, -1, -1, -1 };

	// Eval à montrer pour la barre d'éval
	float _global_eval = 0.0f;
	string _global_eval_text = "+0.0";

	// Valeur des pièces pour l'affichage sur la GUI (rien/roi, pion, cavalier, fou, tour, dame)
	const int _piece_GUI_values[6] = { 0, 1, 3, 3, 5, 9 };

	// Matériel manquant
	const int _base_material[6] = { 0, 8, 2, 2, 2, 1 };
	int _missing_w_material[6] = { 0, 0, 0, 0, 0, 0 };
	int _missing_b_material[6] = { 0, 0, 0, 0, 0, 0 };

	// Alphabet de taille 8
	const string _abc8 = "abcdefgh";

	// Flèches sur l'échiquier
	vector<vector<int>> _arrows_array;

	// Composantes de l'évaluation à afficher sur la GUI
	string _eval_components;

	// Gris
	Color _gray = { 100, 100, 100, 255 };

	// Gris foncé
	Color _dark_gray = { 50, 50, 50, 255 };

	// Nombre de coups pour une répétition
	int _max_repetition = 1;

	// Noeud de l'arbre d'exploration
	Node *_root_exploration_node;

	// Affichage des variantes
	string _exploration_variants = "";

	// Nombre de noeuds par frame pour l'exploration
	int _nodes_per_frame = 1000;


	// Constructeurs

	// Par défaut
	GUI();

	// Fonctions

	// Fonction qui met en place le binding avec chess.com pour une nouvelle partie (et se prépare à jouer avec GrogrosZero)
	bool new_bind_game();

	// Fonction qui met en place le binding avec chess.com pour une nouvelle analyse de partie
	bool new_bind_analysis();

	// Fonction qui construit le PGN global
	bool update_global_pgn();

	// Fonction qui met à jour la cadence du PGN
	bool update_time_control();

	// Fonction qui lance le temps
	void start_time();

	// Fonction qui stoppe le temps
	void stop_time();

	// Fonction qui met à jour le temps des joueurs
	void update_time();

	// Fonction qui réinitialise le PGN
	bool reset_pgn();

	// Fonction qui met à jour la date du PGN
	bool update_date();

	// Fonction qui lance les threads de GrogrosZero
	//bool thread_grogros_zero(Evaluator* eval, int nodes);

	// Fonction qui lance grogros sur un thread
	//bool grogros_zero_threaded(Evaluator* eval, int nodes);

	// Fonction qui retire le dernier coup du PGN
	bool remove_last_move_PGN();

	// Fonction qui dessine les flèches en fonction des valeurs dans l'algo de Monte-Carlo
	//void draw_monte_carlo_arrows();

	// Fonction qui dessine les flèches en fonction des valeurs dans l'algo de Monte-Carlo
	void draw_exploration_arrows();

	// Fonction qui dessine la flèche d'un coup
	void draw_arrow(const Move move, const bool player, Color c, float thickness = -1, const bool use_value = false, int value = 0, const int mate = -1, const bool outline = false);

	// A partir de coordonnées sur le plateau (// Thickness = -1 -> default thickness)
	void draw_arrow_from_coord(const int i1, const int j1, const int i2, const int j2, const bool player, Color c, float thickness = -1, const bool use_value = false, int value = 0, const int mate = -1, bool outline = false);

	// Couleur de la flèche en fonction du coup (de son nombre de noeuds)
	Color move_color(int, int) const;

	// Fonction qui charge les textures
	void load_resources();

	// Fonction qui met à la bonne taille les images
	void resize_GUI();

	// Fonction qui actualise les nouvelles dimensions de la fenêtre
	void get_window_size();

	// Fonction qui renvoie si le joueur est en train de jouer (pour que l'IA arrête de réflechir à ce moment sinon ça lagge)
	bool is_playing();

	// Fonction qui change le mode d'affichage des flèches (oui/non)
	void switch_arrow_drawing();

	// Fonction qui affiche un texte dans une zone donnée avec un slider
	void slider_text(const string&, float, float, float, float, float size, float* slider_value, Color t_color, float slider_width = -1.0f, float slider_height = -1.0f);

	// Fonction pour obtenir l'orientation du plateau
	bool get_board_orientation();

	// Fonction qui renvoie si le curseur de la souris se trouve dans le rectangle
	bool is_cursor_in_rect(Rectangle);

	// Fonction qui dessine un rectangle à partir de coordonnées flottantes
	bool draw_rectangle(float, float, float, float, Color);

	// Fonction qui dessine un rectangle à partir de coordonnées flottantes, en fonction des coordonnées de début et de fin
	bool draw_rectangle_from_pos(float, float, float, float, Color);

	// Fonction qui dessine un cercle à partir de coordonnées flottantes
	void draw_circle(float, float, float, Color);

	// Fonction qui dessine une ligne à partir de coordonnées flottantes
	void draw_line_ex(float, float, float, float, float, Color);

	// Fonction qui dessine une ligne de Bézier à partir de coordonnées flottantes
	void draw_line_bezier(float, float, float, float, float, Color);

	// Fonction qui dessine une texture à partir de coordonnées flottantes
	void draw_texture(const Texture&, float, float, Color);

	// Fonction qui affiche la barre d'evaluation
	void draw_eval_bar(float, const string&, float, float, float, float, float max_eval, Color, Color, float max_height = -1.0f);

	// Fonction qui retire les surlignages de toutes les cases
	void remove_highlighted_tiles();

	// Fonction qui selectionne une case
	void select_tile(int, int);

	// Fonction qui surligne une case (ou la de-surligne)
	void highlight_tile(int, int);

	// Fonction qui déselectionne
	void unselect();

	// Fonction qui joue le son de fin de partie
	void play_end_sound();

	// A partir de coordonnées sur le plateau
	void draw_simple_arrow_from_coord(int, int, int, int, float, Color);

	// Fonction qui obtient la case correspondante à la position sur la GUI
	Pos get_pos_from_GUI(float, float);

	// Fonction qui permet de changer l'orientation du plateau
	void switch_orientation();

	// Fonction aidant à l'affichage du plateau (renvoie i si board_orientation, et 7 - i sinon)
	int orientation_index(int) const;

	// TODO
	bool play_move_keep(const Move move);

	// Fonction qui renvoie le type de pièce sélectionnée
	uint_fast8_t selected_piece() const;

	// Fonction qui renvoie le type de pièce où la souris vient de cliquer
	uint_fast8_t clicked_piece() const;

	// Fonction qui lance une analyse de GrogrosZero
	void grogros_analysis(int nodes = -1);

	// Fonction qui charge une position à partir d'une FEN
	void load_FEN(const string fen);

	// Fonction qui reset la partie
	void reset_game();

	// Fonction qui compare deux flèches d'analyse de Grogros
	bool compare_arrows(const int m1, const int m2) const;


	// TODO
	void draw();
};

// Instantiation de la GUI globale
extern GUI main_GUI;