#pragma once

#include "windows.h"
#include <iostream>
#include "windows_tests.h"

using namespace std;


// Couleurs

// Cases lichess :
// blanches : 240, 217, 181
// noires : 181, 136, 99


// Constructeur de couleur
SimpleColor::SimpleColor() = default;

// Constructeur de couleur
SimpleColor::SimpleColor(const int r, const int g, const int b) {
	_r = r;
	_g = g;
	_b = b;
}

// Fonction qui affiche les composantes de la couleur
void SimpleColor::print() const
{
	cout << "(" << _r << ", " << _g << ", " << _b << ")" << endl;
}

// Fonction qui renvoie si deux couleurs sont des composantes égales
bool SimpleColor::equals(const SimpleColor c) const
{
	return _r == c._r && _g == c._g && _b == c._b;
}

// Fonction qui renvoie si deux couleurs sont assez proches
bool SimpleColor::equals(const SimpleColor c, const float alike) const
{
	return (abs(static_cast<int>(_r) - static_cast<int>(c._r)) + abs(static_cast<int>(_g) - static_cast<int>(c._g)) + abs(static_cast<int>(_b) - static_cast<int>(c._b))) / 255.0f / 3.0f <= 1 - alike;
}

// Fonction qui simule un clic de souris à une position donnée
void simulate_mouse_click(const int x, const int y)
{
	SetCursorPos(x, y);
	INPUT input = { 0 };
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP;
	SendInput(1, &input, sizeof(INPUT));
	return;
}

// Fonction qui relâche le clic de la souris
void simulate_mouse_release()
{
	INPUT input = { 0 };
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
	SendInput(1, &input, sizeof(INPUT));
	return;
}

// Fonction qui palce la souris à une position donnée
void set_mouse_pos(const int x, const int y) {
	SetCursorPos(x, y);
	return;
}

// Fonction qui renvoie la mémoire disponible dans l'ordinateur
// FIXME : ça renvoie pas le bon truc
unsigned long long get_total_system_memory()
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return status.ullTotalPhys;
}

// Function that gets the screen as bitmap
HBITMAP get_screen_bmp(const HDC hdc, const int x1, const int y1, const int x2, const int y2) {
	// Calculate the width and height of the capture area
	const int capture_width = x2 - x1 + 1;
	const int capture_height = y2 - y1 + 1;

	// Create compatible DC, create a compatible bitmap and copy the screen area using BitBlt()
	const HDC h_capture_dc = CreateCompatibleDC(hdc);
	const HBITMAP h_bitmap = CreateCompatibleBitmap(hdc, capture_width, capture_height);
	const HGDIOBJ h_old = SelectObject(h_capture_dc, h_bitmap);
	BOOL b_ok = BitBlt(h_capture_dc, 0, 0, capture_width, capture_height, hdc, x1, y1, SRCCOPY | CAPTUREBLT);

	SelectObject(h_capture_dc, h_old); // always select the previously selected object once done
	DeleteDC(h_capture_dc);
	return h_bitmap;
}

// Fonction qui affiche la couleur de chacune des cases de l'échiquier sur l'écran, en donnant ses coordonnées (top-left, bottom-right)
uint_fast8_t* get_board_move(const int x1, const int y1, const int x2, const int y2, ChessSite website, const bool orientation, const bool display) {

	int x_begin = -1;
	int y_begin = -1;
	int x_end = -1;
	int y_end = -1;

	// Taille d'une case
	const float tile_size = (x2 - x1) / 8.0f;

	// Fait un screenshot en bmp
	const HDC hdc = GetDC(nullptr);
	const HBITMAP h_bitmap = get_screen_bmp(hdc, x1, y1, x2, y2);
	BITMAPINFO my_bm_info = { 0 };
	my_bm_info.bmiHeader.biSize = sizeof(my_bm_info.bmiHeader);

	if (0 == GetDIBits(hdc, h_bitmap, 0, 0, nullptr, &my_bm_info, DIB_RGB_COLORS))
		cout << "error" << endl;

	const auto lp_pixels = new BYTE[my_bm_info.bmiHeader.biSizeImage];
	my_bm_info.bmiHeader.biCompression = BI_RGB;

	if (0 == GetDIBits(hdc, h_bitmap, 0, my_bm_info.bmiHeader.biHeight, (LPVOID)lp_pixels, &my_bm_info, DIB_RGB_COLORS))
		cout << "error2" << endl;

	bool found_start_square = false;
	bool found_end_square = false;

	// Regarde chaque case de l'échiquier
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			// Couleur de la case
			int x = (i + website._tile_location_on_tile.first) * tile_size;
			int y = (j + website._tile_location_on_tile.second) * tile_size;
			int pixel_offset = x * my_bm_info.bmiHeader.biWidth + y;
			const BYTE* pixel_address = lp_pixels + pixel_offset * (my_bm_info.bmiHeader.biBitCount / 8);
			auto color = SimpleColor(static_cast<int>(pixel_address[2]), static_cast<int>(pixel_address[1]),
				static_cast<int>(pixel_address[0]));
			if (display) {
				cout << x << ", " << y << "(" << i << ", " << j << ") -> ";
				color.print();
			}

			// Si la couleur correspond à une couleur de case jouée
			if (color.equals(website._white_tile_played_color, 0.98f) || color.equals(website._black_tile_played_color, 0.98f)) {
				if (display) {
					cout << "moved tile : ";
				}

				// Regarde s'il y'a rien sur la case
				x = (i + 0.3f) * tile_size;
				y = (j + 0.5f) * tile_size;
				pixel_offset = x * my_bm_info.bmiHeader.biWidth + y;
				pixel_address = lp_pixels + pixel_offset * (my_bm_info.bmiHeader.biBitCount / 8);
				color = SimpleColor(static_cast<int>(pixel_address[2]), static_cast<int>(pixel_address[1]), static_cast<int>(pixel_address[0]));
				if (display) {
					cout << x << ", " << y << "(" << i << ", " << j << ") -> ";
					color.print();
				}

				// Si c'est le cas, alors c'est la case de départ
				if (!found_start_square && (color.equals(website._white_tile_played_color, 0.98f) || color.equals(website._black_tile_played_color, 0.98f))) {
					y_begin = orientation ? i : 7 - i;
					x_begin = orientation ? j : 7 - j;
					if (display)
						cout << "begin tile" << endl;

					if (found_end_square)
						goto found;

					found_start_square = true;
				}

				// Sinon, c'est la case de fin
				else {
					y_end = orientation ? i : 7 - i;
					x_end = orientation ? j : 7 - j;
					if (display)
						cout << "end tile" << endl;

					if (found_start_square)
						goto found;

					found_end_square = true;
				}
			}
		}
	}

	found:

	// Release le screen
	DeleteObject(h_bitmap);
	ReleaseDC(nullptr, hdc);
	delete[] lp_pixels;

	// Coordonnées du coup joué
	const auto coord = new uint_fast8_t[4];
	coord[0] = y_begin;
	coord[1] = x_begin;
	coord[2] = y_end;
	coord[3] = x_end;

	return coord;
}

