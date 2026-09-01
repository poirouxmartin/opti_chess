#pragma once

#include "time.h"
#include <string>
#include "raylib.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <process.h>
#include "board.h"
#include "game_tree.h"
#include "exploration.h"
#include "windows_tests.h"

using namespace std;

// Debug logging (enabled by OPTI_DEBUG=1 env var)
extern bool g_debug;
void debug_log(const char* fmt, ...);

// TODO: add const in a lot more places

// GUI
class GUI {
public:
	// Variables

	// Dimensions of the window
	int _screen_width = 1280;
	int _screen_height = 720;

	// Displayed board
	Board *_board;

	// Should the display be drawn?
	bool _draw = true;

	// Should the moves be clicked through the chess.com binding?
	bool _click_bind = false;

	// Automatic play mode bound to chess.com
	bool _binding_full = false; // To read every move of the game
	bool _binding_solo = false; // To read only the moves of the player at the bottom

	// Time interval between two chess.com checks
	int _binding_interval_check = 100;

	// Time of the last check
	clock_t _last_binding_check = clock();

	// Move read by the binding
	Move _binding_move;

	// Coordinates of the board on chess.com
	int _binding_left = 108; // (+10 with an evaluation bar)
	int _binding_top = 219;
	int _binding_right = 851;
	int _binding_bottom = 962;

	// Coordinates of the board for the binding
	//SimpleRectangle _binding_coord;

	// Initial time of the players
	//clock_t _initial_time_white = 180000;
	//clock_t _initial_time_black = 180000;
	clock_t _initial_time_white = 60000;
	clock_t _initial_time_black = 60000;

	// Time of the players
	clock_t _time_white;
	clock_t _time_black;

	// Increment
	clock_t _time_increment_white = 0;
	clock_t _time_increment_black = 0;

	//clock_t _time_increment_white = 15000;
	//clock_t _time_increment_black = 15000;

	// Grogros analysis mode
	bool _grogros_analysis = false;

	// Is the clock enabled?
	bool _time = false;

	// For time management
	clock_t _last_move_clock;

	// Player to move at the last check (for the clocks)
	bool _last_player = true;

	// Arrow display: shows the winning chances (true) or the evaluation (false)
	bool _display_win_chances = true;

	// Text for the timers
	TextBox _white_time_text_box;
	TextBox _black_time_text_box;

	// Parameters of the Monte Carlo search
	//double _alpha = 0.0075;
	//double _beta = 2.5;
	//double _gamma = 0.65;
	double _alpha = 0.005; // Raises the weight of the evaluation (self-play tested: +126 Elo vs 0.00001)
	//double _alpha = 0.00001; // Raises the weight of the evaluation
	double _beta = 5.0; // Raises the weight of the win rate
	double _gamma = 1.10; // Raises the diversity of the explored moves

	// Depth of the search
	//int _quiescence_depth = 4; // Looks at every check and capture
	//int _quiescence_depth = 6;
	int _quiescence_depth = 10;
	//int _max_depth = 10; // Only for the stand pats on the remaining checks and captures

	bool _explore_checks = true; // FIXME? do the checks really have to be explored?
	bool _tt_main_search = false; // #11 Plan A - TT in the main search (runtime A/B, default OFF)
	bool _tt_node_dag = true; // #11 Plan B - transposition DAG (default ON, +8/5000 puzzles vs OFF)

	// Have the player names been added to the PGN?
	bool _named_pgn = false;
	bool _timed_pgn = false;

	// PGN display

	// TODO: a player class with name, elo, country, url, title...
	// Players
	string _white_player = "White";
	string _black_player = "Black";

	// FEN of the initial position
	string _initial_fen;

	// FEN of the current position
	string _current_fen;

	// PGN of the game
	string _pgn;

	// Time control
	string _time_control;

	// Global PGN
	string _global_pgn;

	// Titles of the players
	string _white_title;
	string _black_title;

	// Elo of the players
	string _white_elo;
	string _black_elo;

	// URL of the players (for the images)
	string _white_url;
	string _black_url;

	// Country of the players
	string _white_country;
	string _black_country;

	// Date of the game
	string _date;

	// Elo of GrogrosZero
	string _grogros_zero_elo = "2300";

	// Bot name of GrogrosZero
	string _grogros_zero_name = "GrogrosZero";

	// Version of GrogrosZero
	string _grogros_zero_version = "1.0";

	// Evaluator used to display the evaluation
	Evaluator* _grogros_eval = new Evaluator();
	//Evaluator* _grogros_eval = new Evaluator(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

