class SimpleColor {
	public:
		int _r; // Red
		int _g; // Green
		int _b; // Blue

		SimpleColor();
		SimpleColor(int, int, int);
		void Print();
		bool Equals(SimpleColor);
		bool Equals(SimpleColor, float alike);
};


// Fonction qui simule un clic de souris � une position donn�e
void simulate_mouse_click(int, int);

// Fonction qui palce la souris � une position donn�e
void set_mouse_pos(int, int);

// Fonction qui renvoie la m�moire disponible dans l'ordinateur
unsigned long long  get_total_system_memory();

// Fonction qui affiche la couleur de chacune des cases de l'�chiquier sur l'�cran, en donnant ses coordonn�es (top-left, bottom-right)
int* get_board_move(int, int, int, int, bool orientation = false, bool display = false);

// Fonction qui clique un coup en fonction de l'orientation du plateau
void click_move(int j1, int i1, int j2, int i2, int x1, int y1, int x2, int y2, bool orientation = false);