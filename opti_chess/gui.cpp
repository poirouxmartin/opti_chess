#include "gui.h"
#include "buffer.h"
#include "useful_functions.h"
#include "zobrist.h"
#include "windows_tests.h"
#include <wchar.h>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include "ranges"
#include <cstdarg>
#include <regex>
#include <fstream>
#include <iomanip>

// Win32 thread priority + thread creation (forward declarations to avoid windows.h / raylib DrawTextEx conflict)
extern "C" {
	__declspec(dllimport) void* __stdcall GetCurrentThread();
	__declspec(dllimport) int __stdcall SetThreadPriority(void* hThread, int nPriority);
	__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void* hHandle, unsigned long dwMilliseconds);
	__declspec(dllimport) int __stdcall CloseHandle(void* hObject);
}
#ifndef THREAD_PRIORITY_HIGHEST
#define THREAD_PRIORITY_HIGHEST 2
#endif
#ifndef INFINITE
#define INFINITE 0xFFFFFFFF
#endif

// --- Debug logging ---
bool g_debug = true;
static ofstream g_debug_file;

static void ensure_debug_file() {
	if (g_debug && !g_debug_file.is_open()) {
		g_debug_file.open("opti_chess_debug.log", ios::app);
		g_debug_file << "\n=== session start ===" << endl;
	}
}

void debug_log(const char* fmt, ...) {
	if (!g_debug) return;
	ensure_debug_file();
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	g_debug_file << buf << endl;
	g_debug_file.flush();
}

// FEN extraction helper for puzzle lookup
static string extract_fen_from_line(const string& line) {
	static const std::regex fen_regex(R"((?:[rnbqkpRNBQKP1-8]+(?:\/[rnbqkpRNBQKP1-8]+){7})\s+[wb]\s+(?:-|[KQkq]{1,4})\s+(?:-|[a-h][1-8])\s+\d+\s+\d+)");
	std::smatch m;
	if (std::regex_search(line, m, fen_regex)) {
		return m.str(0);
	}
	return string();
}

static string trim_copy(string s) {
	const char* ws = " \t\n\r";
	size_t start = s.find_first_not_of(ws);
	if (start == string::npos) return string();
	size_t end = s.find_last_not_of(ws);
	return s.substr(start, end - start + 1);
}

// Updates the clocks of the players
void GUI::update_time() {
	// Should the clock still be updated when it is disabled?
	if (!_time)
		return;

	if (_last_player)
		_time_white -= clock() - _last_move_clock;
	else
		_time_black -= clock() - _last_move_clock;

	_last_move_clock = clock();

	// Time management
	if (_board->_player != _last_player) {
		if (_board->_player) {
			_time_black -= clock() - _last_move_clock - _time_increment_black;
			//_pgn += " {[%clk " + clock_to_string(_time_black, true) + "]}";
		}
		else {
			_time_white -= clock() - _last_move_clock - _time_increment_white;
			//_pgn += " {[%clk " + clock_to_string(_time_white, true) + "]}";
		}

		_last_move_clock = clock();
	}

	_last_player = _board->_player;
}

// Starts the clock
void GUI::start_time() {
	update_time_control();
	_time = true;
	_last_move_clock = clock();
}

// Stops the clock
void GUI::stop_time() {
	update_time_control();
	update_time();
	_time = false;
}

// GUI constructor
GUI::GUI() {
}

// GUI
GUI main_GUI;

// Sets up the binding with the chess website for a new game
bool GUI::new_bind_game() {
	const int orientation = bind_board_orientation(_binding_left, _binding_top, _binding_right, _binding_bottom, _current_site);

	if (orientation == -1)
		return false;

	reset_game();

	if (get_board_orientation() != orientation)
		switch_orientation();

	if (orientation) {
		// White player
		_white_player = _grogros_zero_name;
		_white_title = "BOT";
		_white_elo = _grogros_zero_elo;
		_white_url = "https://images.chesscomfiles.com/uploads/v1/user/284728633.4af59e2f.50x50o.0c8cdf830b69.png";
		_white_country = "57";

		// Black player
		_black_player = _current_site._name + " player";
		_black_title = "";
		_black_elo = "";
		_black_url = "";
		_black_country = "";
	}
	else {
		// White player
		_white_player = _current_site._name + " player";
		_white_title = "";
		_white_elo = "";
		_white_url = "";
		_white_country = "";

		// Black player
		_black_player = _grogros_zero_name;
		_black_title = "BOT";
		_black_elo = _grogros_zero_elo;
		_black_url = "https://images.chesscomfiles.com/uploads/v1/user/284728633.4af59e2f.50x50o.0c8cdf830b69.png";
		_black_country = "57";
	}

	_binding_solo = true;
	_binding_full = false;
	_click_bind = true;
	if (!monte_board_buffer._init || !monte_node_buffer._init) {
		const PoolSizing ps = compute_pool_sizing();
		if (!monte_board_buffer._init)
			monte_board_buffer.init(ps.board_length);
		if (!monte_node_buffer._init)
			monte_node_buffer.init(ps.node_length);
	}
	start_time();
	_grogros_analysis = false;

	return true;
}

// Sets up the chess.com binding to analyse a new game
bool GUI::new_bind_analysis() {
	const int orientation = bind_board_orientation(_binding_left, _binding_top, _binding_right, _binding_bottom, _current_site);

	if (orientation == -1)
		return false;

	reset_game();

	if (get_board_orientation() != orientation)
		switch_orientation();

	if (orientation) {
		_white_player = "chess.com player 1";
		_black_player = "chess.com player 2";
	}
	else {
		_white_player = "chess.com player 2";
		_black_player = "chess.com player 1";
	}

	_binding_solo = false;
	_binding_full = true;
	_click_bind = false;
	if (!monte_board_buffer._init || !monte_node_buffer._init) {
		const PoolSizing ps = compute_pool_sizing();
		if (!monte_board_buffer._init)
			monte_board_buffer.init(ps.board_length);
		if (!monte_node_buffer._init)
			monte_node_buffer.init(ps.node_length);
	}
	_grogros_analysis = true;

	return true;
}

// Builds the global PGN
bool GUI::update_global_pgn()
{
	_global_pgn = "";

	// Headers

	// Joueurs
	if (!_white_player.empty())
		_global_pgn += "[White \"" + _white_player + "\"]\n";
	if (!_black_player.empty())
		_global_pgn += "[Black \"" + _black_player + "\"]\n";

	// Titles of the players
	if (!_white_title.empty())
		_global_pgn += "[WhiteTitle \"" + _white_title + "\"]\n";
	if (!_black_title.empty())
		_global_pgn += "[BlackTitle \"" + _black_title + "\"]\n";

	// Elo of the players
	if (!_white_elo.empty())
		_global_pgn += "[WhiteElo \"" + _white_elo + "\"]\n";
	if (!_black_elo.empty())
		_global_pgn += "[BlackElo \"" + _black_elo + "\"]\n";

	// URL of the players
	if (!_white_url.empty())
		_global_pgn += "[WhiteUrl \"" + _white_url + "\"]\n";
	if (!_black_url.empty())
		_global_pgn += "[BlackUrl \"" + _black_url + "\"]\n";

	// Country of the players
	if (!_white_country.empty())
		_global_pgn += "[WhiteCountry \"" + _white_country + "\"]\n";
	if (!_black_country.empty())
		_global_pgn += "[BlackCountry \"" + _black_country + "\"]\n";

	// Cadence
	if (!_time_control.empty())
		_global_pgn += "[TimeControl \"" + _time_control + "\"]\n";

	// Imported FEN
	if (!_initial_fen.empty())
		_global_pgn += "[FEN \"" + _initial_fen + "\"]\n";

	// Date
	if (!_date.empty())
		_global_pgn += "[Date \"" + _date + "\"]\n";

	// Addition of the PGN of the game
	_global_pgn += _pgn;

	return true;
}

// Updates the time control of the PGN
bool GUI::update_time_control()
{
	_time_control = to_string(static_cast<int>(max(_time_white, _time_black) / 1000)) + " + " + to_string(static_cast<int>(max(_time_increment_white, _time_increment_black) / 1000));
	return true;
}

// Resets the PGN
bool GUI::reset_pgn()
{
	update_date();
	_pgn = "";
	_global_pgn = "";
	_initial_fen = "";

	return  true;
}

// Updates the date of the PGN
bool GUI::update_date() {
	const time_t current_time = time(nullptr);
	tm local_time;
	localtime_s(&local_time, &current_time);

	const int year = local_time.tm_year + 1900;
	const int month = local_time.tm_mon + 1;
	const int day = local_time.tm_mday;

	_date = to_string(year) + "." + to_string(month) + "." + to_string(day);

	return true;
}

// Starts the GrogrosZero threads
//bool GUI::thread_grogros_zero(Evaluator* eval, int nodes)
//{
//	// Initialization of the buffer for GrogrosZero, if needed
//	if (!monte_buffer._init)
//		monte_buffer.init();
//
//
//	// Run grogros on every child node for the initialization
//	//_board->grogros_zero(eval, _board->_got_moves, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr, 0);
//
//	_threads_grogros_zero.clear();
//
//	for (int i = 0; i < _board->_got_moves; i++)
//		_threads_grogros_zero.emplace_back(&Board::grogros_zero, &monte_buffer._heap_boards[_board->_index_children[i]], eval, nodes, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr, 0);
//
//
//	//_threads_grogros_zero.emplace_back(&Board::grogros_zero, &_board, eval, nodes, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr, 0);
//	//_threads_grogros_zero.emplace_back(&Board::grogros_zero, &_board, eval, nodes, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr, 0);
//
//	
//	for (auto& thread : _threads_grogros_zero) {
//		thread.join();
//		//thread.detach();
//		//cout << "Thread done" << endl;
//	}
//
//	// TODO:
//	// The Monte Carlo times of every child have to be summed again (same for the quiescence nodes)
//	// Every variation has to be updated too
//
//	// Run grogros again on 1 node (to refresh the values)
//	//_board->grogros_zero(eval, 100, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr, 0);
//
//	return true;
//}

// Runs grogros on a thread
//bool GUI::grogros_zero_threaded(Evaluator* eval, int nodes) {
//	// Initialization of the buffer for GrogrosZero, if needed
//	if (!monte_buffer._init)
//		monte_buffer.init();
//
//	// Run grogros on a thread
//	_thread_grogros_zero = thread(&Board::grogros_zero, &_board, eval, nodes, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr, 0);
//
//	_thread_grogros_zero.detach();
//
//	return true;
//}

// Removes the last move from the PGN
bool GUI::remove_last_move_PGN()
{
	// TODO	

	return false;
}

// Draws the arrows from the values of the Monte Carlo algorithm
void GUI::draw_exploration_arrows()
{
	// Vector of arrows to display
	_grogros_arrows.clear();

	if (!_root_exploration_node || !_root_exploration_node->_board) {
		debug_log("[draw_exploration_arrows] root or root board null, skipping");
		return;
	}

	// Use the pre-computed snapshot (taken under _tree_mutex in draw())
	if (!_tree_snapshot.valid)
		return;

	const Move best_move = _tree_snapshot.best_move;
	const Move best_eval_move = _tree_snapshot.best_eval_move;

	// Is a piece selected?
	const bool is_selected = _selected_pos.row != -1 && _selected_pos.col != -1;

	// Build a vector with the moves explored by GrogrosZero
	vector<Move> iterated_moves_vector;

	for (auto const& entry : _tree_snapshot.arrows) {
		// If a piece is selected, draw every arrow for that piece
		if (is_selected) {
			if (_selected_pos.row == entry.move.start_row && _selected_pos.col == entry.move.start_col)
				iterated_moves_vector.push_back(entry.move);
		}

		// Otherwise, draw the arrows of the most explored moves
		else {
			// The highlighted moves are not added for now
			if (entry.move == best_move || entry.move == best_eval_move)
				continue;

			// Has the move been explored by GrogrosZero, or only by the quiescence?
			if (_tree_snapshot.iterations > 0) {
				if (static_cast<float>(entry.chosen_iterations) / static_cast<float>(_tree_snapshot.iterations) > _arrow_rate) {
					iterated_moves_vector.push_back(entry.move);
				}
			}
			else {
				if (_tree_snapshot.nodes > 0) {
					iterated_moves_vector.push_back(entry.move);
				}
			}
		}
	}

	// Sort the moves by node count, and for a more readable display
	std::ranges::sort(iterated_moves_vector.begin(), iterated_moves_vector.end(), [this](const Move m1, const Move m2) {
		return this->compare_arrows(m1, m2); }
	);

	if (!is_selected) {
		// Add the moves to display in every case
		iterated_moves_vector.push_back(best_eval_move);

		if (best_eval_move != best_move) {
			iterated_moves_vector.push_back(best_move);
		}
	}

	// Draw the arrows using snapshot data
	for (const Move move : iterated_moves_vector) {
		// Find the arrow entry in the snapshot
		auto it = std::find_if(_tree_snapshot.arrows.begin(), _tree_snapshot.arrows.end(),
			[&](const TreeSnapshot::ArrowEntry& e) { return e.move == move; });
		if (it == _tree_snapshot.arrows.end())
			continue;

		const int mate = _root_exploration_node->_board->is_eval_mate(it->eval_value);
		draw_arrow(move, _root_exploration_node->_board->_player, move_color(it->chosen_iterations, _tree_snapshot.iterations, it->child_iterations == 0), -1.0f, true, it->eval_avg_score, mate, it->is_best_move, it->is_best_eval_move);
	}
}

