#include "board.h"
#include "useful_functions.h"
#include "buffer.h"
#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <thread>
#include <fstream>
#include <future>
#include "zobrist.h"

using namespace std;

// TODO:
// Add the link to the lichess bot in the readme
// Enter it in tournaments
// Add other parameters (nodes, time...)
// Display the evaluation when asked? the depth?...
// Make Grogros talk during the game? depending on how the game is going
// Look at the TODOs in the code
// Check whether should_play takes too long
// Display the nodes per second at the end of the thinking time?
// Try other exploration parameters (narrower exploration (larger beta), deeper quiescence...)

// COMMANDS:
// cd .\Documents\Info\Echecs\opti_chess\lichess-bot\
// to start the bot: .\venv\bin\activate
// python3 lichess-bot.py -v

// Game reviews:
// Too much weight on the rook files: FIXED
// Too many pointless checks (queen/bishop batteries -> Bh7+ -> trapped bishop): FIXED
// Not enough weight on the central pawns? FIXED
// Too many pointless c3 moves: FIXED
// Too many pointless sacrifices on the opposing king: FIXED
// Does not blockade passed pawns enough
// Gives away too many pawns for free: FIXED?
// Poor model of king safety (piles the pieces up around the king for nothing in the endgame)
// Does not castle quickly enough: FIXED
// Traps its own pieces (bishops/queens)
// Gives pieces away too easily for a couple of pawns
// 1. e4 Nc6 2. d4 e5 3. d5 Nd4 4. c3 -> wins the piece, and Grogros barely cares (eval: +7 for White...)
// r1bq2k1/pppp2rp/2n3P1/3N1p1Q/2PP4/3pP3/PP3PP1/R3K2R w KQ - 1 16: here h7 must absolutely not be taken (it closes the h-file)
// 5r2/ppp5/3p1nk1/8/4P2R/5PP1/PP6/1K6 w - - 0 35: here the pawns must not be overrated... the knight stays stronger (and the rooks should not be traded) -> winnable has to be implemented, to trade the pawns but not the pieces
// Pawn structure values to be revisited...
// Time management too (see the endgames)
// Too many Scandinavians and d4 Nc3 (or c3) rather than Nf3 or c4
// King safety definitely to be revisited... weak squares weigh too much (a Scandinavian taking the g2 pawn, weak squares near -1...)


// BUG:
// It sometimes crashes (when there are repetitions...)


// Grogros parameters
struct Param {

    // Should Grogros play?
    bool play = false;

    // Number of nodes per request
    int nodes = 50;

    // Beta
    float beta_grogros = 0.1f;

    // K add
    float k_add = 25.0f;

    // Quiescence depth
    int quiescence_depth = 4;

    // Explore checks
    bool explore_checks = true;

    // Time left (in ms)
    int time_white = 600000;
    int time_black = 600000;

    // Clock at the start of the thinking time
    clock_t clock_start = 0;
};


// Plays Grogros's best move and displays it
inline void bestmove(Board& board, Param& param) {
    //Move best_move = board._moves[board.best_monte_carlo_move()];
    //string best_move_string = board.algebric_notation(best_move);
    //board.play_monte_carlo_move_keep(best_move);
    //cout << "bestmove " << best_move_string << endl;
    
    //cout << "test" << board._positions_history.size() << endl;
    //board.display_positions_history();
    //cout << "eval: " << board._evaluation << endl;
    //cout << "repetitions: " << board.repetition_count() << endl;
    param.play = false;
}

// Function to parse UCI commands
inline void parseUCICommand(const string& command, Param& param, Evaluator evaluator, Board& board) {
    istringstream iss(command);
    string token;

    while (iss >> token) {

        // UCI commands
        if (token == "uci") {

            // Introduction of Grogros
            cout << "id name Grogros" << endl;
            cout << "id author Grobert" << endl;
            cout << "uciok" << endl;
        }

        // Is it ready?
        else if (token == "isready") {
            cout << "readyok" << endl;
        }

        // New game
        else if (token == "ucinewgame") {
            board.from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        }

        // Move played by the opponent
        else if (token == "position") {

            // Read the last move
            string last_move = command.substr(command.length() - 5);
            if (last_move[0] == ' ')
                last_move = last_move.substr(1);
            Move move = board.move_from_algebric_notation(last_move);

            // If the move is playable, play it
            if (board.is_legal(move)) {
                //board.play_monte_carlo_move_keep(move);
                board._game_over_checked = false;
                board.is_game_over(3);
                //cout << "test" << board._positions_history.size() << endl;
            }

            // Otherwise, take the starting position or the FEN
            else {
                //cout << "Illegal move by opponent: " << last_move << "; in position: " << board.to_fen() << endl;

                // Starting position
                if (command.substr(9, 8) == "startpos") {
                    board.restart();
                }

                // FEN
                else {
                    board.from_fen(command.substr(9));
                }
            }


   //         // If these are moves
   //         if (command.substr(9, 8) == "startpos") {

   //             // If it is just the starting position ("position startpos")
   //             if (command.length() <= 18) {
			//		board.restart();
			//		continue;
			//	}

   //             // Otherwise, read the last move
   //             string last_move = command.substr(command.length() - 5);
   //             if (last_move[0] == ' ')
   //                 last_move = last_move.substr(1);

   //             // Play the move
   //             board.play_monte_carlo_move_keep(board.move_from_algebric_notation(last_move));
   //             cout << "test" << board._positions_history.size() << endl;
			//}

   //         // It is a FEN
   //         else {
   //             board.from_fen(command.substr(9));
			//}
        }

        // Tell Grogros to play
        else if (token == "go") {
            param.play = true;

            // Update the time left
            // For instance: go wtime 100000 btime 100000
            while (iss >> token) {
                if (token == "wtime") {
					iss >> token;
					param.time_white = stoi(token);
				}
                else if (token == "btime") {
					iss >> token;
					param.time_black = stoi(token);
				}
			}

            param.clock_start = clock();

            // TODO: to be checked


            // TODO: take the rest into account? (movetime...)
        }

        // Tell Grogros to stop
        else if (token == "stop") {
		}

        // Quit the program
        else if (token == "quit") {
            exit(0);
        }
    }
}

