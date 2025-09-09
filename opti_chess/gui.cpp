#include "gui.h"
#include "buffer.h"
#include "useful_functions.h"
#include "windows_tests.h"
#include <wchar.h>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include "ranges"

// Fonction qui met à jour le temps des joueurs
void GUI::update_time() {
	// Faut-il quand même mettre à jour le temps quand il est désactivé?
	if (!_time)
		return;

	if (_last_player)
		_time_white -= clock() - _last_move_clock;
	else
		_time_black -= clock() - _last_move_clock;

	_last_move_clock = clock();

	// Gestion du temps
	if (_board._player != _last_player) {
		if (_board._player) {
			_time_black -= clock() - _last_move_clock - _time_increment_black;
			//_pgn += " {[%clk " + clock_to_string(_time_black, true) + "]}";
		}
		else {
			_time_white -= clock() - _last_move_clock - _time_increment_white;
			//_pgn += " {[%clk " + clock_to_string(_time_white, true) + "]}";
		}

		_last_move_clock = clock();
	}

	_last_player = _board._player;
}

// Fonction qui lance le temps
void GUI::start_time() {
	update_time_control();
	_time = true;
	_last_move_clock = clock();
}

// Fonction qui stoppe le temps
void GUI::stop_time() {
	update_time_control();
	update_time();
	_time = false;
}

// Constructeur GUI
GUI::GUI() {
}

// GUI
GUI main_GUI;

// Fonction qui met en place le binding avec le site d'échecs pour une nouvelle partie
bool GUI::new_bind_game() {
	const int orientation = bind_board_orientation(_binding_left, _binding_top, _binding_right, _binding_bottom, _current_site);

	if (orientation == -1)
		return false;

	reset_game();

	if (get_board_orientation() != orientation)
		switch_orientation();

	if (orientation) {
		// Joueur blanc
		_white_player = _grogros_zero_name;
		_white_title = "BOT";
		_white_elo = _grogros_zero_elo;
		_white_url = "https://images.chesscomfiles.com/uploads/v1/user/284728633.4af59e2f.50x50o.0c8cdf830b69.png";
		_white_country = "57";

		// Joueur noir
		_black_player = _current_site._name + " player";
		_black_title = "";
		_black_elo = "";
		_black_url = "";
		_black_country = "";
	}
	else {
		// Joueur blanc
		_white_player = _current_site._name + " player";
		_white_title = "";
		_white_elo = "";
		_white_url = "";
		_white_country = "";

		// Joueur noir
		_black_player = _grogros_zero_name;
		_black_title = "BOT";
		_black_elo = _grogros_zero_elo;
		_black_url = "https://images.chesscomfiles.com/uploads/v1/user/284728633.4af59e2f.50x50o.0c8cdf830b69.png";
		_black_country = "57";
	}

	_binding_solo = true;
	_binding_full = false;
	_click_bind = true;
	if (!monte_buffer._init)
		monte_buffer.init();
	start_time();
	_grogros_analysis = false;

	return true;
}

// Fonction qui met en place le binding avec chess.com pour une nouvelle analyse de partie
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
	if (!monte_buffer._init)
		monte_buffer.init();
	_grogros_analysis = true;

	return true;
}

// Fonction qui construit le PGN global
bool GUI::update_global_pgn()
{
	_global_pgn = "";

	// Headers

	// Joueurs
	if (!_white_player.empty())
		_global_pgn += "[White \"" + _white_player + "\"]\n";
	if (!_black_player.empty())
		_global_pgn += "[Black \"" + _black_player + "\"]\n";

	// Titres des joueurs
	if (!_white_title.empty())
		_global_pgn += "[WhiteTitle \"" + _white_title + "\"]\n";
	if (!_black_title.empty())
		_global_pgn += "[BlackTitle \"" + _black_title + "\"]\n";

	// Elo des joueurs
	if (!_white_elo.empty())
		_global_pgn += "[WhiteElo \"" + _white_elo + "\"]\n";
	if (!_black_elo.empty())
		_global_pgn += "[BlackElo \"" + _black_elo + "\"]\n";

	// URL des joueurs
	if (!_white_url.empty())
		_global_pgn += "[WhiteUrl \"" + _white_url + "\"]\n";
	if (!_black_url.empty())
		_global_pgn += "[BlackUrl \"" + _black_url + "\"]\n";

	// Pays des joueurs
	if (!_white_country.empty())
		_global_pgn += "[WhiteCountry \"" + _white_country + "\"]\n";
	if (!_black_country.empty())
		_global_pgn += "[BlackCountry \"" + _black_country + "\"]\n";

	// Cadence
	if (!_time_control.empty())
		_global_pgn += "[TimeControl \"" + _time_control + "\"]\n";

	// FEN importé
	if (!_initial_fen.empty())
		_global_pgn += "[FEN \"" + _initial_fen + "\"]\n";

	// Date
	if (!_date.empty())
		_global_pgn += "[Date \"" + _date + "\"]\n";

	// Ajout du PGN de la partie
	_global_pgn += _pgn;

	return true;
}

// Fonction qui met à jour la cadence du PGN
bool GUI::update_time_control()
{
	_time_control = to_string(static_cast<int>(max(_time_white, _time_black) / 1000)) + " + " + to_string(static_cast<int>(max(_time_increment_white, _time_increment_black) / 1000));
	return true;
}

// Fonction qui réinitialise le PGN
bool GUI::reset_pgn()
{
	update_date();
	_pgn = "";
	_global_pgn = "";
	_initial_fen = "";

	return  true;
}

// Fonction qui met à jour la date du PGN
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

// Fonction qui lance les threads de GrogrosZero
//bool GUI::thread_grogros_zero(Evaluator* eval, int nodes)
//{
//	// Initialisation du buffer pour GrogrosZero, si besoin
//	if (!monte_buffer._init)
//		monte_buffer.init();
//
//
//	// Lance grogros sur chaque noeud fils pour l'initialisation
//	//_board.grogros_zero(eval, _board._got_moves, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr, 0);
//
//	_threads_grogros_zero.clear();
//
//	for (int i = 0; i < _board._got_moves; i++)
//		_threads_grogros_zero.emplace_back(&Board::grogros_zero, &monte_buffer._heap_boards[_board._index_children[i]], eval, nodes, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr, 0);
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
//	// Faut re-additionner les temps de monte carlo de chaque fils (pareil pour les quiescence nodes)
//	// Il faut aussi update toutes les variantes
//
//	// Relance grogros sur 1 noeud (pour actualiser les valeurs)
//	//_board.grogros_zero(eval, 100, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr, 0);
//
//	return true;
//}

// Fonction qui lance grogros sur un thread
//bool GUI::grogros_zero_threaded(Evaluator* eval, int nodes) {
//	// Initialisation du buffer pour GrogrosZero, si besoin
//	if (!monte_buffer._init)
//		monte_buffer.init();
//
//	// Lance grogros sur un thread
//	_thread_grogros_zero = thread(&Board::grogros_zero, &_board, eval, nodes, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr, 0);
//
//	_thread_grogros_zero.detach();
//
//	return true;
//}

// Fonction qui retire le dernier coup du PGN
bool GUI::remove_last_move_PGN()
{
	// TODO	

	return false;
}

