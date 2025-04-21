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
// Vitesse d'�valuation


// TODO ***
// Faire des fonctions pour chaque type de test: run_all_problems, etc...
// Faire des exercices par THEME, pour voir sur quel th�me il p�che (probl�me strat�gique � th�me, �valuation d'un param�tre sp�cifique, etc...)
// V�rifier sur des �changes de pi�ces, si elles sont toutes �valu�es correctement selon les positions

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

	// Constructeur
	//Tests(Evaluator* eval);

	Tests(GUI *gui);


	// Fonctions

	// Perft test
	bool perft_test(string fen, int depth, vector<int> expected_nodes);

	// Renvoie une valeur entre 0 et 1, 1 �tant la position �valu�e correctement
	double evaluation_test(string fen, int expected_evaluation, pair<int, int> evaluation_range, double expected_score, pair<double, double> score_range);

	// Renvoie une valeur entre 0 et 1 (1 = probl�me r�solu) (faut-il prendre en compte si le coup jou� est quand-m�me bon?)
	double problem_test(string fen, map<Move, double> moves, double time);

	// Mise � jour de la GUI
	void update_GUI();


	// Fonction qui fait tous les tests
	void run_all_tests();
};