// Returns the square matching a position in the GUI
Pos GUI::get_pos_from_GUI(const float x, const float y) {
	if (!is_in(x, _board_padding_x, _board_padding_x + _board_size) || !is_in(y, _board_padding_y, _board_padding_y + _board_size))
		return Pos(-1, -1);
	else
		return Pos(orientation_index(8 - (y - _board_padding_y) / _tile_size), orientation_index((x - _board_padding_x) / _tile_size));
}

// Changes the orientation of the board
void GUI::switch_orientation() {
	_board_orientation = !_board_orientation;
}

// Helper for the board display (returns i if board_orientation is set, 7 - i otherwise)
int GUI::orientation_index(const int i) const {
	if (_board_orientation)
		return i;
	return 7 - i;
}

// Draws the arrow of a move
void GUI::draw_arrow(const Move move, const bool player, Color c, float thickness, const bool use_value, const float avg_score, const int mate, const bool is_most_explored, const bool is_best_eval)
{
	const uint8_t start_row = move.start_row;
	const uint8_t start_col = move.start_col;
	const uint8_t end_row = move.end_row;
	const uint8_t end_col = move.end_col;

	if (thickness == -1.0f)
		thickness = _arrow_thickness;

	const float x1 = _board_padding_x + _tile_size * orientation_index(start_col) + _tile_size / 2;
	const float y1 = _board_padding_y + _tile_size * orientation_index(7 - start_row) + _tile_size / 2;
	const float x2 = _board_padding_x + _tile_size * orientation_index(end_col) + _tile_size / 2;
	const float y2 = _board_padding_y + _tile_size * orientation_index(7 - end_row) + _tile_size / 2;

	// Transparence nulle
	//c.a = 255;

	int d_row = end_row - start_row;
	int d_col = end_col - start_col;

	bool is_knight_move = (abs(d_row) == 2 && abs(d_col) == 1) || (abs(d_row) == 1 && abs(d_col) == 2);

	// Outline for the most explored move
	if (is_most_explored) {
		if (is_knight_move)
			draw_line_bezier(x1, y1, x2, y2, thickness * 1.4f, BLACK);
		else
			draw_line_ex(x1, y1, x2, y2, thickness * 1.4f, BLACK);
		draw_circle(x1, y1, thickness * 1.2f, BLACK);
		draw_circle(x2, y2, thickness * 2.0f * 1.1f, BLACK);
	}
	
	// Outline for the move with the best evaluation
	if (is_best_eval) {
		if (is_knight_move)
			draw_line_bezier(x1, y1, x2, y2, thickness * 1.4f, WHITE);
		else
			draw_line_ex(x1, y1, x2, y2, thickness * 1.4f, WHITE);
		draw_circle(x1, y1, thickness * 1.2f, WHITE);
		draw_circle(x2, y2, thickness * 2.0f * 1.1f, WHITE);
	}

	// "Arrow"
	if (is_knight_move)
		draw_line_bezier(x1, y1, x2, y2, thickness, c);
	else
		draw_line_ex(x1, y1, x2, y2, thickness, c);
	draw_circle(x1, y1, thickness, c);
	draw_circle(x2, y2, thickness * 2.0f, c);

	// Add a value to the arrow
	if (use_value) {

		// Value to display
		char v[5];
		string eval;

		if (mate != 0) {
			if (mate * (player ? 1 : -1) > 0)
				eval = "+";
			else
				eval = "-";
			eval += "M";
			eval += to_string(abs(mate));
			snprintf(v, sizeof(v), eval.c_str());
		}
		else {
			if (_display_win_chances) {
				sprintf_s(v, "%d", float_to_int(100.0f * (player ? avg_score : 1.0f - avg_score)));
			}
		}

		float size = thickness * 1.85f;
		const float max_size = thickness * 3.25f;
		float width = MeasureTextEx(_text_font, v, size, _font_spacing * size).x;
		if (width > max_size) {
			size = size * max_size / width;
			width = MeasureTextEx(_text_font, v, size, _font_spacing * size).x;
		}
		const float height = MeasureTextEx(_text_font, v, size, _font_spacing * size).y;

		Color t_c = ColorAlpha(BLACK, static_cast<float>(c.a) / 255.0f);
		DrawTextEx(_text_font, v, { x2 - width / 2.0f, y2 - height / 2.0f }, size, _font_spacing * size, BLACK);
	}

	// Add the arrow to the vector
	_grogros_arrows.push_back(move);

	return;
}

// Colour of the arrow depending on the move (on its node count)
Color GUI::move_color(const int explorations, const int total_explorations, bool is_quiescence) const {
	// When there is no exploration, display in white
	if (is_quiescence)
		return GRAY;

	const float x = static_cast<float>(explorations) / static_cast<float>(total_explorations);

	// Attenuation factor towards white
	const float white_attenuation = 0.3f;

	const auto red = static_cast<unsigned char>(255.0f * ((1 - white_attenuation) * ((x <= 0.2f) + (x > 0.2f && x < 0.4f) * (0.4f - x) / 0.2f + (x > 0.8f) * (x - 0.8f) / 0.2f) + white_attenuation));
	const auto green = static_cast<unsigned char>(255.0f * ((1 - white_attenuation) * ((x < 0.2f) * x / 0.2f + (x >= 0.2f && x <= 0.6f) + (x > 0.6f && x < 0.8f) * (0.8f - x) / 0.2f) + white_attenuation));
	const auto blue = static_cast<unsigned char>(255.0f * ((1 - white_attenuation) * ((x > 0.4f && x < 0.6f) * (x - 0.4f) / 0.2f + (x >= 0.6f)) + white_attenuation));

	//unsigned char alpha = 100 + 155 * explorations / total_explorations;
	const unsigned char alpha = 255;

	return { red, green, blue, alpha };
}

// Loads the textures
void GUI::load_resources() {
	cout << GetWorkingDirectory() << endl;

	// Pieces
	_piece_images[0] = LoadImage("resources/images/w_pawn.png");
	_piece_images[1] = LoadImage("resources/images/w_knight.png");
	_piece_images[2] = LoadImage("resources/images/w_bishop.png");
	_piece_images[3] = LoadImage("resources/images/w_rook.png");
	_piece_images[4] = LoadImage("resources/images/w_queen.png");
	_piece_images[5] = LoadImage("resources/images/w_king.png");
	_piece_images[6] = LoadImage("resources/images/b_pawn.png");
	_piece_images[7] = LoadImage("resources/images/b_knight.png");
	_piece_images[8] = LoadImage("resources/images/b_bishop.png");
	_piece_images[9] = LoadImage("resources/images/b_rook.png");
	_piece_images[10] = LoadImage("resources/images/b_queen.png");
	_piece_images[11] = LoadImage("resources/images/b_king.png");

	// Mini pieces
	_mini_piece_images[0] = LoadImage("resources/images/mini_pieces/w_pawn.png");
	_mini_piece_images[1] = LoadImage("resources/images/mini_pieces/w_knight.png");
	_mini_piece_images[2] = LoadImage("resources/images/mini_pieces/w_bishop.png");
	_mini_piece_images[3] = LoadImage("resources/images/mini_pieces/w_rook.png");
	_mini_piece_images[4] = LoadImage("resources/images/mini_pieces/w_queen.png");
	_mini_piece_images[5] = LoadImage("resources/images/mini_pieces/w_king.png");
	_mini_piece_images[6] = LoadImage("resources/images/mini_pieces/b_pawn.png");
	_mini_piece_images[7] = LoadImage("resources/images/mini_pieces/b_knight.png");
	_mini_piece_images[8] = LoadImage("resources/images/mini_pieces/b_bishop.png");
	_mini_piece_images[9] = LoadImage("resources/images/mini_pieces/b_rook.png");
	_mini_piece_images[10] = LoadImage("resources/images/mini_pieces/b_queen.png");
	_mini_piece_images[11] = LoadImage("resources/images/mini_pieces/b_king.png");

	// Loading of the sound
	_move_sound = LoadSound((_sounds_path + "move.mp3").c_str());
	_castle_sound = LoadSound((_sounds_path + "castle.mp3").c_str());
	_check_sound = LoadSound((_sounds_path + "check.mp3").c_str());
	_capture_sound = LoadSound((_sounds_path + "capture.mp3").c_str());
	_checkmate_sound = LoadSound((_sounds_path + "checkmate.mp3").c_str());
	_stalemate_sound = LoadSound((_sounds_path + "stalemate.mp3").c_str());
	_game_begin_sound = LoadSound((_sounds_path + "game_begin.mp3").c_str());
	_promotion_sound = LoadSound((_sounds_path + "promotion.mp3").c_str());

	// Text font
	//_text_font = LoadFontEx("resources/fonts/SFTransRobotics.otf", 128, nullptr, 1000);
	_text_font = LoadFontEx("resources/fonts/Montserrat-Bold.otf", 128, nullptr, 256);
	//_text_font = LoadFontEx("resources/fonts/Montserrat-Medium.ttf", 128, nullptr, 0);
	GenTextureMipmaps(&_text_font.texture);
	SetTextureFilter(_text_font.texture, TEXTURE_FILTER_TRILINEAR);
	//SetTextureFilter(_text_font.texture, TEXTURE_FILTER_ANISOTROPIC_4X);

	// Shader for the text
	//_text_shader = LoadShader(nullptr, "resources/shaders/font_sdf.fs");
	//_text_shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(_text_shader, "view");
	//_text_font.texture.sg

	// Icon
	_icon = LoadImage("resources/images/grogros_zero.png"); // TODO essayer de charger le .ico, pour que l'icone s'affiche tout le temps (pas seulement lors du build)
	SetWindowIcon(_icon);
	UnloadImage(_icon);

	// Grogros
	_grogros_image = LoadImage("resources/images/grogros_zero.png");

	// Curseur
	_cursor_image = LoadImage("resources/images/cursor_new.png");

	_loaded_resources = true;
}

// Scales the images and the texts of the GUI to the right size
void GUI::resize_GUI() {
	const int min_screen = min(_screen_height, _screen_width);
	_board_size = _board_scale * min_screen;
	_board_padding_y = (_screen_height - _board_size) / 4.0f;
	_board_padding_x = (_screen_height - _board_size) / 8.0f;

	_tile_size = _board_size / 8.0f;
	_piece_size = _tile_size * _piece_scale;
	_arrow_thickness = _tile_size * _arrow_scale;

	// Generation of the textures

	// Pieces
	for (int i = 0; i < 12; i++) {
		Image piece_image = ImageCopy(_piece_images[i]);
		ImageResize(&piece_image, _piece_size, _piece_size);
		Texture2D texture = LoadTextureFromImage(piece_image);
		//GenTextureMipmaps(&texture);
		//SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
		//SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
		_piece_textures[i] = texture;
	}

	// Size of the text
	_text_size = _board_size / 16.0f;

	// Grogros
	_grogros_size = _board_size / 16.0f;
	Image grogros_copy = ImageCopy(_grogros_image);
	ImageResize(&grogros_copy, _grogros_size, _grogros_size);
	_grogros_texture = LoadTextureFromImage(grogros_copy);

	// Curseur
	Image cursor_copy = ImageCopy(_cursor_image);
	ImageResize(&cursor_copy, _cursor_size, _cursor_size);
	_cursor_texture = LoadTextureFromImage(cursor_copy);

	// Mini pieces (for the count of the pieces captured during the game)
	_mini_piece_size = _text_size / 3;
	for (int i = 0; i < 12; i++) {
		Image mini_piece_image = ImageCopy(_mini_piece_images[i]);
		ImageResize(&mini_piece_image, _mini_piece_size, _mini_piece_size);
		_mini_piece_textures[i] = LoadTextureFromImage(mini_piece_image);
	}
}

// Applies the new dimensions of the window
void GUI::get_window_size() {
	_screen_width = GetScreenWidth();
	_screen_height = GetScreenHeight();
}

