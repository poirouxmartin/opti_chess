#include <clocale>

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

// Helper: try to ensure console outputs UTF-8 so French accents display correctly
static void ensure_utf8_output() {
    try {
        // Attempt to set global locale to user's environment (UTF-8 if available)
        try {
            std::locale loc("");
            std::locale::global(loc);
            std::cout.imbue(loc);
            std::cerr.imbue(loc);
        } catch (...) {
            // fallback: try to use common UTF-8 locales
            try { std::locale loc("en_US.UTF-8"); std::locale::global(loc); std::cout.imbue(loc); std::cerr.imbue(loc); } catch(...) {}
            try { std::locale loc("C.UTF-8"); std::locale::global(loc); std::cout.imbue(loc); std::cerr.imbue(loc); } catch(...) {}
        }
        std::setlocale(LC_ALL, "");
    } catch (...) { }
}

// Extract the canonical FEN (first 6 whitespace-separated tokens) from a line that may include comments
static string extract_fen_from_line(const string &line) {
    // Look for a FEN anywhere in the line using a regex that matches the
    // canonical 6-field FEN: piece-placement (8 slash-separated ranks),
    // active color, castling, en-passant, halfmove clock and fullmove number.
    // This is more robust than taking the first 6 whitespace tokens.
    static const std::regex fen_regex(R"((?:[rnbqkpRNBQKP1-8]+(?:\/[rnbqkpRNBQKP1-8]+){7})\s+[wb]\s+(?:-|[KQkq]{1,4})\s+(?:-|[a-h][1-8])\s+\d+\s+\d+)");
    std::smatch m;
    if (std::regex_search(line, m, fen_regex)) {
        return m.str(0);
    }
    return string();
}

// Trim helper
static string trim_copy(string s) {
    const char* ws = " \t\n\r";
    size_t start = s.find_first_not_of(ws);
    if (start == string::npos) return string();
    size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

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

    // Nicely formatted multi-line output to avoid long single lines and duplicates
    cout << "EVAL: " << fixed << setprecision(3) << score_final << "/1" << " | Time: " << fixed << setprecision(3) << (double)(end - begin) / CLOCKS_PER_SEC << "s" << endl;
    cout << "  FEN: " << fen << endl;
    cout << "  Eval: " << evaluation << " (Expected: " << expected_evaluation << " [" << evaluation_range.first << ", " << evaluation_range.second << "]) = " << fixed << setprecision(3) << evaluation_proximity << "/1"
         << " | Score: " << fixed << setprecision(3) << score << " (Expected: " << expected_score << " [" << score_range.first << ", " << score_range.second << "]) = " << fixed << setprecision(3) << score_proximity << "/1" << endl;

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

    cout << "PUZZLE: " << fixed << setprecision(3) << move_score << "/1" << endl;
    cout << "  FEN: " << fen << endl;
    cout << "  Played: " << _gui->_board->move_label(chosen_move) << " (score: " << fixed << setprecision(3) << move_score << ") - Expected: " << _gui->_board->move_label(moves.begin()->first) << " | Time: " << fixed << setprecision(3) << (double)(end - begin) / CLOCKS_PER_SEC << "s" << endl;

    return move_score;
}

// Fonction qui fait tous les tests
void Tests::run_all_tests() {
    // Ensure console outputs use UTF-8/locale so french characters print correctly
    ensure_utf8_output();

    // TODO: faire en sorte que la GUI reste à jour au fur et a mesure des tests

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
        double agg = run_imported_tests("Tests.txt", 3.0, total_tests, total_score);
        cout << "Imported tests aggregate score: " << fixed << setprecision(3) << agg << endl;
        // update overall totals
        // run_imported_tests updates file markings and prints progress; it also returns aggregate; we also update total_tests/score using internal markers written back
    }
    else {
        cout << "Imported tests disabled." << endl;
    }

    // Generate evaluation-only tests from Tests.txt (adds conservative evaluation_test calls)
    int added = add_generated_evaluation_tests("Tests.txt");
    cout << "Generated evaluation tests added: " << added << endl;
}

// Mise à jour de la GUI
void Tests::update_GUI() {
    BeginDrawing();
    _gui->draw();
    EndDrawing();
}

