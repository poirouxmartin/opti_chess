#include "raylib.h"
#include "time.h"


// Définition des variables


// Paramètres d'initialisation
static int screen_width = 1800;
static int screen_height = 900;

// Nombre de FPS
static int fps = 144;

// Couleur de fond
static Color background_color = {30, 30, 30, 255};

// Couleur du rectangle de texte
static Color background_text_color = {20, 20, 20, 255};

// Couleur du texte
static Color text_color = {255, 75, 75, 255};

// Couleurs du plateau
static Color board_color_light = {190, 162, 127, 255};
static Color board_color_dark = {109, 78, 54, 255};

// Couleur de surlignage de cases
static Color highlight_color = {255, 255, 100, 150};

// Couleur de selection de cases
static Color select_color = {50, 225, 50, 100};

// Couleur des cases du dernier coup joué
static Color last_move_color = {250, 50, 50, 125};

// Couleur des flèches
static Color arrow_color = {225, 225, 50, 255};

// Epaisseur des flèches (par rapport à la taille d'une case)
static float arrow_scale = 0.125;
static float arrow_thickness = 50;

// Pourcentage de noeuds à partir duquel on montre le coup
static float arrow_rate = 0.05;

// Variable qui indique si l'initialisation a été faite
static bool loaded_resources = false;

// Textures
static Texture2D piece_textures[12];
static Image piece_images[12];

// Icône
static Image icon;

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

// Taille du plateau par rapport à la fenêtre
static float board_scale = 0.8;
static float board_size;
static float board_padding_x;
static float board_padding_y;

// Taille des pièces
static int tile_size;
static float piece_size;
static float piece_scale = 0.75;

// Taille standard du texte
static float text_size;


// Orientation du plateau
static bool board_orientation = true;


// Position de la souris
static Vector2 mouse_pos = GetMousePosition();

// Position de la case cliquée
static pair<int, int> clicked_pos = {-1, -1};

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

// Calcul de temps
static clock_t begin_time;

// Fonction pour dessiner une flèche
void draw_arrow(float, float, float, float, float thickness = arrow_thickness, Color c = arrow_color);

// A partir de coordonnées sur le plateau (// Thickness = -1 -> default thickness)
void draw_arrow_from_coord(int, int, int, int, float thickness = -1, Color c = arrow_color, bool use_value = false, int value = 0, int mate = -1);

// Couleur de la flèche en fonction du coup (de son nombre de noeuds)
Color move_color(int, int);

// Fonction qui charge les textures
void load_resources();

// Fonction qui met à la bonne taille les images
void resize_gui();

// Fonction qui actualise les nouvelles dimensions de la fenêtre
void get_window_size();