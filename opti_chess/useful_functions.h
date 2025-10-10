#pragma once

#include <execution>
#include <string>
#include "time.h"

using namespace std;

// Fonction qui renvoie si un entier appartient à un intervalle
bool is_in(int x, int min, int max);

// Fonction qui renvoie si un flottant appartient à un intervalle
bool is_in(float x, float min, float max);

// Fonction qui renvoie si un entier uint8_t appartient à un intervalle
bool is_in_fast(uint8_t x, uint8_t min, uint8_t max);

// Fonction qui renvoie le maximum de deux entiers
int max_int(int, int);

// Fonction qui renvoie le minimum de deux entiers
int min_int(int, int);

// Fonction qui renvoie le maximum de deux flottants
float max_float(float, float);

// Fonction qui renvoie le minimum de deux flottants
float min_float(float, float);

// Fonction pour générer une seed
unsigned long long generate_seed();

// Fonction qui renvoie un entier aléatoire entre deux entiers (le second non inclus)
int rand_int(int, int);

// Fonction qui renvoie un entier long aléatoire entre deux entiers (le second non inclus)
long long rand_long(const long long a, const long long b);

// Fonction qui renvoie un double aléatoire entre deux double
double rand_double(double, double);

// Fonction qui renvoie la valeur maximum d'une liste d'entiers
int max_value(int*, const int);

// Fonction qui renvoie la valeur minimum d'une liste d'entiers
int min_value(int*, const int);

// Fonction qui renvoie la valeur minimum d'une liste de flottans
int min_value(float*, const int);

// Fonction qui affiche une liste d'entiers (array)
void print_array(int*, const int);

// Fonction qui affiche une liste d'entiers longs (array)
void print_array(long long int*, const int);

// Fonction qui affiche une liste d'entiers 8 bits fast (array)
void print_array(int_fast8_t*, const int);

// Fonction qui affiche une liste d'entiers positifs 8 bits fast (array)
void print_array(uint8_t*, const int);

// Fonction qui affiche une liste de flottants (array)
void print_array(float*, const int);

// Fonction qui affiche une liste de double (array)
void print_array(double*, const int);

// Fonction qui affiche une liste de chaines de caractères (array)
void print_array(string*, const int);

// Fonction qui renvoie l'index de la valeur maximale d'une liste d'entiers
int max_index(int*, const int);

// Fonction qui renvoie l'index de la valeur maximale d'une liste de flottants
int max_index(float*, const int);

// Fonction qui renvoie l'index de la valeur maximale d'une liste d'entiers uint8_t
int max_index(uint8_t*, const int);

// Fonction qui renvoie l'index de la valeur maximale de deux listes d'entiers (la seconde est là pour départager en cas d'égalité)
int max_index(const int*, int, const int*, int);

// Fonction qui renvoie l'index de la valeur minimale d'une liste d'entiers
int min_index(int*, const int);

// Fonction qui calcule une distance entre deux points
float distance(int, int, int, int);

// Fonction qui calcule la proximité entre deux points (pour l'évaluation de la sécurité du roi)
float proximity(int, int, int, int, float k = 2);

// Fonction qui transforme un entier en string (et arrondit s'il est supérieur à 1000)
string int_to_round_string(int);

// Fonction qui transforme un entier en string (et arrondit s'il est supérieur à 1000)
string long_int_to_round_string(unsigned long long);

// Fonction qui transforme un clock en string (pour les timestamps dans les PGN)
string clock_to_timestamp(const clock_t t, bool full);

// Fonction qui transforme un clock en string (pour les timestamps dans les PGN)
string clock_to_string(clock_t, bool full = false);

// Fonction qui arrondit un flottant en entier
int float_to_int(float);

// Fonction qui renvoie si une chaine de caractères est présente dans un tableau de taille n
bool is_in(const string&, string[], int);

// Fonction qui pondère les valeurs de la liste, en fonction d'un taux d'exploration par valeur
void nodes_weighting(double*, const double*, int);

// Sigmoïde
double sigmoid(double x, double alpha, double beta);

// Fonction qui renvoie la valeur d'évaluation en fonction de l'avancement, et d'un facteur multiplicatif en fonction de l'avancement
double eval_from_progress(const int eval, const float advancement, const float factor);