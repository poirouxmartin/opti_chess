#pragma once
#include "time.h"
#include <string>
#include "raylib.h"
#include <iostream>
#include <vector>

using namespace std;

// Définition des variables


// Définition des couleurs
#define VDARKGRAY CLITERAL(Color){50, 50, 50, 255}


// Nombre de FPS
static constexpr  int fps = 144;
static constexpr int max_drawing_fps = 144; // Nombre d'updates max par secondes pour ces fonctions
static constexpr clock_t last_drawing_time = 0;

// Couleur de fond
static constexpr Color background_color = {25, 25, 25, 255};

// Couleur du rectangle de texte
static constexpr Color background_text_color = {0, 0, 0, 255};

// Couleurs du texte
static constexpr Color text_color = {255, 75, 75, 255};
static constexpr Color text_color_dark = {200, 50, 50, 255};
static constexpr Color text_color_light = {200, 200, 200, 255};
static constexpr Color text_color_blue = {150, 150, 200, 255};
static constexpr Color text_color_info = {140, 140, 140, 255};

// Couleurs du plateau
static constexpr Color board_color_light = {190, 162, 127, 255};
static constexpr Color board_color_dark = {109, 78, 54, 255};

// Couleur de surlignage de cases
static constexpr Color highlight_color = {255, 255, 100, 150};

// Couleur de selection de cases
static constexpr Color select_color = {50, 225, 50, 100};

// Couleur des cases du dernier coup joué
static constexpr Color last_move_color = {220, 150, 50, 125};

// Couleur de la case de pre-move
static constexpr Color pre_move_color = {220, 30, 30, 125};

// Couleur des flèches
static constexpr Color arrow_color = {255, 225, 0, 150};

// Couleur des sliders
static constexpr Color slider_color = {200, 200, 200, 100};
static constexpr Color slider_background_color = {100, 100, 100, 75};

// Couleurs de la barre d'évaluation
static constexpr Color eval_bar_color_light = {224, 206, 186, 255};
static constexpr Color eval_bar_color_dark = {57, 50, 47, 255};

// Epaisseur des flèches (par rapport à la taille d'une case)
static float arrow_scale = 0.125f;
static float arrow_thickness = 50.0f;

// Est-ce qu'on affiche les flèches (non par exemple si l'utilisateur veut jouer contre l'IA)
static bool drawing_arrows = true;

// Pourcentage de noeuds à partir duquel on montre le coup
static float arrow_rate = 0.05f;

// Variable qui indique si l'initialisation a été faite
static bool loaded_resources = false;

// Textures et images
static Image piece_images[12];
static Texture2D piece_textures[12];

// Icône
static Image icon;

// Tête de Grogros
static Image grogros_image;
static Texture2D grogros_texture;
static float grogros_size;

// Curseur
static Image cursor_image;
static Texture2D cursor_texture;
static float cursor_size = 32.0f;

// Mini-pièces
static Image mini_piece_images[12];
static Texture2D mini_piece_textures[12];
static int mini_piece_size;

// Sons
static Sound move_1_sound;
static Sound move_2_sound;
static Sound castle_1_sound;
static Sound castle_2_sound;
static Sound check_1_sound;
static Sound check_2_sound;
static Sound capture_1_sound;
static Sound capture_2_sound;
static Sound checkmate_sound;
static Sound stealmate_sound;
static Sound game_begin_sound;
static Sound game_end_sound;
static Sound promotion_sound;

// Taille du plateau par rapport à la fenêtre
static float board_scale = 0.7f;
static float board_size;
static float board_padding_x;
static float board_padding_y;

// Taille des pièces
static float tile_size;
static float piece_size;
static float piece_scale = 0.8f;

// Taille standard du texte
static float text_size;

// Police du texte
static Font text_font;

// Espacement entre les caractères
static float font_spacing = 0.1f;


// Orientation du plateau
static bool board_orientation = true;


// Position de la souris
static Vector2 mouse_pos;

// Position de la case cliquée
static pair<int, int> clicked_pos = {-1, -1};

// Position de la case cliquée (droit)
static pair<int, int> right_clicked_pos = {-1, -1};

// La souris est-elle cliquée
static bool clicked = false;

// Pièce sélectionnée de la case cliquée
static pair<int, int> selected_pos = {-1, -1};

// Cases surlignées
static int highlighted_array[8][8] {{0, 0, 0, 0, 0, 0, 0, 0}, 
                                    {0, 0, 0, 0, 0, 0, 0, 0}, 
                                    {0, 0, 0, 0, 0, 0, 0, 0}, 
                                    {0, 0, 0, 0, 0, 0, 0, 0}, 
                                    {0, 0, 0, 0, 0, 0, 0, 0}, 
                                    {0, 0, 0, 0, 0, 0, 0, 0}, 
                                    {0, 0, 0, 0, 0, 0, 0, 0}, 
                                    {0, 0, 0, 0, 0, 0, 0, 0}};

