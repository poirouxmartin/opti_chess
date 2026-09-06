#include "puzzle.h"
#include "exploration.h"
#include "buffer.h"
#include "zobrist.h"
#include <cmath>
#include <iostream>
#include <chrono>
#include <process.h>
#include <vector>

// No windows.h (raylib conflicts elsewhere): minimal decls like gui.cpp.
extern "C" {
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void* hHandle, unsigned long dwMilliseconds);
__declspec(dllimport) int __stdcall CloseHandle(void* hObject);
__declspec(dllimport) void* __stdcall CreateEventA(void* lpEventAttributes, int bManualReset, int bInitialState, const char* lpName);
__declspec(dllimport) int __stdcall SetEvent(void* hEvent);
__declspec(dllimport) unsigned long __stdcall WaitForMultipleObjects(unsigned long nCount, void* const* lpHandles, int bWaitAll, unsigned long dwMilliseconds);
}
#ifndef INFINITE
#define INFINITE 0xFFFFFFFF
#endif

using namespace std;

static double score_reward_for_move(const Puzzle& p, const Move& chosen) {
    for (const auto& rm : p.allowed_moves) {
        if (rm.move == chosen) return rm.reward;
    }
    return 0.0;
}

// Phase 6: Lazy-SMP workers with per-thread arenas (thread_local buffers,
// node_map, TT, kscache). Separate trees, merged by summed visits at the
// root. 16MB stacks via _beginthreadex (quiescence recursion needs it).
static const int kThreadBufSize = 32768;
static const int kThreadTTSize = 1 << 20;

struct ParallelWork {
    string fen;
    Evaluator* evaluator = nullptr;
    int quiescence_depth = 10;
    double alpha = 0.005, beta = 5.0, gamma = 1.10;
    int thread_index = 0;
    int thread_count = 1;
    int iters = 0; // NODES share (>0) ; 0 = TIME mode (loop till deadline)
    double search_budget_s = 0.0; // TIME mode
    clock_t t_search_start = 0;   // TIME mode
    // Results (written by worker, read after join):
    long long iterations = 0;
    long long total_nodes = 0;
    int root_static_value = 0;
    float root_static_avg = 0.5f;
    struct ChildStat { Move mv; long long visits = 0; float avg = 0.5f; };
    vector<ChildStat> children;
};

static void run_search_share(ParallelWork* w) {
    // Per-thread arenas (main thread keeps its own sizes from run()).
    double gamma = w->gamma;
    monte_node_buffer.init(kThreadBufSize, false);
    monte_board_buffer.init(kThreadBufSize, false);
    monte_node_buffer.reset();
    monte_board_buffer.reset();
    node_map.clear();
    transposition_table.init(kThreadTTSize, nullptr, false);
    transposition_table.clear();
    Board b;
    b.from_fen(w->fen);
    Node root(&b);
    if (w->iters > 0) {
        root.grogros_zero(&monte_board_buffer, w->evaluator, w->alpha, w->beta, gamma, w->iters, w->quiescence_depth, nullptr, nullptr, 0);
    } else {
        while (!g_search_abort.load(std::memory_order_relaxed)
            && (double)(clock() - w->t_search_start) / CLOCKS_PER_SEC < w->search_budget_s) {
            root.grogros_zero(&monte_board_buffer, w->evaluator, w->alpha, w->beta, gamma, 1, w->quiescence_depth, nullptr, nullptr, 0);
        }
    }
    w->iterations = root._iterations;
    w->total_nodes = root.get_total_nodes();
    w->root_static_value = root._static_evaluation._value;
    w->root_static_avg = root._static_evaluation._avg_score;
    for (auto const& [mv, link] : root._children) {
        if (link._node)
            w->children.push_back({ mv, (long long)link._chosen_iterations, link._node->_deep_evaluation._avg_score });
    }
}

// Persistent worker pool (Phase 6): spawn-per-puzzle churned 64MB arenas +
// TT reserves per thread per puzzle (fragmentation death after ~50 puzzles).
// Workers spawn once, sleep on an event, process shares across puzzles.
struct WorkerSlot {
    void* hThread = nullptr;
    void* hWork = nullptr; // auto-reset: main signals one share
    void* hDone = nullptr; // auto-reset: worker signals completion
    ParallelWork* work = nullptr;
    bool stop = false;
};
static vector<WorkerSlot> g_pool;
static int g_pool_threads = 0;

static unsigned __stdcall pool_worker_entry(void* param) {
    WorkerSlot* s = static_cast<WorkerSlot*>(param);
    while (true) {
        WaitForSingleObject(s->hWork, INFINITE);
        if (s->stop) return 0;
        run_search_share(s->work);
        SetEvent(s->hDone);
    }
    return 0;
}

static void ensure_pool(int n_workers) {
    if (g_pool_threads == n_workers && !g_pool.empty()) return;
    // (Re)build: stop old workers first (only when count changes).
    for (auto& s : g_pool) {
        s.stop = true;
        SetEvent(s.hWork);
        WaitForSingleObject(s.hThread, INFINITE);
        CloseHandle(s.hThread);
        CloseHandle(s.hWork);
        CloseHandle(s.hDone);
    }
    g_pool.clear();
    g_pool.resize((size_t)n_workers); // stable addresses: no growth after this
    for (int t = 0; t < n_workers; t++) {
        WorkerSlot& s = g_pool[(size_t)t];
        s.stop = false;
        s.work = nullptr;
        s.hWork = CreateEventA(nullptr, 0, 0, nullptr);
        s.hDone = CreateEventA(nullptr, 0, 0, nullptr);
        s.hThread = reinterpret_cast<void*>(_beginthreadex(nullptr, 16 * 1024 * 1024, pool_worker_entry, &s, 0, nullptr));
    }
    g_pool_threads = n_workers;
}