// Fonction qui dessine les flèches en fonction des valeurs dans l'algo de Monte-Carlo
void GUI::draw_exploration_arrows()
{
	// Vecteur de flèches à afficher
	_grogros_arrows.clear();

	if (_root_exploration_node->_nodes <= 1 || _root_exploration_node->_is_terminal)
		return;

	// Coup à surligner
	const Move best_move = _root_exploration_node->get_most_explored_child_move();

	// Coup avec la meilleure évaluation
	const Move best_eval_move = _root_exploration_node->get_best_score_move(_alpha, _beta);

	if (best_eval_move.is_null_move()) {
		cout << "null best eval move" << endl;
	}

	// Une pièce est-elle sélectionnée?
	const bool is_selected = _selected_pos.row != -1 && _selected_pos.col != -1;

	// Crée un vecteur avec les coups explorés par GrogrosZero
	vector<Move> iterated_moves_vector;

	for (auto const& [move, child] : _root_exploration_node->_children) {
		// Si une pièce est sélectionnée, dessine toutes les flèches pour cette pièce
		if (is_selected) {
			if (_selected_pos.row == move.start_row && _selected_pos.col == move.start_col)
				iterated_moves_vector.push_back(move);
		}

		// Sinon, dessine les flèches pour les coups les plus explorés
		else {
			// On ne rajoute pas pour le moment les coups en surbrillance
			if (move == best_move || move == best_eval_move)
				continue;

			// Le coup a-t-il été exploré par GrogrosZero, ou seulement la quiescence?
			if (_root_exploration_node->_iterations > 0) {
				if (static_cast<float>(child->_chosen_iterations) / static_cast<float>(_root_exploration_node->_iterations) > _arrow_rate) {
					iterated_moves_vector.push_back(move);
				}
			}
			else {
				// TODO: quiescence arrows
				// en rouge? blanc? avec un "?" au bout?
				if (_root_exploration_node->_nodes > 0) {
					iterated_moves_vector.push_back(move);
				}
			}
		}
	}

	// Trie les coups en fonction du nombre de noeuds et d'un affichage plus lisible
	std::ranges::sort(iterated_moves_vector.begin(), iterated_moves_vector.end(), [this](const Move m1, const Move m2) {
		return this->compare_arrows(m1, m2); }
	);

	if (!is_selected) {
		// Ajoute les coups à afficher dans tous les cas
		iterated_moves_vector.push_back(best_eval_move);

		if (best_eval_move != best_move) {
			iterated_moves_vector.push_back(best_move);
		}
	}

	// Dessine les flèches
	for (const Move move : iterated_moves_vector) {
		const int mate = _root_exploration_node->_board->is_eval_mate(_root_exploration_node->_children[move]->_deep_evaluation._value);
		Node const *child = _root_exploration_node->_children[move];
		draw_arrow(move, _root_exploration_node->_board->_player, move_color(child->_chosen_iterations, _root_exploration_node->_iterations, child->_iterations == 0), -1.0f, true, child->_deep_evaluation._avg_score, mate, move == best_move, move == best_eval_move);
	}
}

// Fonction qui obtient la case correspondante à la position sur la GUI
Pos GUI::get_pos_from_GUI(const float x, const float y) {
	if (!is_in(x, _board_padding_x, _board_padding_x + _board_size) || !is_in(y, _board_padding_y, _board_padding_y + _board_size))
		return Pos(-1, -1);
	else
		return Pos(orientation_index(8 - (y - _board_padding_y) / _tile_size), orientation_index((x - _board_padding_x) / _tile_size));
}

// Fonction qui permet de changer l'orientation du plateau
void GUI::switch_orientation() {
	_board_orientation = !_board_orientation;
}

// Fonction aidant à l'affichage du plateau (renvoie i si board_orientation, et 7 - i sinon)
int GUI::orientation_index(const int i) const {
	if (_board_orientation)
		return i;
	return 7 - i;
}

