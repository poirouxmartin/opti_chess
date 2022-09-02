#include "opti_chess.h"
#include "time_tests.h"
#include "useful_functions.h"
#include "math.h"
#include "gui.h"
//#include <windows.h>


/* TODO


----- Documentation pour la suite -----

https://cs229.stanford.edu/proj2012/DeSa-ClassifyingChessPositions.pdf
https://www.wikiwand.com/en/Negamax
https://thesai.org/Downloads/Volume5No5/Paper_10-A_Comparative_Study_of_Game_Tree_Searching_Methods.pdf
https://www.chessprogramming.org/Evaluation_of_Pieces
https://www.chessprogramming.org/Evaluation
https://www.cs.cornell.edu/boom/2004sp/ProjectArch/Chess/algorithms.html#board
https://www.chessprogramming.org/Main_Page
https://towardsdatascience.com/building-a-chess-ai-that-learns-from-experience-5cff953b6784
https://arxiv.org/pdf/1711.08337.pdf
https://stackoverflow.com/questions/40137240/training-of-chess-evaluation-function
https://arxiv.org/pdf/2007.02130.pdf



----- Jeu -----

-> Echecs?
-> Promotions (de tous types plutôt que seulement dame)
-> Rajouter des tests de validité de FEN
-> Répétition de coups
-> Pat à ajouter



----- Structure globale du projet -----

-> Séparer les fonctions du fichier opti_chess dans d'autres fichiers (GUI, IA...)
-> Virer tous les warnings
-> Faire du ménage dans les fonctions
-> Faire une classe pour les IA?



----- Optimisations à implémenter -----

-> Regarder quel compilateur est le plus rapide/opti
-> Algo Negamax -> copies des tableaux -> undo moves? ---> finalement plus lent?
-> Transposition tables
-> Algorithme MTD(f) <-> alphabeta with memory algorithm
-> stoi et to_string très lents?
-> Ajout de variables globales plutôt que les re définir lors des appels de fonction (valeur des pièces, positionnement...)
-> Parallélisation -> std::for_each avec policy parallel?
-> Au lieu de calculer l'évaluation à chaque coup, l'imcrémenter en fonction du coup
-> Dans le negamax, si y'a un mat, ne plus regarder les autres coups?
-> Optimiser le sort moves - l'améliorer (et l'évaluation?)
-> Remplacer des if par des &&
-> Vérifier que toutes les fonctions sont optimisées
-> Faire le triage des coups grâce aux itérations précédentes?
-> Fonction pour stocker facilement un noeud, ou savoir s'il est similaire à un autre? -> transposition tables
-> Incrémenter le game_over à chaque coup joué plutôt que de le regarder à chaque fois
-> Regarder si l'implémentation des échecs rend les calculs plus rapides
-> Ne plus jouer les échecs?
-> Copie des plateaux : tout copier? ou seulement quelques informations importantes?
-> Liste des coups légaux, et une autre liste pour les coups pseudo-légaux... pour éviter de les recalculer à chaque fois...



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

- RE : multi_agents


--- Améliorations ---

-> Améliorer les heuristiques pour l'évaluation d'une position
    - Positionnement du roi, des pions, de la dame et des pièces changeant au cours de la partie (++ pièces mineures en début de partie, ++ le reste en fin de partie, ++ valeur des pions) (endgame = 13 points or below for each player? less than 4 pieces?)
    - Activité des pièces
    - Sécurité du roi
    - Espace
    - Structures de pions
    - Diagonales ouvertes
    - Lignes ouvertes, tours dessus
    - Clouages
    - Pièces attaquées?
    - Cases noires/blanches
    - Contrôle de cases importantes
    - Cases faibles
    - Pions arrierés/faibles
    - Initiative
    - Contrôle du centre
    - Harmonie des pièces
    - Pièces enfermées
    - Bon/Mauvais fou
    - Tours sur colonnes ouvertes
    - Tours sur une même colonne qu'une dame ou un roi
    - Tour sur la 7ème (-> voir piece_position)
    - Droits de roque(s)
    - Pions bloqués / Développement de pièces impossible
    - Valeur des pièces changeante au cours du temps (tour mieux en endgame)
    - f6
-> Livres d'ouvertures, tables d'engame?
-> Tables de hachages, et apprentissage de l'IA? -> voir tp_jeux (UE IA/IRP)
-> Augmenter la profondeur pour les finales
-> Negascout et PVS, problème (??) : cela doit utiliser l'ordre de coup de l'itération précédente. Cependant, l'évaluation d'un coup d'une itération sur l'autre varie beaucoup -> ordre de coups différent -> peu optimal
-> Ajouter une part de random dans l'IA?
-> Bug de temps quand les IA jouent entre elles?
-> Système d'élo pour les tournois?


----- Interface utilisateur -----

-> Amélioration des sons
-> Montrer en direct sur la GUI l'avancement de l'IA -> quel coup il refléchit et évaluation de chaque coup
-> Undo move dans l'interface, avec les flèches (il faut donc stocker l'ensemble de la partie - à l'aide du PGN -> from_pgn?)
-> Nouveau sons/images
-> Surligner avec le clic droit
-> Dans le negamax, renvoyer le coup à chaque fois, pour noter la ligne que l'ordi regarde?
-> Ajout de pre move
-> Ajout de temps par joueur
-> Pouvoir choisir contre quelle IA jouer
-> Pouvoir faire des flèches
-> Afficher les coordonnées des cases
-> Faire des boutons pour faire des actions (ex copier ou coller le FEN/PGN, activer l'IA ou la changer...)
-> Revoir l'affichage du PGN (ne pas sauter à la ligne au milieu d'un mot)
-> Options : désactivation son, ...
-> Sons : ajouter checkmate, stealmate, promotion
-> Chargement FEN -> "auto complétion" si le FEN est incorrect
-> Afficher quelle IA joue
-> Parfois, l'affichage du PGN bug... à régler
-> Régler le clic (quand IA va jouer), qui affiche mal la pièce
-> Montrer les pièces qui on étaient prises pendant la partie, ainsi que la différence matérielle
-> Pouvoir modifier les noms des joueurs
-> PGN : ajout des noms et des temps par coup
-> Modification du temps
-> Interface qui ne freeze plus quand l'IA réfléchit
-> Sons pour le temps
-> Fonction qui affiche le temps en heures, minutes et secondes plutôt que secondes
-> Ajout d'un carré de couleur avec le temps
-> Incrément de temps
-> Améliorer le surlignage
-> Améliorer l'affichage du PGN
-> Choisir les crossover en fonction des meilleurs elos
-> Elo : que faire quand on copie un agent? crossover? mutation? reset?
-> Chercher pourquoi les rounds de fin sont plus lents que ceux du début...
-> Coup illégal -> perte de la partie
-> Gagner contre elo négatif = perdre elo?
-> Ordonner les flèches de coups de Monte-Carlo pour que ça ne cache plus les autres
-> Ajouter plus d'info sur les coups (ainsi que les positions résultantes et leur évaluation)
-> Couleur des coups à fix... des modulos?? (quand ça arrive dans le bleu...)


----- Fonctionnalités supplémentaires -----

-> Chargement de FEN -> Modifier le PGN en FEN + ... exemple : "[FEN "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w - - 0 2"] 2. Nf3 Nc6 *"
-> Correction PGN -> fins de parties
-> Importation depuis in PGN
-> Copie du FEN/PGN (dans le clipboard?)
-> Afficher pour chaque coup auquel l'ordi réfléchit : la ligne correspondante, ainsi que la position finale avec son évaluation
-> Ajouter les noms des joueurs ainsi que leurs temps par coups sur le PGN


*/




