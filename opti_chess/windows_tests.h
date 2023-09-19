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

// Fonction qui simule un clic de souris à une position donnée
void simulate_mouse_click(int x, int y);

// Fonction qui palce la souris à une position donnée
void set_mouse_pos(int x, int y);

// Fonction qui renvoie la mémoire disponible dans l'ordinateur
unsigned long long  get_total_system_memory();

// Fonction qui affiche la couleur de chacune des cases de l'échiquier sur l'écran, en donnant ses coordonnées (top-left, bottom-right)
uint_fast8_t* get_board_move(int x1, int y1, int x2, int y2, bool orientation = false, bool display = false);

// Fonction qui clique un coup en fonction de l'orientation du plateau
void click_move(int j1, int i1, int j2, int i2, int x1, int y1, int x2, int y2, bool orientation = false);

// Fonction qui récupère l'orientation du plateau. Renvoie 1 si les blancs sont en bas, 0 si c'est les noirs, -1 sinon
int bind_board_orientation(int x1, int y1, int x2, int y2);

// Fonction qui cherche la position du plateau de chess.com sur l'écran
void locate_chessboard(int&, int&, int&, int&);
