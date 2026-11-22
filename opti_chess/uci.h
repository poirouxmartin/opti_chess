#pragma once

#include "board.h"
#include "puzzle.h"
#include <string>
#include <vector>

// Standard chess start position
extern const std::string UCI_START_FEN;

// UCI text of a move (e2e4); promotion suffix only for pawn moves to the last rank
std::string move_to_uci(const Board& b, const Move& m);

// Finds the legal move matching a UCI string; false if none
bool uci_to_move(Board& b, const std::string& s, Move& out);

// Applies "position startpos|fen ... [moves ...]" tokens to the current FEN and returns the new FEN.
// Bad FEN keeps current; an illegal move stops the move list. Diagnostics are appended to *info (no newline).
std::string uci_apply_position(const std::vector<std::string>& tok, const std::string& current, std::vector<std::string>* info = nullptr);

struct UciBudget {
	BudgetMode mode = BudgetMode::NODES;
	double budget = 2000.0;
};

// Search budget of a "go" command. Precedence: nodes > movetime > own wtime/btime
// (time/30 + inc/2, clamped to [50 ms, 10 s]) > infinite (20000 nodes) > default 2000 nodes. depth has no effect.
UciBudget uci_go_budget(const std::vector<std::string>& tok, bool white_to_move);

// UCI score of a white-POV eval from the side to move: "cp <n>", or "mate <n>" for mate scores
std::string uci_score(const Board& b, int white_eval);
