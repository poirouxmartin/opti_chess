#include <iostream>
#include <vector>
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


class Board
{
    public:

        // Attributs

        // Plateau
        int _array[8][8] {{4, 2, 3, 5, 6, 3, 2, 4}, {1, 1, 1, 1, 1, 1, 1, 1}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {7, 7, 7, 7, 7, 7, 7, 7}, {10, 8, 9, 11, 12, 9, 8, 10}};

        // Coups possibles
        // (Augmenter si besoin)
        // On suppose ici que n_moves < 1000 / 4
        int _moves[1000];
        // Les coups sont-ils actualisés?
        int _got_moves = -1;

        // Tour du joueur (true pour les blancs, false pour les noirs)
        bool _player = true;

        // Couleur du joueur (1 = blanc, -1 = noir)
        int _color = 1;

        // Peut-être utile pour les optimisations?
        float _evaluation = 0;

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

        

        // Constructeur par défaut
        Board();

        // Constructeur de copie
        Board(Board &);

        // Fonction qui copie les attributs d'un tableau
        void copy_data(Board &);

        // Affichage du plateau
        void display();

        // Fonction qui ajoute un coup dans la liste de coups
        void add_move(int, int, int, int, int *);

        // Fonction qui ajoute les coups "pions" dans la liste de coups
        void add_pawn_moves(int, int, int *);

        // Fonction qui ajoute les coups "cavaliers" dans la liste de coups
        void add_knight_moves(int, int, int *);

        // Fonction qui ajoute les coups diagonaux dans la liste de coups
        void add_diag_moves(int, int, int *);

        // Fonction qui ajoute les coups horizontaux et verticaux dans la liste de coups
        void add_rect_moves(int, int, int *);

        // Fonction qui ajoute les coups "roi" dans la liste de coups
        void add_king_moves(int, int, int *);

        // Renvoie la liste des coups possibles
        int* get_moves();

        // Fonction qui affiche la liste des coups
        void display_moves();

        // Fonction qui joue un coup
        void make_move(int, int, int, int);

        // Fonction qui joue le coup i de la liste des coups possibles
        void make_index_move(int);

        // Fonction qui évalue la position à l'aide d'heuristiques
        void evaluate();

        // Fonction qui joue le coup d'une position, renvoyant la meilleure évaluation à l'aide d'un negamax (similaire à un minimax)
        float negamax(int, float, float, int);

        // Fonction qui utilise minimax pour déterminer quel est le "meilleur" coup et le joue
        void grogrosfish(int);
        
        // Fonction qui revient à la position précédente
        void undo();

        // Fonction qui arrange les coups de façon "logique", pour optimiser les algorithmes de calcul
        void sort_moves();

        // Fonction qui récupère le plateau d'un FEN
        void from_fen(string);

        // Fonction qui renvoie le FEN du tableau
        void to_fen();

};




