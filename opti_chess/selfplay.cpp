#include "exploration.h"
#include "useful_functions.h"
#include "zobrist.h"
#include <iostream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Headless self-play match runner.
//
// Plays full games between two CONFIGURATIONS of this binary:
//   NEW = current defaults      (value propagation + trust prior + avg cap)
//   OLD = pre-audit behaviour   (all three toggles off)
// Colors alternate each game. Fresh search tree every move (unbiased, simple).
//
// Usage: opti_chess_selfplay [games] [iterations_per_move]
// ---------------------------------------------------------------------------

static void set_config(bool new_engine) {
	g_search_value_propagation = new_engine;
	g_search_trust_prior = new_engine;
	g_search_avg_cap = new_engine;
}

struct GameResult {
	bool new_won = false;
	bool old_won = false;
	bool draw = false;
	int plies = 0;
};

static GameResult play_game(const int iterations_per_move, const bool new_is_white, const int max_plies) {
	GameResult res;

	Board game_board;
	game_board.from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

	Evaluator evaluator;
	int new_big_eval_streak = 0;
	int old_big_eval_streak = 0;

	for (int ply = 0; ply < max_plies; ply++) {
		// Game over by the rules?
		game_board.get_moves();
		const int over = game_board.is_game_over(3);
		if (over != unterminated) {
			if (over == white_win) res.new_won = new_is_white;
			else if (over == black_win) res.new_won = !new_is_white;
			else res.draw = true;
			res.plies = ply;
			return res;
		}

		// Which configuration moves here?
		const bool mover_is_white = game_board.get_color() == 1;
		const bool mover_is_new = (new_is_white == mover_is_white);
		set_config(mover_is_new);

		// Fresh tree on the current position. NOTE: ~Node is a no-op - the tree
		// holds pool slots, so reset() must be called explicitly or the pools
		// exhaust after a couple of moves.
		Move best;
		int mover_eval = 0;
		{
			Board analysis_board;
			analysis_board.from_fen(game_board.to_fen());

			Node root(&analysis_board);
			root.grogros_zero(&monte_board_buffer, &evaluator, 0.00001, 5.0, 1.10, iterations_per_move, 8);

			best = root.get_most_explored_child_move();
			mover_eval = root._deep_evaluation._value * (mover_is_white ? 1 : -1);
			root.reset();
		}

		cout << "  [pool] ply=" << ply
			<< " free_boards=" << monte_board_buffer._free_indices.size()
			<< "/" << monte_board_buffer._length
			<< " free_nodes=" << monte_node_buffer._free_indices.size()
			<< "/" << monte_node_buffer._length
			<< " fen=" << game_board.to_fen() << endl;

		if (best.is_null_move()) {
			// Nothing explored (should not happen): call it a draw
			res.draw = true;
			res.plies = ply;
			return res;
		}

		// Resignation adjudication: a side holding >= +900 for 4 consecutive
		// own moves has converted the game (cuts hopeless endgames short)
		int& streak = mover_is_new ? new_big_eval_streak : old_big_eval_streak;
		if (mover_eval >= 900) streak++;
		else streak = 0;

		game_board.make_move(best, false, true);
		res.plies = ply + 1;

		if (streak >= 4) {
			if (mover_is_new) res.new_won = true;
			else res.old_won = true;
			return res;
		}
	}

	// Ply cap reached: draw by adjudication
	res.draw = true;
	return res;
}

int main(int argc, char* argv[]) {
	int games = 12;
	int iterations_per_move = 2000;
	if (argc > 1) games = std::max(2, atoi(argv[1]));
	if (argc > 2) iterations_per_move = std::max(200, atoi(argv[2]));

	// Optional disables for A/B bisection: -vp -tp -cap
	for (int i = 3; i < argc; i++) {
		const string a = argv[i];
		if (a == "-vp") g_search_value_propagation = false;
		else if (a == "-tp") g_search_trust_prior = false;
		else if (a == "-cap") g_search_avg_cap = false;
	}

	monte_board_buffer.init(60000, false);
	monte_node_buffer.init(120000, false);

	cout << "Self-play: NEW vs OLD | " << games << " games | "
		<< iterations_per_move << " iterations/move" << endl;

	int new_wins = 0, old_wins = 0, draws = 0;

	for (int g = 0; g < games; g++) {
		const bool new_is_white = (g % 2 == 0);
		transposition_table.clear();

		const GameResult r = play_game(iterations_per_move, new_is_white, 240);

		string outcome;
		if (r.new_won) { new_wins++; outcome = "NEW wins"; }
		else if (r.old_won) { old_wins++; outcome = "OLD wins"; }
		else { draws++; outcome = "draw"; }

		cout << "game " << g + 1 << "/" << games
			<< " | NEW " << (new_is_white ? "W" : "B")
			<< " | " << outcome << " in " << r.plies << " plies" << endl;
	}

	cout << "\n=== RESULT: NEW " << new_wins << " - " << old_wins << " OLD, "
		<< draws << " draws ===" << endl;
	const double score_new = new_wins + 0.5 * draws;
	const double total = new_wins + old_wins + draws;
	cout << "NEW score: " << score_new / total * 100.0 << "% (Elo delta ~ "
		<< (int)(400.0 * log10(max(0.01, score_new / max(1.0, total - score_new)))) << ")" << endl;

	return 0;
}
