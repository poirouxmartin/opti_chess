#include "useful_functions.h"
#include "raylib.h"
#include <iostream>
#include <cmath>
#include <random>
#include <chrono>



// Fonction qui renvoie si une valeur est comprise dans un intervalle
bool is_in(const auto x, const auto min, const auto max)
{
	return (x >= min && x <= max);
}

// Fonction qui renvoie si un entier appartient à un intervalle
bool is_in(const int x, const int min, const int max)
{
	return (x >= min && x <= max);
}

// Fonction qui renvoie si un entier uint_fast8_t appartient à un intervalle
bool is_in_fast(const uint_fast8_t x, const uint_fast8_t min, const uint_fast8_t max)
{
	return (x >= min && x <= max);
}

// Fonction qui renvoie si un flottant appartient à un intervalle
bool is_in(const float x, const float min, const float max)
{
	return (x >= min && x <= max);
}

// Fonction qui renvoie le maximum de deux entiers
int max_int(const int a, const int b)
{
	return (a > b) ? a : b;
}

// Fonction qui renvoie le maximum de deux entiers
int min_int(const int a, const int b)
{
	return (a < b) ? a : b;
}

// Fonction qui renvoie le maximum de deux flottants
float max_float(const float a, const float b)
{
	return (a > b) ? a : b;
}

// Fonction qui renvoie le maximum de deux flottans
float min_float(const float a, const float b)
{
	return (a < b) ? a : b;
}

// Fonction pour générer une seed
unsigned long long generate_seed()
{
	const chrono::high_resolution_clock::duration d = chrono::high_resolution_clock::now().time_since_epoch();
	static unsigned long long seed = d.count();

	return seed;
}

// Fonction qui renvoie un entier aléatoire entre deux entiers (le second non inclus)
int rand_int(const int a, const int b)
{
	static unsigned long long seed = generate_seed();

	static default_random_engine generator;
	static bool generated_seed = false;

	if (!generated_seed)
		generator.seed(seed), generated_seed = true;

	uniform_int_distribution<int> distribution(a, b);

	return distribution(generator);
}

// Fonction qui renvoie un entier long aléatoire entre deux entiers (le second non inclus)
long long rand_long(const long long a, const long long b)
{
	static unsigned long long seed = generate_seed();

	static default_random_engine generator;
	static bool generated_seed = false;

	if (!generated_seed)
		generator.seed(seed), generated_seed = true;

	uniform_int_distribution<long long> distribution(a, b);

	return distribution(generator);
}

double rand_double(const double a, const double b)
{
	static thread_local std::mt19937 generator(std::random_device{}());
	std::uniform_real_distribution<double> distribution(a, b);

	return distribution(generator);
}

// Fonction qui renvoie la valeur maximum d'une liste d'entiers
int max_value(int* l, const int n)
{
	int max = -INT_MAX;

	for (int i = 0; i < n; i++)
		if (l[i] > max)
			max = l[i];

	return max;
}

// Fonction qui renvoie la valeur minimum d'une liste d'entiers
int min_value(int* l, const int n)
{
	int min = INT_MAX;

	for (int i = 0; i < n; i++)
		if (l[i] < min)
			min = l[i];

	return min;
}

// Fonction qui renvoie la valeur minimum d'une liste de flottans
int min_value(float* l, const int n)
{
	float min = FLT_MAX;

	for (int i = 0; i < n; i++)
		if (l[i] < min)
			min = l[i];

	return min;
}

// Fonction qui affiche une liste d'entiers (array)
void print_array(int *l, const int n)
{
	cout << "[|";
	for (int i = 0; i < n; i++)
		cout << " " << l[i] << " |";
	cout << "]" << endl;
}

// Fonction qui affiche une liste d'entiers longs (array)
void print_array(long long int *l, const int n)
{
	cout << "[|";
	for (int i = 0; i < n; i++)
		cout << " " << l[i] << " |";
	cout << "]" << endl;
}

// Fonction qui affiche une liste d'entiers 8 bits fast (array)
void print_array(int_fast8_t *l, const int n)
{
	cout << "[|";
	for (int i = 0; i < n; i++)
		cout << " " << static_cast<int>(l[i]) << " |";
	cout << "]" << endl;
}

// Fonction qui affiche une liste d'entiers 8 bits fast (array)
void print_array(uint_fast8_t *l, const int n)
{
	cout << "[|";
	for (int i = 0; i < n; i++)
		cout << " " << static_cast<int>(l[i]) << " |";
	cout << "]" << endl;
}

// Fonction qui affiche une liste de flottants (array)
void print_array(float *l, const int n)
{
	cout << "[|";
	for (int i = 0; i < n; i++)
		cout << " " << l[i] << " |";
	cout << "]" << endl;
}

// Fonction qui affiche une liste de double (array)
void print_array(double* l, const int n)
{
	cout << "[|";
	for (int i = 0; i < n; i++)
		cout << " " << l[i] << " |";
	cout << "]" << endl;
}

// Fonction qui affiche une liste de chaines de caractères (array)
void print_array(string *l, const int n)
{
	cout << "[|";
	for (int i = 0; i < n; i++)
		cout << " " << l[i] << " |";
	cout << "]" << endl;
}

