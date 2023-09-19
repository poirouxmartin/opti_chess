#pragma once

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
};

// Rectangle
typedef struct SimpleRectangle {
	int x1;
	int y1;
	int x2;
	int y2;
};

// Fonction qui simule un clic de souris � une position donn�e
void simulate_mouse_click(int x, int y);

// Fonction qui palce la souris � une position donn�e
void set_mouse_pos(int x, int y);

// Fonction qui renvoie la m�moire disponible dans l'ordinateur
unsigned long long  get_total_system_memory();

// Fonction qui affiche la couleur de chacune des cases de l'�chiquier sur l'�cran, en donnant ses coordonn�es (top-left, bottom-right)
uint_fast8_t* get_board_move(int x1, int y1, int x2, int y2, bool orientation = false, bool display = false);

// Fonction qui clique un coup en fonction de l'orientation du plateau
void click_move(int j1, int i1, int j2, int i2, int x1, int y1, int x2, int y2, bool orientation = false);

// Fonction qui r�cup�re l'orientation du plateau. Renvoie 1 si les blancs sont en bas, 0 si c'est les noirs, -1 sinon
int bind_board_orientation(int x1, int y1, int x2, int y2);

// Fonction qui cherche la position du plateau de chess.com sur l'�cran
void locate_chessboard(int&, int&, int&, int&);