// Reads the inputs
inline string GetLineFromCin() {
    string line;
    getline(cin, line);
    return line;
}

// Returns whether Grogros should play its move
inline bool should_play(const Board& board, Param param) {
    
    // Number of nodes Grogros is assumed to compute per second
    static constexpr int supposed_grogros_speed = 3500;

    // Number of nodes already computed
    //int tot_nodes = board.total_nodes();

    // Share of the thinking time spent on the best move
    //float best_move_percentage = tot_nodes == 0 ? 0.05f : static_cast<float>(board._nodes_children[board.best_monte_carlo_move()]) / static_cast<float>(tot_nodes);
    float best_move_percentage = 0;

    // Update of the thinking time
    if (board._player) {
		param.time_white -= (clock() - param.clock_start) * 1000 / CLOCKS_PER_SEC;
		param.clock_start = clock();
	}
    else {
		param.time_black -= (clock() - param.clock_start) * 1000 / CLOCKS_PER_SEC;
		param.clock_start = clock();
	}
    
    // Time we want to spend on this move
    int max_move_time = board._player ?
        time_to_play_move(param.time_white, param.time_black, 0.2f * (1.0f - best_move_percentage)) :
        time_to_play_move(param.time_black, param.time_white, 0.2f * (1.0f - best_move_percentage));

    // If there is a lot of time left in the endgame, we can think longer
    max_move_time *= (1 + board._adv); // Check whether this works well (TODO)

    // Equivalent in number of nodes
    int nodes_to_play = supposed_grogros_speed * max_move_time / 1000;

    // We want to be sure to play Grogros's best move
    // If there is a better move than the one with the most nodes, wait...
    bool wait_for_best_move = false;
    //bool wait_for_best_move = tot_nodes != 0 && board._eval_children[board.best_monte_carlo_move()] * board.get_color() < board._evaluation * board.get_color();
    nodes_to_play = wait_for_best_move ? nodes_to_play : nodes_to_play / 4; // FIXME: the wait could depend on the evaluation gap between the best move and the most searched one

    //cout << "nodes to play: " << nodes_to_play << endl;
    return true;
    //return tot_nodes >= nodes_to_play && param.play == true;


    // TODO: play the node count of the next iteration based on an estimate of the nodes left
    //int grogros_timed_nodes = min(nodes_per_frame, supposed_grogros_speed * max_move_time / 1000);

    // TODO: take the increment into account
}

// Main
inline int main_lichess() {

    // Size of the buffer
    static constexpr int buffer_size = 5000000;

    // Size of the transposition table
    static constexpr int transposition_table_size = 5000000;

	// Initialization of the buffer
	monte_board_buffer.init(buffer_size, false);

    // Initialization of the transposition table
    transposition_table.init(transposition_table_size);

    // Input
    string input;

    // Grogros parameters
    Param param;

    // Grogros evaluation
    Evaluator evaluator;

    // Board
    Board board;

    // Start the thread that reads the inputs
    auto future = async(launch::async, GetLineFromCin);

    // UCI loop
    while (true) {

        // Asynchronous input
        if (future.wait_for(chrono::seconds(0)) == future_status::ready) {
            auto input = future.get();

            future = async(launch::async, GetLineFromCin);

            // INPUT
            if (!input.empty()) {
                parseUCICommand(input, param, evaluator, board);
            }
        }

        // Grogros thinks while waiting
        //board.grogros_zero(&evaluator, param.nodes, param.beta_grogros, param.k_add, param.quiescence_depth, param.explore_checks);

        if (should_play(board, param))
            bestmove(board, param);
    }

	return 0;
}