#include "exploration.h"
#include "useful_functions.h"
#include "zobrist.h"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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
// Usage: opti_chess_selfplay [games] [iterations_per_move] [options]
//   --modes=NEW,OLD   search mode of each side, one of tree | dag (exact = alias of dag)
//                     tree = g_tt_node_dag off
//                     dag  = g_tt_node_dag on (exact game values, the only DAG behaviour)
//                     With --modes both sides get the default toggles (all on),
//                     then the mover's mode is applied before each search.
//                     Incompatible with --persist.
//   --openings=N      play N random openings of 8 plies (RNG seeded by --seed),
//                     each played twice with colors swapped; games becomes 2N
//   --seed=S          seed of the opening RNG (default 1)
// ---------------------------------------------------------------------------

// CLI A/B disables, re-applied after every set_config() call
static bool cli_disable_value_propagation = false;
static bool cli_disable_trust_prior = false;
static bool cli_disable_avg_cap = false;

// Search parameters for NEW and OLD sides
static double new_alpha = 0.00001, new_beta = 5.0, new_gamma = 1.10;
static double old_alpha = 0.00001, old_beta = 5.0, old_gamma = 1.10;

// --modes=NEW,OLD: search mode per side (used only when modes_given)
enum SearchMode { MODE_TREE, MODE_DAG };
static bool modes_given = false;
static SearchMode new_mode = MODE_TREE;
static SearchMode old_mode = MODE_TREE;

static const char* mode_name(const SearchMode m) {
	return m == MODE_DAG ? "dag" : "tree";
}

static bool parse_mode_name(const string& s, SearchMode& out) {
	if (s == "tree") out = MODE_TREE;
	else if (s == "dag" || s == "exact") out = MODE_DAG; // exact: legacy alias
	else return false;
	return true;
}

// Wall-clock time spent inside grogros_zero, per side (for the avg ms/move line)
static double new_search_ms = 0.0, old_search_ms = 0.0;
static long long new_searches = 0, old_searches = 0;

static void account_search(const bool is_new, const chrono::steady_clock::time_point t0) {
	const double ms = chrono::duration<double, std::milli>(chrono::steady_clock::now() - t0).count();
	if (is_new) { new_search_ms += ms; new_searches++; }
	else { old_search_ms += ms; old_searches++; }
}

// Standard start position
static const string standard_start_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

static void set_config(bool new_engine) {
	if (modes_given) {
		// Both sides get the default toggles; the mover's mode below picks the DAG flags
		g_search_value_propagation = true;
		g_search_trust_prior = true;
		g_search_avg_cap = true;
	}
	else {
		g_search_value_propagation = new_engine;
		g_search_trust_prior = new_engine;
		g_search_avg_cap = new_engine;
	}

	// Re-apply the CLI disables: set_config() runs EVERY PLY and would
	// otherwise stomp them back to true, silently turning "-vp/-tp/-cap"
	// into no-ops (the first bisection matches replayed the baseline).
	if (cli_disable_value_propagation) g_search_value_propagation = false;
	if (cli_disable_trust_prior) g_search_trust_prior = false;
	if (cli_disable_avg_cap) g_search_avg_cap = false;

	if (modes_given) {
		const SearchMode m = new_engine ? new_mode : old_mode;
		g_tt_node_dag = (m != MODE_TREE);
	}
}

struct GameResult {
	bool new_won = false;
	bool old_won = false;
	bool draw = false;
	int plies = 0;
	const char* reason = ""; // rules | resign | cap | null
};

// Builds count random openings of 8 plies from the start position. The RNG is a
// local xorshift64 seeded with seed. A line that ends (game over or no legal move)
// before 8 plies is discarded and the RNG keeps going.
static vector<string> build_openings(const int count, const uint64_t seed) {
	vector<string> fens;
	uint64_t state = seed != 0 ? seed : 1; // xorshift must not start at 0
	auto next_rand = [&state]() {
		state ^= state << 13;
		state ^= state >> 7;
		state ^= state << 17;
		return state;
	};

	while ((int)fens.size() < count) {
		Board b;
		b.from_fen(standard_start_fen);
		bool ok = true;
		for (int ply = 0; ply < 8; ply++) {
			b.get_moves();
			if (b._got_moves <= 0 || b.is_game_over(3) != unterminated) {
				ok = false;
				break;
			}
			const Move m = b._moves[next_rand() % static_cast<uint64_t>(b._got_moves)];
			b.make_move(m, false, true);
		}
		if (ok) fens.push_back(b.to_fen());
	}
	return fens;
}

