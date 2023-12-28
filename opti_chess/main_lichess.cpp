#include "board.h"
#include "time_tests.h"
#include "useful_functions.h"
#include "buffer.h"
#include <iostream>
#include <sstream>
#include <string>
#include <chrono>

using namespace std;

// TODO:
// Faire un .h ou qq chose qui �vite le bug de devoir tout reg�n�rer pour que �a fonctionne
// Prendre en compte le temps qu'il lui reste pour jouer
// Rajouter le lien au bot lichess dans le readme
// L'inscrire � des tournois
// Le faire r�fl�chir sur le temps de l'adversaire
// Pour demander le temps restant: go wtime -1 btime -1

// Commande pour lancer le bot: python3 lichess-bot.py -u

// FIXME
// V�rifier que le jeu sur un autre threat n'est pas plus lent que sur le main thread


// Param�tres de Grogros
struct Param {

    // Est-ce que Grogros doit r�fl�chir?
    bool think = false;

    // Est-ce que Grogros doit jouer?
    bool play = false;

    // Nombre de noeuds max par r�flexion
    int max_nodes = 25000;

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

    // Pour g�rer les threads
    //bool wait = false;
};


// Variables globales

// Est-ce que un input a �t� re�u?
inline bool got_input = false;

// Est-ce que Grogros est en train de r�fl�chir?
inline bool grogros_is_running = false;


// Function to parse UCI commands
inline void parseUCICommand(const string& command, Param& param, Evaluator evaluator, Board& board) {
    istringstream iss(command);
    string token;

    while (iss >> token) {

        // Commandes UCI
        if (token == "uci") {

            // Pr�sentation de Grogros
            cout << "id name Grogros\n";
            cout << "id author Grobert\n";
            cout << "uciok\n";
        }

        // Est-ce qu'il est pr�t?
        else if (token == "isready") {
            cout << "readyok\n";
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

                // Si c'est juste la position de d�part ("position startpos")
                if (command.length() <= 18) {
					board.restart();
					continue;
				}

                // Sinon, r�cup�re le dernier coup
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

        // Dit � Grogros de jouer
        else if (token == "go") {
            //param.think = true;
            param.play = true;

            // Prendre en compte la suite? (movetime...)
        }

        // Dit � Grogros de s'arr�ter
        else if (token == "stop") {
			param.think = false;
		}

        // Quitte le programme
        else if (token == "quit") {
            exit(0);
        }

        // Commande re�ue par Grogros
        //cout << "Grogros received: " << command << endl;
    }
}

// Fonction pour r�cup�rer les inputs
inline void getinput(string& input) {
	getline(cin, input);
}

// Fonction qui joue le meilleur coup de Grogros et l'affiche
inline void bestmove(Board& board, Param& param) {
	Move best_move = board._moves[board.best_monte_carlo_move()];
	string best_move_string = board.algebric_notation(best_move);
	board.play_monte_carlo_move_keep(best_move);
    cout << "bestmove " << best_move_string << endl;
    param.play = false;
}

// Fonction qui renvoie si Grogros doit jouer son coup
inline bool should_play(const Board& board, Param param) {
    return (board.total_nodes() >= param.max_nodes && param.play == true);

    // TODO: prendre en compte le temps qu'il lui reste
    // TODO: prendre en compte la r�flexion actuelle (voir main_gui.cpp)
}


inline void think(Board& board, Param& param, Evaluator evaluator) {

    // Tant qu'aucun input n'a �t� re�u
    while (!got_input) {

        // R�fl�chit...
		grogros_is_running = true;
		//cout << "thinking..." << endl;
		board.grogros_zero(&evaluator, param.nodes, param.beta_grogros, param.k_add, param.quiescence_depth, param.explore_checks);

        // S'il doit jouer, joue le meilleur coup
        if (should_play(board, param)) {
            bestmove(board, param);
            //grogros_is_running = false;
            //return;
        }
	}

    grogros_is_running = false;
}

// Main
inline int main_lichess() {

	// Initialisation du buffer
	monte_buffer.init(5000000, false);

    // Input
    string input;

    // Param�tres de Grogros
    Param param;

    // Evaluation de Grogros
    Evaluator evaluator;

    // Plateau
    Board board;


    // UCI loop
    while (true) {


        got_input = false;
        thread grogros_thread(&think, ref(board), ref(param), ref(evaluator));
        //cout << "starting grogros thread..." << endl;
        grogros_thread.detach();

        // Lit l'input
        getline(cin, input);

        

        // INPUT
        if (!input.empty()) {

            // On a re�u un input
            got_input = true;

            // Tant que Grogros est en train de r�fl�chir, attend
            while (grogros_is_running) {
				//cout << "waiting for Grogros to finish thinking..." << endl;
                this_thread::sleep_for(chrono::milliseconds(10));
			}

            // D�truit le thread
            grogros_thread.~thread();

            // Parse l'input
            //cout << "parsing..." << endl;
            parseUCICommand(input, param, evaluator, board);
        }

    }

	return 0;
}