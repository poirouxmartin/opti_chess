#pragma once
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
#include "raylib.h"

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
// const int max_moves = 218;
constexpr int max_moves = 100; // ça n'arrivera quasi jamais que ça dépasse ce nombre


// Coup (défini par ses coordonnées)
// TODO : l'utiliser !!
// Utilise 2 bytes (16 bits)
// On peut utiliser 2 bits en plus, car on en utilise seulement 14 là (promotion flag -> sur 4 bits, et on retire capture flag?) (castling flag * 2? en passant?)
// check flag?
typedef struct Move {
    uint_fast8_t i1 : 3;
    uint_fast8_t j1 : 3;
    uint_fast8_t i2 : 3;
    uint_fast8_t j2 : 3;
    bool capture_flag : 1;
    bool promotion_flag : 1;
    // Reste 2 bytes à utiliser : check? castling? en passant?

    bool operator== (const Move& other) const {
        return (i1 == other.i1) && (j1 == other.j1) && (i2 == other.i2) && (j2 == other.j2);
    }

    void display() const
    {
	    		cout << "(" << static_cast<int>(i1) << ", " << static_cast<int>(j1) << ") -> (" << static_cast<int>(i2) << ", " << static_cast<int>(j2) << ")" << endl;
	}
};


// Droits de roque (pour optimiser la place mémoire)
// 1 byte
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


// Tests pour une représentation plus compacte d'un plateau
#pragma pack(push, 1)
struct Piece
{
    int type : 3;
    bool color : 1;
};

// Ligne du plateau d'échec (horizontale)
// 40 bytes (Minimum possible pour une représentation board-centric : 32 bytes)
struct Array
{
    Piece pieces[8][8];
};
#pragma pack(pop)


// Position sur le plateau
struct Pos
{
    int i : 4;
    int j : 4;
};



// Plateau
class Board {
    
    public:

    // Attributs

    // Plateau
    // 64 bytes
    uint_fast8_t _array[8][8]{  { 4, 2, 3, 5, 6, 3, 2, 4 },
                                { 1, 1, 1, 1, 1, 1, 1, 1 },
                                { 0, 0, 0, 0, 0, 0, 0, 0 },
                                { 0, 0, 0, 0, 0, 0, 0, 0 },
                                { 0, 0, 0, 0, 0, 0, 0, 0 },
                                { 0, 0, 0, 0, 0, 0, 0, 0 },
                                { 7, 7, 7, 7, 7, 7, 7, 7 },
                                {10, 8, 9,11,12, 9, 8,10 } };

    // Coups possibles
    // Nombre max de coups légaux dans une position : 218

    // 200 bytes
    Move _moves[max_moves];

    // Les coups sont-ils actualisés? Si non : -1, sinon, _got_moves représente le nombre de coups jouables
	// En supposant que le nombre de coups n'excède pas 127
    // 1 byte
    int_fast8_t _got_moves = -1;

    // Les coups sont-ils triés?
    bool _sorted_moves = false;

    // Tri rapide
    bool _quick_sorted_moves = false;

    // Les coups sont-ils pseudo-légaux? (sinon, légaux...)
    bool _pseudo_moves = false;

    // Tour du joueur (true pour les blancs, false pour les noirs)
    bool _player = true;

    // Peut-être utile pour les optimisations?
    float _evaluation = 0.0f;

    // Position mat (pour les calculs de mat plus rapides)
    bool _mate = false;

    // Droits de roque
    CastlingRights _castling_rights;

    // Colonne d'en passant
    int_fast8_t _en_passant_col = -1;

    // Nombre de demi-coups (depuis le dernier déplacement de pion, ou la dernière capture, et reste nul à chacun de ces coups)
    uint_fast8_t _half_moves_count = 0;

    // Nombre de coups de la partie
    short _moves_count = 1;

    // La partie est-elle finie
    bool _is_game_over = false;

    // FEN du plateau
    // 32 bytes?
    string _fen;

    // 32 bytes
    // PGN du plateau
    string _pgn;

