#pragma once
#include <vector>
#include <string>
using namespace std;

// Fonctions d'activation pour les weights? (pour les mettre entre 0 et 1??)
// Passage en flottants à la place?

// Documentation
// https://www.v7labs.com/blog/neural-networks-activation-functions
// https://en.wikipedia.org/wiki/Activation_function

class Network {
public:

	// Attributs

	// Layers
	vector<int> _layers_dimensions = { 768, 64, 1 };
	vector<vector<int>> _layers;

	// Weights
	vector<int> _weights_dimensions;
	vector<vector<int>> _weights;

	// Output (si y'a une seule valeur) -> pour simplifier
	int _output = 0;

	// Constructeurs

	// Constructeur par défaut
	Network();

	// Constructeur par copie
	Network(Network&);

	// Fonctions

	// Fonction qui calcule l'output (mettre tout le réseau à 0 au départ)
	void calculate_output();

	// Fonction qui remplit l'input à l'aide d'une position d'échec sous forme FEN
	void input_from_fen(const string&);

	// Fonction qui génère des poids aléatoires dans le réseau de neurones
	void generate_random_weights(int min = -100, int max = 100);

	// Fonction qui prend un vecteur de positions et le vecteur des évaluations associées, et renvoie la distance globale des évaluations des positions selon le réseau de neurones, comparées aux évaluations en argument
	int global_distance(const vector<string>&, const vector<int>&);
};

// Fonction qui renvoie la distance entre deux évaluations
unsigned int evaluation_distance(int, int);

// Fonction qui renvoie une norme d'un vecteur d'entiers
unsigned int vector_norm(const vector<int>&);

// Fonctions d'activation pour les calculs du réseau de neurones

// Fonction d'activation linéaire
int linear_activation(int, float alpha = 0, float beta = 1);
