#include <iostream>
#include <vector>
#include <execution>
#include <array>
#include <string>
#include "evaluation.h"
#include "neural_network.h"
#include <vector>
#include <map>
#include <cstdint>
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
// extern uint_fast8_t _global_moves[1000];
// extern int _global_moves_size;


// Nombre maximum de coups légaux par position estimé
// const int _max_moves = 218;
const int _max_moves = 100; // ça n'arrivera quasi jamais que ça dépasse ce nombre


// Coup (défini par ses coordonnées)
// TODO : l'utiliser !!
typedef struct Move {
    uint_fast8_t x1 : 3;
    uint_fast8_t y1 : 3;
    uint_fast8_t x2 : 3;
    uint_fast8_t y2 : 3;

    bool operator== (const Move& other) const {
        return (x1 == other.x1) && (y1 == other.y1) && (x2 == other.x2) && (y2 == other.y2);
    }
};


// Droits de roque (pour optimiser la place mémoire)
struct CastlingRights {
    bool k_w : 1; // Kingside - White
    bool q_w : 1; // Queenside - White
    bool k_b : 1; // Kingside - Black
    bool q_b : 1; // Queenside - Black

    CastlingRights() :
        k_w(true),
        q_w(true),
        k_b(true),
        q_b(true)
    {}

    bool operator== (const CastlingRights& other) const {
        return (k_w == other.k_w) && (q_w == other.q_w) && (k_b == other.k_b) && (q_b == other.q_b);
    }
};



// Plateau
class Board {
    
    public:

        // Attributs

        // Plateau
        uint_fast8_t _array[8][8]  {{ 4, 2, 3, 5, 6, 3, 2, 4 },
                                    { 1, 1, 1, 1, 1, 1, 1, 1 },
                                    { 0, 0, 0, 0, 0, 0, 0, 0 },
                                    { 0, 0, 0, 0, 0, 0, 0, 0 },
                                    { 0, 0, 0, 0, 0, 0, 0, 0 },
                                    { 0, 0, 0, 0, 0, 0, 0, 0 },
                                    { 7, 7, 7, 7, 7, 7, 7, 7 },
                                    {10, 8, 9,11,12, 9, 8,10 }};

        // Coups possibles
        // Nombre max de coups légaux dans une position : 218 -> 872 (car 1 moves = 4 coord)
        uint_fast8_t _moves[_max_moves * 4]; 

        //Move _moves_test[_max_moves];

        // Les coups sont-ils actualisés? Si non : -1, sinon, _got_moves représente le nombre de coups jouables
        int _got_moves = -1;

        // Les coups sont-ils triés?
        bool _sorted_moves = false;

        // Tri rapide
        bool _quick_sorted_moves = false;

        // Les coups sont-ils pseudo-légaux? (sinon, légaux...)
        bool _pseudo_moves = false;

        // Tour du joueur (true pour les blancs, false pour les noirs)
        bool _player = true;

        // Couleur du joueur (1 = blanc, -1 = noir)
        int _color = 1;

        // Peut-être utile pour les optimisations?
        float _evaluation = 0.0f;

        // Position mat (pour les calculs de mat plus rapides)
        bool _mate = false;

        // Droits de roque
        CastlingRights _castling_rights;

        // En passant possible (se note par exemple "d6" -> coordonnée de la case)
        string _en_passant = "-";

        // Nombre de demi-coups (depuis le dernier déplacement de pion, ou la dernière capture, et reste nul à chacun de ces coups)
        int _half_moves_count = 0;

        // Nombre de coups de la partie
        int _moves_count = 1;

        // La partie est-elle finie
        bool _is_game_over = false;

        // FEN du plateau
        string _fen = "";

        // PGN du plateau
        string _pgn = "";