// Fonction qui clique un coup en fonction de l'orientation du plateau
void click_move(const int j1, const int i1, const int j2, const int i2, const int x1, const int y1, const int x2, int y2, const bool orientation) {
	const float tile_size = static_cast<float>((x2 - x1)) / 8.0f;
	constexpr float tile_click = 0.5f; // Pour que ça clique au milieu de la case

	const int cx1 = x1 + (tile_click + (orientation ? i1 : 7 - i1)) * tile_size;
	const int cy1 = y1 + (tile_click + (orientation ? 7 - j1 : j1)) * tile_size;
	const int cx2 = x1 + (tile_click + (orientation ? i2 : 7 - i2)) * tile_size;
	const int cy2 = y1 + (tile_click + (orientation ? 7 - j2 : j2)) * tile_size;

	simulate_mouse_click(cx1, cy1);
	simulate_mouse_click(cx2, cy2);

	return;
}

// Fonction qui récupère l'orientation du plateau. Renvoie 1 si les blancs sont en bas, 0 si c'est les noirs, -1 sinon
int bind_board_orientation(const int x1, const int y1, const int x2, const int y2, ChessSite website) {

	cout << "looking for " << website._name << " board orientation..." << endl;

	// Taille d'une case
	const float tile_size = static_cast<float>((x2 - x1)) / 8.0f;

	// Couleur de la case

	// Fait un screenshot en bmp
	const HDC hdc = GetDC(nullptr);
	const HBITMAP h_bitmap = get_screen_bmp(hdc, x1, y1, x2, y2);
	BITMAPINFO my_bm_info = { 0 };
	my_bm_info.bmiHeader.biSize = sizeof(my_bm_info.bmiHeader);

	if (0 == GetDIBits(hdc, h_bitmap, 0, 0, nullptr, &my_bm_info, DIB_RGB_COLORS))
		cout << "error" << endl;

	const auto lp_pixels = new BYTE[my_bm_info.bmiHeader.biSizeImage];
	my_bm_info.bmiHeader.biCompression = BI_RGB;

	if (0 == GetDIBits(hdc, h_bitmap, 0, my_bm_info.bmiHeader.biHeight, (LPVOID)lp_pixels, &my_bm_info, DIB_RGB_COLORS))
		cout << "error2" << endl;

	// Position où regarder la couleur de la case
	const int x = static_cast<int>(website._piece_location_on_tile.first * tile_size);
	const int y = static_cast<int>(website._piece_location_on_tile.second * tile_size);

	const int pixel_offset = x * my_bm_info.bmiHeader.biWidth + y;
	const BYTE* pixel_address = lp_pixels + pixel_offset * (my_bm_info.bmiHeader.biBitCount / 8);
	const auto color = SimpleColor(static_cast<int>(pixel_address[2]), static_cast<int>(pixel_address[1]), static_cast<int>(pixel_address[0]));

	cout << "color of the piece: " << color._r << ", " << color._g << ", " << color._b << ", expected: " << website._white_piece_color._r << ", " << website._white_piece_color._g << ", " << website._white_piece_color._b << ", or " << website._black_piece_color._r << ", " << website._black_piece_color._g << ", " << website._black_piece_color._b << endl;

	const int orientation = color.equals(website._white_piece_color, 0.95f) ? 1 : (color.equals(website._black_piece_color, 0.95f) ? 0 : -1);

	cout << "board orientation: " << (orientation == 1 ? "white" : orientation == 0 ? "black" : "unknown") << endl << endl;

	return orientation;
}

