#include "useful_functions.h"
#include "raylib.h"
#include <iostream>
#include <cmath>
#include <random>
#include <chrono>



// Returns whether a value lies within an interval
//bool is_in(const auto x, const auto min, const auto max)
//{
//	return (x >= min && x <= max);
//}

// Returns whether an integer lies within an interval
bool is_in(const int x, const int min, const int max) noexcept
{
	return (x >= min && x <= max);
}

// Returns whether a uint8_t integer lies within an interval
bool is_in_fast(const uint8_t x, const uint8_t min, const uint8_t max) noexcept
{
	//return static_cast<uint8_t>(x - min) <= static_cast<uint8_t>(max - min);
	return (x >= min && x <= max);
}

//inline constexpr bool is_in_fast(uint8_t x, uint8_t min, uint8_t max) noexcept {
//	return static_cast<uint8_t>(x - min) <= static_cast<uint8_t>(max - min);
//}


// Returns whether a float lies within an interval
bool is_in(const float x, const float min, const float max) noexcept
{
	return (x >= min && x <= max);
}

// Returns the maximum of two integers
int max_int(const int a, const int b)
{
	return (a > b) ? a : b;
}

// Returns the minimum of two integers
int min_int(const int a, const int b)
{
	return (a < b) ? a : b;
}

// Returns the maximum of two floats
float max_float(const float a, const float b)
{
	return (a > b) ? a : b;
}

// Returns the minimum of two floats
float min_float(const float a, const float b)
{
	return (a < b) ? a : b;
}

// Generates a seed
unsigned long long generate_seed()
{
	const chrono::high_resolution_clock::duration d = chrono::high_resolution_clock::now().time_since_epoch();
	static unsigned long long seed = d.count();

	return seed;
}

// Returns a random integer between two integers (the second one excluded)
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

// Returns a random long integer between two integers (the second one excluded)
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

// Returns the maximum value of a list of integers
int max_value(int* l, const int n)
{
	int max = -INT_MAX;

	for (int i = 0; i < n; i++)
		if (l[i] > max)
			max = l[i];

	return max;
}

// Returns the minimum value of a list of integers
int min_value(int* l, const int n)
{
	int min = INT_MAX;

	for (int i = 0; i < n; i++)
		if (l[i] < min)
			min = l[i];

	return min;
}

// Returns the minimum value of a list of floats
int min_value(float* l, const int n)
{
	float min = FLT_MAX;

	for (int i = 0; i < n; i++)
		if (l[i] < min)
			min = l[i];

	return min;
}

// Prints a list of integers (array)
void print_array(int *l, const int n)
{
	cout << "[|";
	for (int i = 0; i < n; i++)
		cout << " " << l[i] << " |";
	cout << "]" << endl;
}

// Prints a list of long integers (array)
void print_array(long long int *l, const int n)
{
	cout << "[|";
	for (int i = 0; i < n; i++)
		cout << " " << l[i] << " |";
	cout << "]" << endl;
}

// Prints a list of fast 8-bit integers (array)
void print_array(int_fast8_t *l, const int n)
{
	cout << "[|";
	for (int i = 0; i < n; i++)
		cout << " " << static_cast<int>(l[i]) << " |";
	cout << "]" << endl;
}

// Prints a list of unsigned fast 8-bit integers (array)
void print_array(uint8_t *l, const int n)
{
	cout << "[|";
	for (int i = 0; i < n; i++)
		cout << " " << static_cast<int>(l[i]) << " |";
	cout << "]" << endl;
}

// Prints a list of floats (array)
void print_array(float *l, const int n)
{
	cout << "[|";
	for (int i = 0; i < n; i++)
		cout << " " << l[i] << " |";
	cout << "]" << endl;
}

// Prints a list of doubles (array)
void print_array(double* l, const int n)
{
	cout << "[|";
	for (int i = 0; i < n; i++)
		cout << " " << l[i] << " |";
	cout << "]" << endl;
}

// Prints a list of strings (array)
void print_array(string *l, const int n)
{
	cout << "[|";
	for (int i = 0; i < n; i++)
		cout << " " << l[i] << " |";
	cout << "]" << endl;
}

// Returns the index of the maximum value of a list of integers
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

// Returns the index of the maximum value of a list of floats
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

// Returns the index of the maximum value of a list of uint8_t integers
int max_index(uint8_t* l, const int n)
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

// Returns the index of the maximum value of two lists of integers (the second one breaks ties)
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

// Returns the index of the minimum value of a list of integers
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

// Computes the distance between two points
float distance(const int row_1, const int col_1, const int row_2, const int col_2)
{
	return (row_1 - row_2) * (row_1 - row_2) + (col_1 - col_2) * (col_1 - col_2);
}

