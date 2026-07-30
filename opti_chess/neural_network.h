#pragma once
#include <vector>
#include <string>

using namespace std;

// Activation functions for the weights? (to keep them between 0 and 1??) -> exponentials between 0 and 1
// An output between 0 and 1 would represent the winning probability (for White?)
// Switch to floats instead?

// Documentation
// https://www.v7labs.com/blog/neural-networks-activation-functions
// https://en.wikipedia.org/wiki/Activation_function

// TODO: take parameters other than just the pieces into account? (side to move, move count...)

class Network {
public:

	// Attributes

	// Layers
	vector<int> _layers_dimensions = { 768, 64, 1 };
	vector<vector<float>> _layers;

	// Weights
	vector<int> _weights_dimensions;
	vector<vector<float>> _weights;

	// Output (when there is a single value) -> to keep things simple
	float _output = 0;

	// Activation function
	//int (*_activation_function)(int, float, float) = linear_activation;

	// Constructors

	// Default constructor
	Network();

	// Copy constructor
	Network(Network&);

	// Functions

	// Computes the output (the whole network must be zeroed first)
	void calculate_output();

	// Fills the input from a chess position in FEN form
	void input_from_fen(const string&);

	// Fills the input from a board
	// TODO

	// Generates random weights in the neural network
	//void generate_random_weights(int min = -100, int max = 100);

	// Generates random weights in the neural network
	void generate_random_weights(float min = -1.0f, float max = 1.0f);

	// Takes a vector of positions and the vector of their evaluations, and returns the global distance between the network's evaluations of those positions and the evaluations given as arguments
	//int global_distance(const vector<string>&, const vector<int>&);

	// Resets every value of the network to zero
	void reset_values();

	// Displays the weights of the network
	void display_weights();

	// Displays the values of the network
	void display_values();

	// Returns an evaluation from the output (which roughly represents the winning chances, between 0 and 1)
	int output_eval(int mate_value, float half_win_proba = 0.667f, int half_win_eval = 100);
};

// Returns the distance between two evaluations
//unsigned int evaluation_distance(int, int);

// Returns a norm of a vector of integers
//unsigned int vector_norm(const vector<int>&);

// Activation functions for the neural network computations

// Linear activation function
//int linear_activation(int, float alpha = 0.0f, float beta = 1.0f);

// Sigmoid activation function
float sigmoid_activation(float sum, float alpha = 0.0f, float beta = 1.0f);