// Fonction qui dessine la flèche d'un coup
void GUI::draw_arrow(const Move move, const bool player, Color c, float thickness, const bool use_value, const float avg_score, const int mate, const bool is_most_explored, const bool is_best_eval)
{
	const uint_fast8_t start_row = move.start_row;
	const uint_fast8_t start_col = move.start_col;
	const uint_fast8_t end_row = move.end_row;
	const uint_fast8_t end_col = move.end_col;

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

	// Outline pour le coup le plus exploré
	if (is_most_explored) {
		if (is_knight_move)
			draw_line_bezier(x1, y1, x2, y2, thickness * 1.4f, BLACK);
		else
			draw_line_ex(x1, y1, x2, y2, thickness * 1.4f, BLACK);
		draw_circle(x1, y1, thickness * 1.2f, BLACK);
		draw_circle(x2, y2, thickness * 2.0f * 1.1f, BLACK);
	}
	
	// Outline pour le coup avec la meilleure évaluation
	if (is_best_eval) {
		if (is_knight_move)
			draw_line_bezier(x1, y1, x2, y2, thickness * 1.4f, WHITE);
		else
			draw_line_ex(x1, y1, x2, y2, thickness * 1.4f, WHITE);
		draw_circle(x1, y1, thickness * 1.2f, WHITE);
		draw_circle(x2, y2, thickness * 2.0f * 1.1f, WHITE);
	}

	// "Flèche"
	if (is_knight_move)
		draw_line_bezier(x1, y1, x2, y2, thickness, c);
	else
		draw_line_ex(x1, y1, x2, y2, thickness, c);
	draw_circle(x1, y1, thickness, c);
	draw_circle(x2, y2, thickness * 2.0f, c);

	// Rajoute une valeur à la flèche
	if (use_value) {

		// Valeur à afficher
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

	// Ajoute la flèche au vecteur
	_grogros_arrows.push_back(move);

	return;
}

// Couleur de la flèche en fonction du coup (de son nombre de noeuds)
Color GUI::move_color(const int explorations, const int total_explorations, bool is_quiescence) const {
	// S'il n'y a pas d'exploration, on affiche en blanc
	if (is_quiescence)
		return GRAY;

	const float x = static_cast<float>(explorations) / static_cast<float>(total_explorations);

	// Facteur d'atténuation par le blanc
	const float white_attenuation = 0.3f;

	const auto red = static_cast<unsigned char>(255.0f * ((1 - white_attenuation) * ((x <= 0.2f) + (x > 0.2f && x < 0.4f) * (0.4f - x) / 0.2f + (x > 0.8f) * (x - 0.8f) / 0.2f) + white_attenuation));
	const auto green = static_cast<unsigned char>(255.0f * ((1 - white_attenuation) * ((x < 0.2f) * x / 0.2f + (x >= 0.2f && x <= 0.6f) + (x > 0.6f && x < 0.8f) * (0.8f - x) / 0.2f) + white_attenuation));
	const auto blue = static_cast<unsigned char>(255.0f * ((1 - white_attenuation) * ((x > 0.4f && x < 0.6f) * (x - 0.4f) / 0.2f + (x >= 0.6f)) + white_attenuation));

	//unsigned char alpha = 100 + 155 * explorations / total_explorations;
	const unsigned char alpha = 255;

	return { red, green, blue, alpha };
}

// Fonction qui charge les textures
void GUI::load_resources() {
	cout << GetWorkingDirectory() << endl;

	// Pièces
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

	// Mini-Pièces
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

	// Chargement du son
	_move_sound = LoadSound((_sounds_path + "move.mp3").c_str());
	_castle_sound = LoadSound((_sounds_path + "castle.mp3").c_str());
	_check_sound = LoadSound((_sounds_path + "check.mp3").c_str());
	_capture_sound = LoadSound((_sounds_path + "capture.mp3").c_str());
	_checkmate_sound = LoadSound((_sounds_path + "checkmate.mp3").c_str());
	_stalemate_sound = LoadSound((_sounds_path + "stalemate.mp3").c_str());
	_game_begin_sound = LoadSound((_sounds_path + "game_begin.mp3").c_str());
	_promotion_sound = LoadSound((_sounds_path + "promotion.mp3").c_str());

	// Police de l'écriture
	//_text_font = LoadFontEx("resources/fonts/SFTransRobotics.otf", 128, nullptr, 1000);
	_text_font = LoadFontEx("resources/fonts/Montserrat-Bold.otf", 128, nullptr, 256);
	//_text_font = LoadFontEx("resources/fonts/Montserrat-Medium.ttf", 128, nullptr, 0);
	GenTextureMipmaps(&_text_font.texture);
	SetTextureFilter(_text_font.texture, TEXTURE_FILTER_TRILINEAR);
	//SetTextureFilter(_text_font.texture, TEXTURE_FILTER_ANISOTROPIC_4X);

	// Shader pour le texte
	//_text_shader = LoadShader(nullptr, "resources/shaders/font_sdf.fs");
	//_text_shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(_text_shader, "view");
	//_text_font.texture.sg

	// Icône
	_icon = LoadImage("resources/images/grogros_zero.png"); // TODO essayer de charger le .ico, pour que l'icone s'affiche tout le temps (pas seulement lors du build)
	SetWindowIcon(_icon);
	UnloadImage(_icon);

	// Grogros
	_grogros_image = LoadImage("resources/images/grogros_zero.png");

	// Curseur
	_cursor_image = LoadImage("resources/images/cursor_new.png");

	_loaded_resources = true;
}

// Fonction qui met à la bonne taille les images et les textes de la GUI
void GUI::resize_GUI() {
	const int min_screen = min(_screen_height, _screen_width);
	_board_size = _board_scale * min_screen;
	_board_padding_y = (_screen_height - _board_size) / 4.0f;
	_board_padding_x = (_screen_height - _board_size) / 8.0f;

	_tile_size = _board_size / 8.0f;
	_piece_size = _tile_size * _piece_scale;
	_arrow_thickness = _tile_size * _arrow_scale;

	// Génération des textures

	// Pièces
	for (int i = 0; i < 12; i++) {
		Image piece_image = ImageCopy(_piece_images[i]);
		ImageResize(&piece_image, _piece_size, _piece_size);
		Texture2D texture = LoadTextureFromImage(piece_image);
		//GenTextureMipmaps(&texture);
		//SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
		//SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
		_piece_textures[i] = texture;
	}

	// Taille du texte
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

	// Mini-pièces (pour le compte des pièces prises durant la partie)
	_mini_piece_size = _text_size / 3;
	for (int i = 0; i < 12; i++) {
		Image mini_piece_image = ImageCopy(_mini_piece_images[i]);
		ImageResize(&mini_piece_image, _mini_piece_size, _mini_piece_size);
		_mini_piece_textures[i] = LoadTextureFromImage(mini_piece_image);
	}
}

// Fonction qui actualise les nouvelles dimensions de la fenêtre
void GUI::get_window_size() {
	_screen_width = GetScreenWidth();
	_screen_height = GetScreenHeight();
}

// Fonction qui renvoie si le joueur est en train de jouer (pour que l'IA arrête de réflechir à ce moment sinon ça lagge)
bool GUI::is_playing() const {
	const auto [x, y] = GetMousePosition();
	return (_selected_pos.row != -1 || x != _mouse_pos.x || y != _mouse_pos.y);
}

// Fonction qui change le mode d'affichage des flèches (oui/non)
void GUI::switch_arrow_drawing() {
	_drawing_arrows = !_drawing_arrows;
}

// Fonction qui affiche un texte dans une zone donnée avec un slider
void GUI::slider_text(const string& s, float pos_x, float pos_y, float width, float height, float size, float* slider_value, Color t_color, float slider_width, float slider_height) {

	// FIXME *** test: si rien a changé dans le texte et les dimensions, on pourrait stocker ce texte, non?

	// Dessine le canvas, rectangle
	DrawRectangleRec({ pos_x, pos_y, width, height }, _background_text_color);

	// Taille des sliders
	if (slider_width == -1.0f)
		slider_width = _screen_width / 100.0f;

	if (slider_height == -1.0f)
		slider_height = height;

	// Split le texte en parties égales

	// Estimation du nombre de caractères par ligne
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

	// Taille verticale totale du texte

	// Estimation de la taille verticale d'une ligne
	const float space_size = MeasureTextEx(_text_font, "\n", size, _font_spacing * size).y - estimated_size.y;

	float text_height = estimated_size.y + space_size * rows;

	// Si le texte prend plus de place verticalement que l'espace alloué
	if (text_height > height) {

		// Nombre de lignes à afficher
		int n_lines = 1 + (height - estimated_size.y) / space_size;

		// Numéro de ligne à laquelle commencer
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

		// Avec la molette
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

// Fonction pour obtenir l'orientation du plateau
bool GUI::get_board_orientation() const {
	return _board_orientation;
}

// Fonction qui renvoie si le curseur de la souris se trouve dans le rectangle
bool GUI::is_cursor_in_rect(const Rectangle rec) {
	_mouse_pos = GetMousePosition();
	return (is_in(_mouse_pos.x, rec.x, rec.x + rec.width) && is_in(_mouse_pos.y, rec.y, rec.y + rec.height));
}

// Fonction qui dessine un rectangle à partir de coordonnées flottantes
bool GUI::draw_rectangle(const float pos_x, const float pos_y, const float width, const float height, const Color color) {
	DrawRectangle(float_to_int(pos_x), float_to_int(pos_y), float_to_int(width + pos_x) - float_to_int(pos_x), float_to_int(height + pos_y) - float_to_int(pos_y), color);
	return true;
}

// Fonction qui dessine un rectangle à partir de coordonnées flottantes, en fonction des coordonnées de début et de fin
bool GUI::draw_rectangle_from_pos(const float pos_x1, const float pos_y1, const float pos_x2, const float pos_y2, const Color color) {
	DrawRectangle(float_to_int(pos_x1), float_to_int(pos_y1), float_to_int(pos_x2) - float_to_int(pos_x1), float_to_int(pos_y2) - float_to_int(pos_y1), color);
	return true;
}

// Fonction qui dessine un cercle à partir de coordonnées flottantes
void GUI::draw_circle(const float pos_x, const float pos_y, const float radius, const Color color) {
	DrawCircle(float_to_int(pos_x), float_to_int(pos_y), radius, color);
}

// Fonction qui dessine une ligne à partir de coordonnées flottantes
void GUI::draw_line_ex(const float x1, const float y1, const float x2, const float y2, const float thick, const Color color) {
	DrawLineEx({ static_cast<float>(float_to_int(x1)), static_cast<float>(float_to_int(y1)) }, { static_cast<float>(float_to_int(x2)), static_cast<float>(float_to_int(y2)) }, thick, color);
}

// Fonction qui dessine une ligne de Bézier à partir de coordonnées flottantes
void GUI::draw_line_bezier(const float x1, const float y1, const float x2, const float y2, const float thick, const Color color) {
	DrawLineBezier({ static_cast<float>(float_to_int(x1)), static_cast<float>(float_to_int(y1)) }, { static_cast<float>(float_to_int(x2)), static_cast<float>(float_to_int(y2)) }, thick, color);
}

// Fonction qui dessine une texture à partir de coordonnées flottantes
void GUI::draw_texture(const Texture& texture, const float pos_x, const float pos_y, const Color color) {
	DrawTexture(texture, float_to_int(pos_x), float_to_int(pos_y), color);
}

// Fonction qui affiche la barre d'evaluation
void GUI::draw_eval_bar(const float eval, WDL wdl, const string& text_eval, const float x, const float y, const float width, const float height, const float max_eval, const Color white, const Color gray, Color black, float max_height) {
	const bool is_mate = text_eval.find('M') != -1;

	// Taille max de la barre
	if (max_height == -1.0f)
		max_height = 0.95f;

	// Coupe l'évaluation à 2 chiffres max
	// FIXME: ça suppose que l'eval dépasse pas +100 (ou +10000 dans l'équivalent de Grogros)
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

	// Largeur que le texte doit occuper
	float max_text_width = width * 0.9f;
	if (text_dimensions.x > max_text_width)
		t_size = t_size * max_text_width / text_dimensions.x;
	text_dimensions = MeasureTextEx(_text_font, eval_text.c_str(), t_size, _font_spacing);
	DrawTextEx(_text_font, eval_text.c_str(), { x + (width - text_dimensions.x) / 2.0f, y + (y_margin + text_pos * (1.0f - y_margin * 2.0f)) * height - text_dimensions.y * text_pos }, t_size, _font_spacing, (eval < 0) ? white : black);
}

// Fonction qui retire les surlignages de toutes les cases
void GUI::remove_highlighted_tiles() {
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++)
			_highlighted_array[i][j] = 0;
}

// Fonction qui selectionne une case
void GUI::select_tile(int a, int b) {
	_selected_pos = Pos(a, b);
}

// Fonction qui surligne une case (ou la de-surligne)
void GUI::highlight_tile(const int a, const int b) {
	_highlighted_array[a][b] = 1 - _highlighted_array[a][b];
}

// Fonction qui déselectionne
void GUI::unselect() {
	_selected_pos = Pos(-1, -1);
}

// A partir de coordonnées sur le plateau
void GUI::draw_simple_arrow_from_coord(const int i1, const int j1, const int i2, const int j2, float thickness, Color c) {
	// cout << thickness << endl;
	if (thickness == -1.0f)
		thickness = _arrow_thickness;
	const float x1 = _board_padding_x + _tile_size * orientation_index(j1) + _tile_size / 2;
	const float y1 = _board_padding_y + _tile_size * orientation_index(7 - i1) + _tile_size / 2;
	const float x2 = _board_padding_x + _tile_size * orientation_index(j2) + _tile_size / 2;
	const float y2 = _board_padding_y + _tile_size * orientation_index(7 - i2) + _tile_size / 2;

	c.a = 255;

	// "Flèche"
	if (abs(j2 - j1) != abs(i2 - i1) && abs(j2 - j1) + abs(i2 - i1) == 3)
		draw_line_bezier(x1, y1, x2, y2, thickness, c);
	else
		draw_line_ex(x1, y1, x2, y2, thickness, c);

	//c.a = 255;

	draw_circle(x1, y1, thickness, c);
	draw_circle(x2, y2, thickness * 2.0f, c);
}

// Joue un coup en gardant la réflexion de GrogrosZero
bool GUI::play_move_keep(Move move)
{
	_board.assign_move_flags(&move);

	// S'assure que le coup est légal
	if (!_board.is_legal(move))
		return false;

	// Joue le son du coup
	_board.play_move_sound(move);

	// On update les variantes
	_update_variants = true;

	// Arbre de la partie
	_game_tree.add_child(move);

	// FIXME: les vraies distinctions de cas à faire: 
	// y'a t-il eu des coups calculés? -> oui/non
	// si oui, le coup joué en fait-il partie? -> oui/non

	if (_root_exploration_node->children_count() == 0) {
		// On joue simplement le coup
		_board.make_move(move, false, false, true);

		// On met à jour le plateau de recherche
		//_root_exploration_node->_board = &_board;
	}

	// Si le coup a effectivement été calculé
	else {
		if (_root_exploration_node->_children.contains(move)) {
			//cout << "children nodes: " << _root_exploration_node->_children[move]->_nodes << endl;

			for (auto const& [m, child] : _root_exploration_node->_children) {
				if (m != move) {
					//cout << 0 << endl;
					child->reset();
					//cout << 1 << endl;
					delete child;
				}
			}

			//cout << "toto" << endl;

			// Fait un reset du plateau
			_root_exploration_node->_board->reset_board();
			_board.reset_board();

			//cout << "tata" << endl;

			// Il faudra supprimer le parent et tous les fils (TODO)

			// On met à jour le noeud de recherche
			_root_exploration_node = _root_exploration_node->_children[move];

			// On met à jour le plateau
			_board = *_root_exploration_node->_board;
			//_root_exploration_node->_board = &_board;
		}

		// Sinon, on joue simplement le coup
		else {
			// On supprime toutes les recherces
			_root_exploration_node->reset();

			// On joue simplement le coup
			_board.make_move(move, false, false, true);

			// On met à jour le plateau de recherche
			//_root_exploration_node->_board = &_board;
		}
	}

	_root_exploration_node->_board = &_board;

	_board.get_moves();

	//cout << "same board: " << (_root_exploration_node->_board == &_board) << endl;
	//cout << "same board2: " << (*_root_exploration_node->_board == _board) << endl;


	// Update le PGN
	_game_tree.select_next_node(move);
	_pgn = _game_tree.tree_display();

	if (!_board.selected_piece_has_trait())
		_selected_pos = Pos(-1, -1);

	return true;
}

// Fonction qui renvoie le type de pièce sélectionnée
uint_fast8_t GUI::selected_piece() const
{
	// Faut-il stocker cela pour éviter de le re-calculer?
	if (_selected_pos.row == -1 || _selected_pos.col == -1)
		return 0;

	return _board._array[_selected_pos.row][_selected_pos.col];
}

// Fonction qui renvoie le type de pièce où la souris vient de cliquer
uint_fast8_t GUI::clicked_piece() const
{
	if (_clicked_pos.row == -1 || _clicked_pos.col == -1)
		return 0;

	return _board._array[_clicked_pos.row][_clicked_pos.col];
}

// Fonction qui lance une analyse de GrogrosZero
void GUI::grogros_analysis(int iterations) {
	// Noeuds à explorer par frame, en visant _target_fps FPS

	// Iterations par seconde
	int iterations_per_second = _root_exploration_node->get_ips();

	// Iterations max par seconde
	//constexpr int max_iterations_per_second = 100000;

	//if (iterations_per_second > max_iterations_per_second) {
	//	//cout << "Too many iterations per second? : " << iterations_per_second << endl;
	//	iterations_per_second = 0;
	//}

	int iterations_to_explore = iterations_per_second / _target_fps;
	if (iterations_to_explore == 0)
		iterations_to_explore = 1;

	//int iterations_to_explore = _root_exploration_node->get_avg_nps() / 60;
	//if (iterations_to_explore == 0)
	//	iterations_to_explore = _nodes_per_frame;

	// Il faut pas dépasser la taille du buffer
	// FIXME: meilleure façon de gérer cela?
	iterations_to_explore = min(iterations_to_explore, monte_buffer._length - _root_exploration_node->_nodes);

	//cout << "IPS: " << _root_exploration_node->get_ips() << ", exploring " << iterations_to_explore << " nodes" << endl;

	_root_exploration_node->grogros_zero(&monte_buffer, _grogros_eval, _alpha, _beta, _gamma, iterations == -1 ? iterations_to_explore : iterations, _quiescence_depth); // TODO: nombre de noeuds à paramétrer
	_update_variants = true;
}

// TODO
void GUI::draw()
{
	// Chargement des textures, si pas déjà fait
	if (!_loaded_resources) {
		load_resources();
		resize_GUI();
		PlaySound(_game_begin_sound);
	}


	// *** CLICS SOURIS ***

	// Position de la souris
	_mouse_pos = GetMousePosition();

	// Si on clique avec la souris
	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

		// Retire toute les cases surlignées
		remove_highlighted_tiles();

		// Retire toutes les flèches
		_arrows_array = {};

		// Stocke la case cliquée sur le plateau
		_clicked_pos = get_pos_from_GUI(_mouse_pos.x, _mouse_pos.y);
		_clicked = true;
		bool has_played = false;

		// S'il y'a les flèches de réflexion de GrogrosZero, et qu'aucune pièce n'est sélectionnée
		if (_drawing_arrows && !selected_piece()) {

			// On regarde dans le sens inverse pour jouer la flèche la plus récente (donc celle visible en cas de superposition)
			for (Move move : ranges::reverse_view(_grogros_arrows))
			{
				if (move.end_row == _clicked_pos.row && move.end_col == _clicked_pos.col) {
					if (_click_bind)
						_board.click_m_move(move, get_board_orientation());
					play_move_keep(move);
					has_played = true;
					continue;
				}
			}
		}

		// Si aucune pièce n'est sélectionnée et que l'on clique sur une pièce, la sélectionne
		if (!selected_piece() && clicked_piece()) {
			if (!has_played || _board.clicked_piece_has_trait()) {
				_selected_pos = _clicked_pos;
				if (_board._got_moves == -1)
					_board.get_moves();
			}
		}

		// Si une pièce est déjà sélectionnée
		else if (selected_piece()) {

			// Si on clique sur la même case que celle sélectionnée, la déselectionne
			if (_selected_pos == _clicked_pos) {
				//unselect();
			}

			else {
				
				// Si le coup est légal, le joue
				Move move = Move(_selected_pos.row, _selected_pos.col, _clicked_pos.row, _clicked_pos.col);

				if (_board.is_legal(move)) {
					if (_click_bind)
						_board.click_m_move(move, get_board_orientation());
					play_move_keep(move);

					// Déselectionne la pièce
					unselect();
				}

				else {
					// Si on clique sur une autre pièce, la sélectionne
					if (clicked_piece() && _board.clicked_piece_has_trait())
						_selected_pos = _clicked_pos;
					
					// Sinon, déselectionne la pièce
					else
						unselect();
				}
			}
		}
	}	

	// Si on relâche la souris
	if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {

		// Position de la case où l'on a relâché la souris
		Pos drop_pos = get_pos_from_GUI(_mouse_pos.x, _mouse_pos.y);

		// Si la case est bien sur le plateau
		if (is_in_fast(drop_pos.row, 0, 7) && is_in_fast(drop_pos.col, 0, 7) && is_in_fast(_selected_pos.row, 0, 7) && is_in_fast(_selected_pos.col, 0, 7)) {

			// Si on relâche la souris sur une autre case que celle où l'on a cliqué
			if (drop_pos != _selected_pos) {

				// Si le coup est légal, le joue
				Move move = Move(_selected_pos.row, _selected_pos.col, drop_pos.row, drop_pos.col);

				if (_board.is_legal(move)) {
					if (_click_bind)
						_board.click_m_move(move, get_board_orientation());
					play_move_keep(move);

					// Déselectionne la pièce
					unselect();
				}
			}
		}

		_clicked = false;
	}

	// Si on fait un clic droit
	if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {

		// Stocke la case cliquée sur le plateau
		_right_clicked_pos = get_pos_from_GUI(_mouse_pos.x, _mouse_pos.y);
	}

	// Si on fait relâche le clic droit
	if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
		Pos drop_pos = get_pos_from_GUI(_mouse_pos.x, _mouse_pos.y);

		// Si on relâche la souris sur le plateau
		if (is_in_fast(drop_pos.row, 0, 7) && is_in_fast(drop_pos.col, 0, 7)) {

			// Si on relâche la souris sur une autre case que celle où l'on a cliqué
			if (drop_pos == _right_clicked_pos) {

				// Sélectionne/déselectionne la case
				_highlighted_array[drop_pos.row][drop_pos.col] = 1 - _highlighted_array[drop_pos.row][drop_pos.col];
			}
				
			// Sinon, fait une flèche
			else {
				if (_right_clicked_pos.row != -1 && _right_clicked_pos.col != -1) {
					vector<int> arrow = { _right_clicked_pos.row, _right_clicked_pos.col, drop_pos.row, drop_pos.col };

					// Si la flèche existe, la supprime
					if (auto found_arrow = find(_arrows_array.begin(), _arrows_array.end(), arrow); found_arrow != _arrows_array.end())
						_arrows_array.erase(found_arrow);

					// Sinon, la rajoute
					else
						_arrows_array.push_back(arrow);
				}

			}
		}
	}


	// *** AFFICHAGE ***

	// Couleur de fond
	ClearBackground(_background_color);

	// Nombre de FPS
	DrawTextEx(_text_font, ("FPS : " + to_string(GetFPS())).c_str(), { _screen_width - 2 * _text_size, _text_size / 3 }, _text_size / 3, _font_spacing, _text_color);

	// Plateau
	draw_rectangle(_board_padding_x, _board_padding_y, _tile_size * 8, _tile_size * 8, _board_color_light);

	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++)
			((i + j) % 2 == 1) && draw_rectangle(_board_padding_x + _tile_size * j, _board_padding_y + _tile_size * i, _tile_size, _tile_size, _board_color_dark);

	// Coordonnées sur le plateau
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++) {
			if (j == 0 + 7 * _board_orientation) // Chiffres
				DrawTextEx(_text_font, to_string(i + 1).c_str(), { _board_padding_x + _text_size / 8, _board_padding_y + _tile_size * orientation_index(7 - i) + _text_size / 8 }, _text_size / 2, _font_spacing, ((i + j) % 2 == 1) ? _board_color_light : _board_color_dark);
			if (i == 0 + 7 * _board_orientation) // Lettres
				DrawTextEx(_text_font, _abc8.substr(j, 1).c_str(), { _board_padding_x + _tile_size * (orientation_index(j) + 1) - _text_size / 2, _board_padding_y + _tile_size * 8 - _text_size / 2 }, _text_size / 2, _font_spacing, ((i + j) % 2 == 1) ? _board_color_light : _board_color_dark);
		}

	// Surligne du dernier coup joué
	if (!_game_tree._current_node->_move.is_null_move()) {
		draw_rectangle(_board_padding_x + orientation_index(_game_tree._current_node->_move.start_col) * _tile_size, _board_padding_y + orientation_index(7 - _game_tree._current_node->_move.start_row) * _tile_size, _tile_size, _tile_size, _last_move_color);
		draw_rectangle(_board_padding_x + orientation_index(_game_tree._current_node->_move.end_col) * _tile_size, _board_padding_y + orientation_index(7 - _game_tree._current_node->_move.end_row) * _tile_size, _tile_size, _tile_size, _last_move_color);
	}

	// Cases surglignées
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++)
			if (_highlighted_array[i][j])
				draw_rectangle(_board_padding_x + _tile_size * orientation_index(j), _board_padding_y + _tile_size * orientation_index(7 - i), _tile_size, _tile_size, _highlight_color);

	// Sélection de cases et de pièces
	if (_selected_pos.row != -1 && _selected_pos.col != -1) {

		// Affiche la case séléctionnée
		draw_rectangle(_board_padding_x + orientation_index(_selected_pos.col) * _tile_size, _board_padding_y + orientation_index(7 - _selected_pos.row) * _tile_size, _tile_size, _tile_size, _select_color);
		
		// Affiche les coups possibles pour la pièce séléctionnée
		for (int i = 0; i < _board._got_moves; i++) {
			if (_board._moves[i].start_row == _selected_pos.row && _board._moves[i].start_col == _selected_pos.col) {
				draw_rectangle(_board_padding_x + orientation_index(_board._moves[i].end_col) * _tile_size, _board_padding_y + orientation_index(7 - _board._moves[i].end_row) * _tile_size, _tile_size, _tile_size, _select_color);
			}
		}
	}

	// Dessine les pièces adverses
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			uint_fast8_t piece = _board._array[i][j];
			if (piece > 0 && ((_board._player && piece >= 7) || (!_board._player && piece < 7))) {
				if (_clicked && i == _clicked_pos.row && j == _clicked_pos.col)
					draw_texture(_piece_textures[piece - 1], _mouse_pos.x - _piece_size / 2, _mouse_pos.y - _piece_size / 2, WHITE);
				else
					draw_texture(_piece_textures[piece - 1], _board_padding_x + _tile_size * orientation_index(j) + (_tile_size - _piece_size) / 2, _board_padding_y + _tile_size * orientation_index(7 - i) + (_tile_size - _piece_size) / 2, WHITE);
			}
		}
	}

	// Coups auquel l'IA réflechit...
	if (_drawing_arrows) {
		draw_exploration_arrows();
	}

	// Dessine les pièces alliées
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			uint_fast8_t piece = _board._array[i][j];
			if (piece > 0 && ((_board._player && piece < 7) || (!_board._player && piece >= 7))) {
				if (_clicked && i == _clicked_pos.row && j == _clicked_pos.col)
					draw_texture(_piece_textures[piece - 1], _mouse_pos.x - _piece_size / 2, _mouse_pos.y - _piece_size / 2, WHITE);
				else
					draw_texture(_piece_textures[piece - 1], _board_padding_x + _tile_size * orientation_index(j) + (_tile_size - _piece_size) / 2, _board_padding_y + _tile_size * orientation_index(7 - i) + (_tile_size - _piece_size) / 2, WHITE);
			}
		}
	}

	// Flèches déssinées
	for (vector<int> arrow : _arrows_array)
		draw_simple_arrow_from_coord(arrow[0], arrow[1], arrow[2], arrow[3], -1, _arrow_color);

	// Titre
	DrawTextEx(_text_font, "GROGROS CHESS", { _board_padding_x + _grogros_size / 2 + _text_size / 4.0f, _text_size / 4.0f }, _text_size / 1.25f, _font_spacing * _text_size / 1.25f, _text_color);

	// Grogros
	draw_texture(_grogros_texture, _board_padding_x, _text_size / 4.0f - _text_size / 5.6f, WHITE);

	// Joueurs de la partie
	int material = _board.material_difference();
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

	// Temps des joueurs
	// Update du temps
	update_time();
	float x_pad = _board_padding_x + _board_size - _text_size * 2;
	Color time_colors[4] = { (_time && !_board._player) ? BLACK : _dark_gray, (_time && !_board._player) ? WHITE : LIGHTGRAY, (_time && _board._player) ? WHITE : LIGHTGRAY, (_time && _board._player) ? BLACK : _dark_gray };

	// Temps des blancs
	if (!_white_time_text_box.active) {
		_white_time_text_box.value = _time_white;
		_white_time_text_box.text = clock_to_string(_white_time_text_box.value);
	}
	update_text_box(_white_time_text_box);
	if (!_white_time_text_box.active) {
		_time_white = _white_time_text_box.value;
		_white_time_text_box.text = clock_to_string(_white_time_text_box.value);
	}

	// Position du texte
	_white_time_text_box.set_rect(x_pad, _board_padding_y - _text_size / 2 * !_board_orientation + _board_size * _board_orientation, _board_padding_x + _board_size - x_pad, _text_size / 2);
	_white_time_text_box.text_size = _text_size / 3;
	_white_time_text_box.text_color = time_colors[3];
	_white_time_text_box.text_font = _text_font;
	_white_time_text_box.main_color = time_colors[2];
	draw_text_box(_white_time_text_box);

	// Temps des noirs
	if (!_black_time_text_box.active) {
		_black_time_text_box.value = _time_black;
		_black_time_text_box.text = clock_to_string(_black_time_text_box.value);
	}
	update_text_box(_black_time_text_box);
	if (!_black_time_text_box.active) {
		_time_black = _black_time_text_box.value;
		_black_time_text_box.text = clock_to_string(_black_time_text_box.value);
	}

	// Position du texte
	_black_time_text_box.set_rect(x_pad, _board_padding_y - _text_size / 2 * _board_orientation + _board_size * !_board_orientation, _board_padding_x + _board_size - x_pad, _text_size / 2);
	_black_time_text_box.text_size = _text_size / 3;
	_black_time_text_box.text_color = time_colors[1];
	_black_time_text_box.text_font = _text_font;
	_black_time_text_box.main_color = time_colors[0];
	draw_text_box(_black_time_text_box);

	// FEN
	_current_fen = _board.to_fen();
	const char* fen = _current_fen.c_str();
	DrawTextEx(_text_font, fen, { _text_size / 2, _board_padding_y + _board_size + _text_size * 3 / 2 }, _text_size / 3, _font_spacing * _text_size / 3, _text_color_blue);

	// PGN
	update_global_pgn();
	slider_text(_global_pgn, _text_size / 2, _board_padding_y + _board_size + _text_size * 2, _screen_width - _text_size, _screen_height - (_board_padding_y + _board_size + _text_size * 2) - _text_size / 3, _text_size / 3, &_pgn_slider, _text_color);

	// Analyse de Grogros
	string monte_carlo_text = static_cast<string>(_grogros_analysis ? "STOP GrogrosZero-Auto (CTRL-H)" : "RUN GrogrosZero-Auto (CTRL-G)") + "\nCONTROLS (H)" + "\n\nSEARCH PARAMETERS\nalpha: " + to_string(_alpha) + "\nbeta: " + to_string(_beta) + "\ngamma : " + to_string(_gamma) + "\nq_depth : " + to_string(_quiescence_depth) + "\nexplore checks : " + (_explore_checks ? "true" : "false");
	
	// S'il y a eu une recherche
	if (_root_exploration_node->children_count() && _drawing_arrows) {

		// Meilleure évaluation
		//int best_eval = _root_exploration_node->_deep_evaluation._value;
		Move best_move = _root_exploration_node->get_best_score_move(_alpha, _beta);
		Evaluation best_evaluation = _root_exploration_node->_children[best_move]->_deep_evaluation;

		//bool all_moves_explored = _root_exploration_node->get_fully_explored_children_count() == _root_exploration_node->_board->_got_moves;
		bool all_moves_explored = _root_exploration_node->children_count() == _root_exploration_node->_board->_got_moves;

		if (!all_moves_explored && ((_board._player && _root_exploration_node->_static_evaluation > best_evaluation) || (!_board._player && _root_exploration_node->_static_evaluation < best_evaluation))) {
			best_evaluation = _root_exploration_node->_static_evaluation;
		}

		int best_eval = best_evaluation._value;

		string eval;
		int mate = _board.is_eval_mate(best_eval);
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
		//float win_chance = get_winning_chances_from_eval(best_eval, _board._player);
		//if (!_board._player)
		//	win_chance = 1 - win_chance;
		//string win_chances = "W/D/L: " + to_string(static_cast<int>(100 * win_chance)) + "/0/" + to_string(static_cast<int>(100 * (1 - win_chance))) + "\%";

		//2bk1r2/4b1Qp/8/1P6/3P4/2p5/1q2NPPP/R1K2B1R w - - 1 26

		_wdl = best_evaluation._wdl;

		// Pour l'évaluation statique
		if (!_board._displayed_components) {
			int evaluation = _board._evaluation;
			_board.evaluate(_grogros_eval, true, nullptr, true);
			_board._evaluation = evaluation;
		}
		
		int max_depth = _root_exploration_node->get_main_depth(_alpha, _beta);
		monte_carlo_text += "\n\nSTATIC EVAL\n" + _eval_components +
			"\nTime: " + clock_to_string(_root_exploration_node->_time_spent, true) +
			"\nDepth: " + to_string(max_depth) +
			"\nQdepth: " + (_root_exploration_node->_iterations == 0 ? to_string(_root_exploration_node->_quiescence_depth) : "N/A") +
			"\nEval: " + ((best_eval > 0) ? static_cast<string>("+") : (mate != 0 ? static_cast<string>("-") : static_cast<string>(""))) + eval +
			"\nConfidence: " + to_string(100 - (int)(100 * best_evaluation._uncertainty)) + "%" +
			"\nWinnable: " + to_string(static_cast<int>(best_evaluation._winnable_white * 100)) + "% / " + to_string(static_cast<int>(best_evaluation._winnable_black * 100)) + "%" +
			"\n" + _wdl.to_string() + "\nScore: " + score_string(best_evaluation._avg_score) +
			"\nNodes: " + int_to_round_string(_root_exploration_node->_nodes) + "/" + int_to_round_string(monte_buffer._length) + " (" + int_to_round_string(_root_exploration_node->_nodes / (static_cast<float>(_root_exploration_node->_time_spent + 1) / CLOCKS_PER_SEC)) + "N/s)" +
			"\nIterations: " + int_to_round_string(_root_exploration_node->_iterations) + " (" + int_to_round_string(_root_exploration_node->_iterations / (static_cast<float>(_root_exploration_node->_time_spent + 1) / CLOCKS_PER_SEC)) + "I/s)";
		
		// Affichage des paramètres d'analyse de GrogrosZero
		slider_text(monte_carlo_text, _board_padding_x + _board_size + _text_size / 2, _text_size, _screen_width - _text_size - _board_padding_x - _board_size, _board_size * 9 / 16, _text_size / 4, &_monte_carlo_slider, _text_color);

		// Lignes d'analyse de GrogrosZero
		// TODO: on devrait utiliser ça aussi pour éviter de recalculer les autres paramètres
		if (_update_variants) {
			_exploration_variants = _root_exploration_node->get_exploration_variants(_alpha, _beta);
			_update_variants = false;
		}

		// Affichage des variantes
		slider_text(_exploration_variants, _board_padding_x + _board_size + _text_size / 2, _board_padding_y + _board_size * 9 / 16, _screen_width - _text_size - _board_padding_x - _board_size, _board_size / 2, _text_size / 3, &_variants_slider, _text_color);

		// Affichage de la barre d'évaluation
		draw_eval_bar(_global_eval, _wdl, _global_eval_text, _board_padding_x / 6, _board_padding_y, 2 * _board_padding_x / 3, _board_size, 800, _eval_bar_color_light, _eval_bar_color_gray, _eval_bar_color_dark);
	}

	// Affichage des contrôles et autres informations
	else {
		// Touches
		static string keys_information = "CTRL-G: Start GrogrosZero analysis\nCTRL-H: Stop GrogrosZero analysis\n\n";

		// Binding chess.com
		static string binding_information;
		binding_information = "Binding chess.com:\n- Auto-click: " + (_click_bind ? static_cast<string>("enabled") : static_cast<string>("disabled")) + "\n- Binding mode: " + (_binding_full ? static_cast<string>("analysis") : _binding_solo ? static_cast<string>("play") : "none");

		// Texte total
		static string controls_information;
		controls_information = "Controls:\n\n" + keys_information + binding_information;

		// TODO : ajout d'une valeur de slider
		slider_text(controls_information, _board_padding_x + _board_size + _text_size / 2, _board_padding_y, _screen_width - _text_size - _board_padding_x - _board_size, _board_size, _text_size / 3, &_variants_slider, _text_color_info);
	}

	// Affichage du curseur
	draw_texture(_cursor_texture, _mouse_pos.x - _cursor_size / 2, _mouse_pos.y - _cursor_size / 2, WHITE);
}