static GameResult play_game(const int iterations_per_move, const bool new_is_white, const int max_plies, const string& start_fen) {
	GameResult res;

	Board game_board;
	game_board.from_fen(start_fen);

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
			res.reason = "rules";
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
			const double a = mover_is_new ? new_alpha : old_alpha;
			const double b = mover_is_new ? new_beta : old_beta;
			const double g = mover_is_new ? new_gamma : old_gamma;
			node_map.clear(); // fresh DAG map for each search (harmless in tree mode)
			const auto t0 = chrono::steady_clock::now();
			root.grogros_zero(&monte_board_buffer, &evaluator, a, b, g, iterations_per_move, 8);
			account_search(mover_is_new, t0);

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
			res.reason = "null";
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
			res.reason = "resign";
			return res;
		}
	}

	// Ply cap reached: draw by adjudication
	res.draw = true;
	res.reason = "cap";
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

static GameResult play_game_persistent(const int iterations_per_move, const bool new_is_white, const int max_plies, const string& start_fen) {
	GameResult res;

	Board game_board;
	game_board.from_fen(start_fen);

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
			res.reason = "rules";
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

		const double a = mover_is_new ? new_alpha : old_alpha;
		const double b = mover_is_new ? new_beta : old_beta;
		const double g = mover_is_new ? new_gamma : old_gamma;
		const auto t0 = chrono::steady_clock::now();
		mover.root->grogros_zero(&monte_board_buffer, &evaluator, a, b, g, iterations_per_move, 8);
		account_search(mover_is_new, t0);

		Move best = mover.root->get_most_explored_child_move();
		const int mover_eval = mover.root->_deep_evaluation._value * (mover_is_white ? 1 : -1);

		if (best.is_null_move()) {
			res.draw = true;
			res.plies = ply;
			res.reason = "null";
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
			res.reason = "resign";
			return res;
		}
	}

	res.draw = true;
	res.reason = "cap";
	return res;
}

// Elo of a score fraction x: -400 log10(1/x - 1). x is clamped into [0.001, 0.999]
// so that the bounds and the point estimate stay finite
static double elo_of(double x) {
	x = min(0.999, max(0.001, x));
	return -400.0 * log10(1.0 / x - 1.0);
}

