// Fonction qui renvoie si un entier appartient à un intervalle
bool is_in(int, int, int);

// Fonction qui renvoie le maximum de deux entiers
int max_int(int, int);

// Fonction qui renvoie le minimum de deux entiers
int min_int(int, int);

// Fonction qui renvoie le maximum de deux flottants
float max_float(float, float);

// Fonction qui renvoie le minimum de deux flottans
float min_float(float, float);

// Fonction de détermination pour les probabilités des coups (exponentielle?)
int move_power(float, float, float);

// Fonction qui permet de donner une puissance au coup afin de faciliter les choix
void softmax(int*, int, double beta = 0.035, int k_add = 25); // beta = 0.05, k_add = 1

// Fonction pour générer une seed
int generate_seed();

// Fonction qui renvoie un entier aléatoire entre deux entiers (le second non inclus)
int rand_int(int, int);

// Fonction qui renvoie parmi une liste d'entiers, renvoie un index aléatoire, avec une probabilité variante en fonction de la grandeur du nombre correspondant à cet index
int pick_random_good_move(int[], int, int, bool);

// Fonction qui renvoie la valeur maximum d'une liste d'entiers
int max_value(int[], int);

// Fonction qui renvoie la valeur minimum d'une liste d'entiers
int min_value(int[], int);

// Fonction qui affiche une liste d'entiers (array)
void print_array(int[], int);

// Fonction qui affiche une liste de flottans (array)
void print_array(float[], int);

// Fonction qui renvoie l'index de la valeur maximale d'une liste d'entiers
int max_index(int[], int);

// Fonction qui renvoie l'index de la valeur minimale d'une liste d'entiers
int min_index(int[], int);