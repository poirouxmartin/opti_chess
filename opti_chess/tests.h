#pragma once
#include "board.h"
#include "gui.h"

// Framework de tests

// Choses à tester:

// Perft test, sur différentes positions (avec affichage de la vitesse de génération de coups)
// Tests d'évaluations sur différentes positions: score basé sur la proximité avec l'évaluation réelle, et la fréquence de la position *** TODO (affichage de la vitesse d'évaluation)
// Problèmes (tests avec 1 minutes, 3 minutes et 10 minutes): lesquels sont bons (ordre croissant des problèmes)
// - Problèmes tactique
// - Coups d'ouvertures
// - Problèmes de finales
// - Coups stratégiques forts
// - Coups de défense
// Vitesse de jeu?
// Evaluation des chances de gain, et prises de risques sur les positions incertaines
// Tests de recherche de mat (avec affichage de la vitesse de recherche)
// Symétrie de l'évaluation
// Coups ratés
// Coups d'instinct
// Vitesse de jeu
// Tests de quiescence


// Implémentation d'un score par critère
// Score global représentant la puissance de la configuration (génération de coups, évaluation, algorithme, paramètres de recherche...)

class Tests {
public:

	// Attributs

	// Plateau
	//Board *_board;

	// Evaluation
	//Evaluator *_eval;

	// Algorithme

	// Paramètres de recherche

	// GUI? (pour importer tous les paramètres et tester direct)
	GUI *_gui;

	// Valeurs des tests

	// Perft
	int perft_tests = 0;
	int perft_tests_passed = 0;

	// Evaluation
	int evaluation_tests = 0;
	double evaluation_tests_score = 0.0;

	// Problèmes
	int problem_tests = 0;
	double problem_tests_score = 0.0;


	// Constructeur
	//Tests(Evaluator* eval);

	Tests(GUI *gui);


	// Fonctions

	// Perft test
	bool perft_test(string fen, int depth, vector<int> expected_nodes);

	// Renvoie une valeur entre 0 et 1, 1 étant la position évaluée correctement
	double evaluation_test(string fen, int evaluation, double win_rate);

	// Renvoie une valeur entre 0 et 1 (1 = problème résolu) (faut-il prendre en compte si le coup joué est quand-même bon?)
	bool problem_test(string fen, map<Move, double> moves, double time);


	// Fonction qui fait tous les tests
	void run_all_tests();
};