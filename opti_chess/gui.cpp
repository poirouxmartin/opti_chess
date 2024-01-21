#include "gui.h"
#include "buffer.h"
#include "useful_functions.h"
#include "windows_tests.h"

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
	const int orientation = bind_board_orientation(main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom);

	if (orientation == -1)
		return false;

	_board.restart();
	main_GUI.reset_pgn();

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
	main_GUI.start_time();
	_grogros_analysis = false;

	return true;
}

// Fonction qui met en place le binding avec chess.com pour une nouvelle analyse de partie
bool GUI::new_bind_analysis() {
	const int orientation = bind_board_orientation(main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom);

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
void GUI::draw_monte_carlo_arrows() const
{
	main_GUI.grogros_arrows = {};

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
			if (_board._nodes_children[i] / static_cast<float>(sum_nodes) > arrow_rate)
				moves_vector.push_back(i);
		}
	}

	sort(moves_vector.begin(), moves_vector.end(), compare_move_arrows);

	for (const int i : moves_vector) {
		const int mate = _board.is_eval_mate(_board._eval_children[i]);
		draw_arrow_from_coord(_board._moves[i].i1, _board._moves[i].j1, _board._moves[i].i2, _board._moves[i].j2, i, _board.get_color(), -1.0, move_color(_board._nodes_children[i], sum_nodes), true, _board._eval_children[i], mate, i == best_move);
	}
}

// Fonction qui obtient la case correspondante à la position sur la GUI
Pos get_pos_from_GUI(const float x, const float y) {
	if (!is_in(x, board_padding_x, board_padding_x + board_size) || !is_in(y, board_padding_y, board_padding_y + board_size))
		return Pos(-1, -1);
	else
		return Pos(orientation_index(8 - (y - board_padding_y) / tile_size), orientation_index((x - board_padding_x) / tile_size));
}

// Fonction qui permet de changer l'orientation du plateau
void switch_orientation() {
	board_orientation = !board_orientation;
}

// Fonction aidant à l'affichage du plateau (renvoie i si board_orientation, et 7 - i sinon)
int orientation_index(const int i) {
	if (board_orientation)
		return i;
	return 7 - i;
}