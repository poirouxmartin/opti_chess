#include "board.h"
#include "time_tests.h"
#include "useful_functions.h"
#include "gui.h"
#include "windows_tests.h"
#include "buffer.h"
#include "match.h"
#include "zobrist.h"
#include <io.h>
#include <fcntl.h>


// Fonction de test
inline void launch_eval() {
	main_GUI._board.reset_board();
	main_GUI._board.is_game_over();
	main_GUI._board.evaluate(main_GUI._grogros_eval);
	/*main_GUI._board._quick_sorted_moves = false;
	main_GUI._board.quick_moves_sort();*/
	//main_GUI._board.is_controlled(3, 3);
	//main_GUI._board.attacked(3, 3);
}



// Fonction qui fait le dessin de la GUI
inline void gui_draw() {
	if (!main_GUI._draw)
		return;
	BeginDrawing();
	//main_GUI._board.draw();
	main_GUI.draw();
	EndDrawing();
}

// Main
inline int main_ui() {
	// Faire une fonction Init pour raylib?

	// Fenêtre resizable
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	SetConfigFlags(FLAG_MSAA_4X_HINT);
	SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
	SetConfigFlags(FLAG_VSYNC_HINT);

	// Pour ne pas afficher toutes les infos (on peut mettre le log level de 0 à 7 -> 7 = rien)
	SetTraceLogLevel(LOG_ALL);

	// Initialisation de la fenêtre
	InitWindow(main_GUI._screen_width, main_GUI._screen_height, "Grogros Chess");

	// Initialisation de l'audio
	InitAudioDevice();
	SetMasterVolume(1.0f);

	// Nombre d'images par secondes
	SetTargetFPS(main_GUI._fps);

	// Espace entre les lignes de texte
	SetTextLineSpacing(4);

	// Curseur
	HideCursor();
	//SetMouseCursor(3);

	// Evaluateur de position
	Evaluator eval_white(1.0f);
	Evaluator eval_black;

	// Nombre de noeuds max pour le jeu automatique de GrogrosZero
	int grogros_nodes = 3000000;

	// Nombre de noeuds calculés par frame
	// Si c'est sur son tour
	int nodes_per_frame = 100;

	// Sur le tour de l'autre (pour que ça plante moins)
	int nodes_per_user_frame = 25;



	//monte_evaluator = eval_white;

	// Paramètres pour l'IA
	int search_depth = 6;

	// Fin de partie
	bool main_game_over = false;

	// Met les timers en place
	main_GUI._board.reset_timers();

	// Met le PGN à la date du jour
	main_GUI.update_date();

	// Met à jour le nom de bot de GrogrosZero
	main_GUI.update_grogros_zero_name();

	//printAttributeSizes(main_GUI._board);
	//testFunc(main_GUI._board);

	// Taille du buffer de Monte-Carlo
	constexpr int buffer_size = 5000000;

	// Taille de la table de transposition
	constexpr int transposition_table_size = 5000000;

	// Initialisation de la table de transposition
	transposition_table.init(transposition_table_size, nullptr, true);

	// Initialisation du buffer de Monte-Carlo
	monte_buffer.init(buffer_size);

	// Noeud d'exploration
	// Plateau test: rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq - 0 2
	Board test_board;
	//test_board.from_fen("rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq - 0 2");
	main_GUI._root_exploration_node = new Node(&test_board); // FIXME: à faire autre part (dans la GUI?)


	// Test de réseaux de neurones
	Network eval_network;
	eval_network.generate_random_weights();

	// Génération des sites web
	main_GUI.init_chess_sites();



	// Boucle principale (Quitter à l'aide de la croix, ou en faisant échap)
	while (!WindowShouldClose()) {
		// INPUTS

		// Full screen
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_F11)) {
			if (!IsWindowMaximized())
				SetWindowState(FLAG_WINDOW_MAXIMIZED);
			else
				ClearWindowState(FLAG_WINDOW_MAXIMIZED);
			if (!IsWindowFullscreen())
				SetWindowState(FLAG_FULLSCREEN_MODE);
			else
				ClearWindowState(FLAG_FULLSCREEN_MODE);
		}

		// T - Tests
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_T)) {
			//main_GUI._board.validate_nodes_count_at_depth("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, { 1, 20, 400, 8902, 197281, 4865609, 119060324 }, true);
			//main_GUI._board.validate_nodes_count_at_depth("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5, { 1, 48, 2039, 97862, 4085603, 193690690 }, true);
			//main_GUI._board.validate_nodes_count_at_depth("", 5, { }, true);

			main_GUI.grogros_analysis(1);

			//main_GUI._root_exploration_node->quiescence(&monte_buffer, main_GUI._grogros_eval, 6);
			//main_GUI._root_exploration_node->quiescence(&monte_buffer, main_GUI._grogros_eval, 6, -2147483647, 2147483647, nullptr, true, -1000);

			//main_GUI._root_exploration_node->_board->switch_colors();
			//main_GUI._root_exploration_node->_board->get_king_squares_distance(true).print();
			//main_GUI._root_exploration_node->_board->get_king_squares_distance(false).print();
		}

		// CTRL-T - Cherche le plateau du site d'échecs sur l'écran, et lance une partie
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_T)) {
			for (auto& site : main_GUI._chess_sites) {
				bool located_board = locate_chessboard(main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom, site);
				if (located_board) {
					main_GUI._current_site = site;
					main_GUI.new_bind_game();
					break;
				}
			}
		}

		// LCTRL-A - Binding full
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) {
			for (auto& site : main_GUI._chess_sites) {
				bool located_board = locate_chessboard(main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom, site);
				if (located_board) {
					main_GUI._current_site = site;
					main_GUI._binding_full = !main_GUI._binding_full;
					break;
				}
			}
		}

		// LCTRL-Q - Mode de jeu automatique (binding chess.com) -> Check le binding seulement sur les coups de l'adversaire
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_A)) {
			main_GUI._binding_solo = !main_GUI._binding_solo;
			main_GUI._click_bind = !main_GUI._click_bind;
		}

		// Changements de la taille de la fenêtre
		if (IsWindowResized()) {
			main_GUI.get_window_size();
			//load_resources(); // Sinon ça devient flou
			main_GUI.resize_GUI();
		}

		// S - Save FEN dans data/text.txt
		if (IsKeyPressed(KEY_S)) {
			SaveFileText("data/test.txt", const_cast<char*>(main_GUI._current_fen.c_str()));
			cout << "saved FEN : " << main_GUI._current_fen << endl;
		}

		// L - Load FEN dans data/text.txt
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_L)) {
			string fen = LoadFileText("data/test.txt");
			main_GUI.load_FEN(fen);
		}

		// F - Retourne le plateau
		if (IsKeyPressed(KEY_F)) {
			main_GUI.switch_orientation();
		}

		// LCTRL-N - Recommencer une partie
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_N)) {
			main_GUI.reset_game();
		}

		// Utilisation du réseau de neurones
		// if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_N)) {
		//     use_neural_network = !use_neural_network;
		// }

		// C - Copie dans le clipboard du PGN
		if (IsKeyPressed(KEY_C)) {
			SetClipboardText(main_GUI._global_pgn.c_str());
			cout << "copied PGN : \n" << main_GUI._global_pgn << endl;
		}

		// X - Copie dans le clipboard du FEN
		if (IsKeyPressed(KEY_X)) {
			SetClipboardText(main_GUI._current_fen.c_str());
			cout << "copied FEN : " << main_GUI._current_fen << endl;
		}

		// V - Colle le FEN du clipboard (le charge)
		if (IsKeyPressed(KEY_V)) {
			string fen = GetClipboardText();
			main_GUI.load_FEN(fen);
		}

		// // Colle le PGN du clipboard (le charge)
		// if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) {
		//     string pgn = GetClipboardText();
		//     main_GUI._board.from_pgn(pgn);
		//     cout << "loaded PGN : " << pgn << endl;
		// }

		// A - Analyse de partie sur chess.com (A)
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) {
			SetClipboardText(main_GUI._global_pgn.c_str());
			cout << "copied PGN for analysis on chess.com : \n" << main_GUI._global_pgn << endl;
			OpenURL("https://www.chess.com/analysis");
		}

		// A - Analyse de partie sur chess.com en direct, par GrogrosZero
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) {
			main_GUI.new_bind_analysis();
		}

		// TAB - Screenshot
		if (IsKeyPressed(KEY_TAB)) {
			string screenshot_name = "resources/screenshots/" + to_string(time(nullptr)) + ".png";
			cout << "Screenshot : " << screenshot_name << endl;
			TakeScreenshot(screenshot_name.c_str());

			// Mettre le screenshot dans le presse-papier?
		}

		// D - Dessine ou non
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_D)) {
			main_GUI._draw = false;
		}

		// B - Création du buffer
		if (IsKeyPressed(KEY_B)) {
			cout << "available memory : " << long_int_to_round_string(get_total_system_memory()) << "b" << endl;
			monte_buffer.init(buffer_size);
		}

		// G - GrogrosZero
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_G)) {
			if (!monte_buffer._init)
				monte_buffer.init(buffer_size);
			// LSHIFT - Utilisation du réseau de neurones
			if (IsKeyDown(KEY_LEFT_SHIFT))
				//main_GUI._board.grogros_zero(nullptr, nodes_per_frame, main_GUI._beta, main_GUI._k_add, false, 0, &grogros_network);
				false;
			else
				main_GUI.grogros_analysis();
		}

		// LCTRL-G - Lancement de GrogrosZero en recherche automatique
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_G)) {
			if (!monte_buffer._init)
				monte_buffer.init(buffer_size);
			main_GUI._grogros_analysis = true;
		}

		// Espace - GrogrosZero 1 noeud : DEBUG
		if (IsKeyPressed(KEY_SPACE)) {
			/*if (!monte_buffer._init)
				monte_buffer.init(buffer_size);
			main_GUI._board.grogros_zero(&monte_evaluator, 1, true, main_GUI._beta, main_GUI._k_add);*/
		}

		// LCTRL-H - Arrêt de la recherche automatique de GrogrosZero
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_H)) {
			main_GUI._grogros_analysis = false;
		}

		// H - Déffichage/Affichage des flèches, Affichage/Désaffichage des contrôles
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_H)) {
			main_GUI.switch_arrow_drawing();
		}

		// R - Réinitialisation des timers
		if (IsKeyPressed(KEY_R)) {
			main_GUI._board.reset_timers();
			main_GUI._time = false;
		}

		// Suppr. - Supprime les reflexions de GrogrosZero
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_DELETE)) {
			//main_GUI._board.reset_all(true, true);
			main_GUI._root_exploration_node->reset(); // FIXME... ça fait rien??
			//main_GUI._root_exploration_node = new Node(&main_GUI._board, Move());
		}

		// CTRL - Suppr. - Supprime le buffer de Monte-Carlo
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_DELETE)) {
			//main_GUI._board.reset_all(true, true);
			main_GUI._root_exploration_node->reset();
			monte_buffer.remove();
		}

		// D - Affichage dans la console de tous les coups légaux de la position
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_D)) {
			main_GUI._draw = true;
			//main_GUI._board.display_moves(true);
			cout << main_GUI._board._positions_history.size() << endl;
		}

		// E - Évalue la position et renvoie les composantes dans la console
		if (IsKeyPressed(KEY_E)) {
			main_GUI._board.evaluate(main_GUI._grogros_eval, true);
			cout << "Evaluation : \n" << main_GUI._eval_components << endl;
		}

		// CTRL - L - Promeut la variation en actuelle en tant que variation principale
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_L)) {
			//cout << main_GUI._game_tree._current_node->_move_label << endl;
			main_GUI._game_tree.promote_current_variation();
			//cout << main_GUI._game_tree._current_node->_move_label << endl;
			main_GUI._pgn = main_GUI._game_tree.tree_display();
		}

		// Keypads - met le temps (en minutes)

		// 1 - 1 minute
		if (IsKeyPressed(KEY_KP_1)) {
			main_GUI._time_black = 60000;
			main_GUI._time_white = 60000;
		}

		// 2 - 2 minutes
		if (IsKeyPressed(KEY_KP_2)) {
			main_GUI._time_black = 120000;
			main_GUI._time_white = 120000;
		}

		// 3 - 3 minutes
		if (IsKeyPressed(KEY_KP_3)) {
			main_GUI._time_black = 180000;
			main_GUI._time_white = 180000;
		}

		// 4 - 4 minutes
		if (IsKeyPressed(KEY_KP_4)) {
			main_GUI._time_black = 240000;
			main_GUI._time_white = 240000;
		}

		// 5 - 5 minutes
		if (IsKeyPressed(KEY_KP_5)) {
			main_GUI._time_black = 300000;
			main_GUI._time_white = 300000;
		}

		// 6 - 6 minutes
		if (IsKeyPressed(KEY_KP_6)) {
			main_GUI._time_black = 360000;
			main_GUI._time_white = 360000;
		}

		// 7 - 7 minutes
		if (IsKeyPressed(KEY_KP_7)) {
			main_GUI._time_black = 420000;
			main_GUI._time_white = 420000;
		}

		// 8 - 8 minutes
		if (IsKeyPressed(KEY_KP_8)) {
			main_GUI._time_black = 480000;
			main_GUI._time_white = 480000;
		}

		// 9 - 9 minutes
		if (IsKeyPressed(KEY_KP_9)) {
			main_GUI._time_black = 540000;
			main_GUI._time_white = 540000;
		}

		// P - Joue le coup recommandé par l'algorithme de GrogrosZero
		if (!IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_P)) {
			/*if (main_GUI._board._tested_moves > 0)
				((main_GUI._click_bind && main_GUI._board.click_m_move(main_GUI._board._moves[main_GUI._board.best_monte_carlo_move()], main_GUI.get_board_orientation())) || true) && main_GUI._board.play_monte_carlo_move_keep(main_GUI._board._moves[main_GUI._board.best_monte_carlo_move()], true, true, false);
			else
				cout << "no more moves are in memory" << endl;*/
			if (main_GUI._root_exploration_node->children_count() > 0)
				((main_GUI._click_bind && main_GUI._board.click_m_move(main_GUI._root_exploration_node->get_best_move(), main_GUI.get_board_orientation())) || true) && main_GUI.play_move_keep(main_GUI._root_exploration_node->get_best_move());
			else
				cout << "no more moves are in memory" << endl;
		}

		// LShift-P - Joue les coups recommandés par l'algorithme de GrogrosZero
		if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyDown(KEY_P)) {
			/*if (main_GUI._board._tested_moves > 0)
				((main_GUI._click_bind && main_GUI._board.click_m_move(main_GUI._board._moves[main_GUI._board.best_monte_carlo_move()], main_GUI.get_board_orientation())) || true) && main_GUI._board.play_monte_carlo_move_keep(main_GUI._board._moves[main_GUI._board.best_monte_carlo_move()], true, true, false);
			else
				cout << "no more moves are in memory" << endl;*/
			if (main_GUI._root_exploration_node->children_count() > 0)
				((main_GUI._click_bind && main_GUI._board.click_m_move(main_GUI._root_exploration_node->get_best_move(), main_GUI.get_board_orientation())) || true) && main_GUI.play_move_keep(main_GUI._root_exploration_node->get_best_move());
			else
				cout << "no more moves are in memory" << endl;
		}

		// Return - Lancement et arrêt du temps
		if (IsKeyPressed(KEY_SPACE)) {
			if (main_GUI._time)
				main_GUI.stop_time();
			else
				main_GUI.start_time();
		}

		// Z - Clé de Zobrist de la position actuelle
		if (IsKeyPressed(KEY_W)) {
			cout << "Zobrist key : " << main_GUI._board._zobrist_key << endl;
			cout << main_game_over << endl;
		}

		// UP/DOWN - Activation, désactivation de GrogrosFish pour les pièces blanches
		if (!IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && main_GUI.get_board_orientation()) || (IsKeyPressed(KEY_UP) && !main_GUI.get_board_orientation()))) {
			if (main_GUI._white_player.substr(0, 11) == "GrogrosFish")
			{
				main_GUI._white_player = "White";
				main_GUI._white_title = "";
				main_GUI._white_elo = "?";
				main_GUI._white_url = "";
				main_GUI._white_country = "";
			}

			else
			{
				main_GUI._white_player = "GrogrosFish (depth " + to_string(search_depth) + ")";
				main_GUI._white_title = "BOT";
				main_GUI._white_elo = "?";
				main_GUI._white_url = "https://images.chesscomfiles.com/uploads/v1/user/284728633.4af59e2f.50x50o.0c8cdf830b69.png";
				main_GUI._white_country = "57";
			}
		}

		// UP/DOWN - Activation, désactivation de GrogrosFish pour les pièces noires
		if (!IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && !main_GUI.get_board_orientation()) || (IsKeyPressed(KEY_UP) && main_GUI.get_board_orientation()))) {
			if (main_GUI._black_player.substr(0, 11) == "GrogrosFish")
			{
				main_GUI._black_player = "Black";
				main_GUI._black_title = "";
				main_GUI._black_elo = "?";
				main_GUI._black_url = "";
				main_GUI._black_country = "";
			}

			else
			{
				main_GUI._black_player = "GrogrosFish (depth " + to_string(search_depth) + ")";
				main_GUI._black_title = "BOT";
				main_GUI._black_elo = "?";
				main_GUI._black_url = "https://images.chesscomfiles.com/uploads/v1/user/284728633.4af59e2f.50x50o.0c8cdf830b69.png";
				main_GUI._black_country = "57";
			}
		}

		// CTRL-UP/DOWN - Activation, désactivation de GrogrosZero pour les pièces blanches
		if (IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && main_GUI.get_board_orientation()) || (IsKeyPressed(KEY_UP) && !main_GUI.get_board_orientation()))) {
			if (main_GUI._white_player == main_GUI._grogros_zero_name)
			{
				main_GUI._white_player = "White";
				main_GUI._white_title = "";
				main_GUI._white_elo = "?";
				main_GUI._white_url = "";
				main_GUI._white_country = "";
			}

			else
			{
				main_GUI._white_player = main_GUI._grogros_zero_name;
				main_GUI._white_title = "BOT";
				main_GUI._white_elo = main_GUI._grogros_zero_elo;
				main_GUI._white_url = "https://images.chesscomfiles.com/uploads/v1/user/284728633.4af59e2f.50x50o.0c8cdf830b69.png";
				main_GUI._white_country = "57";
			}
		}

		// CTRL-UP/DOWN - Activation, désactivation de GrogrosZero pour les pièces noires
		if (IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && !main_GUI.get_board_orientation()) || (IsKeyPressed(KEY_UP) && main_GUI.get_board_orientation()))) {
			if (main_GUI._black_player == main_GUI._grogros_zero_name)
			{
				main_GUI._black_player = "Black";
				main_GUI._black_title = "";
				main_GUI._black_elo = "?";
				main_GUI._black_url = "";
				main_GUI._black_country = "";
			}

			else
			{
				main_GUI._black_player = main_GUI._grogros_zero_name;
				main_GUI._black_title = "BOT";
				main_GUI._black_elo = main_GUI._grogros_zero_elo;
				main_GUI._black_url = "https://images.chesscomfiles.com/uploads/v1/user/284728633.4af59e2f.50x50o.0c8cdf830b69.png";
				main_GUI._black_country = "57";
			}
		}

		// Fin de partie (à reset aussi...) (le son ne se lance pas...)
		// Calculer la fin de la partie ici une fois, pour éviter de la refaire?

		// Plus de temps... (en faire une fonction)
		// if (main_GUI._board._time) {
		//     if (main_GUI._board._time_black < 0) {
		//         main_GUI._board._time = false;
		//         main_GUI._board._time_black = 0;
		//         play_end_sound();
		//         main_GUI._board._is_game_over = true;
		//         cout << "White won on time" << endl; // Pas toujours vrai car il peut y avoir des manques de matériel
		//     }
		//     if (main_GUI._board._time_white < 0) {
		//         main_GUI._board._time = false;
		//         main_GUI._board._time_white = 0;
		//         play_end_sound();
		//         main_GUI._board._is_game_over = true;
		//         cout << "Black won on time" << endl;
		//     }

		// }


		// Flèche gauche: revient sur la position précédente
		if (IsKeyPressed(KEY_LEFT)) {
			main_GUI._game_tree.select_previous_node();
		}

		// Flèche droite: avance sur la position suivante
		if (IsKeyPressed(KEY_RIGHT)) {
			main_GUI._game_tree.select_first_next_node();
		}



		// Jeu des IA

		// TODO: il faut améliorer tout ça, et faire passer des choses dans la GUI

		int new_game_over = 0;

		// Fait jouer l'IA automatiquement en fonction des paramètres
		if (main_GUI._board._game_over_checked && main_GUI._board._game_over_value != 0) {
			//cout << "Game seems to be over... or is it?" << endl;
			main_GUI._board._game_over_checked = false;
			new_game_over = main_GUI._board.is_game_over(3);
			main_GUI._root_exploration_node->_iterations = 0;
			main_GUI._root_exploration_node->_chosen_iterations = 0;
			//cout << "New game over value : " << new_game_over << endl;
			//cout << "exploration game over value : " << (int)main_GUI._root_exploration_node->_board->_game_over_value << endl;
		}

		//main_GUI._board._game_over_checked = false;  // On recheck, pour les threefold (car dans la réflexion on dit que la partie est finie si y'a une seule répétition)
		//if (main_GUI._board.is_game_over(3) == 0) {
		if (new_game_over == 0) {
			// GrogrosZero

			//cout << "test" << endl;

			// Analyse de GrogrosZero
			if (main_GUI._grogros_analysis || main_GUI._white_player == main_GUI._grogros_zero_name || main_GUI._black_player == main_GUI._grogros_zero_name) {
				if (!monte_buffer._init)
					monte_buffer.init(buffer_size);

				main_GUI.grogros_analysis();
			}

			// Quand c'est son tour (TODO: fonction pour ça)
			if ((main_GUI._board._player && main_GUI._white_player == main_GUI._grogros_zero_name) || (!main_GUI._board._player && main_GUI._black_player == main_GUI._grogros_zero_name)) {
				if (!monte_buffer._init)
					monte_buffer.init(buffer_size);

				main_GUI.play_grogros_zero_move();
			}

			if (main_GUI._board.is_game_over(3) != 0)
				goto game_over;

			// GrogrosFish (seulement lorsque c'est son tour)
			//if (main_GUI._board._player && main_GUI._white_player.substr(0, 11) == "GrogrosFish")
			//	main_GUI._board.grogrosfish(search_depth, &eval_white, true);

			//if (!main_GUI._board._player && main_GUI._black_player.substr(0, 11) == "GrogrosFish")
			//	main_GUI._board.grogrosfish(search_depth, &eval_black, true);
		}

		// Si la partie est terminée
		else {

		game_over:

			if (!main_game_over) {
				main_GUI._time = false;
				main_GUI._board.display_pgn();
				main_game_over = true;
			}
		}

		// Jeu automatique sur sites d'échecs
		if (main_GUI._binding_full || (main_GUI._binding_solo && main_GUI.get_board_orientation() != main_GUI._board._player)) {
			// Le fait à chaque intervalle de temps 'binding_interval_check'
			if (clock() - main_GUI._last_binding_check > main_GUI._binding_interval_check) {
				// Coup joué sur le plateau
				main_GUI._binding_move = get_board_move(main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom, main_GUI._current_site, main_GUI.get_board_orientation());

				// Vérifie que le coup est légal avant de le jouer
				for (int i = 0; i < main_GUI._board._got_moves; i++) {
					if (main_GUI._board._moves[i].i1 == main_GUI._binding_move[0] && main_GUI._board._moves[i].j1 == main_GUI._binding_move[1] && main_GUI._board._moves[i].i2 == main_GUI._binding_move[2] && main_GUI._board._moves[i].j2 == main_GUI._binding_move[3]) {
						//main_GUI._board.play_move_sound(Move(main_GUI._binding_move[0], main_GUI._binding_move[1], main_GUI._binding_move[2], main_GUI._binding_move[3]));
						main_GUI.play_move_keep(main_GUI._board._moves[i]);

						// Retire du temps en fonction du temps perdu par coup
						if (main_GUI._board._player)
							main_GUI._time_white -= main_GUI._current_site._time_lost_per_move;
						else
							main_GUI._time_black -= main_GUI._current_site._time_lost_per_move;

						break;
					}
				}

				main_GUI._last_binding_check = clock();
			}
		}

		gui_draw();
	}

	// Fermeture de la fenêtre
	CloseWindow();

	return 0;
}