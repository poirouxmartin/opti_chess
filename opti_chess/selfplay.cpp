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

// CLI A/B disables, re-applied after every set_config() call
static bool cli_disable_value_propagation = false;
static bool cli_disable_trust_prior = false;
static bool cli_disable_avg_cap = false;

static void set_config(bool new_engine) {
	g_search_value_propagation = new_engine;
	g_search_trust_prior = new_engine;
	g_search_avg_cap = new_engine;

	// Re-apply the CLI disables: set_config() runs EVERY PLY and would
	// otherwise stomp them back to true, silently turning "-vp/-tp/-cap"
	// into no-ops (the first bisection matches replayed the baseline).
	if (cli_disable_value_propagation) g_search_value_propagation = false;
	if (cli_disable_trust_prior) g_search_trust_prior = false;
	if (cli_disable_avg_cap) g_search_avg_cap = false;
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

// ---------------------------------------------------------------------------
// PERSISTENT-TREE mode (--persist): each side owns a search tree that FOLLOWS
// the game, descending into the played child like the GUI does. This is the
// engine's real operating condition (the GUI keeps its tree between moves);
// fresh-tree mode above is the unbiased-but-different proxy.
// ---------------------------------------------------------------------------

struct SideEngine {
	Node* root = nullptr;
	Board* board = nullptr; // heap-owned, outside the pools
};

// (Re)create a side's tree at the given position
static void side_rebuild(SideEngine& se, const string& fen) {
	if (se.root != nullptr) {
		se.root->reset();
		recycle_detached_node(se.root);
		se.root = nullptr;
	}
	if (se.board == nullptr)
		se.board = new Board();
	se.root = monte_node_buffer.get_first_free_node();
	se.root->_board = se.board;
	se.root->_is_active = true;
	se.board->from_fen(fen);
}

// Descend into the played child when it exists (GUI play_move_keep core),
// recycle the discarded siblings, rebuild from scratch otherwise
static void side_advance(SideEngine& se, const Move& move, const string& fen_after) {
	if (se.root->children_count() > 0 && se.root->_children.contains(move)) {
		Node* next_root = se.root->_children[move]._node;
		Node* const old_root = se.root;

		for (auto const& [m, child_link] : old_root->_children) {
			if (!(m == move)) {
				Node* child = child_link._node;
				child->_parent_count--;
				if (child->_parent_count <= 0) {
					child->reset(true);
					recycle_detached_node(child);
				}
			}
		}

		old_root->_board->reset_board();
		old_root->reset(false);
		next_root->_parent_count--;
		se.root = next_root;
		recycle_detached_node(old_root);
	}
	else {
		side_rebuild(se, fen_after);
	}
}

static GameResult play_game_persistent(const int iterations_per_move, const bool new_is_white, const int max_plies) {
	GameResult res;

	Board game_board;
	game_board.from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

	Evaluator evaluator;
	int new_big_eval_streak = 0;
	int old_big_eval_streak = 0;

	SideEngine white_side, black_side;
	side_rebuild(white_side, game_board.to_fen());
	side_rebuild(black_side, game_board.to_fen());

	for (int ply = 0; ply < max_plies; ply++) {
		game_board.get_moves();
		const int over = game_board.is_game_over(3);
		if (over != unterminated) {
			if (over == white_win) res.new_won = new_is_white;
			else if (over == black_win) res.new_won = !new_is_white;
			else res.draw = true;
			res.plies = ply;
			return res;
		}

		const bool mover_is_white = game_board.get_color() == 1;
		const bool mover_is_new = (new_is_white == mover_is_white);
		set_config(mover_is_new);

		SideEngine& mover = mover_is_white ? white_side : black_side;
		const string fen_now = game_board.to_fen();

		// Safety net: the tree must stand on the current position (it should,
		// by construction of the descent below)
		if (mover.root->_board->to_fen() != fen_now)
			side_rebuild(mover, fen_now);

		mover.root->grogros_zero(&monte_board_buffer, &evaluator, 0.00001, 5.0, 1.10, iterations_per_move, 8);

		Move best = mover.root->get_most_explored_child_move();
		const int mover_eval = mover.root->_deep_evaluation._value * (mover_is_white ? 1 : -1);

		if (best.is_null_move()) {
			res.draw = true;
			res.plies = ply;
			return res;
		}

		int& streak = mover_is_new ? new_big_eval_streak : old_big_eval_streak;
		if (mover_eval >= 900) streak++;
		else streak = 0;

		game_board.make_move(best, false, true);
		res.plies = ply + 1;

		const string fen_after = game_board.to_fen();
		side_advance(mover, best, fen_after);
		side_advance(mover_is_white ? black_side : white_side, best, fen_after);

		// Pool pressure: hard-rebuild both sides before exhaustion
		if (monte_node_buffer._free_indices.size() < monte_node_buffer._length / 5
			|| monte_board_buffer._free_indices.size() < monte_board_buffer._length / 5) {
			side_rebuild(white_side, fen_after);
			side_rebuild(black_side, fen_after);
		}

		if (streak >= 4) {
			if (mover_is_new) res.new_won = true;
			else res.old_won = true;
			return res;
		}
	}

	res.draw = true;
	return res;
}

int main(int argc, char* argv[]) {
	int games = 12;
	int iterations_per_move = 2000;
	bool persist_trees = false;
	if (argc > 1) games = std::max(2, atoi(argv[1]));
	if (argc > 2) iterations_per_move = std::max(200, atoi(argv[2]));

	// Optional disables for A/B bisection: -vp -tp -cap, and --persist for
	// GUI-like kept trees
	for (int i = 3; i < argc; i++) {
		const string a = argv[i];
		if (a == "-vp") { cli_disable_value_propagation = true; g_search_value_propagation = false; }
		else if (a == "-tp") { cli_disable_trust_prior = true; g_search_trust_prior = false; }
		else if (a == "-cap") { cli_disable_avg_cap = true; g_search_avg_cap = false; }
		else if (a == "--persist") persist_trees = true;
	}

	if (persist_trees) {
		// Two live trees grow across plies: double the pools
		monte_board_buffer.init(150000, false);
		monte_node_buffer.init(300000, false);
	}
	else {
		monte_board_buffer.init(60000, false);
		monte_node_buffer.init(120000, false);
	}

	cout << "Self-play: NEW vs OLD | " << games << " games | "
		<< iterations_per_move << " iterations/move"
		<< (persist_trees ? " | PERSISTENT TREES" : " | fresh trees") << endl;

	int new_wins = 0, old_wins = 0, draws = 0;

	for (int g = 0; g < games; g++) {
		const bool new_is_white = (g % 2 == 0);
		transposition_table.clear();

		const GameResult r = persist_trees
			? play_game_persistent(iterations_per_move, new_is_white, 240)
			: play_game(iterations_per_move, new_is_white, 240);

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
