#include "neural_network.h"
#include "raylib.h"
#include "math.h"
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
Network::Network(Network &n) {

    // Copie les poids
}




// Fonction qui calcule l'output (mettre tout le réseau à 0 au départ)
void Network::calculate_output() {

    // Remet toutes les valeurs à 0 dans les layers cachées et d'output
    for (int i = 1; i < _layers.size(); i++)
        for (int j = 0; j < _layers[i].size(); j++)
            _layers[i][j] = 0;

    // Calcul des noeuds
    for (int i = 0; i < _layers.size() - 1; i++) {
        for (int j = 0; j < _layers[i].size(); j++) {
            for (int k = 0; k < _layers[i + 1].size(); k++) {
                _layers[i + 1][k] += _layers[i][j] * _weights[i][_layers[i + 1].size() * j + k];
            }
        }
    }

    // Division par 1000 pour éviter les évaluations garguantuesques
    _output = _layers[_layers_dimensions.size() - 1][0] / 100;

}




// Fonction qui remplit l'input à l'aide d'une position d'échec sous forme FEN
void Network::input_from_fen(string fen) {

    // rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1

    // Itérateur dans la couche d'input du réseau de neurones
    int k = 0;

    int digit;

    // Remise à zéro des inputs
    for (int i = 0; i < 768; i++)
        _layers[0][i] = 0;


    for (char c : fen) {

        switch (c) {
            case '/' : case ' ' : break;
            case 'P' : _layers[0][k] = 1; k += 12; break;
            case 'N' : _layers[0][k + 1] = 1; k += 12; break;
            case 'B' : _layers[0][k + 2] = 1; k += 12; break;
            case 'R' : _layers[0][k + 3] = 1; k += 12; break;
            case 'Q' : _layers[0][k + 4] = 1; k += 12; break;
            case 'K' : _layers[0][k + 5] = 1; k += 12; break;
            case 'p' : _layers[0][k + 6] = 1; k += 12; break;
            case 'n' : _layers[0][k + 7] = 1; k += 12; break;
            case 'b' : _layers[0][k + 8] = 1; k += 12; break;
            case 'r' : _layers[0][k + 9] = 1; k += 12; break;
            case 'q' : _layers[0][k + 10] = 1; k += 12; break;
            case 'k' : _layers[0][k + 11] = 1; k += 12; break;
            default :
                if (isdigit(c)) {
                    digit = ((int)c) - ((int)'0');
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
void Network::generate_random_weights(int min, int max) {
    for (int i = 0; i < _weights.size(); i++) {
        for (int j = 0; j < _weights[i].size(); j++) {
            _weights[i][j] = GetRandomValue(min, max);
        }
    }    
}


// Fonction qui renvoie la distance entre deux évaluations
unsigned int evaluation_distance(int a, int b) {
    return abs(a - b);
}

// Fonction qui renvoie une norme d'un vecteur d'entiers positifs
unsigned int vector_norm(vector<int> v) {
    // On pourra choisir une autre manière de la calculer selon les besoins
    unsigned int sum = 0;
    int length = v.size();

    for (int k : v)
        sum += pow(k, length);

    return (pow(sum, 1.0 / (float)length));

}


// Fonction qui prend un vecteur de positions et le vecteur des évaluations associées, et renvoie la distance globale des évaluations des positions selon le réseau de neurones, comparées aux évaluations en argument
int Network::global_distance(vector<string> positions_vector, vector<int> evaluations_vector) {

    int length = positions_vector.size();
    vector<int> distances (length, 0);

    for (int i = 0; i < length; i++) {
        input_from_fen(positions_vector[i]);
        calculate_output();
        distances[i] = evaluation_distance(_layers[_layers.size() - 1][0], evaluations_vector[i]);
    }

    return vector_norm(distances);

}


// Fonctions d'activation pour les calculs du réseau de neurones

// Fonction d'activation linéaire
int linear_activation(int k, float alpha, float beta) {
    return alpha + k * beta;
}