// Returns whether the player is currently moving (so the AI stops thinking then, otherwise it lags)
bool GUI::is_playing() const {
	const auto [x, y] = GetMousePosition();
	return (_selected_pos.row != -1 || x != _mouse_pos.x || y != _mouse_pos.y);
}

// Changes the arrow display mode (yes/no)
void GUI::switch_arrow_drawing() {
	_drawing_arrows = !_drawing_arrows;
}

// Displays a text inside a given area, with a slider
void GUI::slider_text(const string& s, float pos_x, float pos_y, float width, float height, float size, float* slider_value, Color t_color, float slider_width, float slider_height) {

	// FIXME *** test: if nothing changed in the text or the dimensions, this text could be cached, no?

	// Draw the canvas, a rectangle
	DrawRectangleRec({ pos_x, pos_y, width, height }, _background_text_color);

	// Size of the sliders
	if (slider_width == -1.0f)
		slider_width = _screen_width / 100.0f;

	if (slider_height == -1.0f)
		slider_height = height;

	// Split the text into equal parts

	// Estimate of the number of characters per line
	const Vector2 estimated_size = MeasureTextEx(_text_font, "A", size, _font_spacing * size);

	int split_length = static_cast<int>(width / estimated_size.x);
	int margin = split_length * 1.2f; // Pour forcer le split
	string new_string;
	int rows = 0;
	int k = 0;

	for (int i = 0; i < s.length(); i++) {
		if (s[i] == '\n' || (k >= split_length && s[i] == ' ') || k > margin) {
			new_string += "\n";
			k = 0;
			rows++;
			continue;
		}
		new_string += s[i];
		k++;
	}

	// Total vertical size of the text

	// Estimate of the vertical size of one line
	const float space_size = MeasureTextEx(_text_font, "\n", size, _font_spacing * size).y - estimated_size.y;

	float text_height = estimated_size.y + space_size * rows;

	// If the text takes more vertical room than the space allocated
	if (text_height > height) {

		// Number of lines to display
		int n_lines = 1 + (height - estimated_size.y) / space_size;

		// Number of the line to start from
		int starting_line = (rows + 1 - n_lines) * *slider_value;

		string final_text;
		int current_line = 0;

		for (int i = 0; i < new_string.length(); i++) {
			if (new_string.substr(i, 1) == "\n") {
				current_line++;
				if (current_line >= n_lines + starting_line)
					break;
			}

			if (current_line >= starting_line) {
				final_text += new_string[i];
			}
		}

		new_string = final_text;
		slider_height = height / sqrtf(rows - n_lines + 1) / 2;

		// Background
		Rectangle slider_background_rect = { pos_x + width - slider_width, pos_y, slider_width, height };
		DrawRectangleRec(slider_background_rect, _slider_background_color);

		// Slider
		Rectangle slider_rect = { pos_x + width - slider_width, pos_y + *slider_value * (height - slider_height), slider_width, slider_height };
		DrawRectangleRec(slider_rect, _slider_color);

		// Slide

		// With the mouse wheel
		if (is_cursor_in_rect({ pos_x, pos_y, width, height })) {
			*slider_value -= GetMouseWheelMove() * 3.0 / (rows - n_lines + 1);
			if (*slider_value < 0.0f)
				*slider_value = 0.0f;
			if (*slider_value > 1.0f)
				*slider_value = 1.0f;
		}
	}

	// Texte total
	const char* c = new_string.c_str();

	DrawTextEx(_text_font, c, { pos_x, pos_y }, size, _font_spacing * size, t_color);
}

// Returns the orientation of the board
bool GUI::get_board_orientation() const {
	return _board_orientation;
}

// Returns whether the mouse cursor is inside the rectangle
bool GUI::is_cursor_in_rect(const Rectangle rec) {
	_mouse_pos = GetMousePosition();
	return (is_in(_mouse_pos.x, rec.x, rec.x + rec.width) && is_in(_mouse_pos.y, rec.y, rec.y + rec.height));
}

// Draws a rectangle from floating-point coordinates
bool GUI::draw_rectangle(const float pos_x, const float pos_y, const float width, const float height, const Color color) {
	DrawRectangle(float_to_int(pos_x), float_to_int(pos_y), float_to_int(width + pos_x) - float_to_int(pos_x), float_to_int(height + pos_y) - float_to_int(pos_y), color);
	return true;
}

// Draws a rectangle from floating-point coordinates, using the start and end coordinates
bool GUI::draw_rectangle_from_pos(const float pos_x1, const float pos_y1, const float pos_x2, const float pos_y2, const Color color) {
	DrawRectangle(float_to_int(pos_x1), float_to_int(pos_y1), float_to_int(pos_x2) - float_to_int(pos_x1), float_to_int(pos_y2) - float_to_int(pos_y1), color);
	return true;
}

// Draws a circle from floating-point coordinates
void GUI::draw_circle(const float pos_x, const float pos_y, const float radius, const Color color) {
	DrawCircle(float_to_int(pos_x), float_to_int(pos_y), radius, color);
}

// Draws a line from floating-point coordinates
void GUI::draw_line_ex(const float x1, const float y1, const float x2, const float y2, const float thick, const Color color) {
	DrawLineEx({ static_cast<float>(float_to_int(x1)), static_cast<float>(float_to_int(y1)) }, { static_cast<float>(float_to_int(x2)), static_cast<float>(float_to_int(y2)) }, thick, color);
}

// Draws a Bezier curve from floating-point coordinates
void GUI::draw_line_bezier(const float x1, const float y1, const float x2, const float y2, const float thick, const Color color) {
	DrawLineBezier({ static_cast<float>(float_to_int(x1)), static_cast<float>(float_to_int(y1)) }, { static_cast<float>(float_to_int(x2)), static_cast<float>(float_to_int(y2)) }, thick, color);
}

// Draws a texture from floating-point coordinates
void GUI::draw_texture(const Texture& texture, const float pos_x, const float pos_y, const Color color) {
	DrawTexture(texture, float_to_int(pos_x), float_to_int(pos_y), color);
}

// Displays the evaluation bar
void GUI::draw_eval_bar(const float eval, WDL wdl, float avg_score, const string& text_eval, const float x, const float y, const float width, const float height, const float max_eval, const Color white, const Color gray, Color black, float max_height) {
	const bool is_mate = text_eval.find('M') != -1;

	// Maximum size of the bar
	if (max_height == -1.0f)
		max_height = 0.95f;

	// Truncate the evaluation to 2 digits at most
	// FIXME: this assumes the evaluation never goes above +100 (or +10000 in Grogros's equivalent)
	string eval_text = is_mate ? text_eval : text_eval.substr(0, min(4, static_cast<int>(text_eval.size())));
	if (eval_text[eval_text.size() - 1] == '.')
		eval_text = eval_text.substr(0, eval_text.size() - 1);

	const bool orientation = get_board_orientation();
	if (orientation) {
		draw_rectangle(x, y, width, height, black);
		draw_rectangle(x, y + wdl.lose_chance * height, width, wdl.draw_chance * height, gray);
		draw_rectangle(x, y + (1 - wdl.win_chance) * height, width, wdl.win_chance * height, white);
	}
	else {
		draw_rectangle(x, y, width, height, black);
		draw_rectangle(x, y + wdl.win_chance * height, width, wdl.draw_chance * height, gray);
		draw_rectangle(x, y, width, wdl.win_chance * height, white);
	}

	const float y_margin = (1 - max_height) / 4;
	const bool text_pos = (orientation ^ (eval < 0));
	float t_size = width / 2;
	Vector2 text_dimensions = MeasureTextEx(_text_font, eval_text.c_str(), t_size, _font_spacing);

	// Width the text has to take up
	float max_text_width = width * 0.9f;
	if (text_dimensions.x > max_text_width)
		t_size = t_size * max_text_width / text_dimensions.x;
	text_dimensions = MeasureTextEx(_text_font, eval_text.c_str(), t_size, _font_spacing);

	float text_pos_x = x + (width - text_dimensions.x) / 2.0f;
	float text_pos_y = y + (y_margin + text_pos * (1.0f - y_margin * 2.0f)) * (height * 0.95f) - text_dimensions.y * text_pos;

	DrawTextEx(_text_font, eval_text.c_str(), { text_pos_x, text_pos_y }, t_size, _font_spacing, (eval < 0) ? white : black);

	// Average score
	DrawTextEx(_text_font, score_string(avg_score).c_str(), { text_pos_x, text_pos_y + t_size }, t_size * 0.75f, _font_spacing, (eval < 0) ? white : black);

	// Renormalized evaluation
	DrawTextEx(_text_font, get_renormalized_evaluation(avg_score).c_str(), { text_pos_x, text_pos_y + 1.75f * t_size }, t_size * 0.75f, _font_spacing, (eval < 0) ? white : black);
}

// Removes the highlight from every square
void GUI::remove_highlighted_tiles() {
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++)
			_highlighted_array[i][j] = 0;
}

// Selects a square
void GUI::select_tile(int a, int b) {
	_selected_pos = Pos(a, b);
}

// Highlights a square (or un-highlights it)
void GUI::highlight_tile(const int a, const int b) {
	_highlighted_array[a][b] = 1 - _highlighted_array[a][b];
}

// Deselects
void GUI::unselect() {
	_selected_pos = Pos(-1, -1);
}

// From coordinates on the board
void GUI::draw_simple_arrow_from_coord(const int i1, const int j1, const int i2, const int j2, float thickness, Color c) {
	// cout << thickness << endl;
	if (thickness == -1.0f)
		thickness = _arrow_thickness;
	const float x1 = _board_padding_x + _tile_size * orientation_index(j1) + _tile_size / 2;
	const float y1 = _board_padding_y + _tile_size * orientation_index(7 - i1) + _tile_size / 2;
	const float x2 = _board_padding_x + _tile_size * orientation_index(j2) + _tile_size / 2;
	const float y2 = _board_padding_y + _tile_size * orientation_index(7 - i2) + _tile_size / 2;

	c.a = 255;

	// "Arrow"
	if (abs(j2 - j1) != abs(i2 - i1) && abs(j2 - j1) + abs(i2 - i1) == 3)
		draw_line_bezier(x1, y1, x2, y2, thickness, c);
	else
		draw_line_ex(x1, y1, x2, y2, thickness, c);

	//c.a = 255;

	draw_circle(x1, y1, thickness, c);
	draw_circle(x2, y2, thickness * 2.0f, c);
}

