#include "neural_network.h"
#include "raylib.h"
#include <cmath>
#include <iostream>

// Default constructor
Network::Network() {
	// Generation of the layers
	_layers.resize(_layers_dimensions.size());

	for (int i = 0; i < _layers.size(); i++)
		_layers[i].resize(_layers_dimensions[i], 0.0f); // Sets every value to 0

	// Generation of the weights
	_weights_dimensions.resize(_layers_dimensions.size() - 1);

	for (int i = 0; i < _weights_dimensions.size(); i++)
		_weights_dimensions[i] = _layers_dimensions[i] * _layers_dimensions[i + 1];

	_weights.resize(_weights_dimensions.size());

	for (int i = 0; i < _weights.size(); i++)
		_weights[i].resize(_weights_dimensions[i], 1.0f); // Initializes every weight to 1
}

// Copy constructor
Network::Network(Network& n) {
	// Copies the weights
}

// Computes the output (the whole network must be zeroed first)
void Network::calculate_output() {
	// Resets every value of the hidden and output layers to 0
	reset_values();

	// Node computation

	// For every layer
	for (int layer = 0; layer < _layers.size() - 1; layer++) {

		// Compute the node values of the next layer
		for (int k = 0; k < _layers[layer + 1].size(); k++) {

			// Compute the weighted sum of the nodes of the previous layer
			for (int i = 0; i < _layers[layer].size(); i++) {
				_layers[layer + 1][k] += _layers[layer][i] * _weights[layer][k * _layers[layer].size() + i];
			}

			// Apply the activation function
			//_layers[layer + 1][k] = _activation_function(_layers[layer + 1][k], 0, 1);
			//_layers[layer + 1][k] = linear_activation(_layers[layer + 1][k], 0.0f, 1.0f);
			_layers[layer + 1][k] = sigmoid_activation(_layers[layer + 1][k], 0.0f, 1.0f);
		}
	}

	// Set the output to the value of the single node of the last layer
	_output = _layers[_layers.size() - 1][0];
}

// Fills the input from a chess position in FEN form
void Network::input_from_fen(const string& fen) {
	// rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1

	// Iterator over the input layer of the neural network
	int k = 0;

	// Reset the values
	reset_values();

	for (const char c : fen) {
		switch (c) {
		case '/': case ' ': break;
		case 'P': _layers[0][k] = 1.0f; k += 12; break;
		case 'N': _layers[0][k + 1] = 1.0f; k += 12; break;
		case 'B': _layers[0][k + 2] = 1.0f; k += 12; break;
		case 'R': _layers[0][k + 3] = 1.0f; k += 12; break;
		case 'Q': _layers[0][k + 4] = 1.0f; k += 12; break;
		case 'K': _layers[0][k + 5] = 1.0f; k += 12; break;
		case 'p': _layers[0][k + 6] = 1.0f; k += 12; break;
		case 'n': _layers[0][k + 7] = 1.0f; k += 12; break;
		case 'b': _layers[0][k + 8] = 1.0f; k += 12; break;
		case 'r': _layers[0][k + 9] = 1.0f; k += 12; break;
		case 'q': _layers[0][k + 10] = 1.0f; k += 12; break;
		case 'k': _layers[0][k + 11] = 1.0f; k += 12; break;
		default:
			if (isdigit(c)) {
				const int digit = (static_cast<int>(c)) - (static_cast<int>('0'));
				k += 12 * digit;
				break;
			}

			else {
				return;
			}
		}
	}

	return;
}

// Generates random weights in the neural network
//void Network::generate_random_weights(const int min, const int max) {
//	for (int i = 0; i < _weights.size(); i++) {
//		for (int j = 0; j < _weights[i].size(); j++) {
//			_weights[i][j] = GetRandomValue(min, max);
//		}
//	}
//}

// Generates random weights in the neural network
void Network::generate_random_weights(const float min, const float max) {
	for (int i = 0; i < _weights.size(); i++) {
		for (int j = 0; j < _weights[i].size(); j++) {
			_weights[i][j] = (float)GetRandomValue(-INT_MAX / 2, INT_MAX / 2) / (INT_MAX / 2);
		}
	}
}

// Returns the distance between two evaluations
//unsigned int evaluation_distance(const int a, const int b) {
//	return abs(a - b);
//}

// Returns a norm of a vector of positive integers
//unsigned int vector_norm(const vector<int>& v) {
//	// Another way of computing it can be chosen depending on the needs
//	unsigned int sum = 0;
//	const int length = v.size();
//
//	for (const int k : v)
//		sum += pow(k, length);
//
//	return (pow(sum, 1.0f / static_cast<float>(length)));
//}

// Takes a vector of positions and the vector of their evaluations, and returns the global distance between the network's evaluations of those positions and the evaluations given as arguments
//int Network::global_distance(const vector<string>& positions_vector, const vector<int>& evaluations_vector) {
//	const int length = positions_vector.size();
//	vector<int> distances(length, 0);
//
//	for (int i = 0; i < length; i++) {
//		input_from_fen(positions_vector[i]);
//		calculate_output();
//		distances[i] = evaluation_distance(_layers[_layers.size() - 1][0], evaluations_vector[i]);
//	}
//
//	return vector_norm(distances);
//}

// Resets every value of the network to zero
void Network::reset_values() {
	for (int i = 1; i < _layers.size(); i++)
		for (int j = 0; j < _layers[i].size(); j++)
			_layers[i][j] = 0.0f;
}

// Displays the weights of the network
void Network::display_weights() {
	for (int i = 0; i < _weights.size(); i++) {
		for (int j = 0; j < _weights[i].size(); j++) {
			cout << _weights[i][j] << " ";
		}
		cout << endl;
	}
}

// Displays the values of the network
void Network::display_values() {
	for (int i = 0; i < _layers.size(); i++) {
		for (int j = 0; j < _layers[i].size(); j++) {
			cout << _layers[i][j] << " ";
		}
		cout << endl;
	}
}

// Activation functions for the neural network computations

// Linear activation function
//int linear_activation(const int k, const float alpha, const float beta) {
//	return alpha + k * beta;
//}

// Sigmoid activation function
float sigmoid_activation(const float sum, const float alpha, const float beta) {
	//cout << "sum: " << sum << ", result: " << alpha + beta * (1.0f / (1.0f + exp(-sum))) << endl;
	return alpha + beta * (1.0f / (1.0f + exp(-sum)));
}

// Returns an evaluation from the output (which roughly represents the winning chances, between 0 and 1)
int Network::output_eval(int mate_value, float half_win_proba, int half_win_eval) {
	// 0 -> -mate_value
	// 1 -> mate_value
	// 0.5 -> 0
	// 0.667? -> +100, parameter to be defined

	float a = std::atanh(half_win_proba * 2 - 1); // Shift to use custom_prob as midpoint
	float b = std::atanh(_output * 2 - 1); // Shift input to the same range

	float evaluation = ((float)half_win_eval / a) * b;

	return evaluation;
}