// Fonction qui renvoie l'index de la valeur maximale d'une liste d'entiers
int max_index(int* l, const int n)
{
	int max = -INT_MAX;
	int max_i = 0;

	for (int i = 0; i < n; i++)
		if (l[i] > max)
		{
			max = l[i];
			max_i = i;
		}

	return max_i;
}

// Fonction qui renvoie l'index de la valeur maximale d'une liste d'entiers
int max_index(float* l, const int n)
{
	float max = -FLT_MAX;
	int max_i = 0;

	for (int i = 0; i < n; i++)
		if (l[i] > max)
		{
			max = l[i];
			max_i = i;
		}

	return max_i;
}

// Fonction qui renvoie l'index de la valeur maximale d'une liste d'entiers
int max_index(uint_fast8_t* l, const int n)
{
	int max = -UINT8_MAX;
	int max_i = 0;

	for (int i = 0; i < n; i++)
		if (l[i] > max)
		{
			max = l[i];
			max_i = i;
		}

	return max_i;
}

// Fonction qui renvoie l'index de la valeur maximale de deux listes d'entiers (la seconde est là pour départager en cas d'égalité)
int max_index(const int* l, const int n, const int* l_annex, const int sign)
{
	int max = -INT_MAX;
	int max_annex = -INT_MAX;
	int max_i = 0;

	for (int i = 0; i < n; i++)
	{
		if (l[i] > max)
		{
			max = l[i];
			max_annex = l_annex[i] * sign;
			max_i = i;
		}
		if (l[i] == max)
		{
			if (l_annex[i] * sign > max_annex)
			{
				max_annex = l_annex[i] * sign;
				max_i = i;
			}
		}
	}

	return max_i;
}

// Fonction qui renvoie l'index de la valeur minimale d'une liste d'entiers
int min_index(int* l, const int n)
{
	int min = INT_MAX;
	int min_i = 0;

	for (int i = 0; i < n; i++)
		if (l[i] < min)
		{
			min = l[i];
			min_i = i;
		}

	return min_i;
}

// Fonction qui calcule une distance entre deux points
float distance(const int row_1, const int col_1, const int row_2, const int col_2)
{
	return (row_1 - row_2) * (row_1 - row_2) + (col_1 - col_2) * (col_1 - col_2);
}

// Fonction qui calcule la proximité entre deux points (pour l'évaluation de la sécurité du roi)
float proximity(const int row_1, const int col_1, const int row_2, const int col_2, const float k)
{
	return k / distance(row_1, col_1, row_2, col_2);
}

// Fonction qui transforme un entier en string (et arrondit s'il est supérieur à 1000)
// TODO : cas négatif à gérer
string int_to_round_string(const int k)
{
	if (k < 1000)
		return to_string(k);
	if (k < 10000)
		return to_string(static_cast<float>(k) / 1000).substr(0, 3) + "k";
	if (k < 100000)
		return to_string(static_cast<float>(k) / 1000).substr(0, 4) + "k";
	if (k < 1000000)
		return to_string(static_cast<float>(k) / 1000).substr(0, 3) + "k";

	return to_string(static_cast<float>(k) / 1000000).substr(0, 3) + "M"; // FIXME: "10.M" -> "10M"
}

// Fonction qui transforme un entier en string (et arrondit s'il est supérieur à 1000)
string long_int_to_round_string(const unsigned long long k)
{
	if (k < 1000)
		return to_string(k);
	if (k < 10000)
		return to_string(static_cast<float>(k) / 1000).substr(0, 3) + "k";
	if (k < 100000)
		return to_string(static_cast<float>(k) / 1000).substr(0, 4) + "k";
	if (k < 1000000)
		return to_string(static_cast<float>(k) / 1000).substr(0, 3) + "k";
	if (k < 1000000000)
		return to_string(static_cast<float>(k) / 1000000).substr(0, 3) + "M";

	return to_string(static_cast<float>(k) / 1000000000).substr(0, 4) + "G";
}