// Plays a move, keeping the GrogrosZero search
bool GUI::play_move_keep(Move move)
{
	debug_log("[play_move_keep] enter move=%d->%d promo=%d board=%p root=%p root_board=%p",
		(int)move.start_row * 8 + move.start_col,
		(int)move.end_row * 8 + move.end_col,
		(int)move.promo_piece,
		(void*)_board, (void*)_root_exploration_node,
		_root_exploration_node ? (void*)_root_exploration_node->_board : nullptr);

	if (!_board) {
		debug_log("[play_move_keep] CRITICAL: _board is null!");
		return false;
	}
	if (!_root_exploration_node) {
		debug_log("[play_move_keep] CRITICAL: _root_exploration_node is null!");
		return false;
	}

	stop_compute();

	_board->assign_move_flags(&move);

	// Make sure the move is legal
	if (!_board->is_legal(move))
		return false;

	// Play the sound of the move
	_board->play_move_sound(move);

	// Update the variations
	_update_variants = true;

	// Timestamp of the move
	clock_t move_timestamp = _board->_player ? _time_white : _time_black;
	string additional_time_str = _time ? clock_to_timestamp(move_timestamp, true) : "";

	// Tree of the game
	_game_tree._current_node->_board = *_board;
	_game_tree.add_child(move, additional_time_str);

	// FIXME: the real cases to distinguish:
	// have any moves been computed? -> yes/no
	// if so, is the played move one of them? -> yes/no

	if (_root_exploration_node->children_count() == 0) {
		// Simply play the move
		_board->make_move(move, false, true);

		// Update the search board
		//_root_exploration_node->_board = &_board;
	}

	// If the move was actually computed
		else {
		if (_root_exploration_node->_children.contains(move)) {
			Node* next_root = _root_exploration_node->_children[move]._node;

			debug_log("[play_move_keep] move found in children, next_root=%p parent_count=%d",
				(void*)next_root, next_root ? next_root->_parent_count : -1);

			if (!next_root || !next_root->_board) {
				debug_log("[play_move_keep] CRITICAL: next_root or its board is null!");
				return false;
			}

			// Former root: detached further down (replaced by next_root), not reused
			// in place -> to be recycled explicitly (approach B).
			Node* const old_root = _root_exploration_node;

			for (auto const& [m, child_link] : _root_exploration_node->_children) {
				if (m != move) {
					Node* child = child_link._node;
					child->_parent_count--;
					if (child->_parent_count <= 0) {
						debug_log("[play_move_keep] recycling child move=%d->%d child=%p parent_count=0 nodes=%d iters=%d children=%d",
							(int)m.start_row * 8 + m.start_col, (int)m.end_row * 8 + m.end_col,
							(void*)child, (int)child->_nodes, (int)child->_iterations,
							(int)child->_children.size());
						child->reset(true);
						// Enfant non choisi definitivement detache -> recyclage.
						recycle_detached_node(child);
					}
				}
			}

			//cout << "toto" << endl;

			// Save positions history BEFORE reset (reset clears it)
			auto saved_positions_history = _root_exploration_node->_board->_positions_history;

			// Reset the node (non-recursive): clears fields, resets board
			// internally, and clears _children.  Do NOT call reset_board()
			// explicitly — Node::reset already handles it.
			_root_exploration_node->reset(false);

			//cout << "tata" << endl;

			// The parent and all the children will have to be deleted (TODO)

			// Update the search node
			next_root->_parent_count--;
			_root_exploration_node = next_root;

			// Reassign _board BEFORE recycling the old root so that _board
			// is never a dangling pointer to a recycled buffer slot.
			_board = _root_exploration_node->_board;

			// Propagate game history to new root's board (child boards lose
			// _positions_history during copy_data in grogros_zero)
			_board->_positions_history = saved_positions_history;
			_board->_positions_history[_board->_zobrist_key]++;

			debug_log("[play_move_keep] new root: board=%p root=%p children=%d iters=%d",
				(void*)_board, (void*)_root_exploration_node,
				(int)_root_exploration_node->_children.size(),
				(int)_root_exploration_node->_iterations);

			// Former root, now orphaned (next_root is the new root):
			// recycling of its node + board (B_R), distinct from next_root.
			recycle_detached_node(old_root);
			//_root_exploration_node->_board = &_board;
		}

		// Otherwise, simply play the move
		else {
			// Delete every search
			_root_exploration_node->reset();
			_root_exploration_node->_is_active = true;

			// Simply play the move
			_board->make_move(move, false, true);
			_board->_is_active = true;

			// Update the search board
			//_root_exploration_node->_board = &_board;
		}
	}

	_root_exploration_node->_board = _board;
	reset_buffers(); // #6: systematic TT/node_map clear on position change

	_board->get_moves();

	//cout << "same board: " << (_root_exploration_node->_board == &_board) << endl;
	//cout << "same board2: " << (*_root_exploration_node->_board == _board) << endl;


	// Update the PGN
	_game_tree.select_next_node(move);
	_pgn = _game_tree.tree_display();

	if (!_board->selected_piece_has_trait())
		_selected_pos = Pos(-1, -1);

	// Any played move invalidates a stale promotion picker
	_promotion_pending = false;

	// Clear stale arrows from the old root so they cannot be clicked
	_grogros_arrows.clear();

	return true;
}

// Intercepts a user move (click or drag): opens the promotion picker
// when the move is a pawn reaching the last rank, plays it otherwise.
bool GUI::play_user_move(const int start_row, const int start_col, const int end_row, const int end_col) {
	if (_board->_got_moves == -1)
		_board->get_moves();

	// Legality by geometry (Move equality includes the promo piece, and a
	// user-constructed move always carries PROMO_QUEEN)
	Move move(start_row, start_col, end_row, end_col);
	bool legal = false;
	for (int i = 0; i < _board->_got_moves; i++) {
		const Move& m = _board->_moves[i];
		if (m.start_row == start_row && m.start_col == start_col && m.end_row == end_row && m.end_col == end_col) {
			legal = true;
			break;
		}
	}

	if (!legal)
		return false;

	// Promotion? Detected GEOMETRICALLY (pawn reaching the last rank):
	// raw generator output does not carry usable flag bits.
	const uint8_t moved_piece = _board->_array[start_row][start_col];
	if (is_pawn(moved_piece) && end_row == (_board->_player ? 7 : 0)) {
		_promotion_pending = true;
		_promotion_move = move;
		_promotion_choice_count = 4;
		return true;
	}

	if (_click_bind)
		_board->click_m_move(move, get_board_orientation());
	play_move_keep(move);
	unselect();
	return true;
}

// Completes a pending promotion with the chosen piece (PROMO_*)
void GUI::complete_promotion(const uint8_t promo_piece) {
	Move move = _promotion_move;
	move.set_promo_piece(promo_piece);

	if (_click_bind)
		_board->click_m_move(move, get_board_orientation());
	play_move_keep(move);

	_promotion_pending = false;
	unselect();
}

// Cancels a pending promotion
void GUI::cancel_promotion() {
	_promotion_pending = false;
}

// Handles a click while the promotion picker is open (true: consumed)
bool GUI::handle_promotion_click() {
	const float x = _board_padding_x + _tile_size * orientation_index(_promotion_move.end_col);
	const int disp_row = orientation_index(7 - _promotion_move.end_row);
	const int step = (disp_row <= 3) ? 1 : -1;

	for (int i = 0; i < _promotion_choice_count; i++) {
		const float y = _board_padding_y + _tile_size * (disp_row + i * step);
		if (_mouse_pos.x >= x && _mouse_pos.x <= x + _tile_size && _mouse_pos.y >= y && _mouse_pos.y <= y + _tile_size) {
			complete_promotion(_promotion_choices[i]);
			return true;
		}
	}

	return false;
}

// Draws the promotion picker overlay: four tiles from the promotion square
// toward the board interior (Queen first), like the chess websites do.
// The rest of the board is dimmed so the choices read as a modal, not as
// regular squares - same tile geometry as handle_promotion_click().
void GUI::draw_promotion_picker() {
	const float x = _board_padding_x + _tile_size * orientation_index(_promotion_move.end_col);
	const int disp_row = orientation_index(7 - _promotion_move.end_row);
	const int step = (disp_row <= 3) ? 1 : -1;

	// Dim the whole board so only the picker stands out
	draw_rectangle(_board_padding_x, _board_padding_y, _board_size, _board_size, { 0, 0, 0, 150 });

	// Panel styling
	const Color panel_color = { 38, 38, 44, 255 };
	const Color border_color = { 215, 215, 220, 255 };
	const Color hover_color = { 255, 200, 90, 255 };

	// First/last tile positions, to frame the strip
	const int last_offset = (_promotion_choice_count - 1) * step;
	const float strip_top = _board_padding_y + _tile_size * min(disp_row, disp_row + last_offset);
	const float strip_height = _tile_size * _promotion_choice_count;

	// Frame around the strip
	DrawRectangleLinesEx({ x - 2.0f, strip_top - 2.0f, _tile_size + 4.0f, strip_height + 4.0f }, 2, border_color);

	for (int i = 0; i < _promotion_choice_count; i++) {
		const float y = _board_padding_y + _tile_size * (disp_row + i * step);
		const bool hovered = is_cursor_in_rect({ x, y, _tile_size, _tile_size });

		// Solid panel instead of board colors: unambiguous overlay
		draw_rectangle(x, y, _tile_size, _tile_size, panel_color);
		DrawRectangleLinesEx({ x, y, _tile_size, _tile_size }, 2, hovered ? hover_color : border_color);
		draw_texture(_piece_textures[promo_to_piece(_promotion_choices[i], _board->_player) - 1],
			x + (_tile_size - _piece_size) / 2, y + (_tile_size - _piece_size) / 2,
			hovered ? Color{ 255, 230, 170, 255 } : WHITE);
	}
}

// Returns the type of the selected piece
uint8_t GUI::selected_piece() const
{
	// Should this be cached to avoid recomputing it?
	if (_selected_pos.row == -1 || _selected_pos.col == -1)
		return 0;

	return _board->_array[_selected_pos.row][_selected_pos.col];
}

// Returns the type of the piece the mouse has just clicked on
uint8_t GUI::clicked_piece() const
{
	if (_clicked_pos.row == -1 || _clicked_pos.col == -1)
		return 0;

	return _board->_array[_clicked_pos.row][_clicked_pos.col];
}

// Starts a GrogrosZero analysis
// iterations ==  0: background worker (Ctrl-G auto analysis, Ctrl-Up/Down play)
// iterations == -1: inline, auto-calculated iterations per frame (G key hold, playing modes)
// iterations == N:  inline, exactly N iterations (Enter key)
void GUI::grogros_analysis(int iterations) {
	if (!_root_exploration_node || !_board) {
		debug_log("[grogros_analysis] CRITICAL: root=%p board=%p — skipping",
			(void*)_root_exploration_node, (void*)_board);
		return;
	}

	g_tt_main_search = _tt_main_search;
	g_tt_node_dag = _tt_node_dag;

	// Inline mode with auto-calculated iterations (G key hold, playing modes)
	if (iterations == -1) {
		int iterations_per_second = _root_exploration_node->get_ips();
		int iterations_to_explore = iterations_per_second / _target_fps;
		if (iterations_to_explore == 0)
			iterations_to_explore = 1;
		if (monte_board_buffer.is_full())
			iterations_to_explore = 0;
		if (iterations_to_explore > 0) {
			_root_exploration_node->grogros_zero(&monte_board_buffer, _grogros_eval, _alpha, _beta, _gamma, iterations_to_explore, _quiescence_depth);
			if (g_tt_node_dag)
				dag_debug_report();
			_update_variants = true;
		}
		return;
	}

	// Inline mode with explicit iteration count (Enter key)
	if (iterations > 0) {
		_root_exploration_node->grogros_zero(&monte_board_buffer, _grogros_eval, _alpha, _beta, _gamma, iterations, _quiescence_depth);
		if (g_tt_node_dag)
			dag_debug_report();
		_update_variants = true;
		return;
	}

	// Background mode (iterations == 0) — start worker if not already running
	if (_compute_running.load(std::memory_order_acquire))
		return;
	stop_compute();
	start_compute(0);
}

// Runs a puzzle headlessly with the GUI's parameters, prints live results to cout
// Uses the GUI's own root node and buffers (same code path as normal analysis)
void GUI::run_puzzle_headless(double time_s) {
	if (!_board || !_root_exploration_node) {
		cout << "[puzzle] no board loaded" << endl;
		return;
	}

	string fen = _board->to_fen();
	cout << "[puzzle] FEN: " << fen << endl;
	cout << "[puzzle] params: alpha=" << _alpha << " beta=" << _beta << " gamma=" << _gamma
		<< " qdepth=" << _quiescence_depth << " dag=" << _tt_node_dag << endl;
	cout << "[puzzle] budget: " << time_s << "s" << endl;

	// Resolve expected move: look up current FEN in lichess_5000.txt
	string expected_san;
	string puzzle_name;
	{
		ifstream file("tests/lichess_5000.txt");
		string line;
		while (getline(file, line)) {
			if (line.empty() || line[0] == '#') continue;
			string fen_part = extract_fen_from_line(line);
			if (fen_part == fen) {
				size_t fen_end = line.find(fen_part) + fen_part.size();
				string rest = line.substr(fen_end);
				rest.erase(0, rest.find_first_not_of(" \t"));
				size_t space = rest.find(' ');
				if (space != string::npos) {
					expected_san = rest.substr(0, space);
					puzzle_name = rest.substr(space + 1);
					size_t comment = puzzle_name.find("//");
					if (comment != string::npos) puzzle_name = puzzle_name.substr(0, comment);
					puzzle_name = trim_copy(puzzle_name);
				}
				break;
			}
		}
	}

	if (!expected_san.empty()) {
		cout << "[puzzle] expected: " << expected_san << " (" << puzzle_name << ")" << endl;
	} else {
		cout << "[puzzle] no expected move found in lichess_5000.txt" << endl;
	}

	// Reset everything — same as loading a fresh FEN
	init_buffers();
	reset_buffers();
	_root_exploration_node->reset();
	_root_exploration_node->_board = _board;
	_root_exploration_node->_is_active = true;
	_board->_is_active = true;
	g_buffers_full_logged = false;

	// Pause continuous analysis so next frame doesn't overwrite results
	_grogros_analysis = false;

	// Run search in a tight clock loop — same as PuzzleRunner::run TIME mode
	// Call grogros_zero directly on the root node (NOT grogros_analysis which is frame-based)
	g_tt_node_dag = _tt_node_dag;
	g_tt_main_search = _tt_main_search;
	_update_variants = true;

	// Start background computation thread — keeps the window responsive
	start_compute(time_s);

	// Render loop: window stays responsive while the worker runs
	while (!_compute_done.load(std::memory_order_acquire)) {
		if (WindowShouldClose()) break;
		BeginDrawing();
		draw();
		EndDrawing();
	}

	stop_compute();

	Move chosen = _root_exploration_node->get_most_explored_child_move();
	string chosen_san = _board->move_label(chosen);
	int iters = _root_exploration_node->_iterations;
	int nodes = _root_exploration_node->get_total_nodes();
	int nps = _compute_budget_s > 0 ? (int)(nodes / _compute_budget_s) : 0;
	bool solved = (!expected_san.empty() && chosen_san == expected_san);

	cout << "[puzzle] chosen: " << chosen_san
		<< (solved ? " CORRECT" : " WRONG")
		<< endl;
	cout << "[puzzle] iters=" << iters << " nodes=" << nodes
		<< " time=" << fixed << setprecision(3) << _compute_budget_s << "s"
		<< " NPS=" << nps << endl;
	cout << endl;
}

