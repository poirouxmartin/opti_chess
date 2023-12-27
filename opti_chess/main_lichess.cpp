#include "board.h"
#include "time_tests.h"
#include "useful_functions.h"
#include "buffer.h"
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

// TODO:
// Faire un .h ou qq chose qui évite le bug de devoir tout regénérer pour que ça fonctionne
// Prendre en compte le temps qu'il lui reste pour jouer
// Rajouter le lien au bot lichess dans le readme
// L'inscrire à des tournois
// Le faire réfléchir sur le temps de l'adversaire
// Pour demander le temps restant: go wtime -1 btime -1


struct Param {

    // Est-ce que Grogros doit réfléchir?
    bool think = true;

    // Nombre de noeuds max par réflexion
    int max_nodes = 10000;

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




// Function to parse UCI commands
inline void parseUCICommand(const string& command, Param param, Evaluator evaluator, Board& board) {
    istringstream iss(command);
    string token;

    while (iss >> token) {

        // Commandes UCI
        if (token == "uci") {
            // Handle UCI initialization here
            cout << "id name Grogros\n";
            cout << "id author Grobert\n";
            cout << "uciok\n";
        }

        // Est-ce qu'il est prêt?
        else if (token == "isready") {
            cout << "readyok\n";
        }

        // Nouvelle partie
        else if (token == "ucinewgame") {
            board.restart();
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

                //board.move_from_algebric_notation(last_move).display();

                // Joue le coup
                board.play_monte_carlo_move_keep(board.move_from_algebric_notation(last_move));
			}

            // C'est un FEN
            else {
                board.from_fen(command.substr(9));
			}
        }

        // Should return bestmove
        else if (token == "go") {
            param.think = true;
        }

        else if (token == "quit") {
            exit(0);
        }
    }
}


// Main
inline int main_lichess() {

	// Nombre de noeuds max pour le jeu automatique de GrogrosZero
	int grogros_nodes = 3000000;

	// Initialisation du buffer
	monte_buffer.init(5000000, false);

    string input;

    // Paramètres de Grogros
    Param param;

    // Evaluation de Grogros
    Evaluator evaluator;

    // Plateau
    Board board;

    //cout << "id name Grogros\n";

    // UCI loop
    while (true) {
        getline(cin, input);
        //cout << "input: " << input << endl;
        //cout << input.empty() << endl;

        if (!input.empty()) {
            parseUCICommand(input, param, evaluator, board);
            //cout << "fen " << board.to_fen() << endl;
        }

        // Si aucune commande n'a été entrée
        else {

            // Grogros réfléchit
            if (param.think) {
                cout << "info string thinking..." << endl;
				board.grogros_zero(&evaluator, param.nodes, param.beta_grogros, param.k_add, param.quiescence_depth, param.explore_checks);

                // S'il dépasse le nombre de noeuds max
                if (board.total_nodes() >= param.max_nodes) {
                    Move best_move = board._moves[board.best_monte_carlo_move()];
                    string best_move_string = board.algebric_notation(best_move);
                    board.play_monte_carlo_move_keep(best_move);
                    cout << "bestmove " << best_move_string << endl;
                }
				
			}

            // Grogros demande combien de temps il lui reste
        }
    }

	return 0;
}