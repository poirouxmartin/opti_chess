#pragma once

#include <string>
#include <vector>
#include <map>
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
    double time_s = 0.0;
    bool is_eval_puzzle = false;
    double eval_score = 0.0;
};

class PuzzleRunner {
public:
    static PuzzleResult run(const Puzzle& p, BudgetMode mode, double budget,
        Evaluator* evaluator = nullptr, int quiescence_depth = 10,
        double alpha = 0.00001, double beta = 5.0, double gamma = 1.10);

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
