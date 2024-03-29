#include "board.h"
#include "time_tests.h"
#include "useful_functions.h"
#include "gui.h"
#include "windows_tests.h"
#include "buffer.h"



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
	main_GUI._board.draw();
	EndDrawing();
}

// Main
inline int main_ui() {
	// Faire une fonction Init pour raylib?

	// Fen�tre resizable
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	SetConfigFlags(FLAG_MSAA_4X_HINT);
	SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
	SetConfigFlags(FLAG_VSYNC_HINT);

	// Pour ne pas afficher toutes les infos (on peut mettre le log level de 0 � 7 -> 7 = rien)
	SetTraceLogLevel(LOG_WARNING);

	// Initialisation de la fen�tre
	InitWindow(main_GUI._screen_width, main_GUI._screen_height, "Grogros Chess");

	// Initialisation de l'audio
	InitAudioDevice();
	SetMasterVolume(1.0f);

	// Nombre d'images par secondes
	SetTargetFPS(main_GUI._fps);

	// Curseur
	HideCursor();
	//SetMouseCursor(3);

	// Evaluateur de position
	Evaluator eval_white(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	Evaluator eval_black;

	// Nombre de noeuds max pour le jeu automatique de GrogrosZero
	int grogros_nodes = 3000000;

	// Nombre de noeuds calcul�s par frame
	// Si c'est sur son tour
	int nodes_per_frame = 100;

	// Sur le tour de l'autre (pour que �a plante moins)
	int nodes_per_user_frame = 25;



	//monte_evaluator = eval_white;

	// Param�tres pour l'IA
	int search_depth = 8;
	search_depth = 6;

	// Fin de partie
	bool main_game_over = false;

	//// R�seau de neurones
	//Network grogros_network;
	//grogros_network.generate_random_weights();
	//bool use_neural_network = false;

	//// Liste de r�seaux de neurones pour les tournois
	//int n_networks = 5;
	//Evaluator **evaluators = new Evaluator*[n_networks];
	//for (int i = 0; i < n_networks; i++)
	//    evaluators[i] = nullptr;
	//Network **neural_networks = new Network*[n_networks];
	//Network *neural_networks_test = new Network[n_networks];
	//for (int i = 0; i < n_networks; i++) {
	//    neural_networks[i] = &neural_networks_test[i];
	//    neural_networks[i]->generate_random_weights();
	//}

	//neural_networks[0] = nullptr;
	//evaluators[0] = &monte_evaluator;

	// Met les timers en place
	main_GUI._board.reset_timers();

	// Met le PGN � la date du jour
	main_GUI.update_date();

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
	main_GUI._root_exploration_node = new Node(test_board, 0, Move());


	// Boucle principale (Quitter � l'aide de la croix, ou en faisant �chap)
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

		// T - Test de thread
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_T)) {

			//main_GUI._board.grogros_zero(&monte_evaluator, 1, true, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._deep_mates_search, main_GUI._explore_checks);




			// grogrosZero sur le thread de la GUI
			//main_GUI._thread_grogros_zero = thread(&Board::grogros_zero, &main_GUI._board, &monte_evaluator, 50000, true, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, true, true, false, 0, nullptr);
			//main_GUI._thread_grogros_zero = thread(&Board::grogros_zero, &monte_buffer._heap_boards[main_GUI._board._index_children[0]], &monte_evaluator, 50000, true, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, true, true, false, 0, nullptr);
			//main_GUI._thread_grogros_zero.detach();

			// arr�te le thread
			//main_GUI._thread_grogros_zero.~thread();

			// Threads
			//main_GUI.thread_grogros_zero(&monte_evaluator, 5000);

			//main_GUI.grogros_zero_threaded(&monte_evaluator, 5000);

			// Teste la vitesse de la fonction d'�valuation
			/*cout << "testing eval speed..." << endl;
			test_function(&launch_eval, 1);*/

			// Teste la vitesse de g�n�ration des coups
			//cout << "testing moves generation speed..." << endl;
			/*Board b;
			b.from_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
			b.moves_generation_benchmark(5);*/
			//main_GUI._board.moves_generation_benchmark(6);

			//	1 move : 20 possible positions.
			//	2 moves : 400 possible positions.
			//	3 moves : 8 902 possible positions.
			//	4 moves : 197 281 possible positions.
			//	5 moves : 4 865 609 possible positions.
			//	6 moves : 119 060 324 possible positions.
			//	7 moves : 3 195 901 860 possible positions.
			//	8 moves : 84 998 978 956 possible positions.
			//	9 moves : 2 439 530 234 167 possible positions.
			//	10 moves : 69 352 859 712 417 possible positions.


			// Test de quiescence
			/*cout << "testing quiescence..." << endl;
			cout << main_GUI._board.get_color() * main_GUI._board.quiescence(main_GUI._grogros_eval) << endl;
			cout << main_GUI._board._quiescence_nodes << endl;*/

			//main_GUI.grogros_zero_threaded(main_GUI._grogros_eval, 50000);
			//main_GUI.thread_grogros_zero(main_GUI._grogros_eval, 1000);

			//main_GUI._board.grogros_quiescence(main_GUI._grogros_eval);

			main_GUI._root_exploration_node->grogros_zero(monte_buffer, *main_GUI._grogros_eval, main_GUI._beta, main_GUI._k_add, 50000);
			if (main_GUI._root_exploration_node->_nodes > 100000) {
				main_GUI.play_move_keep(main_GUI._root_exploration_node->_children[main_GUI._root_exploration_node->get_most_explored_child_index()]->_move);
			}
			cout << main_GUI._root_exploration_node->_nodes << endl;
			cout << main_GUI._root_exploration_node->_board._evaluation << endl;
		}

		// CTRL-T - Cherche le plateau de chess.com sur l'�cran, et lance une partie
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_T)) {
			locate_chessboard(main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom);
			main_GUI.new_bind_game();
		}

		// LCTRL-A - Binding full (binding chess.com)
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) {
			locate_chessboard(main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom);
			main_GUI._binding_full = !main_GUI._binding_full;
		}

		// LCTRL-Q - Mode de jeu automatique (binding chess.com) -> Check le binding seulement sur les coups de l'adversaire
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_A)) {
			main_GUI._binding_solo = !main_GUI._binding_solo;
			main_GUI._click_bind = !main_GUI._click_bind;
		}

		// Changements de la taille de la fen�tre
		if (IsWindowResized()) {
			main_GUI.get_window_size();
			//load_resources(); // Sinon �a devient flou
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
			main_GUI._board.from_fen(fen);
			cout << "loaded FEN : " << fen << endl;
		}

		// F - Retourne le plateau
		if (IsKeyPressed(KEY_F)) {
			main_GUI.switch_orientation();
		}

		// LCTRL-N - Recommencer une partie
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_N)) {
			//monte_buffer.reset();
			//main_GUI._board = monte_buffer._heap_boards[monte_buffer.get_first_free_index()];
			main_GUI.reset_pgn();
			main_GUI._board.restart();
			PlaySound(main_GUI._game_begin_sound);
		}

		// Utilisation du r�seau de neurones
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
			main_GUI._board.from_fen(fen);
			main_GUI.update_global_pgn();
			cout << "loaded FEN : " << fen << endl;
		}

		// // Colle le PGN du clipboard (le charge)
		// if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) {
		//     string pgn = GetClipboardText();
		//     main_GUI._board.from_pgn(pgn);
		//     cout << "loaded PGN : " << pgn << endl;
		// }

		// A - Analyse de partie sur chess.com (A)
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) {
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

		// B - Cr�ation du buffer
		if (IsKeyPressed(KEY_B)) {
			cout << "available memory : " << long_int_to_round_string(get_total_system_memory()) << "b" << endl;
			monte_buffer.init(buffer_size);
		}

		// G - GrogrosZero
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_G)) {
			if (!monte_buffer._init)
				monte_buffer.init(buffer_size);
			// LSHIFT - Utilisation du r�seau de neurones
			if (IsKeyDown(KEY_LEFT_SHIFT))
				//main_GUI._board.grogros_zero(nullptr, nodes_per_frame, main_GUI._beta, main_GUI._k_add, false, 0, &grogros_network);
				false;
			else
				main_GUI._board.grogros_zero(main_GUI._grogros_eval, nodes_per_frame, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._explore_checks);
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

		// LCTRL-H - Arr�t de la recherche automatique de GrogrosZero
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_H)) {
			main_GUI._grogros_analysis = false;
		}

		// H - D�ffichage/Affichage des fl�ches, Affichage/D�saffichage des contr�les
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_H)) {
			main_GUI.switch_arrow_drawing();
		}

		// R - R�initialisation des timers
		if (IsKeyPressed(KEY_R)) {
			main_GUI._board.reset_timers();
			main_GUI._time = false;
		}

		// Suppr. - Supprime les reflexions de GrogrosZero
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_DELETE)) {
			main_GUI._board.reset_all(true, true);
		}

		// CTRL - Suppr. - Supprime le buffer de Monte-Carlo
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_DELETE)) {
			main_GUI._board.reset_all(true, true);
			monte_buffer.remove();
		}

		// D - Affichage dans la console de tous les coups l�gaux de la position
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_D)) {
			main_GUI._draw = true;
			//main_GUI._board.display_moves(true);
			cout << main_GUI._board._positions_history.size() << endl;
		}

		// E - �value la position et renvoie les composantes dans la console
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


		// Modification des param�tres de recherche de GrogrosZero
		IsKeyPressed(KEY_KP_ADD) && (main_GUI._beta *= 1.1f);
		IsKeyPressed(KEY_KP_SUBTRACT) && (main_GUI._beta /= 1.1f);
		IsKeyPressed(KEY_KP_MULTIPLY) && (main_GUI._k_add *= 1.25f);
		IsKeyPressed(KEY_KP_DIVIDE) && (main_GUI._k_add /= 1.25f);

		// R-Return - Reset aux valeurs initiales
		if (IsKeyPressed(KEY_KP_ENTER)) {
			main_GUI._beta = 0.05f;
			main_GUI._k_add = 25.0f;
		}

		// 1 - Recherche en profondeur extr�me
		if (IsKeyPressed(KEY_ONE)) {
			main_GUI._beta = 0.5f;
			main_GUI._k_add = 0.0f;
		}

		// 2 - Recherche en profondeur
		if (IsKeyPressed(KEY_TWO)) {
			main_GUI._beta = 0.2f;
			main_GUI._k_add = 10.0f;
		}

		// 3 - Recherche large
		if (IsKeyPressed(KEY_THREE)) {
			main_GUI._beta = 0.01f;
			main_GUI._k_add = 100.0f;
		}

		// 4 - Recherche de mat
		if (IsKeyPressed(KEY_FOUR)) {
			main_GUI._beta = 0.005f;
			main_GUI._k_add = 2500.0f;
		}

		// 5 - Recherche de victoire en endgame
		if (IsKeyPressed(KEY_FIVE)) {
			main_GUI._beta = 0.05f;
			main_GUI._k_add = 5000.0f;
		}

		// P - Joue le coup recommand� par l'algorithme de GrogrosZero
		if (!IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_P)) {
			if (main_GUI._board._tested_moves > 0)
				((main_GUI._click_bind && main_GUI._board.click_m_move(main_GUI._board._moves[main_GUI._board.best_monte_carlo_move()], main_GUI.get_board_orientation())) || true) && main_GUI._board.play_monte_carlo_move_keep(main_GUI._board._moves[main_GUI._board.best_monte_carlo_move()], true, true, false);
			else
				cout << "no more moves are in memory" << endl;
		}

		// LShift-P - Joue les coups recommand�s par l'algorithme de GrogrosZero
		if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyDown(KEY_P)) {
			if (main_GUI._board._tested_moves > 0)
				((main_GUI._click_bind && main_GUI._board.click_m_move(main_GUI._board._moves[main_GUI._board.best_monte_carlo_move()], main_GUI.get_board_orientation())) || true) && main_GUI._board.play_monte_carlo_move_keep(main_GUI._board._moves[main_GUI._board.best_monte_carlo_move()], true, true, false);
			else
				cout << "no more moves are in memory" << endl;
		}

		// Return - Lancement et arr�t du temps
		if (IsKeyPressed(KEY_SPACE)) {
			if (main_GUI._time)
				main_GUI.stop_time();
			else
				main_GUI.start_time();
		}

		// Z - Cl� de Zobrist de la position actuelle
		if (IsKeyPressed(KEY_W)) {
			cout << "Zobrist key : " << main_GUI._board._zobrist_key << endl;
			cout << main_game_over << endl;
		}

		// UP/DOWN - Activation, d�sactivation de GrogrosFish pour les pi�ces blanches
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

		// UP/DOWN - Activation, d�sactivation de GrogrosFish pour les pi�ces noires
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

		// CTRL-UP/DOWN - Activation, d�sactivation de GrogrosZero pour les pi�ces blanches
		if (IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && main_GUI.get_board_orientation()) || (IsKeyPressed(KEY_UP) && !main_GUI.get_board_orientation()))) {
			if (main_GUI._white_player.substr(0, 11) == "GrogrosZero")
			{
				main_GUI._white_player = "White";
				main_GUI._white_title = "";
				main_GUI._white_elo = "?";
				main_GUI._white_url = "";
				main_GUI._white_country = "";
			}

			else
			{
				main_GUI._white_player = "GrogrosZero";
				main_GUI._white_title = "BOT";
				main_GUI._white_elo = main_GUI._grogros_zero_elo;
				main_GUI._white_url = "https://images.chesscomfiles.com/uploads/v1/user/284728633.4af59e2f.50x50o.0c8cdf830b69.png";
				main_GUI._white_country = "57";
			}
		}

		// CTRL-UP/DOWN - Activation, d�sactivation de GrogrosZero pour les pi�ces noires
		if (IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && !main_GUI.get_board_orientation()) || (IsKeyPressed(KEY_UP) && main_GUI.get_board_orientation()))) {
			if (main_GUI._black_player.substr(0, 11) == "GrogrosZero")
			{
				main_GUI._black_player = "Black";
				main_GUI._black_title = "";
				main_GUI._black_elo = "?";
				main_GUI._black_url = "";
				main_GUI._black_country = "";
			}

			else
			{
				main_GUI._black_player = "GrogrosZero";
				main_GUI._black_title = "BOT";
				main_GUI._black_elo = main_GUI._grogros_zero_elo;
				main_GUI._black_url = "https://images.chesscomfiles.com/uploads/v1/user/284728633.4af59e2f.50x50o.0c8cdf830b69.png";
				main_GUI._black_country = "57";
			}
		}

		// Fin de partie (� reset aussi...) (le son ne se lance pas...)
		// Calculer la fin de la partie ici une fois, pour �viter de la refaire?

		// Plus de temps... (en faire une fonction)
		// if (main_GUI._board._time) {
		//     if (main_GUI._board._time_black < 0) {
		//         main_GUI._board._time = false;
		//         main_GUI._board._time_black = 0;
		//         play_end_sound();
		//         main_GUI._board._is_game_over = true;
		//         cout << "White won on time" << endl; // Pas toujours vrai car il peut y avoir des manques de mat�riel
		//     }
		//     if (main_GUI._board._time_white < 0) {
		//         main_GUI._board._time = false;
		//         main_GUI._board._time_white = 0;
		//         play_end_sound();
		//         main_GUI._board._is_game_over = true;
		//         cout << "Black won on time" << endl;
		//     }

		// }


		// Fl�che gauche: revient sur la position pr�c�dente
		if (IsKeyPressed(KEY_LEFT)) {
			if (main_GUI._game_tree.select_previous_node())
				main_GUI._board = (main_GUI._game_tree._current_node)->_board;
		}

		// Fl�che droite: avance sur la position suivante
		if (IsKeyPressed(KEY_RIGHT)) {
			if (main_GUI._game_tree.select_first_next_node())
				main_GUI._board = (main_GUI._game_tree._current_node)->_board;
		}



		// Jeu des IA

		// Fait jouer l'IA automatiquement en fonction des param�tres
		main_GUI._board._game_over_checked = false;  // On recheck, pour les threefold (car dans la r�flexion on dit que la partie est finie si y'a une seule r�p�tition)
		if (main_GUI._board.is_game_over(3) == 0) {
			// GrogrosZero

			// Quand c'est son tour
			if ((main_GUI._board._player && main_GUI._white_player.substr(0, 11) == "GrogrosZero") || (!main_GUI._board._player && main_GUI._black_player.substr(0, 11) == "GrogrosZero")) {
				if (!monte_buffer._init)
					monte_buffer.init(buffer_size);

				// Grogros doit g�rer son temps
				if (main_GUI._time) {
					// Nombre de noeuds que Grogros doit calculer (en fonction des contraintes de temps)
					static constexpr int supposed_grogros_speed = 2500; // En supposant que Grogros va � plus de 5k noeuds par seconde
					int tot_nodes = main_GUI._board.total_nodes();
					float best_move_percentage = tot_nodes == 0 ? 0.05f : static_cast<float>(main_GUI._board._nodes_children[main_GUI._board.best_monte_carlo_move()]) / static_cast<float>(main_GUI._board.total_nodes());
					int max_move_time = main_GUI._board._player ?
						time_to_play_move(main_GUI._time_white, main_GUI._time_black, 0.2f * (1.0f - best_move_percentage)) :
						time_to_play_move(main_GUI._time_black, main_GUI._time_white, 0.2f * (1.0f - best_move_percentage));

					// Si il nous reste beaucoup de temps en fin de partie, on peut r�fl�chir plus longtemps
					max_move_time *= (1 + main_GUI._board._adv); // Regarder si �a marche bien (TODO)

					// On veut �tre s�r de jouer le meilleur coup de Grogros
					// Si il y a un meilleur coup que celui avec le plus de noeuds, attendre...
					bool wait_for_best_move = tot_nodes != 0 && main_GUI._board._eval_children[main_GUI._board.best_monte_carlo_move()] * main_GUI._board.get_color() < main_GUI._board._evaluation * main_GUI._board.get_color();
					max_move_time = wait_for_best_move ? max_move_time : max_move_time / 4;

					// Nombre de noeuds � calculer
					int grogros_timed_nodes = min(nodes_per_frame, supposed_grogros_speed * max_move_time / 1000);
					main_GUI._board.grogros_zero(main_GUI._grogros_eval, min(!main_GUI._time ? nodes_per_frame : grogros_timed_nodes, grogros_nodes - main_GUI._board.total_nodes()), main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._explore_checks);
					
					/*if (main_GUI._board._time_monte_carlo >= max_move_time)
						((main_GUI._click_bind && main_GUI._board.click_m_move(main_GUI._board._moves[main_GUI._board.best_monte_carlo_move()], get_board_orientation())) || true) && main_GUI._board.play_monte_carlo_move_keep(main_GUI._board._moves[main_GUI._board.best_monte_carlo_move()], true, true, false, false);
				*/
					// Equivalent en nombre de noeuds
					int nodes_to_play = supposed_grogros_speed * max_move_time / 1000;

					if (main_GUI._board.total_nodes() >= nodes_to_play)
						((main_GUI._click_bind && main_GUI._board.click_m_move(main_GUI._board._moves[main_GUI._board.best_monte_carlo_move()], main_GUI.get_board_orientation())) || true) && main_GUI._board.play_monte_carlo_move_keep(main_GUI._board._moves[main_GUI._board.best_monte_carlo_move()], true, true, false);

				}
				else
					main_GUI._board.grogros_zero(main_GUI._grogros_eval, nodes_per_frame, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._explore_checks);
			}

			// Quand c'est pas son tour
			if ((!main_GUI._board._player && main_GUI._white_player.substr(0, 12) == "GrogrosZero") || (main_GUI._board._player && main_GUI._black_player.substr(0, 12) == "GrogrosZero")) {
				if (!monte_buffer._init)
					monte_buffer.init(buffer_size);
				main_GUI._board.grogros_zero(main_GUI._grogros_eval, nodes_per_user_frame, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._explore_checks);
			}

			// Mode analyse
			if (main_GUI._grogros_analysis) {
				if (!monte_buffer._init)
					monte_buffer.init(buffer_size);

				if (!main_GUI.is_playing())
					main_GUI._board.grogros_zero(main_GUI._grogros_eval, nodes_per_frame, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._explore_checks);
				else
					main_GUI._board.grogros_zero(main_GUI._grogros_eval, nodes_per_user_frame, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._explore_checks); // Pour que �a ne lag pas pour l'utilisateur
			}

			if (main_GUI._board.is_game_over(3) != 0)
				goto game_over;

			// GrogrosFish (seulement lorsque c'est son tour)
			if (main_GUI._board._player && main_GUI._white_player.substr(0, 11) == "GrogrosFish")
				main_GUI._board.grogrosfish(search_depth, &eval_white, true);

			if (!main_GUI._board._player && main_GUI._black_player.substr(0, 11) == "GrogrosFish")
				main_GUI._board.grogrosfish(search_depth, &eval_black, true);
		}

		// Si la partie est termin�e
		else {
		game_over:

			if (!main_game_over) {
				main_GUI._time = false;
				main_GUI._board.display_pgn();
				main_game_over = true;
			}
		}

		// Jeu automatique sur chess.com
		if (main_GUI._binding_full || (main_GUI._binding_solo && main_GUI.get_board_orientation() != main_GUI._board._player)) {
			// Le fait � chaque intervalle de temps 'binding_interval_check'
			if (clock() - main_GUI._last_binding_check > main_GUI._binding_interval_check) {
				// Coup jou� sur le plateau
				main_GUI._binding_move = get_board_move(main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom, main_GUI.get_board_orientation());

				// V�rifie que le coup est l�gal avant de le jouer
				for (int i = 0; i < main_GUI._board._got_moves; i++) {
					if (main_GUI._board._moves[i].i1 == main_GUI._binding_move[0] && main_GUI._board._moves[i].j1 == main_GUI._binding_move[1] && main_GUI._board._moves[i].i2 == main_GUI._binding_move[2] && main_GUI._board._moves[i].j2 == main_GUI._binding_move[3]) {
						main_GUI._board.play_move_sound(Move(main_GUI._binding_move[0], main_GUI._binding_move[1], main_GUI._binding_move[2], main_GUI._binding_move[3]));
						main_GUI._board.play_monte_carlo_move_keep(main_GUI._board._moves[i], true, true, true);
						break;
					}
				}

				main_GUI._last_binding_check = clock();
			}
		}

		gui_draw();
	}

	// Fermeture de la fen�tre
	CloseWindow();

	return 0;
}