// Calcul du nombre de noeuds visités
static int visited_nodes;

// Vecteur des flèches de GrogrosZero : coordonnées du coup + indice du coup
static vector<vector<int>> grogros_arrows;

// Calcul de temps
static clock_t begin_time;

// Valeurs des sliders
static float pgn_slider = 0.0f;
static float monte_carlo_slider = 0.0f;
static float variants_slider = 0.0f;

// Pre-move
static int pre_move[4] = {-1, -1, -1, -1};

// Eval à montrer pour la barre d'éval
static float global_eval = 0.0f;
static string global_eval_text = "+0.0";

// Temps de base pour les joueurs (en ms)
static int base_time_white = 180000;
static int base_time_black = 180000;

// Incrément (en ms)
static int base_time_increment_white = -50;
static int base_time_increment_black = -50;

// Valeur des pièces pour l'affichage sur la GUI (rien/roi, pion, cavalier, fou, tour, dame)
static const int piece_GUI_values[6] = {0, 1, 3, 3, 5, 9};

// Matériel manquant
static const int base_material[6] = {0, 8, 2, 2, 2, 1};
static int missing_w_material[6] = {0, 0, 0, 0, 0, 0};
static int missing_b_material[6] = {0, 0, 0, 0, 0, 0};

// Alphabet de taille 8
static const string abc8 = "abcdefgh";

// Flèches sur l'échiquier
static vector<vector<int>> arrows_array;

// Composantes de l'évaluation à afficher sur la GUI
static string eval_components;

// A partir de coordonnées sur le plateau (// Thickness = -1 -> default thickness)
void draw_arrow_from_coord(int, int, int, int, int, int, float thickness = -1, Color c = arrow_color, bool use_value = false, int value = 0, int mate = -1, bool outline = false);

// Couleur de la flèche en fonction du coup (de son nombre de noeuds)
Color move_color(int, int);

// Fonction qui charge les textures
void load_resources();

// Fonction qui met à la bonne taille les images
void resize_GUI();

// Fonction qui actualise les nouvelles dimensions de la fenêtre
void get_window_size();

// Fonction qui renvoie si le joueur est en train de jouer (pour que l'IA arrête de réflechir à ce moment sinon ça lagge)
bool is_playing();

// Fonction qui change le mode d'affichage des flèches (oui/non)
void switch_arrow_drawing();

// Fonction qui affiche un texte dans une zone donnée avec un slider
void slider_text(const string&, float, float, float, float, float size = text_size, float *slider_value = nullptr, Color t_color = text_color, float slider_width = board_size * 0.025f, float slider_height = board_size * 0.1f);

// Fonction pour obtenir l'orientation du plateau
bool get_board_orientation();

// Fonction qui renvoie si le curseur de la souris se trouve dans le rectangle
bool is_cursor_in_rect(Rectangle);

// Fonction qui dessine un rectangle à partir de coordonnées flottantes
bool draw_rectangle(float, float, float, float, Color);

// Fonction qui dessine un rectangle à partir de coordonnées flottantes, en fonction des coordonnées de début et de fin
bool draw_rectangle_from_pos(float, float, float, float, Color);

// Fonction qui dessine un cercle à partir de coordonnées flottantes
void draw_circle(float, float, float, Color);

// Fonction qui dessine une ligne à partir de coordonnées flottantes
void draw_line_ex(float, float, float, float, float, Color);

// Fonction qui dessine une ligne de Bézier à partir de coordonnées flottantes
void draw_line_bezier(float, float, float, float, float, Color);

// Fonction qui dessine une texture à partir de coordonnées flottantes
void draw_texture(const Texture&, float, float, Color);

// Fonction qui affiche la barre d'evaluation
void draw_eval_bar(float, const string&, float, float, float, float, float max_eval = 500, Color = eval_bar_color_light, Color = eval_bar_color_dark, float max_height = 0.95f);

// Fonction qui retire les surlignages de toutes les cases
void remove_highlighted_tiles();

// Fonction qui selectionne une case
void select_tile(int, int);

// Fonction qui surligne une case (ou la de-surligne)
void highlight_tile(int, int);

// Fonction qui déselectionne
void unselect();

// Fonction qui joue le son de fin de partie
void play_end_sound();

// A partir de coordonnées sur le plateau
void draw_simple_arrow_from_coord(int, int, int, int, float, Color);