#include "tests.h"
#include <fstream>
#include <regex>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <robin_map.h>
#include <locale>
#include <iomanip>

using namespace tsl;
using namespace std::literals;

// Constructeur
Tests::Tests(GUI *gui) {
    _gui = gui;
}

// Perft test
bool Tests::perft_test(string fen, int depth, vector<long long int> expected_nodes) {
    // TODO: lier ça à la GUI directement, pour qu'on voit la position et les tests qui avancent
    _gui->load_FEN(fen, false);
    update_GUI();

    // Teste le nombre de noeuds générés
    return _gui->_board->validate_nodes_count_at_depth(fen, depth, expected_nodes, true);
}

// Renvoie une valeur entre 0 et 1, 1 étant la position évaluée correctement
double Tests::evaluation_test(string fen, int expected_evaluation, pair<int, int> evaluation_range, double expected_score, pair<double, double> score_range) {
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
    double evaluation_proximity = max(0.0, 1.0 - pow(eval_diff / (acceptable_eval_range + 1e-9), 2.0) / range_factor);

    // Le score est-il dans la plage attendue?
    bool correct_score = score >= score_range.first && score <= score_range.second;

    // Si il est dans la plage attendue, calcule sa proximité avec la valeur attendue
    double acceptable_score_range = score > expected_score ? score_range.second - expected_score : expected_score - score_range.first;
    double score_diff = abs(score - expected_score);
    double score_proximity = max(0.0, 1.0 - pow(score_diff / (acceptable_score_range + 1e-9), 2.0) / range_factor);

    // Score final
    double score_final = (evaluation_proximity + score_proximity) / 2.0;

    cout << "EVAL: " << score_final << "/1 (" << fen
        << " | Eval: " << evaluation << " (Expected: " << expected_evaluation << " [" << evaluation_range.first << ", " << evaluation_range.second << "]) = " << evaluation_proximity << "/1"
        << " | Score: " << score << " (Expected: " << expected_score << " [" << score_range.first << ", " << score_range.second << "]) = " << score_proximity << "/1"
        << " | Time : " << fixed << setprecision(3) << (double)(end - begin) / CLOCKS_PER_SEC << "s)" << endl;

    return score_final;
}

// Renvoie une valeur entre 0 et 1 (1 = problème résolu)
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

        // Allow user to interrupt a long-running puzzle evaluation
        if (_stop_imported_tests || IsKeyPressed(KEY_ESCAPE)) {
            cout << "Problem test interrupted by user." << endl;
            break;
        }
    }

    // Arrête le chrono
    clock_t end = clock();

    // Récupère le meilleur coup
    Move chosen_move = _gui->_root_exploration_node->get_most_explored_child_move();

    // Récupère le score de ce coup (s'il y en a un)
    double move_score = moves.find(chosen_move) != moves.end() ? moves[chosen_move] : 0.0;

    cout << "PUZZLE: " << move_score << "/1 (" << fen << " | Played: " << _gui->_board->move_label(chosen_move) << " (score: " << move_score << ") - Expected: " << _gui->_board->move_label(moves.begin()->first) << " | Time: " << fixed << setprecision(3) << (double)(end - begin) / CLOCKS_PER_SEC << "s)" << endl;

    return move_score;
}

