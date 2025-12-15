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
#include "tests.h"

// Fonction qui fait le dessin de la GUI
inline void gui_draw() {
	if (!main_GUI._draw)
		return;
	BeginDrawing();
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
	SetConfigFlags(FLAG_WINDOW_HIGHDPI);

	// Pour ne pas afficher toutes les infos (on peut mettre le log level de 0 à 7 -> 7 = rien)
	SetTraceLogLevel(LOG_ALL);

	// Initialisation de la fenêtre
	InitWindow(main_GUI._screen_width, main_GUI._screen_height, "Grogros Chess");

	// Initialisation de l'audio
	InitAudioDevice();
	SetMasterVolume(1.0f);

	// Nombre d'images par secondes
	SetTargetFPS(main_GUI._max_fps);

	// Espace entre les lignes de texte
	SetTextLineSpacing(4);

	// Curseur
	HideCursor();

	// Shader pour les pièces sélectionnées
	main_GUI._selected_shader = LoadShader(0, "resources/shaders/outline.fs");

	int outline_size_loc = GetShaderLocation(main_GUI._selected_shader, "outlineSize");
	int outline_color_loc = GetShaderLocation(main_GUI._selected_shader, "outlineColor");
	int texture_size_loc = GetShaderLocation(main_GUI._selected_shader, "textureSize");

	// Taille du contout
	float outline_size = 16.0f;
	float outline_color[4] = { 1.0f, 0.0f, 0.0f, 0.5f };
	float texture_size[2] = { 1024.0f, 1024.0f };

	SetShaderValue(main_GUI._selected_shader, outline_size_loc, &outline_size, SHADER_UNIFORM_FLOAT);
	SetShaderValue(main_GUI._selected_shader, outline_color_loc, outline_color, SHADER_UNIFORM_VEC4);
	SetShaderValue(main_GUI._selected_shader, texture_size_loc, texture_size, SHADER_UNIFORM_VEC2);

	// Paramètres pour l'IA
	int search_depth = 6;

	// Fin de partie
	bool main_game_over = false;

	// Met les timers en place
	main_GUI._board->reset_timers();

	// Met le PGN à la date du jour
	main_GUI.update_date();

	// Met à jour le nom de bot de GrogrosZero
	main_GUI.update_grogros_zero_name();

	// Taille de la table de transposition
	constexpr int transposition_table_size = 5E6;

	// Initialisation de la table de transposition
	transposition_table.init(transposition_table_size, nullptr, true);


	// Initialisation du buffer de Monte-Carlo
	main_GUI.init_buffers();

	main_GUI._board = new Board();
	main_GUI._root_exploration_node = new Node();
	main_GUI._game_tree = GameTree(*main_GUI._board);

	main_GUI.reset_game();

	// Noeud d'exploration
	// Plateau test: rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq - 0 2
	//test_board.from_fen("rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq - 0 2");



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

			//main_GUI._board->get_pins(main_GUI._board->_player).print();
			//main_GUI._board->get_moves_fast();
			//main_GUI._board->update_kings_pos();
			//Pos king_pos = main_GUI._board->_player ? main_GUI._board->_white_king_pos : main_GUI._board->_black_king_pos;
			//bool player = main_GUI._board->_player;
			//CastlingRights castling_rights = main_GUI._board->_castling_rights;
			//
			//uint16_t controls_around_king = main_GUI._board->get_controls_around_king(king_pos, player, player ? castling_rights.k_w : castling_rights.k_b, player ? castling_rights.q_w : castling_rights.q_b);
			//print_controls(controls_around_king);

			// Position test pour la reflexion en milieu de partie: r1bq1b1r/pp4pp/2p1k3/3np3/1nBP4/2N2Q2/PPP2PPP/R1B2RK1 b - - 0 10

			// Benchmark de la fonction d'évaluation
			clock_t start = clock();
			uint64_t iterations = 0;

			cout << "Benchmarking evaluation function for 1 second..." << endl;

			while (clock() - start < 1000) {
				main_GUI.evaluate_position(false);
				iterations++;
			}

			clock_t end = clock();
			double duration = double(end - start) / CLOCKS_PER_SEC;
			cout << "Function executed " << iterations << " times in " << duration << " seconds. (" << (iterations / duration) << " calls per second, average " << (duration / iterations * 1e6) << " microseconds per call)" << endl;
			
			main_GUI._board->benchmark_nodes_count_at_depth("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6, { 1, 20, 400, 8902, 197281, 4865609, 119060324, 3195901860 }, 10, true);
			//main_GUI._board->validate_nodes_count_at_depth("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6, { 1, 20, 400, 8902, 197281, 4865609, 119060324, 3195901860 }, true);
			//main_GUI._board->validate_nodes_count_at_depth("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 7, { 1, 20, 400, 8902, 197281, 4865609, 119060324, 3195901860 }, true, false, true);
			//main_GUI._board->validate_nodes_count_at_depth("rnbqkbnr/pppppppp/8/8/8/3P4/PPP1PPPP/RNBQKBNR b KQkq - 0 1", 6, { 1, 20, 539, 11959, 328511, 8073082, 227598692 }, true, true);
			//main_GUI._board->validate_nodes_count_at_depth("rnbqkbnr/pp1ppppp/8/2p5/8/3P4/PPP1PPPP/RNBQKBNR w KQkq - 0 2", 3, { 1, 27, 593, 15971 }, true, true);
			//main_GUI._board->validate_nodes_count_at_depth("rnbqkbnr/pp1ppppp/8/2p5/8/3P4/PPPKPPPP/RNBQ1BNR b kq - 1 2", 2, { 1, 22, 487 }, true, true);
			//main_GUI._board->validate_nodes_count_at_depth("rnbqkbnr/pp1ppppp/8/8/2p5/3P4/PPPKPPPP/RNBQ1BNR w kq - 0 3", 1, { 1, 23 }, true, true);




			//main_GUI._board->validate_nodes_count_at_depth("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5, { 1, 48, 2039, 97862, 4085603, 193690690 }, true);
			//main_GUI._board->validate_nodes_count_at_depth("", 5, { }, true);

			//main_GUI.grogros_analysis(1);

			//r3kqr1/1pp2p1p/2b1p3/p1b1P3/P4P2/2N1p2Q/1PP3PP/1RBR3K w - - 0 20
			//main_GUI._root_exploration_node->quiescence(&monte_buffer, main_GUI._grogros_eval, main_GUI._quiescence_depth, main_GUI._alpha, main_GUI._beta);
			//main_GUI._root_exploration_node->quiescence(&monte_buffer, main_GUI._grogros_eval, 6, -2147483647, 2147483647, nullptr, true, -1000);

			//main_GUI._root_exploration_node->_board->switch_colors();
			//main_GUI._root_exploration_node->_board->get_king_squares_distance(true).print();
			//main_GUI._root_exploration_node->_board->get_king_squares_distance(false).print();



			//main_GUI._root_exploration_node->quiescence(&monte_buffer, main_GUI._grogros_eval, main_GUI._quiescence_depth, main_GUI._alpha, main_GUI._beta);
			//cout << "deep eval: " << main_GUI._root_exploration_node->_deep_evaluation._value << endl;
			//auto move_scores = main_GUI._root_exploration_node->get_move_scores(main_GUI._alpha, main_GUI._beta);
			//for (auto const& [move, score] : move_scores) {
			//	cout << move.to_string() << ": " << score << endl;
			//}
			// r1bqk2r/ppp2ppp/1b6/nP1nP3/2P5/5P2/P5PP/RNBQKBNR b KQkq - 0 9 : Cxc4? ça devrait être vu...
			// r1bqk2r/ppp2ppp/1b6/1P1nP3/2B5/5P2/P5PP/RNBQK1NR b KQkq - 0 10 : il voit rien après Dh4?? -> Pas de standpat en échec!!

			//Map w_blocked_pieces = main_GUI._root_exploration_node->_board->get_all_blocked_pieces(true);
			//w_blocked_pieces.print();
			//Map b_blocked_pieces = main_GUI._root_exploration_node->_board->get_all_blocked_pieces(false);
			//b_blocked_pieces.print();

			//int short_term_piece_mobility = main_GUI._root_exploration_node->_board->get_short_term_piece_mobility(true);
			//cout << "Short term piece mobility: " << short_term_piece_mobility << endl;

			//int long_term_piece_mobility = main_GUI._root_exploration_node->_board->get_long_term_piece_mobility(true);
			//cout << "Long term piece mobility: " << long_term_piece_mobility << endl;

			//1k6/p3r2p/1nBq2p1/2NP1nP1/5p1P/P1Q5/1PKR1P2/8 w - - 5 38 : il met #2 sur Dh8+..?
			//1k2r2Q/p6p/1nBq2p1/2NP1nP1/5p1P/P7/1PKR1P2/8 w - - 7 39 : pareil sur quiescence, il met #2 au lieu de 3
			//1k2Q3/p6p/1nBq2p1/2NP1nP1/5p1P/P7/1PKR1P2/8 b - - 0 39 : ici pour la deep eval il met -96000000 au lieu de -95900000 (en gros il dit -#1 au lieu de -#2)

			//r1b2r2/1ppqbppk/p1n1p3/3P4/1P1Pn3/P3PN1P/R1QN1PP1/2B2K1R b - - 0 14

			//cout << "Quietness: " << main_GUI._root_exploration_node->_board->get_quietness() << endl;
			
			// Test du tri des coups
			//main_GUI._root_exploration_node->_board->get_moves();
			//main_GUI._root_exploration_node->_board->assign_all_move_flags();
			//main_GUI._root_exploration_node->_board->sort_moves();

			//2rqr1k1/pNbnnpp1/2p1p1p1/P2pP3/Q2P4/B1P4P/4BPP1/RR4K1 b - - 6 22 : ???
			//Board b(*main_GUI._root_exploration_node->_board);
			//b._player = !b._player; // Tour de l'adversaire
			//b.get_moves();
			//Node* stand_pat_node = new Node(&b);
			////cout << "player: " << b._player << endl;
			//stand_pat_node->quiescence(&monte_buffer, main_GUI._grogros_eval, 2, main_GUI._alpha, main_GUI._beta, -INT32_MAX, INT32_MAX, nullptr, false);
			//cout << "Stand pat eval: " << stand_pat_node->_deep_evaluation._value << endl;

			//Tests tests(&main_GUI);
			//tests.run_all_tests();

			//main_GUI._board->update_bitboards();
			//main_GUI._board->print_all_bitboards();
		}

		// B - Bitboards
		if (IsKeyPressed(KEY_B)) {
			//main_GUI._board->update_bitboards();
			main_GUI._board->print_all_bitboards();
		}

		// Q - Quiescence
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_A)) {
			main_GUI._root_exploration_node->quiescence(&monte_board_buffer, main_GUI._grogros_eval, main_GUI._quiescence_depth, main_GUI._alpha, main_GUI._beta);
			main_GUI._update_variants = true;
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
		//     main_GUI._board->from_pgn(pgn);
		//     cout << "loaded PGN : " << pgn << endl;
		// }

		// A - Analyse de partie sur chess.com (A)
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) {
			SetClipboardText(main_GUI._global_pgn.c_str());
			cout << "copied PGN for analysis on chess.com : \n" << main_GUI._global_pgn << endl;
			OpenURL("https://www.chess.com/analysis");
		}

		// LCTRL-A - Analyse de partie sur chess.com en direct, par GrogrosZero
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) {
			main_GUI.new_bind_analysis();
		}

		// TAB - Screenshot
		if (IsKeyPressed(KEY_TAB)) {
			string screenshot_name = "resources/screenshots/screenshot_" + to_string(time(nullptr)) + ".png";
			//const char* full_screenshot_name = (GetWorkingDirectory() + screenshot_name).c_str();
			const char* full_screenshot_name = screenshot_name.c_str();
			cout << "Screenshot : " << full_screenshot_name << endl;
			TakeScreenshot(full_screenshot_name);

			// Mettre le screenshot dans le presse-papier?
		}

		// B - Création du buffer
		if (IsKeyPressed(KEY_B)) {
			cout << "available memory : " << long_int_to_round_string(get_total_system_memory()) << "b" << endl;
			main_GUI.init_buffers();
		}

		// G - GrogrosZero
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_G)) {
			main_GUI.init_buffers();

			// LSHIFT - Utilisation du réseau de neurones
			if (IsKeyDown(KEY_LEFT_SHIFT))
				//main_GUI._board->grogros_zero(nullptr, nodes_per_frame, main_GUI._beta, main_GUI._k_add, false, 0, &grogros_network);
				false;
			else
				main_GUI.grogros_analysis();
		}

		// LCTRL-G - Lancement de GrogrosZero en recherche automatique
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_G)) {
			main_GUI.init_buffers();
			main_GUI._grogros_analysis = true;
		}

		// Entrée - GrogrosZero 1 noeud : DEBUG
		if (IsKeyPressed(KEY_ENTER)) {
			main_GUI.init_buffers();
			main_GUI.grogros_analysis(IsKeyPressed(KEY_LEFT_SHIFT) ? 10 : 1);
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
			main_GUI._board->reset_timers();
			main_GUI._time = false;
		}

		// Suppr. - Supprime les reflexions de GrogrosZero
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_DELETE)) {
			//main_GUI._board->reset_all(true, true);
			main_GUI._root_exploration_node->reset(); // FIXME... ça fait rien??
			main_GUI._root_exploration_node->_is_active = true;
			main_GUI._root_exploration_node->_board->_is_active = true;
			//main_GUI._root_exploration_node = new Node(&main_GUI._board, Move());
			cout << "Grogros's thought deleted... current moves explored: " << main_GUI._root_exploration_node->children_count() << endl;
		}

		// CTRL - Suppr. - Supprime le buffer de Monte-Carlo
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_DELETE)) {
			cout << "FIXME BEFORE USING" << endl;

			//main_GUI._board->reset_all(true, true);
			//monte_node_buffer.remove();
			//monte_board_buffer.remove();
			//main_GUI._root_exploration_node->_is_active = true;

			//main_GUI._board = Board();
			//main_GUI._root_exploration_node = new Node();

			//main_GUI.reset_game();
		}

		// D - Affichage dans la console de tous les coups légaux de la position
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_D)) {
			//main_GUI._draw = true;
			main_GUI._board->display_moves();
			monte_board_buffer.display_buffer_state();
			monte_node_buffer.display_buffer_state();
			//cout << main_GUI._board->_positions_history.size() << endl;
		}

		// E - Évalue la position et renvoie les composantes dans la console
		if (IsKeyPressed(KEY_E)) {
			main_GUI.evaluate_position();
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
			/*if (main_GUI._board->_tested_moves > 0)
				((main_GUI._click_bind && main_GUI._board->click_m_move(main_GUI._board->_moves[main_GUI._board->best_monte_carlo_move()], main_GUI.get_board_orientation())) || true) && main_GUI._board->play_monte_carlo_move_keep(main_GUI._board->_moves[main_GUI._board->best_monte_carlo_move()], true, true, false);
			else
				cout << "no more moves are in memory" << endl;*/
			if (main_GUI._root_exploration_node->children_count() > 0)
				((main_GUI._click_bind && main_GUI._board->click_m_move(main_GUI._root_exploration_node->get_most_explored_child_move(), main_GUI.get_board_orientation())) || true) && main_GUI.play_move_keep(main_GUI._root_exploration_node->get_most_explored_child_move());
			else
				cout << "no more moves are in memory" << endl;
		}

		// LShift-P - Joue les coups recommandés par l'algorithme de GrogrosZero
		if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyDown(KEY_P)) {
			/*if (main_GUI._board->_tested_moves > 0)
				((main_GUI._click_bind && main_GUI._board->click_m_move(main_GUI._board->_moves[main_GUI._board->best_monte_carlo_move()], main_GUI.get_board_orientation())) || true) && main_GUI._board->play_monte_carlo_move_keep(main_GUI._board->_moves[main_GUI._board->best_monte_carlo_move()], true, true, false);
			else
				cout << "no more moves are in memory" << endl;*/
			if (main_GUI._root_exploration_node->children_count() > 0)
				((main_GUI._click_bind && main_GUI._board->click_m_move(main_GUI._root_exploration_node->get_most_explored_child_move(), main_GUI.get_board_orientation())) || true) && main_GUI.play_move_keep(main_GUI._root_exploration_node->get_most_explored_child_move());
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
			cout << "Zobrist key : " << main_GUI._board->_zobrist_key << endl;
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
		// if (main_GUI._board->_time) {
		//     if (main_GUI._board->_time_black < 0) {
		//         main_GUI._board->_time = false;
		//         main_GUI._board->_time_black = 0;
		//         play_end_sound();
		//         main_GUI._board->_is_game_over = true;
		//         cout << "White won on time" << endl; // Pas toujours vrai car il peut y avoir des manques de matériel
		//     }
		//     if (main_GUI._board->_time_white < 0) {
		//         main_GUI._board->_time = false;
		//         main_GUI._board->_time_white = 0;
		//         play_end_sound();
		//         main_GUI._board->_is_game_over = true;
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

		// Fait jouer l'IA automatiquement en fonction des paramètres

		// Ici on re-vérifie, car les nulles par répétition sont écourtées par l'algo
		if (main_GUI._board->_game_over_checked && main_GUI._board->_game_over_value == draw) {
			game_over:

			//cout << "Game seems to be over... or is it?" << endl;
			main_GUI._board->_game_over_checked = false;
			main_GUI._board->is_game_over(3);
			main_GUI._root_exploration_node->_iterations = 0;
			main_GUI._root_exploration_node->_chosen_iterations = 0;
			main_GUI._root_exploration_node->_is_terminal = false;
			//cout << "New game over value : " << new_game_over << endl;
			//cout << "exploration game over value : " << (int)main_GUI._root_exploration_node->_board->_game_over_value << endl;
		}

		//main_GUI._board->_game_over_checked = false;  // On recheck, pour les threefold (car dans la réflexion on dit que la partie est finie si y'a une seule répétition)
		//if (main_GUI._board->is_game_over(3) == 0) {
		if (main_GUI._board->_game_over_value == unterminated) {
			// GrogrosZero

			//cout << "test" << endl;

			// Analyse de GrogrosZero
			if (main_GUI._grogros_analysis || main_GUI._white_player == main_GUI._grogros_zero_name || main_GUI._black_player == main_GUI._grogros_zero_name) {
				main_GUI.init_buffers();
				main_GUI.grogros_analysis();
			}

			// Quand c'est son tour (TODO: fonction pour ça)
			if ((main_GUI._board->_player && main_GUI._white_player == main_GUI._grogros_zero_name) || (!main_GUI._board->_player && main_GUI._black_player == main_GUI._grogros_zero_name)) {
				main_GUI.init_buffers();
				main_GUI.play_grogros_zero_move();
			}

			if (main_GUI._board->_game_over_value != unterminated)
				goto game_over;

			// GrogrosFish (seulement lorsque c'est son tour)
			//if (main_GUI._board->_player && main_GUI._white_player.substr(0, 11) == "GrogrosFish")
			//	main_GUI._board->grogrosfish(search_depth, &eval_white, true);

			//if (!main_GUI._board->_player && main_GUI._black_player.substr(0, 11) == "GrogrosFish")
			//	main_GUI._board->grogrosfish(search_depth, &eval_black, true);

			if (main_game_over) {
				main_game_over = false;
			}

		}

		// Test pour les répétitions qui arrêtent le timer mais ne devraient pas: r1bqkb1r/pppppppp/2n5/8/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 9 8

		// Si la partie est terminée
		else {

			if (!main_game_over) {
				main_GUI._time = false;
				main_GUI._board->display_pgn();
				main_game_over = true;
			}
		}

		// Jeu automatique sur sites d'échecs
		if (main_GUI._binding_full || (main_GUI._binding_solo && main_GUI.get_board_orientation() != main_GUI._board->_player)) {
			// Le fait à chaque intervalle de temps 'binding_interval_check'
			if (clock() - main_GUI._last_binding_check > main_GUI._binding_interval_check) {

				// Mise à jour du coup joué sur le plateau
				bool got_new_move = main_GUI.update_binding_move();

				// Vérifie que le coup est légal avant de le jouer
				if (got_new_move) {
					//cout << "Binding move : " << main_GUI._root_exploration_node->_board->move_label(main_GUI._binding_move) << endl;

					main_GUI.play_move_keep(main_GUI._binding_move);

					//cout << "Binding move played" << endl;

					// Retire du temps en fonction du temps perdu par coup
					if (main_GUI._board->_player)
						main_GUI._time_white -= main_GUI._current_site._time_lost_per_move;
					else
						main_GUI._time_black -= main_GUI._current_site._time_lost_per_move;
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