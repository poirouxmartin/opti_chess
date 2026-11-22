#include "uci.h"
#include <cstdlib>

using namespace std;

const string UCI_START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Square names: file a-h, rank 1-8; promotion suffix only for pawn moves to last rank
string move_to_uci(const Board& b, const Move& m) {
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
bool uci_to_move(Board& b, const string& s, Move& out) {
    b.get_moves();
    for (int i = 0; i < b._got_moves; i++) {
        if (move_to_uci(b, b._moves[i]) == s) {
            out = b._moves[i];
            return true;
        }
    }
    return false;
}

// Applies "position" tokens to a FEN, returns the resulting FEN
string uci_apply_position(const vector<string>& tok, const string& cur, vector<string>* info) {
    if (tok.size() < 2) return cur;
    string fen;
    size_t i = 2;
    if (tok[1] == "startpos") {
        fen = UCI_START_FEN;
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
        if (info) info->push_back("info string bad fen");
        return cur;
    }
    if (i < tok.size() && tok[i] == "moves") {
        for (size_t j = i + 1; j < tok.size(); j++) {
            Move m;
            if (!uci_to_move(b, tok[j], m)) {
                if (info) info->push_back("info string illegal move " + tok[j]);
                break;
            }
            b.make_move(m, false, true);
        }
    }
    return b.to_fen();
}

UciBudget uci_go_budget(const vector<string>& tok, bool white) {
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

    long long mytime = white ? wtime : btime;
    long long myinc = white ? winc : binc;

    UciBudget ub;
    if (nodes > 0) {
        ub.budget = (double)nodes;
    } else if (movetime > 0) {
        ub.mode = BudgetMode::TIME;
        ub.budget = movetime / 1000.0;
    } else if (mytime > 0) {
        ub.mode = BudgetMode::TIME;
        ub.budget = (mytime / 30.0 + myinc / 2.0) / 1000.0;
        if (ub.budget < 0.05) ub.budget = 0.05;
        if (ub.budget > 10.0) ub.budget = 10.0;
    } else if (infinite) {
        ub.budget = 20000.0;
    } else if (depth > 0) {
        ub.budget = 2000.0;
    }
    return ub;
}

string uci_score(const Board& b, int white_eval) {
    if (int mate = b.is_eval_mate(white_eval); mate != 0)
        return "mate " + to_string(b._player ? mate : -mate);
    return "cp " + to_string(b._player ? white_eval : -white_eval);
}