PuzzleResult PuzzleRunner::run(const Puzzle& p, BudgetMode mode, double budget,
    Evaluator* evaluator, int quiescence_depth,
    double alpha, double beta, double gamma) {
    // Concentration sweep hook: OPTI_GAMMA overrides the gamma arg so
    // benchmarks can A/B exploration pressure without code changes.
    if (const char* gamma_env = getenv("OPTI_GAMMA")) gamma = atof(gamma_env);
    // Convergence-speed hook: OPTI_ALPHA overrides value discrimination.
    if (const char* alpha_env = getenv("OPTI_ALPHA")) alpha = atof(alpha_env);
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

    // Phase 6 parallel path (OPTI_THREADS=N, default 1 = legacy path below).
    // N-1 workers (_beginthreadex, 16MB stacks) + main thread as share 0.
    // Separate trees (thread_local arenas), merged by summed root visits.
    int n_threads = 1;
    if (const char* th_env = getenv("OPTI_THREADS")) {
        n_threads = atoi(th_env);
        if (n_threads < 1) n_threads = 1;
        if (n_threads > 64) n_threads = 64;
    }
    if (n_threads > 1 && (mode == BudgetMode::TIME || mode == BudgetMode::NODES)) {
        vector<ParallelWork> works((size_t)n_threads);
        for (int t = 0; t < n_threads; t++) {
            ParallelWork& w = works[(size_t)t];
            w.fen = p.fen; w.evaluator = evaluator;
            w.quiescence_depth = quiescence_depth;
            w.alpha = alpha; w.beta = beta; w.gamma = gamma;
            w.thread_index = t; w.thread_count = n_threads;
        }
        clock_t t_search_start = 0, t_search_end = 0;
        if (mode == BudgetMode::TIME) {
            t_search_start = clock();
            double search_budget = budget - s_overhead_ema_s;
            const double min_search = 0.2 * budget;
            if (search_budget < min_search) search_budget = min_search;
            g_search_deadline = t_search_start + (clock_t)(search_budget * CLOCKS_PER_SEC);
            g_search_abort = false;
            for (auto& w : works) { w.iters = 0; w.search_budget_s = search_budget; w.t_search_start = t_search_start; }
        } else {
            int iters = (int)budget;
            for (int t = 0; t < n_threads; t++)
                works[(size_t)t].iters = iters / n_threads + (t < iters % n_threads ? 1 : 0);
        }
        ensure_pool(n_threads - 1);
        for (int t = 1; t < n_threads; t++) {
            g_pool[(size_t)(t - 1)].work = &works[(size_t)t];
            SetEvent(g_pool[(size_t)(t - 1)].hWork);
        }
        run_search_share(&works[0]); // main thread does share 0
        for (int t = 1; t < n_threads; t++)
            WaitForSingleObject(g_pool[(size_t)(t - 1)].hDone, INFINITE);
        if (mode == BudgetMode::TIME) { g_search_deadline = 0; t_search_end = clock(); }
        g_tt_node_dag = prev_dag;
        // Merge by summed visits; avg = visit-weighted mean across workers.
        struct Agg { Move mv; long long visits = 0; double avgw = 0.0; };
        vector<Agg> agg;
        long long total_iters = 0, total_nodes = 0;
        for (auto& w : works) {
            total_iters += w.iterations; total_nodes += w.total_nodes;
            for (auto& c : w.children) {
                size_t k = 0;
                while (k < agg.size() && !(agg[k].mv == c.mv)) k++;
                if (k == agg.size()) agg.push_back({ c.mv, 0, 0.0 });
                agg[k].visits += c.visits;
                agg[k].avgw += (double)c.visits * (double)c.avg;
            }
        }
        result.iterations = total_iters;
        result.total_nodes = total_nodes;
        result.chosen_move = Move();
        long long best_vis = -1;
        double best_avg = 0.5;
        for (auto& a : agg) {
            if (a.visits > best_vis) {
                best_vis = a.visits;
                result.chosen_move = a.mv;
                best_avg = a.visits > 0 ? a.avgw / (double)a.visits : 0.5;
            }
        }
        result.chosen_move_san = b.move_label(result.chosen_move);
        result.actual_eval_cp = works[0].root_static_value;
        result.actual_wdl_w = works[0].root_static_avg;
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
            bool isWhite = b._player;
            double eval_cont = isWhite ? best_avg : (1.0 - best_avg);
            eval_cont = std::clamp(eval_cont, 0.0, 1.0);
            if (move_score > 0.5) {
                result.score = eval_cont;
            } else {
                result.score = 0.0;
            }
            result.eval_score = eval_cont;
        }
        auto t1 = chrono::steady_clock::now();
        result.time_s = chrono::duration<double>(t1 - t0).count();
        if (mode == BudgetMode::TIME && t_search_end > t_search_start) {
            double overhead = (double)(result.time_s - (t_search_end - t_search_start) / (double)CLOCKS_PER_SEC);
            if (overhead >= 0.0 && overhead <= 3.0 * s_overhead_ema_s + 0.005)
                s_overhead_ema_s = 0.7 * s_overhead_ema_s + 0.3 * overhead;
        }
        return result;
    }

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
