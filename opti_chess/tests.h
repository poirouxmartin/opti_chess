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
// Vitesse d'évaluation


// TODO ***
// Faire des fonctions pour chaque type de test: run_all_problems, etc...
// Faire des exercices par THEME, pour voir sur quel thème il pêche (problème stratégique à théme, évaluation d'un paramètre spécifique, etc...)
// Vérifier sur des échanges de pièces, si elles sont toutes évaluées correctement selon les positions

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

	// Constructeur
	//Tests(Evaluator* eval);

	Tests(GUI *gui);


	// Fonctions

	// Perft test
	bool perft_test(string fen, int depth, vector<int> expected_nodes);

	// Renvoie une valeur entre 0 et 1, 1 étant la position évaluée correctement
	double evaluation_test(string fen, int expected_evaluation, pair<int, int> evaluation_range, double expected_score, pair<double, double> score_range);

	// Renvoie une valeur entre 0 et 1 (1 = problème résolu) (faut-il prendre en compte si le coup joué est quand-même bon?)
	double problem_test(string fen, map<Move, double> moves, double time);

	// Mise à jour de la GUI
	void update_GUI();


	// Fonction qui fait tous les tests
	void run_all_tests();
};