// Fonction qui charge une position à partir d'une FEN
void GUI::load_FEN(const string fen, bool display) {
	// TODO: il faut vériifer que la FEN est valide
	_board.from_fen(fen);
	update_global_pgn();
	_root_exploration_node->reset();
	_root_exploration_node->_board = &_board;
	_update_variants = true;
	monte_buffer.reset();

	if (display)
		cout << "loaded FEN : " << fen << endl;
}

// Fonction qui reset la partie
void GUI::reset_game() {
	cout << "*** RESETING GAME ***\n" << endl;

	cout << _global_pgn << endl;
	cout << _pgn << endl;
	_board.reset_board();
	_board.restart();
	_game_tree.reset();
	reset_pgn();
	_root_exploration_node->reset();
	_root_exploration_node->_board = &_board;
	_update_variants = true;
	monte_buffer.reset();

	PlaySound(_game_begin_sound);

	cout << "\n*** GAME RESET DONE ***" << endl;
}

// Fonction qui compare deux flèches d'analyse de Grogros
bool GUI::compare_arrows(const Move m1, const Move m2) const {

	// Si deux flèches finissent en un même point, affiche en dernier (au dessus), le "meilleur" coup
	if (m1.end_row == m2.end_row && m1.end_col == m2.end_col)
		return _root_exploration_node->_children[m1]->_nodes < _root_exploration_node->_children[m2]->_nodes;

	// Si les deux flèches partent d'un même point, alors affiche par dessus la flèche la plus courte
	if (m1.start_row == m2.start_row && m1.start_col == m2.start_col) {
		const int d1 = (m1.start_row - m1.end_row) * (m1.start_row - m1.end_row) + (m1.start_col - m1.end_col) * (m1.start_col - m1.end_col);
		const int d2 = (m2.start_row - m2.end_row) * (m2.start_row - m2.end_row) + (m2.start_col - m2.end_col) * (m2.start_col - m2.end_col);

		return d1 > d2;
	}

	return true;
}

