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
bool Tests::perft_test(string fen, int depth, vector<long long int> expected_nodes) {

	// TODO *** lier ça à la GUI directement, pour qu'on voit la position et les tests qui avancent
	_gui->load_FEN(fen, false);
	update_GUI();

	// Teste le nombre de noeuds générés
	return _gui->_board->validate_nodes_count_at_depth(fen, depth, expected_nodes, true);
}

// Renvoie une valeur entre 0 et 1, 1 étant la position évaluée correctement
double Tests::evaluation_test(string fen, int expected_evaluation, pair<int, int> evaluation_range, double expected_score, pair<double, double> score_range) {

	// TODO *** rajouter une plage d'acceptance pour l'évaluation et le win_rate (et créer le score linéairement)

	// Met la position
	_gui->load_FEN(fen, false);
	update_GUI();

	// Lance le chrono
	clock_t begin = clock();

	// Evalue la position
	_gui->evaluate_position(false);
	int evaluation = _gui->_root_exploration_node->_static_evaluation._value;

	// Evalue le win rate
	double score = _gui->_root_exploration_node->_static_evaluation._avg_score;

	// Arrête le chrono
	clock_t end = clock();

	constexpr double range_factor = 2.0;

	// L'évaluation est-elle dans la plage attendue?
	bool correct_evaluation = evaluation >= evaluation_range.first && evaluation <= evaluation_range.second;

	// Si elle est dans la plage attendue, calcule sa proximité avec la valeur attendue
	double acceptable_eval_range = evaluation > expected_evaluation ? evaluation_range.second - expected_evaluation : expected_evaluation - evaluation_range.first;
	double eval_diff = abs(evaluation - expected_evaluation);
	double evaluation_proximity = max(0.0, 1.0 - pow(eval_diff / acceptable_eval_range, 2.0) / range_factor);

	// Le score est-il dans la plage attendue?
	bool correct_score = score >= score_range.first && score <= score_range.second;

	// Si il est dans la plage attendue, calcule sa proximité avec la valeur attendue
	double acceptable_score_range = score > expected_score ? score_range.second - expected_score : expected_score - score_range.first;
	double score_diff = abs(score - expected_score);
	double score_proximity = max(0.0, 1.0 - pow(score_diff / acceptable_score_range, 2.0) / range_factor);

	// Score final
	double score_final = (evaluation_proximity + score_proximity) / 2.0;

	cout << "EVAL: " << score_final << "/1 (" << fen << 
		" | Eval: " << evaluation << " (Expected: " << expected_evaluation << " [" << evaluation_range.first << ", " << evaluation_range.second << "]) = " << evaluation_proximity << "/1" <<
		" | Score: " << score << " (Expected: " << expected_score << " [" << score_range.first << ", " << score_range.second << "]) = " << score_proximity << "/1" <<
		" | Time : " << (double)(end - begin) / CLOCKS_PER_SEC << ")" << endl;

	return score_final;
}

// Renvoie une valeur entre 0 et 1 (1 = problème résolu) (faut-il prendre en compte si le coup joué est quand-même bon?)
// TODO *** faire un panel de coups accepptés avec une valeur pour chacun
double Tests::problem_test(string fen, robin_map<Move, double> moves, double time) {

	// Met la position
	_gui->load_FEN(fen, false);
	update_GUI();

	// Lance le chrono
	clock_t begin = clock();

	// Lance GrogrosZero
	while ((double)(clock() - begin) / CLOCKS_PER_SEC < time) {
		_gui->grogros_analysis();
		update_GUI();
	}


	// Arrête le chrono
	clock_t end = clock();

	// Récupère le meilleur coup
	Move chosen_move = _gui->_root_exploration_node->get_most_explored_child_move();

	// Récupère le score de ce coup (s'il y en a un)
	double move_score = moves.find(chosen_move) != moves.end() ? moves[chosen_move] : 0.0;

	cout << "PUZZLE: " << move_score << "/1 (" << fen << " | Played: " << _gui->_board->move_label(chosen_move) << " (" << move_score << "/1) - Expected: " << _gui->_board->move_label(moves.begin()->first) << " | Time: " << (double)(end - begin) / CLOCKS_PER_SEC << ")" << endl;

	return move_score;
}