        // Dernier coup joué (coordonnées, pièce) ( *2 pour les roques...)
        int_fast8_t _last_move[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

        // Plateau libre ou actif? (pour le buffer)
        bool _is_active = false;

        // Liste des index des plateaux fils dans le buffer (changer la structure de données?)
        int *_index_children = nullptr;

        // Nombre de coups déjà testés
        int _tested_moves = 0;

        // Coup auquel il en est dans sa recherche
        int _current_move = 0;

        // Evaluation des fils
        int *_eval_children = nullptr;

        // Nombre de noeuds
        int _nodes = 0;

        // Noeuds des enfants
        int *_nodes_children = nullptr;

        // Est-ce que le plateau a été évalué?
        bool _evaluated = false;

        // Adresse de l'évaluateur
        Evaluator *_evaluator = nullptr;

        // Le plateau a t-il été initialisé?
        bool _new_board = true;

        // Activité des pièces
        int _piece_activity = 0;
        bool _activity = false; // à renommer pour plus de lisibilité

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

        // Structure de pions
        int _pawn_structure = 0;
        bool _structure = false;

        // Attaque des pièces
        int _attacks_eval = 0;
        bool _attacks = false;

        // Defense des pièces
        int _defenses_eval = 0;
        bool _defenses = false;

        // Opposition des rois en finale
        int _kings_opposition = 0;
        bool _opposition = false;

        // Matériel
        int _material_count = 0;
        bool _material = false;

        // Avancement de la partie
        float _adv = 0;
        bool _advancement = false;

        // Positionnement des pièces
        int _pos = 0;
        bool _positioning = false;

        // Tour sur colonne ouverte
        int _rook_open = 0;
        bool _rook_open_file = false;

        // Tour sur colonne semi-ouverte
        int _rook_semi = 0;
        bool _rook_semi_open_file = false;

        // Contrôle des cases
        int _control = 0;
        bool _square_controls = false;

        // Chances de gain/nulle/perte
        float _white_winning_chance = 0.0f;
        float _drawing_chance = 0.0f;
        float _black_winning_chance = 0.0f;
        bool _winning_chances = false;

        // Chances de gain des fils

        // Est-ce que la vérification de mat a déjà été faite?
        bool _mate_checked = false;
        int _mate_value = -1;

        // Est-ce que le calcul de game over a déjà été fait?
        bool _game_over_checked = false;
        int _game_over_value = 0;

        // Nombre de noeuds regardés par le quiescence search
        int _quiescence_nodes = 0;


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
        bool add_move(uint_fast8_t, uint_fast8_t, uint_fast8_t, uint_fast8_t, int*);

        // Fonction qui ajoute un coup dans la liste de coups
        // bool add_move(uint_fast8_t, uint_fast8_t, uint_fast8_t, uint_fast8_t, int*);

        // Fonction qui ajoute les coups "pions" dans la liste de coups
        bool add_pawn_moves(uint_fast8_t, uint_fast8_t, int*);

        // Fonction qui ajoute les coups "cavaliers" dans la liste de coups
        bool add_knight_moves(uint_fast8_t, uint_fast8_t, int*);

        // Fonction qui ajoute les coups diagonaux dans la liste de coups
        bool add_diag_moves(uint_fast8_t, uint_fast8_t, int*);

        // Fonction qui ajoute les coups horizontaux et verticaux dans la liste de coups
        bool add_rect_moves(uint_fast8_t, uint_fast8_t, int*);

        // Fonction qui ajoute les coups "roi" dans la liste de coups
        bool add_king_moves(uint_fast8_t, uint_fast8_t, int*);

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
        void make_move(uint_fast8_t, uint_fast8_t, uint_fast8_t, uint_fast8_t, bool pgn = false, bool new_board = false, bool add_to_list = false);

        // Fonction qui joue le coup i de la liste des coups possibles
        void make_index_move(int, bool pgn = false, bool add_to_list = false);

        // Fonction qui renvoie l'avancement de la partie (0 = début de partie, 1 = fin de partie)
        void game_advancement();

        // Fonction qui compte le matériel sur l'échiquier
        void count_material(Evaluator *e = nullptr);

        // Fonction qui calcule les valeurs de positionnement des pièces sur l'échiquier
        void pieces_positionning(Evaluator *e = nullptr);

        // Fonction qui évalue la position à l'aide d'heuristiques
        bool evaluate(Evaluator *e = nullptr, bool checkmates = false, bool display = false, Network *n = nullptr);

        // Fonction qui évalue la position à l'aide d'heuristiques -> évaluation entière
        bool evaluate_int(Evaluator *e = nullptr, bool checkmates = false, bool display = false, Network *n = nullptr);

        // Fonction qui joue le coup d'une position, renvoyant la meilleure évaluation à l'aide d'un negamax (similaire à un minimax)
        float negamax(int, float, float, int, bool, Evaluator *, bool, bool);

        // Version un peu mieux optimisée de Grogrosfish
        bool grogrosfish(int, Evaluator *, bool);

        // Fonction qui revient à la position précédente
        bool undo(uint_fast8_t, uint_fast8_t, uint_fast8_t, uint_fast8_t, uint_fast8_t, uint_fast8_t, int);

        // Une surcharge
        bool undo();

        // Fonction qui arrange les coups de façon "logique", pour optimiser les algorithmes de calcul
        void sort_moves(Evaluator *);

        // Fonction qui récupère le plateau d'un FEN
        void from_fen(string, bool fen_in_pgn = true, bool keep_headings = true);

        // Fonction qui renvoie le FEN du tableau
        void to_fen();

        // Fonction qui renvoie le gagnant si la partie est finie (-1/1), et 0 sinon
        int game_over();

        // Fonction qui renvoie le label d'un coup
        string move_label(uint_fast8_t, uint_fast8_t, uint_fast8_t, uint_fast8_t);

        // Fonction qui renvoie le label d'un coup en fonction de son index
        string move_label_from_index(int);

        // Fonction qui fait un coup à partir de son label
        void make_label_move(string);

        // Fonction qui renvoie un plateau à partir d'un PGN
        void from_pgn(string);

        // Fonction qui affiche un texte dans une zone donnée
        void draw_text_rect(string, float, float, float, float, int);

        // Fonction qui dessine le plateau
        bool draw();
        
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
        bool play_monte_carlo_move_keep(int, bool keep = true, bool keep_display = false, bool display = false, bool add_to_list = false);

        // Pas très opti pour l'affichage, mais bon... Fonction qui cherche la profondeur la plus grande dans la recherche de Monté-Carlo
        int max_monte_carlo_depth();

        // Algo de grogros_zero
        void grogros_zero(Evaluator *eval = nullptr, int nodes = 1, bool checkmates = false, float beta = 0.035f, float k_add = 50.0f, bool display = false, int depth = 0, Network *net = nullptr, int quiescence_depth = 4);

        // Fonction qui réinitialise le plateau dans son état de base (pour le buffer)
        void reset_board(bool display = false);

        // Fonction qui réinitialise tous les plateaux fils dans le buffer
        void reset_all(bool self = true, bool display = false);

        // Fonction qui renvoie le nombre de noeuds calculés par GrogrosZero
        int total_nodes();

        // Fonction qui calcule la sécurité des rois
        void get_king_safety(int piece_attack = 50, int piece_defense = 5, int pawn_attack = 25, int pawn_defense = 100, int edge_defense = 125);

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

        // Fonction qui génère le livre d'ouvertures
        void generate_opening_book(int nodes = 100000);

        // Fonction qui renvoie une représentation simple et rapide de la position
        string simple_position();

        // Fonction qui calcule la structure de pions
        void get_pawn_structure();

        // Fonction qui met à jour le temps des joueurs
        void update_time();

        // Fonction qui lance le temps
        void start_time();

        // Fonction qui stoppe le temps
        void stop_time();

        // Fonction qui calcule la résultante des attaques et des défenses
        void get_attacks_and_defenses();

        // Fonction qui calcule l'opposition des rois (en finales de pions)
        void get_kings_opposition();

        // Fonction qui renvoie le type de pièce sélectionnée
        uint_fast8_t selected_piece();

        // Fonction qui renvoie le type de pièce où la souris vient de cliquer
        uint_fast8_t clicked_piece();

        // Fonction qui renvoie si la pièce sélectionnée est au joueur ayant trait ou non
        bool selected_piece_has_trait();

        // Fonction qui renvoie si la pièce cliquée est au joueur ayant trait ou non
        bool clicked_piece_has_trait();

        // Fonction qui remet les compteurs de temps "à zéro" (temps de base)
        void reset_timers();

        // Fonction qui remet le plateau dans sa position initiale
        void restart();

        // Fonction qui renvoie la différence matérielle entre les deux camps
        int material_difference();

        // Fonction qui réinitialise les composantes de l'évaluation
        void reset_eval();

        // Fonction qui compte les tours sur les colonnes ouvertes et semi-ouvertes
        void get_rook_on_open_file();

        // Fonction qui renvoie la profondeur de calcul de la variante principale
        int grogros_main_depth();

        // Fonction qui calcule la valeur des cases controllées sur l'échiquier
        void get_square_controls();

        // Fonction qui calcule les chances de gain/nulle/perte
        void get_winning_chances();

        // Fonction qui sélectionne et renvoie le coup avec le meilleur UCT
        int select_uct(float c = 1.0f);

        // Fonction qui fait un tri rapide des coups (en plaçant les captures en premier)
        bool quick_moves_sort();

        // Fonction qui fait un quiescence search
        int quiescence(Evaluator *, int, int, int depth = 4);

        // Fonction qui fait un quiescence search, avec des méthodes de pruning avancées
        int quiescence_improved(Evaluator*, int, int, int depth = 4);

        // Fonction qui renvoie le i-ème coup
        int* get_i_move(int);

        // Fonction qui fait cliquer le i-ème coup
        bool click_i_move(int i, bool orientation);
};


// Fonction qui obtient la case correspondante à la position sur la GUI
pair<int, int> get_pos_from_gui(float, float);

// Fonction qui permet de changer l'orientation du plateau
void switch_orientation();

// Fonction aidant à l'affichage du plateau (renvoie i si board_orientation, et 7 - i sinon)
int orientation_index(int);

// Paramètres pour la recherche de Monte-Carlo
extern float _beta;
extern float _k_add; 



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
        void init(int length = 5000000);

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

// Fonction qui renvoie si deux positions (en format FEN) sont les mêmes
bool equal_fen(string, string);

// Fonction qui renvoie si deux positions (en format FEN) sont les mêmes (pour les répétitions)
bool equal_positions(Board, Board);

// Test de liste des positions (taille 100, pour la règle des 50 coups.. si on joue une prise ou un coup de pion, on peut reset la liste -> 52 : +1 pour la position de départ, +1 quand on joue exactement le 50ème coup)
extern string _all_positions[102];
extern int _total_positions;


// Fonction qui renvoie le temps que l'IA doit passer sur le prochain coup (en ms), en fonction d'un facteur k, et des temps restant
int time_to_play_move(int t1, int t2, float k = 0.05);



// std::map<string, int> _positions_history = {
//     { "A", 1 },
//     { "B", 1 },
//     { "C", 2 }
// };

// Fonction qui renvoie la valeur UCT
float uct(float, float, int, int);


// GUI
class GUI {
    public:
        // Variables