// Background computation thread: runs grogros_zero in a tight loop
// Uses shared monte_board_buffer/monte_node_buffer (safe: GUI doesn't allocate during search)
// _compute_budget_s == 0 means run indefinitely until _compute_running is set to false
// All _children reads in draw() are protected by _tree_mutex — no pause needed
void GUI::compute_worker() {
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	g_tt_node_dag = _tt_node_dag;
	g_tt_main_search = _tt_main_search;
	_update_variants = true;

	int iter_since_update = 0;
	clock_t begin = clock();
	while (_compute_running.load(std::memory_order_relaxed)) {
		// Time budget check: if > 0, respect it (puzzle mode); if == 0, run forever (continuous)
		if (_compute_budget_s > 0 && (double)(clock() - begin) / CLOCKS_PER_SEC >= _compute_budget_s)
			break;
		// Stop if buffer is full
		if (monte_board_buffer.is_full())
			break;
		{
			std::lock_guard<std::mutex> lock(_tree_mutex);
			_root_exploration_node->grogros_zero(&monte_board_buffer, _grogros_eval, _alpha, _beta, _gamma, 1, _quiescence_depth);
		}
		// Periodic GUI updates (every 100 iterations to avoid overhead)
		if (++iter_since_update >= 100) {
			iter_since_update = 0;
			_update_variants = true;
		}
	}
	_compute_done.store(true, std::memory_order_release);
	_compute_running.store(false, std::memory_order_release);
}

// Background thread entry point (16MB stack via _beginthreadex)
static unsigned __stdcall compute_worker_entry(void* param) {
	GUI* self = static_cast<GUI*>(param);
	self->compute_worker();
	return 0;
}

// Starts background computation with the given time budget
void GUI::start_compute(double time_s) {
	stop_compute();
	_compute_budget_s = time_s;
	_compute_done.store(false, std::memory_order_release);
	_compute_running.store(true, std::memory_order_release);
	// 16MB stack (same as main thread) — quiescence recursion needs it
	_compute_thread_handle = reinterpret_cast<void*>(_beginthreadex(nullptr, 16 * 1024 * 1024, compute_worker_entry, this, 0, nullptr));
}

// Stops background computation and waits for it to finish
void GUI::stop_compute() {
	_compute_running.store(false, std::memory_order_release);
	if (_compute_thread_handle) {
		WaitForSingleObject(_compute_thread_handle, INFINITE);
		CloseHandle(_compute_thread_handle);
		_compute_thread_handle = nullptr;
	}
	_compute_done.store(false, std::memory_order_release);
}