int main(int argc, char* argv[]) {
	// Same fixed seed as the CLI (cli.cpp main) so Zobrist keys are deterministic
	if (!getenv("OPTI_SEED")) {
#ifdef _WIN32
		_putenv_s("OPTI_SEED", "42");
#else
		setenv("OPTI_SEED", "42", 1);
#endif
	}

	int games = 12;
	int iterations_per_move = 2000;
	bool persist_trees = false;
	int openings_n = 0;
	uint64_t opening_seed = 1;
	if (argc > 1) games = std::max(2, atoi(argv[1]));
	if (argc > 2) iterations_per_move = std::max(200, atoi(argv[2]));

	// Optional disables for A/B bisection: -vp -tp -cap, and --persist for
	// GUI-like kept trees. Search params: --new-alpha=X --old-beta=Y etc.
	// Modes and openings: --modes=NEW,OLD --openings=N --seed=S
	for (int i = 3; i < argc; i++) {
		const string a = argv[i];
		// Prefix match with the literal's own length (hand-counted substr lengths
		// silently disabled the beta/gamma flags)
		auto value_of = [&a](const string& prefix, double& out) {
			if (a.rfind(prefix, 0) != 0) return false;
			out = strtod(a.c_str() + prefix.size(), nullptr);
			return true;
		};
		if (a == "-vp") { cli_disable_value_propagation = true; g_search_value_propagation = false; }
		else if (a == "-tp") { cli_disable_trust_prior = true; g_search_trust_prior = false; }
		else if (a == "-cap") { cli_disable_avg_cap = true; g_search_avg_cap = false; }
		else if (a == "--persist") persist_trees = true;
		else if (a.rfind("--modes=", 0) == 0) {
			const string spec = a.substr(8);
			const size_t comma = spec.find(',');
			if (comma == string::npos
				|| !parse_mode_name(spec.substr(0, comma), new_mode)
				|| !parse_mode_name(spec.substr(comma + 1), old_mode)) {
				cerr << "bad --modes value (expected NEW,OLD with tree|dag):" << a << endl;
				return 2;
			}
			modes_given = true;
		}
		else if (a.rfind("--openings=", 0) == 0) openings_n = max(0, atoi(a.c_str() + 11));
		else if (a.rfind("--seed=", 0) == 0) opening_seed = strtoull(a.c_str() + 7, nullptr, 10);
		else if (value_of("--new-alpha=", new_alpha) || value_of("--new-beta=", new_beta) || value_of("--new-gamma=", new_gamma)
			|| value_of("--old-alpha=", old_alpha) || value_of("--old-beta=", old_beta) || value_of("--old-gamma=", old_gamma)) {}
		else cerr << "unknown argument: " << a << endl;
	}

	if (persist_trees && modes_given) {
		cerr << "error: --modes= cannot be combined with --persist" << endl;
		return 2;
	}

	// Random openings: N of them, each played twice (colors swapped)
	vector<string> openings;
	if (openings_n > 0) {
		openings = build_openings(openings_n, opening_seed);
		games = 2 * openings_n;
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
		<< (persist_trees ? " | PERSISTENT TREES" : " | fresh trees");
	if (modes_given) cout << " | NEW mode=" << mode_name(new_mode) << " OLD mode=" << mode_name(old_mode);
	cout << endl;
	cout << "  NEW: alpha=" << new_alpha << " beta=" << new_beta << " gamma=" << new_gamma << endl;
	cout << "  OLD: alpha=" << old_alpha << " beta=" << old_beta << " gamma=" << old_gamma << endl;

	int new_wins = 0, old_wins = 0, draws = 0;

	for (int g = 0; g < games; g++) {
		const bool new_is_white = (g % 2 == 0);
		transposition_table.clear();

		const string start_fen = openings.empty() ? standard_start_fen : openings[g / 2];

		const GameResult r = persist_trees
			? play_game_persistent(iterations_per_move, new_is_white, 240, start_fen)
			: play_game(iterations_per_move, new_is_white, 240, start_fen);

		string outcome;
		if (r.new_won) { new_wins++; outcome = "NEW wins"; }
		else if (r.old_won) { old_wins++; outcome = "OLD wins"; }
		else { draws++; outcome = "draw"; }

		cout << "game " << g + 1 << "/" << games
			<< " | NEW " << (new_is_white ? "W" : "B")
			<< " | " << outcome << " in " << r.plies << " plies";
		if (!openings.empty()) cout << " | opening " << g / 2;
		cout << " | " << r.reason << endl;
	}

	cout << "\n=== RESULT: NEW " << new_wins << " - " << old_wins << " OLD, "
		<< draws << " draws ===" << endl;
	const double score_new = new_wins + 0.5 * draws;
	const double total = new_wins + old_wins + draws;
	cout << "NEW score: " << score_new / total * 100.0 << "% (Elo delta ~ "
		<< (int)(400.0 * log10(max(0.01, score_new / max(1.0, total - score_new)))) << ")" << endl;

	// 95% error margin of the Elo delta: per-game variance, normal approximation
	const double s = score_new / total;
	const double var = (new_wins * (1.0 - s) * (1.0 - s) + draws * (0.5 - s) * (0.5 - s) + old_wins * s * s) / total;
	const double se = sqrt(var / total);
	cout << "Elo = " << elo_of(s) << " [" << elo_of(s - 1.96 * se) << ", " << elo_of(s + 1.96 * se) << "] (95%)" << endl;

	cout << "NEW avg ms/move = " << (new_searches > 0 ? new_search_ms / new_searches : 0.0)
		<< " | OLD avg ms/move = " << (old_searches > 0 ? old_search_ms / old_searches : 0.0) << endl;

	return 0;
}
