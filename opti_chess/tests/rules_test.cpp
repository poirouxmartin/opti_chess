#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <gtest/gtest.h>
#include <cstdlib>
#include "board.h"
#include "zobrist.h"

// ============================================================================
// Rules: terminal-state detection (stalemate, checkmate, 50-move, threefold)
// Engine convention: game_over() returns unterminated=0, white_win=1, draw=2,
// black_win=3. When the side to move has no legal move and is in check, the
// OTHER side wins.
// ============================================================================

namespace {

// Finds a legal move by its start/end squares (rows/cols are 0-based, row 0 = rank 1)
bool find_move(Board& b, int sr, int sc, int er, int ec, Move& out) {
    b.get_moves();
    for (int i = 0; i < b._got_moves; i++) {
        const Move& m = b._moves[i];
        if (m.start_row == sr && m.start_col == sc && m.end_row == er && m.end_col == ec) {
            out = m;
            return true;
        }
    }
    return false;
}

// Plays a move by squares with history recording; returns false if not legal
bool play(Board& b, int sr, int sc, int er, int ec) {
    Move m;
    if (!find_move(b, sr, sc, er, ec, m))
        return false;
    b.make_move(m, false, true);
    return true;
}

// Checks incremental Zobrist == full recompute for every king move spanning two files (castling)
int check_castling_keys(const char* fen) {
    Board b;
    b.from_fen(fen);
    b.get_moves();

    int castles = 0;
    for (int i = 0; i < b._got_moves; i++) {
        const Move m = b._moves[i];
        const uint8_t p = b._array[m.start_row][m.start_col];
        if ((p != w_king && p != b_king) || std::abs(int(m.end_col) - int(m.start_col)) != 2)
            continue;
        castles++;

        Board inc(b);
        Board full(b);
        inc.make_move(m, false, true);
        full.make_move(m, false, false);
        full.get_zobrist_key();

        EXPECT_EQ(inc._zobrist_key, full._zobrist_key)
            << "Incremental Zobrist mismatch after castling " << i << " in " << fen;
    }
    return castles;
}

// Same check for the pawn capture onto the (empty) en passant square
bool check_ep_key(const char* fen, int sr, int sc, int er, int ec) {
    Board b;
    b.from_fen(fen);
    Move m;
    if (!find_move(b, sr, sc, er, ec, m))
        return false;

    Board inc(b);
    Board full(b);
    inc.make_move(m, false, true);
    full.make_move(m, false, false);
    full.get_zobrist_key();

    EXPECT_EQ(inc._zobrist_key, full._zobrist_key)
        << "Incremental Zobrist mismatch after en passant in " << fen;
    // The captured pawn must be gone
    EXPECT_EQ(inc._array[sr][ec], none);
    return true;
}

} // namespace

TEST(Rules, StalemateIsDraw) {
    Board b;
    b.from_fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    b.get_moves();
    EXPECT_EQ(b._got_moves, 0);
    EXPECT_FALSE(b.in_check());
    EXPECT_EQ(b.game_over(3), draw);
}

TEST(Rules, CheckmateIsDecisive) {
    // Black is mated -> white_win
    Board bm;
    bm.from_fen("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
    bm.get_moves();
    EXPECT_EQ(bm._got_moves, 0);
    int r = bm.game_over(3);
    EXPECT_NE(r, draw);
    EXPECT_NE(r, unterminated);
    EXPECT_EQ(r, white_win);

    // White is mated -> black_win
    Board wm;
    wm.from_fen("8/8/8/8/8/6k1/6q1/7K w - - 0 1");
    wm.get_moves();
    EXPECT_EQ(wm._got_moves, 0);
    r = wm.game_over(3);
    EXPECT_NE(r, draw);
    EXPECT_NE(r, unterminated);
    EXPECT_EQ(r, black_win);
}

TEST(Rules, FiftyMoveRule) {
    // FIDE: draw once 50 full moves (= 100 half-moves) without capture or pawn move.
    // Engine: max_half_moves = 100, draw when _half_moves_count >= 100 (same rule).
    Board at100;
    at100.from_fen("4k3/8/8/8/8/8/8/R3K3 w - - 100 1");
    EXPECT_EQ(at100.game_over(3), draw);

    Board at99;
    at99.from_fen("4k3/8/8/8/8/8/8/R3K3 w - - 99 1");
    EXPECT_EQ(at99.game_over(3), unterminated);
}

TEST(Rules, ThreefoldRepetitionInGame) {
    // No castling rights, no pawns: rook/king shuffles are reversible so history accumulates.
    // max_repetitions = 3 -> draw when the current position has occurred 3 times.
    Board b;
    b.from_fen("4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
    EXPECT_EQ(b.game_over(3), unterminated);

    for (int cycle = 0; cycle < 2; cycle++) {
        // Ra1-a2, Ke8-d8, Ra2-a1, Kd8-e8 returns to the start position
        ASSERT_TRUE(play(b, 0, 0, 1, 0));
        EXPECT_EQ(b.game_over(3), unterminated);
        ASSERT_TRUE(play(b, 7, 4, 7, 3));
        EXPECT_EQ(b.game_over(3), unterminated);
        ASSERT_TRUE(play(b, 1, 0, 0, 0));
        EXPECT_EQ(b.game_over(3), unterminated);
        ASSERT_TRUE(play(b, 7, 3, 7, 4));

        if (cycle == 0) {
            // 2nd occurrence: not yet a draw with max_repetitions = 3 (draw with 2)
            EXPECT_EQ(b.repetition_count(), 2);
            EXPECT_EQ(b.game_over(3), unterminated);
        }
        else {
            // 3rd occurrence -> draw
            EXPECT_EQ(b.repetition_count(), 3);
            EXPECT_EQ(b.game_over(3), draw);
        }
    }
}

TEST(Zobrist2, IncrementalMatchesFullAfterCastling) {
    EXPECT_GE(check_castling_keys("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"), 1);
    EXPECT_GE(check_castling_keys("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1"), 1);
}

TEST(Zobrist2, IncrementalMatchesFullAfterEnPassant) {
    // White e5xd6 e.p.
    EXPECT_TRUE(check_ep_key("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", 4, 4, 5, 3));
    // Black d4xe3 e.p.
    EXPECT_TRUE(check_ep_key("4k3/8/8/8/3pP3/8/8/4K3 b - e3 0 1", 3, 3, 2, 4));
}

TEST(Zobrist2, CastlingRightsAndEpChangeKey) {
    Board with_rights, without_rights;
    with_rights.from_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    without_rights.from_fen("r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1");
    EXPECT_NE(with_rights._zobrist_key, without_rights._zobrist_key);

    Board with_ep, without_ep;
    with_ep.from_fen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    without_ep.from_fen("4k3/8/8/3pP3/8/8/8/4K3 w - - 0 1");
    EXPECT_NE(with_ep._zobrist_key, without_ep._zobrist_key);
}