// Fonction qui fait tous les tests
void Tests::run_all_tests() {
    // Ensure locale so french characters print correctly
    try { std::locale::global(std::locale("")); std::setlocale(LC_ALL, ""); } catch(...) {}

    // TODO: faire en sorte que la GUI reste à jour au fur et à mesure des tests

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

    // 3 *** PROBLEM TESTS ***
    cout << endl << "--------------------------------" << endl;
    cout << endl << "*** PROBLEM TESTS (3s) ***" << endl;

    int problem_tests = 0;
    double problem_tests_score = 0.0;

    // 3.a *** Tactiques classiques ***
    cout << endl << "Tactical problems tests" << endl;

    int tactical_problems = 0;
    double tactical_problems_score = 0.0;

    tactical_problems++, tactical_problems_score += problem_test("3rk2r/ppp2pp1/2p1bq2/2P4p/4P1B1/2P3P1/PP3P1P/RNBQK2R b KQk - 0 12", { { Move(5, 4, 3, 6), 1.0 } }, 3.0);
    tactical_problems++, tactical_problems_score += problem_test("r1b1k2r/pp1nqpp1/4p2p/3pP1N1/8/3BQ3/PP3PPP/2R2RK1 w kq - 0 1", { { Move(2, 4, 6, 0), 1.0 } }, 3.0);

    cout << "Tactical problems results: " << tactical_problems_score << "/" << tactical_problems << endl;

    problem_tests += tactical_problems;
    problem_tests_score += tactical_problems_score;

    // 3.b *** Coups stratégiques forts ***
    cout << endl << "Strong strategic moves tests" << endl;

    int strong_strategic_moves = 0;
    double strong_strategic_moves_score = 0.0;

    strong_strategic_moves++, strong_strategic_moves_score += problem_test("r4rk1/1pqb1ppp/p3pb2/3p4/1P1N4/P1PQB3/6PP/4RRK1 w - - 1 21", { { Move(0, 5, 5, 5), 1.0 } }, 3.0);

    cout << "Strong strategic moves results: " << strong_strategic_moves_score << "/" << strong_strategic_moves << endl;

    problem_tests += strong_strategic_moves;
    problem_tests_score += strong_strategic_moves_score;

    // *** TOTAL SCORE ***
    cout << endl << "*** PROBLEM RESULTS: " << problem_tests_score << "/" << problem_tests << " ***" << endl;

    total_tests += problem_tests;
    total_score += problem_tests_score;

    cout << endl << "--------------------------------" << endl;
    cout << endl << "*** TOTAL SCORE ***" << endl;

    cout << "PERFT: " << perft_tests_passed << "/" << perft_tests_passed + perft_tests_failed << endl;
    cout << "EVALUATIONS: " << evaluation_tests_score << "/" << evaluation_tests << endl;
    cout << "PROBLEMS: " << problem_tests_score << "/" << problem_tests << endl;

    cout << endl << "--------------------------------" << endl;
    cout << "TOTAL SCORE: " << total_score << "/" << total_tests << endl;

    // Run imported tests only if enabled
    if (_imported_tests_enabled) {
        double agg = run_imported_tests("Tests.txt", 2.0);
        cout << "Imported tests aggregate score: " << fixed << setprecision(3) << agg << endl;
    } else {
        cout << "Imported tests disabled." << endl;
    }
}

// Mise à jour de la GUI
void Tests::update_GUI() {
    BeginDrawing();
    _gui->draw();
    EndDrawing();
}

