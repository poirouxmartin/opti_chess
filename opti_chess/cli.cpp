#include "board.h"
#include "puzzle.h"
#include "exploration.h"
#include "zobrist.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdint>

using namespace std;

static const string START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

static int usage() {
    cerr << "usage: opti_chess_cli perft <depth> [fen...]\n"
         << "       opti_chess_cli perft-bulk <depth> [fen...]\n"
         << "       opti_chess_cli divide <depth> [fen...]\n"
         << "       opti_chess_cli perft-suite [quick|full] [bulk]\n"
         << "       opti_chess_cli bench [iterations]\n"
         << "       opti_chess_cli uci\n"
         << "       opti_chess_cli search <iterations> [fen...]   (OPTI_NO_DAG=1: tree mode)\n";
    return 2;
}

static double now_s() {
    return chrono::duration<double>(chrono::steady_clock::now().time_since_epoch()).count();
}

// Joins argv[from..] into a FEN, start position if none given
static string fen_from_args(int argc, char** argv, int from) {
    string fen;
    for (int i = from; i < argc; i++) {
        if (!fen.empty()) fen += ' ';
        fen += argv[i];
    }
    return fen.empty() ? START_FEN : fen;
}

// Square names: file a-h, rank 1-8; promotion suffix only for pawn moves to last rank
static string move_to_uci(const Board& b, const Move& m) {
    string s;
    s += char('a' + m.start_col);
    s += char('1' + m.start_row);
    s += char('a' + m.end_col);
    s += char('1' + m.end_row);
    int piece = b._array[m.start_row][m.start_col];
    if ((piece == 1 || piece == 7) && (m.end_row == 0 || m.end_row == 7))
        s += "qrbn"[m.get_promo_piece()];
    return s;
}

// Legal move matching a UCI string
static bool uci_to_move(Board& b, const string& s, Move& out) {
    b.get_moves();
    for (int i = 0; i < b._got_moves; i++) {
        if (move_to_uci(b, b._moves[i]) == s) {
            out = b._moves[i];
            return true;
        }
    }
    return false;
}

static int cmd_perft(int argc, char** argv, bool bulk) {
    if (argc < 3) return usage();
    int depth = (int)strtoll(argv[2], nullptr, 10);
    Board b;
    b.from_fen(fen_from_args(argc, argv, 3));
    if (!b.fen_ok()) { cerr << "bad fen\n"; return 2; }
    double t0 = now_s();
    long long n = bulk ? b.count_nodes_bulk(depth) : b.count_nodes_at_depth(depth, false);
    double ms = (now_s() - t0) * 1000.0;
    cout << "nodes=" << n << " time_ms=" << fixed << setprecision(1) << ms
         << " mnps=" << setprecision(2) << (ms > 0 ? n / ms / 1000.0 : 0.0) << endl;
    return 0;
}

static int cmd_divide(int argc, char** argv) {
    if (argc < 3) return usage();
    int depth = (int)strtoll(argv[2], nullptr, 10);
    string fen = fen_from_args(argc, argv, 3);
    Board b;
    b.from_fen(fen);
    if (!b.fen_ok()) { cerr << "bad fen\n"; return 2; }
    b.get_moves();
    long long total = 0;
    for (int i = 0; i < b._got_moves; i++) {
        Move m = b._moves[i];
        string uci = move_to_uci(b, m);
        // Depth-1 count from a fresh board; depth 1 is one leaf per move
        long long n = 1;
        if (depth > 1) {
            Board c;
            c.from_fen(fen);
            c.make_move(m);
            n = c.count_nodes_at_depth(depth - 1, false);
        }
        total += n;
        cout << uci << ": " << n << endl;
    }
    cout << "total=" << total << endl;
    return 0;
}

struct SuiteRow {
    const char* name;
    const char* fen;
    int quick_depth;
    long long quick_nodes;
    int full_depth;
    long long full_nodes;
};

