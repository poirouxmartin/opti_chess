#pragma once

#include <execution>
#include <string>
#include "time.h"

using namespace std;

// Returns whether an integer lies within an interval
bool is_in(int x, int min, int max) noexcept;

// Returns whether a float lies within an interval
bool is_in(float x, float min, float max) noexcept;

// Returns whether a uint8_t integer lies within an interval
bool is_in_fast(uint8_t x, uint8_t min, uint8_t max) noexcept;

//inline constexpr bool is_in_fast(const uint8_t x, const uint8_t min, const uint8_t max) noexcept
//{
//	return (x >= min && x <= max);
//}

//inline constexpr bool is_in_fast(uint8_t x, uint8_t min, uint8_t max) noexcept {
//    return static_cast<uint8_t>(x - min) <= static_cast<uint8_t>(max - min);
//}


// Returns the maximum of two integers
int max_int(int, int);

// Returns the minimum of two integers
int min_int(int, int);

// Returns the maximum of two floats
float max_float(float, float);

// Returns the minimum of two floats
float min_float(float, float);

// Generates a seed
unsigned long long generate_seed();

// Returns a random integer between two integers (the second one excluded)
int rand_int(int, int);

// Returns a random long integer between two integers (the second one excluded)
long long rand_long(const long long a, const long long b);

// Returns a random double between two doubles
double rand_double(double, double);

// Returns the maximum value of a list of integers
int max_value(int*, const int);

// Returns the minimum value of a list of integers
int min_value(int*, const int);

// Returns the minimum value of a list of floats
int min_value(float*, const int);

// Prints a list of integers (array)
void print_array(int*, const int);

// Prints a list of long integers (array)
void print_array(long long int*, const int);

// Prints a list of fast 8-bit integers (array)
void print_array(int_fast8_t*, const int);

// Prints a list of unsigned fast 8-bit integers (array)
void print_array(uint8_t*, const int);

// Prints a list of floats (array)
void print_array(float*, const int);

// Prints a list of doubles (array)
void print_array(double*, const int);

// Prints a list of strings (array)
void print_array(string*, const int);

// Returns the index of the maximum value of a list of integers
int max_index(int*, const int);

// Returns the index of the maximum value of a list of floats
int max_index(float*, const int);

// Returns the index of the maximum value of a list of uint8_t integers
int max_index(uint8_t*, const int);

// Returns the index of the maximum value of two lists of integers (the second one breaks ties)
int max_index(const int*, int, const int*, int);

// Returns the index of the minimum value of a list of integers
int min_index(int*, const int);

// Computes the distance between two points
float distance(int, int, int, int);

// Computes the proximity between two points (for the king safety evaluation)
float proximity(int, int, int, int, float k = 2);

// Turns an integer into a string (rounding it if it is greater than 1000)
string int_to_round_string(int);

// Turns a long integer into a string (rounding it if it is greater than 1000)
string long_int_to_round_string(unsigned long long);

// Turns a clock value into a string (for the timestamps in PGN files)
string clock_to_timestamp(const clock_t t, bool full);

// Turns a clock value into a string (for the timestamps in PGN files)
string clock_to_string(clock_t, bool full = false);

// Rounds a float into an integer
int float_to_int(float);

// Returns whether a string is present in an array of size n
bool is_in(const string&, string[], int);

// Weights the values of the list according to an exploration rate per value
void nodes_weighting(double*, const double*, int);

// Sigmoid
double sigmoid(double x, double alpha, double beta);

// Returns the evaluation value as a function of the game advancement, with a multiplicative factor that also depends on it
double eval_from_progress(const int eval, const float advancement, const float factor);