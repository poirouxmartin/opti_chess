#pragma once
#include <vector>
#include <string>

using namespace std;

// Fonctions d'activation pour les weights? (pour les mettre entre 0 et 1??) -> exponentielles entre 0 et 1
// L'output entre 0 et 1 représenterait la probabilité de gain (pour les blancs?)
// Passage en flottants à la place?

// Documentation
// https://www.v7labs.com/blog/neural-networks-activation-functions
// https://en.wikipedia.org/wiki/Activation_function

// TODO: prendre en compte d'autres paramètres que simplement les pièces? (genre le trait, nombre de coups...)

class Network {
public:

	// Attributs

	// Layers
	vector<int> _layers_dimensions = { 768, 64, 1 };
	vector<vector<float>> _layers;

	// Weights
	vector<int> _weights_dimensions;
	vector<vector<float>> _weights;

	// Output (si y'a une seule valeur) -> pour simplifier
	float _output = 0;

	// Fonction d'activation
	//int (*_activation_function)(int, float, float) = linear_activation;

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

	// Fonction qui remplit l'input à l'aide d'un plateau
	// TODO

	// Fonction qui génère des poids aléatoires dans le réseau de neurones
	//void generate_random_weights(int min = -100, int max = 100);

	// Fonction qui génère des poids aléatoires dans le réseau de neurones
	void generate_random_weights(float min = -1.0f, float max = 1.0f);

	// Fonction qui prend un vecteur de positions et le vecteur des évaluations associées, et renvoie la distance globale des évaluations des positions selon le réseau de neurones, comparées aux évaluations en argument
	//int global_distance(const vector<string>&, const vector<int>&);

	// Fonction qui remet à zéro toutes les valeurs du réseau
	void reset_values();

	// Fonction qui affiche les poids du réseau
	void display_weights();

	// Fonction qui affiche les valeurs du réseau
	void display_values();

	// Fonction qui renvoie une évaluation en fonction de l'output (qui représente à peu près les chances de gain entre 0 et 1)
	int output_eval(int mate_value, float half_win_proba = 0.667f, int half_win_eval = 100);
};

// Fonction qui renvoie la distance entre deux évaluations
//unsigned int evaluation_distance(int, int);

// Fonction qui renvoie une norme d'un vecteur d'entiers
//unsigned int vector_norm(const vector<int>&);

// Fonctions d'activation pour les calculs du réseau de neurones

// Fonction d'activation linéaire
//int linear_activation(int, float alpha = 0.0f, float beta = 1.0f);

// Fonction d'activation sigmoïde
float sigmoid_activation(float sum, float alpha = 0.0f, float beta = 1.0f);