// Run imported tests from a Tests.txt-like file. Returns aggregate score in [0,1].
double Tests::run_imported_tests(const string& tests_path, double time_per_puzzle) {
    // Ensure locale for proper French characters
    try { std::locale::global(std::locale("")); std::setlocale(LC_ALL, ""); } catch(...) {}

    std::ifstream tests_file(tests_path);
    if (!tests_file.is_open()) {
        cout << "Could not open " << tests_path << endl;
        return 0.0;
    }

    vector<string> file_lines;
    string line;
    while (std::getline(tests_file, line)) file_lines.push_back(line);

    auto to_lower = [](string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
        return s;
    };

    auto trim = [](string s) {
        const char* ws = " \t\n\r";
        size_t start = s.find_first_not_of(ws);
        if (start == string::npos) return string();
        size_t end = s.find_last_not_of(ws);
        return s.substr(start, end - start + 1);
    };

    auto is_fen_line = [&](const string &l)->bool {
        std::istringstream iss(l);
        vector<string> toks;
        string t;
        while (iss >> t) toks.push_back(t);
        if (toks.size() < 6) return false;
        if (toks[0].find('/') == string::npos) return false;
        return true;
    };

    std::regex san_regex(R"(([KQRNB]?[a-h]?[1-8]?[x:]?[a-h][1-8](=[QRNB])?[+#]?))");
    std::regex uci_regex(R"(([a-h][1-8][a-h][1-8]))");

    int imported_count = 0;
    int imported_passed = 0;

    for (size_t i = 0; i < file_lines.size(); ++i) {
        if (_stop_imported_tests) { cout << "Imported tests stopped by request." << endl; break; }

        string cur = trim(file_lines[i]);
        if (!is_fen_line(cur)) continue;

        // Build context
        string context;
        for (int k = (int)i - 6; k < (int)i; ++k) if (k >= 0) context += file_lines[k] + " ";
        string ctx = to_lower(context + " " + cur);

        string category = "GENERAL";
        if (ctx.find("quiesc") != string::npos || ctx.find("quiescence") != string::npos) category = "QUIESCENCE";
        else if (ctx.find("perft") != string::npos) category = "PERFT";
        else if (ctx.find("repet") != string::npos || ctx.find("repeat") != string::npos) category = "REPETITION";
        else if (ctx.find("mate") != string::npos || ctx.find("#") != string::npos) category = "MATE";
        else if (ctx.find("puzzle") != string::npos || ctx.find("tact") != string::npos) category = "PUZZLE";
        else if (ctx.find("eval") != string::npos || ctx.find("evalue") != string::npos) category = "EVAL";

        cout << "[IMPORTED] [" << setw(4) << imported_count+1 << "/" << file_lines.size() << "] " << cur << " -> category: " << category << endl;

        // Load position
        _gui->load_FEN(cur, false);
        update_GUI();

        // Extract expected move
        string expected_move_str;
        std::smatch m;
        if (std::regex_search(cur, m, san_regex)) expected_move_str = m.str(1);
        else if (i + 1 < file_lines.size() && std::regex_search(file_lines[i+1], m, san_regex)) expected_move_str = m.str(1);
        else if (i > 0 && std::regex_search(file_lines[i-1], m, san_regex)) expected_move_str = m.str(1);
        else if (std::regex_search(cur, m, uci_regex)) expected_move_str = m.str(1);

        expected_move_str = trim(expected_move_str);
        expected_move_str.erase(remove(expected_move_str.begin(), expected_move_str.end(), '!'), expected_move_str.end());
        expected_move_str.erase(remove(expected_move_str.begin(), expected_move_str.end(), '?'), expected_move_str.end());

        int expected_eval_sign = 0;
        if (ctx.find("gagn") != string::npos || ctx.find("winning") != string::npos || ctx.find("gagne") != string::npos) expected_eval_sign = 1;
        if (ctx.find("noir") != string::npos || ctx.find("black") != string::npos || ctx.find("perd") != string::npos) expected_eval_sign = -1;
        if (ctx.find("nulle") != string::npos || ctx.find("draw") != string::npos) expected_eval_sign = 0;

        double test_score = 0.0;
        double test_time = 0.0;

        if (!expected_move_str.empty()) {
            try {
                Move expected_move = _gui->_board->move_from_algebric_notation(expected_move_str);
                robin_map<Move,double> moves_map; moves_map[expected_move] = 1.0;
                clock_t t0 = clock();
                double res = problem_test(cur, moves_map, time_per_puzzle);
                clock_t t1 = clock();
                test_time = (double)(t1 - t0) / CLOCKS_PER_SEC;
                test_score = res;
                cout << "  Expected move: " << expected_move_str << " | Score: " << fixed << setprecision(3) << test_score << " | Time: " << test_time << "s" << endl;
            } catch (...) {
                cout << "  WARN: could not parse expected move '" << expected_move_str << "'" << endl;
            }
        }

        if (expected_move_str.empty()) {
            int expected_eval = 0; pair<int,int> eval_range = { -1000, 1000 };
            double expected_score = 0.5; pair<double,double> score_range = { 0.0, 1.0 };
            if (expected_eval_sign > 0) { expected_eval = 300; eval_range = {100,2000}; expected_score = 0.75; score_range = {0.6,1.0}; }
            else if (expected_eval_sign < 0) { expected_eval = -300; eval_range = {-2000,-100}; expected_score = 0.25; score_range = {0.0,0.4}; }
            else { expected_eval = 0; eval_range = {-150,150}; expected_score = 0.5; score_range = {0.3,0.7}; }
            clock_t t0 = clock();
            double sc = evaluation_test(cur, expected_eval, eval_range, expected_score, score_range);
            clock_t t1 = clock();
            test_time = (double)(t1 - t0) / CLOCKS_PER_SEC;
            test_score = sc;
            cout << "  Expected eval_sign: " << expected_eval_sign << " | Eval score: " << fixed << setprecision(3) << test_score << " | Time: " << test_time << "s" << endl;
        }

        imported_count++;
        if (test_score > 0.5) imported_passed++;

        // allow user to interrupt via ESC or flag
        if (IsKeyPressed(KEY_ESCAPE) || _stop_imported_tests) {
            cout << "Stopping imported tests on user request." << endl;
            break;
        }
    }

    cout << "Imported tests processed: " << imported_count << ", passed(>0.5): " << imported_passed << endl;
    double aggregate = imported_count == 0 ? 0.0 : (double)imported_passed / (double)imported_count;
    return aggregate;
}