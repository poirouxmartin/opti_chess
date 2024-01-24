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
#include "zobrist.h"

using namespace std;

// TODO:
// Rajouter le lien au bot lichess dans le readme
// L'inscrire à des tournois
// Rajouter d'autres paramètres (nodes, time...)
// Afficher l'eval si demandée? profondeur?...
// Faire parler Grogros dans la partie? en fonction de la partie en cours
// Voir les TODO dans le code
// Regarder si should_play prend trop de temps
// Afficher le nombre de noeuds par seconde à la fin de la réflexion?
// Tester d'autres paramètres d'exploration (exploration plus restreinte (beta plus grand), quiescence plus profonde...)

// COMMANDS:
// cd .\Documents\Info\Echecs\opti_chess\lichess-bot\
// pour lancer le bot: .\venv\bin\activate
// python3 lichess-bot.py -v

// Bilans de parties:
// Trop d'importance sur les colonnes des tours : FIXED
// Trop d'échecs inutiles (batteries dame/fou -> Fh7+ -> Fou enfermé) : FIXED
// Manque d'importance pour les pions au centre? : FIXED
// Trop de c3 inutiles : FIXED
// Trop de sacrifices inutiles sur le roi adverse : FIXED
// Bloque pas assez les pions passés
// Donne trop de pions gratuitement : FIXED?
// Mauvaise modélisation de la sécurité du roi (agglutine les pièces vers le roi inutilement en fin de partie)
// Ne roque pas assez vite : FIXED
// Enfermement des pièces (fous/dames)
// Donne des pièces trop facilement contre quelques pions
// 1. e4 Cc6 2. d4 e5 3. d5 Cd4 4. c3 -> Gagne la pièce, et Grogros s'en fout un peu (eval: +7 pour les blancs...)
// r1bq2k1/pppp2rp/2n3P1/3N1p1Q/2PP4/3pP3/PP3PP1/R3K2R w KQ - 1 16 : ici il faut surtout pas prendre h7 (ça ferme la colonne h)
// 5r2/ppp5/3p1nk1/8/4P2R/5PP1/PP6/1K6 w - - 0 35 : ici faut pas abuser avec les ionps... le cheval reste plus fort (et faut pas échanger les tours) -> il faut implémenter winnable pour échanger les pions mais pas les pièces
// Valeurs des structures de pions à revoir...
// Gestion du temps aussi (voir fins de parties)
// Trop de scandi et de d4 Cc3 (ou c3) plutôt que Cf3 ou c4
// King safety à revoir absolument... weak squares trop importants (scandi qui prend le pion g2, weak squares quasi -1...)



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

    // Si il nous reste beaucoup de temps en fin de partie, on peut réfléchir plus longtemps
    max_move_time *= (1 + board._adv); // Regarder si ça marche bien (TODO)

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

    // Taille du buffer
    static constexpr int buffer_size = 5000000;

    // Taille de la table de transposition
    static constexpr int transposition_table_size = 5000000;

	// Initialisation du buffer
	monte_buffer.init(buffer_size, false);

    // Initialisation de la table de transposition
    transposition_table.init(transposition_table_size);

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

        if (should_play(board, param))
            bestmove(board, param);
    }

	return 0;
}