// Fonction qui cherche la position du plateau de chess.com sur l'écran
bool locate_chessboard(int& top_left_x, int& top_left_y, int& bottom_right_x, int& bottom_right_y, ChessSite website) {

	// Cherche le plateau sur le site
	cout << "looking for " << website._name << " chessboard..." << endl;

	const int screen_width = GetSystemMetrics(SM_CXSCREEN);
	const int screen_height = GetSystemMetrics(SM_CYSCREEN);

	// Fait un screenshot en bmp
	const HDC hdc = GetDC(nullptr);
	const HBITMAP h_bitmap = get_screen_bmp(hdc, 0, 0, screen_width, screen_height);
	BITMAPINFO my_bm_info = { 0 };
	my_bm_info.bmiHeader.biSize = sizeof(my_bm_info.bmiHeader);

	if (0 == GetDIBits(hdc, h_bitmap, 0, 0, nullptr, &my_bm_info, DIB_RGB_COLORS))
		cout << "error" << endl;

	const auto lp_pixels = new BYTE[my_bm_info.bmiHeader.biSizeImage];
	my_bm_info.bmiHeader.biCompression = BI_RGB;

	if (0 == GetDIBits(hdc, h_bitmap, 0, my_bm_info.bmiHeader.biHeight, (LPVOID)lp_pixels, &my_bm_info, DIB_RGB_COLORS))
		cout << "error2" << endl;

	BYTE* pixel_address;

	// Distance au bas
	int y;
	int pixel_offset;
	SimpleColor color;

	// Cherche au milieu de l'écran, mais on peut faire un cadrillage si besoin...

	// Cherche la gauche du plateau
	bool found_left = false;
	int x = screen_height / 2;
	for (y = 0; y < screen_width; y++) {
		pixel_offset = x * my_bm_info.bmiHeader.biWidth + y;
		pixel_address = lp_pixels + pixel_offset * (my_bm_info.bmiHeader.biBitCount / 8);
		color = SimpleColor(static_cast<int>(pixel_address[2]), static_cast<int>(pixel_address[1]), static_cast<int>(pixel_address[0]));

		if (color.equals(website._black_tile_color, 0.98f) || color.equals(website._white_tile_color, 0.98f)) {
			top_left_x = y;
			found_left = true;
			break;
		}
	}

	// Cherche le bas du plateau
	bool found_bottom = false;
	y = top_left_x + 10;
	for (x = 0; x < screen_height; x++) {
		pixel_offset = x * my_bm_info.bmiHeader.biWidth + y;
		pixel_address = lp_pixels + pixel_offset * (my_bm_info.bmiHeader.biBitCount / 8);
		color = SimpleColor(static_cast<int>(pixel_address[2]), static_cast<int>(pixel_address[1]), static_cast<int>(pixel_address[0]));

		if (color.equals(website._black_tile_color, 0.98f) || color.equals(website._white_tile_color, 0.98f)) {
			bottom_right_y = screen_height - x;
			found_bottom = true;
			break;
		}
	}

	// Cherche le haut du plateau
	bool found_top = false;
	y = top_left_x + 10;
	for (x = screen_height; x > 0; x--) {
		pixel_offset = x * my_bm_info.bmiHeader.biWidth + y;
		pixel_address = lp_pixels + pixel_offset * (my_bm_info.bmiHeader.biBitCount / 8);
		color = SimpleColor(static_cast<int>(pixel_address[2]), static_cast<int>(pixel_address[1]), static_cast<int>(pixel_address[0]));

		if (color.equals(website._black_tile_color, 0.98f) || color.equals(website._white_tile_color, 0.98f)) {
			top_left_y = screen_height - x;
			found_top = true;
			break;
		}
	}

	// Droite du plateau (normalement il est carré)
	bottom_right_x = top_left_x + bottom_right_y - top_left_y;

	bool located = found_left && found_bottom && found_top;

	if (located) {

		cout << website._name << " chessboard has been located: ";
		printf("Top-Left: (%d, %d), ", top_left_x, top_left_y);
		printf("Bottom-Right: (%d, %d)\n", bottom_right_x, bottom_right_y);
	}
	else {
		cout << "chessboard not found" << endl;
	}

	cout << endl;

	return located;
}

// Constructeur par défaut
ChessSite::ChessSite() = default;

// TODO : clean
// -> Clean les fonctions
// -> Pouvoir changer les couleurs du plateau
// -> Optimiser encore les fonctions pour get les pixels
// -> Promotions à gérer (pour vs les bots, car c'est pas automatique)
// -> Gérer le temps de Grogros (récupérer celui de chess.com?)
// -> Faire une fonction qui récupère le plateau automatiquement? (la position)