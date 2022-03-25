#include "opti_chess.h"
#include "time_tests.h"
#include "useful_functions.h"
#include "math.h"
#include "gui.h"


/* TODO


----- Jeu -----

-> Echecs?
-> Promotions (de tous types plutôt que seulement dame)
-> Rajouter des tests de validité de FEN
-> Répétition de coups
-> Pat à ajouter



----- Structure globale du projet -----

-> Séparer les fonctions du fichier opti_chess dans d'autres fichiers (GUI, IA...)



----- Optimisations à implémenter -----

-> Regarder quel compilateur est le plus rapide/opti
-> Algo Negamax -> copies des tableaux -> undo moves? ---> finalement plus lent?
-> Transposition tables
-> Algorithme MTD(f) <-> alphabeta with memory algorithm
-> stoi et to_string très lents?
-> Ajout de variables globales plutôt que les re définir lors des appels de fonction (valeur des pièces, positionnement...)
-> Parallélisation -> std::for_each avec policy parallel?
-> Au lieu de calculer l'évaluation à chaque coup, l'implémenter en fonction du coup
-> Dans le negamax, si y'a un mat, ne plus regarder les autres coups?
-> Optimiser le sort moves - l'améliorer (et l'évaluation?)
-> Remplacer des if par des &&
-> Vérifier que toutes les fonctions sont optimisées
-> Faire le triage des coups grâce aux itérations précédentes?
-> Fonction pour stocker facilement un noeud, ou savoir s'il est similaire à un autre? -> transposition tables



----- Intelligences artificielles -----


--- Nouveaux algorithmes ---

- Monter Carlo -
-> Implémenter un agent basique (pas déterministe - qui peut jouer plusieurs coups par position)
-> Implémenter un algorithme de Monte Carlo 

- Grogrofish_iterative_depth -
-> Changer Grogrosfish : depth -> temps (--> moteur d'analyse)
-> Faire une profodeur -> trier les coups du meilleur au pire, puis continuer la recherche plus loin

- Agent_new = Agent_old++ -
-> Faire un agent qui gagne toujours contre un autre : regarde tous les coups, et joue pour chacun la partie jusqu'au bout en utilisant agent_old -> puis joue les coups qui gagnent


--- Améliorations ---

-> Améliorer les heuristiques pour l'évaluation d'une position
    - Positionnement du roi changeant au cours de la partie
    - Activité des pièces
    - Sécurité du roi
    - Espace
    - Structures de pions
-> Livres d'ouvertures, tables d'engame?
-> Tables de hachages, et apprentissage de l'IA? -> voir tp_jeux (UE IA/IRP)
-> Augmenter la profondeur pour les finales



----- Interface utilisateur -----

-> Amélioration des sons
-> Afficher les coups jouables par l'utilisateur (et par pièce s'il en clique une)
-> Montrer en direct sur la GUI l'avancement de l'IA -> quel coup il refléchit et évaluation de chaque coup
-> Undo move dans l'interface, avec les flèches (il faut donc stocker l'ensemble de la partie - à l'aide du PGN -> from_pgn?)
-> Tourner l'échiquier
-> Nouveau sons/images
-> Surligner avec le clic droit
-> Dans le negamax, renvoyer le coup à chaque fois, pour noter la ligne que l'ordi regarde?



----- Fonctionnalités supplémentaires -----

-> Chargement de FEN -> Modifier le PGN en FEN + ... exemple : "[FEN "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w - - 0 2"] 2. Nf3 Nc6 *"
-> Correction PGN -> fins de parties


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
    bool play_white = false;
    bool play_black = false;
    int search_depth = 6;


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



        // if (IsKeyDown(KEY_T)) {
        //     t.to_fen();
        //     cout << t._fen << endl;
        //     cout << t._pgn << endl;
        //     t.get_moves();
        //     t.display_moves();
        // }


        // Activations rapides de l'IA
        if (IsKeyDown(KEY_G))
            self_play = true;

        if (IsKeyDown(KEY_DOWN))
            play_white = true;
        
        if (IsKeyDown(KEY_UP))
            play_black = true;

        if (t.game_over() == 0 && ((self_play) || (play_black && !t._player) || (play_white && t._player))) {
            t.grogrosfish2(search_depth, test_parameters);
            //t.grogrosfish_multiagents(4, n_agents, test_begin_parameters, test_end_parameters);
            t.to_fen();
            cout << t._fen << endl;
            cout << t._pgn << endl;
        }

        // if (play_black && !t._player) {
        //     t.grogrosfish2(6, test_parameters);
        //     t.to_fen();
        //     cout << t._fen << endl;
        //     cout << t._pgn << endl;
        // }

        // if (play_white && t._player) {
        //     //t.grogrosfish_multiagents(4, n_agents, test_begin_parameters, test_end_parameters);
        //     t.grogrosfish2(6, test_parameters);
        //     t.to_fen();
        //     cout << t._fen << endl;
        //     cout << t._pgn << endl;
        // }


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