#include "opti_chess.h"
#include "time_tests.h"
#include "useful_functions.h"
#include "math.h"
#include "gui.h"


/* Projets


6nk/4b3/5nKp/4N3/8/8/8/8 b - - 0 1
FEN à garder, pour tester les règles de chess.com... que se passe t-il si les noirs tombent au temps? y'a t-il nulle?
4n1nk/4b3/6Kp/8/6N1/8/8/8 b - - 0 1 -> Nef6 -> Ne5 -> ... time over -> draw on time + lack of material?

6bk/2n5/6K1/8/5B2/8/8/8 b - - 0 1



En passant
Echecs?
Promotions
Roque
Rajouter des tests de validité de FEN

Séparer les fonctions du fichier opti_chess dans d'autres fichiers (GUI, IA...)


Optimisations à implémenter

Regarder quel compilateur est le plus rapide/opti
Algo Negamax -> copies des tableaux -> undo moves? ---> finalement plus lent?
Sort moves ?? fonctionne ? ---> alors utiliser optimisation vers negascout
Transposition tables
Algorithme MTD(f) <-> alphabeta with memory algorithm
stoi et to_string très lents?
Ajout de variables globales plutôt que les re définir lors des appels de fonction (valeur des pièces, positionnement...)
Parallélisation -> std::for_each avec policy parallel?

Au lieu de calculer l'évaluation à chaque coup, l'implémenter en fonction du coup


Implémenter les fins de parties
-> implémenter un agent basique (pas déterministe - qui peut jouer plusieurs coups par position)
-> implémenter un algorithme de Monte Carlo 


Améliorer les heuristiques pour l'évaluation d'une position
- Positionnement du roi changeant au cours de la partie
- Activité des pièces
- Sécurité du roi
- Espace
- Structures de pions


Amélioration des sons


Dans le negamax, si y'a un mat, ne plus regarder les autres coups?

Quand on joue un coup, vérifie s'il est valide


Ajouter les coups légaux pour en passant/roque/promotion (seulement dame pour le moment)

Stealmate à ajouter

Chargement de FEN -> Modifier le PGN en FEN + ...
[FEN "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w - - 0 2"]
2. Nf3 Nc6 *


[FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1"]


Changer Grogrosfish : depth -> temps (--> moteur d'analyse)
Faire une profodeur -> trier les coups du meilleur au pire, puis continuer la recherche plus loin



Livres d'ouvertures, tables d'engame?


Correction PGN -> fins de parties

Ne plus autoriser les coups illégaux dans la console?

Optimiser le sort moves - l'améliorer (et l'évaluation?)

Montrer en direct sur la GUI l'avancement de l'IA


Evaluation dans le plateau -> incrémentation de l'évaluation lors de coups, plutôt que calcul de zéro

Remplacer des if par des &&



Vérifier que toutes les fonctions sont optimisées


Faire le triage des coups grace aux itérations précédentes?

Fonction pour stocker facilement un noeud, ou savoir s'il est similaire à un autre? -> transposition tables

Iterative deeping



Augmenter la profondeur pour les finales

Undo move dans l'interface, avec les flèches


Approche de Monte Carlo

Faire un agent qui gagne toujours contre un autre


Tourner l'échiquier

Nouveau sons/images

Afficher quel coup l'ordinateur est en train de refléchir

Afficher tous les coups possibles

Surligner avec le clic droit

Quand une tour ou le roi bouge, retire le roque

Stealmates



*/


// Test function
void test() {


    // Board t1, t2;
    // t2.copy_data(t1);


    Board t;
    //t.grogrosfish(6);
    //t.grogrosfish2(6);
    //t.sort_moves();


}








