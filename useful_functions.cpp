#include "useful_functions.h"
#include "raylib.h"
#include <iostream>
#include "math.h"
#include <random>
#include <chrono>
#include <cstdlib>
#include <type_traits>


// Fonction qui renvoie si un entier appartient à un intervalle
bool is_in(int x, int min, int max) {
    return (x >= min && x <= max);
}

// Fonction qui renvoie si un entier uint_fast8_t appartient à un intervalle
bool is_in_fast(uint_fast8_t x, uint_fast8_t min, uint_fast8_t max) {
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
    float k = 10.0 / pow(r, static_cast<float>(min / range));
    // cout << "min : " << min << ", range : " << range << ", pow : " << pow(r, static_cast<float>(min / range)) << ", k : " << k << endl;

    return k * pow(r, static_cast<float>(n / range));
}

// Fonction qui fait un softmax
void softmax(int* input, int size, float beta, float k_add) {
    int r = 10000;

	int i;
	float sum, constant;

    int m = *max_element(input, input + size);
		
	sum = 0.0f;
	for (i = 0; i < size; i++)
		sum += exp(beta * (input[i] - m));

    static const float evaluation_softener = 0.25f; // Pour que ça regarde un peu plus des coups en dessous (plus la valeur est faible, plus ça applanit le k_add)
    float win_chance;
    float best_win_chance = get_winning_chances_from_eval(m * evaluation_softener, false, true);
    float adding;

	constant = beta * m + log(sum);
	for (i = 0; i < size; i++) {
        // TODO prendre en compte les mats
        win_chance = get_winning_chances_from_eval(input[i] * evaluation_softener, false, true);
        adding = (win_chance == 0.0f) ? 0.0f : win_chance / best_win_chance * 2; // Juste histoire de continuer à regarder un peu les coups (on sait jamais)
		input[i] = r * exp(beta * input[i] - constant) + k_add * adding; 
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

    // return rand() % (b - a) + a;
}


// Fonction qui renvoie parmi une liste d'entiers, renvoie un index aléatoire, avec une probabilité variantes, en fonction de la grandeur du nombre correspondant à cet index
int pick_random_good_move(int* l, int n, int color, bool print, int nodes, int* nodes_children, float beta, float k_add) {
    int sum = 0;
    int min = inf_int;

    int range = max_value(l, n) - min_value(l, n);
    int min_val = (color == 1) ? min_value(l, n) : max_value(l, n);


    int l2[100];

    for (int i = 0; i < n; i++)
        l2[i] = color * l[i];

    softmax(l2, n, beta, k_add);

    // Liste de pondération en fonction de l'exploration de chaque noeud
    // Pour que ça explore les noeuds les moins regardés
    float pond[100];
    float inv_nodes = 1.0f / static_cast<float>(nodes);

    for (int i = 0; i < n; i++)
        pond[i] = 1.0f - static_cast<float>(nodes_children[i] * inv_nodes);

    nodes_ponderation(l2, pond, n);

    // print_array(l2, n);

    // Somme de toutes les valeurs
    for (int i = 0; i < n; i++) {
        if (l2[i] < min)
            min = l2[i];
        sum += l2[i];
    }

    // Choix du coup en fonction d'une valeur aléatoire
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
    int max = -inf_int;

    for (int i = 0; i < n; i++)
        if (l[i] > max)
            max = l[i];

    return max;

}

// Fonction qui renvoie la valeur minimum d'une liste d'entiers
int min_value(int* l, int n) {
    int min = inf_int;

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
void print_array(int l[], int n) {
    cout << "[|";
    for (int i = 0; i < n; i++)
        cout << " " << l[i] << " |";
    cout << "]" << endl;
}

// Fonction qui affiche une liste d'entiers 8 bits fast (array)
void print_array(int_fast8_t l[], int n) {
    cout << "[|";
    for (int i = 0; i < n; i++)
        cout << " " << static_cast<int>(l[i]) << " |";
    cout << "]" << endl;
}

// Fonction qui affiche une liste d'entiers 8 bits fast (array)
void print_array(uint_fast8_t l[], int n) {
    cout << "[|";
    for (int i = 0; i < n; i++)
        cout << " " << static_cast<int>(l[i]) << " |";
    cout << "]" << endl;
}

// Fonction qui affiche une liste de flottants (array)
void print_array(float l[], int n) {
    cout << "[|";
    for (int i = 0; i < n; i++)
        cout << " " << l[i] << " |";
    cout << "]" << endl;
}

// Fonction qui affiche une liste de chaines de caractères (array)
void print_array(string l[], int n) {
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

// Fonction qui renvoie l'index de la valeur maximale d'une liste d'entiers
int max_index(float* l, int n) {
    int max = -1000000;
    int max_i = 0;

    for (int i = 0; i < n; i++)
        if (l[i] > max) {
            max = l[i];
            max_i = i;
        }

    return max_i;

}

// Fonction qui renvoie l'index de la valeur maximale d'une liste d'entiers
int max_index(uint_fast8_t* l, int n) {
    int max = -UINT8_MAX;
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


// Fonction qui calcule une distance entre deux points
float distance(int i, int j, int x, int y) {
    return (i - x) * (i - x) + (j - y) * (j - y);
}

// Fonction qui calcule la proximité entre deux points (pour l'évaluation de la sécurité du roi)
float proximity(int i, int j, int x, int y, float k) {
    return k / distance(i, j, x, y);
}


// Fonction qui transforme un entier en string (et arrondit s'il est supérieur à 1000)
// TODO : cas négatif à gérer
string int_to_round_string(int k) {
    if (k < 1000)
        return to_string(k);
    if (k < 10000)
        return to_string(static_cast<float>(k) / 1000).substr(0, 3) + "k";
    if (k < 100000)
        return to_string(static_cast<float>(k) / 1000).substr(0, 4) + "k";
    if (k < 1000000)
        return to_string(static_cast<float>(k) / 1000).substr(0, 3) + "k";
    
    return to_string(static_cast<float>(k) / 1000000).substr(0, 3) + "M";
}

// Fonction qui transforme un entier en string (et arrondit s'il est supérieur à 1000)
string long_int_to_round_string(unsigned long long k) {
    if (k < 1000)
        return to_string(k);
    if (k < 10000)
        return to_string(static_cast<float>(k) / 1000).substr(0, 3) + "k";
    if (k < 100000)
        return to_string(static_cast<float>(k) / 1000).substr(0, 4) + "k";
    if (k < 1000000)
        return to_string(static_cast<float>(k) / 1000).substr(0, 3) + "k";
    if (k < 1000000000)
        return to_string(static_cast<float>(k) / 1000000).substr(0, 3) + "M";

    return to_string(static_cast<float>(k) / 1000000000).substr(0, 4) + "G";
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
    return static_cast<int>(x) + (x - static_cast<int>(x) > 0.5);
}


// Fonction qui renvoie si une chaine de caractères est présente dans un tableau de taille n
bool is_in(string s, string string_array[], int n) {
    for (int i = 0; i < n; i++)
        if (s == string_array[i])
            return true;

    return false;
}

// Fonction qui calcule les chances de gain/nulle/perte
float get_winning_chances_from_eval(float eval, bool mate, bool player) {
    if (mate)
        return 1;
    // cout << eval << " : " << 0.5 * (1 + (2 / (1 + exp(-0.4 * (player ? 1 : -1) * eval)) - 1)) << endl;
    return 0.5 * (1 + (2 / (1 + exp(-0.75 * (player ? 1 : -1) * eval / 100)) - 1));
}

// Fonction qui pondère les valeurs de la liste, en fonction d'un taux d'exploration par valeur
void nodes_ponderation(int *l, float *pond, int size) {
    for (int i = 0; i < size; i++) {
        l[i] *= pond[i];
    }
}

// Fonction qui affiche la taille d'un attribut
template <typename T>
void printAttributeSize(const std::string& attributeName, const T& attribute) {
    std::cout << attributeName << "\t" << sizeof(attribute) << " bytes\n";
}

// Fonction qui affiche chaque attribut d'une classe ainsi que sa taille
// TODO : à fix, car ça bug quand on l'appelle
template <typename T>
void printAttributeSizes(const T& obj) {
    cout << "Attribute sizes for class " << typeid(T).name() << ":\n";
    cout << "----------------------------------------\n";
    printAttributeSize("Attribute  ", obj);
    cout << "----------------------------------------\n";
}


template <typename T>
void testFunc(const T& obj) {
}