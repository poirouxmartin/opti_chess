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
    // Concentration sweep hook: OPTI_GAMMA overrides the gamma arg so
    // benchmarks can A/B exploration pressure without code changes.
    if (const char* gamma_env = getenv("OPTI_GAMMA")) gamma = atof(gamma_env);
    PuzzleResult result;
    result.is_eval_puzzle = p.is_eval_puzzle;
    auto t0 = chrono::steady_clock::now();
    const clock_t t_entry = clock();

    // EMA of per-puzzle non-search overhead, seconds (init/reset/clear before
    // the search + traverse/score after). TIME budgets promise TOTAL time, so
    // the search gets budget minus expected overhead (floored). Self-tuning
    // across machines/loads; single-threaded use (matches codebase style).
    static double s_overhead_ema_s = 0.008;

    // Hard-deadline state never leaks across calls (a TIME abort must not
    // starve a later NODES run).
    g_search_deadline = 0;
    g_search_abort = false;

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
    transposition_table.clear();
    g_buffers_full_logged = false;

    const bool prev_dag = g_tt_node_dag;
    g_tt_node_dag = true;

    Node root(&b);

    clock_t t_search_start = 0, t_search_end = 0; // TIME mode only (EMA below)
    if (mode == BudgetMode::TIME) {
        t_search_start = clock();
        // Search budget = total budget minus expected fixed overhead, so the
        // whole run() call (not just the search loop) lands on budget.
        // Floor keeps a usable search on tiny budgets / huge overhead.
        double search_budget = budget - s_overhead_ema_s;
        const double min_search = 0.2 * budget;
        if (search_budget < min_search) search_budget = min_search;
        // Hard deadline: quiescence samples the clock (throttled) and raises
        // g_search_abort past due, so a single deep iteration cannot overrun
        // the budget by seconds. Reset per puzzle (never leaks across runs).
        g_search_deadline = t_search_start + (clock_t)(search_budget * CLOCKS_PER_SEC);
        g_search_abort = false;
        while ((double)(clock() - t_search_start) / CLOCKS_PER_SEC < search_budget) {
            root.grogros_zero(&monte_board_buffer, evaluator, alpha, beta, gamma, 1, quiescence_depth, nullptr, nullptr, 0);
        }
        g_search_deadline = 0;
        t_search_end = clock();
    } else if (mode == BudgetMode::NODES) {
        int iters = (int)budget;
        root.grogros_zero(&monte_board_buffer, evaluator, alpha, beta, gamma, iters, quiescence_depth, nullptr, nullptr, 0);
    } else if (mode == BudgetMode::QUIESCENCE_ONLY) {
        int iters = max(1, (int)(budget / 100));
        int qdepth = (int)budget;
        root.grogros_zero(&monte_board_buffer, evaluator, 0.0, 1.0, 0.5, iters, qdepth, nullptr, nullptr, 0);
    }

    g_tt_node_dag = prev_dag;

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
        // --- Continuous variable reward: move * eval spectrum (0..1) ---
        bool isWhite = b._player;
        double wdl = result.actual_wdl_w;
        auto it = root._children.find(result.chosen_move);
        if(it != root._children.end() && it->second._node){
            wdl = it->second._node->_deep_evaluation._avg_score;
        }
        // wdl is white win prob 0..1. For white to move expected ~1, for black ~0.
        double eval_cont = isWhite ? wdl : (1.0 - wdl); // 0..1 continuous
        // Clamp to avoid extreme 0/1 noise, keep spectrum
        eval_cont = std::clamp(eval_cont, 0.0, 1.0);
        if(move_score > 0.5){
            result.score = eval_cont; // 0..1 spectrum: 0.9 true find, 0.2 luck
        } else {
            result.score = 0.0;
        }
        result.eval_score = eval_cont;
    }

    auto t1 = chrono::steady_clock::now();
    result.time_s = chrono::duration<double>(t1 - t0).count();
    if (mode == BudgetMode::TIME && t_search_end > t_search_start) {
        // Feed the overhead EMA: non-search time = total minus search loop.
        // Keeps future TIME budgets landing on total time, not search-only.
        // Spike rejection: cold first-puzzle allocs (hundreds of ms) must not
        // poison the average; only track representative overhead.
        double overhead = (double)(result.time_s - (t_search_end - t_search_start) / (double)CLOCKS_PER_SEC);
        if (overhead >= 0.0 && overhead <= 3.0 * s_overhead_ema_s + 0.005)
            s_overhead_ema_s = 0.7 * s_overhead_ema_s + 0.3 * overhead;
    }
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