// Run imported tests from a Tests.txt-like file. Returns aggregate score in [0,1].
// base_total_tests/base_total_score allow displaying combined overall score while running.
double Tests::run_imported_tests(const string& tests_path, double time_per_puzzle, int base_total_tests, double base_total_score) {
    // Ensure locale for proper French characters
    ensure_utf8_output();

    std::ifstream tests_file(tests_path);
    if (!tests_file.is_open()) {
        cout << "Could not open " << tests_path << endl;
        return 0.0;
    }

    vector<string> file_lines;
    string rawline;
    while (std::getline(tests_file, rawline)) file_lines.push_back(rawline);

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
        string fen = extract_fen_from_line(l);
        return !fen.empty();
    };

    // count total FENs for progress
    int total_fens = 0;
    for (auto &ln : file_lines) if (is_fen_line(trim(ln))) total_fens++;

    std::regex san_regex(R"(([KQRNBTDCF]?[a-h]?[1-8]?[x:]?[a-h][1-8](=[KQRNBTDCF])?[+#]?))");
    std::regex uci_regex(R"(([a-h][1-8][a-h][1-8]))");

    int imported_count = 0;
    int imported_passed = 0;
    vector<char> tested_flag(file_lines.size(), 0);
    int processed = 0;

    // helper to detect expected eval sign more robustly
    auto detect_eval_sign = [&](const string &ctx)->int {
        if (ctx.find("pour les noirs") != string::npos) return -1;
        if (ctx.find("noirs") != string::npos && (ctx.find("gagn") != string::npos || ctx.find("mieux") != string::npos)) return -1;
        if (ctx.find("black") != string::npos && (ctx.find("winning") != string::npos || ctx.find("better") != string::npos)) return -1;
        if (ctx.find("gagn") != string::npos || ctx.find("gagne") != string::npos || ctx.find("winning") != string::npos) return 1;
        if (ctx.find("nulle") != string::npos || ctx.find("draw") != string::npos) return 0;
        return 0;
    };

    static const std::regex tag_regex(R"(^\s*\[([A-Za-z]+)\]\s*(.*))");
    for (size_t i = 0; i < file_lines.size(); ++i) {
        if (_stop_imported_tests) { cout << "Imported tests stopped by request." << endl; break; }

        string raw = file_lines[i];
        string full = trim_copy(raw);

        // Detect optional tag prefix like [EVALUATION], [PUZZLE], [PERFT], etc.
        string tag;
        std::smatch tagm;
        string body = full;
        if (std::regex_match(full, tagm, tag_regex)) {
            tag = tagm.str(1);
            body = trim_copy(tagm.str(2));
        }

        if (!is_fen_line(body)) continue;

        string fen = extract_fen_from_line(body);
        if (fen.empty()) continue;

        processed++;
        // Build context (previous 6 lines and next 2 for clues) using raw file lines
        string context;
        for (int k = (int)i - 6; k <= (int)i + 2; ++k) if (k >= 0 && k < (int)file_lines.size()) context += file_lines[k] + " ";
        string ctx = to_lower(context + " " + body);

        // Determine category default
        string category = "GENERAL";
        if (ctx.find("quiesc") != string::npos || ctx.find("quiescence") != string::npos) category = "QUIESCENCE";
        else if (ctx.find("perft") != string::npos) category = "PERFT";
        else if (ctx.find("repet") != string::npos || ctx.find("repeat") != string::npos) category = "REPETITION";
        else if (ctx.find("mate") != string::npos || ctx.find("#") != string::npos) category = "MATE";
        else if (ctx.find("puzzle") != string::npos || ctx.find("tact") != string::npos) category = "PUZZLE";
        else if (ctx.find("eval") != string::npos || ctx.find("evalue") != string::npos) category = "EVAL";

        cout << "[IMPORTED] [" << setw(4) << processed << "/" << total_fens << "] " << fen << (tag.empty()?"":" [" + tag + "]") << endl;
        if (full.size() > fen.size() + 1) {
            string comment = trim_copy(full.substr(full.find(fen) + fen.size()));
            if (!comment.empty()) cout << "  Comment: " << (comment.size() > 120 ? comment.substr(0,120) + "..." : comment) << endl;
        }


        // Load position
        _gui->load_FEN(fen, false);
        update_GUI();

        int expected_eval_sign = detect_eval_sign(ctx);

        double test_score = 0.0;
        double test_time = 0.0;

        // If tag explicitly requests PUZZLE, try to parse a UCI move from context and run problem_test
        bool handled = false;
        if (!tag.empty()) {
            string utag = tag;
            for (auto &c : utag) c = (char)toupper((unsigned char)c);

            if (utag == "PUZZLE") {
                // try to find a UCI move in the surrounding context (excluding the FEN)
                string ctx_no_fen = ctx;
                size_t fen_pos = ctx_no_fen.find(fen);
                if (fen_pos != string::npos) ctx_no_fen.erase(fen_pos, fen.size());
                std::smatch m2;
                if (std::regex_search(ctx_no_fen, m2, uci_regex)) {
                    string uci = m2.str(0);
                    // parse uci like e2e4 -> Move
                    auto parse_uci = [](const string &s)->Move {
                        if (s.size() < 4) return Move();
                        int sc = s[0] - 'a';
                        int sr = s[1] - '1';
                        int ec = s[2] - 'a';
                        int er = s[3] - '1';
                        return Move(sr, sc, er, ec);
                    };
                    Move mv = parse_uci(uci);
                    robin_map<Move,double> moves;
                    moves.emplace(mv, 1.0);
                    clock_t t0 = clock();
                    double sc = problem_test(fen, moves, time_per_puzzle);
                    clock_t t1 = clock();
                    test_time = (double)(t1 - t0) / CLOCKS_PER_SEC;
                    test_score = sc;
                    handled = true;
                } else {
                    cout << "  [PUZZLE] no UCI move found in context; skipping puzzle evaluation." << endl;
                    handled = true; // consider handled to avoid falling back to evaluation
                }
            }
        }

        if (!handled) {
            // For imported run we focus on evaluation positions (unless tag requests otherwise)
            int expected_eval = 0; pair<int,int> eval_range = { -150, 150 };
            double expected_score = 0.5; pair<double,double> score_range = { 0.3, 0.7 };
            if (expected_eval_sign > 0) { expected_eval = 300; eval_range = {100,2000}; expected_score = 0.75; score_range = {0.6,1.0}; }
            else if (expected_eval_sign < 0) { expected_eval = -300; eval_range = {-2000,-100}; expected_score = 0.25; score_range = {0.0,0.4}; }

            clock_t t0 = clock();
            double sc = evaluation_test(fen, expected_eval, eval_range, expected_score, score_range);
            clock_t t1 = clock();
            test_time = (double)(t1 - t0) / CLOCKS_PER_SEC;
            test_score = sc;
        }

        imported_count++;
        if (test_score > 0.5) imported_passed++;
        tested_flag[i] = 1;

        // progress summary and combined overall score
        double combined_score = 0.0;
        int combined_tests = 0;
        if (base_total_tests + imported_count > 0) {
            combined_tests = base_total_tests + imported_count;
            combined_score = (base_total_score + imported_passed) / (double)combined_tests;
        }

        cout << "  [PROGRESS] processed=" << processed << ", remaining~=" << (total_fens - processed) << ", passed=" << imported_passed << ", total_runs=" << imported_count << ", current_rate=" << fixed << setprecision(3) << (imported_count? (double)imported_passed/imported_count : 0.0) << ", overall_rate=" << fixed << setprecision(3) << combined_score << "\n";

        // allow user to interrupt via ESC or flag
        if (IsKeyPressed(KEY_ESCAPE) || _stop_imported_tests) {
            cout << "Stopping imported tests on user request." << endl;
            break;
        }
    }

    cout << "Imported tests processed: " << imported_count << ", passed(>0.5): " << imported_passed << endl;
    double aggregate = imported_count == 0 ? 0.0 : (double)imported_passed / (double)imported_count;

    // Mark tested lines in the source Tests.txt by prepending [TESTED] for processed FEN lines
    try {
        bool wrote = false;
        for (size_t i = 0; i < file_lines.size(); ++i) {
            if (!tested_flag[i]) continue;
            string s = file_lines[i];
            string prefix = "[TESTED]";
            string trimmed = s;
            if (trimmed.rfind(prefix, 0) != 0) { wrote = true; break; }
        }
        if (wrote) {
            // rewrite file with markings
            std::ofstream out(tests_path + "+tmp");
            for (size_t i = 0; i < file_lines.size(); ++i) {
                string s = file_lines[i];
                if (tested_flag[i]) {
                    if (s.rfind("[TESTED]", 0) != 0) out << "[TESTED] " << s << "\n";
                    else out << s << "\n";
                } else {
                    out << s << "\n";
                }
            }
            out.close();
            // replace original
            std::remove(tests_path.c_str());
            std::rename((tests_path + "+tmp").c_str(), tests_path.c_str());
            cout << "Marked processed positions with [TESTED] in " << tests_path << endl;
        }
    } catch (...) {
        cout << "Warning: could not update " << tests_path << " to mark tested positions." << endl;
    }
    return aggregate;
}