// Draws the GUI
void GUI::draw()
{
	// Loading of the textures, if not already done
	if (!_loaded_resources) {
		load_resources();
		resize_GUI();
		PlaySound(_game_begin_sound);
	}

	// Null guard: if root or board is null, draw a black screen with a message
	if (!_board || !_root_exploration_node || !_root_exploration_node->_board) {
		BeginDrawing();
		ClearBackground(BLACK);
		const char* msg = "[CRASH GUARD] _board or _root_exploration_node is null";
		DrawText(msg, 20, 20, 20, RED);
		char buf[256];
		snprintf(buf, sizeof(buf), "_board=%p root=%p root_board=%p",
			(void*)_board, (void*)_root_exploration_node,
			_root_exploration_node ? (void*)_root_exploration_node->_board : nullptr);
		DrawText(buf, 20, 50, 16, YELLOW);
		EndDrawing();
		debug_log("[draw] null guard triggered: %s", msg);
		return;
	}

	// Board corruption detector: track the board's FEN hash across frames.
	// If it changes WITHOUT play_move_keep being called, log the corruption.
	{
		static uint64_t prev_zobrist = 0;
		const uint64_t cur_zobrist = _board->_zobrist_key;

		if (prev_zobrist != 0 && cur_zobrist != prev_zobrist) {
			debug_log("[draw] BOARD CHANGED: prev=%016llX cur=%016llX fen=%s root_board_zob=%016llX same_as_root=%d",
				(unsigned long long)prev_zobrist, (unsigned long long)cur_zobrist,
				_board->to_fen().c_str(),
				(unsigned long long)(_root_exploration_node->_board ? _root_exploration_node->_board->_zobrist_key : 0),
				(int)(_board == _root_exploration_node->_board));
		}
		prev_zobrist = cur_zobrist;
	}

	// Take a snapshot of the tree state under a single lock.
	// This ensures arrows and eval display are consistent with each other,
	// even while the background worker mutates the tree.
	_tree_snapshot.valid = false;
	_tree_snapshot.arrows.clear();
	if (_root_exploration_node && _root_exploration_node->_board
		&& _root_exploration_node->_nodes > 1 && !_root_exploration_node->_is_terminal) {
		std::lock_guard<std::mutex> lock(_tree_mutex);
		_tree_snapshot.best_move = _root_exploration_node->get_most_explored_child_move();
		_tree_snapshot.best_eval_move = _root_exploration_node->get_best_score_move(_alpha, _beta);
		if (!_tree_snapshot.best_eval_move.is_null_move()) {
			_tree_snapshot.best_evaluation = _root_exploration_node->_children[_tree_snapshot.best_eval_move]._node->_deep_evaluation;
		}
		_tree_snapshot.iterations = _root_exploration_node->_iterations;
		_tree_snapshot.nodes = _root_exploration_node->_nodes;
		_tree_snapshot.time_spent = _root_exploration_node->_time_spent;
		_tree_snapshot.avg_score = _root_exploration_node->_deep_evaluation._avg_score;

		// Build arrow data under the same lock
		for (auto const& [move, child_link] : _root_exploration_node->_children) {
			Node const* child = child_link._node;
			if (!child) continue;
			_tree_snapshot.arrows.push_back({
				move,
				(int)child_link._chosen_iterations,
				(int)child->_iterations,
				move == _tree_snapshot.best_move,
				move == _tree_snapshot.best_eval_move,
				child->_deep_evaluation._value,
				child->_deep_evaluation._avg_score
			});
		}
		_tree_snapshot.valid = true;
	}


	// *** CLICS SOURIS ***

	// Position of the mouse
	_mouse_pos = GetMousePosition();

	// Promotion picker open: this click either picks a piece or cancels
	if (_promotion_pending && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		remove_highlighted_tiles();
		_arrows_array = {};
		_clicked = false;
		_clicked_pos = Pos(-1, -1);
		if (!handle_promotion_click())
			cancel_promotion();
	}

	// If the mouse is clicked
	else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

		// Remove the highlight from every square
		remove_highlighted_tiles();

		// Remove every arrow
		_arrows_array = {};

		// Store the square clicked on the board
		_clicked_pos = get_pos_from_GUI(_mouse_pos.x, _mouse_pos.y);
		_clicked = true;
		bool has_played = false;

		// If the GrogrosZero search arrows are there, and no piece is selected
		if (_drawing_arrows && !selected_piece()) {

			// Iterate backwards to play the most recent arrow (the visible one when they overlap)
			for (Move move : ranges::reverse_view(_grogros_arrows))
			{
				if (move.end_row == _clicked_pos.row && move.end_col == _clicked_pos.col) {
					if (_click_bind)
						_board->click_m_move(move, get_board_orientation());
					play_move_keep(move);
					has_played = true;
					continue;
				}
			}
		}

		// If no piece is selected and a piece is clicked, select it
		if (!selected_piece() && clicked_piece()) {
			if (!has_played || _board->clicked_piece_has_trait()) {
				_selected_pos = _clicked_pos;
				if (_board->_got_moves == -1)
					_board->get_moves();
			}
		}

		// If a piece is already selected
		else if (selected_piece()) {

			// If the click lands on the selected square, deselect it
			if (_selected_pos == _clicked_pos) {
				//unselect();
			}

			else {

				// If the move is legal, play it (opens the promotion picker
				// when the pawn reaches the last rank)
				if (play_user_move(_selected_pos.row, _selected_pos.col, _clicked_pos.row, _clicked_pos.col)) {
					// Deselect the piece (deferred when the picker is open)
					if (!_promotion_pending)
						unselect();
				}

				else {
					// If another piece is clicked, select it
					if (clicked_piece() && _board->clicked_piece_has_trait())
						_selected_pos = _clicked_pos;
					
					// Otherwise, deselect the piece
					else
						unselect();
				}
			}
		}
	}	

	// If the mouse is released
	if (!_promotion_pending && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {

		// Position of the square where the mouse was released
		Pos drop_pos = get_pos_from_GUI(_mouse_pos.x, _mouse_pos.y);

		// If the square really is on the board
		if (is_in_fast(drop_pos.row, 0, 7) && is_in_fast(drop_pos.col, 0, 7) && is_in_fast(_selected_pos.row, 0, 7) && is_in_fast(_selected_pos.col, 0, 7)) {

			// If the mouse is released on a square other than the clicked one
			if (drop_pos != _selected_pos) {

				// If the move is legal, play it (opens the promotion picker
				// when the pawn reaches the last rank)
				if (play_user_move(_selected_pos.row, _selected_pos.col, drop_pos.row, drop_pos.col)) {
					// Deselect the piece (deferred when the picker is open)
					if (!_promotion_pending)
						unselect();
				}
			}
		}

		_clicked = false;
	}

	// If a right click happens
	if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {

		// Store the square clicked on the board
		_right_clicked_pos = get_pos_from_GUI(_mouse_pos.x, _mouse_pos.y);
	}

	// If the right click is released
	if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
		Pos drop_pos = get_pos_from_GUI(_mouse_pos.x, _mouse_pos.y);

		// If the mouse is released on the board
		if (is_in_fast(drop_pos.row, 0, 7) && is_in_fast(drop_pos.col, 0, 7)) {

			// If the mouse is released on a square other than the clicked one
			if (drop_pos == _right_clicked_pos) {

				// Select/deselect the square
				_highlighted_array[drop_pos.row][drop_pos.col] = 1 - _highlighted_array[drop_pos.row][drop_pos.col];
			}
				
			// Otherwise, draw an arrow
			else {
				if (_right_clicked_pos.row != -1 && _right_clicked_pos.col != -1) {
					vector<int> arrow = { _right_clicked_pos.row, _right_clicked_pos.col, drop_pos.row, drop_pos.col };

					// If the arrow exists, delete it
					if (auto found_arrow = find(_arrows_array.begin(), _arrows_array.end(), arrow); found_arrow != _arrows_array.end())
						_arrows_array.erase(found_arrow);

					// Otherwise, add it
					else
						_arrows_array.push_back(arrow);
				}

			}
		}
	}


	// *** AFFICHAGE ***

	// Background colour
	ClearBackground(_background_color);

	// Number of FPS
	DrawTextEx(_text_font, ("FPS : " + to_string(GetFPS())).c_str(), { _screen_width - 2 * _text_size, _text_size / 3 }, _text_size / 3, _font_spacing, _text_color);

	// Board
	draw_rectangle(_board_padding_x, _board_padding_y, _tile_size * 8, _tile_size * 8, _board_color_light);

	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++)
			((i + j) % 2 == 1) && draw_rectangle(_board_padding_x + _tile_size * j, _board_padding_y + _tile_size * i, _tile_size, _tile_size, _board_color_dark);

	// Coordinates on the board
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++) {
			if (j == 0 + 7 * _board_orientation) // Chiffres
				DrawTextEx(_text_font, to_string(i + 1).c_str(), { _board_padding_x + _text_size / 8, _board_padding_y + _tile_size * orientation_index(7 - i) + _text_size / 8 }, _text_size / 2, _font_spacing, ((i + j) % 2 == 1) ? _board_color_light : _board_color_dark);
			if (i == 0 + 7 * _board_orientation) // Lettres
				DrawTextEx(_text_font, _abc8.substr(j, 1).c_str(), { _board_padding_x + _tile_size * (orientation_index(j) + 1) - _text_size / 2, _board_padding_y + _tile_size * 8 - _text_size / 2 }, _text_size / 2, _font_spacing, ((i + j) % 2 == 1) ? _board_color_light : _board_color_dark);
		}

	// Highlight of the last played move
	if (!_game_tree._current_node->_move.is_null_move()) {
		draw_rectangle(_board_padding_x + orientation_index(_game_tree._current_node->_move.start_col) * _tile_size, _board_padding_y + orientation_index(7 - _game_tree._current_node->_move.start_row) * _tile_size, _tile_size, _tile_size, _last_move_color);
		draw_rectangle(_board_padding_x + orientation_index(_game_tree._current_node->_move.end_col) * _tile_size, _board_padding_y + orientation_index(7 - _game_tree._current_node->_move.end_row) * _tile_size, _tile_size, _tile_size, _last_move_color);
	}

	// Highlighted squares
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++)
			if (_highlighted_array[i][j])
				draw_rectangle(_board_padding_x + _tile_size * orientation_index(j), _board_padding_y + _tile_size * orientation_index(7 - i), _tile_size, _tile_size, _highlight_color);

	// Selection of squares and pieces
	if (_selected_pos.row != -1 && _selected_pos.col != -1) {

		// Display the selected square
		draw_rectangle(_board_padding_x + orientation_index(_selected_pos.col) * _tile_size, _board_padding_y + orientation_index(7 - _selected_pos.row) * _tile_size, _tile_size, _tile_size, _select_color);
		
		// Display the possible moves of the selected piece
		for (int i = 0; i < _board->_got_moves; i++) {
			if (_board->_moves[i].start_row == _selected_pos.row && _board->_moves[i].start_col == _selected_pos.col) {
				draw_rectangle(_board_padding_x + orientation_index(_board->_moves[i].end_col) * _tile_size, _board_padding_y + orientation_index(7 - _board->_moves[i].end_row) * _tile_size, _tile_size, _tile_size, _select_color);
			}
		}
	}

	// Draw the opposing pieces
	for (int row = 0; row < 8; row++) {
		for (int col = 0; col < 8; col++) {
			uint8_t piece = _board->_array[row][col];
			if (is_white(piece) && !_board->_player || is_black(piece) && _board->_player) {
				if (!_clicked || row != _clicked_pos.row || col != _clicked_pos.col)
					draw_texture(_piece_textures[piece - 1], _board_padding_x + _tile_size * orientation_index(col) + (_tile_size - _piece_size) / 2, _board_padding_y + _tile_size * orientation_index(7 - row) + (_tile_size - _piece_size) / 2, WHITE);
			}
		}
	}

	// Moves the AI is thinking about...
	if (_drawing_arrows) {
		draw_exploration_arrows();
	}

	// Draw the friendly pieces
	for (int row = 0; row < 8; row++) {
		for (int col = 0; col < 8; col++) {
			uint8_t piece = _board->_array[row][col];
			if (is_white(piece) && _board->_player || is_black(piece) && !_board->_player) {
				if (!_clicked || row != _clicked_pos.row || col != _clicked_pos.col)
					draw_texture(_piece_textures[piece - 1], _board_padding_x + _tile_size * orientation_index(col) + (_tile_size - _piece_size) / 2, _board_padding_y + _tile_size * orientation_index(7 - row) + (_tile_size - _piece_size) / 2, WHITE);
			}
		}
	}

	// Draw the clicked piece (when a piece is being clicked)
	for (int row = 0; row < 8; row++) {
		for (int col = 0; col < 8; col++) {
			uint8_t piece = _board->_array[row][col];
			if (_clicked && piece != none && row == _clicked_pos.row && col == _clicked_pos.col) {
				BeginShaderMode(_selected_shader);
				draw_texture(_piece_textures[piece - 1], _mouse_pos.x - _piece_size / 2, _mouse_pos.y - _piece_size / 2, WHITE);
				EndShaderMode();
			}
		}
	}

	// Arrows drawn
	for (vector<int> arrow : _arrows_array)
		draw_simple_arrow_from_coord(arrow[0], arrow[1], arrow[2], arrow[3], -1, _arrow_color);

	// Titre
	DrawTextEx(_text_font, "GROGROS CHESS", { _board_padding_x + _grogros_size / 2 + _text_size / 4.0f, _text_size / 4.0f }, _text_size / 1.25f, _font_spacing * _text_size / 1.25f, _text_color);

	// Grogros
	draw_texture(_grogros_texture, _board_padding_x, _text_size / 4.0f - _text_size / 5.6f, WHITE);

	// Players of the game
	int material = _board->material_difference();
	string black_material = (material < 0) ? ("+" + to_string(-material)) : "";
	string white_material = (material > 0) ? ("+" + to_string(material)) : "";

	int t_size = _text_size / 3.0f;

	int x_mini_piece = _board_padding_x + t_size * 4;
	int y_mini_piece_black = _board_padding_y - t_size + (_board_size + 2 * t_size) * !_board_orientation;
	int y_mini_piece_white = _board_padding_y - t_size + (_board_size + 2 * t_size) * _board_orientation;

	// Noirs
	DrawCircle(x_mini_piece - t_size * 3, y_mini_piece_black, t_size * 0.6f, _board_color_dark);
	DrawTextEx(_text_font, _black_player.c_str(), { static_cast<float>(x_mini_piece - t_size * 2), static_cast<float>(y_mini_piece_black - t_size) }, t_size, _font_spacing* t_size, _text_color);
	DrawTextEx(_text_font, black_material.c_str(), { static_cast<float>(x_mini_piece - t_size * 2), static_cast<float>(y_mini_piece_black + t_size / 6) }, t_size, _font_spacing* t_size, _text_color_info);

	bool next = false;
	for (int i = 1; i < 6; i++) {
		for (int j = 0; j < _missing_w_material[i]; j++) {
			DrawTexture(_mini_piece_textures[i - 1], x_mini_piece, y_mini_piece_black, WHITE);
			x_mini_piece += _mini_piece_size / 2;
			next = true;
		}
		if (next)
			x_mini_piece += _mini_piece_size;
		next = false;
	}

	x_mini_piece = _board_padding_x + t_size * 4;

	// Blancs
	DrawCircle(x_mini_piece - t_size * 3, y_mini_piece_white, t_size * 0.6f, _board_color_light);
	DrawTextEx(_text_font, _white_player.c_str(), { static_cast<float>(x_mini_piece - t_size * 2), static_cast<float>(y_mini_piece_white - t_size) }, t_size, _font_spacing * t_size, _text_color);
	DrawTextEx(_text_font, white_material.c_str(), { static_cast<float>(x_mini_piece - t_size * 2), static_cast<float>(y_mini_piece_white + t_size / 6) }, t_size, _font_spacing * t_size, _text_color_info);

	for (int i = 1; i < 6; i++) {
		for (int j = 0; j < _missing_b_material[i]; j++) {
			DrawTexture(_mini_piece_textures[i - 1 + 6], x_mini_piece, y_mini_piece_white, WHITE);
			x_mini_piece += _mini_piece_size / 2;
			next = true;
		}
		if (next)
			x_mini_piece += _mini_piece_size;
		next = false;
	}

	// Clocks of the players
	// Update of the clock
	update_time();
	float x_pad = _board_padding_x + _board_size - _text_size * 2;
	Color time_colors[4] = { (_time && !_board->_player) ? BLACK : _dark_gray, (_time && !_board->_player) ? WHITE : LIGHTGRAY, (_time && _board->_player) ? WHITE : LIGHTGRAY, (_time && _board->_player) ? BLACK : _dark_gray };

	// White's clock
	if (!_white_time_text_box.active) {
		_white_time_text_box.value = _time_white;
		_white_time_text_box.text = clock_to_string(_white_time_text_box.value);
	}
	update_text_box(_white_time_text_box);
	if (!_white_time_text_box.active) {
		_time_white = _white_time_text_box.value;
		_white_time_text_box.text = clock_to_string(_white_time_text_box.value);
	}

	// Position of the text
	_white_time_text_box.set_rect(x_pad, _board_padding_y - _text_size / 2 * !_board_orientation + _board_size * _board_orientation, _board_padding_x + _board_size - x_pad, _text_size / 2);
	_white_time_text_box.text_size = _text_size / 3;
	_white_time_text_box.text_color = time_colors[3];
	_white_time_text_box.text_font = _text_font;
	_white_time_text_box.main_color = time_colors[2];
	draw_text_box(_white_time_text_box);

	// Black's clock
	if (!_black_time_text_box.active) {
		_black_time_text_box.value = _time_black;
		_black_time_text_box.text = clock_to_string(_black_time_text_box.value);
	}
	update_text_box(_black_time_text_box);
	if (!_black_time_text_box.active) {
		_time_black = _black_time_text_box.value;
		_black_time_text_box.text = clock_to_string(_black_time_text_box.value);
	}

	// Position of the text
	_black_time_text_box.set_rect(x_pad, _board_padding_y - _text_size / 2 * _board_orientation + _board_size * !_board_orientation, _board_padding_x + _board_size - x_pad, _text_size / 2);
	_black_time_text_box.text_size = _text_size / 3;
	_black_time_text_box.text_color = time_colors[1];
	_black_time_text_box.text_font = _text_font;
	_black_time_text_box.main_color = time_colors[0];
	draw_text_box(_black_time_text_box);

	// FEN
	_current_fen = _board->to_fen();
	const char* fen = _current_fen.c_str();
	DrawTextEx(_text_font, fen, { _text_size / 2, _board_padding_y + _board_size + _text_size * 3 / 2 }, _text_size / 3, _font_spacing * _text_size / 3, _text_color_blue);

	// PGN
	update_global_pgn();
	slider_text(_global_pgn, _text_size / 2, _board_padding_y + _board_size + _text_size * 2, _screen_width - _text_size, _screen_height - (_board_padding_y + _board_size + _text_size * 2) - _text_size / 3, _text_size / 3, &_pgn_slider, _text_color);

	// Grogros analysis
	string monte_carlo_text = static_cast<string>(_grogros_analysis ? "STOP GrogrosZero-Auto (CTRL-H)" : "RUN GrogrosZero-Auto (CTRL-G)") + "\nCONTROLS (H)" + "\n\nSEARCH PARAMETERS\nalpha: " + to_string(_alpha) + "\nbeta: " + to_string(_beta) + "\ngamma : " + to_string(_gamma) + "\nq_depth : " + to_string(_quiescence_depth) + "\nexplore checks : " + (_explore_checks ? "true" : "false") + "\nTT main search : " + (_tt_main_search ? "true" : "false") + " (I)" + "\nTT node DAG : " + (_tt_node_dag ? "true" : "false") + " (O)";
	
	// If a search has happened (use snapshot for consistency)
	if (_tree_snapshot.valid && _drawing_arrows) {

		Move best_move = _tree_snapshot.best_eval_move;

		if (best_move.is_null_move()) {
			if (_promotion_pending)
				draw_promotion_picker();
			draw_texture(_cursor_texture, _mouse_pos.x - _cursor_size / 2, _mouse_pos.y - _cursor_size / 2, WHITE);
			return;
		}
		Evaluation best_evaluation = _tree_snapshot.best_evaluation;

		bool all_moves_explored = _root_exploration_node->children_count() == _root_exploration_node->_board->_got_moves;

		if (!all_moves_explored && ((_board->_player && _root_exploration_node->_static_evaluation > best_evaluation) || (!_board->_player && _root_exploration_node->_static_evaluation < best_evaluation))) {
			best_evaluation = _root_exploration_node->_static_evaluation;
		}

		int best_eval = best_evaluation._value;

		string eval;
		int mate = _board->is_eval_mate(best_eval);
		if (mate != 0) {
			eval += "M";
			eval += to_string(abs(mate));
		}

		else
			eval = to_string(best_eval);

		_global_eval = best_eval;

		stringstream stream;
		stream << fixed << setprecision(2) << best_eval / 100.0f;
		_global_eval_text = mate ? (best_eval > 0 ? "+" + eval : "-" + eval) : (best_eval > 0) ? "+" + stream.str() : stream.str();

		// TODO
		//float win_chance = get_winning_chances_from_eval(best_eval, _board->_player);
		//if (!_board->_player)
		//	win_chance = 1 - win_chance;
		//string win_chances = "W/D/L: " + to_string(static_cast<int>(100 * win_chance)) + "/0/" + to_string(static_cast<int>(100 * (1 - win_chance))) + "\%";

		//2bk1r2/4b1Qp/8/1P6/3P4/2p5/1q2NPPP/R1K2B1R w - - 1 26

		_wdl = best_evaluation._wdl;

		// For the static evaluation
		if (!_board->_displayed_components) {
			evaluate_position(true, true);
		}
		
		int max_depth = _root_exploration_node->get_main_depth(_alpha, _beta);
		monte_carlo_text += "\n\nSTATIC EVAL\n" + _eval_components +
			"\nTime: " + clock_to_string(_tree_snapshot.time_spent, true) +
			"\nDepth: " + to_string(max_depth) +
			"\nQdepth: " + (_tree_snapshot.iterations == 0 ? to_string(_root_exploration_node->_quiescence_depth) : "N/A") +
			"\nEval: " + ((best_eval > 0) ? static_cast<string>("+") : (mate != 0 ? static_cast<string>("-") : static_cast<string>(""))) + eval +
			"\nConfidence: " + to_string(100 - (int)(100 * best_evaluation._uncertainty)) + "%" +
			"\nWinnable: " + to_string(static_cast<int>(best_evaluation._winnable_white * 100)) + "% / " + to_string(static_cast<int>(best_evaluation._winnable_black * 100)) + "%" +
			"\n" + _wdl.to_string() + "\nScore: " + score_string(best_evaluation._avg_score) +
			"\nNodes: " + int_to_round_string(_tree_snapshot.nodes) + "/" + int_to_round_string(monte_board_buffer._length) + " (" + int_to_round_string(_tree_snapshot.nodes / (static_cast<float>(_tree_snapshot.time_spent + 1) / CLOCKS_PER_SEC)) + "N/s)" +
			"\nIterations: " + int_to_round_string(_tree_snapshot.iterations) + " (" + int_to_round_string(_tree_snapshot.iterations / (static_cast<float>(_tree_snapshot.time_spent + 1) / CLOCKS_PER_SEC)) + "I/s)" +
				"\n\n" + transposition_table.stats_string();
		
		// Display of the GrogrosZero analysis parameters
		slider_text(monte_carlo_text, _board_padding_x + _board_size + _text_size / 2, _text_size, _screen_width - _text_size - _board_padding_x - _board_size, _board_size * 9 / 16, _text_size / 4, &_monte_carlo_slider, _text_color);

		// GrogrosZero analysis lines
		// TODO: this should be used too, to avoid recomputing the other parameters
		if (_update_variants) {
			_exploration_variants = _root_exploration_node->get_exploration_variants(_alpha, _beta);
			_update_variants = false;
		}

		// Display of the variations
		slider_text(_exploration_variants, _board_padding_x + _board_size + _text_size / 2, _board_padding_y + _board_size * 9 / 16, _screen_width - _text_size - _board_padding_x - _board_size, _board_size / 2, _text_size / 3, &_variants_slider, _text_color);

		// Display of the evaluation bar
		draw_eval_bar(_global_eval, _wdl, _root_exploration_node->_deep_evaluation._avg_score, _global_eval_text, _board_padding_x / 6, _board_padding_y, 2 * _board_padding_x / 3, _board_size, 800, _eval_bar_color_light, _eval_bar_color_gray, _eval_bar_color_dark);
	}

	// Display of the controls and other information
	else {
		// Touches
		static string keys_information = "CTRL-G: Start GrogrosZero analysis\nCTRL-H: Stop GrogrosZero analysis\n\n";

		// Binding chess.com
		static string binding_information;
		binding_information = "Binding chess.com:\n- Auto-click: " + (_click_bind ? static_cast<string>("enabled") : static_cast<string>("disabled")) + "\n- Binding mode: " + (_binding_full ? static_cast<string>("analysis") : _binding_solo ? static_cast<string>("play") : "none");

		// Whole text
		static string controls_information;
		controls_information = "Controls:\n\n" + keys_information + binding_information;

		// TODO: add a slider value
		slider_text(controls_information, _board_padding_x + _board_size + _text_size / 2, _board_padding_y, _screen_width - _text_size - _board_padding_x - _board_size, _board_size, _text_size / 3, &_variants_slider, _text_color_info);
	}

	// Promotion picker overlay (above the pieces)
	if (_promotion_pending)
		draw_promotion_picker();

	// Display of the cursor
	draw_texture(_cursor_texture, _mouse_pos.x - _cursor_size / 2, _mouse_pos.y - _cursor_size / 2, WHITE);
}

