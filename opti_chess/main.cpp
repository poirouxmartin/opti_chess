#include "opti_chess.h"
#include "time_tests.h"
#include "useful_functions.h"
#include "gui.h"
#include "windows_tests.h"

// *** Répertoire git ***
// Documents/Info/Echecs/opti_chess/c++_git



// Fonction de test
void launch_eval() {
	/*main_GUI._board.reset_board();
	main_GUI._board.is_game_over();
	main_GUI._board.evaluate_int(main_GUI._eval, true);*/
	main_GUI._board._quick_sorted_moves = false;
	main_GUI._board.quick_moves_sort();
	//main_GUI._board.is_controlled(3, 3);
	//main_GUI._board.attacked(3, 3);
}



// Fonction qui fait le dessin de la GUI
void gui_draw() {
	if (!main_GUI._draw)
		return;
	BeginDrawing();
	main_GUI._board.draw();
	EndDrawing();
}

// Main
int main() {
	// Faire une fonction Init pour raylib?

	// Fenêtre resizable
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	SetConfigFlags(FLAG_MSAA_4X_HINT);
	SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
	SetConfigFlags(FLAG_VSYNC_HINT);

	// Pour ne pas afficher toutes les infos (on peut mettre le log level de 0 à 7 -> 7 = rien)
	SetTraceLogLevel(LOG_WARNING);

	// Initialisation de la fenêtre
	InitWindow(main_GUI._screen_width, main_GUI._screen_height, "Grogros Chess");

	// Initialisation de l'audio
	InitAudioDevice();
	SetMasterVolume(1.0f);

	// Nombre d'images par secondes
	SetTargetFPS(fps);

	// Curseur
	HideCursor();
	//SetMouseCursor(3);

	// Variables
	// Board t;
	all_positions[0] = main_GUI._board.simple_position();
	total_positions = 1;

	// Evaluateur de position
	Evaluator eval_white(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	Evaluator eval_black;

	// Evaluateur pour Monte Carlo
	Evaluator monte_evaluator;

	// Nombre de noeuds max pour le jeu automatique de GrogrosZero
	int grogros_nodes = 3000000;

	// Nombre de noeuds calculés par frame
	// Si c'est sur son tour
	int nodes_per_frame = 250;

	// Sur le tour de l'autre (pour que ça plante moins)
	int nodes_per_user_frame = 50;



	//monte_evaluator = eval_white;

	// Paramètres pour l'IA
	int search_depth = 8;
	search_depth = 6;

	// Fin de partie
	bool main_game_over = false;

	//// Réseau de neurones
	//Network grogros_network;
	//grogros_network.generate_random_weights();
	//bool use_neural_network = false;

	//// Liste de réseaux de neurones pour les tournois
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

	// Met le PGN à la date du jour
	main_GUI.update_date();

	//printAttributeSizes(main_GUI._board);
	//testFunc(main_GUI._board);

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

		// T - Test de thread
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_T)) {

			//main_GUI._board.grogros_zero(&monte_evaluator, 1, true, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._deep_mates_search, main_GUI._explore_checks);

			


			// grogrosZero sur le thread de la GUI
			//main_GUI._thread_grogros_zero = thread(&Board::grogros_zero, &main_GUI._board, &monte_evaluator, 50000, true, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, true, true, false, 0, nullptr);
			//main_GUI._thread_grogros_zero = thread(&Board::grogros_zero, &monte_buffer._heap_boards[main_GUI._board._index_children[0]], &monte_evaluator, 50000, true, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, true, true, false, 0, nullptr);
			//main_GUI._thread_grogros_zero.detach();
			
			// arrête le thread
			//main_GUI._thread_grogros_zero.~thread();

			// Threads
			//main_GUI.thread_grogros_zero(&monte_evaluator, 5000);

			//main_GUI.grogros_zero_threaded(&monte_evaluator, 5000);

			// Teste la vitesse de la fonction d'évaluation
			cout << "testing eval speed..." << endl;
			test_function(&launch_eval, 1);

			// Teste la vitesse de génération des coups
			//cout << "testing moves generation speed..." << endl;
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
		}

		// CTRL-T - Cherche le plateau de chess.com sur l'écran, et lance une partie
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_T)) {
			cout << "looking for chess.com chessboard..." << endl;
			locate_chessboard(main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom);
			printf("Top-Left: (%d, %d)\n", main_GUI._binding_left, main_GUI._binding_top);
			printf("Bottom-Right: (%d, %d)\n", main_GUI._binding_right, main_GUI._binding_bottom);
			cout << "chess.com chessboard has been located" << endl;
			main_GUI.new_bind_game();
		}

		// LCTRL-A - Binding full (binding chess.com)
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) {
			main_GUI._binding_full = !main_GUI._binding_full;
		}

		// LCTRL-Q - Mode de jeu automatique (binding chess.com) -> Check le binding seulement sur les coups de l'adversaire
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_A)) {
			main_GUI._binding_solo = !main_GUI._binding_solo;
			main_GUI._click_bind = !main_GUI._click_bind;
		}

		// Changements de la taille de la fenêtre
		if (IsWindowResized()) {
			get_window_size();
			//load_resources(); // Sinon ça devient flou
			resize_GUI();
		}

		// S - Save FEN dans data/text.txt
		if (IsKeyPressed(KEY_S)) {
			SaveFileText("data/test.txt", const_cast<char*>(main_GUI._current_fen.c_str()));
			cout << "saved FEN : " << main_GUI._current_fen << endl;
		}

		// L - Load FEN dans data/text.txt
		if (IsKeyPressed(KEY_L)) {
			string fen = LoadFileText("data/test.txt");
			main_GUI._board.from_fen(fen);
			cout << "loaded FEN : " << fen << endl;
		}

		// F - Retourne le plateau
		if (IsKeyPressed(KEY_F)) {
			switch_orientation();
		}

		// LCTRL-N - Recommencer une partie
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_N)) {
			//monte_buffer.reset();
			//main_GUI._board = monte_buffer._heap_boards[monte_buffer.get_first_free_index()];
			main_GUI.reset_pgn();
			main_GUI._board.restart();
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

		// B - Création du buffer
		if (IsKeyPressed(KEY_B)) {
			cout << "available memory : " << long_int_to_round_string(get_total_system_memory()) << "b" << endl;
			monte_buffer.init();
		}

		// G - GrogrosZero
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_G)) {
			if (!monte_buffer._init)
				monte_buffer.init();
			// LSHIFT - Utilisation du réseau de neurones
			if (IsKeyDown(KEY_LEFT_SHIFT))
				//main_GUI._board.grogros_zero(nullptr, nodes_per_frame, main_GUI._beta, main_GUI._k_add, false, 0, &grogros_network);
				false;
			else
				main_GUI._board.grogros_zero(&monte_evaluator, nodes_per_frame, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._explore_checks);
		}

		// LCTRL-G - Lancement de GrogrosZero en recherche automatique
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_G)) {
			if (!monte_buffer._init)
				monte_buffer.init();
			main_GUI._grogros_analysis = true;
		}

		// Espace - GrogrosZero 1 noeud : DEBUG
		if (IsKeyPressed(KEY_SPACE)) {
			/*if (!monte_buffer._init)
				monte_buffer.init();
			main_GUI._board.grogros_zero(&monte_evaluator, 1, true, main_GUI._beta, main_GUI._k_add);*/
		}

		// LCTRL-H - Arrêt de la recherche automatique de GrogrosZero
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_H)) {
			main_GUI._grogros_analysis = false;
		}

		// H - Déffichage/Affichage des flèches, Affichage/Désaffichage des contrôles
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_H)) {
			switch_arrow_drawing();
		}

		// R - Réinitialisation des timers
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

		// D - Affichage dans la console de tous les coups légaux de la position
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_D)) {
			main_GUI._draw = true;
			main_GUI._board.display_moves(true);
		}

		// E - Évalue la position et renvoie les composantes dans la console
		if (IsKeyPressed(KEY_E)) {
			main_GUI._board.evaluate_int(&monte_evaluator, true);
			cout << "Evaluation : \n" << eval_components << endl;
		}

		// U - Undo de dernier coup joué
		IsKeyPressed(KEY_U) && main_GUI._board.undo();

		// Modification des paramètres de recherche de GrogrosZero
		IsKeyPressed(KEY_KP_ADD) && (main_GUI._beta *= 1.1f);
		IsKeyPressed(KEY_KP_SUBTRACT) && (main_GUI._beta /= 1.1f);
		IsKeyPressed(KEY_KP_MULTIPLY) && (main_GUI._k_add *= 1.25f);
		IsKeyPressed(KEY_KP_DIVIDE) && (main_GUI._k_add /= 1.25f);

		// R-Return - Reset aux valeurs initiales
		if (IsKeyPressed(KEY_KP_ENTER)) {
			main_GUI._beta = 0.05f;
			main_GUI._k_add = 25.0f;
		}

		// 1 - Recherche en profondeur extrême
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

		// P - Joue le coup recommandé par l'algorithme de GrogrosZero
		if (!IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_P)) {
			if (main_GUI._board._tested_moves > 0)
				((main_GUI._click_bind && main_GUI._board.click_i_move(main_GUI._board.best_monte_carlo_move(), get_board_orientation())) || true) && main_GUI._board.play_monte_carlo_move_keep(main_GUI._board.best_monte_carlo_move(), true, true, false, false);
			else
				cout << "no more moves are in memory" << endl;
		}

		// LShift-P - Joue les coups recommandés par l'algorithme de GrogrosZero
		if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyDown(KEY_P)) {
			if (main_GUI._board._tested_moves > 0)
				((main_GUI._click_bind && main_GUI._board.click_i_move(main_GUI._board.best_monte_carlo_move(), get_board_orientation())) || true) && main_GUI._board.play_monte_carlo_move_keep(main_GUI._board.best_monte_carlo_move(), true, true, false, false);
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

		// UP/DOWN - Activation, désactivation de GrogrosFish pour les pièces blanches
		if (!IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && get_board_orientation()) || (IsKeyPressed(KEY_UP) && !get_board_orientation()))) {
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
		if (!IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && !get_board_orientation()) || (IsKeyPressed(KEY_UP) && get_board_orientation()))) {
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
		if (IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && get_board_orientation()) || (IsKeyPressed(KEY_UP) && !get_board_orientation()))) {
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

		// CTRL-UP/DOWN - Activation, désactivation de GrogrosZero pour les pièces noires
		if (IsKeyDown(KEY_LEFT_CONTROL) && ((IsKeyPressed(KEY_DOWN) && !get_board_orientation()) || (IsKeyPressed(KEY_UP) && get_board_orientation()))) {
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




		// Jeu des IA

		// Fait jouer l'IA automatiquement en fonction des paramètres
		if (main_GUI._board.is_game_over() == 0) {
			// GrogrosZero

			// Quand c'est son tour
			if ((main_GUI._board._player && main_GUI._white_player.substr(0, 11) == "GrogrosZero") || (!main_GUI._board._player && main_GUI._black_player.substr(0, 11) == "GrogrosZero")) {
				if (!monte_buffer._init)
					monte_buffer.init();

				// Grogros doit gérer son temps
				if (main_GUI._time) {
					// Nombre de noeuds que Grogros doit calculer (en fonction des contraintes de temps)
					static constexpr int supposed_grogros_speed = 5000; // En supposant que Grogros va à plus de 20k noeuds par seconde
					int tot_nodes = main_GUI._board.total_nodes();
					float best_move_percentage = tot_nodes == 0 ? 0.05f : static_cast<float>(main_GUI._board._nodes_children[main_GUI._board.best_monte_carlo_move()]) / static_cast<float>(main_GUI._board.total_nodes());
					int max_move_time = main_GUI._board._player ?
						time_to_play_move(main_GUI._time_white, main_GUI._time_black, 0.075f * (1.0f - best_move_percentage)) :
						time_to_play_move(main_GUI._time_black, main_GUI._time_white, 0.075f * (1.0f - best_move_percentage));
					int grogros_timed_nodes = min(nodes_per_frame, supposed_grogros_speed * max_move_time / 1000);
					main_GUI._board.grogros_zero(&monte_evaluator, min(!main_GUI._time ? nodes_per_frame : grogros_timed_nodes, grogros_nodes - main_GUI._board.total_nodes()), main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._explore_checks);
					if (main_GUI._board._time_monte_carlo >= max_move_time)
						((main_GUI._click_bind && main_GUI._board.click_i_move(main_GUI._board.best_monte_carlo_move(), get_board_orientation())) || true) && main_GUI._board.play_monte_carlo_move_keep(main_GUI._board.best_monte_carlo_move(), true, true, false, false);
				}
				else
					main_GUI._board.grogros_zero(&monte_evaluator, nodes_per_frame, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._explore_checks);
			}

			// Quand c'est pas son tour
			if ((!main_GUI._board._player && main_GUI._white_player.substr(0, 12) == "GrogrosZero") || (main_GUI._board._player && main_GUI._black_player.substr(0, 12) == "GrogrosZero")) {
				if (!monte_buffer._init)
					monte_buffer.init();
				main_GUI._board.grogros_zero(&monte_evaluator, nodes_per_user_frame, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._explore_checks);
			}

			// Mode analyse
			if (main_GUI._grogros_analysis) {
				if (!monte_buffer._init)
					monte_buffer.init();

				if (!is_playing())
					main_GUI._board.grogros_zero(&monte_evaluator, nodes_per_frame, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._explore_checks);
				else
					main_GUI._board.grogros_zero(&monte_evaluator, nodes_per_user_frame, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, main_GUI._explore_checks); // Pour que ça ne lag pas pour l'utilisateur
			}

			if (main_GUI._board.is_game_over() != 0)
				goto game_over;

			// GrogrosFish (seulement lorsque c'est son tour)
			if (main_GUI._board._player && main_GUI._white_player.substr(0, 11) == "GrogrosFish")
				main_GUI._board.grogrosfish(search_depth, &eval_white, true);

			if (!main_GUI._board._player && main_GUI._black_player.substr(0, 11) == "GrogrosFish")
				main_GUI._board.grogrosfish(search_depth, &eval_black, true);
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

		// Jeu automatique sur chess.com
		if (main_GUI._binding_full || (main_GUI._binding_solo && get_board_orientation() != main_GUI._board._player)) {
			// Le fait à chaque intervalle de temps 'binding_interval_check'
			if (clock() - main_GUI._last_binding_check > main_GUI._binding_interval_check) {
				// Coup joué sur le plateau
				main_GUI._binding_move = get_board_move(main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom, get_board_orientation());

				// Vérifie que le coup est légal avant de le jouer
				for (int i = 0; i < main_GUI._board._got_moves; i++) {
					if (main_GUI._board._moves[i].i1 == main_GUI._binding_move[0] && main_GUI._board._moves[i].j1 == main_GUI._binding_move[1] && main_GUI._board._moves[i].i2 == main_GUI._binding_move[2] && main_GUI._board._moves[i].j2 == main_GUI._binding_move[3]) {
						main_GUI._board.play_move_sound(Move(main_GUI._binding_move[0], main_GUI._binding_move[1], main_GUI._binding_move[2], main_GUI._binding_move[3]));
						main_GUI._board.play_monte_carlo_move_keep(i, true, true, true, true);
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