// Fonction qui renvoie la date sous le format 'yyyymmdd'
string GUI::get_date() {
	const time_t current_time = time(nullptr);
	tm local_time;
	localtime_s(&local_time, &current_time);

	const int year = local_time.tm_year + 1900;
	const int month = local_time.tm_mon + 1;
	const int day = local_time.tm_mday;

	return to_string(year) + (month < 10 ? "0" : "") + to_string(month) + (day < 10 ? "0" : "") + to_string(day);
}

// Fonction qui met à jour le nom de bot de GrogrosZero
void GUI::update_grogros_zero_name() {
	//_grogros_zero_name = "Gr0_" + get_date();
	_grogros_zero_name = "Gr0-" + _grogros_zero_version;
}

// Fonction qui fait jouer le coup de GrogrosZero ou non en fonction du temps restant
void GUI::play_grogros_zero_move(float time_proportion_per_move) {

	// Positions bug:
	// rnbq1rk1/pp1p1ppp/7n/2p1P3/3p4/3B1N1P/PPPN1PP1/R2Q1RK1 w - - 0 10 : il joue pas Ce4

	// TOOD: faire en fonction du nombre de noeuds de RECHERCHE, pour prendre plus de temps dans les positions complexes

	// Si le buffer est complet, joue le coup de GrogrosZero
	// TODO

	// Si le temps n'est pas compté
	if (!_time) {
		return;
	}

	// S'il n'y a pas encore eu d'exploration
	if (_root_exploration_node->_iterations <= 1) {
		return;
	}

	// Pour les calculs d'évaluation
	int color = _board.get_color();

	// Noeud le plus exploré
	//Node const *most_explored_child = _root_exploration_node->get_most_explored_child();

	// Noeud avec la meilleure évaluation
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

	Node const* most_explored_child = _root_exploration_node->_children[most_explored_move];

	robin_map<Move, double> move_scores = _root_exploration_node->get_move_scores(_alpha, _beta);

	double most_explored_score = -DBL_MAX;
	double best_score = -DBL_MAX;
	Move best_move;

	// Meilleur coup
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

	// FIXME *** revoir la notion de best move ici

	//cout << "best eval : " << best_eval_colored << ", color : " << color << ", best move : " << _board.move_label(best_move) << endl;

	// Pourcentage de réflexion sur le meilleur coup
	float best_move_percentage = static_cast<float>(most_explored_child->_chosen_iterations) / static_cast<float>(_root_exploration_node->_iterations);

	// Temps idéal qu'il faut prendre sur ce coup
	//int max_move_time = _board._player ? time_to_play_move(_time_white, _time_black, time_proportion_per_move * (1.0f - best_move_percentage)) : time_to_play_move(_time_black, _time_white, time_proportion_per_move * (1.0f - best_move_percentage));
	double max_move_time = _board._player ? time_to_play_move(_time_white, _time_black, time_proportion_per_move) : time_to_play_move(_time_black, _time_white, time_proportion_per_move);

	//cout << "best move percentage : " << best_move_percentage << " | max move time : " << max_move_time << " | supposed speed : " << grogros_nps << " | nodes : " << _root_exploration_node->_nodes << " | time spent : " << _root_exploration_node->_time_spent << endl;	

	// Si il nous reste beaucoup de temps en fin de partie, on peut réfléchir plus longtemps
	// FIXME: Regarder si ça marche bien (TODO)
	max_move_time *= 1.0f + _board._adv;

	//cout << "max move time : " << max_move_time << endl;

	//int most_explored_child_eval = most_explored_child->_deep_evaluation._value * color;

	// On veut être sûr de jouer le meilleur coup de Grogros: s'il y a un meilleur coup que celui avec le plus de noeuds, attendre...
	bool wait_for_best_move = !most_explored_move_is_best;

	// FIXME: des choses à améliorer ici!
	// A quel point faut-il attendre pour être sûr de jouer le meilleur coup?
	//float eval_diff;
	//float move_wait_factor;

	//// Si on attend pour le meilleur coup, attend plus longtemps si la différence d'évaluation est grande entre le meilleur coup et le coup actuel
	//if (wait_for_best_move) {
	//	eval_diff = abs(most_explored_child->_deep_evaluation._value - _board._evaluation) / 100.0f;
	//	move_wait_factor = 2 + eval_diff;
	//	//cout << "eval diff : " << eval_diff << " | move wait factor : " << move_wait_factor << endl;
	//}

	//// Sinon, joue plus vite si la différence d'évaluation est grande entre le meilleur coup et le second meilleur coup??
	//else {
	//	//move_wait_factor = 1 / (1 + eval_diff);
	//	move_wait_factor = 1;
	//}

	// Combien de temps devrait-on attendre?
	// On met une valeur max pour éviter les overflow
	// FIXME: la différence d'éval devrait être relative?

	// Peut-on se permettre d'attendre? Dépend du temps restant (1 minute = limite...)
	//int time_left = _board._player ? _time_white : _time_black;

	//float move_wait_factor = min(100.0f, 1.0f + abs(most_explored_child_eval - best_eval_colored) / 50.0f * time_left / 60000.0f);
	float move_wait_factor = 1.0f + ((best_score + 1E-6) / (most_explored_score + 1E-6) - 1.0f) * 5.0f;

	//cout << "move wait factor : " << move_wait_factor << endl;

	// 4r1k1/2Q2ppp/3p4/2p5/2B1P3/2P1q2P/PPP3P1/1K4R1 w - - 1 25


	//float move_wait_factor = wait_for_best_move ? wait_factor : 1.0f; // C'est beaucoup mais bon, il faut trouver un truc pour améliorer ça

	//cout << "base time : " << max_move_time << " | wait for best move : " << wait_for_best_move << " | eval diff : " << eval_diff << " | move wait factor : " << move_wait_factor << " | final time : " << max_move_time * move_wait_factor << endl;
	
	//cout << "max move time : " << max_move_time << endl;

	max_move_time = max_move_time * move_wait_factor;

	//cout << "new max move time : " << max_move_time << endl;

	// Réduit le temps passé sur le coup si on est sûr que c'est le bon coup
	if (most_explored_move_is_best) {
		max_move_time *= 1.0f - best_move_percentage;

		//cout << "is best move, new max move time : " << max_move_time << endl;
	}

	// Parfois on a un overflow
	if (max_move_time < 0) {
		cout << "overflow in max move time" << endl;
		cout << "max move time : " << max_move_time << endl;
		max_move_time = DBL_MAX;
	}

	// Nombre d'itérations supposées par seconde
	//constexpr int supposed_ips = 1000;
	//const int supposed_ips = max(750, _root_exploration_node->get_ips());

	constexpr int average_nps = 2500; // Pour une position semi-complexe
	constexpr float consistent_factor = 0.5f; // Plus ce facteur est grand, plus le temps utilisé sera constant, quelle que soit la complexité de la position

	const int actual_ips = _root_exploration_node->get_ips();

	const int supposed_ips = average_nps + (actual_ips - average_nps) * consistent_factor;



	// Nombre de noeuds que Grogros doit calculer (en fonction des contraintes de temps)
	//int grogros_nps = _root_exploration_node->get_avg_nps();

	// Equivalent en nombre de noeuds
	double seconds_to_play = max_move_time / 1000.0;
	//int nodes_to_play = grogros_nps * seconds_to_play;
	double iterations_to_play = supposed_ips * seconds_to_play;

	// Overflow (FIXME: faut mieux gérer ça...)
	//if (nodes_to_play < 0) {
	//	cout << "RE: overflow in max move time (nodes to play)" << endl;
	//	nodes_to_play = 0;
	//}

	//cout << "nodes to play : " << nodes_to_play << ", " << _root_exploration_node->_nodes << endl;

	if (_root_exploration_node->_iterations >= iterations_to_play) {
	//if (_root_exploration_node->_nodes >= nodes_to_play) {

		//cout << "best eval" << best_eval_colored << endl;
		//for (auto const& child : _root_exploration_node->_children) {
		//	cout << "move : " << _board.move_label(child.first) << " | eval : " << child.second->_deep_evaluation._value << " | nodes : " << child.second->_nodes << " | iterations : " << child.second->_iterations << endl;
		//}

		if (wait_for_best_move) {
			cout << "Position: " << _board.to_fen() << " : played the sub-optimal " << _board._moves_count << ". " << _board.move_label(_root_exploration_node->get_most_explored_child_move()) << " because it was taking too long to wait for it... best move was probably:" << _board.move_label(best_move) << endl;
			cout << "Scores: " << most_explored_score << " | " << best_score << " -> wait factor: " << move_wait_factor << endl;
		}

		//cout << nodes_to_play << ", max move time : " << max_move_time << ", supposed speed : " << supposed_grogros_speed << ", nodes : " << _root_exploration_node->_nodes << endl;
		((_click_bind && _board.click_m_move(_root_exploration_node->get_most_explored_child_move(), get_board_orientation())) || true) && play_move_keep(_root_exploration_node->get_most_explored_child_move());
	}

	return;
}

