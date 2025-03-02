#pragma once

// Framework de tests

// Choses � tester:

// Perft test, sur diff�rentes positions (avec affichage de la vitesse de g�n�ration de coups)
// Tests d'�valuations sur diff�rentes positions: score bas� sur la proximit� avec l'�valuation r�elle, et la fr�quence de la position (affichage de la vitesse d'�valuation)
// Probl�mes (tests avec 1 minutes, 3 minutes et 10 minutes): lesquels sont bons (ordre croissant des probl�mes)
// - Probl�mes tactique
// - Coups d'ouvertures
// - Probl�mes de finales
// - Coups strat�giques forts
// - Coups de d�fense
// Vitesse de jeu?
// Evaluation des chances de gain, et prises de risques sur les positions incertaines
// Tests de recherche de mat (avec affichage de la vitesse de recherche)
// Sym�trie de l'�valuation
// Coups rat�s
// Coups d'instinct
// Vitesse de jeu


// Impl�mentation d'un score par crit�re
// Score global repr�sentant la puissance de la configuration (g�n�ration de coups, �valuation, algorithme, param�tres de recherche...)
