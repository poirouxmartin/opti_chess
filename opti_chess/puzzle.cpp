#include "puzzle.h"
#include "exploration.h"
#include "buffer.h"
#include "zobrist.h"
#include <cmath>
#include <iostream>
#include <chrono>

using namespace std;

static double score_reward_for_move(const Puzzle& p, const Move& chosen) {
    for (const auto& rm : p.allowed_moves) {
        if (rm.move == chosen) return rm.reward;
    }
    return 0.0;
}

PuzzleResult PuzzleRunner::run(const Puzzle& p, BudgetMode mode, double budget,
    Evaluator* evaluator, int quiescence_depth,
    double alpha, double beta, double gamma) {
    PuzzleResult result;
    result.is_eval_puzzle = p.is_eval_puzzle;
    auto t0 = chrono::steady_clock::now();

    Board b;
    b.from_fen(p.fen);

    if (!evaluator) {
        static Evaluator default_eval;
        evaluator = &default_eval;
    }

    Evaluation static_eval;
    b.evaluate(&static_eval, evaluator, false, nullptr, true);
    result.actual_eval_cp = static_eval._value;
    result.actual_wdl_w = static_eval._avg_score;

    if (mode == BudgetMode::STATIC_EVAL) {
        result.eval_score = 0.0;
        if (p.is_eval_puzzle) {
            int diff = abs(result.actual_eval_cp - p.expected_eval_cp);
            double acceptable = max(abs(p.eval_range.first - p.expected_eval_cp),
                abs(p.eval_range.second - p.expected_eval_cp)) + 1.0;
            result.eval_score = max(0.0, 1.0 - pow(diff / acceptable, 2.0) / 2.0);

            double wdl_diff = abs(result.actual_wdl_w - p.expected_wdl_w);
            double wdl_acceptable = max(abs(p.wdl_range.first - p.expected_wdl_w),
                abs(p.wdl_range.second - p.expected_wdl_w)) + 1e-9;
            double wdl_score = max(0.0, 1.0 - pow(wdl_diff / wdl_acceptable, 2.0) / 2.0);

            result.score = (result.eval_score + wdl_score) / 2.0;
        } else {
            result.score = 0.0;
            result.chosen_move = Move();
        }
        auto t1 = chrono::steady_clock::now();
        result.time_s = chrono::duration<double>(t1 - t0).count();
        return result;
    }

    monte_node_buffer.init(500000, false);
    monte_board_buffer.init(500000, false);
    monte_node_buffer.reset();
    monte_board_buffer.reset();
    node_map.clear();

    Node root(&b);

    if (mode == BudgetMode::TIME) {
        clock_t begin = clock();
        while ((double)(clock() - begin) / CLOCKS_PER_SEC < budget) {
            root.grogros_zero(&monte_board_buffer, evaluator, alpha, beta, gamma, 1, quiescence_depth, nullptr, nullptr, 0);
        }
    } else if (mode == BudgetMode::NODES) {
        int iters = (int)budget;
        root.grogros_zero(&monte_board_buffer, evaluator, alpha, beta, gamma, iters, quiescence_depth, nullptr, nullptr, 0);
    } else if (mode == BudgetMode::QUIESCENCE_ONLY) {
        int iters = max(1, (int)(budget / 100));
        int qdepth = (int)budget;
        root.grogros_zero(&monte_board_buffer, evaluator, 0.0, 1.0, 0.5, iters, qdepth, nullptr, nullptr, 0);
    }

    result.iterations = root._iterations;
    result.total_nodes = root.get_total_nodes();
    result.chosen_move = root.get_most_explored_child_move();
    result.chosen_move_san = b.move_label(result.chosen_move);
    result.actual_eval_cp = root._static_evaluation._value;
    result.actual_wdl_w = root._static_evaluation._avg_score;

    if (p.is_eval_puzzle) {
        int diff = abs(result.actual_eval_cp - p.expected_eval_cp);
        double acceptable = max(abs(p.eval_range.first - p.expected_eval_cp),
            abs(p.eval_range.second - p.expected_eval_cp)) + 1.0;
        result.eval_score = max(0.0, 1.0 - pow(diff / acceptable, 2.0) / 2.0);

        double wdl_diff = abs(result.actual_wdl_w - p.expected_wdl_w);
        double wdl_acceptable = max(abs(p.wdl_range.first - p.expected_wdl_w),
            abs(p.wdl_range.second - p.expected_wdl_w)) + 1e-9;
        double wdl_score = max(0.0, 1.0 - pow(wdl_diff / wdl_acceptable, 2.0) / 2.0);

        result.score = (result.eval_score + wdl_score) / 2.0;
    } else {
        double move_score = score_reward_for_move(p, result.chosen_move);
        // --- New variable reward: eval correctness + PV correctness (luck vs find) ---
        // Expected: tactic puzzles are winning for side to move, so eval should indicate winning
        bool isWhite = b._player;
        // Deep eval of chosen move if available, else root static
        int deep_cp = result.actual_eval_cp;
        double wdl = result.actual_wdl_w;
        auto it = root._children.find(result.chosen_move);
        if(it != root._children.end() && it->second._node){
            deep_cp = it->second._node->_deep_evaluation._value;
            wdl = it->second._node->_deep_evaluation._avg_score;
        }
        bool eval_correct = isWhite ? (deep_cp > 150 || wdl > 0.60) : (deep_cp < -150 || wdl < 0.40);
        // PV correctness: does engine think the line is winning till the end? Use deep wdl as proxy for PV winning
        bool pv_correct = eval_correct; // placeholder: PV is winning if deep eval is winning
        // If move is correct but eval says losing, it's luck (0.5), not a true find (1.0)
        if(move_score > 0.5){
            result.score = eval_correct ? 1.0 : 0.5;
            // If you want finer: 0.7 if move correct but PV not winning? Keep 0.5 for now
        } else {
            result.score = 0.0;
        }
        // Store eval_score for debugging
        result.eval_score = eval_correct ? 1.0 : 0.0;
    }

    auto t1 = chrono::steady_clock::now();
    result.time_s = chrono::duration<double>(t1 - t0).count();
    return result;
}

PuzzleRunner::BatchResult PuzzleRunner::run_batch(
    const vector<Puzzle>& puzzles,
    BudgetMode mode,
    double budget,
    Evaluator* evaluator,
    int quiescence_depth,
    double alpha, double beta, double gamma) {

    BatchResult batch;
    batch.total = (int)puzzles.size();

    for (const auto& p : puzzles) {
        PuzzleResult r = run(p, mode, budget, evaluator, quiescence_depth, alpha, beta, gamma);
        string cat_name = puzzle_category_name(p.category);
        string th_name = p.theme;

        batch.results.push_back({ p.name, r });
        batch.total_score += r.score;

        auto& cat = batch.by_category[cat_name];
        cat.first++;
        cat.second += r.score;

        auto& th = batch.by_theme[th_name];
        th.first++;
        th.second += r.score;
    }

    return batch;
}