// Fonction qui fait tous les tests
void Tests::run_all_tests() {
	// TODO *** faire en sorte que la GUI reste à jour au fur et à mesure des tests

	int total_tests = 0;
	double total_score = 0.0;

	// 1 *** PERFT TESTS ***
	cout << endl << "--------------------------------" << endl;
	cout << endl << "*** PERFT TESTS ***" << endl;

	int perft_tests_failed = 0;
	int perft_tests_passed = 0;

	perft_test("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, { 1, 20, 400, 8902, 197281, 4865609, 119060324 }) ? perft_tests_passed++ : perft_tests_failed++;
	perft_test("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5, { 1, 48, 2039, 97862, 4085603, 193690690 }) ? perft_tests_passed++ : perft_tests_failed++;

	// *** PERFT RESULTS ***
	cout << endl << "*** PERFT RESULTS: " << perft_tests_passed << "/" << perft_tests_passed + perft_tests_failed << " ***" << endl;

	total_tests += perft_tests_passed + perft_tests_failed;
	total_score += perft_tests_passed;

	// 2 *** EVALUATION TESTS ***
	cout << endl << "--------------------------------" << endl;
	cout << endl << "*** EVALUATION TESTS ***" << endl;

	int evaluation_tests = 0;
	double evaluation_tests_score = 0.0;

	// 2.a *** Positions usuelles ***

	cout << endl << "Usual positions evaluation tests" << endl;

	int usual_positions = 0;
	double usual_positions_score = 0.0;

	usual_positions++, usual_positions_score += evaluation_test("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 40, { 0, 70 }, 0.55, { 0.5, 0.6 }); // Position initiale

	cout << "Usual positions evaluation results: " << usual_positions_score << "/" << usual_positions << endl;

	evaluation_tests += usual_positions;
	evaluation_tests_score += usual_positions_score;

	// TODO ***
	// Positions de sicilienne classique, ruilopez, etc...

	// 2.b *** Pièces enfermées (ou non) ***

	cout << endl << "Trapped pieces evaluation tests" << endl;

	int trapped_pieces = 0;
	double trapped_pieces_score = 0.0;

	trapped_pieces++, trapped_pieces_score += evaluation_test("5rk1/r3npbp/2p2np1/2N1p3/2B1P1P1/1P2BP2/b1P4P/2KR2NR b - - 2 19", 400, { 300, 600 }, 0.9, { 0.85, 0.95 }); // Fou enfermé en a2

	cout << "Trapped pieces evaluation results: " << trapped_pieces_score << "/" << trapped_pieces << endl;

	evaluation_tests += trapped_pieces;
	evaluation_tests_score += trapped_pieces_score;

	// 2.c *** Sécurité du roi ***

	cout << endl << "King safety evaluation tests" << endl;

	int king_safety = 0;
	double king_safety_score = 0.0;

	king_safety++, king_safety_score += evaluation_test("8/pppbn2r/3p4/4k1p1/1P2P3/P1P1RP2/6P1/3R2K1 b - - 1 27", -250, { -400, -150 }, 0.1, { 0.05, 0.2 }); // Le roi noir est en fait safe

	cout << "King safety evaluation results: " << king_safety_score << "/" << king_safety << endl;

	evaluation_tests += king_safety;
	evaluation_tests_score += king_safety_score;

	// 2.d *** Cases faibles ***

	cout << endl << "Weak squares evaluation tests" << endl;

	int weak_squares = 0;
	double weak_squares_score = 0.0;

	weak_squares++, weak_squares_score += evaluation_test("r1bqkb1r/1p3pp1/p1np3p/3Np3/4P3/N7/PPP2PPP/R2QKB1R w KQkq - 2 11", 120, { 90, 150 }, 0.7, { 0.65, 0.75 }); // Gros trou en d5

	cout << "Weak squares evaluation results: " << weak_squares_score << "/" << weak_squares << endl;

	evaluation_tests += weak_squares;
	evaluation_tests_score += weak_squares_score;

	// 2.z *** Autres ***

	cout << endl << "Other evaluation tests" << endl;

	int others = 0;
	double others_score = 0.0;

	others++, others_score += evaluation_test("r2qrbk1/5ppp/pn3n2/4N3/1ppP1P2/4PQ2/PB2N1PP/2R2RK1 b - - 1 20", -350, { -500, -200 }, 0.08, { 0.03, 0.15 }); // Complètement gagnant pour les noirs

	cout << "Other evaluation results: " << others_score << "/" << others << endl;

	evaluation_tests += others;
	evaluation_tests_score += others_score;

	// TODO ***
	// Ouvertures
	// Endgame
	// King safety
	// ...

	// *** EVALUATION RESULTS ***
	cout << endl << "*** EVALUATION RESULTS: " << evaluation_tests_score << "/" << evaluation_tests << " ***" << endl;

	total_tests += evaluation_tests;
	total_score += evaluation_tests_score;

	// 3 *** PROBLEM TESTS ***
	cout << endl << "--------------------------------" << endl;
	cout << endl << "*** PROBLEM TESTS (3s) ***" << endl;

	int problem_tests = 0;
	double problem_tests_score = 0.0;

	// TODO *** thème pour les puzzles

	// 3.a *** Tactiques classiques ***

	cout << endl << "Tactical problems tests" << endl;

	int tactical_problems = 0;
	double tactical_problems_score = 0.0;

	tactical_problems++, tactical_problems_score += problem_test("3rk2r/ppp2pp1/2p1bq2/2P4p/4P1B1/2P3P1/PP3P1P/RNBQK2R b KQk - 0 12", { { Move(5, 4, 3, 6), 1.0 } }, 3.0); // Fg4 seul coup qui gagne, tous les autres perdent
	tactical_problems++, tactical_problems_score += problem_test("r1b1k2r/pp1nqpp1/4p2p/3pP1N1/8/3BQ3/PP3PPP/2R2RK1 w kq - 0 1", { { Move(2, 4, 6, 0), 1.0 } }, 3.0); // Dxa7!! résulte en une position complètement gagnante


	cout << "Tactical problems results: " << tactical_problems_score << "/" << tactical_problems << endl;

	problem_tests += tactical_problems;
	problem_tests_score += tactical_problems_score;

	// 3.b *** Coups stratégiques forts ***

	cout << endl << "Strong strategic moves tests" << endl;

	int strong_strategic_moves = 0;
	double strong_strategic_moves_score = 0.0;

	strong_strategic_moves++, strong_strategic_moves_score += problem_test("r4rk1/1pqb1ppp/p3pb2/3p4/1P1N4/P1PQB3/6PP/4RRK1 w - - 1 21", { { Move(0, 5, 5, 5), 1.0 } }, 3.0); //  Txf6 seul coup qui tente quelque chose: il apporte de l'incertitude dans un position foutue stratégiquement, en ouvrant le roi adverse

	cout << "Strong strategic moves results: " << strong_strategic_moves_score << "/" << strong_strategic_moves << endl;

	problem_tests += strong_strategic_moves;
	problem_tests_score += strong_strategic_moves_score;

	// TODO *** problèmes à plusieurs coups?

	// Tous les puzzles qui sont ratés, c'est eux qu'on refait avec plus de temps

	// TODO: situation réelle:
	// Résoudre le plus de problèmes en 1 minute, 3 minutes et 10 minutes

	// *** PROBLEM RESULTS ***
	cout << endl << "*** PROBLEM RESULTS: " << problem_tests_score << "/" << problem_tests << " ***" << endl;

	total_tests += problem_tests;
	total_score += problem_tests_score;


	// 4 *** Total score ***
	cout << endl << "--------------------------------" << endl;
	cout << endl << "*** TOTAL SCORE ***" << endl;

	cout << "PERFT: " << perft_tests_passed << "/" << perft_tests_passed + perft_tests_failed << endl;
	cout << "EVALUATIONS: " << evaluation_tests_score << "/" << evaluation_tests << endl;
	cout << "PROBLEMS: " << problem_tests_score << "/" << problem_tests << endl;

	cout << endl << "--------------------------------" << endl;
	cout << "TOTAL SCORE: " << total_score << "/" << total_tests << endl;


}

// Mise à jour de la GUI
void Tests::update_GUI() {
	BeginDrawing();
	_gui->draw();
	EndDrawing();
}