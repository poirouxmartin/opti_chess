#include "board.h"
#include "useful_functions.h"
#include "gui.h"
#include "windows_tests.h"
#include "buffer.h"
#include "zobrist.h"
#include <io.h>
#include <fcntl.h>
#include "tests.h"

// Draws the GUI
inline void gui_draw() {
	if (!main_GUI._draw)
		return;
	BeginDrawing();
	main_GUI.draw();
	EndDrawing();
}

// Main
inline int main_ui() {
	// Write an Init function for raylib?

	// Resizable window
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	SetConfigFlags(FLAG_MSAA_4X_HINT);
	SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN);
	SetConfigFlags(FLAG_VSYNC_HINT);
	SetConfigFlags(FLAG_WINDOW_HIGHDPI);

	// To avoid printing every piece of information (the log level goes from 0 to 7 -> 7 = nothing)
	SetTraceLogLevel(LOG_ALL);

	// Initialization of the window
	InitWindow(main_GUI._screen_width, main_GUI._screen_height, "Grogros Chess");

	// Initialization of the audio
	InitAudioDevice();
	SetMasterVolume(1.0f);

	// Frames per second
	SetTargetFPS(main_GUI._max_fps);

	// Space between the text lines
	SetTextLineSpacing(4);

	// Cursor (hidden when hovering the board, see draw())
	// HideCursor();

	// Shader for the selected pieces
	main_GUI._selected_shader = LoadShader(0, "resources/shaders/outline.fs");

	int outline_size_loc = GetShaderLocation(main_GUI._selected_shader, "outlineSize");
	int outline_color_loc = GetShaderLocation(main_GUI._selected_shader, "outlineColor");
	int texture_size_loc = GetShaderLocation(main_GUI._selected_shader, "textureSize");

	// Size of the outline
	float outline_size = 16.0f;
	float outline_color[4] = { 1.0f, 0.0f, 0.0f, 0.5f };
	float texture_size[2] = { 1024.0f, 1024.0f };

	SetShaderValue(main_GUI._selected_shader, outline_size_loc, &outline_size, SHADER_UNIFORM_FLOAT);
	SetShaderValue(main_GUI._selected_shader, outline_color_loc, outline_color, SHADER_UNIFORM_VEC4);
	SetShaderValue(main_GUI._selected_shader, texture_size_loc, texture_size, SHADER_UNIFORM_VEC2);

	// Parameters of the AI
	int search_depth = 6;

	// End of the game
	bool main_game_over = false;

	// Sets the timers up
	main_GUI._board->reset_timers();

	// Sets the PGN to today's date
	main_GUI.update_date();

	// Updates the bot name of GrogrosZero
	main_GUI.update_grogros_zero_name();

	// Size of the transposition table: adaptive sizing (#13)
	const int transposition_table_size = compute_pool_sizing().tt_length;

	// Initialization of the transposition table
	transposition_table.init(transposition_table_size, nullptr, true);


	// Initialization of the Monte Carlo buffer
	main_GUI.init_buffers();

	main_GUI._board = new Board();
	main_GUI._root_exploration_node = new Node();
	main_GUI._game_tree = GameTree(*main_GUI._board);

	main_GUI.reset_game();

	// Exploration node
	// Test board: rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq - 0 2
	//test_board.from_fen("rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq - 0 2");



	// Neural network test
	Network eval_network;
	eval_network.generate_random_weights();

	// Generation of the websites
	main_GUI.init_chess_sites();



	// Main loop (quit with the cross, or with escape)
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

			// Test position for middlegame thinking: r1bq1b1r/pp4pp/2p1k3/3np3/1nBP4/2N2Q2/PPP2PPP/R1B2RK1 b - - 0 10

			// Benchmark of the evaluation function
			bool do_benchmark = false;

			if (do_benchmark) {
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
			}



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
			// r1bqk2r/ppp2ppp/1b6/nP1nP3/2P5/5P2/P5PP/RNBQKBNR b KQkq - 0 9: Nxc4? this should be seen...
			// r1bqk2r/ppp2ppp/1b6/1P1nP3/2B5/5P2/P5PP/RNBQK1NR b KQkq - 0 10: it sees nothing after Qh4?? -> no stand pat while in check!!

			//Map w_blocked_pieces = main_GUI._root_exploration_node->_board->get_all_blocked_pieces(true);
			//w_blocked_pieces.print();
			//Map b_blocked_pieces = main_GUI._root_exploration_node->_board->get_all_blocked_pieces(false);
			//b_blocked_pieces.print();

			//int short_term_piece_mobility = main_GUI._root_exploration_node->_board->get_short_term_piece_mobility(true);
			//cout << "Short term piece mobility: " << short_term_piece_mobility << endl;

			//int long_term_piece_mobility = main_GUI._root_exploration_node->_board->get_long_term_piece_mobility(true);
			//cout << "Long term piece mobility: " << long_term_piece_mobility << endl;

			// 1k6/p3r2p/1nBq2p1/2NP1nP1/5p1P/P1Q5/1PKR1P2/8 w - - 5 38: it announces #2 on Qh8+..?
			// 1k2r2Q/p6p/1nBq2p1/2NP1nP1/5p1P/P7/1PKR1P2/8 w - - 7 39: same in quiescence, it announces #2 instead of 3
			// 1k2Q3/p6p/1nBq2p1/2NP1nP1/5p1P/P7/1PKR1P2/8 b - - 0 39: here the deep eval gives -96000000 instead of -95900000 (roughly, it says -#1 instead of -#2)

			//r1b2r2/1ppqbppk/p1n1p3/3P4/1P1Pn3/P3PN1P/R1QN1PP1/2B2K1R b - - 0 14

			//cout << "Quietness: " << main_GUI._root_exploration_node->_board->get_quietness() << endl;
			
			// Move ordering test
			//main_GUI._root_exploration_node->_board->get_moves();
			//main_GUI._root_exploration_node->_board->assign_all_move_flags();
			//main_GUI._root_exploration_node->_board->sort_moves();

			//2rqr1k1/pNbnnpp1/2p1p1p1/P2pP3/Q2P4/B1P4P/4BPP1/RR4K1 b - - 6 22 : ???
			//Board b(*main_GUI._root_exploration_node->_board);
			//b._player = !b._player; // The opponent's turn
			//b.get_moves();
			//Node* stand_pat_node = new Node(&b);
			////cout << "player: " << b._player << endl;
			//stand_pat_node->quiescence(&monte_buffer, main_GUI._grogros_eval, 2, main_GUI._alpha, main_GUI._beta, -INT32_MAX, INT32_MAX, nullptr, false);
			//cout << "Stand pat eval: " << stand_pat_node->_deep_evaluation._value << endl;

			Tests tests(&main_GUI);
			tests.run_all_tests();

			//main_GUI._board->update_bitboards();
			//main_GUI._board->print_all_bitboards();
		}

		// B - Bitboards
		if (IsKeyPressed(KEY_B)) {
			main_GUI._board->print_all_bitboards();
		}

		// Q - Quiescence (worker owns the tree while running: it covers analysis)
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_A)
			&& !main_GUI._compute_running.load(std::memory_order_acquire)) {
			main_GUI._root_exploration_node->quiescence(&monte_board_buffer, main_GUI._grogros_eval, main_GUI._quiescence_depth, main_GUI._alpha, main_GUI._beta);
			main_GUI.update_snapshot(); // variants ride the snapshot now
		}

		// CTRL-T - Looks for the chess website board on screen, and starts a game
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

		// LCTRL-Q - Full binding
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

		// LCTRL-A - Automatic play mode (chess.com binding) -> checks the binding only on the opponent's moves
		// This is the only place input injection is turned on: binding alone reads the board, it never clicks.
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_A)) {
			main_GUI._binding_solo = !main_GUI._binding_solo;
			main_GUI._click_bind = !main_GUI._click_bind;
			input_injection_enabled = main_GUI._click_bind;
			std::cout << (input_injection_enabled
				? "input injection ON - Play-Computer mode only"
				: "input injection OFF") << std::endl;
		}

		// Changes to the size of the window
		if (IsWindowResized()) {
			main_GUI.get_window_size();
			//load_resources(); // Otherwise it goes blurry
			main_GUI.resize_GUI();
		}

		// S - Saves the FEN in data/text.txt
		if (IsKeyPressed(KEY_S)) {
			SaveFileText("data/test.txt", const_cast<char*>(main_GUI._current_fen.c_str()));
			cout << "saved FEN : " << main_GUI._current_fen << endl;
		}

		// L - Loads the FEN from data/text.txt. LoadFileText returns NULL when
		// the file is missing: constructing a std::string from that NULL used
		// to crash outright.
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_L)) {
			char* file_fen = LoadFileText("data/text.txt");
			if (file_fen == nullptr) {
				cout << "load FEN: data/text.txt not found" << endl;
			}
			else {
				const string fen(file_fen);
				UnloadFileText(file_fen);
				Board probe;
				probe.from_fen(fen);
				if (probe.fen_ok())
					main_GUI.load_FEN(fen);
				else
					cout << "invalid FEN ignored: " << fen << endl;
			}
		}

		// F - Flips the board
		if (IsKeyPressed(KEY_F)) {
			main_GUI.switch_orientation();
		}

		// I - #11 Plan A: toggles the TT in the main search (runtime A/B)
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_I)) {
			main_GUI._tt_main_search = !main_GUI._tt_main_search;
			cout << "TT main search : " << (main_GUI._tt_main_search ? "true" : "false") << endl;
		}

		// O - #11 Plan B: toggles the transposition DAG (runtime A/B)
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_O)) {
			main_GUI._tt_node_dag = !main_GUI._tt_node_dag;
			cout << "TT node DAG : " << (main_GUI._tt_node_dag ? "true" : "false") << endl;
		}

		// LCTRL-N - Starts a new game
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_N)) {
			main_GUI.reset_game();
		}

		// Use of the neural network
		// if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_N)) {
		//     use_neural_network = !use_neural_network;
		// }

		// C - Copies the PGN to the clipboard
		if (IsKeyPressed(KEY_C)) {
			SetClipboardText(main_GUI._global_pgn.c_str());
			cout << "copied PGN : \n" << main_GUI._global_pgn << endl;
		}

		// X - Copies the FEN to the clipboard
		if (IsKeyPressed(KEY_X)) {
			SetClipboardText(main_GUI._current_fen.c_str());
			cout << "copied FEN : " << main_GUI._current_fen << endl;
		}

		// V - Pastes the FEN from the clipboard (and loads it). Validated first:
		// an arbitrary clipboard payload used to reach the board unchecked and
		// leave a kingless (or empty) position live in the GUI.
		if (IsKeyPressed(KEY_V)) {
			const string fen = GetClipboardText();
			Board probe;
			probe.from_fen(fen);
			if (probe.fen_ok())
				main_GUI.load_FEN(fen);
			else
				cout << "invalid FEN ignored: " << fen << endl;
		}

		// // Pastes the PGN from the clipboard (and loads it)
		// if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) {
		//     string pgn = GetClipboardText();
		//     main_GUI._board->from_pgn(pgn);
		//     cout << "loaded PGN : " << pgn << endl;
		// }

		// A - Game analysis on chess.com (A)
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) {
			SetClipboardText(main_GUI._global_pgn.c_str());
			cout << "copied PGN for analysis on chess.com : \n" << main_GUI._global_pgn << endl;
			OpenURL("https://www.chess.com/analysis");
		}

		// LCTRL-A - Live game analysis on chess.com, by GrogrosZero
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

			// Put the screenshot in the clipboard?
		}

		// B - Creation of the buffer
		if (IsKeyPressed(KEY_B)) {
			cout << "available memory : " << long_int_to_round_string(get_total_system_memory()) << "b" << endl;
			main_GUI.init_buffers();
		}

		// G - GrogrosZero (hold for inline computation)
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_G)) {
			main_GUI.init_buffers();
			main_GUI.grogros_analysis(-1);
		}

		// LCTRL-G - Starts GrogrosZero in automatic search
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_G)) {
			main_GUI.init_buffers();
			main_GUI._grogros_analysis = true;
		}

		// Enter - GrogrosZero, 1 node: DEBUG
		if (IsKeyPressed(KEY_ENTER)) {
			main_GUI.init_buffers();
			main_GUI.grogros_analysis(IsKeyPressed(KEY_LEFT_SHIFT) ? 10 : 1);
		}

		// LCTRL-H - Stops the automatic search of GrogrosZero
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_H)) {
			main_GUI._grogros_analysis = false;
			main_GUI.stop_compute();
		}

		// H - Shows/hides the arrows, shows/hides the controls
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_H)) {
			main_GUI.switch_arrow_drawing();
		}

		// R - Resets the timers
		if (IsKeyPressed(KEY_R)) {
			main_GUI._board->reset_timers();
			main_GUI._time = false;
		}

		// Del - Deletes the GrogrosZero search
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_DELETE)) {
			//main_GUI._board->reset_all(true, true);
			debug_log("[key] DEL pressed");
			main_GUI.stop_compute(); // worker first: tree surgery below is not thread-safe
			std::lock_guard<std::mutex> tree_lk(main_GUI._tree_mutex);
			main_GUI._root_exploration_node->reset(true); // cycle-safe full recycle
			main_GUI.reset_buffers(); // #6: systematic TT/node_map clear
			main_GUI._root_exploration_node->_is_active = true;
			main_GUI._root_exploration_node->_board->_is_active = true;
			main_GUI._tree_snapshot.valid = false;
			main_GUI._tree_snapshot.arrows.clear();
			//main_GUI._root_exploration_node = new Node(&main_GUI._board, Move());
			cout << "Grogros's thought deleted... current moves explored: " << main_GUI._root_exploration_node->children_count() << endl;
		}

		// CTRL - Del - Deletes the Monte Carlo buffer
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

		// D - Prints every legal move of the position in the console
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_D)) {
			//main_GUI._draw = true;
			main_GUI._board->display_moves();
			monte_board_buffer.display_buffer_state();
			monte_node_buffer.display_buffer_state();
			//cout << main_GUI._board->_positions_history.size() << endl;
		}

		// E - Evaluates the position and prints the components in the console
		if (IsKeyPressed(KEY_E)) {
			main_GUI.evaluate_position();
			cout << "Evaluation : \n" << main_GUI._eval_components << endl;
		}

		// CTRL - L - Promotes the current variation to the main variation
		if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_L)) {
			//cout << main_GUI._game_tree._current_node->_move_label << endl;
			main_GUI._game_tree.promote_current_variation();
			//cout << main_GUI._game_tree._current_node->_move_label << endl;
			main_GUI._pgn = main_GUI._game_tree.tree_display();
		}

		// Keypads - set the time (in minutes)

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

		// P - Puzzle test: run current FEN, keep analysis visible, print results to console
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_P)) {
			main_GUI.run_puzzle_headless();
		}

		// LShift-P - Puzzle test with 1s budget
		if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyPressed(KEY_P)) {
			main_GUI.run_puzzle_headless(1.0);
		}

		// J - Plays the move recommended by the GrogrosZero algorithm
		if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_J)) {
			if (main_GUI._root_exploration_node->children_count() > 0)
				((main_GUI._click_bind && main_GUI._board->click_m_move(main_GUI._root_exploration_node->get_most_explored_child_move(), main_GUI.get_board_orientation())) || true) && main_GUI.play_move_keep(main_GUI._root_exploration_node->get_most_explored_child_move());
			else
				cout << "no more moves are in memory" << endl;
		}

		// LShift-J - Plays the moves recommended by the GrogrosZero algorithm
		if (IsKeyDown(KEY_LEFT_SHIFT) && IsKeyDown(KEY_J)) {
			if (main_GUI._root_exploration_node->children_count() > 0)
				((main_GUI._click_bind && main_GUI._board->click_m_move(main_GUI._root_exploration_node->get_most_explored_child_move(), main_GUI.get_board_orientation())) || true) && main_GUI.play_move_keep(main_GUI._root_exploration_node->get_most_explored_child_move());
			else
				cout << "no more moves are in memory" << endl;
		}

		// Return - Starts and stops the clock
		if (IsKeyPressed(KEY_SPACE)) {
			if (main_GUI._time)
				main_GUI.stop_time();
			else
				main_GUI.start_time();
		}

		// Z - Zobrist key of the current position
		if (IsKeyPressed(KEY_W)) {
			cout << "Zobrist key : " << main_GUI._board->_zobrist_key << endl;
			cout << main_game_over << endl;
		}

		// UP/DOWN - Enables or disables GrogrosFish for the white pieces
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

		// UP/DOWN - Enables or disables GrogrosFish for the black pieces
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

		// CTRL-UP/DOWN - Enables or disables GrogrosZero for the white pieces
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

		// CTRL-UP/DOWN - Enables or disables GrogrosZero for the black pieces
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

		// End of the game (to be reset too...) (the sound does not play...)
		// Compute the end of the game here once, to avoid doing it again?

		// Out of time... (turn this into a function)
		// if (main_GUI._board->_time) {
		//     if (main_GUI._board->_time_black < 0) {
		//         main_GUI._board->_time = false;
		//         main_GUI._board->_time_black = 0;
		//         play_end_sound();
		//         main_GUI._board->_is_game_over = true;
		//         cout << "White won on time" << endl; // Not always true, as there can be insufficient material
		//     }
		//     if (main_GUI._board->_time_white < 0) {
		//         main_GUI._board->_time = false;
		//         main_GUI._board->_time_white = 0;
		//         play_end_sound();
		//         main_GUI._board->_is_game_over = true;
		//         cout << "Black won on time" << endl;
		//     }

		// }


		// Left arrow: goes back to the previous position
		if (IsKeyPressed(KEY_LEFT)) {
			main_GUI._game_tree.select_previous_node();
		}

		// Right arrow: goes forward to the next position
		if (IsKeyPressed(KEY_RIGHT)) {
			main_GUI._game_tree.select_first_next_node();
		}



		// Play of the AIs

		// TODO: all of this needs improving, and some of it should move into the GUI

		// Makes the AI play automatically, according to the parameters

		// Checked again here, because draws by repetition are cut short by the algorithm
		if (main_GUI._board->_game_over_checked && main_GUI._board->_game_over_value == draw) {
			game_over:

			//cout << "Game seems to be over... or is it?" << endl;
			main_GUI._board->_game_over_checked = false;
			main_GUI._board->is_game_over(3);
			main_GUI._root_exploration_node->_iterations = 0;
			main_GUI._root_exploration_node->_is_terminal = false;
			//cout << "New game over value : " << new_game_over << endl;
			//cout << "exploration game over value : " << (int)main_GUI._root_exploration_node->_board->_game_over_value << endl;
		}

		//main_GUI._board->_game_over_checked = false;  // Re-checked, for the threefold case (during the search a single repetition is already called a finished game)
		//if (main_GUI._board->is_game_over(3) == 0) {
		if (main_GUI._board->_game_over_value == unterminated) {
			// GrogrosZero

			//cout << "test" << endl;

			// GrogrosZero analysis
			// Start background worker whenever continuous analysis is enabled
			if (main_GUI._grogros_analysis) {
				main_GUI.init_buffers();
				main_GUI.grogros_analysis(0);
			}

			// When it is its turn (TODO: a function for that)
			if ((main_GUI._board->_player && main_GUI._white_player == main_GUI._grogros_zero_name) || (!main_GUI._board->_player && main_GUI._black_player == main_GUI._grogros_zero_name)) {
				main_GUI.init_buffers();
				main_GUI.play_grogros_zero_move();
			}

			if (main_GUI._board->_game_over_value != unterminated)
				goto game_over;

			// GrogrosFish (only when it is its turn)
			//if (main_GUI._board->_player && main_GUI._white_player.substr(0, 11) == "GrogrosFish")
			//	main_GUI._board->grogrosfish(search_depth, &eval_white, true);

			//if (!main_GUI._board->_player && main_GUI._black_player.substr(0, 11) == "GrogrosFish")
			//	main_GUI._board->grogrosfish(search_depth, &eval_black, true);

			if (main_game_over) {
				main_game_over = false;
			}

		}

		// Test for the repetitions that stop the timer but should not: r1bqkb1r/pppppppp/2n5/8/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 9 8

		// If the game is over
		else {

			if (!main_game_over) {
				main_GUI._time = false;
				main_GUI._board->display_pgn();
				main_game_over = true;
			}
		}

		// Automatic play on chess websites
		if (main_GUI._binding_full || (main_GUI._binding_solo && main_GUI.get_board_orientation() != main_GUI._board->_player)) {
			// Done at every 'binding_interval_check' time interval
			if (clock() - main_GUI._last_binding_check > main_GUI._binding_interval_check) {

				// Update of the move played on the board
				bool got_new_move = main_GUI.update_binding_move();

				// Check that the move is legal before playing it
				if (got_new_move) {
					//cout << "Binding move : " << main_GUI._root_exploration_node->_board->move_label(main_GUI._binding_move) << endl;

					main_GUI.play_move_keep(main_GUI._binding_move);

					//cout << "Binding move played" << endl;

					// Remove time according to the time lost per move
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

	// Closing of the window
	CloseWindow();

	return 0;
}