// Fonction qui permet de tester le temps que prend une fonction
void test() {

    Board t;
    //t.sort_moves();

}






// Main
int main() {

    // Initialisation de la fenêtre
    InitWindow(screen_width, screen_height, "Grogros Chess");

    // Initialisation de l'audio
    InitAudioDevice();

    // Nombre d'images par secondes
    SetTargetFPS(fps);



    // Variables
    Board t;


    // Calcul du temps de la fonction
    test_function(&test, 1);


    // Evaluateur de position
    Evaluator eval_white;
    Evaluator eval_black;

    // Evaluateur pour Monte Carlo
    Evaluator monte_evaluator;
    // monte_evaluator._piece_activity = 0.1;
    // monte_evaluator._piece_positioning = 0.025; // beta = 0.01
    monte_evaluator._piece_activity = 0.05;
    monte_evaluator._piece_positioning = 0.015; // beta = 0.035


    // Activité des pièces à 0, car pour le moment, cela ralentit beaucoup le calcul d'évaluation
    eval_white._piece_activity = 0;
    eval_black._piece_activity = 0;



    // IA self play
    bool play_white = false;
    bool play_black = false;

    // Paramètres pour l'IA
    int search_depth = 8;


    // Temps
    clock_t current_time;
    bool previous_player = true;


    // Test des agents GrogrosZero

    // Liste d'agents
    const int n_agents = 25;
    Agent l_agents[n_agents];
    int generation = 0;
    int max_generations = 100;
    Agent final_agent;

    // Initialisation des agents
    for (int i = 0; i < n_agents; i++) {
        Agent a;
        l_agents[i] = a;
    }

    // Liste de scores pour les tournois
    int *l_scores;


    // Boucle principale (Quitter à l'aide de la croix, ou en faisant échap)
    while (!WindowShouldClose()) {

        // Gestion du temps des joueurs
        if (t._time) {

            if (previous_player)
            t._time_player_1 -= clock() - current_time;
        else
            t._time_player_2 -= clock() - current_time;
        previous_player = t._player;
        current_time = clock();

        }



        // Orientation du plateau
        if (IsKeyPressed(KEY_F))
            switch_orientation();

        // Recommencer une partie
        if (IsKeyPressed(KEY_N))
            t.from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

        // Charger une partie
        if (IsKeyPressed(KEY_R)) {
            // t.from_fen("3kr3/PK1p4/B7/8/8/8/8/8 w - - 0 7");
            // t.from_fen("8/6k1/8/8/8/8/P7/K7 w - - 0 7");
            // t.from_fen("8/8/8/8/8/5K2/3R4/5k2 b - - 12 13");
            t.from_fen("r1b1k1r1/1ppqpp2/p4Q1n/3P2NB/1n6/2N4P/PP3PP1/R3R1K1 b q - 4 18");
            // t.from_fen("r1b1r1k1/pp1p1pp1/2p3p1/q1P1P3/2PP4/3Q2P1/5PP1/2R1R1K1 b - - 2 22");
        }

        // Suppression des plateaux
        if (IsKeyPressed(KEY_DELETE))
            t.delete_all();

        // Copie dans le clipboard
        if (IsKeyPressed(KEY_C)) {
            // const char *copy = t._pgn.c_str();
            // const size_t len = strlen(copy) + 1;
            // HGLOBAL hMem =  GlobalAlloc(GMEM_MOVEABLE, len);
            // memcpy(GlobalLock(hMem), copy, len);
            // GlobalUnlock(hMem);
            // OpenClipboard(0);
            // EmptyClipboard();
            // SetClipboardData(CF_TEXT, hMem);
            // CloseClipboard();
        }

        

        // Mont-Carlo, en regardant les mats/pats
        if (IsKeyDown(KEY_O))
            t.monte_carlo_2(l_agents[0], monte_evaluator, 25000, false, true);

        // Monte-Carlo, sans...
        if (IsKeyDown(KEY_I))
            t.monte_carlo_2(l_agents[0], monte_evaluator, 25000);

        if (IsKeyPressed(KEY_D)) {  
            t.display_moves(true);
        }


        // Joue le coup recommandé par l'algorithme de Monte-Carlo
        if (IsKeyPressed(KEY_P)) {
            if (t._tested_moves > 0)
                t.play_monte_carlo_move(true);
        }


        // Self play avec Monte-Carlo
        if (IsKeyPressed(KEY_M)) {
            t.monte_carlo_2(l_agents[0], monte_evaluator, 1000);
            //t.play_monte_carlo_move();
        }

        // Evaluation de l'agent GrogrosZero
        if (IsKeyPressed(KEY_E)) {
            // t.evaluate(l_agents[0]);
            // cout << "Evaluation de GrogrosZero : " << l_agents[0]._output << endl;
            t.evaluate(monte_evaluator, true);
            cout << "eval with checkmates : " << t._evaluation << endl;
            t.to_fen();
            cout << t._fen << " : " << t.in_check() << endl;
        }

        // Affiche le FEN
        if (IsKeyPressed(KEY_U)) {  
            t.to_fen();
            cout << t._fen << endl;
        }

        // Fonction test pour les temps
        if (IsKeyDown(KEY_T)) {
           //test_function(&test, 1);
           //cout << "in check : " << t.in_check() << endl;
        }
        
        // Lancement du temps
        if (IsKeyPressed(KEY_ENTER)) {
            t._time = !t._time;
            if (t._time) {
                current_time = clock();
            }
        }
        
        // ----- Tests d'algorithmes
        if (IsKeyDown(KEY_B))
            t.grogrosfish3(search_depth, eval_white);

        if (IsKeyDown(KEY_V))
            t.grogrosfish4(search_depth, eval_white);

        // if (IsKeyDown(KEY_M))
        //     t.grogrosfish_multiagents(search_depth, n_agents, test_begin_parameters, test_end_parameters);
        // ----- Fin des tests d'agents  -----


        // Activations rapides de l'IA

        // Fait jouer l'IA sur un coup
        (IsKeyDown(KEY_SPACE)) && t.grogrosfish2(search_depth, eval_white, true);

        // Joueur des pièces blanches : IA/humain
        if (IsKeyPressed(KEY_DOWN)) {
            play_white = !play_white;
            if (play_white)
                t._player_1 = (char*)"IA";
            else
                t._player_1 = (char*)"Player 1";
        }

        // Joueur des pièces noires : IA/humain
        if (IsKeyPressed(KEY_UP)) {
            play_black = !play_black;
            if (play_black)
                t._player_2 = (char*)"IA";
            else
                t._player_2 = (char*)"Player 2";
        }
            
        // Stoppe toutes les IA
        if (IsKeyDown(KEY_BACKSPACE)) {
            play_white = false;
            play_black = false;
        }

        // Fait jouer l'IA automatiquement en fonction des paramètres
        if (((play_black && !t._player) || (play_white && t._player)) && t.game_over() == 0) {
            if (t._player)
                t.grogrosfish2(search_depth, eval_white, true);
            else
                t.grogrosfish2(search_depth, eval_black, true);
            cout << "Avancement de la partie : " << t.game_advancement() << endl;
            cout << "game over : " << t.game_over() << endl;
        }

        // Entrainement d'agents
        if (IsKeyDown(KEY_KP_0)) {
            t.monte_carlo_2(l_agents[0], monte_evaluator, 25000, true);
        }
            
        // Match
        if (IsKeyDown(KEY_KP_1)) {
            cout << match(l_agents[0], l_agents[1]) << endl;
        }

        // Tournoi
        if (IsKeyPressed(KEY_KP_2)) {
            tournament(l_agents, n_agents);
        }

        // Nouvelle génération
        if (IsKeyPressed(KEY_KP_3)) {
            next_generation(l_agents, n_agents, 0.1, 0.25, 0.2);
        }

        // Mutation
        if (IsKeyPressed(KEY_KP_4)) {
            l_agents[0].mutation(0.25);
        }

        // Lance plusieurs générations
        if (IsKeyPressed(KEY_KP_5)) {
            generation = 0;
            cout << "Generation 0, launch until generation " << max_generations << "..." << endl;
            tournament(l_agents, n_agents);
            while (generation < max_generations) {
                cout << "Generation " << generation << endl;
                next_generation(l_agents, n_agents, 0.1, 0.25, 0.2);
                tournament(l_agents, n_agents);
                generation++;
            }

            // Copie du meilleur agent
            int best_agent = 0;
            int best_score = 0;
        

            for (int i = 0; i < n_agents; i++) {
                if (l_agents[i]._score > best_score) {
                    best_agent = i;
                    best_score = l_agents[i]._score;
                }
            }


            final_agent.copy_data(l_agents[best_agent], true);

            cout << "Simulation done. Best agent has been copied (last score : " << best_score << ")" << endl;
        }

        // Joue avec le nouvel agent
        if (IsKeyDown(KEY_KP_6)) {
            t.monte_carlo_2(final_agent, monte_evaluator, 25000, true);
        }

        if (IsKeyDown(KEY_KP_7)) {
            t.grogrosfish2(5, final_agent, true);
        }



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