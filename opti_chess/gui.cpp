#include "gui.h"
#include "buffer.h"
#include "useful_functions.h"
#include "windows_tests.h"

#include <sstream>

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
			_pgn += " {[%clk " + clock_to_string(_time_black, true) + "]}";
		}
		else {
			_time_white -= clock() - _last_move_clock - _time_increment_white;
			_pgn += " {[%clk " + clock_to_string(_time_white, true) + "]}";
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

// Fonction qui met en place le binding avec chess.com pour une nouvelle partie
bool GUI::new_bind_game() {
	const int orientation = bind_board_orientation(_binding_left, _binding_top, _binding_right, _binding_bottom);

	if (orientation == -1)
		return false;

	_board.restart();
	reset_pgn();

	if (get_board_orientation() != orientation)
		switch_orientation();

	if (orientation) {
		// Joueur blanc
		_white_player = "GrogrosZero";
		_white_title = "BOT";
		_white_elo = _grogros_zero_elo;
		_white_url = "https://images.chesscomfiles.com/uploads/v1/user/284728633.4af59e2f.50x50o.0c8cdf830b69.png";
		_white_country = "57";

		// Joueur noir
		_black_player = "chess.com player";
		_black_title = "";
		_black_elo = "";
		_black_url = "";
		_black_country = "";
	}
	else {
		// Joueur blanc
		_white_player = "chess.com player";
		_white_title = "";
		_white_elo = "";
		_white_url = "";
		_white_country = "";

		// Joueur noir
		_black_player = "GrogrosZero";
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
	const int orientation = bind_board_orientation(_binding_left, _binding_top, _binding_right, _binding_bottom);

	if (orientation == -1)
		return false;

	_board.restart();

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

	_date = std::to_string(year) + "." + std::to_string(month) + "." + std::to_string(day);

	return true;
}

// Fonction qui lance les threads de GrogrosZero
bool GUI::thread_grogros_zero(Evaluator* eval, int nodes)
{
	// Initialisation du buffer pour GrogrosZero, si besoin
	if (!monte_buffer._init)
		monte_buffer.init();


	// Lance grogros sur chaque noeud fils pour l'initialisation
	//_board.grogros_zero(eval, _board._got_moves, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr, 0);

	_threads_grogros_zero.clear();

	for (int i = 0; i < _board._got_moves; i++)
		_threads_grogros_zero.emplace_back(&Board::grogros_zero, &monte_buffer._heap_boards[_board._index_children[i]], eval, nodes, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr, 0);


	//_threads_grogros_zero.emplace_back(&Board::grogros_zero, &_board, eval, nodes, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr, 0);
	//_threads_grogros_zero.emplace_back(&Board::grogros_zero, &_board, eval, nodes, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr, 0);

	
	for (auto& thread : _threads_grogros_zero) {
		thread.join();
		//thread.detach();
		//cout << "Thread done" << endl;
	}

	// TODO:
	// Faut re-additionner les temps de monte carlo de chaque fils (pareil pour les quiescence nodes)
	// Il faut aussi update toutes les variantes

	// Relance grogros sur 1 noeud (pour actualiser les valeurs)
	//_board.grogros_zero(eval, 100, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr, 0);

	return true;
}

// Fonction qui lance grogros sur un thread
bool GUI::grogros_zero_threaded(Evaluator* eval, int nodes) {
	// Initialisation du buffer pour GrogrosZero, si besoin
	if (!monte_buffer._init)
		monte_buffer.init();

	// Lance grogros sur un thread
	_thread_grogros_zero = thread(&Board::grogros_zero, &_board, eval, nodes, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr, 0);

	_thread_grogros_zero.detach();

	return true;
}

// Fonction qui retire le dernier coup du PGN
bool GUI::remove_last_move_PGN()
{
	// TODO	

	return false;
}

// Fonction qui dessine les flèches en fonction des valeurs dans l'algo de Monte-Carlo
void GUI::draw_monte_carlo_arrows()
{
	_grogros_arrows = {};

	const int best_move = _board.best_monte_carlo_move();

	int sum_nodes = 0;
	for (int i = 0; i < _board._tested_moves; i++)
		sum_nodes += _board._nodes_children[i];

	// Une pièce est-elle sélectionnée?
	//cout << _selected_pos.i << " " << _selected_pos.j << endl;
	const bool is_selected = _selected_pos.i != -1 && _selected_pos.j != -1;

	// Crée un vecteur avec les coups visibles
	vector<int> moves_vector;
	for (int i = 0; i < _board._tested_moves; i++) {
		if (is_selected) {
			// Dessine pour la pièce sélectionnée
			if (_selected_pos.i == _board._moves[i].i1 && _selected_pos.j == _board._moves[i].j1)
				moves_vector.push_back(i);
		}

		else {
			if (_board._nodes_children[i] / static_cast<float>(sum_nodes) > _arrow_rate)
				moves_vector.push_back(i);
		}
	}

	sort(moves_vector.begin(), moves_vector.end(), compare_move_arrows);

	for (const int i : moves_vector) {
		const int mate = _board.is_eval_mate(_board._eval_children[i]);
		draw_arrow_from_coord(_board._moves[i].i1, _board._moves[i].j1, _board._moves[i].i2, _board._moves[i].j2, i, _board.get_color(), move_color(_board._nodes_children[i], sum_nodes), -1.0f, true, _board._eval_children[i], mate, i == best_move);
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
int GUI::orientation_index(const int i) {
	if (_board_orientation)
		return i;
	return 7 - i;
}

// A partir de coordonnées sur le plateau
void GUI::draw_arrow_from_coord(int i1, int j1, int i2, int j2, int index, const int player, Color c, float thickness, const bool use_value, int value, const int mate, const bool outline)
{
	if (thickness == -1.0f)
		thickness = _arrow_thickness;

	const float x1 = _board_padding_x + _tile_size * orientation_index(j1) + _tile_size / 2;
	const float y1 = _board_padding_y + _tile_size * orientation_index(7 - i1) + _tile_size / 2;
	const float x2 = _board_padding_x + _tile_size * orientation_index(j2) + _tile_size / 2;
	const float y2 = _board_padding_y + _tile_size * orientation_index(7 - i2) + _tile_size / 2;

	// Transparence nulle
	c.a = 255;

	// Outline pour le coup choisi
	if (outline) {
		if (abs(j2 - j1) != abs(i2 - i1))
			draw_line_bezier(x1, y1, x2, y2, thickness * 1.4f, BLACK);
		else
			draw_line_ex(x1, y1, x2, y2, thickness * 1.4f, BLACK);
		draw_circle(x1, y1, thickness * 1.2f, BLACK);
		draw_circle(x2, y2, thickness * 2.0f * 1.1f, BLACK);
	}

	// "Flèche"
	if (abs(j2 - j1) != abs(i2 - i1))
		draw_line_bezier(x1, y1, x2, y2, thickness, c);
	else
		draw_line_ex(x1, y1, x2, y2, thickness, c);
	draw_circle(x1, y1, thickness, c);
	draw_circle(x2, y2, thickness * 2.0f, c);

	if (use_value) {
		char v[4];
		string eval;
		if (mate != 0) {
			if (mate * player > 0)
				eval = "+";
			else
				eval = "-";
			eval += "M";
			eval += to_string(abs(mate));
#pragma warning(suppress : 4996)
			sprintf(v, eval.c_str());
		}
		else {
#pragma warning(suppress : 4996)
			if (_display_win_chances)
				value = float_to_int(100.0f * get_winning_chances_from_eval(value, _board._player));
			sprintf_s(v, "%d", value);
		}

		float size = thickness * 1.5f;
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
	_grogros_arrows.push_back(Move(i1, j1, i2, j2));

	return;
}

// Couleur de la flèche en fonction du coup (de son nombre de noeuds)
Color GUI::move_color(const int nodes, const int total_nodes) const {
	const float x = static_cast<float>(nodes) / static_cast<float>(total_nodes);

	const auto red = static_cast<unsigned char>(255.0f * ((x <= 0.2f) + (x > 0.2f && x < 0.4f) * (0.4f - x) / 0.2f + (x > 0.8f) * (x - 0.8f) / 0.2f));
	const auto green = static_cast<unsigned char>(255.0f * ((x < 0.2f) * x / 0.2f + (x >= 0.2f && x <= 0.6f) + (x > 0.6f && x < 0.8f) * (0.8f - x) / 0.2f));
	const auto blue = static_cast<unsigned char>(255.0f * ((x > 0.4f && x < 0.6f) * (x - 0.4f) / 0.2f + (x >= 0.6f)));

	//unsigned char alpha = 100 + 155 * nodes / total_nodes;

	return { red, green, blue, 255 };
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
	_move_1_sound = LoadSound("resources/sounds/move_1.mp3");
	_move_2_sound = LoadSound("resources/sounds/move_2.mp3");
	_castle_1_sound = LoadSound("resources/sounds/castle_1.mp3");
	_castle_2_sound = LoadSound("resources/sounds/castle_2.mp3");
	_check_1_sound = LoadSound("resources/sounds/check_1.mp3");
	_check_2_sound = LoadSound("resources/sounds/check_2.mp3");
	_capture_1_sound = LoadSound("resources/sounds/capture_1.mp3");
	_capture_2_sound = LoadSound("resources/sounds/capture_2.mp3");
	_checkmate_sound = LoadSound("resources/sounds/checkmate.mp3");
	_stealmate_sound = LoadSound("resources/sounds/stealmate.mp3");
	_game_begin_sound = LoadSound("resources/sounds/game_begin.mp3");
	_game_end_sound = LoadSound("resources/sounds/game_end.mp3");
	_promotion_sound = LoadSound("resources/sounds/promotion.mp3");

	// Police de l'écriture
	_text_font = LoadFontEx("resources/fonts/SF TransRobotics.ttf", 64, 0, 250);
	// text_font = GetFontDefault();

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
		_piece_textures[i] = LoadTextureFromImage(piece_image);
	}
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
bool GUI::is_playing() {
	const auto [x, y] = GetMousePosition();
	return (_selected_pos.i != -1 || x != _mouse_pos.x || y != _mouse_pos.y);
}

// Fonction qui change le mode d'affichage des flèches (oui/non)
void GUI::switch_arrow_drawing() {
	_drawing_arrows = !_drawing_arrows;
}

// Fonction qui affiche un texte dans une zone donnée avec un slider
void GUI::slider_text(const string& s, float pos_x, float pos_y, float width, float height, float size, float* slider_value, Color t_color, float slider_width, float slider_height) {
	Rectangle rect_text = { pos_x, pos_y, width, height };
	DrawRectangleRec(rect_text, _background_text_color);

	// Taille des sliders
	if (slider_width == -1.0f)
		slider_width = _screen_width / static_cast<float>(100);

	if (slider_height == -1.0f)
		slider_height = height;

	string new_string;
	float new_width = width - slider_width;

	// Pour chaque bout de texte
	std::stringstream ss(s);
	std::string line;

	while (getline(ss, line, '\n')) {
		// Taille horizontale du texte

		// Split le texte en parties égales
		if (float horizontal_text_size = MeasureTextEx(_text_font, line.c_str(), size, _font_spacing * size).x; horizontal_text_size > new_width) {
			int split_length = line.length() * new_width / horizontal_text_size;
			for (int i = split_length - 1; i < line.length() - 1; i += split_length)
				line.insert(i, "\n");
		}

		new_string += line + "\n";
	}

	// Taille verticale totale du texte

	// Si le texte prend plus de place verticalement que l'espace alloué
	if (float vertical_text_size = MeasureTextEx(_text_font, new_string.c_str(), size, _font_spacing * size).y; vertical_text_size > height) {
		int n_lines;

		// Nombre de lignes total
		int total_lines = 1;
		for (int i = 0; i < new_string.length() - 1; i++) {
			if (new_string.substr(i, 1) == "\n")
				total_lines++;
		}

		n_lines = total_lines * height / MeasureTextEx(_text_font, new_string.c_str(), size, _font_spacing * size).y;

		int starting_line = (total_lines - n_lines) * *slider_value;

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
		slider_height = height / sqrtf(total_lines - n_lines + 1);

		// Background
		Rectangle slider_background_rect = { pos_x + width - slider_width, pos_y, slider_width, height };
		DrawRectangleRec(slider_background_rect, _slider_background_color);

		// Slider
		Rectangle slider_rect = { pos_x + width - slider_width, pos_y + *slider_value * (height - slider_height), slider_width, slider_height };
		DrawRectangleRec(slider_rect, _slider_color);

		// Slide

		// Avec la molette
		if (is_cursor_in_rect({ pos_x, pos_y, width, height })) {
			*slider_value -= GetMouseWheelMove() * GetFrameTime() * 100 / (total_lines - n_lines);
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
bool GUI::get_board_orientation() {
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
void GUI::draw_eval_bar(const float eval, const string& text_eval, const float x, const float y, const float width, const float height, const float max_eval, const Color white, const Color black, float max_height) {
	const bool is_mate = text_eval.find('M') != -1;

	// Taille max de la barre
	if (max_height == -1.0f)
		max_height = 0.95f;

	// Coupe l'évaluation à 2 chiffres max
	// FIXME: ça suppose que l'eval dépasse pas +100
	string eval_text = is_mate ? text_eval : text_eval.substr(0, min(4, static_cast<int>(text_eval.size())));
	if (eval_text[eval_text.size() - 1] == '.')
		eval_text = eval_text.substr(0, eval_text.size() - 1);

	const float max_bar = is_mate ? 1 : max_height;
	const float switch_color = min(max_bar * height, max((1 - max_bar) * height, height / 2 - eval / max_eval * height / 2));
	const float static_eval_switch = min(max_bar * height, max((1 - max_bar) * height, height / 2 - _board._static_evaluation / max_eval * height / 2));
	const bool orientation = get_board_orientation();
	if (orientation) {
		draw_rectangle(x, y, width, height, black);
		draw_rectangle(x, y + switch_color, width, height - switch_color, white);
		draw_rectangle(x, y + static_eval_switch - 1.0f, width, 2.0f, RED);
	}
	else {
		draw_rectangle(x, y, width, height, black);
		draw_rectangle(x, y, width, height - switch_color, white);
		draw_rectangle(x, y + height - static_eval_switch - 1.0f, width, 2.0f, RED);
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

// Fonction qui joue le son de fin de partie
void GUI::play_end_sound() {
	PlaySound(_game_end_sound);
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
