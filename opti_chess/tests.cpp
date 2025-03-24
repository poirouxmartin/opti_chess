#include "tests.h"

// Constructeur
//Tests::Tests(Evaluator* eval) {
//	_eval = eval;
//	_board = new Board();
//}


Tests::Tests(GUI *gui) {
	_gui = gui;
}

// Perft test
bool Tests::perft_test(string fen, int depth, vector<int> expected_nodes) {

	// TODO *** lier ça à la GUI directement, pour qu'on voit la position et les tests qui avancent

	// Teste le nombre de noeuds générés
	bool passed = _gui->_board.validate_nodes_count_at_depth(fen, depth, expected_nodes, true);
	perft_tests++;
	perft_tests_passed += passed;

	return passed;
}

// Renvoie une valeur entre 0 et 1, 1 étant la position évaluée correctement
double Tests::evaluation_test(string fen, int evaluation, double win_rate) {

	// TODO *** rajouter une plage d'acceptance pour l'évaluation et le win_rate (et créer le score linéairement)

	// Met la position
	_gui->load_FEN(fen, false);

	// Lance le chrono
	clock_t begin = clock();

	// Evalue la position
	_gui->_board.evaluate(_gui->_grogros_eval, false, nullptr, true);

	// Evalue le win rate
	double score = _gui->_board.get_average_score();

	// Arrête le chrono
	clock_t end = clock();

	cout << "Position: " << fen << " | Evaluation : " << _gui->_board._evaluation << " / Expected: " << evaluation << ", WR: " << win_rate << ", Score: " << score << ", Time: " << (double)(end - begin) / CLOCKS_PER_SEC << endl;

	// TODO *** évaluer la précision de l'évaluation

	return 1.0;
}

// Renvoie une valeur entre 0 et 1 (1 = problème résolu) (faut-il prendre en compte si le coup joué est quand-même bon?)
// TODO *** faire un panel de coups accepptés avec une valeur pour chacun
bool Tests::problem_test(string fen, map<Move, double> moves, double time) {

	// Met la position
	_gui->load_FEN(fen, false);

	// Lance le chrono
	clock_t begin = clock();

	// Lance GrogrosZero
	while ((double)(clock() - begin) / CLOCKS_PER_SEC < time) {
		_gui->grogros_analysis();
	}

	// Arrête le chrono
	clock_t end = clock();

	// Récupère le meilleur coup
	Move chosen_move = _gui->_root_exploration_node->get_most_explored_child_move();

	// Récupère le score de ce coup (s'il y en a un)
	double move_score = moves.find(chosen_move) != moves.end() ? moves[chosen_move] : 0.0;

	cout << "Position: " << fen << " | Played: " << _gui->_board.move_label(chosen_move) << " (" << move_score << ") / Expected: " << _gui->_board.move_label(moves.begin()->first) << ", Time: " << (double)(end - begin) / CLOCKS_PER_SEC << endl;

	return move_score == 1.0;
}



// Fonction qui fait tous les tests
void Tests::run_all_tests() {
	// TODO *** faire en sorte que la GUI reste à jour au fur et à mesure des tests

	// 1. Perft test
	cout << endl << "*** PERFT TESTS ***" << endl;

	perft_test("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6, { 1, 20, 400, 8902, 197281, 4865609, 119060324 });
	perft_test("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 6, { 1, 48, 2039, 97862, 4085603, 193690690 });

	// 2. Evaluation test
	cout << endl << "*** EVALUATION TESTS ***" << endl;

	evaluation_test("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 50, 0.55);
	evaluation_test("5rk1/r3npbp/2p2np1/2N1p3/2B1P1P1/1P2BP2/b1P4P/2KR2NR b - - 2 19", 500, 0.90);

	// 3. Problem test
	cout << endl << "*** PROBLEM TESTS ***" << endl;

	// Ici, il faut résoudre le problème en moins de 1 seconde
	problem_test("3rk2r/ppp2pp1/2p1bq2/2P4p/4P1B1/2P3P1/PP3P1P/RNBQK2R b KQk - 0 12", { { Move(5, 4, 3, 6), 1.0 } }, 1.0); // Fg4 seul coup qui gagne



	// TODO: situation réelle:
	// Résoudre le plus de problèmes en 1 minute, 3 minutes et 10 minutes

}