    // Dernier coup joué (coordonnées, pièce) ( *2 pour les roques...)
	// 10 bytes
    int_fast8_t _last_move[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

    // Plateau libre ou actif? (pour le buffer)
    bool _is_active = false;

    // Liste des index des plateaux fils dans le buffer (changer la structure de données?)
    int *_index_children = nullptr;

    // Nombre de coups déjà testés
    uint_fast8_t _tested_moves = 0;

    // Coup auquel il en est dans sa recherche
    uint_fast8_t _current_move = 0;

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

    // Pour l'affichage
    int _static_evaluation = 0;

    // Est-ce que les noms des joueurs ont été ajoutés au PGN
    // TODO : à mettre dans la GUI
    bool _named_pgn = false;
    bool _timed_pgn = false;

    // Paramètres pour éviter de tout recalculer pour le draw() avec les stats de Monte-Carlo
    bool _monte_called = false;

    // Temps passé sur l'anayse de Monte-Carlo
    clock_t _time_monte_carlo = 0;

    // Avancement de la partie
    float _adv = 0.0f;
    bool _advancement = false;

    // Chances de gain/nulle/perte (4 bytes / 4 bytes / 4 bytes) -> 12 bytes
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

    // On stocke les positions des rois
    Pos _white_king_pos = {0, 4};
    Pos _black_king_pos = {6, 4};

    // Est-ce qu'on a affiché les composantes du plateau?
    bool _displayed_components = false;


    // Constructeur par défaut
    Board();

    // Constructeur de copie
    Board(const Board&);

    // Fonction qui copie les attributs d'un plateau
    void copy_data(const Board&);

    // Fonction qui copie les coups d'un plateau
    void copy_moves(const Board&);

    // Affichage du plateau
    void display() const;

    // Fonction qui ajoute un coup dans la liste de coups
    bool add_move(uint_fast8_t, uint_fast8_t, uint_fast8_t, uint_fast8_t, int*, uint_fast8_t);

    // Fonction qui ajoute les coups "pions" dans la liste de coups
    bool add_pawn_moves(uint_fast8_t, uint_fast8_t, int*, uint_fast8_t);

    // Fonction qui ajoute les coups "cavaliers" dans la liste de coups
    bool add_knight_moves(uint_fast8_t, uint_fast8_t, int*, uint_fast8_t);

    // Fonction qui ajoute les coups diagonaux dans la liste de coups
    bool add_diag_moves(uint_fast8_t, uint_fast8_t, int*, uint_fast8_t);

    // Fonction qui ajoute les coups horizontaux et verticaux dans la liste de coups
    bool add_rect_moves(uint_fast8_t, uint_fast8_t, int*, uint_fast8_t);

    // Fonction qui ajoute les coups "roi" dans la liste de coups
    bool add_king_moves(uint_fast8_t, uint_fast8_t, int*, uint_fast8_t);

    // Renvoie la liste des coups possibles
    bool get_moves(const bool pseudo = false, const bool forbide_check = false);

    // Fonction qui dit si une case est attaquée
    [[nodiscard]] bool attacked(int, int) const;

    // Fonction qui donne la position du roi du joueur
    [[nodiscard]] pair<int, int> get_king_pos() const;

    // Fonction qui dit s'il y'a échec
    [[nodiscard]] bool in_check();

    // Fonction qui affiche la liste des coups
    void display_moves(bool pseudo = false);

    // Fonction qui joue un coup
    void make_move(uint_fast8_t, uint_fast8_t, uint_fast8_t, uint_fast8_t, bool pgn = false, bool new_board = false, bool add_to_list = false);

    // Fonction qui joue le coup i de la liste des coups possibles
    void make_index_move(int, bool pgn = false, bool add_to_list = false);

    // Fonction qui renvoie l'avancement de la partie (0 = début de partie, 1 = fin de partie)
    void game_advancement();

    // Fonction qui compte le matériel sur l'échiquier et renvoie sa valeur
    int count_material(const Evaluator *e = nullptr) const;

    // Fonction qui calcule et renvoie la valeur de positionnement des pièces sur l'échiquier
    int pieces_positioning(const Evaluator *eval = nullptr) const;

    // Fonction qui évalue la position à l'aide d'heuristiques
    bool evaluate(Evaluator *eval = nullptr, bool checkmates = false, bool display = false, Network *n = nullptr);

    // Fonction qui évalue la position à l'aide d'heuristiques -> évaluation entière
    bool evaluate_int(Evaluator *eval = nullptr, bool checkmates = false, bool display = false, Network *n = nullptr);

    // Fonction qui joue le coup d'une position, renvoyant la meilleure évaluation à l'aide d'un negamax (similaire à un minimax)
    float negamax(int, float, float, bool, Evaluator *, bool play = false, bool display = false, int quiescence_depth = 4);

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

    // Fonction qui renvoie un plateau à partir d'un PGN
    void from_pgn(string);

    // Fonction qui affiche un texte dans une zone donnée
    static void draw_text_rect(const string&, float, float, float, float, float);

    // Fonction qui dessine le plateau
    bool draw();
    
    // Fonction qui joue le son d'un coup
    void play_move_sound(uint_fast8_t, uint_fast8_t, uint_fast8_t, uint_fast8_t) const;

    // Fonction qui joue le son d'un coup à partir de son index
    void play_index_move_sound(int) const;

    // Fonction qui dessine les flèches en fonction des valeurs dans l'algo de Monte-Carlo d'un plateau
    void draw_monte_carlo_arrows() const;

    // Fonction qui calcule et renvoie la mobilité des pièces
    [[nodiscard]] int get_piece_mobility(bool legal = false) const;

    // Fonction qui renvoie le meilleur coup selon l'analyse faite par l'algo de Monte-Carlo
    [[nodiscard]] int best_monte_carlo_move() const;

    // Fonction qui joue le coup après analyse par l'algo de Monte-Carlo, et qui garde en mémoire les infos du nouveau plateau
    bool play_monte_carlo_move_keep(int, bool keep = true, bool keep_display = false, bool display = false, bool add_to_list = false);

    // Pas très opti pour l'affichage, mais bon... Fonction qui cherche la profondeur la plus grande dans la recherche de Monté-Carlo
    [[nodiscard]] int max_monte_carlo_depth() const;

    // Algo de grogros_zero
    void grogros_zero(Evaluator *eval = nullptr, int nodes = 1, bool checkmates = false, float beta = 0.035f, float k_add = 50.0f, int quiescence_depth = 4, bool deep_mates_check = true, bool explore_checks = true, bool display = false, int depth = 0, Network *net = nullptr);

    // Fonction qui réinitialise le plateau dans son état de base (pour le buffer)
    void reset_board(bool display = false);

    // Fonction qui réinitialise tous les plateaux fils dans le buffer
    void reset_all(bool self = true, bool display = false);

    // Fonction qui renvoie le nombre de noeuds calculés par GrogrosZero
    [[nodiscard]] int total_nodes() const;

    // Fonction qui calcule et renvoie la valeur correspondante à la sécurité des rois
    int get_king_safety();

    // Fonction qui renvoie s'il y a échec et mat (ou pat) (-1, 1 ou 0)
    int is_mate();

    // Fonction qui dit si une pièce est capturable par l'ennemi (pour les affichages GUI)
    bool is_capturable(int, int);

    // Fonction qui affiche le PGN
    void display_pgn() const;

    // Fonction qui ajoute les noms des gens au PGN
    void add_names_to_pgn();

    // Fonction qui ajoute le time control au PGN
    void add_time_to_pgn();

    // Fonction qui renvoie en chaîne de caractères la meilleure variante selon monte carlo
    string get_monte_carlo_variant(bool evaluate_final_pos = false);

    // Fonction qui trie les index des coups par nombre de noeuds décroissant
    [[nodiscard]] vector<int> sort_by_nodes(bool ascending = false) const;

    // Fonction qui renvoie selon l'évaluation si c'est un mat ou non
    [[nodiscard]] int is_eval_mate(int) const;

    // Fonction qui génère le livre d'ouvertures
    void generate_opening_book(int nodes = 100000);

    // Fonction qui renvoie une représentation simple et rapide de la position
    [[nodiscard]] string simple_position() const;

    // Fonction qui calcule la structure de pions et renvoie sa valeur
    [[nodiscard]] int get_pawn_structure() const;

    // Fonction qui met à jour le temps des joueurs
    void update_time();

    // Fonction qui lance le temps
    void start_time();

    // Fonction qui stoppe le temps
    void stop_time();

    // Fonction qui calcule la résultante des attaques et des défenses et la renvoie
    [[nodiscard]] float get_attacks_and_defenses(float attack_scale = 1.0f, float defense_scale = 1.0f) const;

    // Fonction qui calcule et renvoie l'opposition des rois (en finales de pions)
    int get_kings_opposition();

    // Fonction qui renvoie le type de pièce sélectionnée
    [[nodiscard]] uint_fast8_t selected_piece() const;

    // Fonction qui renvoie le type de pièce où la souris vient de cliquer
    [[nodiscard]] uint_fast8_t clicked_piece() const;

    // Fonction qui renvoie si la pièce sélectionnée est au joueur ayant trait ou non
    [[nodiscard]] bool selected_piece_has_trait() const;

    // Fonction qui renvoie si la pièce cliquée est au joueur ayant trait ou non
    [[nodiscard]] bool clicked_piece_has_trait() const;

    // Fonction qui remet les compteurs de temps "à zéro" (temps de base)
    static void reset_timers();

    // Fonction qui remet le plateau dans sa position initiale
    void restart();

    // Fonction qui renvoie la différence matérielle entre les deux camps
    [[nodiscard]] int material_difference() const;

    // Fonction qui réinitialise les composantes de l'évaluation
    void reset_eval();

    // Fonction qui compte les tours sur les colonnes ouvertes et semi-ouvertes et renvoie la valeur
    [[nodiscard]] int get_rooks_on_open_file() const;

    // Fonction qui renvoie la profondeur de calcul de la variante principale
    [[nodiscard]] int grogros_main_depth() const;

    // Fonction qui calcule la valeur des cases controllées sur l'échiquier
    [[nodiscard]] int get_square_controls() const;

    // Fonction qui calcule les chances de gain/nulle/perte
    void get_winning_chances();

    // Fonction qui sélectionne et renvoie le coup avec le meilleur UCT
    [[nodiscard]] int select_uct(float c = 1.0f) const;

    // Fonction qui fait un tri rapide des coups (en plaçant les captures en premier)
    bool quick_moves_sort();

    // Fonction qui fait un quiescence search
    int quiescence(Evaluator *eval, int alpha = -2147483647, int beta = 2147483647, int depth = 4, bool checkmates_check = true, bool main_call = true, bool deep_mates_check = true, bool explore_checks = true);

    // Fonction qui renvoie le i-ème coup
    [[nodiscard]] int* get_i_move(int) const;

    // Fonction qui fait cliquer le i-ème coup
    [[nodiscard]] bool click_i_move(int i, bool orientation) const;

    // Fonction qui récupère et renvoie la couleur du joueur au trait (1 pour les blancs, -1 pour les noirs)
    [[nodiscard]] int get_color() const;

    // Fonction qui génère et renvoie la clé de Zobrist de la position
    [[nodiscard]] uint_fast64_t get_zobrist_key() const;

    // Fonction qui calcule et renvoie l'avantage d'espace
    [[nodiscard]] int get_space() const;

    // Fonction qui calcule et renvoie une évaluation des vis-à-vis
	[[nodiscard]] int get_alignments() const;

    // Fonction qui met à jour la position des rois
    bool update_kings_pos();

    // Fonction qui renvoie la puissance d'attaque d'une pièce sur le roi adverse
    [[nodiscard]] int get_piece_attack_power(int i, int j) const;

    // Fonction qui renvoie la puissance de défense d'une pièce pour le roi allié
	[[nodiscard]] int get_piece_defense_power(int i, int j) const;

    // Fonction qui renvoie l'activité des pièces
	[[nodiscard]] int get_piece_activity() const;

};


// Fonction qui obtient la case correspondante à la position sur la GUI
pair<int, int> get_pos_from_GUI(float, float);

// Fonction qui permet de changer l'orientation du plateau
void switch_orientation();

// Fonction aidant à l'affichage du plateau (renvoie i si board_orientation, et 7 - i sinon)
int orientation_index(int);


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
extern Buffer monte_buffer;


// Fonction qui joue un match entre deux IA utilisant GrogrosZero, et une évaluation par réseau de neurones ou des évaluateurs, avec un certain nombre de noeuds de calcul
int match(Evaluator *e_white = nullptr, Evaluator *e_black = nullptr, Network *n_white = nullptr, Network *n_black = nullptr, int nodes = 1000, bool display = false, int max_moves = 100);

// Fonction qui organise un tournoi entre les IA utilisant évaluateurs et réseaux de neurones des listes et renvoie la liste des scores (dépendant des nombres par victoires/nulles, et leur valeur)
int* tournament(Evaluator **, Network **, int, int nodes = 1000, int victory = 3, int draw = 1, bool display_full = false, int max_moves = 100);

// Fonction qui renvoie si deux positions (en format FEN) sont les mêmes
bool equal_fen(const string&, const string&);

// Fonction qui renvoie si deux positions (en format FEN) sont les mêmes (pour les répétitions)
bool equal_positions(const Board&, const Board&);

// Test de liste des positions (taille 100, pour la règle des 50 coups.. si on joue une prise ou un coup de pion, on peut reset la liste -> 52 : +1 pour la position de départ, +1 quand on joue exactement le 50ème coup)
extern string all_positions[102];
extern int total_positions;


// Fonction qui renvoie le temps que l'IA doit passer sur le prochain coup (en ms), en fonction d'un facteur k, et des temps restant
int time_to_play_move(int t1, int t2, float k = 0.05f);



// std::map<string, int> _positions_history = {
//     { "A", 1 },
//     { "B", 1 },
//     { "C", 2 }
// };

// Fonction qui renvoie la valeur UCT
float uct(float, float, int, int);


// Text box
struct TextBox {
    float x;
    float y;
    float width;
    float height;
    string text;
    int value;
    bool active;
    float text_size;
    Color main_color;
    Color text_color;
    Font text_font;

