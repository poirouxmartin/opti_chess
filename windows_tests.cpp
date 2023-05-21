#include "windows.h"
#include <iostream>
#include "windows_tests.h"
using namespace std;


// Constructeur de couleur
SimpleColor::SimpleColor() {

}


// Constructeur de couleur
SimpleColor::SimpleColor(int r, int g, int b) {
    _r = r;
    _g = g;
    _b = b;
}


// Fonction qui affiche les composantes de la couleur
void SimpleColor::Print() {
    cout << "(" << _r << ", " << _g << ", " << _b << ")" << endl;
}


// Fonction qui renvoie si deux couleurs sont des composantes égales
bool SimpleColor::Equals(SimpleColor c) {
    return _r == c._r && _g == c._g && _b == c._b;
}

// Fonction qui renvoie si deux couleurs sont assez proches
bool SimpleColor::Equals(SimpleColor c, float alike) {
    return (abs(static_cast<int>(_r) - static_cast<int>(c._r)) + abs(static_cast<int>(_g) - static_cast<int>(c._g)) + abs(static_cast<int>(_b) - static_cast<int>(c._b))) / 255.0f / 3.0f <= 1 - alike;
}


// Fonction qui simule un clic de souris à une position donnée
void simulate_mouse_click(int x, int y)
{
    SetCursorPos(x, y);
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
    return;
}


// Fonction qui palce la souris à une position donnée
void set_mouse_pos(int x, int y) {
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
HBITMAP get_screen_bmp(HDC hdc, int x1, int y1, int x2, int y2) {
    // Calculate the width and height of the capture area
    int captureWidth = x2 - x1 + 1;
    int captureHeight = y2 - y1 + 1;

    // Create compatible DC, create a compatible bitmap and copy the screen area using BitBlt()
    HDC hCaptureDC = CreateCompatibleDC(hdc);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, captureWidth, captureHeight);
    HGDIOBJ hOld = SelectObject(hCaptureDC, hBitmap);
    BOOL bOK = BitBlt(hCaptureDC, 0, 0, captureWidth, captureHeight, hdc, x1, y1, SRCCOPY | CAPTUREBLT);

    SelectObject(hCaptureDC, hOld); // always select the previously selected object once done
    DeleteDC(hCaptureDC);
    return hBitmap;
}



