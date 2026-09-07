#pragma once

#include <string>
#include <vector>
#include <map>
#include <cmath>
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

// Nature-first gap between two white-relative cp evals: the WDL-derived
// expected-score difference dominates, raw cp only breaks ties within a
// nature. +300 vs +50 (different natures) >> +1000 vs +480 (both winning).
// Uses the engine's own cp->WDL curve (zero uncertainty, fully winnable).
inline double eval_gap_cp(int ours_cp, int ref_cp) {
    Evaluation a, b;
    a.reset(); b.reset();
    a._value = ours_cp; a._evaluated = true;
    b._value = ref_cp; b._evaluated = true;
    a.get_WDL(); a.get_average_score();
    b.get_WDL(); b.get_average_score();
    double wdl_gap = fabs((double)a._avg_score - (double)b._avg_score);
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