	// Do the variations need to be refreshed in the GUI?
	bool _update_variants = false;

	// Position selected in the GUI
	Pos _selected_pos = Pos(-1, -1);

	// Position clicked in the GUI
	Pos _clicked_pos = Pos(-1, -1);

	// Position of the mouse
	Vector2 _mouse_pos;

	// Position of the right-clicked square
	Pos _right_clicked_pos = { -1, -1 };

	// Pending promotion: a pawn reached the last rank, waiting for the
	// user to pick Q/R/B/N in the overlay picker
	bool _promotion_pending = false;
	Move _promotion_move;
	uint8_t _promotion_choices[4] = { PROMO_QUEEN, PROMO_ROOK, PROMO_BISHOP, PROMO_KNIGHT };
	int _promotion_choice_count = 0;

	// Is the mouse clicked?
	bool _clicked = false;

	// Vector of the GrogrosZero arrows: a vector of moves
	vector<Move> _grogros_arrows;

	// TODO: threads (for the parallelization)
	// GUI thread

	// GrogrosZero thread
	thread _thread_grogros_zero;

	// Threads for the child boards of GrogrosZero
	vector<thread> _threads_grogros_zero;

	// --- Background computation thread ---
	// Uses Windows HANDLE with 16MB stack (std::thread defaults to 1MB, too small for quiescence recursion)
	void* _compute_thread_handle = nullptr;
	std::atomic<bool> _compute_running{ false };
	std::atomic<bool> _compute_done{ false };
	std::atomic<bool> _compute_continue{ false };
	std::mutex _tree_mutex;
	double _compute_budget_s = 0.1;

	// Search tree, variations played in the PGN
	GameTree _game_tree;

	// Number of FPS
	int _max_fps = 180;
	int _target_fps = 60;
	clock_t _last_drawing_time = 0;

	// Background colour
	Color _background_color = { 25, 25, 25, 255 };

	// Colour of the text rectangle
	Color _background_text_color = { 0, 0, 0, 255 };

	// Colours of the text
	Color _text_color = { 255, 75, 75, 255 };
	Color _text_color_dark = { 200, 50, 50, 255 };
	Color _text_color_light = { 200, 200, 200, 255 };
	Color _text_color_blue = { 150, 150, 200, 255 };
	Color _text_color_info = { 140, 140, 140, 255 };

	// Colours of the board
	Color _board_color_light = { 180, 150, 115, 255 };
	Color _board_color_dark = { 109, 75, 54, 255 };

	// Highlight colour of the squares
	Color _highlight_color = { 255, 255, 100, 150 };

	// Selection colour of the squares
	Color _select_color = { 50, 225, 50, 100 };

	// Colour of the squares of the last played move
	Color _last_move_color = { 220, 150, 50, 125 };

	// Colour of the pre-move square
	Color _pre_move_color = { 220, 30, 30, 125 };

	// Colour of the arrows
	Color _arrow_color = { 255, 225, 0, 150 };

	// Colour of the sliders
	Color _slider_color = { 200, 200, 200, 100 };
	Color _slider_background_color = { 100, 100, 100, 75 };

	// Colours of the evaluation bar
	Color _eval_bar_color_light = { 224, 206, 186, 255 };
	Color _eval_bar_color_gray = { 141, 128, 117, 255 };
	Color _eval_bar_color_dark = { 57, 50, 47, 255 };

	// Thickness of the arrows (relative to the size of a square)
	float _arrow_scale = 0.125f;
	float _arrow_thickness = 50.0f;

	// Are the arrows displayed? (not when the user wants to play against the AI, for instance)
	bool _drawing_arrows = true;

	// Node percentage above which a move is shown
	float _arrow_rate = 0.03f;

	// Whether the initialization has been done
	bool _loaded_resources = false;

	// Textures and images
	Image _piece_images[12];
	Texture2D _piece_textures[12];

	// Icon
	Image _icon;

	// Grogros's head
	Image _grogros_image;
	Texture2D _grogros_texture;
	float _grogros_size;

	// Cursor
	Image _cursor_image;
	Texture2D _cursor_texture;
	float _cursor_size = 64.0f;

	// Mini pieces
	Image _mini_piece_images[12];
	Texture2D _mini_piece_textures[12];
	int _mini_piece_size;

	// Sounds
	Sound _move_sound;
	Sound _castle_sound;
	Sound _check_sound;
	Sound _capture_sound;
	Sound _checkmate_sound;
	Sound _stalemate_sound;
	Sound _game_begin_sound;
	Sound _game_end_sound;
	Sound _promotion_sound;