// Loads a position from a FEN
void GUI::load_FEN(const string fen, bool display) {
	debug_log("[load_FEN] fen=%s root=%p board=%p", fen.c_str(),
		(void*)_root_exploration_node, (void*)_board);

	if (!_root_exploration_node || !_board) {
		debug_log("[load_FEN] CRITICAL: root or board null, skipping");
		return;
	}

	stop_compute();

	// TODO: the FEN has to be validated
	//_board->from_fen(fen);
	//update_global_pgn();
	reset_buffers();
	_root_exploration_node->reset();
	_root_exploration_node->_board = _board;
	_root_exploration_node->_board->from_fen(fen);
	_root_exploration_node->_is_active = true;
	_root_exploration_node->_board->_is_active = true;
	update_global_pgn();
	_update_variants = true;

	if (display)
		cout << "loaded FEN : " << fen << endl;
}

// Resets the game
void GUI::reset_game() {
	stop_compute();
	cout << "*** RESETING GAME ***\n" << endl;
	debug_log("[reset_game] enter boards_free=%d nodes_free=%d root=%p board=%p",
		monte_board_buffer.get_first_free_index(),
		monte_node_buffer.get_first_free_index(),
		(void*)_root_exploration_node, (void*)_board);

	cout << _global_pgn << endl;
	cout << _pgn << endl;

	reset_buffers();
	bool current_orientation = get_board_orientation();

	// Free old root's board FIRST to reclaim a slot before allocating the new one.
	Node* const old_root = _root_exploration_node;
	Board* const old_board = old_root ? old_root->_board : nullptr;
	if (old_board && old_board->_buffer_index >= 0 && !monte_board_buffer._bulk_resetting) {
		monte_board_buffer.free_index(old_board->_buffer_index);
		old_board->_buffer_index = -1; // prevent double-free in recycle_detached_node
	}

	_board = monte_board_buffer.get_first_free_board();
	if (!_board) {
		debug_log("[reset_game] CRITICAL: board buffer full! get_first_free_board returned null");
		cout << "[CRASH] board buffer full in reset_game" << endl;
		return;
	}
	_board->restart();
	_board->_is_active = true;
	_game_tree.reset();
	reset_pgn();
	// Former root: recursive reset (recycles its subtrees through approach B),
	// then replaced by a new node -> explicit recycling of the old one
	// (node + board), otherwise 1 node + 1 board leak on every reset.
	old_root->reset();
	recycle_detached_node(old_root);
	_update_variants = true;
	_board_orientation = current_orientation;
	_root_exploration_node = monte_node_buffer.get_first_free_node();
	if (!_root_exploration_node) {
		debug_log("[reset_game] CRITICAL: node buffer full! get_first_free_node returned null");
		cout << "[CRASH] node buffer full in reset_game" << endl;
		return;
	}
	_root_exploration_node->_board = _board;

	debug_log("[reset_game] done root=%p board=%p", (void*)_root_exploration_node, (void*)_board);
	PlaySound(_game_begin_sound);

	cout << "\n*** GAME RESET DONE ***" << endl;
}

// Compares two Grogros analysis arrows
bool GUI::compare_arrows(const Move m1, const Move m2) const {

	// If two arrows end on the same point, display the "better" move last (on top)
	if (m1.end_row == m2.end_row && m1.end_col == m2.end_col)
		return _root_exploration_node->_children[m1]._node->_nodes < _root_exploration_node->_children[m2]._node->_nodes;

	// If the two arrows start from the same point, display the shorter one on top
	if (m1.start_row == m2.start_row && m1.start_col == m2.start_col) {
		const int d1 = (m1.start_row - m1.end_row) * (m1.start_row - m1.end_row) + (m1.start_col - m1.end_col) * (m1.start_col - m1.end_col);
		const int d2 = (m2.start_row - m2.end_row) * (m2.start_row - m2.end_row) + (m2.start_col - m2.end_col) * (m2.start_col - m2.end_col);

		return d1 > d2;
	}

	return true;
}

// Returns the date in the 'yyyymmdd' format
string GUI::get_date() {
	const time_t current_time = time(nullptr);
	tm local_time;
	localtime_s(&local_time, &current_time);

	const int year = local_time.tm_year + 1900;
	const int month = local_time.tm_mon + 1;
	const int day = local_time.tm_mday;

	return to_string(year) + (month < 10 ? "0" : "") + to_string(month) + (day < 10 ? "0" : "") + to_string(day);
}

// Updates the bot name of GrogrosZero
void GUI::update_grogros_zero_name() {
	//_grogros_zero_name = "Gr0_" + get_date();
	_grogros_zero_name = "Gr0-" + _grogros_zero_version;
}

