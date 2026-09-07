#pragma once

#include <string>
#include <memory>

#pragma once

#include <string>
#include <memory>

struct StockfishImpl;

class StockfishAdapter {
    std::unique_ptr<StockfishImpl> _impl;

public:
    StockfishAdapter();
    explicit StockfishAdapter(const std::string& exe_path);
    ~StockfishAdapter();

    StockfishAdapter(StockfishAdapter&&) noexcept;
    StockfishAdapter& operator=(StockfishAdapter&&) noexcept;

    bool is_available() const;

    void send(const std::string& cmd);
    std::string read_until(const std::string& token, int timeout_ms = 10000);

    struct AnalysisResult {
        std::string best_move;
        int eval_cp = 0;
        int depth = 0;
        bool is_mate = false;
        int mate_in = 0;
    };

    AnalysisResult analyze(const std::string& fen, int depth = 20);
    AnalysisResult analyze_with_move(const std::string& fen, const std::string& move_uci, int depth = 20);

    // Raw NNUE static eval (no search) via the `eval` command, white-relative
    // cp. Apples-to-apples truth for tuning a static evaluation function.
    struct StaticResult {
        int eval_cp = 0;
        bool ok = false;
        bool in_check = false; // side to move in check: SF prints no static eval
    };
    StaticResult static_eval(const std::string& fen);
};