	// Size of the board relative to the window
	float _board_scale = 0.7f;
	float _board_size;
	float _board_padding_x;
	float _board_padding_y;

	// Size of the pieces
	float _tile_size;
	float _piece_size;
	float _piece_scale = 0.8f;

	// Shader for the selected piece
	Shader _selected_shader;

	// Standard text size
	float _text_size;

	// Text font
	Font _text_font;

	// Shader for the text
	Shader _text_shader;

	// Font for the chess symbols
	Font _chess_font;

	// Spacing between the characters
	float _font_spacing = 0.03f;

	// Orientation of the board
	bool _board_orientation = true;

	// Highlighted squares
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

	// Computation of the number of visited nodes
	int _visited_nodes;

	// Time computation
	clock_t _begin_time;

	// Values of the sliders
	float _pgn_slider = 0.0f;
	float _monte_carlo_slider = 0.0f;
	float _variants_slider = 0.0f;

	// Pre-move
	//int pre_move[4] = { -1, -1, -1, -1 };

	// Evaluation to show on the evaluation bar
	float _global_eval = 0.0f;
	string _global_eval_text = "+0.0";

	// WDL
	WDL _wdl;

	// Piece values for the GUI display (nothing/king, pawn, knight, bishop, rook, queen)
	const int _piece_GUI_values[6] = { 0, 1, 3, 3, 5, 9 };

	// Missing material
	const int _base_material[6] = { 0, 8, 2, 2, 2, 1 };
	int _missing_w_material[6] = { 0, 0, 0, 0, 0, 0 };
	int _missing_b_material[6] = { 0, 0, 0, 0, 0, 0 };

	// Alphabet of size 8
	const string _abc8 = "abcdefgh";

	// Arrows on the chessboard
	vector<vector<int>> _arrows_array;

	// Evaluation components to display in the GUI
	string _eval_components;

	// Grey
	const Color _gray = { 100, 100, 100, 255 };

	// Dark grey
	const Color _dark_gray = { 50, 50, 50, 255 };

	// Node of the exploration tree
	Node *_root_exploration_node;

	// Display of the variations
	string _exploration_variants = "";

	// Number of nodes per frame for the exploration
	const int _nodes_per_frame = 1000;

	// Symbols of the pieces
	const string P_symbol = "\xC4\x80";
	const string N_symbol = "\xC4\x81";
	const string B_symbol = "\xC4\x82";
	const string R_symbol = "\xC4\x83";
	const string Q_symbol = "\xC4\x84";
	const string K_symbol = "\xC4\x85";
	const string p_symbol = "\xC4\x86";
	const string n_symbol = "\xC4\x87";
	const string b_symbol = "\xC4\x88";
	const string r_symbol = "\xC4\x89";
	const string q_symbol = "\xC4\x8A";
	const string k_symbol = "\xC4\x8B";

	// Chess websites
	vector<ChessSite> _chess_sites;

	// Current chess website
	ChessSite _current_site;

	// Sounds to use
	string _sounds_path = "resources/sounds/lisp/";

	// Size of the buffers: adaptive sizing at startup
	// (compute_pool_sizing from the available physical RAM - cf. buffer.h)

	// Constructors

	// Default
	GUI();

	// Functions

	// Sets up the chess.com binding for a new game (and gets ready to play with GrogrosZero)
	bool new_bind_game();

	// Sets up the chess.com binding to analyse a new game
	bool new_bind_analysis();

	// Builds the global PGN
	bool update_global_pgn();

	// Updates the time control of the PGN
	bool update_time_control();

	// Starts the clock
	void start_time();

	// Stops the clock
	void stop_time();

	// Updates the clocks of the players
	void update_time();

	// Resets the PGN
	bool reset_pgn();

	// Updates the date of the PGN
	bool update_date();

	// Starts the GrogrosZero threads
	//bool thread_grogros_zero(Evaluator* eval, int nodes);

	// Runs Grogros on a thread
	//bool grogros_zero_threaded(Evaluator* eval, int nodes);

	// Removes the last move from the PGN
	bool remove_last_move_PGN();

	// Draws the arrows from the values of the Monte Carlo algorithm
	//void draw_monte_carlo_arrows();

	// Draws the arrows from the values of the Monte Carlo algorithm
	void draw_exploration_arrows();

	// Draws the arrow of a move
	void draw_arrow(const Move move, const bool player, Color c, float thickness = -1.0f, const bool use_value = false, const float avg_score = 0.0f, const int mate = 0, const bool is_most_explored = false, const bool is_best_eval = false);

