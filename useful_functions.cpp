#include "useful_functions.h"
#include "raylib.h"
#include <iostream>
#include "math.h"
#include <random>
#include <chrono>
//#include <windows.h>



// Fonction qui renvoie si un entier appartient à un intervalle
bool is_in(int x, int min, int max) {
    return (x >= min && x <= max);
}

// Fonction qui renvoie si un flottant appartient à un intervalle
bool is_in(float x, float min, float max) {
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

// Fonction qui renvoie le maximum de deux flottants
float max_float(float a, float b) {
    return (a > b) ? a : b;
}

// Fonction qui renvoie le maximum de deux flottans
float min_float(float a, float b) {
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


void softmax(int* input, int size, double beta, int k_add) {
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
        input[i] += k_add; // Juste histoire de continuer à regarder un peu les coups (on sait jamais)
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

    // cout << "random gen : " << a << ", " << b << "->" << distribution(generator) << endl;

    return distribution(generator);
}


// Fonction qui renvoie parmi une liste d'entiers, renvoie un index aléatoire, avec une probabilité variantes, en fonction de la grandeur du nombre correspondant à cet index
int pick_random_good_move(int* l, int n, int color, bool print, double beta, int k_add) {
    int sum = 0;
    int min = 2147483647;

    int range = max_value(l, n) - min_value(l, n);
    int min_val;

    if (color == 1)
        min_val = min_value(l, n);
    else
        min_val = max_value(l, n);

    int l2[250];

    for (int i = 0; i < n; i++) {
        // l2[i] = move_power(color * l[i], range, color * min_val);
        l2[i] = color * l[i];
    }


    softmax(l2, n, beta, k_add);

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
    if (print)
        cout << "rand : " << rand_val << endl;

    int cumul = 0;

    for (int i = 0; i < n; i++) {
        cumul += l2[i];
        if (cumul >= rand_val) {
            if (print)
                cout << "val : " << i << endl;
            return i;
        }
    }

    return 0;

}



// Fonction qui renvoie la valeur maximum d'une liste d'entiers
int max_value(int* l, int n) {
    int max = -2147483647;

    for (int i = 0; i < n; i++)
        if (l[i] > max)
            max = l[i];

    return max;

}

// Fonction qui renvoie la valeur minimum d'une liste d'entiers
int min_value(int* l, int n) {
    int min = 2147483647;

    for (int i = 0; i < n; i++)
        if (l[i] < min)
            min = l[i];

    return min;

}


// Fonction qui renvoie la valeur minimum d'une liste de flottans
int min_value(float* l, int n) {
    float min = 2147483647;

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


// Fonction qui affiche une liste d'entiers 8 bits fast (array)
void print_array(int_fast8_t* l, int n) {
    cout << "[|";
    for (int i = 0; i < n; i++)
        cout << " " << (int)l[i] << " |";
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


// Fonction qui renvoie l'index de la valeur maximale de deux listes d'entiers (la seconde est là pour départager en cas d'égalité)
int max_index(int* l, int n, int* l_annex, int sign) {
    int max = -2147483648;
    int max_annex = -2147483648;
    int max_i = 0;

    for (int i = 0; i < n; i++) {
        if (l[i] > max) {
            max = l[i];
            max_annex = l_annex[i] * sign;
            max_i = i;
        }
        if (l[i] == max) {
            if (l_annex[i] * sign > max_annex) {
                max_annex = l_annex[i] * sign;
                max_i = i;
            }
        }
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


unsigned long long getTotalSystemMemory() {
    // MEMORYSTATUSEX status;
    // status.dwLength = sizeof(status);
    // GlobalMemoryStatusEx(&status);
    // return status.ullTotalPhys;
    return 0;
}



// Fonction qui calcule une distance entre deux points
float distance(int i, int j, int x, int y) {
    // return sqrtf((i - x) * (i - x) + (j - y) * (j - y));
    return (i - x) * (i - x) + (j - y) * (j - y);
}


// Fonction qui calcule la proximité entre deux points (pour l'évaluation de la sécurité du roi)
float proximity(int i, int j, int x, int y, float k) {
    return k / distance(i, j, x, y);
}

// Fonction qui transforme un entier en string (et arrondit s'il est supérieur à 1000)
string int_to_round_string(int k) {
    if (k < 1000)
        return to_string(k);
    if (k < 10000)
        return to_string((float)k / 1000).substr(0, 3) + "k";
    if (k < 100000)
        return to_string((float)k / 1000).substr(0, 4) + "k";
    if (k < 1000000)
        return to_string((float)k / 1000).substr(0, 3) + "k";
    
    return to_string((float)k / 1000000).substr(0, 3) + "M";
}

// Fonction qui transforme un clock en string (pour les timestamps dans les PGN)
string clock_to_string(clock_t t, bool full) {
    int ms = t % 1000;
    int s = t / 1000;
    int m = (s / 60) % 60;
    int h = s / 3600;
    s = s % 60;

    string time = "";

    if (h || full) {
        string hours = to_string(h);
        if (hours.length() == 1)
            hours = "0" + hours;
        time += hours + ":";
    }

    if (m || full) {
        string minutes = to_string(m);
        if (minutes.length() == 1)
            minutes = "0" + minutes;
        time += minutes + ":";
    }

    string seconds = to_string(s);
    if (seconds.length() == 1)
        seconds = "0" + seconds;
    time += seconds;

    if (full || (!h && !m))
        time += "." + to_string(ms);
    

    return time;
}

// Fonction qui arrondit un flottant en entier
int float_to_int(float x) {
    return (int)x + (x - (int)x > 0.5);
}