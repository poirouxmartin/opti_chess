#include <iostream>
#include <vector>
#include <execution>
#include <array>
#include <string>
#include "evaluation.h"
#include "agent.h"
#include "neural_network.h"
#include <vector>
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



// Liste de coups globale, pour les calculs, et éviter d'avoir des listes trop grosses pour chaque plateau
extern uint_fast8_t _global_moves[1000];
extern int _global_moves_size;



class Board {
    
    public:

        // Attributs

        // Plateau
        uint_fast8_t _array[8][8]  {{4, 2, 3, 5, 6, 3, 2, 4}, 
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
        uint_fast8_t _moves[1000];

        // Liste des coups, sous forme de vecteur
        vector<uint_fast8_t> _moves_vector; // A changer par dynamic array? Peut-être pas car il faudrait delete
        

        // Les coups sont-ils actualisés? Si non : -1, sinon, _got_moves représente le nombre de coups jouables
        int _got_moves = -1;

        // Ordre des coups à jouer (pour l'optimisation)
        uint_fast8_t _move_order[250];
        // Les coups sont-ils triés?
        bool _sorted_moves = false;
        // Les coups sont-ils pseudo-légaux? (sinon, légaux...)
        bool _pseudo_moves = false;

        // Tour du joueur (true pour les blancs, false pour les noirs)
        bool _player = true;

        // Couleur du joueur (1 = blanc, -1 = noir)
        int _color = 1;

        // Peut-être utile pour les optimisations?
        float _evaluation = 0;

        // Position mat (pour les calculs de mat plus rapides)
        bool _mate = false;

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

        // La partie est-elle finie
        bool _is_game_over = false;


        // FEN du plateau
        string _fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        // PGN du plateau
        string _pgn = "";

        // Dernier coup joué (coordonnées, pièce) ( *2 pour les roques...)
        int_fast8_t _last_move[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};


        // Joueurs de la partie
        string _white_player = "Player 1";
        string _black_player = "Player 2";

        // Temps pour les joueurs
        bool _time = false;

        // 15 minutes par personne
        clock_t _time_white = 900000;
        clock_t _time_black = 900000;

        // Incrément (5s/coup)
        clock_t _time_increment_white = 5000;
        clock_t _time_increment_black = 5000;

        // Plateau libre ou actif? (pour le buffer)
        bool _is_active = false;

        // Liste des index des plateaux fils dans le buffer (changer la structure de données?)
        int *_index_children;

        // Nombre de coups déjà testés
        int _tested_moves = 0;

        // Coup auquel il en est dans sa recherche
        int _current_move = 0;

        // Evaluation des fils
        int *_eval_children;

        // Nombre de noeuds
        int _nodes = 0;

        // Noeuds des enfants
        int *_nodes_children;

        // Est-ce que le plateau a été évalué?
        bool _evaluated = false;

        // Le plateau a t-il été initialisé?
        bool _new_board = true;

        // Activité des pièces
        int _piece_activity = 0;
        bool _activity = false;

        // Pour l'affichage
        int _static_evaluation = 0;

        // Sécurité du roi
        int _king_safety = 0;
        bool _safety = false;

        // Est-ce que les noms des joueurs ont été ajoutés au PGN
        bool _named_pgn = false;
        bool _timed_pgn = false;

        // Paramètres pour éviter de tout recalculer pour le draw() avec les stats de Monte-Carlo
        bool _monte_called = false;

        // Temps passé sur l'anayse de Monte-Carlo
        clock_t _time_monte_carlo = 0;
        

        // Constructeur par défaut
        Board();

        // Constructeur de copie
        Board(Board&);

        // Fonction qui copie les attributs d'un plateau
        void copy_data(Board&);

        // Fonction qui copie les coups d'un plateau
        void copy_moves(Board&);

        // Affichage du plateau
        void display();

        // Fonction qui à partir des coordonnées d'un coup renvoie le coup codé sur un entier (à 4 chiffres)
        int move_to_int(int, int, int, int);

        // Fonction qui ajoute un coup dans la liste de coups
        bool add_move(int, int, int, int, int*);

        // Fonction qui ajoute les coups "pions" dans la liste de coups
        bool add_pawn_moves(int, int, int*);

        // Fonction qui ajoute les coups "cavaliers" dans la liste de coups
        bool add_knight_moves(int, int, int*);

        // Fonction qui ajoute les coups diagonaux dans la liste de coups
        bool add_diag_moves(int, int, int*);

        // Fonction qui ajoute les coups horizontaux et verticaux dans la liste de coups
        bool add_rect_moves(int, int, int*);

        // Fonction qui ajoute les coups "roi" dans la liste de coups
        bool add_king_moves(int, int, int*);

        // Fonction qui génère la liste des coups sous forme de vecteur
        bool get_moves_vector();

        // Renvoie la liste des coups possibles
        bool get_moves(bool pseudo = false, bool forbide_check = false);

        // Fonction qui dit si une case est attaquée
        bool attacked(int, int);

        // Fonction qui donne la position du roi du joueur
        pair<int, int> get_king_pos();

        // Fonction qui dit s'il y'a échec
        bool in_check();

        // Fonction qui affiche la liste des coups
        void display_moves(bool pseudo = false);

        // Fonction qui joue un coup
        void make_move(int, int, int, int, bool pgn = false, bool new_board = false);

        // Fonction qui joue le coup i de la liste des coups possibles
        void make_index_move(int, bool pgn = false);

        // Fonction qui renvoie l'avancement de la partie (0 = début de partie, 1 = fin de partie)
        float game_advancement();

        // Fonction qui évalue la position à l'aide d'heuristiques
        bool evaluate(Evaluator *e = nullptr, bool checkmates = false, bool display = false, Network *n = nullptr);

        // Fonction qui évalue la position à l'aide d'heuristiques -> évaluation entière
        bool evaluate_int(Evaluator *e = nullptr, bool checkmates = false, bool display = false, Network *n = nullptr);

        // Fonction qui évalue la position à l'aide d'un agent
        void evaluate(Agent);

        // Fonction qui joue le coup d'une position, renvoyant la meilleure évaluation à l'aide d'un negamax (similaire à un minimax)
        float negamax(int, float, float, int, bool, Evaluator *, Agent, bool, bool, bool);

        // Version un peu mieux optimisée de Grogrosfish
        bool grogrosfish(int, Evaluator *, bool);

        // Fonction qui revient à la position précédente
        bool undo(int, int, int, int, int, int, int);

        // Une surcharge
        bool undo();

        // Fonction qui arrange les coups de façon "logique", pour optimiser les algorithmes de calcul
        void sort_moves(Evaluator *);

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
        void from_pgn(string);

        // Fonction qui affiche un texte dans une zone donnée
        void draw_text_rect(string, float, float, float, float, int);

        // Fonction qui dessine le plateau
        void draw();
        
        // Fonction qui joue le son d'un coup
        void play_move_sound(int, int, int, int);

        // Fonction qui joue le son d'un coup à partir de son index
        void play_index_move_sound(int);

        // Fonction qui joue le coup après analyse par l'algo de Monte Carlo
        void play_monte_carlo_move(bool display = false);

        // Fonction qui dessine les flèches en fonction des valeurs dans l'algo de Monte-Carlo d'un plateau
        void draw_monte_carlo_arrows();

        // Fonction qui calcule l'activité des pièces
        void get_piece_activity(bool legal = false);

        // Fonction qui renvoie le meilleur coup selon l'analyse faite par l'algo de Monte-Carlo
        int best_monte_carlo_move();

        // Fonction qui joue le coup après analyse par l'algo de Monte-Carlo, et qui garde en mémoire les infos du nouveau plateau
        void play_monte_carlo_move_keep(int, bool keep = true, bool keep_display = false, bool display = false);

        // Pas très opti pour l'affichage, mais bon... Fonction qui cherche la profondeur la plus grande dans la recherche de Monté-Carlo
        int max_monte_carlo_depth();

        // Algo de grogros_zero
        void grogros_zero(Evaluator *eval = nullptr, int nodes = 1, bool checkmates = false, double beta = 0.035, int k_add = 50, bool display = false, int depth = 0, Network *net = nullptr);

        // Fonction qui réinitialise le plateau dans son état de base (pour le buffer)
        void reset_board(bool display = false);

        // Fonction qui réinitialise tous les plateaux fils dans le buffer
        void reset_all(bool self = true, bool display = false);

        // Fonction qui renvoie le nombre de noeuds calculés par GrogrosZero
        int total_nodes();

        // Fonction qui calcule la sécurité des rois
        void get_king_safety(float game_adv, int piece_attack = 50, int piece_defense = 15, int pawn_attack = 10, int pawn_defense = 50, int edge_defense = 100);

        // Fonction qui renvoie s'il y a échec et mat (ou pat) (-1, 1 ou 0)
        int is_mate();

        // Fonction qui dit si une pièce est capturable par l'ennemi (pour les affichages GUI)
        bool is_capturable(int, int);

        // Fonction qui affiche le PGN
        void display_pgn();

        // Fonction qui ajoute les noms des gens au PGN
        void add_names_to_pgn();

        // Fonction qui ajoute le time control au PGN
        void add_time_to_pgn();

        // Fonction qui renvoie en chaîne de caractères la meilleure variante selon monte carlo
        string get_monte_carlo_variant(bool evaluate_final_pos = false);

        // Fonction qui trie les index des coups par nombre de noeuds décroissant
        vector<int> sort_by_nodes(bool ascending = false);

        // Fonction qui renvoie selon l'évaluation si c'est un mat ou non
        int is_eval_mate(int);

};


// Fonction qui obtient la case correspondante à la position sur la GUI
pair<int, int> get_pos_from_gui(float, float);

// Fonction qui permet de changer l'orientation du plateau
void switch_orientation();

// Fonction aidant à l'affichage du plateau (renvoie i si board_orientation, et 7 - i sinon)
int orientation_index(int);

// Fonction qui joue un match entre deux agents, et donne du score à l'agent correspondant (le premier agent a les blancs)
int match(Agent&, Agent&);

// Fonction qui fait un tournoi d'agents, et retourne la liste des scores
int* tournament(Agent*, int);

// Paramètres pour la recherche de Monte-Carlo
extern double _beta;
extern int _k_add; 



class Buffer {
    
