#pragma once

#include <string>
#include <utility>

using namespace std;

// Color
class SimpleColor {
public:
	int _r = 0; // Red
	int _g = 0; // Green
	int _b = 0; // Blue

	SimpleColor();
	SimpleColor(int r, int g, int b);
	void print() const;
	[[nodiscard]] bool equals(SimpleColor c) const;
	[[nodiscard]] bool equals(SimpleColor c, float alike) const;
	[[nodiscard]] float color_distance(SimpleColor c) const;
};

// Rectangle
typedef struct SimpleRectangle {
	int x1;
	int y1;
	int x2;
	int y2;
};

// Site de jeux d'échecs
class ChessSite {
public:

	// Nom du site
	string _name;

	// Couleur des cases blanches
	SimpleColor _white_tile_color;

	// Couleur des cases noires
	SimpleColor _black_tile_color;

	// Couleur des pièces blanches
	SimpleColor _white_piece_color;

	// Couleur des pièces noires
	SimpleColor _black_piece_color;

	// Couleur d'une case blanche jouée
	SimpleColor _white_tile_played_color;

	// Couleur d'une case noire jouée
	SimpleColor _black_tile_played_color;

	// A quel endroit regarder sur la case pour être sûr de trouver la couleur de la pièce (en % de la taille de la case, et en partant du coin bas-gauche)
	pair<float, float> _piece_location_on_tile;

	// A quel endroit regarder sur la case pour être sûr de trouver la couleur de la case (en % de la taille de la case, et en partant du coin bas-gauche)
	pair<float, float> _tile_location_on_tile;

	// Temps perdu par coup
	int _time_lost_per_move;

	// Tolérance de couleur pour les cases
	float _tile_color_tolerance;

	// Tolérance de couleur pour les pièces
	float _piece_color_tolerance;


	// Constructeur

	// Constructeur par défaut
	ChessSite();
};


// Fonction qui simule un clic de souris à une position donnée
void simulate_mouse_click(int x, int y);

// Fonction qui relâche le clic de la souris
void simulate_mouse_release();

// Fonction qui palce la souris à une position donnée
void set_mouse_pos(int x, int y);

// Fonction qui renvoie la mémoire disponible dans l'ordinateur
unsigned long long  get_total_system_memory();

// Fonction qui affiche la couleur de chacune des cases de l'échiquier sur l'écran, en donnant ses coordonnées (top-left, bottom-right)
uint8_t* get_board_move(int x1, int y1, int x2, int y2, ChessSite website, bool orientation = false, bool display = false);

// Fonction qui clique un coup en fonction de l'orientation du plateau
void click_move(int j1, int i1, int j2, int i2, int x1, int y1, int x2, int y2, bool orientation = false);

// Fonction qui récupère l'orientation du plateau. Renvoie 1 si les blancs sont en bas, 0 si c'est les noirs, -1 sinon
int bind_board_orientation(int x1, int y1, int x2, int y2, ChessSite website);

// Fonction qui cherche la position du plateau de chess.com sur l'écran
bool locate_chessboard(int&, int&, int&, int&, ChessSite website);
