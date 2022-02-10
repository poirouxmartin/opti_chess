#include "useful_functions.h"

// Fonction qui renvoie si un entier appartient à un intervalle
bool is_in(int x, int min, int max) {
    return (x >= min && x <= max);
}