// Fonction qui initialise les couleurs des sites de jeux d'échecs
void GUI::init_chess_sites() {

	// Chess.com (setup avec cases vertes et pièces de base)
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

	// Lichess.org (plateau vert et blanc, avec brightness réduite)
	// Piece set "alpha"
	ChessSite lichess_org;
	lichess_org._name = "lichess.org";
	lichess_org._white_tile_color = SimpleColor(191, 191, 166);
	lichess_org._black_tile_color = SimpleColor(100, 124, 76);
	lichess_org._white_piece_color = SimpleColor(185, 185, 185);
	lichess_org._black_piece_color = SimpleColor(12, 12, 12);
	lichess_org._white_tile_played_color = SimpleColor(112, 160, 159);
	lichess_org._black_tile_played_color = SimpleColor(59, 120, 106);
	lichess_org._piece_location_on_tile = { 0.17f, 0.66f };
	lichess_org._tile_location_on_tile = { 0.90f, 0.90f };
	lichess_org._time_lost_per_move = 100;
	lichess_org._tile_color_tolerance = 0.02f;
	lichess_org._piece_color_tolerance = 0.05f;

	_chess_sites.push_back(lichess_org);

	// Internet Chess Club (setup avec cases marrons et pièces 'Default')
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


// Fonction qui update le binding move à partir du plateau en ligne. Renvoie vrai si le coup a été modifié et s'il est valide
bool GUI::update_binding_move() {

	//cout << "updating binding move" << endl;

	// Récupération des coordonnées du coup
	uint_fast8_t *move_coords = get_board_move(_binding_left, _binding_top, _binding_right, _binding_bottom, _current_site, get_board_orientation());

	if (move_coords == nullptr) {
		//cout << "no move coords found" << endl;
		return false;
	}

	//cout << "binding move: " << (int)move_coords[0] << ", " << (int)move_coords[1] << ", " << (int)move_coords[2] << ", " << (int)move_coords[3] << endl;

	// Adaptation du coup selon les sites:
	// Sur lichess, pour roquer, les cases surlignées sont le roi et la tour (et non la case de destination du roi)
	uint_fast8_t start_row = move_coords[0];
	uint_fast8_t start_col = move_coords[1];
	uint_fast8_t end_row = move_coords[2];
	uint_fast8_t end_col = move_coords[3];

	// Pour les roques sur certains sites, on ne peut pas determiner la case de départ et d'arrivée, ça peut donc être inversé
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

	// Si le coup est existant
	for (int i = 0; i < _root_exploration_node->_board->_got_moves; i++) {
		if (_root_exploration_node->_board->_moves[i] == move) {
			_binding_move = move;
			return true;
		}
	}

	return false;
}