#pragma once

#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <functional>
#include "board.h"

using namespace std;

enum class PuzzleCategory : uint8_t {
    TACTIC,
    EVALUATION,
    ENDGAME,
    STRATEGIC,
    DEFENSIVE
};

enum class BudgetMode : uint8_t {
    TIME,
    NODES,
    STATIC_EVAL,
    QUIESCENCE_ONLY
};

inline const char* puzzle_category_name(PuzzleCategory c) {
    switch (c) {
        case PuzzleCategory::TACTIC:     return "TACTIC";
        case PuzzleCategory::EVALUATION: return "EVAL";
        case PuzzleCategory::ENDGAME:    return "ENDGAME";
        case PuzzleCategory::STRATEGIC:  return "STRATEGIC";
        case PuzzleCategory::DEFENSIVE:  return "DEFENSIVE";
    }
    return "?";
}

inline const char* budget_mode_name(BudgetMode m) {
    switch (m) {
        case BudgetMode::TIME:           return "TIME";
        case BudgetMode::NODES:          return "NODES";
        case BudgetMode::STATIC_EVAL:    return "STATIC";
        case BudgetMode::QUIESCENCE_ONLY: return "QSEARCH";
    }
    return "?";
}

struct RatedMove {
    Move move;
    double reward;
};

// Total material of a FEN (Stockfish WDL-model units: P+3N+3B+5R+9Q,
// both sides, kings excluded). Stops at the first space (board part).
inline int fen_material_total(const std::string& fen) {
    int wp = 0, bp = 0, wn = 0, bn = 0, wb = 0, bb = 0, wr = 0, br = 0, wq = 0, bq = 0;
    for (char c : fen) {
        if (c == ' ') break;
        if (c == '/') continue;
        if (c >= '1' && c <= '8') continue;
        bool is_white = (c >= 'A' && c <= 'Z');
        switch ((char)tolower(c)) {
        case 'p': if (is_white) wp++; else bp++; break;
        case 'n': if (is_white) wn++; else bn++; break;
        case 'b': if (is_white) wb++; else bb++; break;
        case 'r': if (is_white) wr++; else br++; break;
        case 'q': if (is_white) wq++; else bq++; break;
        }
    }
    return wp + bp + (wn + bn + wb + bb) * 3 + (wr + br) * 5 + (wq + bq) * 9;
}

// Win-rate model fitted on LTC fishtest statistics (Stockfish
// win_rate_model, uci.cpp): W(v) = 1/(1+exp((a-v)/b)) with material-fitted
// a, b. v is in Stockfish internal units, i.e. external cp rescaled by
// 100/a (to_cp inverts it: cp = round(100*v/a)). Returns the white
// expected score in [0,1]: a real-world winning chance, not the engine's
// confidence in its own eval.
inline double wdl_expected_score(int cp, int material) {
    double m = std::clamp(material, 17, 78) / 58.0;
    static constexpr double as[] = { -142.72052667, 372.35176398, -340.71073572, 415.23490212 };
    static constexpr double bs[] = { 5.93832785, 15.61267078, -30.57816876, 69.63866711 };
    double a = (((as[0] * m + as[1]) * m + as[2]) * m) + as[3];
    double b = (((bs[0] * m + bs[1]) * m + bs[2]) * m) + bs[3];
    double v = (double)cp * a / 100.0;
    double w = 1.0 / (1.0 + exp((a - v) / b));
    double vn = -(double)cp * a / 100.0;
    double l = 1.0 / (1.0 + exp((a - vn) / b));
    return w + 0.5 * (1.0 - w - l);
}

// Nature-first gap between two white-relative cp evals, measured in
// real-world winning chances: |P_win(ours) - P_win(ref)| on the empirical
// WDL curve above, raw cp only breaking ties within a nature.
// +300 vs +50 (different natures) >> +1000 vs +480 (both winning).
inline double eval_gap_cp(int ours_cp, int ref_cp, int material = 58) {
    double wdl_gap = fabs(wdl_expected_score(ours_cp, material) - wdl_expected_score(ref_cp, material));
    double cp_gap = fmin(1.0, fabs((double)ours_cp - (double)ref_cp) / 1000.0);
    return wdl_gap + 0.1 * cp_gap;
}

struct Puzzle {
    string fen;
    PuzzleCategory category;
    string theme;
    string name;
    vector<RatedMove> allowed_moves;

    bool is_eval_puzzle = false;
    int expected_eval_cp = 0;
    pair<int, int> eval_range = { -500, 500 };
    double expected_wdl_w = 0.5;
    pair<double, double> wdl_range = { 0.0, 1.0 };

    Puzzle() = default;
    Puzzle(const string& f, PuzzleCategory cat, const string& th, const string& n,
        vector<RatedMove> moves)
        : fen(f), category(cat), theme(th), name(n), allowed_moves(moves) {}
};

struct PuzzleResult {
    double score = 0.0;
    Move chosen_move;
    string chosen_move_san;
    int actual_eval_cp = 0;
    double actual_wdl_w = 0.5;
    int iterations = 0;
    int total_nodes = 0;
    double time_s = 0.0;
    bool is_eval_puzzle = false;
    double eval_score = 0.0;
};

class PuzzleRunner {
public:
    static PuzzleResult run(const Puzzle& p, BudgetMode mode, double budget,
        Evaluator* evaluator = nullptr, int quiescence_depth = 10,
        double alpha = 0.005, double beta = 5.0, double gamma = 1.10);

    struct BatchResult {
        int total = 0;
        double total_score = 0.0;
        map<string, pair<int, double>> by_category;
        map<string, pair<int, double>> by_theme;
        vector<pair<string, PuzzleResult>> results;
    };

    static BatchResult run_batch(
        const vector<Puzzle>& puzzles,
        BudgetMode mode,
        double budget,
        Evaluator* evaluator = nullptr,
        int quiescence_depth = 10,
        double alpha = 0.00001, double beta = 5.0, double gamma = 1.10);
};