        // Dimensions de la fenêtre
        int _screen_width = 1800;
        int _screen_height = 945;

        // Plateau affiché
        Board _board;

        // Faut-il faire l'affichage?
        bool _draw = true;
        
        // Faut-il que les coups soient cliqués en binding chess.com?
        bool _click_bind = false;

        // Mode de jeu automatique en binding avec chess.com
        bool _binding_full = false; // Pour récupérer tous les coups de la partie
        bool _binding_solo = false; // Pour récupérer seulement les coups de la couleur du joueur du bas

        // Intervalle de tmeps pour check chess.com
        int _binding_interval_check = 100;

        // Moment du dernier check
        clock_t _last_binding_check = clock();

        // Coup récupéré par le binding
        int* _binding_move = new int[4];

        // Coordonnées du plateau sur chess.com
        int _binding_left = 108; // (+10 si barre d'éval)
        int _binding_top = 219;
        int _binding_right = 851;
        int _binding_bottom = 962;

        // Coordonées du plateau pour le binding
        //SimpleRectangle _binding_coord;

        // Joueurs
        string _white_player = "White";
        string _black_player = "Black";

        // Temps des joueurs
        clock_t _time_white = 900000;
        clock_t _time_black = 900000;

        // Incrément (5s/coup)
        clock_t _time_increment_white = 5000;
        clock_t _time_increment_black = 5000;

        // Mode analyse de Grogros
        bool _grogros_analysis = false;

        // Le temps est-il activé?
        bool _time = false;

        // Pour la gestion du temps
        clock_t _last_move_clock;

        // Joueur au trait lors du dernier check (pour les temps
        bool _last_player = true;

        // Affichage des flèches : affiche les chances de gain (true), l'évaluation (false)
        bool _display_win_chances = true;


        // Constructeurs

        // Par défaut
        GUI();


        // Fonctions

        // Fonction qui met en place le binding avec chess.com pour une nouvelle partie (et se prépare à jouer avec GrogrosZero)
        bool new_bind_game();

        // Fonction qui met en place le binding avec chess.com pour une nouvelle analyse de partie
        bool new_bind_analysis();
};

// instantiation de la GUI globale
extern GUI _GUI;