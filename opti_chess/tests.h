#pragma once
#include "board.h"
#include "gui.h"

// Framework de tests

// Choses � tester:

// Perft test, sur diff�rentes positions (avec affichage de la vitesse de g�n�ration de coups)
// Tests d'�valuations sur diff�rentes positions: score bas� sur la proximit� avec l'�valuation r�elle, et la fr�quence de la position *** TODO (affichage de la vitesse d'�valuation)
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
// Tests de quiescence


// Impl�mentation d'un score par crit�re
// Score global repr�sentant la puissance de la configuration (g�n�ration de coups, �valuation, algorithme, param�tres de recherche...)

class Tests {
public:

	// Attributs

	// Plateau
	//Board *_board;

	// Evaluation
	//Evaluator *_eval;

	// Algorithme

	// Param�tres de recherche

	// GUI? (pour importer tous les param�tres et tester direct)
	GUI *_gui;

	// Valeurs des tests

	// Perft
	int perft_tests = 0;
	int perft_tests_passed = 0;

	// Evaluation
	int evaluation_tests = 0;
	double evaluation_tests_score = 0.0;

	// Probl�mes
	int problem_tests = 0;
	double problem_tests_score = 0.0;


	// Constructeur
	//Tests(Evaluator* eval);

	Tests(GUI *gui);


	// Fonctions

	// Perft test
	bool perft_test(string fen, int depth, vector<int> expected_nodes);

	// Renvoie une valeur entre 0 et 1, 1 �tant la position �valu�e correctement
	double evaluation_test(string fen, int evaluation, double win_rate);

	// Renvoie une valeur entre 0 et 1 (1 = probl�me r�solu) (faut-il prendre en compte si le coup jou� est quand-m�me bon?)
	bool problem_test(string fen, map<Move, double> moves, double time);


	// Fonction qui fait tous les tests
	void run_all_tests();
};