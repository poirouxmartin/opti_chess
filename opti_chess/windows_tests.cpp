#pragma once

#include "windows.h"
#include <iostream>
#include "windows_tests.h"

using namespace std;


// Colours

// Lichess squares:
// light: 240, 217, 181
// dark: 181, 136, 99


// Colour constructor
SimpleColor::SimpleColor() = default;

// Colour constructor
SimpleColor::SimpleColor(const int r, const int g, const int b) {
	_r = r;
	_g = g;
	_b = b;
}

// Displays the components of the colour
void SimpleColor::print() const
{
	cout << "(" << _r << ", " << _g << ", " << _b << ")" << endl;
}

// Returns whether two colours have equal components
bool SimpleColor::equals(const SimpleColor c) const
{
	return _r == c._r && _g == c._g && _b == c._b;
}

// Returns whether two colours are close enough
bool SimpleColor::equals(const SimpleColor c, const float alike) const
{
	return color_distance(c) <= 1 - alike;
}

// Returns how close two colours are
float SimpleColor::color_distance(const SimpleColor c) const
{
	return (abs(_r - c._r) + abs(_g - c._g) + abs(_b - c._b)) / 255.0f / 3.0f;
}

// Simulates a mouse click at a given position
void simulate_mouse_click(const int x, const int y)
{
	SetCursorPos(x, y);
	INPUT input = { 0 };
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP;
	SendInput(1, &input, sizeof(INPUT));
	return;
}

// Releases the mouse click
void simulate_mouse_release()
{
	INPUT input = { 0 };
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
	SendInput(1, &input, sizeof(INPUT));
	return;
}

// Places the mouse at a given position
void set_mouse_pos(const int x, const int y) {
	SetCursorPos(x, y);
	return;
}

// Returns the memory available on the computer
// FIXME: this does not return the right thing
unsigned long long get_total_system_memory()
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return status.ullTotalPhys;
}

// Physical memory available (free) at the time of the call.
unsigned long long get_available_physical_memory()
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return status.ullAvailPhys;
}