static const SuiteRow SUITE[] = {
    {"startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609, 6, 119060324},
    {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603, 5, 193690690},
    {"pos3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624, 6, 11030083},
    {"pos4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4, 422333, 5, 15833292},
    {"pos5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2103487, 5, 89941194},
    {"pos6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4, 3894594, 5, 164075551},
    {"storm", "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1", 4, 182838, 5, 3605103},
};

static int cmd_suite(int argc, char** argv) {
    bool full = false;
    if (argc > 2) {
        string mode = argv[2];
        if (mode == "full") full = true;
        else if (mode != "quick") return usage();
    }
    bool bulk = false;
    if (argc > 3) {
        if (string(argv[3]) == "bulk") bulk = true;
        else return usage();
    }
    cout << "mode: " << (bulk ? "bulk counting" : "copy-make") << endl;
    const int rows = (int)(sizeof(SUITE) / sizeof(SUITE[0]));
    int ok = 0;
    long long total_nodes = 0;
    double total_ms = 0.0;
    cout << left << setw(10) << "name" << right << setw(4) << "d" << setw(14) << "nodes"
         << setw(14) << "expected" << setw(6) << "ok" << setw(12) << "ms" << setw(10) << "Mnps" << endl;
    for (const SuiteRow& r : SUITE) {
        int depth = full ? r.full_depth : r.quick_depth;
        long long expected = full ? r.full_nodes : r.quick_nodes;
        Board b;
        b.from_fen(r.fen);
        double t0 = now_s();
        long long n = bulk ? b.count_nodes_bulk(depth) : b.count_nodes_at_depth(depth, false);
        double ms = (now_s() - t0) * 1000.0;
        bool good = n == expected;
        if (good) ok++;
        total_nodes += n;
        total_ms += ms;
        cout << left << setw(10) << r.name << right << setw(4) << depth << setw(14) << n
             << setw(14) << expected << setw(6) << (good ? "OK" : "FAIL")
             << fixed << setw(12) << setprecision(1) << ms
             << setw(10) << setprecision(2) << (ms > 0 ? n / ms / 1000.0 : 0.0) << endl;
    }
    cout << "TOTAL nodes=" << total_nodes << " time_ms=" << fixed << setprecision(1) << total_ms
         << " mnps=" << setprecision(2) << (total_ms > 0 ? total_nodes / total_ms / 1000.0 : 0.0) << endl;
    cout << "RESULT: " << ok << "/" << rows << " OK" << endl;
    return ok == rows ? 0 : 1;
}

static int cmd_bench(int argc, char** argv) {
    long long iters = 2000;
    if (argc > 2) {
        iters = strtoll(argv[2], nullptr, 10);
        if (iters <= 0) return usage();
    }
    static const char* FENS[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
        "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4",
        "8/8/8/3k4/8/1PK5/8/8 w - - 5 17",
        "2rq1rk1/1p2bpp1/p1n2n1p/3pNb2/2pP1P2/P1N1PBB1/1P4PP/R2Q1RK1 w - - 0 1",
    };
    const int count = (int)(sizeof(FENS) / sizeof(FENS[0]));
    uint64_t hash = 1469598103934665603ULL; // FNV-1a 64-bit offset basis
    long long sum_nodes = 0;
    double sum_ms = 0.0;
    for (int i = 0; i < count; i++) {
        Puzzle p;
        p.fen = FENS[i];
        p.category = PuzzleCategory::TACTIC;
        double t0 = now_s();
        PuzzleResult r = PuzzleRunner::run(p, BudgetMode::NODES, (double)iters);
        double ms = (now_s() - t0) * 1000.0;
        Board b;
        b.from_fen(FENS[i]);
        string uci = move_to_uci(b, r.chosen_move);
        cout << i << " bestmove=" << uci << " eval=" << r.actual_eval_cp << " iters=" << r.iterations
             << " nodes=" << r.total_nodes << " ms=" << fixed << setprecision(1) << ms << endl;
        sum_nodes += r.total_nodes;
        sum_ms += ms;
        // Hash of bestmove followed by node count, per position
        for (char c : uci + to_string(r.total_nodes)) {
            hash ^= (unsigned char)c;
            hash *= 1099511628211ULL;
        }
    }
    cout << "total nodes=" << sum_nodes << " ms=" << fixed << setprecision(1) << sum_ms
         << " nodes/s=" << setprecision(0) << (sum_ms > 0 ? sum_nodes / (sum_ms / 1000.0) : 0.0) << endl;
    cout << "signature=" << hex << setw(16) << setfill('0') << hash << dec << setfill(' ') << endl;
    if (g_qstats_on)
        dump_qstats(); // OPTI_QSTATS: quiescence exit census
    return 0;
}

// Applies "position" tokens to a FEN, returns the resulting FEN
static string apply_position(const vector<string>& tok, const string& cur) {
    if (tok.size() < 2) return cur;
    string fen;
    size_t i = 2;
    if (tok[1] == "startpos") {
        fen = START_FEN;
    } else if (tok[1] == "fen") {
        for (; i < tok.size() && i < 8 && tok[i] != "moves"; i++) {
            if (!fen.empty()) fen += ' ';
            fen += tok[i];
        }
    } else {
        return cur;
    }
    Board b;
    b.from_fen(fen);
    if (!b.fen_ok()) {
        cout << "info string bad fen" << endl;
        return cur;
    }
    if (i < tok.size() && tok[i] == "moves") {
        for (size_t j = i + 1; j < tok.size(); j++) {
            Move m;
            if (!uci_to_move(b, tok[j], m)) {
                cout << "info string illegal move " << tok[j] << endl;
                break;
            }
            b.make_move(m, false, true);
        }
    }
    return b.to_fen();
}

static void cmd_go(const vector<string>& tok, const string& fen) {
    long long nodes = 0, movetime = 0, depth = 0, wtime = 0, btime = 0, winc = 0, binc = 0;
    bool infinite = false;
    for (size_t i = 1; i < tok.size(); i++) {
        if (tok[i] == "infinite") { infinite = true; continue; }
        if (i + 1 >= tok.size()) break;
        const string& key = tok[i];
        char* end = nullptr;
        long long v = strtoll(tok[i + 1].c_str(), &end, 10);
        if (end != tok[i + 1].c_str() && *end == '\0') {
            if (key == "nodes") nodes = v;
            else if (key == "movetime") movetime = v;
            else if (key == "depth") depth = v;
            else if (key == "wtime") wtime = v;
            else if (key == "btime") btime = v;
            else if (key == "winc") winc = v;
            else if (key == "binc") binc = v;
        }
        i++;
    }

    Board b;
    b.from_fen(fen);
    bool white = b._player;
    long long mytime = white ? wtime : btime;
    long long myinc = white ? winc : binc;

    Puzzle p;
    p.fen = fen;
    p.category = PuzzleCategory::TACTIC;
    BudgetMode mode = BudgetMode::NODES;
    double budget = 2000.0;
    if (nodes > 0) {
        budget = (double)nodes;
    } else if (movetime > 0) {
        mode = BudgetMode::TIME;
        budget = movetime / 1000.0;
    } else if (mytime > 0) {
        mode = BudgetMode::TIME;
        budget = (mytime / 30.0 + myinc / 2.0) / 1000.0;
        if (budget < 0.05) budget = 0.05;
        if (budget > 10.0) budget = 10.0;
    } else if (infinite) {
        budget = 20000.0;
    } else if (depth > 0) {
        budget = 2000.0;
    }

    PuzzleResult r = PuzzleRunner::run(p, mode, budget);
    // Searched value (actual_eval_cp is the root static eval)
    int score = white ? r.searched_eval_cp : -r.searched_eval_cp;
    long long ms = (long long)(r.time_s * 1000.0 + 0.5);
    cout << "info nodes " << r.total_nodes << " time " << ms << " score cp " << score << endl;

    // Chosen move must be among the legal moves, otherwise 0000
    string best = "0000";
    b.get_moves();
    string want = move_to_uci(b, r.chosen_move);
    for (int i = 0; i < b._got_moves; i++) {
        if (move_to_uci(b, b._moves[i]) == want) {
            best = want;
            break;
        }
    }
    cout << "bestmove " << best << endl;
}

// Searches a FEN like PuzzleRunner NODES mode (DAG on unless OPTI_NO_DAG) and
// prints the SEARCHED root value (white POV), not the static eval, at 10 steps
static int cmd_search(int argc, char** argv) {
    if (argc < 3) return usage();
    const int iters = atoi(argv[2]);
    const string fen = fen_from_args(argc, argv, 3);
    static Evaluator evaluator;
    monte_node_buffer.init(500000, false);
    monte_board_buffer.init(500000, false);
    monte_node_buffer.reset();
    monte_board_buffer.reset();
    node_map.clear();
    transposition_table.clear();
    g_buffers_full_logged = false;
    g_tt_node_dag = getenv("OPTI_NO_DAG") == nullptr;
    g_tt_main_search = getenv("OPTI_TT_MAIN") != nullptr;

    Board b;
    b.from_fen(fen);
    Node root(&b);
    const double t0 = now_s();
    int done = 0;
    for (int step = 1; step <= 10; step++) {
        const int target = (int)((long long)iters * step / 10);
        if (target > done)
            root.grogros_zero(&monte_board_buffer, &evaluator, 0.005, 5.0, 1.10, target - done, 10, nullptr, nullptr, 0);
        done = target;
        const Move bm = root.get_most_explored_child_move();
        cout << "iters " << done
             << " ms " << (long long)((now_s() - t0) * 1000.0)
             << " boards " << (monte_board_buffer._length - (int)monte_board_buffer._free_indices.size())
             << " value " << root._deep_evaluation._value
             << " best " << (bm.is_null_move() ? string("0000") : move_to_uci(b, bm))
             << " depth " << root.get_main_depth(0.005, 5.0)
             << " pruned " << g_pruned_nodes << endl;
    }
    cout << "pv " << root.get_exploration_variants(0.005, 5.0, true, false, 60) << endl;

    // OPTI_SEARCH_DUMP=1: root children, then the children of the most explored one
    if (getenv("OPTI_SEARCH_DUMP") != nullptr) {
        auto dump = [](Node& n, const char* tag) {
            for (auto const& [m, link] : n._children) {
                if (link._node == nullptr) continue;
                Node* c = link._node;
                cout << tag << " " << move_to_uci(*n._board, m)
                     << " visits " << link._chosen_iterations
                     << " stored " << c->_deep_evaluation._value
                     << " avg " << c->_deep_evaluation._avg_score
                     << " standpat " << c->_is_stand_pat_eval
                     << " kids " << c->children_count() << "/" << (int)c->_board->_got_moves
                     << " pathval " << (c->_has_path_value ? to_string(c->_path_value._value) : string("-"))
                     << " pc " << c->_parent_count << endl;
                cout << "   line " << c->get_exploration_variants(0.005, 5.0, true, false, 40) << endl;
            }
        };
        // Origin of the root value: follow children holding the same value
        {
            Node* n = &root;
            const int v = root._deep_evaluation._value;
            for (int d = 0; d < 200 && n != nullptr; d++) {
                Node* next = nullptr;
                for (auto const& [m, link] : n->_children)
                    if (link._node != nullptr && link._node->_deep_evaluation._value == v) { next = link._node; break; }
                if (next == nullptr) {
                    cout << "origin depth " << d << " fen " << n->_board->to_fen()
                         << " terminal " << n->_is_terminal << " standpat " << n->_is_stand_pat_eval
                         << " static " << n->_static_evaluation._value << " deep " << n->_deep_evaluation._value
                         << " avg " << n->_deep_evaluation._avg_score << " qdepth " << n->_quiescence_depth
                         << " kids " << n->children_count() << "/" << (int)n->_board->_got_moves << endl;
                    break;
                }
                n = next;
            }
        }
        dump(root, "root");
        const Move bm = root.get_most_explored_child_move();
        if (!bm.is_null_move()) dump(*root._children[bm]._node, "best");
    }
    return 0;
}

static int cmd_uci() {
    string cur = START_FEN;
    string line;
    while (getline(cin, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.rfind("\xEF\xBB\xBF", 0) == 0) line.erase(0, 3); // UTF-8 BOM from PowerShell pipes
        istringstream iss(line);
        vector<string> tok;
        string t;
        while (iss >> t) tok.push_back(t);
        if (tok.empty()) continue;
        const string& c = tok[0];
        if (c == "uci") {
            cout << "id name OptiChess" << endl;
            cout << "id author Martin" << endl;
            cout << "uciok" << endl;
        } else if (c == "isready") {
            cout << "readyok" << endl;
        } else if (c == "ucinewgame") {
            cur = START_FEN;
        } else if (c == "position") {
            cur = apply_position(tok, cur);
        } else if (c == "go") {
            cmd_go(tok, cur);
        } else if (c == "quit") {
            return 0;
        }
        // stop and unknown commands are ignored
    }
    return 0;
}

int main(int argc, char** argv) {
    // Fixed seed for reproducible bench/search unless the caller sets one
    if (!getenv("OPTI_SEED")) {
#ifdef _WIN32
        _putenv_s("OPTI_SEED", "42");
#else
        setenv("OPTI_SEED", "42", 1);
#endif
    }
    if (argc < 2) return usage();
    string cmd = argv[1];
    if (cmd == "perft") return cmd_perft(argc, argv, false);
    if (cmd == "perft-bulk") return cmd_perft(argc, argv, true);
    if (cmd == "divide") return cmd_divide(argc, argv);
    if (cmd == "perft-suite") return cmd_suite(argc, argv);
    if (cmd == "bench") return cmd_bench(argc, argv);
    if (cmd == "uci") return cmd_uci();
    if (cmd == "search") return cmd_search(argc, argv);
    return usage();
}