// Fonction qui affiche la couleur de chacune des cases de l'échiquier sur l'écran, en donnant ses coordonnées (top-left, bottom-right)
int* get_board_move(int x1, int y1, int x2, int y2, bool orientation, bool display) {

    // Pour un échiquier de base (blanc et vert sur chess.com)

    // Couleurs des coups joués
    // blanches : 247, 247, 105
    // noires : 187, 203, 43
    SimpleColor white_played(247, 247, 105);
    SimpleColor black_played(187, 203, 43);

    // Couleurs des cases normales
    // blanches : 238, 238, 210
    // noires : 118, 150, 86

    // Couleur de chaque case
    SimpleColor color;

    int x_begin = -1;
    int y_begin = -1;
    int x_end = -1;
    int y_end = -1;

    // Taille d'une case
    float tile_size = (x2 - x1) / 8.0f;

    // Couleur de la case à 85%, 85% (haut-droite)
    float tile_corner_x = 0.85f;
    float tile_corner_y = 0.85f;

    // Fait un screenshot en bmp
    HDC hdc = GetDC(0);
    HBITMAP hBitmap = get_screen_bmp(hdc, x1, y1, x2, y2);
    BITMAPINFO MyBMInfo = { 0 };
    MyBMInfo.bmiHeader.biSize = sizeof(MyBMInfo.bmiHeader);

    if (0 == GetDIBits(hdc, hBitmap, 0, 0, NULL, &MyBMInfo, DIB_RGB_COLORS))
        cout << "error" << endl;

    BYTE* lpPixels = new BYTE[MyBMInfo.bmiHeader.biSizeImage];
    MyBMInfo.bmiHeader.biCompression = BI_RGB;

    if (0 == GetDIBits(hdc, hBitmap, 0, MyBMInfo.bmiHeader.biHeight, (LPVOID)lpPixels, &MyBMInfo, DIB_RGB_COLORS))
        cout << "error2" << endl;

    BYTE* pixelAddress;
    BYTE blue;
    BYTE green;
    BYTE red;
    int x;
    int y;
    int pixelOffset;


    // Regarde chaque case de l'échiquier
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            // Couleur de la case
            x = (i + tile_corner_x) * tile_size;
            y = (j + tile_corner_y) * tile_size;
            pixelOffset = x * MyBMInfo.bmiHeader.biWidth + y;
            pixelAddress = lpPixels + pixelOffset * (MyBMInfo.bmiHeader.biBitCount / 8);
            color = SimpleColor(static_cast<int>(pixelAddress[2]), static_cast<int>(pixelAddress[1]), static_cast<int>(pixelAddress[0]));
            if (display) {
                cout << x << ", " << y << "(" << i << ", " << j << ") -> ";
                color.Print();
            }
            

            // Si la couleur correspond à une couleur de case jouée
            if (color.Equals(white_played, 0.98f) || color.Equals(black_played, 0.98f)) {
                if (display) {
                    cout << "moved tile : ";
                }


                // Regarde s'il y'a rien sur la case
                x = (i + 0.3f) * tile_size;
                y = (j + 0.5f) * tile_size;
                pixelOffset = x * MyBMInfo.bmiHeader.biWidth + y;
                pixelAddress = lpPixels + pixelOffset * (MyBMInfo.bmiHeader.biBitCount / 8);
                color = SimpleColor(static_cast<int>(pixelAddress[2]), static_cast<int>(pixelAddress[1]), static_cast<int>(pixelAddress[0]));
                if (display) {
                    cout << x << ", " << y << "(" << i << ", " << j << ") -> ";
                    color.Print();
                }

                // Si c'est le cas, alors c'est la case de départ
                if (color.Equals(white_played, 0.98f) || color.Equals(black_played, 0.98f)) {
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
    DeleteObject(hBitmap);
    ReleaseDC(NULL, hdc);
    delete[] lpPixels;

    // Coordonnées du coup joué
    int* coord = new int[4];
    coord[0] = y_begin;
    coord[1] = x_begin;
    coord[2] = y_end;
    coord[3] = x_end;

    return coord;
}


// Fonction qui clique un coup en fonction de l'orientation du plateau
void click_move(int j1, int i1, int j2, int i2, int x1, int y1, int x2, int y2, bool orientation) {
    float tile_size = (x2 - x1) / 8.0f;
    float tile_click = 0.5f; // Pour que ça clique au milieu de la case 

    int cx1 = x1 + (tile_click + (orientation ? i1 : 7 - i1)) * tile_size;
    int cy1 = y1 + (tile_click + (orientation ? 7 - j1 : j1)) * tile_size;
    int cx2 = x1 + (tile_click + (orientation ? i2 : 7 - i2)) * tile_size;
    int cy2 = y1 + (tile_click + (orientation ? 7 - j2 : j2)) * tile_size;

    simulate_mouse_click(cx1, cy1);
    simulate_mouse_click(cx2, cy2);

    return;
}


// Fonction qui récupère l'orientation du plateau. Renvoie 1 si les blancs sont en bas, 0 si c'est les noirs, -1 sinon
int bind_board_orientation(int x1, int y1, int x2, int y2) {

    // Couleur des pièces sur le plateau
    SimpleColor white_color(248, 248, 248);
    SimpleColor black_color(86, 83, 82);

    // Taille d'une case
    float tile_size = (x2 - x1) / 8.0f;

    // Couleur de la case à :
    float tile_corner_x = 0.15f; // 15% en bas
    float tile_corner_y = 0.50f; // 50% au milieu

    // Couleur de la case
    SimpleColor color;


    // Fait un screenshot en bmp
    HDC hdc = GetDC(0);
    HBITMAP hBitmap = get_screen_bmp(hdc, x1, y1, x2, y2);
    BITMAPINFO MyBMInfo = { 0 };
    MyBMInfo.bmiHeader.biSize = sizeof(MyBMInfo.bmiHeader);

    if (0 == GetDIBits(hdc, hBitmap, 0, 0, NULL, &MyBMInfo, DIB_RGB_COLORS))
        cout << "error" << endl;

    BYTE* lpPixels = new BYTE[MyBMInfo.bmiHeader.biSizeImage];
    MyBMInfo.bmiHeader.biCompression = BI_RGB;

    if (0 == GetDIBits(hdc, hBitmap, 0, MyBMInfo.bmiHeader.biHeight, (LPVOID)lpPixels, &MyBMInfo, DIB_RGB_COLORS))
        cout << "error2" << endl;

    BYTE* pixelAddress;
    BYTE blue;
    BYTE green;
    BYTE red;
    int x = tile_corner_x * tile_size;
    int y = tile_corner_y * tile_size;
    int pixelOffset;
    
    pixelOffset = x * MyBMInfo.bmiHeader.biWidth + y;
    pixelAddress = lpPixels + pixelOffset * (MyBMInfo.bmiHeader.biBitCount / 8);
    color = SimpleColor(static_cast<int>(pixelAddress[2]), static_cast<int>(pixelAddress[1]), static_cast<int>(pixelAddress[0]));


    return color.Equals(white_color, 0.98f) ? 1 : color.Equals(black_color, 0.98f) ? 0 : -1;
}



// Fonction qui cherche la position du plateau de chess.com sur l'écran
void locate_chessboard(int& topLeftX, int& topLeftY, int& bottomRightX, int& bottomRightY) {
    SimpleColor dark_square_color(118, 150, 86);
    SimpleColor light_square_color(238, 238, 210);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Fait un screenshot en bmp
    HDC hdc = GetDC(0);
    HBITMAP hBitmap = get_screen_bmp(hdc, 0, 0, screenWidth, screenHeight);
    BITMAPINFO MyBMInfo = { 0 };
    MyBMInfo.bmiHeader.biSize = sizeof(MyBMInfo.bmiHeader);

    if (0 == GetDIBits(hdc, hBitmap, 0, 0, NULL, &MyBMInfo, DIB_RGB_COLORS))
        cout << "error" << endl;

    BYTE* lpPixels = new BYTE[MyBMInfo.bmiHeader.biSizeImage];
    MyBMInfo.bmiHeader.biCompression = BI_RGB;

    if (0 == GetDIBits(hdc, hBitmap, 0, MyBMInfo.bmiHeader.biHeight, (LPVOID)lpPixels, &MyBMInfo, DIB_RGB_COLORS))
        cout << "error2" << endl;

    BYTE* pixelAddress;
    BYTE blue;
    BYTE green;
    BYTE red;
    int x; // distance au bas
    int y; // distance à gauche
    int pixelOffset;
    SimpleColor color;


    // Cherche au milieu de l'écran, mais on peut faire un cadrillage si besoin...

    // Cherche la gauche du plateau
    x = screenHeight / 2;
    for (y = 0; y < screenWidth; y++) {
        pixelOffset = x * MyBMInfo.bmiHeader.biWidth + y;
        pixelAddress = lpPixels + pixelOffset * (MyBMInfo.bmiHeader.biBitCount / 8);
        color = SimpleColor(static_cast<int>(pixelAddress[2]), static_cast<int>(pixelAddress[1]), static_cast<int>(pixelAddress[0]));
        if (color.Equals(dark_square_color, 0.98f) || color.Equals(light_square_color, 0.98f)) {
            topLeftX = y;
            break;
        }
    }

    // Cherche le bas du plateau
    y = topLeftX + 10;
    for (x = 0; x < screenHeight; x++) {
        pixelOffset = x * MyBMInfo.bmiHeader.biWidth + y;
        pixelAddress = lpPixels + pixelOffset * (MyBMInfo.bmiHeader.biBitCount / 8);
        color = SimpleColor(static_cast<int>(pixelAddress[2]), static_cast<int>(pixelAddress[1]), static_cast<int>(pixelAddress[0]));
        if (color.Equals(dark_square_color, 0.98f) || color.Equals(light_square_color, 0.98f)) {
            bottomRightY = screenHeight - x;
            break;
        }
    }

    // Cherche le haut du plateau
    y = topLeftX + 10;
    for (x = screenHeight; x > 0; x--) {
        pixelOffset = x * MyBMInfo.bmiHeader.biWidth + y;
        pixelAddress = lpPixels + pixelOffset * (MyBMInfo.bmiHeader.biBitCount / 8);
        color = SimpleColor(static_cast<int>(pixelAddress[2]), static_cast<int>(pixelAddress[1]), static_cast<int>(pixelAddress[0]));
        if (color.Equals(dark_square_color, 0.98f) || color.Equals(light_square_color, 0.98f)) {
            topLeftY = screenHeight - x;
            break;
        }
    }

    // Droite du plateau (normalement il est carré)
    bottomRightX = topLeftX + bottomRightY - topLeftY;

    return;
}




// TODO : clean
// -> Clean les fonctions
// -> Pouvoir changer les couleurs du plateau
// -> Optimiser encore les fonctions pour get les pixels
// -> Promotions à gérer (pour vs les bots, car c'est pas automatique)
// -> Gérer le temps de Grogros
// -> Faire une fonction qui récupère le plateau automatiquement?