    // Constructeur par défaut
    TextBox() {}

    TextBox(const float pos_x, const float pos_y, const float box_width, const float box_height, string initial_text, const int initial_value) :
        x(pos_x),
        y(pos_y),
        width(box_width),
        height(box_height),
        text(std::move(initial_text)),
        value(initial_value),
        active(false) {}

    void set_rect(const float pos_x, const float pos_y, const float box_width, const float box_height) {
        x = pos_x;
        y = pos_y;
        width = box_width;
        height = box_height;
    }
};


// Fonction qui met à jour une text box
void update_text_box(TextBox& text_box);

// Fonction qui dessine une text box
void draw_text_box(const TextBox& text_box);



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
        uint_fast8_t* _binding_move = new uint_fast8_t[4];

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

        // Joueur au trait lors du dernier check (pour les temps)
        bool _last_player = true;

        // Affichage des flèches : affiche les chances de gain (true), l'évaluation (false)
        bool _display_win_chances = true;

        // Texte pour les timers
        TextBox _white_time_text_box;
        TextBox _black_time_text_box;

        // Paramètres pour la recherche de Monte-Carlo
        float _beta = 0.03f;
        float _k_add = 25.0f;
        int _quiescence_depth = 4;
        bool _deep_mates_search = true;
        bool _explore_checks = true;


        // Constructeurs

        // Par défaut
        GUI();


        // Fonctions

        // Fonction qui met en place le binding avec chess.com pour une nouvelle partie (et se prépare à jouer avec GrogrosZero)
        bool new_bind_game();

        // Fonction qui met en place le binding avec chess.com pour une nouvelle analyse de partie
        bool new_bind_analysis();
};

// Instantiation de la GUI globale
extern GUI main_GUI;


// Fonction qui compare deux coups pour savoir lequel afficher en premier
bool compare_move_arrows(int m1, int m2);


// Classe qui gère les clés de Zobrist
class Zobrist
{
	public:
		// Variables

		// Clés du plateau
		uint_fast64_t _board_keys[64][12];

		// Clé du trait
		uint_fast64_t _player_key;

		// Clés des roques
		uint_fast64_t _castling_keys[16];

		// Clés du en-passant
		uint_fast64_t _en_passant_keys[8];

		// Fonctions

		// Fonction qui génère les clés du plateau
		uint_fast64_t generate_board_keys();

};