#include "raylib.h"


// Définition des variables


// Paramètres d'initialisation
static int screen_width = 1200;
static int screen_height = 800;

// Nombre de FPS
static int fps = 60;

// Couleur de fond
static Color background_color = {25, 25, 25};

// Couleur du texte
static Color text_color = {255, 100, 100, 255};


// Variable qui indique si l'initialisation a été faite
static bool loaded_textures = false;


// Textures
static Texture2D board_texture;
static Image board_image;
static Texture2D piece_textures[12];
static Image piece_images[12];


// Taille du plateau par rapport à la fenêtre
static float board_scale = 0.9;
static float board_size;
static float board_padding_x;
static float board_padding_y;


// Taille des pièces
static float piece_size;


// Position de la souris
static Vector2 mouse_pos = GetMousePosition();

// Position de la case cliquée
static pair<int, int> clicked_pos = {-1, -1};

// La souris est-elle cliquée
static bool clicked = false;