    public:

        bool _init = false;
        int _length = 0;
        Board *_heap_boards;

        // Itérateur pour rechercher moins longtemps un index de plateau libre
        int _iterator = -1;


        // Constructeur par défaut
        Buffer();

        // Constructeur utilisant la taille max (en bits) du buffer
        Buffer(unsigned long int);

        // Initialize l'allocation de n plateaux
        void init(int length = 3000000);

        // Fonction qui donne l'index du premier plateau de libre dans le buffer
        int get_first_free_index();

        // Fonction qui désalloue toute la mémoire
        void remove();

};


// Buffer pour monte-carlo
extern Buffer _monte_buffer;


// Fonction qui joue un match entre deux IA utilisant GrogrosZero, et une évaluation par réseau de neurones ou des évaluateurs, avec un certain nombre de noeuds de calcul
int match(Evaluator *e_white = nullptr, Evaluator *e_black = nullptr, Network *n_white = nullptr, Network *n_black = nullptr, int nodes = 1000, bool display = false, int max_moves = 100);

// Fonction qui organise un tournoi entre les IA utilisant évaluateurs et réseaux de neurones des listes et renvoie la liste des scores (dépendant des nombres par victoires/nulles, et leur valeur)
int* tournament(Evaluator **, Network **, int, int nodes = 1000, int victory = 3, int draw = 1, bool display_full = false, int max_moves = 100);