// Function that gets the screen as bitmap
static HBITMAP get_screen_bmp(const HDC hdc, const int x1, const int y1, const int x2, const int y2) {
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

// Displays the colour of every square of the chessboard on screen, given its coordinates (top-left, bottom-right)
uint8_t* get_board_move(const int x1, const int y1, const int x2, const int y2, ChessSite website, const bool orientation, const bool display) {

	int x_begin = -1;
	int y_begin = -1;
	int x_end = -1;
	int y_end = -1;

	// Size of a square
	const float tile_size = (x2 - x1) / 8.0f;

	// Take a screenshot as a bmp
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

	// Look at every square of the chessboard
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			// Colour of the square
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

			// If the colour matches the colour of a played square
			if (color.equals(website._white_tile_played_color, 1.0f - website._tile_color_tolerance) || color.equals(website._black_tile_played_color, 1.0f - website._tile_color_tolerance)) {
				if (display) {
					cout << "moved tile : ";
				}

				// Check whether there is nothing on the square
				x = (i + 0.3f) * tile_size;
				y = (j + 0.5f) * tile_size;
				pixel_offset = x * my_bm_info.bmiHeader.biWidth + y;
				pixel_address = lp_pixels + pixel_offset * (my_bm_info.bmiHeader.biBitCount / 8);
				color = SimpleColor(static_cast<int>(pixel_address[2]), static_cast<int>(pixel_address[1]), static_cast<int>(pixel_address[0]));
				if (display) {
					cout << x << ", " << y << "(" << i << ", " << j << ") -> ";
					color.print();
				}

				// If so, this is the departure square
				if (!found_start_square && (color.equals(website._white_tile_played_color, 1.0f - website._tile_color_tolerance) || color.equals(website._black_tile_played_color, 1.0f - website._tile_color_tolerance))) {
					y_begin = orientation ? i : 7 - i;
					x_begin = orientation ? j : 7 - j;
					if (display)
						cout << "begin tile" << endl;

					if (found_end_square)
						goto found;

					found_start_square = true;
				}

				// Otherwise, this is the arrival square
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

	// Release the screen
	DeleteObject(h_bitmap);
	ReleaseDC(nullptr, hdc);
	delete[] lp_pixels;

	if (y_begin == -1 || x_begin == -1 || y_end == -1 || x_end == -1) {
		//cout << "No move found on the board" << endl;
		return nullptr; // No move found
	}

	// Coordinates of the move played
	const auto coord = new uint8_t[4];
	coord[0] = y_begin;
	coord[1] = x_begin;
	coord[2] = y_end;
	coord[3] = x_end;

	return coord;
}

// Clicks a move according to the orientation of the board
bool input_injection_enabled = false;

void click_move(const int j1, const int i1, const int j2, const int i2, const int x1, const int y1, const int x2, int y2, const bool orientation, const bool is_promotion) {
	// Off by default: nothing reaches the mouse until it is enabled at runtime.
	if (!input_injection_enabled)
		return;

	const float tile_size = static_cast<float>((x2 - x1)) / 8.0f;
	constexpr float tile_click = 0.5f; // So that the click lands in the middle of the square

	const int cx1 = x1 + (tile_click + (orientation ? i1 : 7 - i1)) * tile_size;
	const int cy1 = y1 + (tile_click + (orientation ? 7 - j1 : j1)) * tile_size;
	const int cx2 = x1 + (tile_click + (orientation ? i2 : 7 - i2)) * tile_size;
	const int cy2 = y1 + (tile_click + (orientation ? 7 - j2 : j2)) * tile_size;

	simulate_mouse_click(cx1, cy1);
	simulate_mouse_click(cx2, cy2);

	// Promotion: a second click may be needed
	if (is_promotion)
		simulate_mouse_click(cx2, cy2);

	return;
}

// Retrieves the orientation of the board. Returns 1 if White is at the bottom, 0 if Black is, -1 otherwise
int bind_board_orientation(const int x1, const int y1, const int x2, const int y2, ChessSite website) {

	cout << "looking for " << website._name << " board orientation..." << endl;

	// Size of a square
	const float tile_size = static_cast<float>((x2 - x1)) / 8.0f;

	// Colour of the square

	// Take a screenshot as a bmp
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

	// Where to look for the colour of the square
	const int x = static_cast<int>(website._piece_location_on_tile.first * tile_size);
	const int y = static_cast<int>(website._piece_location_on_tile.second * tile_size);

	const int pixel_offset = x * my_bm_info.bmiHeader.biWidth + y;
	const BYTE* pixel_address = lp_pixels + pixel_offset * (my_bm_info.bmiHeader.biBitCount / 8);
	const auto color = SimpleColor(static_cast<int>(pixel_address[2]), static_cast<int>(pixel_address[1]), static_cast<int>(pixel_address[0]));

	cout << "color of the piece: " << color._r << ", " << color._g << ", " << color._b << ", expected: " << website._white_piece_color._r << ", " << website._white_piece_color._g << ", " << website._white_piece_color._b << ", or " << website._black_piece_color._r << ", " << website._black_piece_color._g << ", " << website._black_piece_color._b << endl;

	const int orientation = color.equals(website._white_piece_color, 1.0f - website._piece_color_tolerance) ? 1 : (color.equals(website._black_piece_color, 1.0f - website._piece_color_tolerance) ? 0 : -1);

	cout << "board orientation: " << (orientation == 1 ? "white" : orientation == 0 ? "black" : "unknown") << endl << endl;

	return orientation;
}

// Looks for the position of the chess.com board on screen
bool locate_chessboard(int& top_left_x, int& top_left_y, int& bottom_right_x, int& bottom_right_y, ChessSite website) {

	// Look for the board on the website
	cout << "looking for " << website._name << " chessboard..." << endl;

	const int screen_width = GetSystemMetrics(SM_CXSCREEN);
	const int screen_height = GetSystemMetrics(SM_CYSCREEN);

	// Take a screenshot as a bmp
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

	// Distance to the bottom
	int y;
	int pixel_offset;
	SimpleColor color;

	// Look at the middle of the screen, but a grid scan could be done if needed...

	// Look for the left edge of the board
	bool found_left = false;
	int x = screen_height / 2;
	for (y = 0; y < screen_width; y++) {
		pixel_offset = x * my_bm_info.bmiHeader.biWidth + y;
		pixel_address = lp_pixels + pixel_offset * (my_bm_info.bmiHeader.biBitCount / 8);
		color = SimpleColor(static_cast<int>(pixel_address[2]), static_cast<int>(pixel_address[1]), static_cast<int>(pixel_address[0]));

		bool is_black_tile = color.equals(website._black_tile_color, 1.0f - website._tile_color_tolerance);
		bool is_white_tile = color.equals(website._white_tile_color, 1.0f - website._tile_color_tolerance);

		if (is_black_tile || is_white_tile) {
			top_left_x = y;
			found_left = true;
			cout << "left side of the chessboard found at x = " << top_left_x << " (color proximity: " << (is_black_tile ? "white - " : "black - ") << color.color_distance(is_black_tile ? website._black_tile_color : website._white_tile_color) << ")" << endl;
			break;
		}
	}

	// Look for the bottom edge of the board
	bool found_bottom = false;
	y = top_left_x + 10;
	for (x = 0; x < screen_height; x++) {
		pixel_offset = x * my_bm_info.bmiHeader.biWidth + y;
		pixel_address = lp_pixels + pixel_offset * (my_bm_info.bmiHeader.biBitCount / 8);
		color = SimpleColor(static_cast<int>(pixel_address[2]), static_cast<int>(pixel_address[1]), static_cast<int>(pixel_address[0]));

		bool is_black_tile = color.equals(website._black_tile_color, 1.0f - website._tile_color_tolerance);
		bool is_white_tile = color.equals(website._white_tile_color, 1.0f - website._tile_color_tolerance);

		if (is_black_tile || is_white_tile) {
			bottom_right_y = screen_height - x;
			found_bottom = true;
			cout << "bottom side of the chessboard found at y = " << bottom_right_y << " (color proximity: " << (is_black_tile ? "white - " : "black - ") << color.color_distance(is_black_tile ? website._black_tile_color : website._white_tile_color) << ")" << endl;
			break;
		}
	}

	// Look for the top edge of the board
	bool found_top = false;
	y = top_left_x + 10;
	for (x = screen_height; x > 0; x--) {
		pixel_offset = x * my_bm_info.bmiHeader.biWidth + y;
		pixel_address = lp_pixels + pixel_offset * (my_bm_info.bmiHeader.biBitCount / 8);
		color = SimpleColor(static_cast<int>(pixel_address[2]), static_cast<int>(pixel_address[1]), static_cast<int>(pixel_address[0]));

		bool is_black_tile = color.equals(website._black_tile_color, 1.0f - website._tile_color_tolerance);
		bool is_white_tile = color.equals(website._white_tile_color, 1.0f - website._tile_color_tolerance);

		if (is_black_tile || is_white_tile) {
			top_left_y = screen_height - x;
			found_top = true;
			cout << "top side of the chessboard found at y = " << top_left_y << " (color proximity: " << (is_black_tile ? "white - " : "black - ") << color.color_distance(is_black_tile ? website._black_tile_color : website._white_tile_color) << ")" << endl;
			break;
		}
	}

	// Right edge of the board (it is normally square)
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

// Default constructor
ChessSite::ChessSite() = default;

// TODO: clean
// -> Clean up the functions
// -> Be able to change the board colours
// -> Optimize the pixel-getting functions further
// -> Handle promotions (against bots, since it is not automatic there)
// -> Handle Grogros's clock (read the one from chess.com?)
// -> Write a function that locates the board automatically? (the position)