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
	bool equals(SimpleColor c) const;
	bool equals(SimpleColor c, float alike) const;
	float color_distance(SimpleColor c) const;
};

// Rectangle
typedef struct SimpleRectangle {
	int x1;
	int y1;
	int x2;
	int y2;
};

// Chess website
class ChessSite {
public:

	// Name of the website
	string _name;

	// Colour of the light squares
	SimpleColor _white_tile_color;

	// Colour of the dark squares
	SimpleColor _black_tile_color;

	// Colour of the white pieces
	SimpleColor _white_piece_color;

	// Colour of the black pieces
	SimpleColor _black_piece_color;

	// Colour of a light square that was just played
	SimpleColor _white_tile_played_color;

	// Colour of a dark square that was just played
	SimpleColor _black_tile_played_color;

	// Where to look inside the square to be sure to find the colour of the piece (in % of the square size, starting from the bottom-left corner)
	pair<float, float> _piece_location_on_tile;

	// Where to look inside the square to be sure to find the colour of the square (in % of the square size, starting from the bottom-left corner)
	pair<float, float> _tile_location_on_tile;

	// Time lost per move
	int _time_lost_per_move;

	// Colour tolerance for the squares
	float _tile_color_tolerance;

	// Colour tolerance for the pieces
	float _piece_color_tolerance;


	// Constructor

	// Default constructor
	ChessSite();
};


// Simulates a mouse click at a given position
void simulate_mouse_click(int x, int y);

// Releases the mouse click
void simulate_mouse_release();

// Places the mouse at a given position
void set_mouse_pos(int x, int y);

// Returns the total physical memory of the computer
unsigned long long  get_total_system_memory();

// Returns the AVAILABLE (free) physical memory - the basis of the adaptive
// pool sizing (avoids saturating the RAM => #13).
unsigned long long  get_available_physical_memory();

// Displays the colour of every square of the chessboard on screen, given its coordinates (top-left, bottom-right)
uint8_t* get_board_move(int x1, int y1, int x2, int y2, ChessSite website, bool orientation = false, bool display = false);

// Clicks a move according to the orientation of the board
void click_move(int j1, int i1, int j2, int i2, int x1, int y1, int x2, int y2, bool orientation = false, const bool is_promotion = false);

// Retrieves the orientation of the board. Returns 1 if White is at the bottom, 0 if Black is, -1 otherwise
int bind_board_orientation(int x1, int y1, int x2, int y2, ChessSite website);

// Looks for the position of the chess.com board on screen
bool locate_chessboard(int&, int&, int&, int&, ChessSite website);
