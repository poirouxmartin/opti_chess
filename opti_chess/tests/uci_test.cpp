#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <gtest/gtest.h>
#include "board.h"
#include "puzzle.h"
#include "uci.h"
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace {

// Field n (space-separated) of a FEN
string fen_field(const string& fen, int n) {
    istringstream iss(fen);
    string f;
    for (int i = 0; i <= n; i++) iss >> f;
    return f;
}

vector<string> toks(initializer_list<const char*> l) {
    return vector<string>(l.begin(), l.end());
}

}

TEST(Uci, MoveToUciBasics) {
    Board b;
    b.from_fen(UCI_START_FEN);
    Move m;
    ASSERT_TRUE(uci_to_move(b, "e2e4", m));
    EXPECT_EQ(move_to_uci(b, m), "e2e4");
    ASSERT_TRUE(uci_to_move(b, "g1f3", m));
    EXPECT_EQ(move_to_uci(b, m), "g1f3");
    EXPECT_FALSE(uci_to_move(b, "e2e5", m));
    EXPECT_FALSE(uci_to_move(b, "zz99", m));
}

TEST(Uci, RoundTripAllLegalMoves) {
    const char* fens[] = {
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1",
    };
    for (int f = 0; f < 2; f++) {
        Board b;
        b.from_fen(fens[f]);
        b.get_moves();
        vector<Move> moves(b._moves, b._moves + b._got_moves);
        set<string> seen;
        for (const Move& legal : moves) {
            string text = move_to_uci(b, legal);
            EXPECT_TRUE(seen.insert(text).second) << "duplicate " << text;
            Move back;
            ASSERT_TRUE(uci_to_move(b, text, back)) << text;
            EXPECT_EQ(move_to_uci(b, back), text);
            EXPECT_EQ(back.start_row, legal.start_row);
            EXPECT_EQ(back.start_col, legal.start_col);
            EXPECT_EQ(back.end_row, legal.end_row);
            EXPECT_EQ(back.end_col, legal.end_col);
        }
        if (f == 0) {
            EXPECT_TRUE(seen.count("e1g1"));
            EXPECT_TRUE(seen.count("e1c1"));
        } else {
            // Black pawns on f2/g2/h2 promote on rank 1: at least one pawn move has all four suffixes
            int best = 0;
            for (const char* base : {"g2g1", "g2f1", "g2h1", "h2h1", "h2g1", "f2f1", "f2g1", "f2e1"}) {
                int n = 0;
                for (char s : string("qrbn")) n += (int)seen.count(string(base) + s);
                if (n > best) best = n;
            }
            EXPECT_EQ(best, 4);
        }
    }
}

TEST(Uci, ApplyPositionStartposMoves) {
    string fen = uci_apply_position(toks({"position", "startpos", "moves", "e2e4", "e7e5", "g1f3"}), UCI_START_FEN);
    EXPECT_EQ(fen_field(fen, 0), "rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R");
    EXPECT_EQ(fen_field(fen, 1), "b");
}

TEST(Uci, ApplyPositionStopsAtIllegalMove) {
    vector<string> info;
    string fen = uci_apply_position(toks({"position", "startpos", "moves", "e2e4", "e2e4"}), UCI_START_FEN, &info);
    EXPECT_EQ(fen_field(fen, 0), "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR");
    ASSERT_EQ(info.size(), 1u);
    EXPECT_EQ(info[0], "info string illegal move e2e4");
}

TEST(Uci, ApplyPositionBadFenKeepsCurrent) {
    vector<string> info;
    EXPECT_EQ(uci_apply_position(toks({"position", "fen", "garbage"}), UCI_START_FEN, &info), UCI_START_FEN);
    ASSERT_EQ(info.size(), 1u);
    EXPECT_EQ(info[0], "info string bad fen");

    info.clear();
    EXPECT_EQ(uci_apply_position(toks({"position", "foo"}), UCI_START_FEN, &info), UCI_START_FEN);
    EXPECT_TRUE(info.empty());
}

TEST(Uci, ApplyPositionFenWithMoves) {
    string fen = uci_apply_position(
        toks({"position", "fen", "4k3/8/8/8/8/8/8/4K2R", "w", "K", "-", "0", "1", "moves", "e1g1"}), UCI_START_FEN);
    EXPECT_EQ(fen_field(fen, 0), "4k3/8/8/8/8/8/8/5RK1");
}

TEST(Uci, GoBudgetPrecedence) {
    UciBudget u = uci_go_budget(toks({"go", "nodes", "500", "movetime", "100"}), true);
    EXPECT_EQ(u.mode, BudgetMode::NODES);
    EXPECT_DOUBLE_EQ(u.budget, 500.0);

    u = uci_go_budget(toks({"go", "movetime", "250"}), true);
    EXPECT_EQ(u.mode, BudgetMode::TIME);
    EXPECT_NEAR(u.budget, 0.25, 1e-9);

    auto clock = toks({"go", "wtime", "60000", "btime", "1000", "winc", "1000", "binc", "0"});
    u = uci_go_budget(clock, true);
    EXPECT_EQ(u.mode, BudgetMode::TIME);
    EXPECT_NEAR(u.budget, 2.5, 1e-9);
    u = uci_go_budget(clock, false);
    EXPECT_EQ(u.mode, BudgetMode::TIME);
    EXPECT_NEAR(u.budget, 0.05, 1e-9);

    u = uci_go_budget(toks({"go", "wtime", "3600000"}), true);
    EXPECT_EQ(u.mode, BudgetMode::TIME);
    EXPECT_NEAR(u.budget, 10.0, 1e-9);

    u = uci_go_budget(toks({"go", "infinite"}), true);
    EXPECT_EQ(u.mode, BudgetMode::NODES);
    EXPECT_DOUBLE_EQ(u.budget, 20000.0);

    u = uci_go_budget(toks({"go"}), true);
    EXPECT_EQ(u.mode, BudgetMode::NODES);
    EXPECT_DOUBLE_EQ(u.budget, 2000.0);

    u = uci_go_budget(toks({"go", "depth", "12"}), true);
    EXPECT_EQ(u.mode, BudgetMode::NODES);
    EXPECT_DOUBLE_EQ(u.budget, 2000.0);

    u = uci_go_budget(toks({"go", "nodes", "abc"}), true);
    EXPECT_EQ(u.mode, BudgetMode::NODES);
    EXPECT_DOUBLE_EQ(u.budget, 2000.0);
}

TEST(Uci, ScoreCpAndMate) {
    Board w;
    w.from_fen("r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4");
    EXPECT_EQ(uci_score(w, 48), "cp 48");
    EXPECT_EQ(uci_score(w, 99600000), "mate 1");

    Board bl;
    bl.from_fen("r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 4 4");
    EXPECT_EQ(uci_score(bl, 48), "cp -48");
}
