#include "opti_chess.h"
#include "time_tests.h"
#include "useful_functions.h"
#include "math.h"
#include "gui.h"


/* Projets

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
stoi et to_string très lents?
Ajout de variables globales plutôt que les re définir lors des appels de fonction (valeur des pièces, positionnement...)
Parallélisation -> std::for_each avec policy parallel?

Au lieu de calculer l'évaluation à chaque coup, l'implémenter en fonction du coup


Implémenter les fins de parties
-> implémenter un agent basique (pas déterministe - qui peut jouer plusieurs coups par position)
-> implémenter un algorithme de Monte Carlo 


Améliorer les heuristiques pour l'évaluation d'une position


Ajout de sons


Dans le minimax, si y'a un mat, ne plus regarder les autres coups?


PGN -> coups ambigus (si plusieurs cavaliers peuvent faire la même chose)

Quand on joue un coup, vérifie s'il est valide


Ajouter les coups légaux pour en passant/roque/promotion (seulement dame pour le moment)

Stealmate à ajouter

Chargement de FEN -> Modifier le PGN en FEN + ...
[FEN "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w - - 0 2"]
2. Nf3 Nc6 *


[FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1"]


Changer Grogrosfish : depth -> temps (--> moteur d'analyse)
Faire une profodeur -> trier les coups du meilleur au pire, puis continuer la recherche plus loin


Afficher le nombre de noeuds calculés (et le temps pris / noeuds par seconde)

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
    //t.from_fen("r2qkbnr/pp2ppp1/2b5/4p2p/4P3/P1N1B2P/1PP2PP1/R2QKB1R b KQkq - 1 10");


    // Calcul du temps de la fonction
    test_function(&test, 1);


    // Boucle principale (Quitter à l'aide de la croix, ou en faisant échap)
    while (!WindowShouldClose()) {

        if (IsKeyDown(KEY_SPACE)) {
            t.grogrosfish2(6);
            t.to_fen();
            cout << t._fen << endl;
            cout << t._pgn << endl;
        }

        if (!t._player) {
            t.grogrosfish2(6);
            t.to_fen();
            cout << t._fen << endl;
            cout << t._pgn << endl;
        }

        // if (t.game_over() == 0) {
        //     t.grogrosfish2(6);
        //     t.to_fen();
        //     cout << t._fen << endl;
        //     cout << t._pgn << endl;
        // }


        // Dessins
        BeginDrawing();

            // Couleur de fond
            ClearBackground(background_color);

            // Texte
            DrawText("Grogrosfish engine", 20, 20, 20, text_color);

            // Dessin du plateau
            t.draw();
            

            // Sound fxWav = LoadSound("resources/sound.wav");
            // PlaySound(fxWav);
            // UnloadSound(fxWav);


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