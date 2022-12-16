#include <execution>
#include <string>

using namespace std;

// Fonction qui renvoie si un entier appartient à un intervalle
bool is_in(int, int, int);

// Fonction qui renvoie si un flottant appartient à un intervalle
bool is_in(float, float, float);

// Fonction qui renvoie le maximum de deux entiers
int max_int(int, int);

// Fonction qui renvoie le minimum de deux entiers
int min_int(int, int);

// Fonction qui renvoie le maximum de deux flottants
float max_float(float, float);

// Fonction qui renvoie le minimum de deux flottants
float min_float(float, float);

// Fonction de détermination pour les probabilités des coups (exponentielle?)
int move_power(float, float, float);

// Fonction qui permet de donner une puissance au coup afin de faciliter les choix
void softmax(int*, int, double beta = 0.035, int k_add = 50); // beta = 0.05, k_add = 1. k_add x => ~x/10000 (the bigger the larger, the smaller the deeper) 0.05, 250 pour les mats

// Fonction pour générer une seed
int generate_seed();

// Fonction qui renvoie un entier aléatoire entre deux entiers (le second non inclus)
int rand_int(int, int);

// Fonction qui renvoie parmi une liste d'entiers, renvoie un index aléatoire, avec une probabilité variante en fonction de la grandeur du nombre correspondant à cet index
int pick_random_good_move(int[], int, int, bool, double beta = 0.035, int k_add = 50);

// Fonction qui renvoie la valeur maximum d'une liste d'entiers
int max_value(int[], int);

// Fonction qui renvoie la valeur minimum d'une liste d'entiers
int min_value(int[], int);

// Fonction qui renvoie la valeur minimum d'une liste de flottans
int min_value(float[], int);

// Fonction qui affiche une liste d'entiers (array)
void print_array(int[], int);

// Fonction qui affiche une liste d'entiers 8 bits fast (array)
void print_array(int_fast8_t[], int);

// Fonction qui affiche une liste d'entiers positifs 8 bits fast (array)
void print_array(uint_fast8_t [], int);

// Fonction qui affiche une liste de flottants (array)
void print_array(float[], int);

// Fonction qui affiche une liste de chaines de caractères (array)
void print_array(string[], int);

// Fonction qui renvoie l'index de la valeur maximale d'une liste d'entiers
int max_index(int[], int);

// Fonction qui renvoie l'index de la valeur maximale de deux listes d'entiers (la seconde est là pour départager en cas d'égalité)
int max_index(int*, int, int*, int);

// Fonction qui renvoie l'index de la valeur minimale d'une liste d'entiers
int min_index(int[], int);

// Fonction qui renvoie la RAM disponible
unsigned long long getTotalSystemMemory();

// Fonction qui calcule une distance entre deux points
float distance(int, int, int, int);

// Fonction qui calcule la proximité entre deux points (pour l'évaluation de la sécurité du roi)
float proximity(int, int, int, int, float k = 2);

// Fonction qui transforme un entier en string (et arrondit s'il est supérieur à 1000)
string int_to_round_string(int);

// Fonction qui transforme un clock en string (pour les timestamps dans les PGN)
string clock_to_string(clock_t, bool full = false);

// Fonction qui arrondit un flottant en entier
int float_to_int(float);

// Fonction qui renvoie si une chaine de caractères est présente dans un tableau de taille n
bool is_in(string, string[], int);