	// Colour of the arrow depending on the move (on its node count)
	Color move_color(int, int, bool is_quiescence) const;

	// Loads the textures
	void load_resources();

	// Scales the images to the right size
	void resize_GUI();

	// Applies the new dimensions of the window
	void get_window_size();

	// Returns whether the player is currently moving (so the AI stops thinking then, otherwise it lags)
	bool is_playing() const;

	// Changes the arrow display mode (yes/no)
	void switch_arrow_drawing();

	// Displays a text inside a given area, with a slider
	void slider_text(const string&, float, float, float, float, float size, float* slider_value, Color t_color, float slider_width = -1.0f, float slider_height = -1.0f);

	// Returns the orientation of the board
	bool get_board_orientation() const;

	// Returns whether the mouse cursor is inside the rectangle
	bool is_cursor_in_rect(Rectangle);

	// Draws a rectangle from floating-point coordinates
	bool draw_rectangle(float, float, float, float, Color);

	// Draws a rectangle from floating-point coordinates, using the start and end coordinates
	bool draw_rectangle_from_pos(float, float, float, float, Color);

	// Draws a circle from floating-point coordinates
	void draw_circle(float, float, float, Color);

	// Draws a line from floating-point coordinates
	void draw_line_ex(float, float, float, float, float, Color);

	// Draws a Bezier curve from floating-point coordinates
	void draw_line_bezier(float, float, float, float, float, Color);

	// Draws a texture from floating-point coordinates
	void draw_texture(const Texture&, float, float, Color);

	// Displays the evaluation bar
	void draw_eval_bar(float eval, WDL wdl, float avg_score, const string&, float, float, float, float, float max_eval, Color white, Color gray, Color black, float max_height = -1.0f);

	// Removes the highlight from every square
	void remove_highlighted_tiles();

	// Selects a square
	void select_tile(int, int);

	// Highlights a square (or un-highlights it)
	void highlight_tile(int, int);

	// Deselects
	void unselect();

	// From coordinates on the board
	void draw_simple_arrow_from_coord(int, int, int, int, float, Color);

	// Returns the square matching a position in the GUI
	Pos get_pos_from_GUI(float, float);

	// Changes the orientation of the board
	void switch_orientation();

	// Helper for the board display (returns i if board_orientation is set, 7 - i otherwise)
	int orientation_index(int) const;

	// Plays a move, keeping the GrogrosZero search
	bool play_move_keep(Move move);

	// Intercepts a user move (click or drag): opens the promotion picker
	// when the move is a pawn reaching the last rank, plays it otherwise.
	// Returns false when the move is illegal.
	bool play_user_move(int start_row, int start_col, int end_row, int end_col);

	// Completes a pending promotion with the chosen piece (PROMO_*)
	void complete_promotion(uint8_t promo_piece);

	// Cancels a pending promotion
	void cancel_promotion();

	// Handles a click while the promotion picker is open (true: consumed)
	bool handle_promotion_click();

	// Draws the promotion picker overlay
	void draw_promotion_picker();

	// Returns the type of the selected piece
	uint8_t selected_piece() const;

	// Returns the type of the piece the mouse has just clicked on
	uint8_t clicked_piece() const;

	// Starts a GrogrosZero analysis
	void grogros_analysis(int nodes = -1);

	// Runs a puzzle headlessly with the GUI's parameters, prints live results to cout
	void run_puzzle_headless(double time_s = 0.1);

	// Background computation thread: runs grogros_zero in a tight loop
	void compute_worker();

	// Starts background computation with the given time budget
	void start_compute(double time_s);

	// Stops background computation and waits for it to finish
	void stop_compute();

	// Loads a position from a FEN
	void load_FEN(const string fen, bool display = true);

	// Resets the game
	void reset_game();

	// Compares two Grogros analysis arrows
	bool compare_arrows(const Move m1, const Move m2) const;

	// Returns the date in the 'yyyymmdd' format
	string get_date();

	// Updates the bot name of GrogrosZero
	void update_grogros_zero_name();

	// Plays the GrogrosZero move or not, depending on the time left
	void play_grogros_zero_move(float time_proportion_per_move = 0.02f);

	// Initializes the colours of the chess websites
	void init_chess_sites();

	// Updates the binding move from the online board
	bool update_binding_move();

	// Evaluates (and displays the components)
	void evaluate_position(bool display = true, bool static_only = false);

	// Initializes the buffers
	void init_buffers() const;

	// Resets the buffers
	void reset_buffers() const;

	// Draws the GUI
	void draw();
};

// Instantiation of the global GUI
extern GUI main_GUI;