#include "useful_functions.h"
#include "raylib.h"
#include <iostream>
#include "math.h"
#include <random>
#include <chrono>

using namespace std;



// Fonction qui renvoie si un entier appartient à un intervalle
bool is_in(int x, int min, int max) {
    return (x >= min && x <= max);
}

// Fonction qui renvoie le maximum de deux entiers
int max_int(int a, int b) {
    return (a > b) ? a : b;
}

// Fonction qui renvoie le maximum de deux entiers
int min_int(int a, int b) {
    return (a < b) ? a : b;
}


// Fonction de détermination pour les probabilités des coups (exponentielle?)
int move_power(float n, float range, float min) {
    // Rapport de force entre le meilleur et le pire coup
    float r = 10000.0;

    // Normalisation : plus petit coup à 10.0 (pas 1.0 car sinon ça peut devenir 0 en int) (pas très efficace... niveau temps de calcul...)
    float k = 10.0 / pow(r, (float)(min / range));
    // cout << "min : " << min << ", range : " << range << ", pow : " << pow(r, (float)(min / range)) << ", k : " << k << endl;

    return k * pow(r, (float)(n / range));
}


void softmax(int* input, int size, double beta) {
    float r = 10000.0;

	int i;
	double m, sum, constant;

	m = -INFINITY;
	for (i = 0; i < size; ++i) {
		if (m < input[i]) {
			m = input[i];
		}
	}

	sum = 0.0;
	for (i = 0; i < size; ++i) {
		sum += exp(beta * (input[i] - m));
	}

	constant = beta * m + log(sum);
	for (i = 0; i < size; ++i) {
		input[i] = r * exp(beta * input[i] - constant);
	}

}



// Fonction pour générer une seed
int generate_seed() {
    
    chrono::high_resolution_clock::duration d = chrono::high_resolution_clock::now().time_since_epoch();
    static unsigned seed = d.count();

    return seed;
}



// Fonction qui renvoie un entier aléatoire entre deux entiers (le second non inclus)
int rand_int(int a, int b) {

    static int seed = generate_seed();

    static default_random_engine generator;
    static bool generated_seed = false;

    if (!generated_seed)
            generator.seed(seed), generated_seed = true;

    uniform_int_distribution<int> distribution(a, b);

    // cout << a << ", " << b << " -> " << distribution(generator) << endl;

    return distribution(generator);
}


// Fonction qui renvoie parmi une liste d'entiers, renvoie un index aléatoire, avec une probabilité variantes, en fonction de la grandeur du nombre correspondant à cet index
int pick_random_good_move(int* l, int n, int color, bool print) {
    int sum;
    int min = 1000000;

    int range = max_value(l, n) - min_value(l, n);
    int min_val;

    if (color == 1)
        min_val = min_value(l, n);
    else
        min_val = max_value(l, n);

    int *l2 = new int[n];

    for (int i = 0; i < n; i++) {
        // l2[i] = move_power(color * l[i], range, color * min_val);
        l2[i] = color * l[i];
    }

    softmax(l2, n);

    if (print) {
        print_array(l, n);
        print_array(l2, n);
    }


    // Fonctionne pas? à vérifier... les derniers coups n'ont plus de noeuds...

    for (int i = 0; i < n; i++) {
        if (l2[i] < min)
            min = l2[i];
        sum += l2[i];
    }

    // cout << sum << endl;

    int rand_val = rand_int(1, sum);

    int cumul = 0;

    for (int i = 0; i < n; i++) {
        cumul += l2[i];
        if (cumul >= rand_val)
            return i;
    }

    return 0;

}



// Fonction qui renvoie la valeur maximum d'une liste d'entiers
int max_value(int* l, int n) {
    int max = -1000000;

    for (int i = 0; i < n; i++)
        if (l[i] > max)
            max = l[i];

    return max;

}

// Fonction qui renvoie la valeur minimum d'une liste d'entiers
int min_value(int* l, int n) {
    int min = 1000000;

    for (int i = 0; i < n; i++)
        if (l[i] < min)
            min = l[i];

    return min;

}


// Fonction qui affiche une liste d'entiers (array)
void print_array(int* l, int n) {
    cout << "[|";
    for (int i = 0; i < n; i++)
        cout << " " << l[i] << " |";
    cout << "]" << endl;
}


// Fonction qui affiche une liste de flottans (array)
void print_array(float* l, int n) {
    cout << "[|";
    for (int i = 0; i < n; i++)
        cout << " " << l[i] << " |";
    cout << "]" << endl;
}


// Fonction qui renvoie l'index de la valeur maximale d'une liste d'entiers
int max_index(int* l, int n) {
    int max = -1000000;
    int max_i = 0;

    for (int i = 0; i < n; i++)
        if (l[i] > max) {
            max = l[i];
            max_i = i;
        }
            

    return max_i;

}


// Fonction qui renvoie l'index de la valeur minimale d'une liste d'entiers
int min_index(int* l, int n) {
    int min = 1000000;
    int min_i = 0;

    for (int i = 0; i < n; i++)
        if (l[i] < min) {
            min = l[i];
            min_i = i;
        }
            

    return min_i;

}