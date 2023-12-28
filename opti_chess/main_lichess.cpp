#include "board.h"
#include "time_tests.h"
#include "useful_functions.h"
#include "buffer.h"
#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <thread>
#include <fstream>
#include <future>

using namespace std;

// TODO:
// Faire un .h ou qq chose qui évite le bug de devoir tout regénérer pour que ça fonctionne
// Prendre en compte le temps qu'il lui reste pour jouer
// Rajouter le lien au bot lichess dans le readme
// L'inscrire à des tournois
// Le faire réfléchir sur le temps de l'adversaire
// Pour demander le temps restant: go wtime -1 btime -1
// Rajouter d'autres paramètres (nodes, time...)

// Commande pour lancer le bot: .\venv\bin\activate
// python3 lichess-bot.py -v

// FIXME
// Vérifier que le jeu sur un autre threat n'est pas plus lent que sur le main thread


// Paramètres de Grogros
struct Param {

    // Est-ce que Grogros doit réfléchir?
    bool think = false;

    // Est-ce que Grogros doit jouer?
    bool play = false;

    // Nombre de noeuds max par réflexion
    int max_nodes = 10000;
    //int max_nodes = 250;

    // Nombre de noeuds par demande
    int nodes = 100;

    // Beta
    float beta_grogros = 0.1f;

    // K add
    float k_add = 25.0f;

    // Quiescence depth
    int quiescence_depth = 4;

    // Explore checks
    bool explore_checks = true;
};


// Variables globales

// Est-ce que un input a été reçu?
inline bool got_input = false;

// Est-ce que Grogros est en train de réfléchir?
inline bool grogros_is_running = false;

// Fonction qui joue le meilleur coup de Grogros et l'affiche
inline void bestmove(Board& board, Param& param) {
    Move best_move = board._moves[board.best_monte_carlo_move()];
    string best_move_string = board.algebric_notation(best_move);
    board.play_monte_carlo_move_keep(best_move);
    cout << "bestmove " << best_move_string << endl;
    param.play = false;
}

// Function to parse UCI commands
inline void parseUCICommand(const string& command, Param& param, Evaluator evaluator, Board& board) {
    istringstream iss(command);
    string token;

    while (iss >> token) {

        // Commandes UCI
        if (token == "uci") {

            // Présentation de Grogros
            cout << "id name Grogros" << endl;
            cout << "id author Grobert" << endl;
            cout << "uciok" << endl;
        }

        // Est-ce qu'il est prêt?
        else if (token == "isready") {
            cout << "readyok" << endl;
        }

        // Nouvelle partie
        else if (token == "ucinewgame") {
            board.from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
            param.think = true;
        }

        // Move played by the opponent
        else if (token == "position") {

            // Si c'est des coups
            if (command.substr(9, 8) == "startpos") {

                // Si c'est juste la position de départ ("position startpos")
                if (command.length() <= 18) {
					board.restart();
					continue;
				}

                // Sinon, récupère le dernier coup
                string last_move = command.substr(command.length() - 5);
                if (last_move[0] == ' ')
                    last_move = last_move.substr(1);

                // Joue le coup
                board.play_monte_carlo_move_keep(board.move_from_algebric_notation(last_move));
			}

            // C'est un FEN
            else {
                board.from_fen(command.substr(9));
			}
        }

        // Dit à Grogros de jouer
        else if (token == "go") {
            //cout << "bestmove e2e4" << endl;
            //param.think = true;
            param.play = true;
            
            //board.grogros_zero(&evaluator, param.max_nodes, param.beta_grogros, param.k_add, param.quiescence_depth, param.explore_checks);
            //bestmove(board, param);

            // Prendre en compte la suite? (movetime...)
        }

        // Dit à Grogros de s'arrêter
        else if (token == "stop") {
			param.think = false;
		}

        // Quitte le programme
        else if (token == "quit") {
            exit(0);
        }

        // Commande reçue par Grogros
        //cout << "Grogros received: " << command << endl;
    }
}

// Fonction pour récupérer les inputs
inline string GetLineFromCin() {
    string line;
    getline(cin, line);
    return line;
}

// Fonction qui renvoie si Grogros doit jouer son coup
inline bool should_play(const Board& board, Param param) {
    return (board.total_nodes() >= param.max_nodes && param.play == true);

    // TODO: prendre en compte le temps qu'il lui reste
    // TODO: prendre en compte la réflexion actuelle (voir main_gui.cpp)
}


inline void think(Board& board, Param& param, Evaluator evaluator) {

    // Tant qu'aucun input n'a été reçu
    while (!got_input) {

        // Réfléchit...
		grogros_is_running = true;
		//cout << "thinking..." << endl;
		board.grogros_zero(&evaluator, param.nodes, param.beta_grogros, param.k_add, param.quiescence_depth, param.explore_checks);
        // Affiche le nombre de noeuds
        //cout << "Grogros - total nodes: " << board.total_nodes() << endl;

        // S'il doit jouer, joue le meilleur coup
        //if (should_play(board, param)) {
        //    grogros_is_running = false;
        //    return;
        //    /*bestmove(board, param);*/
        //    //grogros_is_running = false;
        //    //return;
        //}
	}

    grogros_is_running = false;
}

// Main
inline int main_lichess() {

	// Initialisation du buffer
	monte_buffer.init(5000000, false);

    // Input
    string input;

    // Paramètres de Grogros
    Param param;

    // Evaluation de Grogros
    Evaluator evaluator;

    // Plateau
    Board board;

    // Lance le thread pour récupérer les inputs
    auto future = async(launch::async, GetLineFromCin);

    // UCI loop
    while (true) {

        // Lance la réflexion de Grogros
        //got_input = false;
        //thread grogros_thread(&think, ref(board), ref(param), ref(evaluator));
        //cout << "starting grogros thread..." << endl;
        //grogros_thread.detach();


        if (future.wait_for(chrono::seconds(0)) == future_status::ready) {
            auto input = future.get();

            // Set a new line. Subtle race condition between the previous line
            // and this. Some lines could be missed. To aleviate, you need an
            // io-only thread. I'll give an example of that as well.
            future = async(launch::async, GetLineFromCin);

            // INPUT
            if (!input.empty()) {

                // Parse l'input
                //cout << "parsing..." << endl;
                parseUCICommand(input, param, evaluator, board);

                //if (should_play(board, param))
                //    bestmove(board, param);
            }
        }

        //cout << "waiting..." << endl;
        //this_thread::sleep_for(chrono::seconds(1));

        board.grogros_zero(&evaluator, param.nodes, param.beta_grogros, param.k_add, param.quiescence_depth, param.explore_checks);

        if (should_play(board, param))
            bestmove(board, param);

        // Lit l'input
        //getline(cin, input);        

        

    }

	return 0;
}