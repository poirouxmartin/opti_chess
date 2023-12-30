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
// Rajouter d'autres paramètres (nodes, time...)
// Afficher l'eval si demandée? profondeur?...
// Faire parler Grogros dans la partie?
// Voir les TODO dans le code
// Demander le temps plusieurs fois pour uptdate pendant la réflexion
// Regarder si should_play prend trop de temps
// Prendre son temps sur le premier coup

// COMMANDS:
// pour lancer le bot: .\venv\bin\activate
// python3 lichess-bot.py -v

// FIXME
// Parfois, Grogros fait b1c3 dans certaines positions (coup illegal)
// test coups illégaux: r1q1kb1r/pb3pp1/1pn1pn1p/2ppN3/Q2P1B2/2P3P1/PP2PPBP/RN3RK1 w kq - 0 10
// 'c2c3' in rn3rk1/7p/1p1N1pp1/p2p3n/P1pP4/2P3Bq/1P2QP2/RN2R1K1 b - - 3 22
// 'c2c3' in r4rk1/1p3pp1/p3bb1p/1N6/2p2P2/2P1P1P1/PP4BP/R4RK1 w - - 0 20
// 'c2c3' in 1rb3k1/3p1ppp/1p1N1n2/pP6/5P2/2PnPB2/P5PP/R1B3K1 w - - 3 24
// EDIT: Grogros plante, et revient sur la position de base, et joue le coup illégal

// FIXME: il faudrait que Grogros reprenne sur la bonne position après un crash

// Bilans de parties:
// Trop d'importance sur les colonnes des tours : FIXED
// Trop d'échecs inutiles (batteries dame/fou -> Fh7+ -> Fou enfermé) : FIXED
// Manque d'importance pour les pions au centre? : FIXED
// Trop de c3 inutiles : FIXED
// Trop de sacrifices inutiles sur le roi adverse : FIXED
// Bloque pas assez les pions passés
// Donne trop de pions gratuitement : FIXED?
// Mauvaise modélisation de la sécurité du roi (agglutine les pièces vers le roi inutilement en fin de partie)
// Ne roque pas assez vite


// Paramètres de Grogros
struct Param {

    // Est-ce que Grogros doit jouer?
    bool play = false;

    // Nombre de noeuds par demande
    int nodes = 50;

    // Beta
    float beta_grogros = 0.1f;

    // K add
    float k_add = 25.0f;

    // Quiescence depth
    int quiescence_depth = 4;

    // Explore checks
    bool explore_checks = true;

    // Temps restant (en ms)
    int time_white = 600000;
    int time_black = 600000;

    // Clock au début de la réflexion
    clock_t clock_start = 0;
};


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
            param.play = true;

            // Met à jour le temps restant
            // Exemple: go wtime 100000 btime 100000
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

            // TODO: à vérifier


            // TODO: Prendre en compte la suite? (movetime...)
        }

        // Dit à Grogros de s'arrêter
        else if (token == "stop") {
		}

        // Quitte le programme
        else if (token == "quit") {
            exit(0);
        }
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
    
    // Nombre de noueds que l'on suppose que Grogros va calculer par seconde
    static constexpr int supposed_grogros_speed = 3500;

    // Nombre de noeuds déjà calculés
    int tot_nodes = board.total_nodes();

    // Pourcentage de réflexion utilisé pour le meilleur coup
    float best_move_percentage = tot_nodes == 0 ? 0.05f : static_cast<float>(board._nodes_children[board.best_monte_carlo_move()]) / static_cast<float>(tot_nodes);

    // Mise à jour du temps de réflexion
    if (board._player) {
		param.time_white -= (clock() - param.clock_start) * 1000 / CLOCKS_PER_SEC;
		param.clock_start = clock();
	}
    else {
		param.time_black -= (clock() - param.clock_start) * 1000 / CLOCKS_PER_SEC;
		param.clock_start = clock();
	}
    
    // Temps que l'on veut passer sur ce coup
    int max_move_time = board._player ?
        time_to_play_move(param.time_white, param.time_black, 0.2f * (1.0f - best_move_percentage)) :
        time_to_play_move(param.time_black, param.time_white, 0.2f * (1.0f - best_move_percentage));

    // Equivalent en nombre de noeuds
    int nodes_to_play = supposed_grogros_speed * max_move_time / 1000;

    // On veut être sûr de jouer le meilleur coup de Grogros
    // Si il y a un meilleur coup que celui avec le plus de noeuds, attendre...
    bool wait_for_best_move = tot_nodes != 0 && board._eval_children[board.best_monte_carlo_move()] * board.get_color() < board._evaluation * board.get_color();
    nodes_to_play = wait_for_best_move ? nodes_to_play : nodes_to_play / 4; // FIXME: on peut attendre en fonction de la différence d'évaluation entre le meilleur coup et le coup le plus réfléchi

    //cout << "nodes to play: " << nodes_to_play << endl;
    return tot_nodes >= nodes_to_play && param.play == true;


    // TODO: jouer le nombre de noeuds à la prochaine itération en fonction de l'estimation du nombre de noeuds restants
    //int grogros_timed_nodes = min(nodes_per_frame, supposed_grogros_speed * max_move_time / 1000);

    // TODO: prendre en compte l'incrément
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

        // Input en asynchrone
        if (future.wait_for(chrono::seconds(0)) == future_status::ready) {
            auto input = future.get();

            future = async(launch::async, GetLineFromCin);

            // INPUT
            if (!input.empty()) {
                parseUCICommand(input, param, evaluator, board);
            }
        }

        // Grogros réfléchit en attendant
        board.grogros_zero(&evaluator, param.nodes, param.beta_grogros, param.k_add, param.quiescence_depth, param.explore_checks);
        //cout << "info nodes " << board.total_nodes() << endl;

        if (should_play(board, param))
            bestmove(board, param);
    }

	return 0;
}