int Tests::add_generated_evaluation_tests(const string& tests_path) {
    // Read the Tests.txt file and create conservative evaluation tests for unmarked FEN lines.
    ensure_utf8_output();
    std::ifstream in(tests_path);
    if (!in.is_open()) {
        cout << "Could not open " << tests_path << " to generate evaluation tests." << endl;
        return 0;
    }

    vector<string> lines;
    string line;
    while (std::getline(in, line)) lines.push_back(line);
    in.close();

    auto is_fen_line = [&](const string &l)->bool {
        string fen = extract_fen_from_line(l);
        return !fen.empty();
    };

    std::regex san_regex(R"(([KQRNBTDCF]?[a-h]?[1-8]?[x:]?[a-h][1-8](=[KQRNBTDCF])?[+#]?))");
    std::regex uci_regex(R"(([a-h][1-8][a-h][1-8]))");

    // count unmarked FENs for progress
    int total_unmarked = 0;
    for (auto &ln : lines) {
        string ts = trim_copy(ln);
        if (ts.rfind("[TESTED]", 0) == 0) continue;
        if (is_fen_line(ts)) total_unmarked++;
    }

    int added = 0;
    int processed = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        string s = lines[i];
        string ts = trim_copy(s);
        if (ts.rfind("[TESTED]", 0) == 0) continue; // already tested
        if (!is_fen_line(ts)) continue;

        // Build context
        processed++;
        string context;
        for (int k = (int)i - 6; k <= (int)i + 2; ++k) if (k >= 0 && k < (int)lines.size()) context += lines[k] + " ";
        string ctx = context + " " + ts;
        for (auto &c : ctx) c = (char)tolower((unsigned char)c);

        // Skip if context (excluding the FEN) contains an explicit move token
        // (we exclude puzzles). Remove the fen portion before checking.
        {
            string ctx_no_fen = ctx;
            size_t fen_pos = ctx_no_fen.find(extract_fen_from_line(ts));
            if (fen_pos != string::npos) ctx_no_fen.erase(fen_pos, extract_fen_from_line(ts).size());
            if (std::regex_search(ctx_no_fen, san_regex) || std::regex_search(ctx_no_fen, uci_regex) || ctx_no_fen.find('!') != string::npos) {
                // do not add as evaluation test
                continue;
            }
        }

        // Decide expected eval ranges conservatively
        int expected_eval = 0;
        pair<int,int> eval_range = { -150, 150 };
        double expected_score = 0.5;
        pair<double,double> score_range = { 0.3, 0.7 };

        if (ctx.find("pour les noirs") != string::npos || (ctx.find("noir") != string::npos && (ctx.find("gagn") != string::npos || ctx.find("mieux") != string::npos))) {
            expected_eval = -300; eval_range = { -2000, -100 }; expected_score = 0.25; score_range = { 0.0, 0.4 };
        } else if (ctx.find("gagn") != string::npos || ctx.find("gagne") != string::npos || ctx.find("winning") != string::npos) {
            expected_eval = 300; eval_range = { 100, 2000 }; expected_score = 0.75; score_range = { 0.6, 1.0 };
        } else if (ctx.find("nulle") != string::npos || ctx.find("draw") != string::npos) {
            expected_eval = 0; eval_range = { -50, 50 }; expected_score = 0.5; score_range = { 0.45, 0.55 };
        } else if (ctx.find("ferme") != string::npos || ctx.find("fermee") != string::npos) {
            expected_eval = 0; eval_range = { -100, 100 }; expected_score = 0.5; score_range = { 0.4, 0.6 };
        } else if (ctx.find("mate") != string::npos || ctx.find("mat") != string::npos || ctx.find("#") != string::npos) {
            expected_eval = 1000; eval_range = { 800, 2000 }; expected_score = 0.95; score_range = { 0.9, 1.0 };
        }

        string fen = extract_fen_from_line(ts);
        string comment;
        if (ts.size() > fen.size() + 1) comment = trim_copy(ts.substr(ts.find(fen) + fen.size()));

        cout << "[GENERATED TEST] (" << processed << "/" << total_unmarked << ")" << endl;
        cout << "  FEN: " << fen << endl;
        if (!comment.empty()) cout << "  Comment: " << (comment.size() > 120 ? comment.substr(0,120) + "..." : comment) << endl;

        double sc = evaluation_test(fen, expected_eval, eval_range, expected_score, score_range);
        (void)sc;

        // Mark line as tested in memory and write back later
        if (ts.rfind("[TESTED]", 0) != 0) lines[i] = string("[TESTED] ") + s;
        added++;

        // Allow interrupt
        if (_stop_imported_tests || IsKeyPressed(KEY_ESCAPE)) {
            cout << "Generation interrupted by user." << endl;
            break;
        }

        // progress
        cout << "  [GEN PROGRESS] " << processed << "/" << total_unmarked << " added=" << added << "\n";
    }

    if (added > 0) {
        // write back file
        std::ofstream out(tests_path + ".tmp");
        for (auto &l : lines) out << l << "\n";
        out.close();
        std::remove(tests_path.c_str());
        std::rename((tests_path + ".tmp").c_str(), tests_path.c_str());
        cout << "Wrote " << added << " [TESTED] markers to " << tests_path << endl;
    }

    return added;
}