// Fonction qui transforme un clock en string (pour les timestamps dans les PGN)
//string clock_to_string(const clock_t t, bool full)
//{
//	double elapsed_seconds = static_cast<double>(t) / CLOCKS_PER_SEC;
//
//	int days = static_cast<int>(elapsed_seconds) / (24 * 3600);
//	elapsed_seconds -= days * 24 * 3600;
//
//	int hours = static_cast<int>(elapsed_seconds) / 3600;
//	elapsed_seconds -= hours * 3600;
//
//	int minutes = static_cast<int>(elapsed_seconds) / 60;
//	double seconds = elapsed_seconds - (minutes * 60);
//
//	ostringstream oss;
//	oss << fixed << setprecision(3);
//
//	if (full) {
//		if (days > 0) {
//			oss << days << "d " << hours << "h " << minutes << "min " << setw(2) << setfill('0') << setprecision(0) << seconds << "s";
//		}
//		else if (hours > 0) {
//			oss << hours << "h " << minutes << "min " << setw(2) << setfill('0') << setprecision(0) << seconds << "s";
//		}
//		else if (minutes > 0) {
//			oss << minutes << "min " << setw(2) << setfill('0') << setprecision(0) << seconds << "s";
//		}
//		else {
//			oss << setprecision(3) << seconds << "s";
//		}
//	}
//	else {
//		if (days > 0) {
//			oss << days << ":" << setw(2) << setfill('0') << hours << ":" << setw(2) << setfill('0') << minutes << ":" << setw(2) << setfill('0') << setprecision(0) << seconds;
//		}
//		else if (hours > 0) {
//			oss << hours << ":" << setw(2) << setfill('0') << minutes << ":" << setw(2) << setfill('0') << setprecision(0) << seconds;
//		}
//		else if (minutes > 0) {
//			oss << minutes << ":" << setw(2) << setfill('0') << setprecision(0) << seconds;
//		}
//		else {
//			oss << setw(2) << setfill('0') << setprecision(3) << seconds;
//		}
//	}
//
//	return oss.str();
//}

string clock_to_string(const clock_t t, bool full) {
	double elapsed_seconds = static_cast<double>(t) / CLOCKS_PER_SEC;

	int days = static_cast<int>(elapsed_seconds) / (24 * 3600);
	elapsed_seconds -= days * 24 * 3600;

	int hours = static_cast<int>(elapsed_seconds) / 3600;
	elapsed_seconds -= hours * 3600;

	int minutes = static_cast<int>(elapsed_seconds) / 60;
	double seconds = elapsed_seconds - (minutes * 60);

	// Un buffer suffisamment grand pour toutes les combinaisons possibles
	// Exemple : "999d 23h 59min 59s" ou "99:59:59:59.999"
	// Un char[64] est très sécuritaire.
	char buffer[64];
	int len;

	if (full) {
		if (days > 0) {
			len = std::snprintf(buffer, sizeof(buffer), "%dd %dh %dmin %02.0fs",
				days, hours, minutes, seconds);
		}
		else if (hours > 0) {
			len = std::snprintf(buffer, sizeof(buffer), "%dh %dmin %02.0fs",
				hours, minutes, seconds);
		}
		else if (minutes > 0) {
			len = std::snprintf(buffer, sizeof(buffer), "%dmin %02.0fs",
				minutes, seconds);
		}
		else {
			len = std::snprintf(buffer, sizeof(buffer), "%.3fs", seconds);
		}
	}
	else { // Format compact (ex: 1:23:45)
		if (days > 0) {
			len = std::snprintf(buffer, sizeof(buffer), "%d:%02d:%02d:%02.0f",
				days, hours, minutes, seconds);
		}
		else if (hours > 0) {
			len = std::snprintf(buffer, sizeof(buffer), "%d:%02d:%02.0f",
				hours, minutes, seconds);
		}
		else if (minutes > 0) {
			len = std::snprintf(buffer, sizeof(buffer), "%d:%02.0f",
				minutes, seconds);
		}
		else {
			len = std::snprintf(buffer, sizeof(buffer), "%.3f", seconds);
		}
	}

	// Gérer le cas où snprintf échoue ou tronque (très rare avec un buffer suffisant)
	if (len < 0 || len >= sizeof(buffer)) {
		// Fallback ou gestion d'erreur, par exemple retourner une chaîne vide ou une erreur
		return "ERROR";
	}

	return std::string(buffer, len);
}

// Fonction qui arrondit un flottant en entier
int float_to_int(const float x)
{
	return static_cast<int>(x) + (x - static_cast<int>(x) > 0.5f);
}

// Fonction qui renvoie si une chaine de caractères est présente dans un tableau de taille n
bool is_in(const string& s, string string_array[], const int n)
{
	for (int i = 0; i < n; i++)
		if (s == string_array[i])
			return true;

	return false;
}

// Fonction qui pondère les valeurs de la liste, en fonction d'un taux d'exploration par valeur
void nodes_weighting(double* l, const double* weights, const int size)
{
	for (int i = 0; i < size; i++)
	{
		if (l[i] * weights[i] < 0) {
			cout << "incoming overflow: " << l[i] << " * " << weights[i] << endl;
		}

		if (l[i] < 0 || l[i] > INT_MAX) {
			cout << "negative or big value: " << l[i] << endl;
		}
		if (weights[i] < 0 || weights[i] > INT_MAX) {
			cout << "negative or big weight: " << weights[i] << endl;
		}

		l[i] *= weights[i];

		if (l[i] < 0) {
			cout << "overflow in node weighting? " << l[i] << endl;
		}
	}
}

// Sigmoïde
double sigmoid(double x, double alpha, double beta) {
	double k = 1.0 / alpha * log(1.0 / beta - 1.0);
	return 1.0 / (1.0 + exp(k * x));
}

// Fonction qui renvoie la valeur d'évaluation en fonction de l'avancement, et d'un facteur multiplicatif en fonction de l'avancement
double eval_from_progress(const int eval, const float advancement, const float factor) {
	return eval * max(0.0f, 1.0f + advancement * (factor - 1.0f));
}