// Plays the GrogrosZero move or not, depending on the time left
void GUI::play_grogros_zero_move(float time_proportion_per_move) {

	if (!_board || !_root_exploration_node) {
		debug_log("[play_grogros_zero_move] null guard: board=%p root=%p",
			(void*)_board, (void*)_root_exploration_node);
		return;
	}

	stop_compute();

	// Positions bug:
	// rnbq1rk1/pp1p1ppp/7n/2p1P3/3p4/3B1N1P/PPPN1PP1/R2Q1RK1 w - - 0 10: it does not play Ne4

	// TODO: base this on the number of SEARCH nodes, to spend more time in complex positions

	// If the buffer is full, play the GrogrosZero move
	// TODO

	// If the clock is not running
	if (!_time) {
		return;
	}

	// If no exploration has happened yet
	if (_root_exploration_node->_iterations <= 1) {
		return;
	}

	// For the evaluation computations
	int color = _board->get_color();

	// Most explored node
	//Node const *most_explored_child = _root_exploration_node->get_most_explored_child();

	// Node with the best evaluation
	//Node const* best_eval_node;
	//int best_eval_colored = -INT_MAX;
	//Move best_move;

	//for (auto const& child : _root_exploration_node->_children)
	//{
	//	if (child.second->_deep_evaluation._value * color > best_eval_colored) {
	//		best_eval_colored = child.second->_deep_evaluation._value * color;
	//		//best_eval_node = child.second;
	//		best_move = child.first;
	//	}
	//}

	const Move most_explored_move = _root_exploration_node->get_most_explored_child_move();

	if (most_explored_move.is_null_move()) {
		return;
	}

	const ChildLink& most_explored_link = _root_exploration_node->_children[most_explored_move];
	Node const* most_explored_child = most_explored_link._node;

	MoveScoreList move_scores = _root_exploration_node->get_move_scores(_alpha, _beta);

	double most_explored_score = -DBL_MAX;
	double best_score = -DBL_MAX;
	Move best_move;

	// Best move
	for (auto const& [move, score] : move_scores) {
		if (score > best_score) {
			best_score = score;
			best_move = move;
		}
		if (move == most_explored_move) {
			most_explored_score = score;
		}
	}

	bool most_explored_move_is_best = best_score == most_explored_score;

	// FIXME *** the notion of best move needs revisiting here

	//cout << "best eval : " << best_eval_colored << ", color : " << color << ", best move : " << _board->move_label(best_move) << endl;

	// Share of the thinking time spent on the best move
	float best_move_percentage = static_cast<float>(most_explored_link._chosen_iterations) / static_cast<float>(_root_exploration_node->_iterations);

	// Ideal time to spend on this move
	//int max_move_time = _board->_player ? time_to_play_move(_time_white, _time_black, time_proportion_per_move * (1.0f - best_move_percentage)) : time_to_play_move(_time_black, _time_white, time_proportion_per_move * (1.0f - best_move_percentage));
	double max_move_time = _board->_player ? time_to_play_move(_time_white, _time_black, time_proportion_per_move) : time_to_play_move(_time_black, _time_white, time_proportion_per_move);

	//cout << "best move percentage : " << best_move_percentage << " | max move time : " << max_move_time << " | supposed speed : " << grogros_nps << " | nodes : " << _root_exploration_node->_nodes << " | time spent : " << _root_exploration_node->_time_spent << endl;	

	// If there is a lot of time left in the endgame, we can think longer
	// FIXME: check whether this works well (TODO)
	max_move_time *= 1.0f + _board->_adv;

	//cout << "max move time : " << max_move_time << endl;

	//int most_explored_child_eval = most_explored_child->_deep_evaluation._value * color;

	// We want to be sure to play Grogros's best move: if there is a better move than the one with the most nodes, wait...
	bool wait_for_best_move = !most_explored_move_is_best;

	// FIXME: things to improve here!
	// How long should we wait to be sure to play the best move?
	//float eval_diff;
	//float move_wait_factor;

	//// When waiting for the best move, wait longer when the evaluation gap between the best move and the current one is large
	//if (wait_for_best_move) {
	//	eval_diff = abs(most_explored_child->_deep_evaluation._value - _board->_evaluation) / 100.0f;
	//	move_wait_factor = 2 + eval_diff;
	//	//cout << "eval diff : " << eval_diff << " | move wait factor : " << move_wait_factor << endl;
	//}

	//// Otherwise, play faster when the evaluation gap between the best move and the second best is large??
	//else {
	//	//move_wait_factor = 1 / (1 + eval_diff);
	//	move_wait_factor = 1;
	//}

	// How long should we wait?
	// A maximum value is used to avoid overflows
	// FIXME: should the evaluation gap be relative?

	// Can we afford to wait? It depends on the time left (1 minute = the limit...)
	//int time_left = _board->_player ? _time_white : _time_black;

	//float move_wait_factor = min(100.0f, 1.0f + abs(most_explored_child_eval - best_eval_colored) / 50.0f * time_left / 60000.0f);
	float move_wait_factor = 1.0f + ((best_score + 1E-6) / (most_explored_score + 1E-6) - 1.0f) * 5.0f;

	//cout << "move wait factor : " << move_wait_factor << endl;

	// 4r1k1/2Q2ppp/3p4/2p5/2B1P3/2P1q2P/PPP3P1/1K4R1 w - - 1 25


	//float move_wait_factor = wait_for_best_move ? wait_factor : 1.0f; // That is a lot, but something better has to be found here

	//cout << "base time : " << max_move_time << " | wait for best move : " << wait_for_best_move << " | eval diff : " << eval_diff << " | move wait factor : " << move_wait_factor << " | final time : " << max_move_time * move_wait_factor << endl;
	
	//cout << "max move time : " << max_move_time << endl;

	max_move_time = max_move_time * move_wait_factor;

	//cout << "new max move time : " << max_move_time << endl;

	// Reduce the time spent on the move when we are sure it is the right one
	if (most_explored_move_is_best) {
		max_move_time *= 1.0f - best_move_percentage;

		//cout << "is best move, new max move time : " << max_move_time << endl;
	}

	// Sometimes an overflow happens
	if (max_move_time < 0) {
		cout << "overflow in max move time" << endl;
		cout << "max move time : " << max_move_time << endl;
		max_move_time = DBL_MAX;
	}

	// Assumed number of iterations per second
	//constexpr int supposed_ips = 1000;
	//const int supposed_ips = max(750, _root_exploration_node->get_ips());

	constexpr int average_nps = 5000; // Pour une position semi-complexe
	constexpr float consistent_factor = 0.35f; // The larger this factor, the more constant the time used, whatever the complexity of the position

	const int actual_ips = _root_exploration_node->get_ips();

	const int supposed_ips = average_nps + (actual_ips - average_nps) * consistent_factor;



	// Number of nodes Grogros has to compute (given the time constraints)
	//int grogros_nps = _root_exploration_node->get_avg_nps();

	// Equivalent in number of nodes
	double seconds_to_play = max_move_time / 1000.0;
	//int nodes_to_play = grogros_nps * seconds_to_play;
	double iterations_to_play = supposed_ips * seconds_to_play;

	// Overflow (FIXME: this needs better handling...)
	//if (nodes_to_play < 0) {
	//	cout << "RE: overflow in max move time (nodes to play)" << endl;
	//	nodes_to_play = 0;
	//}

	//cout << "nodes to play : " << nodes_to_play << ", " << _root_exploration_node->_nodes << endl;

	if (_root_exploration_node->_iterations >= iterations_to_play) {
	//if (_root_exploration_node->_nodes >= nodes_to_play) {

		//cout << "best eval" << best_eval_colored << endl;
		//for (auto const& child : _root_exploration_node->_children) {
		//	cout << "move : " << _board->move_label(child.first) << " | eval : " << child.second->_deep_evaluation._value << " | nodes : " << child.second->_nodes << " | iterations : " << child.second->_iterations << endl;
		//}

		if (wait_for_best_move) {
			cout << "Position: " << _board->to_fen() << " : played the sub-optimal " << _board->_moves_count << ". " << _board->move_label(_root_exploration_node->get_most_explored_child_move()) << " because it was taking too long to wait for it... best move was probably:" << _board->move_label(best_move) << endl;
			cout << "Scores: " << most_explored_score << " | " << best_score << " -> wait factor: " << move_wait_factor << endl;
		}

		//cout << nodes_to_play << ", max move time : " << max_move_time << ", supposed speed : " << supposed_grogros_speed << ", nodes : " << _root_exploration_node->_nodes << endl;
		((_click_bind && _board->click_m_move(_root_exploration_node->get_most_explored_child_move(), get_board_orientation())) || true) && play_move_keep(_root_exploration_node->get_most_explored_child_move());
	}

	return;
}

// Initializes the colours of the chess websites
void GUI::init_chess_sites() {

	// Chess.com (setup with green squares and the default pieces)
	ChessSite chess_com;
	chess_com._name = "chess.com";
	chess_com._white_tile_color = SimpleColor(235, 236, 208);
	chess_com._black_tile_color = SimpleColor(115, 149, 82);
	chess_com._white_piece_color = SimpleColor(249, 249, 249);
	chess_com._black_piece_color = SimpleColor(92, 89, 87);
	chess_com._white_tile_played_color = SimpleColor(245, 246, 130);
	chess_com._black_tile_played_color = SimpleColor(185, 202, 67);
	chess_com._piece_location_on_tile = { 0.15f, 0.50f };
	chess_com._tile_location_on_tile = { 0.85f, 0.85f };
	chess_com._time_lost_per_move = 125;
	chess_com._tile_color_tolerance = 0.02f;
	chess_com._piece_color_tolerance = 0.05f;

	_chess_sites.push_back(chess_com);

	// Lichess.org (green and white board, with reduced brightness)
	// Piece set "alpha"
	ChessSite lichess_org;
	lichess_org._name = "lichess.org";
	lichess_org._white_tile_color = SimpleColor(184, 184, 159);
	lichess_org._black_tile_color = SimpleColor(96, 119, 73);
	lichess_org._white_piece_color = SimpleColor(204, 204, 204);
	lichess_org._black_piece_color = SimpleColor(13, 13, 13);
	lichess_org._white_tile_played_color = SimpleColor(108, 160, 161);
	lichess_org._black_tile_played_color = SimpleColor(56, 122, 110);
	lichess_org._piece_location_on_tile = { 0.17f, 0.66f };
	lichess_org._tile_location_on_tile = { 0.90f, 0.90f };
	lichess_org._time_lost_per_move = 100;
	lichess_org._tile_color_tolerance = 0.02f;
	lichess_org._piece_color_tolerance = 0.05f;

	_chess_sites.push_back(lichess_org);

	// Internet Chess Club (setup with brown squares and the 'Default' pieces)
	ChessSite internet_chess_club;
	internet_chess_club._name = "ICC";
	internet_chess_club._white_tile_color = SimpleColor(255, 219, 163);
	internet_chess_club._black_tile_color = SimpleColor(181, 136, 99);
	internet_chess_club._white_piece_color = SimpleColor(249, 249, 249);
	internet_chess_club._black_piece_color = SimpleColor(63, 70, 77);
	internet_chess_club._white_tile_played_color = SimpleColor(255, 233, 98);
	internet_chess_club._black_tile_played_color = SimpleColor(211, 184, 59);
	internet_chess_club._piece_location_on_tile = { 0.25f, 0.50f };
	internet_chess_club._tile_location_on_tile = { 0.85f, 0.90f };
	internet_chess_club._time_lost_per_move = 425;
	internet_chess_club._tile_color_tolerance = 0.02f;
	internet_chess_club._piece_color_tolerance = 0.05f;

	_chess_sites.push_back(internet_chess_club);

}


// Updates the binding move from the online board. Returns true if the move was modified and is valid
bool GUI::update_binding_move() {

	//cout << "updating binding move" << endl;

	// Read the coordinates of the move
	uint8_t *move_coords = get_board_move(_binding_left, _binding_top, _binding_right, _binding_bottom, _current_site, get_board_orientation());

	if (move_coords == nullptr) {
		//cout << "no move coords found" << endl;
		return false;
	}

	//cout << "binding move: " << (int)move_coords[0] << ", " << (int)move_coords[1] << ", " << (int)move_coords[2] << ", " << (int)move_coords[3] << endl;

	// Adaptation of the move depending on the website:
	// On lichess, when castling, the highlighted squares are the king and the rook (not the destination square of the king)
	uint8_t start_row = move_coords[0];
	uint8_t start_col = move_coords[1];
	uint8_t end_row = move_coords[2];
	uint8_t end_col = move_coords[3];

	// For castling on some websites the departure and arrival squares cannot be told apart, so they may be swapped
	if (is_king(_root_exploration_node->_board->_array[end_row][end_col]) && abs(start_col - end_col) > 2) {
		//cout << "king move inverted" << endl;
		start_row = end_row;
		start_col = end_col;
		end_row = move_coords[0];
		end_col = move_coords[1];
	}

	if (is_king(_root_exploration_node->_board->_array[start_row][start_col]) && abs(start_col - end_col) > 2) {
		//cout << "castling attempt: " << (int)start_row << ", " << (int)start_col << ", " << (int)end_row << ", " << (int)end_col << endl;
		end_col = start_col + 2 * ((end_col > start_col) ? 1 : -1);
		//cout << "castling move: " << (int)start_row << ", " << (int)start_col << ", " << (int)end_row << ", " << (int)end_col << endl;
	}

	Move move = Move(start_row, start_col, end_row, end_col);

	if (move == _binding_move) {
		return false;
	}

	// If the move exists
	for (int i = 0; i < _root_exploration_node->_board->_got_moves; i++) {
		if (_root_exploration_node->_board->_moves[i] == move) {
			_binding_move = move;
			return true;
		}
	}

	return false;
}

// Evaluates (and displays the components)
void GUI::evaluate_position(bool display, bool static_only) {
	_root_exploration_node->evaluate_position(_grogros_eval, display, nullptr, true, static_only);
}

// Initializes the buffers
void GUI::init_buffers() const {

	// Adaptive sizing from the available physical RAM (#13)
	if (!monte_board_buffer._init || !monte_node_buffer._init) {
		const PoolSizing ps = compute_pool_sizing();
		if (!monte_board_buffer._init)
			monte_board_buffer.init(ps.board_length);
		if (!monte_node_buffer._init)
			monte_node_buffer.init(ps.node_length);
	}
}

// Walk the entire reachable tree from `root` and return every node (and its
// board) to the buffer free-list.  parent_count is set to 0 on every node so
// that the bottom-up recycle order is respected.  This must be called BEFORE
// node_map.clear() — otherwise orphaned DAG nodes (parent_count > 0 from
// recycled parents) would never be freed (#node-buffer-leak).
static void recycle_tree(Node* root) {
	if (root == nullptr)
		return;

	thread_local vector<Node*> worklist;
	thread_local vector<Node*> to_recycle;
	worklist.clear();
	to_recycle.clear();

	// Seed: reset root's own parent_count (will be recycled by caller)
	root->_parent_count = 0;

	// BFS: push all direct children
	for (auto const& [_, child_link] : root->_children) {
		if (child_link._node == nullptr)
			continue;
		child_link._node->_parent_count = 0;
		worklist.push_back(child_link._node);
	}

	// Walk the rest of the tree
	while (!worklist.empty()) {
		Node* node = worklist.back();
		worklist.pop_back();

		for (auto const& [_, child_link] : node->_children) {
			if (child_link._node == nullptr)
				continue;
			child_link._node->_parent_count = 0;
			worklist.push_back(child_link._node);
		}
		to_recycle.push_back(node);
	}

	// Recycle deepest-first (children before parents)
	for (auto it = to_recycle.rbegin(); it != to_recycle.rend(); ++it) {
		Node* node = *it;
		// Free board buffer slot
		if (node->_board != nullptr && node->_board->_buffer_index >= 0 && !monte_board_buffer._bulk_resetting)
			monte_board_buffer.free_index(node->_board->_buffer_index);
		// Free node buffer slot
		if (node->_buffer_index >= 0 && !monte_node_buffer._bulk_resetting)
			monte_node_buffer.free_index(node->_buffer_index);
	}
}

// Resets the buffers
// #12: do NOT sweep the whole capacity. monte_*_buffer.reset() looped over
// 5M Board::reset_board() + 10M Node::reset(false) - each clearing a
// robin_map depuis a258fb5 (_positions_history / _children) → chargement FEN
// slow / "never finishes". The real reclamation of the *used* tree is done
// in O(used) by the recursive _root_exploration_node->reset() already called
// just after by load_FEN (gui.cpp:1504) and reset_game (gui.cpp:1530).
// The O(capacity) sweep was purely redundant.
void GUI::reset_buffers() const {
	transposition_table.clear();
	node_map.clear(); // #11 Plan B — purge le DAG en meme temps que la TT (pas de pointeur pendant inter-recherches)
}