// Computes the proximity between two points (for the king safety evaluation)
float proximity(const int row_1, const int col_1, const int row_2, const int col_2, const float k)
{
	return k / distance(row_1, col_1, row_2, col_2);
}

// Turns an integer into a string (rounding it if it is greater than 1000)
// TODO: the negative case still has to be handled
string int_to_round_string(const int k)
{
	if (k < 1E3)
		return to_string(k);
	if (k < 1E4)
		return to_string(k / 1E3).substr(0, 3) + "k";
	if (k < 1E5)
		return to_string(k / 1E3).substr(0, 4) + "k";
	if (k < 1E6)
		return to_string(k / 1E3).substr(0, 3) + "k";
	if (k < 1E7)
		return to_string(k / 1E6).substr(0, 3) + "M";
	if (k < 1E8)
		return to_string(k / 1E6).substr(0, 4) + "M";
	if (k < 1E9)
		return to_string(k / 1E6).substr(0, 3) + "M";

	return to_string(k / 1E9).substr(0, 4) + "G";
}

// Turns a long integer into a string (rounding it if it is greater than 1000)
string long_int_to_round_string(const unsigned long long k)
{
	if (k < 1E3)
		return to_string(k);
	if (k < 1E4)
		return to_string(k / 1E3).substr(0, 3) + "k";
	if (k < 1E5)
		return to_string(k / 1E3).substr(0, 4) + "k";
	if (k < 1E6)
		return to_string(k / 1E3).substr(0, 3) + "k";
	if (k < 1E7)
		return to_string(k / 1E6).substr(0, 3) + "M";
	if (k < 1E8)
		return to_string(k / 1E6).substr(0, 4) + "M";
	if (k < 1E9)
		return to_string(k / 1E6).substr(0, 3) + "M";

	return to_string(k / 1E9).substr(0, 4) + "G";
}

// Turns a clock value into a string (for the timestamps in PGN files)
string clock_to_timestamp(const clock_t t, bool full)
{
	// Format: {[%clk 0:09:59.6]}
	double elapsed_seconds = static_cast<double>(t) / CLOCKS_PER_SEC;

	int hours = static_cast<int>(elapsed_seconds / 3600);
	int minutes = static_cast<int>((elapsed_seconds - hours * 3600) / 60);
	double seconds = elapsed_seconds - hours * 3600 - minutes * 60;

	char buffer[32];

	if (full)
		std::snprintf(buffer, sizeof(buffer), "{[%%clk %d:%02d:%04.1f]}", hours, minutes, seconds);
	else
		std::snprintf(buffer, sizeof(buffer), "{[%%clk %d:%02d]}", hours, minutes);

	return std::string(buffer);
}

string clock_to_string(const clock_t t, bool full) {
	double elapsed_seconds = static_cast<double>(t) / CLOCKS_PER_SEC;

	int days = static_cast<int>(elapsed_seconds) / (24 * 3600);
	elapsed_seconds -= days * 24 * 3600;

	int hours = static_cast<int>(elapsed_seconds) / 3600;
	elapsed_seconds -= hours * 3600;

	int minutes = static_cast<int>(elapsed_seconds) / 60;
	double seconds = elapsed_seconds - (minutes * 60);

	// A buffer large enough for every possible combination
	// For example: "999d 23h 59min 59s" or "99:59:59:59.999"
	// A char[64] is very much on the safe side.
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
	else { // Compact format (e.g. 1:23:45)
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

	// Handle snprintf failing or truncating (very rare with a large enough buffer)
	if (len < 0 || len >= sizeof(buffer)) {
		// Fallback or error handling, for instance returning an empty string or an error
		return "ERROR";
	}

	return std::string(buffer, len);
}

// Rounds a float into an integer
int float_to_int(const float x)
{
	return static_cast<int>(x) + (x - static_cast<int>(x) > 0.5f);
}

// Returns whether a string is present in an array of size n
bool is_in(const string& s, string string_array[], const int n)
{
	for (int i = 0; i < n; i++)
		if (s == string_array[i])
			return true;

	return false;
}

// Weights the values of the list according to an exploration rate per value
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

// Sigmoid
double sigmoid(double x, double alpha, double beta) {
	double k = 1.0 / alpha * log(1.0 / beta - 1.0);
	return 1.0 / (1.0 + exp(k * x));
}

// Returns the evaluation value as a function of the game advancement, with a multiplicative factor that also depends on it
double eval_from_progress(const int eval, const float advancement, const float factor) {
	return eval * max(0.0f, 1.0f + advancement * (factor - 1.0f));
}