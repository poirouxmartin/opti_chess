#include "raylib.h"
#include "time.h"


// Définition des variables


// Paramètres d'initialisation
static int screen_width = 1800;
static int screen_height = 900;

// Nombre de FPS
static int fps = 144;

// Couleur de fond
static Color background_color = {25, 25, 25};

// Couleur du texte
static Color text_color = {255, 100, 100, 255};

// Couleurs du plateau
static Color board_color_light = {150, 150, 200, 255};
static Color board_color_dark = {100, 100, 150, 255};

// Couleur de surlignage de cases
static Color highlight_color = {255, 100, 100, 150};


// Variable qui indique si l'initialisation a été faite
static bool loaded_textures = false;


// Textures et sons
static Texture2D board_texture;
static Image board_image;
static Texture2D piece_textures[12];
static Image piece_images[12];
static Sound move_1_sound;


// Taille du plateau par rapport à la fenêtre
static float board_scale = 0.9;
static float board_size;
static float board_padding_x;
static float board_padding_y;


// Taille des pièces
static int tile_size;
static float piece_size;
static float piece_scale = 0.8;


// Orientation du plateau
static bool board_orientation = true;


// Position de la souris
static Vector2 mouse_pos = GetMousePosition();

// Position de la case cliquée
static pair<int, int> clicked_pos = {-1, -1};

// La souris est-elle cliquée
static bool clicked = false;

// Calcul du nombre de noeuds visités
static int visited_nodes;

// Calcul de temps
static clock_t begin_time;