int main() {

    // Initialisation de la fenêtre
    InitWindow(screen_width, screen_height, "Opti chess");

    // Initialisation de l'audio
    InitAudioDevice();

    // Nombre d'images par secondes
    SetTargetFPS(fps);



    // Variables
    Board t;
    //t.from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    //t.from_fen("r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4");
    //t.from_fen("r4rk1/p1p3pp/4qp2/1Rbpn2b/8/2P2N1P/P1P1BPP1/2BQR1K1 b - - 1 15");
    //t.from_fen("rnbqkbnr/pppp1ppp/8/4p3/4PP2/8/PPPP2PP/RNBQKBNR b KQkq - 0 2");


    // Calcul du temps de la fonction
    test_function(&test, 1);


    // Test de paramètres
    float test_parameters[3] {1, 0.1, 0.025};
    float test_begin_parameters[3] {1, 0.1, 0.01};
    float test_end_parameters[3] {1, 0.1, 0.03};
    int n_agents = 100;


    // IA self play
    bool self_play = false;


    // Boucle principale (Quitter à l'aide de la croix, ou en faisant échap)
    while (!WindowShouldClose()) {

        if (IsKeyDown(KEY_SPACE)) {
            t.grogrosfish2(6, test_parameters);
            t.to_fen();
            cout << t._fen << endl;
            cout << t._pgn << endl;
        }

        if (IsKeyDown(KEY_B)) {
            t.grogrosfish3(6);
            t.to_fen();
            cout << t._fen << endl;
            cout << t._pgn << endl;
        }

        if (IsKeyDown(KEY_V)) {
            t.grogrosfish4(6);
            t.to_fen();
            cout << t._fen << endl;
            cout << t._pgn << endl;
        }

        if (IsKeyDown(KEY_T)) {
            t.grogrosfish_multiagents(6, n_agents, test_begin_parameters, test_end_parameters);
            t.to_fen();
            cout << t._fen << endl;
            cout << t._pgn << endl;
        }


        // if (!t._player) {
        //     t.grogrosfish2(6, test_parameters);
        //     t.to_fen();
        //     cout << "last move" << t._last_move[0] << ", " << t._last_move[1] << endl;
        //     cout << t._fen << endl;
        //     cout << t._pgn << endl;
        // }

        // else {
        //     t.grogrosfish_multiagents(4, n_agents, test_begin_parameters, test_end_parameters);
        //     t.to_fen();
        //     cout << t._fen << endl;
        //     cout << t._pgn << endl;
        // }

        // if (IsKeyDown(KEY_T)) {
        //     t.to_fen();
        //     cout << t._fen << endl;
        //     cout << t._pgn << endl;
        //     t.get_moves();
        //     t.display_moves();
        // }

        if (IsKeyDown(KEY_G))
            self_play = true;

        if (self_play && t.game_over() == 0) {
            t.grogrosfish2(6, test_parameters);
            //t.grogrosfish_multiagents(4, n_agents, test_begin_parameters, test_end_parameters);
            t.to_fen();
            cout << t._fen << endl;
            cout << t._pgn << endl;
        }


        /*if (!IsWindowFullscreen())
            ToggleFullscreen();*/


        // Dessins
        BeginDrawing();

            // Dessin du plateau
            t.draw();
            
        // Fin de la zone de dessin
        EndDrawing();
    

    }

    // Fermeture de la fenêtre
    CloseWindow();


    return 0;

}









/*


// Main
int main() {



    Board t;
    //t.from_fen("rnbqkbnr/pppp1ppp/8/4p2Q/4P3/8/PPPP1PPP/RNB1KBNR b KQkq - 1 2");
    //t._pgn = "1. e4 e5 2. Qh5";
    //t.from_fen("3qkbnr/3n4/5p2/1B4p1/3B3p/4NQ2/P1PP1PPP/K6R w KQkq - 48 58");
    t.display();
    //t.grogrosfish(2);
    //t.display();


    // Calcul du temps de la fonction
    test_function(&test, 1);

    t.to_fen();

    cout << "FEN : " << t._fen << endl;

    //cout << "move : " << t.move_label(1, 4, 6, 5) << endl;


    // for (int i = 0; i < 5; i++) {
    //     t.grogrosfish(6);
    //     t.display();
    //     cout << "PGN : " << t._pgn << endl;
    // }
    

    //cout << "PGN : " << t._pgn << endl;

    // cout << "end : " << t.game_over() << endl;



    // int iter = 0;

    // while (t.game_over() == 0 && iter < 100) {
    //     cout << "grogrosfish..." << endl;
    //     if (t._color)
    //         t.grogrosfish(6);
    //     else
    //         t.grogrosfish(6);

    //     t.display();
    //     t.evaluate();

    //     t.to_fen();
    //     cout << "FEN : " << t._fen << endl;
    //     cout << "Evaluation : " << t._evaluation << endl;
    //     cout << "End : " << t.game_over() << endl;
    //     cout << "PGN : " << t._pgn << endl;

    //     iter += 1;
    // }




    // t.get_moves();

    // int i1, j1, p1, i2, j2, p2, h;
    // int i = 0;

    // i1 = t._moves[4 * i];
    // j1 = t._moves[4 * i + 1];
    // p1 = t._array[i1][j1];
    // i2 = t._moves[4 * i + 2];
    // j2 = t._moves[4 * i + 3];
    // p2 = t._array[i2][j2];
    // h = t._half_moves_count;

    // t.make_index_move(i);

    // t.display();

    // t.undo(i1, j1, p1, i2, j2, p2, h);

    // t.display();


    // t.get_moves();
    // t.display_moves();

    // t.from_fen("rnbqkbnr/pppp1ppp/8/4p2Q/4P3/8/PPPP1PPP/RNB1KBNR b KQkq - 1 2");
    // t.display();

    // t.to_fen();
    // cout << "FEN : " << t._fen << endl;

    // t.grogrosfish(6);
    // t.display();


    // for (int i = 0; i < 100; i++) {
    //     t.grogrosfish(6);
    //     t.display();
    //     t.to_fen();
    //     cout << "FEN : " << t._fen << endl;
    // }

    // Position finale : rnbqkbnr/1ppp1ppp/8/4p3/p7/N7/PPPPPPPP/R1BQKBNR w KQkq - 0 6



    //Joue contre l'IA
    //t.from_fen("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2");
    t.display();
    int depth = -1;
    int move[4];
    while (depth != 0) {
        cout << "your move : ";
        cin >> move[0] >> move[1] >> move[2] >> move[3];
        t.make_move(move[0], move[1], move[2], move[3]);
        t.display();
        cout << "depth : ";
        cin >> depth;
        if (depth == 0)
            break;
        t.grogrosfish(depth);
        t.display();
        t.to_fen();
        cout << "FEN : " << t._fen << endl;
        cout << "PGN : " << t._pgn << endl;
    }


    return 0;


}



*/