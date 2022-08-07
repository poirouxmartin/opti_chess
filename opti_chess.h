#include <iostream>
#include <vector>
#include <execution>
#include <array>
#include <string>
using namespace std;


/*

Plateau : 
    Pièces :
        Blancs :
            Pion = 1
            Cavalier = 2
            Fou = 3
            Tour = 4
            Dame = 5
            Roi = 6

        Noirs :
            Pion = 7
            Cavalier = 8
            Fou = 9
            Tour = 10
            Dame = 11
            Roi = 12


*/



// Paramètres d'évaluation par défaut
//static float default_eval_parameters[3] = {1, 0.1, 0.025};


class Board {
    public:

        // Attributs

        // Plateau
        int _array[8][8]    {{4, 2, 3, 5, 6, 3, 2, 4}, 
                            {1, 1, 1, 1, 1, 1, 1, 1}, 
                            {0, 0, 0, 0, 0, 0, 0, 0}, 
                            {0, 0, 0, 0, 0, 0, 0, 0}, 
                            {0, 0, 0, 0, 0, 0, 0, 0}, 
                            {0, 0, 0, 0, 0, 0, 0, 0}, 
                            {7, 7, 7, 7, 7, 7, 7, 7}, 
                            {10, 8, 9, 11, 12, 9, 8, 10}};

        // Coups possibles
        // (Augmenter si besoin)
        // On suppose ici que n_moves < 1000 / 4
        int _moves[1000];
        // Les coups sont-ils actualisés?
        int _got_moves = -1;

        // Ordre des coups à jouer (pour l'optimisation)
        int _move_order[250];
        // Les coups sont-ils triés?
        bool _sorted_moves = false;

        // Tour du joueur (true pour les blancs, false pour les noirs)
        bool _player = true;

        // Couleur du joueur (1 = blanc, -1 = noir)
        int _color = 1;

        // Peut-être utile pour les optimisations?
        float _evaluation = 0;

        // Paramètres pour l'évaluation de la position
        float _evaluation_parameters[3] = {1, 0.1, 0.025};

        // Roques disponibles
        bool _k_castle_w = true;
        bool _q_castle_w = true;
        bool _k_castle_b = true;
        bool _q_castle_b = true;

        // En passant possible (se note par exemple "d6" -> coordonnée de la case)
        string _en_passant = "-";

        // Nombre de demi-coups (depuis le dernier déplacement de pion, ou la dernière capture, et reste nul à chacun de ces coups)
        int _half_moves_count = 0;

        // Nombre de coups
        int _moves_count = 1;


        // FEN du plateau
        string _fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        // PGN du plateau
        string _pgn = "";

        // Dernier coup joué
        int _last_move[4] = {-1, -1, -1, -1};


        // Joueurs de la partie
        char* _player_1 = "Player 1";
        char* _player_2 = "Player 2";

        // Temps pour les joueurs
        bool _time = false;
        clock_t _time_player_1 = 180000;
        clock_t _time_player_2 = 180000;





        

        // Constructeur par défaut
        Board();

        // Constructeur de copie
        Board(Board &);

        // Fonction qui copie les attributs d'un plateau
        void copy_data(Board &);

        // Fonction qui copie les coups d'un plateau
        void copy_moves(Board &);

        // Affichage du plateau
        void display();

        // Fonction qui ajoute un coup dans la liste de coups
        bool add_move(int, int, int, int, int *);

        // Fonction qui ajoute les coups "pions" dans la liste de coups
        bool add_pawn_moves(int, int, int *);

        // Fonction qui ajoute les coups "cavaliers" dans la liste de coups
        bool add_knight_moves(int, int, int *);

        // Fonction qui ajoute les coups diagonaux dans la liste de coups
        bool add_diag_moves(int, int, int *);

        // Fonction qui ajoute les coups horizontaux et verticaux dans la liste de coups
        bool add_rect_moves(int, int, int *);

        // Fonction qui ajoute les coups "roi" dans la liste de coups
        bool add_king_moves(int, int, int *);

        // Renvoie la liste des coups possibles
        int* get_moves(bool);

        // Fonction qui dit si une case est attaquée
        bool attacked(int, int);

        // Fonction qui donne la position du roi du joueur
        pair<int, int> get_king_pos();

        // Fonction qui dit s'il y'a échec
        bool in_check();

        // Fonction qui affiche la liste des coups
        void display_moves();

        // Fonction qui joue un coup
        void make_move(int, int, int, int);

        // Fonction qui joue le coup i de la liste des coups possibles
        void make_index_move(int);

        // Fonction qui renvoie l'avancement de la partie (0 = début de partie, 1 = fin de partie)
        float game_advancement();

        // Fonction qui évalue la position à l'aide d'heuristiques
        void evaluate(float[]);

        // Fonction qui joue le coup d'une position, renvoyant la meilleure évaluation à l'aide d'un negamax (similaire à un minimax)
        float negamax(int, float, float, int, bool, float[], bool, bool);

        // Mieux que negamax? tend à supprimer plus de coups
        float negascout(int, float, float, int, bool);

        // Algorithme PVS
        float pvs(int, float, float, int, bool);

        // Fonction qui utilise minimax pour déterminer quel est le "meilleur" coup et le joue
        void grogrosfish(int);

        // Version un peu mieux optimisée de Grogrosfish
        bool grogrosfish2(int, float[]);
        
        // Version qui utilise negascout
        void grogrosfish3(int);
        
        // Test de Grogrofish
        void grogrosfish4(int);

        // Test de Grogrofish avec combinaison d'agents
        void grogrosfish_multiagents(int, int, float[], float[]);

        // Fonction qui revient à la position précédente
        void undo(int, int, int, int, int, int, int);

        // Fonction qui arrange les coups de façon "logique", pour optimiser les algorithmes de calcul
        void sort_moves();

        // Fonction qui récupère le plateau d'un FEN
        void from_fen(string);

        // Fonction qui renvoie le FEN du tableau
        void to_fen();

        // Fonction qui renvoie le gagnant si la partie est finie (-1/1), et 0 sinon
        int game_over();

        // Fonction qui renvoie le label d'un coup
        string move_label(int, int, int, int);

        // Fonction qui renvoie le label d'un coup en fonction de son index
        string move_label_from_index(int);

        // Fonction qui fait un coup à partir de son label
        void make_label_move(string);

        // Fonction qui renvoie un plateau à partir d'un PGN
        void from_pgn();

        // Fonction qui affiche un texte dans une zone donnée
        void draw_text_rect(string, float, float, float, float, int);

        // Fonction qui dessine le plateau
        void draw();
        
        // Fonction qui joue le son d'un coup
        void play_move_sound(int, int, int, int);

        // Fonction qui joue le son d'un coup à partir de son index
        void play_index_move_sound(int);

};


// Fonction qui obtient la case correspondante à la position sur la GUI
pair<int, int> get_pos_from_gui(int, int);

// Fonction qui permet de changer l'orientation du plateau
void switch_orientation();

// Fonction aidant à l'affichage du plateau (renvoie i si board_orientation, et 7 - i sinon)
int orientation_index(int);