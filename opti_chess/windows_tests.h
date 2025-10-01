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

// Site de jeux d'�checs
class ChessSite {
public:

	// Nom du site
	string _name;

	// Couleur des cases blanches
	SimpleColor _white_tile_color;

	// Couleur des cases noires
	SimpleColor _black_tile_color;

	// Couleur des pi�ces blanches
	SimpleColor _white_piece_color;

	// Couleur des pi�ces noires
	SimpleColor _black_piece_color;

	// Couleur d'une case blanche jou�e
	SimpleColor _white_tile_played_color;

	// Couleur d'une case noire jou�e
	SimpleColor _black_tile_played_color;

	// A quel endroit regarder sur la case pour �tre s�r de trouver la couleur de la pi�ce (en % de la taille de la case, et en partant du coin bas-gauche)
	pair<float, float> _piece_location_on_tile;

	// A quel endroit regarder sur la case pour �tre s�r de trouver la couleur de la case (en % de la taille de la case, et en partant du coin bas-gauche)
	pair<float, float> _tile_location_on_tile;

	// Temps perdu par coup
	int _time_lost_per_move;

	// Tol�rance de couleur pour les cases
	float _tile_color_tolerance;

	// Tol�rance de couleur pour les pi�ces
	float _piece_color_tolerance;


	// Constructeur

	// Constructeur par d�faut
	ChessSite();
};


// Fonction qui simule un clic de souris � une position donn�e
void simulate_mouse_click(int x, int y);

// Fonction qui rel�che le clic de la souris
void simulate_mouse_release();

// Fonction qui palce la souris � une position donn�e
void set_mouse_pos(int x, int y);

// Fonction qui renvoie la m�moire disponible dans l'ordinateur
unsigned long long  get_total_system_memory();

// Fonction qui affiche la couleur de chacune des cases de l'�chiquier sur l'�cran, en donnant ses coordonn�es (top-left, bottom-right)
uint8_t* get_board_move(int x1, int y1, int x2, int y2, ChessSite website, bool orientation = false, bool display = false);

// Fonction qui clique un coup en fonction de l'orientation du plateau
void click_move(int j1, int i1, int j2, int i2, int x1, int y1, int x2, int y2, bool orientation = false);

// Fonction qui r�cup�re l'orientation du plateau. Renvoie 1 si les blancs sont en bas, 0 si c'est les noirs, -1 sinon
int bind_board_orientation(int x1, int y1, int x2, int y2, ChessSite website);

// Fonction qui cherche la position du plateau de chess.com sur l'�cran
bool locate_chessboard(int&, int&, int&, int&, ChessSite website);
