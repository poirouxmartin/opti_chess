#include "neural_network.h"
#include "raylib.h"
#include <cmath>
#include <iostream>

// Constructeur par défaut
Network::Network() {
	// Génération des layers
	_layers.resize(_layers_dimensions.size());

	for (int i = 0; i < _layers.size(); i++)
		_layers[i].resize(_layers_dimensions[i], 0); // Met toutes les valeurs à 0

	// Génération des poids
	_weights_dimensions.resize(_layers_dimensions.size() - 1);

	for (int i = 0; i < _weights_dimensions.size(); i++)
		_weights_dimensions[i] = _layers_dimensions[i] * _layers_dimensions[i + 1];

	_weights.resize(_weights_dimensions.size());

	for (int i = 0; i < _weights.size(); i++)
		_weights[i].resize(_weights_dimensions[i], 1); // Initialise tous les poids à 1
}

// Constructeur par copie
Network::Network(Network& n) {
	// Copie les poids
}

// Fonction qui calcule l'output (mettre tout le réseau à 0 au départ)
void Network::calculate_output() {

	// Remet toutes les valeurs à 0 dans les layers cachées et d'output
	reset_values();

	// Calcul des noeuds

	// Pour chaque couche
	for (int layer = 0; layer < _layers.size() - 1; layer++) {

		// On calcule les valeurs des noeuds de la couche suivante
		for (int k = 0; k < _layers[layer + 1].size(); k++) {

			// On calcule la somme pondérée des noeuds de la couche précédente
			for (int i = 0; i < _layers[layer].size(); i++) {
				_layers[layer + 1][k] += _layers[layer][i] * _weights[layer][k * _layers[layer].size() + i];
			}

			// On applique la fonction d'activation
			//_layers[layer + 1][k] = _activation_function(_layers[layer + 1][k], 0, 1);
			_layers[layer + 1][k] = linear_activation(_layers[layer + 1][k], 0.0f, 1.0f);
		}
	}

	// On met l'output à la valeur du seul noeud de la dernière couche
	_output = _layers[_layers.size() - 1][0];
}

// Fonction qui remplit l'input à l'aide d'une position d'échec sous forme FEN
void Network::input_from_fen(const string& fen) {
	// rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1

	// Itérateur dans la couche d'input du réseau de neurones
	int k = 0;

	// Remise à zéro des valeurs
	reset_values();

	for (const char c : fen) {
		switch (c) {
		case '/': case ' ': break;
		case 'P': _layers[0][k] = 1; k += 12; break;
		case 'N': _layers[0][k + 1] = 1; k += 12; break;
		case 'B': _layers[0][k + 2] = 1; k += 12; break;
		case 'R': _layers[0][k + 3] = 1; k += 12; break;
		case 'Q': _layers[0][k + 4] = 1; k += 12; break;
		case 'K': _layers[0][k + 5] = 1; k += 12; break;
		case 'p': _layers[0][k + 6] = 1; k += 12; break;
		case 'n': _layers[0][k + 7] = 1; k += 12; break;
		case 'b': _layers[0][k + 8] = 1; k += 12; break;
		case 'r': _layers[0][k + 9] = 1; k += 12; break;
		case 'q': _layers[0][k + 10] = 1; k += 12; break;
		case 'k': _layers[0][k + 11] = 1; k += 12; break;
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

// Fonction qui génère des poids aléatoires dans le réseau de neurones
void Network::generate_random_weights(const int min, const int max) {
	for (int i = 0; i < _weights.size(); i++) {
		for (int j = 0; j < _weights[i].size(); j++) {
			_weights[i][j] = GetRandomValue(min, max);
		}
	}
}

// Fonction qui renvoie la distance entre deux évaluations
unsigned int evaluation_distance(const int a, const int b) {
	return abs(a - b);
}

// Fonction qui renvoie une norme d'un vecteur d'entiers positifs
unsigned int vector_norm(const vector<int>& v) {
	// On pourra choisir une autre manière de la calculer selon les besoins
	unsigned int sum = 0;
	const int length = v.size();

	for (const int k : v)
		sum += pow(k, length);

	return (pow(sum, 1.0f / static_cast<float>(length)));
}

// Fonction qui prend un vecteur de positions et le vecteur des évaluations associées, et renvoie la distance globale des évaluations des positions selon le réseau de neurones, comparées aux évaluations en argument
int Network::global_distance(const vector<string>& positions_vector, const vector<int>& evaluations_vector) {
	const int length = positions_vector.size();
	vector<int> distances(length, 0);

	for (int i = 0; i < length; i++) {
		input_from_fen(positions_vector[i]);
		calculate_output();
		distances[i] = evaluation_distance(_layers[_layers.size() - 1][0], evaluations_vector[i]);
	}

	return vector_norm(distances);
}

// Fonction qui remet à zéro toutes les valeurs du réseau
void Network::reset_values() {
	for (int i = 1; i < _layers.size(); i++)
		for (int j = 0; j < _layers[i].size(); j++)
			_layers[i][j] = 0;
}

// Fonction qui affiche les poids du réseau
void Network::display_weights() {
	for (int i = 0; i < _weights.size(); i++) {
		for (int j = 0; j < _weights[i].size(); j++) {
			cout << _weights[i][j] << " ";
		}
		cout << endl;
	}
}

// Fonction qui affiche les valeurs du réseau
void Network::display_values() {
	for (int i = 0; i < _layers.size(); i++) {
		for (int j = 0; j < _layers[i].size(); j++) {
			cout << _layers[i][j] << " ";
		}
		cout << endl;
	}
}

// Fonctions d'activation pour les calculs du réseau de neurones

// Fonction d'activation linéaire
int linear_activation(const int k, const float alpha, const float beta) {
	return alpha + k * beta;
}