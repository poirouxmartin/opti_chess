#include "opti_chess.h"
#include "time_tests.h"
#include "useful_functions.h"
#include "math.h"
#include "gui.h"
#include "neural_network.h"
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
https://www.chessprogramming.org/Move_Generation
https://www.chessprogramming.org/Checkmate
https://www.chessprogramming.org/Bishop_versus_Knight#WinningPercantages
https://www.chessprogramming.org/Sensor_Chess#MoveGeneration



----- Jeu -----

-> Promotions (de tous types plutôt que seulement dame)
-> Rajouter des tests de validité de FEN
-> Répétition de coups



----- Structure globale du projet -----

-> Séparer les fonctions du fichier opti_chess dans d'autres fichiers (GUI, IA...)
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

- Grogrofish_iterative_depth -
-> Changer Grogrosfish : depth -> temps (--> moteur d'analyse)
-> Faire une profodeur -> trier les coups du meilleur au pire, puis continuer la recherche plus loin

- Agent_new = Agent_old++ -
-> Faire un agent qui gagne toujours contre un autre : regarde tous les coups, et joue pour chacun la partie jusqu'au bout en utilisant agent_old -> puis joue les coups qui gagnent

- RE : multi_agents


--- Améliorations ---

-> Améliorer les heuristiques pour l'évaluation d'une position
    - Positionnement du roi, des pions, de la dame et des pièces changeant au cours de la partie (++ pièces mineures en début de partie, ++ le reste en fin de partie, ++ valeur des pions) (endgame = 13 points or below for each player? less than 4 pieces?)
    - Sécurité du roi (TRES IMPORTANT !) --> A améliorer, car là c'est pourri... comment calculer? !(pion protégeant le roi) *  pieces ennemies proches du roi = !king_safety ? 
    - Espace (dépend aussi du nombre de pièces restantes..)
    - Structures de pions (IMPORTANT)
    - Diagonales ouvertes
    - Lignes ouvertes, tours dessus
    - Clouages
    - Pièces attaquées?
    - Cases noires/blanches
    - Contrôle de cases importantes
    - Cases faibles
    - Pions arrierés/faibles
    - Initiative -> A améliorer : fort dans les positions d'attaque?
    - Contrôle du centre
    - Harmonie des pièces
    - Pièces enfermées
    - Bon/Mauvais fou
    - Tours sur colonnes ouvertes
    - Tours sur une même colonne qu'une dame ou un roi
    - Pions bloqués / Développement de pièces impossible
    - Fous/Paire de fou meilleurs en position ouverte (cavalier : inverse)
    - Tours liées
    - Garder matériel en position perdante?
-> Livres d'ouvertures, tables d'engame?
-> Tables de hachages, et apprentissage de l'IA? -> voir tp_jeux (UE IA/IRP)
-> Augmenter la profondeur pour les finales (GrogrosFish)
-> Bug de temps quand les IA jouent entre elles?
-> Système d'élo pour les tournois à fix (parfois elo négatif)
-> Améliorations pour trouver les mats les plus rapides... arrêter la recherche dans une branche finie...
-> Utiliser raylib pour le random? check la vitesse
-> Création d'une base de données contenant des positions et des évaluations? (qui se remplit au cours des parties...)
-> Allocations mémoires utilisant raylib?
-> Faire aussi un énrome buffer pour les coups? Ils prennent 5000/5512 des bytes d'un board...
-> Changer la structure de données des boards pour réduire leur taille
-> Bug?? e4 Nf6 e5... e6??
-> Comportements bizarres dans la scandi
-> Heuristics : check -- r2q1rk1/1pp2pp1/p1p1b3/7p/4P3/4NP2/PPPPK1P1/RNB2Q2 w - - 3 15     (sees it as bad, while it's completely winning)
-> Trouve pas forcément les mats les plus rapides (dès qu'il en a trouvé un, il cherche plus vraiment sur les autres coups...)
-> Attention au cas ou le buffer est plein
-> Affichage des flèches buggées pour les mats?
-> Tout supprimer les calculs quand on importe un FEN
-> Bug de couleurs parfois : jaune, jaune, jaune... ROUGE, jaune? (vers noeuds = 19%) pareil avec cyan et bleu
-> Heuristiques bof bof : r1b1kb1r/2p2ppp/p7/2pp4/3N4/8/PPP2PPP/RNB1K2R w KQkq - 0 11 : une pièce de moins en endgame, et croit que c'est ok
-> Pareil : r3r1k1/3q1p2/pb1p2p1/1p1p4/1P3n2/5N1P/1BQN1PP1/R3R1K1 w - - 0 31, 4r1k1/3q1p2/pb1p2p1/1p1p4/1P3n2/5N1P/1BQN1PP1/4R1K1 b - - 1 32, 6k1/2q2p2/pb1p2p1/1p1p4/1P3n2/5N1P/1BQN1PP1/5K2 w - - 5 36, r4rk1/2pqbppp/p2p4/1p3P2/Pn6/4B3/1PPQBPPP/R4RK1 b - - 2 15, rnb1kb1r/ppp2ppp/5n2/q7/2P5/1P3B2/PB1P1PPP/RN1QK2R b KQkq - 0 8, 2r2rk1/1p2b3/p3pqp1/P3p2p/NP2p3/1N4P1/1P3P1P/2RQ1RK1 b - - 0 20, 
-> Bitboard
-> Euh.. pourquoi Grogros a ralenti sans raisons? -> Redémarrer PC...
-> Moves : plutôt que _moves[1000] -> *_moves + new _moves[_got_moves]. (avec les moves à 4 digits?). add_move utilise un array local (global?) de grande taille (stack) plutôt que les mettre direct dans _moves
-> TESTER : vecteur de plateaux (plutôt que un new???)
-> Compression des entier : 8 * a + b? sinon shift bit avec << (plus rapide)
-> Faire une nouvelle classe fille, pour optimiser et retirer les attributs inutiles, en fonction des algorithmes utilisés? (_moves_order est inutile si on utilise GrogrosZero)
-> Faut-il du coup utiliser des uint8_t dans les arguments de fonction? ou int est ok?
-> Dans le backpropagation de GrogrosZero... pour l'évaluation, prendre l'eval du meilleur fils seulement? ou faire une moyenne pondérée de tous les fils, avec leur policy/nodes?
-> Incrémentation de l'évaluation quand on joue les coups (make_move(keep))
-> Utiliser la librairie boost
-> Faire une liste séparée pour les legal et les pseudo-legal moves
-> Génération des coups de façon ordonnée? (captures en premier?)
-> Finir les undo
-> Mettre des options pour certaines fonctions pour ne pas faire les étapes inutiles (_last_move dans make_move??)
-> Self play pour GrogrosZero (blancs/noirs)
-> Faire une fonction pour initialiser un plateau (plutôt que le from_fen)
-> Promotions en autre chose que dame?
-> 4rrk1/p5p1/b3p2p/2p1Bp1Q/P7/1q1P4/5PPP/2KRR3 w - - 0 25 -> Il faut que roi faible
-> Faire que les undo gardent les calculs de GrogrosZero sur la position



----- Interface utilisateur -----

-> Amélioration des sons
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
-> Améliorer l'affichage du PGN
-> Choisir les crossover en fonction des meilleurs elos
-> Elo : que faire quand on copie un agent? crossover? mutation? reset?
-> Chercher pourquoi les rounds de fin sont plus lents que ceux du début...
-> Coup illégal -> perte de la partie
-> Gagner contre elo négatif = perdre elo?
-> Ordonner les flèches de coups de Monte-Carlo pour que ça ne cache plus les autres
-> Ajouter plus d'info sur les coups (ainsi que les positions résultantes et leur évaluation)
-> Couleur des coups à fix... des modulos?? (quand ça arrive dans le bleu...)
-> Nouveau curseur
-> Premettre de modifier les paramètres de recherche de l'IA : beta, k_add... (d'une meilleure manière)
-> Changements de taille de la fenêtre
-> Save : pgn + fen, load les deux aussi
-> Ne plus afficher "INFO:" de raylib dans la console
-> Comme Nibbler, faire un slider à droite, qui contient l'info de tous les coups possibles : variations, noeuds, éval, position finale...
-> Mettre une limite à l'utilisation des noeuds de GrogrosZero
-> Musique de fond? (désactivable)
-> Affichage de l'évaluation complète (avec toutes se composantes)
-> Montrer les noeuds par seconde pour GrogrosZero, et le nombre de noeuds total dans le buffer. Ainsi que le nom de l'IA en self play, et son nombre de noeuds en self play
-> Mettre une aura autour du coup qui sera joué pour qu'on puisse le voir directement
-> (changer épaisseur des flèches en fonction de leur importance? garder la transparence?)
-> Combiner les formes (cercles et rectangles) pour faire une flèche unie
-> Ordonner l'affichage des flèches (pour un fou, mettre le coup le plus court en dernier) (pour deux coups qui vont au même endroit, mettre le meilleur en dernier)
-> Ajouter un éditeur de positions (ajouter/supprimer les pièces)
-> Montrer les pièces qui ont déjà été capturées
-> Sons parfois mauvais (en passant par exemple...) -- à fix + rajouter bruits de mats...
-> Binding pour jouer tout seul en ligne
-> Ajout d'une gestion du temps par les IA
-> Bugs de texte (PGN) dus à c_str()? à vérifier
-> Rajouter le nom des joueurs dans le PGN, ainsi que le temps par coup
-> Pourquoi GrogrosZero ne s'arrête plus? CTRL-G...
-> Temps au départ dans le PGN un peu buggé? Comment dire que ça commence avec un temps t?
-> Rajouter les "1-0" dans le PGN? victoires au temps?
-> Bon.. PGN à fix, car les attributs peuvent bugger...
-> Problème au niveau des temps (dans grogrosfish : premier coup à temps max -> le temps n'est actualisé que après son coup...)
-> Quand on ferme la fenêtre, GrogrosZero arrête de réflechir... (voir application en arrière plan)


----- Fonctionnalités supplémentaires -----

-> Correction PGN -> fins de parties
-> Importation depuis in PGN
-> Afficher pour chaque coup auquel l'ordi réfléchit : la ligne correspondante, ainsi que la position finale avec son évaluation
-> Ajouter les noms des joueurs ainsi que leurs temps par coups sur le PGN
-> Afficher sur le PGN la reflexion de GrogrosZero
-> Il y a un espace avant "1. e4" dans le PGN


*/




// Fonction qui permet de tester le temps que prend une fonction
void test() {

    static Board t_test;

    if (t_test._got_moves == -1)
        t_test.get_moves();

}






// Main
int main() {

    // Faire une fonction Init pour raylib?

    // Fenêtre resizable
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);       

    // Initialisation de la fenêtre
    InitWindow(screen_width, screen_height, "Grogros Chess");

    // Initialisation de l'audio
    InitAudioDevice();
    SetMasterVolume(1.0); // Trop faible...

    // Nombre d'images par secondes
    SetTargetFPS(fps);



    // Variables
    Board t;

    // Evaluateur de position
    Evaluator eval_white;
    Evaluator eval_black;

    // Evaluateur pour Monte Carlo
    Evaluator monte_evaluator;
    monte_evaluator._piece_activity = 0.03; // 0.04
    monte_evaluator._piece_positioning = 0.007; // beta = 0.035 // Pos = 0.013
    monte_evaluator._king_safety = 0.0025; // Il faut régler la fonction... avec les pièces autour, s'il est au milieu du plateau...
    monte_evaluator._castling_rights = 0.2;

    // Nombre de noeuds pour le jeu automatique de GrogrosZero
    int grogros_nodes = 100000;

    // Nombre de noeuds calculés par frame
    // Si c'est sur son tour
    int nodes_per_frame = 25000;

    // Sur le tour de l'autre (pour que ça plante moins)
    int nodes_per_user_frame = 1000;


    bool grogros_auto = false;
    bool grogros_play = false;
    bool grogroszero_play_black = false;
    bool grogroszero_play_white = false;

    // Activité des pièces à 0, car pour le moment, cela ralentit beaucoup le calcul d'évaluation
    eval_white._piece_activity = 0;
    eval_black._piece_activity = 0;
    eval_white._king_safety = 0;
    eval_black._king_safety = 0;


    // IA self play
    bool grogrosfish_play_white = false;
    bool grogrosfish_play_black = false;

    // Paramètres pour l'IA
    int search_depth = 7;


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

        // Changements de la taille de la fenêtre
        if (IsWindowResized()) {
            get_window_size();
            load_resources(); // Sinon ça devient flou
            resize_gui();
        }


        // Gestion du temps des joueurs
        if (t._time) {

            if (previous_player)
            t._time_white -= clock() - current_time;
        else
            t._time_black -= clock() - current_time;
        previous_player = t._player;
        current_time = clock();

        }


        // Save FEN
        if (IsKeyPressed(KEY_S)) {
            t.to_fen();
            SaveFileText("data/test.txt", (char*)t._fen.c_str());
            cout << "saved FEN : " << t._fen << endl;
        }

        // Load
        if (IsKeyPressed(KEY_L)) {
            string fen = LoadFileText("data/test.txt");
            t.from_fen(fen);
            cout << "loaded FEN : " << fen << endl;
        }

        // Retourne le plateau
        if (IsKeyPressed(KEY_F))
            switch_orientation();

        // Recommencer une partie
        if (IsKeyPressed(KEY_N))
            t.from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

        // Charger une partie
        if (IsKeyPressed(KEY_R))
            t.from_fen("r1b2rk1/5ppp/p1n1pn2/1pqp4/8/P1N1PN2/1PP1BPPP/R2Q1RK1 w - - 0 13");

        // Copie dans le clipboard du PGN
        if (IsKeyPressed(KEY_C)) {
            SetClipboardText(t._pgn.c_str());
            cout << "copied PGN : \n" << t._pgn << endl;
        }

        // Copie dans le clipboard du FEN
        if (IsKeyPressed(KEY_X)) {
            t.to_fen();
            SetClipboardText(t._fen.c_str());
            cout << "copied FEN : " << t._fen << endl;
        }

        // Colle le FEN du clipboard (le charge)
        if (IsKeyPressed(KEY_V)) {
            string fen = GetClipboardText();
            t.from_fen(fen);
            cout << "loaded FEN : " << fen << endl;
        }

        // // Colle le PGN du clipboard (le charge)
        // if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) {
        //     string pgn = GetClipboardText();
        //     t.from_pgn(pgn);
        //     cout << "loaded PGN : " << pgn << endl;
        // }

        
        if (IsKeyPressed(KEY_B)) {
            cout << "Available memory : " << getTotalSystemMemory() << endl;
            cout << "generating new buffer" << endl;
            _monte_buffer.init();
            // N'affiche pas la bonne taille
            cout << "new buffer, size : "  << sizeof(_monte_buffer) << " bits, length : " << _monte_buffer._length << endl;
            cout << "first free index : " << _monte_buffer.get_first_free_index() << endl;
        }
        

        // GrogrosZero
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_G)) {
            if (!_monte_buffer._init)
                _monte_buffer.init();
            t.grogros_zero(l_agents[0], monte_evaluator, nodes_per_frame, false, true, _beta, _k_add);
        }

        // GrogrosZero recherche automatique
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_G)) {
            if (!_monte_buffer._init)
                _monte_buffer.init();
            grogros_auto = !grogros_auto;            
        }


        // Grogros zero self play
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_P)) {
            if (!_monte_buffer._init)
                _monte_buffer.init();
            grogros_play = !grogros_play;            
        }


        // Affichage des flèches
        if (IsKeyPressed(KEY_H)) {
            switch_arrow_drawing();
        }

        // Calcul en mode auto
        if (grogros_auto && t.game_over() == 0) {
            if (!_monte_buffer._init)
                _monte_buffer.init();
            if (!is_playing()) // Pour que ça ne lag pas pour l'utilisateur
                t.grogros_zero(l_agents[0], monte_evaluator, nodes_per_frame, false, true, _beta, _k_add);     
        }

        // Calcul pour les pièces blanches
        if (grogroszero_play_white && t.game_over() == 0) {
            if (!_monte_buffer._init)
                _monte_buffer.init();
            if (t._player)
                t.grogros_zero(l_agents[0], monte_evaluator, nodes_per_frame, false, true, _beta, _k_add);
            else
                if (!is_playing() || true) // Pour que ça ne lag pas pour l'utilisateur 
                    t.grogros_zero(l_agents[0], monte_evaluator, nodes_per_user_frame, false, true, _beta, _k_add);     
        }

        // Calcul pour les pièces noires
        if (grogroszero_play_black && t.game_over() == 0) {
            if (!_monte_buffer._init)
                _monte_buffer.init();
            if (!t._player)
                t.grogros_zero(l_agents[0], monte_evaluator, nodes_per_frame, false, true, _beta, _k_add);
            else
                if (!is_playing() || true) // Pour que ça ne lag pas pour l'utilisateur
                    t.grogros_zero(l_agents[0], monte_evaluator, nodes_per_user_frame, false, true, _beta, _k_add);     
        }


        // Joue les coups selon grogros en fonction de la reflexion actuelle
        if (grogros_play && t.game_over() == 0) {
            if (t.total_nodes() > grogros_nodes)
                t.play_monte_carlo_move_keep(t.best_monte_carlo_move(), true);
        }
            

        // Supprime les reflexions de GrogrosZero
        if (IsKeyPressed(KEY_DELETE)) {
            t.reset_all(true, true);
        }

        // Mont-Carlo, en regardant les mats/pats
        if (IsKeyPressed(KEY_T)) {
            // test_function(&test, 1);

            /*Network _test_network;
            _test_network.generate_random_weights();

            vector<string> positions_vector {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKB1R w KQkq - 0 1"};
            vector<int> evaluations_vector {60, 18000};

            _test_network.input_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
            _test_network.calculate_output();
            // On utilisera des évaluations allant de -100000 à +100000, en milipions.
            // Pour le réseau, éval max : 64 * 100 * 100 = 640000
            cout << _test_network._layers[2][0] << endl;
            cout << _test_network.global_distance(positions_vector, evaluations_vector) << endl;*/

            // t.evaluate(monte_evaluator);
            // cout << t._king_safety << endl;

        }

        if (IsKeyPressed(KEY_E))
            t.evaluate(monte_evaluator, true, true);


        // Undo
        IsKeyPressed(KEY_U) && t.undo(); 

        // Modification des paramètres
        if (IsKeyPressed(KEY_KP_ADD))
            _beta *= 1.1;

        if (IsKeyPressed(KEY_KP_SUBTRACT))
            _beta /= 1.1;

        if (IsKeyPressed(KEY_KP_MULTIPLY))
            _k_add *= 1.25;

        if (IsKeyPressed(KEY_KP_DIVIDE))
            _k_add /= 1.25;

        // Reset aux valeurs initiales
        if (IsKeyPressed(KEY_KP_ENTER)) {
            _beta = 0.05;
            _k_add = 25;
        }

        // Recherche en profondeur extrême
        if (IsKeyPressed(KEY_ONE)) {
            _beta = 0.5;
            _k_add = 0;
        }

        // Recherche en profondeur
        if (IsKeyPressed(KEY_TWO)) {
            _beta = 0.2;
            _k_add = 10;
        }

        // Recherche large
        if (IsKeyPressed(KEY_THREE)) {
            _beta = 0.01;
            _k_add = 100;
        }

        // Recherche de mat
        if (IsKeyPressed(KEY_FOUR)) {
            _beta = 0.005;
            _k_add = 2500;
        }

        // Recherche de victoire en endgame
        if (IsKeyPressed(KEY_FIVE)) {
            _beta = 0.05;
            _k_add = 5000;
        }


        // Joue le coup recommandé par l'algorithme de Monte-Carlo
        if (IsKeyPressed(KEY_P)) {
            if (t._tested_moves > 0) {
                t.play_monte_carlo_move_keep(t.best_monte_carlo_move(), true);
            }
            else {
                cout << "no more moves are in memory" << endl;
            }
        }
        
        // Lancement du temps
        if (IsKeyPressed(KEY_ENTER)) {
            t._time = !t._time;
            t.add_time_to_pgn();
            if (t._time) {
                current_time = clock();
            }
        }

        // Ajout des noms au PGN
        if (IsKeyPressed(KEY_Q)) // A
            t.add_names_to_pgn();
        

        // Activations rapides de l'IA

        // Fait jouer l'IA sur un coup
        if (IsKeyDown(KEY_SPACE)) {
            if (t._player)
                t.grogrosfish(search_depth, eval_white, true);
            else
                t.grogrosfish(search_depth, eval_black, true);
        }

        // Joueur des pièces blanches : IA/humain
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_DOWN)) {
            grogrosfish_play_white = !grogrosfish_play_white;
            if (grogrosfish_play_white) {
                t._white_player = "GrogrosFish (depth " + to_string(search_depth) + ")";
                t.add_names_to_pgn();
            }  
            else {
                t._white_player = (char*)"Player 1";
                t.add_names_to_pgn();
            }
                
        }

        // Joueur des pièces noires : IA/humain
        if (!IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_UP)) {
            grogrosfish_play_black = !grogrosfish_play_black;
            if (grogrosfish_play_black) {
                t._black_player = "GrogrosFish (depth " + to_string(search_depth) + ")";
                t.add_names_to_pgn();
            }        
            else {
                t._black_player = (char*)"Player 2";
                t.add_names_to_pgn();
            }
                
        }

        // Joueur des pièces blanches : IA/humain
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_DOWN)) {
            grogroszero_play_white = !grogroszero_play_white;
            if (grogroszero_play_white) {
                t._white_player = "GrogrosZero (" + int_to_round_string(grogros_nodes) + " nodes)";
                t.add_names_to_pgn();
            }
            else {
                t._white_player = (char*)"Player 1";
                t.add_names_to_pgn();
            }
                
        }

        // Joueur des pièces noires : IA/humain
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_UP)) {
            grogroszero_play_black = !grogroszero_play_black;
            if (grogroszero_play_black) {
                t._black_player = "GrogrosZero (" + int_to_round_string(grogros_nodes) + " nodes)";
                t.add_names_to_pgn();
            }
                
            else {
                t._black_player = (char*)"Player 2";
                t.add_names_to_pgn();
            }

        }
            
        // Stoppe toutes les IA
        if (IsKeyDown(KEY_BACKSPACE)) {
            grogroszero_play_white = false;
            grogroszero_play_black = false;
            grogrosfish_play_white = false;
            grogrosfish_play_black = false;
        }

        // Fait jouer l'IA automatiquement en fonction des paramètres
        if (t.game_over() == 0) {
            if (t._player) {
                if (grogrosfish_play_white)
                    t.grogrosfish(search_depth, eval_white, true);
                if (grogroszero_play_white)
                   if (t.total_nodes() > grogros_nodes)
                        t.play_monte_carlo_move_keep(t.best_monte_carlo_move(), true); 
            }
            else {
                if (grogrosfish_play_black)
                    t.grogrosfish(search_depth, eval_black, true);
                if (grogroszero_play_black)
                   if (t.total_nodes() > grogros_nodes)
                        t.play_monte_carlo_move_keep(t.best_monte_carlo_move(), true); 
            }
        }

        // Entrainement d'agents
        if (IsKeyDown(KEY_KP_0)) {
            t.grogros_zero(l_agents[0], monte_evaluator, 25000, true);
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
            t.grogros_zero(final_agent, monte_evaluator, 25000, true);
        }

        if (IsKeyDown(KEY_KP_7)) {
            t.grogrosfish(5, final_agent, true);
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