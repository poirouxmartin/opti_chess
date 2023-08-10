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
uint_fast8_t* get_board_move(const int x1, const int y1, const int x2, const int y2, const bool orientation, const bool display) {
	// Pour un échiquier de base (blanc et vert sur chess.com)

	// Couleurs des coups joués
	// blanches : 247, 247, 105
	// noires : 187, 203, 43
	static const SimpleColor white_played(244, 246, 127);
	static const SimpleColor black_played(187, 204, 67);

	// Couleurs des cases normales
	// blanches : 238, 238, 210
	// noires : 118, 150, 86

	// Couleur de chaque case

	int x_begin = -1;
	int y_begin = -1;
	int x_end = -1;
	int y_end = -1;

	// Taille d'une case
	const float tile_size = (x2 - x1) / 8.0f;

	// Couleur de la case à 85%, 85% (haut-droite)
	constexpr float tile_corner_x = 0.85f;
	constexpr float tile_corner_y = 0.85f;

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

	BYTE blue;
	BYTE green;
	BYTE red;

	// Regarde chaque case de l'échiquier
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			// Couleur de la case
			int x = (i + tile_corner_x) * tile_size;
			int y = (j + tile_corner_y) * tile_size;
			int pixel_offset = x * my_bm_info.bmiHeader.biWidth + y;
			const BYTE* pixel_address = lp_pixels + pixel_offset * (my_bm_info.bmiHeader.biBitCount / 8);
			auto color = SimpleColor(static_cast<int>(pixel_address[2]), static_cast<int>(pixel_address[1]),
				static_cast<int>(pixel_address[0]));
			if (display) {
				cout << x << ", " << y << "(" << i << ", " << j << ") -> ";
				color.print();
			}

			// Si la couleur correspond à une couleur de case jouée
			if (color.equals(white_played, 0.98f) || color.equals(black_played, 0.98f)) {
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
				if (color.equals(white_played, 0.98f) || color.equals(black_played, 0.98f)) {
					y_begin = orientation ? i : 7 - i;
					x_begin = orientation ? j : 7 - j;
					if (display)
						cout << "begin tile" << endl;
				}

				// Sinon, c'est la case de fin
				else {
					y_end = orientation ? i : 7 - i;
					x_end = orientation ? j : 7 - j;
					if (display)
						cout << "end tile" << endl;
				}
			}
		}
	}

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
int bind_board_orientation(const int x1, const int y1, const int x2, const int y2) {
	// Couleur des pièces sur le plateau
	const SimpleColor white_color(248, 248, 248);
	const SimpleColor black_color(86, 83, 82);

	// Taille d'une case
	const float tile_size = static_cast<float>((x2 - x1)) / 8.0f;

	// Couleur de la case à :
	constexpr float tile_corner_x = 0.15f; // 15% en bas
	constexpr float tile_corner_y = 0.50f; // 50% au milieu

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

	BYTE blue;
	BYTE green;
	BYTE red;
	const int x = static_cast<int>(tile_corner_x * tile_size);
	const int y = static_cast<int>(tile_corner_y * tile_size);

	const int pixel_offset = x * my_bm_info.bmiHeader.biWidth + y;
	const BYTE* pixel_address = lp_pixels + pixel_offset * (my_bm_info.bmiHeader.biBitCount / 8);
	const auto color = SimpleColor(static_cast<int>(pixel_address[2]), static_cast<int>(pixel_address[1]),
		static_cast<int>(pixel_address[0]));

	return color.equals(white_color, 0.98f) ? 1 : color.equals(black_color, 0.98f) ? 0 : -1;
}

// Fonction qui cherche la position du plateau de chess.com sur l'écran
void locate_chessboard(int& top_left_x, int& top_left_y, int& bottom_right_x, int& bottom_right_y) {
	static const SimpleColor dark_square_color(119, 153, 84);
	static const SimpleColor light_square_color(233, 237, 204);

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
	BYTE blue;
	BYTE green;
	BYTE red;
	// Distance au bas
	int y;
	int pixel_offset;
	SimpleColor color;

	// Cherche au milieu de l'écran, mais on peut faire un cadrillage si besoin...

	// Cherche la gauche du plateau
	int x = screen_height / 2;
	for (y = 0; y < screen_width; y++) {
		pixel_offset = x * my_bm_info.bmiHeader.biWidth + y;
		pixel_address = lp_pixels + pixel_offset * (my_bm_info.bmiHeader.biBitCount / 8);
		color = SimpleColor(static_cast<int>(pixel_address[2]), static_cast<int>(pixel_address[1]), static_cast<int>(pixel_address[0]));
		if (color.equals(dark_square_color, 0.98f) || color.equals(light_square_color, 0.98f)) {
			top_left_x = y;
			break;
		}
	}

	// Cherche le bas du plateau
	y = top_left_x + 10;
	for (x = 0; x < screen_height; x++) {
		pixel_offset = x * my_bm_info.bmiHeader.biWidth + y;
		pixel_address = lp_pixels + pixel_offset * (my_bm_info.bmiHeader.biBitCount / 8);
		color = SimpleColor(static_cast<int>(pixel_address[2]), static_cast<int>(pixel_address[1]), static_cast<int>(pixel_address[0]));
		if (color.equals(dark_square_color, 0.98f) || color.equals(light_square_color, 0.98f)) {
			bottom_right_y = screen_height - x;
			break;
		}
	}

	// Cherche le haut du plateau
	y = top_left_x + 10;
	for (x = screen_height; x > 0; x--) {
		pixel_offset = x * my_bm_info.bmiHeader.biWidth + y;
		pixel_address = lp_pixels + pixel_offset * (my_bm_info.bmiHeader.biBitCount / 8);
		color = SimpleColor(static_cast<int>(pixel_address[2]), static_cast<int>(pixel_address[1]), static_cast<int>(pixel_address[0]));
		if (color.equals(dark_square_color, 0.98f) || color.equals(light_square_color, 0.98f)) {
			top_left_y = screen_height - x;
			break;
		}
	}

	// Droite du plateau (normalement il est carré)
	bottom_right_x = top_left_x + bottom_right_y - top_left_y;

	return;
}

// TODO : clean
// -> Clean les fonctions
// -> Pouvoir changer les couleurs du plateau
// -> Optimiser encore les fonctions pour get les pixels
// -> Promotions à gérer (pour vs les bots, car c'est pas automatique)
// -> Gérer le temps de Grogros (récupérer celui de chess.com?)
// -> Faire une fonction qui récupère le plateau automatiquement? (la position)