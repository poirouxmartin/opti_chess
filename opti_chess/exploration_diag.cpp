#include "exploration.h"
#include "useful_functions.h"
#include "zobrist.h"
#include <cmath>

#ifdef _WIN32
// Diagnostic-only SEH probe: walks _children without dying if the map memory
// was stomped by an upstream corruption. Returns -1 when the walk faults.
static int probe_fully_explored_count(const Node* n) {
	__try {
		int c = 0;
		for (auto const& [_, cl] : n->_children)
			if (cl._node != nullptr && cl._node->_fully_explored)
				c++;
		return c;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
#endif

// Search feature toggles (see exploration.h) - self-play A/B testing
// Search feature toggles. DEFAULTS = legacy behaviour: A/B selfplay matches
// (fresh AND persistent trees, 8-12 games x 2000 iters) scored every
// combination of these features BELOW legacy - best combo 37.5%, worst 0%
// (b1b54ac data). Re-enable individually after a winning match only.
bool g_search_value_propagation = false;
bool g_search_trust_prior = false;
bool g_search_avg_cap = false;

namespace {

constexpr uint8_t search_repetition_limit = 2;

// #11 Plan B - DISPLAY cutoff threshold (get_exploration_variants /
// get_main_depth), decoupled from search pruning. search_repetition_limit
// (twofold, aggressive) would truncate the PV at the 2nd occurrence -> in
// endgames king manoeuvres loop back within ~2-3 moves and the main line is
// cut very early even when the real line is long. Display runs up to the
// REAL draw (threefold = FIDE rule); still bounds genuine cycles well
// before the max_depth=500 cap. Applied under DAG only (see usage).
constexpr uint8_t display_repetition_limit = 3;

// #3: a mate score encodes its distance via _moves_count (board.cpp:1569),
// but _moves_count is ABSENT from the Zobrist key. Without normalisation, the
// same position reached by a path of different length reads back a wrong mate
// distance from the TT ("ghosts"). Canonise on store (drop the _moves_count
// anchor -> depends only on D = distance to mate, which is a property of the
// position); on probe, rebuild it using the _moves_count of the current node.
// Mate threshold = the is_eval_mate / #1 idiom (10*|e| > mate_value), robust:
// robuste : |stored| ~ mate_value(1e8) - Dï¿½mate_ply + mcï¿½mate_ply, mc/D petits.
inline int tt_normalize_mate(int eval, int moves_count) {
	if (10 * abs(eval) > mate_value)
		return eval + (eval > 0 ? 1 : -1) * moves_count * mate_ply;
	return eval;
}
inline int tt_denormalize_mate(int eval, int moves_count) {
	if (10 * abs(eval) > mate_value)
		return eval - (eval > 0 ? 1 : -1) * moves_count * mate_ply;
	return eval;
}

// #11 Plan A - scalar TT in the main search.
// QDEPTH_BAND: shifts every MCTS write-back depth above the quiescence band
// (quiescence depth <= _quiescence_depth, ~10). Depth-preferred replacement
// (zobrist.cpp:113) therefore always keeps a refined entry rather than a
// quiescence leaf, and the reuse gate can require a genuine refined subtree.
//
constexpr int QDEPTH_BAND = 256;
// MIN_REUSE_LOG2: only reuse entries representing >= 2^N refined nodes (never
// a bare quiescence leaf). Tunable at the gate.
constexpr int MIN_REUSE_LOG2 = 4;

// Integer log2 of (nodes+1), bounded. No float (hot-path friendly, MSVC).
inline int tt_writeback_depth(int nodes) {
	int v = (nodes > 0 ? nodes : 0) + 1; // Negative nodes = upstream bug: degrade cleanly (depth = QDEPTH_BAND)
	int log2v = 0;
	while (v > 1) { v >>= 1; ++log2v; }
	return QDEPTH_BAND + log2v;
}

// Keeps the derived fields of an Evaluation consistent when _value comes from
// the TT. EXACT factorisation of the quiescence cutoff block (#1 v2 / #14):
// _value is assumed already set (white-relative). Force _uncertainty=0 (a
// trustworthy TT value must not be filtered by static uncertainty), reset
// _winnable_* by sign on mate (avoids scaling a huge mate score; outside mate
// _winnable_* are kept, being a property of the position), then re-derive
// _wdl / _avg_score from _value.
inline void tt_fixup_derived(Evaluation& e) {
	e._uncertainty = 0.0f;
	if (10 * abs(e._value) > mate_value) {
		e._winnable_white = e._value > 0 ? 1.0f : 0.0f;
		e._winnable_black = e._value < 0 ? 1.0f : 0.0f;
	}
	e.get_WDL();
	e.get_average_score();
}

uint8_t position_history_count(const PositionHistory& path_history, Board& board) {
	// _zobrist_key is already maintained incrementally by make_move()
	const auto it = path_history.find(board._zobrist_key);
	return it == path_history.end() ? 0 : it->second;
}

} // namespace

// The repetition state is path-local: it depends on the exact move sequence that led to
// the current node, so it must stay outside Node/Board state if we want transpositions later.
void ensure_position_in_history(PositionHistory& path_history, Board& board) {
	// _zobrist_key is already maintained incrementally by make_move()
	path_history.try_emplace(board._zobrist_key, 1);
}

void record_position_in_history(PositionHistory& path_history, Board& board) {
	// _zobrist_key is already maintained incrementally by make_move()
	path_history[board._zobrist_key]++;
}

// #7 / Plan B-1 - undoes a path-history push (record). The key is passed
// directly (no Board): exact mirror of record, a plain lookup, and the entry
// is erased at 0 to bound the map size.
void unrecord_position_in_history(PositionHistory& path_history, uint64_t key) {
	const auto it = path_history.find(key);
	if (it == path_history.end()) {
		return; // unbalanced call: should never happen
	}
	if (it->second <= 1) {
		path_history.erase(it);
	}
	else {
		it.value()--;
	}
}

// RAII: records the board position on construction, pops it on destruction.
// The Zobrist key is captured AT CONSTRUCTION (get_zobrist_key is not
// idempotent: it recomputes in O(64)) - the dtor never touches the Board, so
// push/pop pairing is structural. Balance guaranteed on EVERY exit path
// (early returns included) - see the plan's "Balance invariant".
struct PathScope {
	PositionHistory& _history;
	uint64_t _key;
	PathScope(PositionHistory& history, Board& board) : _history(history) {
		// _zobrist_key is already maintained incrementally by make_move()
		_key = board._zobrist_key;
		_history[_key]++;
	}
	~PathScope() {
		unrecord_position_in_history(_history, _key);
	}
	PathScope(const PathScope&) = delete;
	PathScope& operator=(const PathScope&) = delete;
};

bool position_is_draw_by_repetition(const PositionHistory& path_history, Board& board, uint8_t repetition_limit = search_repetition_limit) {
	return position_history_count(path_history, board) + 1 >= repetition_limit;
}

void mark_position_as_draw(Board& board) {
	board._game_over_value = draw;
	board._game_over_checked = true;
}

void init_terminal_draw_child(Node* child, Board* board, Evaluator* eval, Network* network) {
	mark_position_as_draw(*board);

	child->_board = board;
	child->evaluate_position(eval, false, network, true);
	child->_fully_explored = true;
	child->_can_explore = false;
	child->_is_terminal = true;
}

// #11 Plan A - frozen leaf carrying a trustworthy value taken from the TT.
// Structurally modelled on init_terminal_draw_child. Terminal detection comes
// first: Board::evaluate(check_game_over=true) does not COMPUTE game over
// (is_game_over() is commented out there, board.cpp:1548); it only READS
// _game_over_value. So compute it explicitly the way init_node does
// (get_moves then is_game_over) before evaluating. If the position really is
// terminal, Board::evaluate has already set the exact evaluation (mate/draw),
// than the TT scalar: keep it and set _is_terminal=true. Otherwise replace
// _deep_evaluation with the TT value (white-relative) and consistent derived
// fields (#1/#14). Counters identical to the draw leaf (_nodes=1,
// _iterations=1): the UCT exploration term (see pick_random_child, the
// exploration term over _iterations) then behaves exactly as it does for
// existing terminal/draw leaves.
void init_tt_leaf_child(Node* child, Board* board, Evaluator* eval, Network* network, int white_relative_value) {
	child->_board = board;

	// Game over: Board::evaluate does not compute it (see above), so reproduce
	// the init_node sequence before evaluating.
	board->get_moves();
	board->is_game_over();

	child->evaluate_position(eval, false, network, true);

	if (board->_game_over_value != unterminated) {
		// Genuinely terminal: the exact evaluation (mate/draw) was already set by
		// Board::evaluate and beats the TT scalar - do not overwrite it.
		child->_is_terminal = true;
	}
	else {
		// _deep_evaluation == _static_evaluation here (evaluate_position copies it
		// when static_only=false); the explicit copy is kept on purpose: should that
		// default ever change, it is what guarantees a consistent base before
		// substituting the TT value and applying tt_fixup_derived (#1/#14).
		child->_deep_evaluation = child->_static_evaluation;
		child->_deep_evaluation._value = white_relative_value;
		child->_deep_evaluation._evaluated = true;
		tt_fixup_derived(child->_deep_evaluation);
	}

	child->_nodes = 1;
	child->_iterations = 1;
	child->_is_stand_pat_eval = false;
	child->_fully_explored = true;
	child->_can_explore = false;
	// Durable freeze: without _initialized=true, grogros_zero would rerun
	// quiescence (overwriting the TT value) if the leaf is revisited as
	// best_move. Combined with grogros_zero's early !_can_explore return,
	// the leaf stays genuinely frozen.
	child->_initialized = true;
}

// Default constructor
Node::Node() {
}

// Constructor taking a board
Node::Node(Board *board) {
	_board = board;
}

// Adds a child
void Node::add_child(Node* child, Move move) {
	// FIXME: check whether the move is already among the children?
	if (_children.contains(move)) {
		cout << "move already in children" << endl;
        return;
	}

	if (child == nullptr) {
		cout << "WHY DO WE ADD A NULL CHILD??" << endl;
		return;
	}

	if (move.is_null_move()) {
		cout << "null move added" << endl;
	}

	child->_parent_count++;
	ChildLink link;
	link._node = child;
	// Bug 2 model A (literal, decoupled per-edge): under DAG the edge counter
	// NEVER derives from the shared child->_nodes (which self-inflates in
	// cascade on a multi-parent node -> int overflow -> negative/billions/
	// random N). It starts at 0 and counts the iterations routed through
	// THIS edge. OFF: tree unchanged (= child->_nodes) -> byte-identical.
	link._propagated_nodes = g_tt_node_dag ? 0 : child->_nodes;
	_children[move] = link;
	//_nodes += child->_nodes;
}

// Returns the number of children
size_t Node::children_count() const {
	return _children.size();
}

// Returns the first move that has not been added yet
Move Node::get_first_unexplored_move(bool fully_explored) {
	for (int i = 0; i < _board->_got_moves; i++) {
		Move move = _board->_moves[i];
		if (!_children.contains(move) || (fully_explored && _children[move]._node != nullptr && !_children[move]._node->_fully_explored)) {
			return move;
		}
	}

	return Move();
}

// Initialises the node from its board
void Node::init_node() {

	if (_initialized) {
		return;
	}

	_initialized = true;

	if (_board == nullptr) {
		cout << "null board in init_node" << endl;
		return;
	}

	_board->get_moves();
	_board->is_game_over();

	// If the game is over
	if (_board->_game_over_value != unterminated) {
		_fully_explored = true;
		_can_explore = false;
		_is_terminal = true;

		return;
	}

	// Generates and sorts the moves
	_board->assign_all_move_flags();
	_board->sort_moves();
}

// #11 Plan B section 7 - anti-runaway guard. High constant (> any plausible
// real depth): fires ONLY on pathological non-repetition recursion
// (repetition is already cut by the Task 3 recheck).
// File-local (moteur mono-thread) ; defini avant grogros_zero (usage recursif).
static int g_dag_recursion_depth = 0;
constexpr int DAG_MAX_RECURSION_DEPTH = 1024;

// #11 Plan B - diagnostic counters (toggle-gated, cumulative since the last
// GUI reset). They exist to SHOW WHAT HAPPENS (spin? sharing? deep
// recursion?) instead of guessing. Read by dag_debug_report.
static long long g_dag_recheck_hits = 0;  // section 3: path-local repetitions cut
static long long g_dag_link_hits = 0;     // link-on-create: shared node reused
static long long g_dag_link_misses = 0;   // link-on-create: new node created
static long long g_dag_variant_cuts = 0;  // get_exploration_variants: lines cut on a cycle
static int g_dag_max_recursion_seen = 0;  // peak grogros_zero recursion depth

// Per-batch bounded detail (reset by dag_debug_report). The point is to SEE
// the first events (which move is shared, which cycle, which eval) without
// flooding the console or paying for to_fen() millions of times on the
// hottest path (perf #1). dag_dbg_take() returns true at most DAG_DBG_MAX
// times per batch; called only behind an already-true g_tt_node_dag guard.
static int g_dag_dbg_emitted = 0;
constexpr int DAG_DBG_MAX = 40;

// TT probe scale (audit A1): the MCTS Plan-A probe must only consume MCTS-scale
// entries, never quiescence ply-depths. Implemented in zobrist.cpp.
void tt_set_probe_scale(int scale);

// COMPILE-TIME diagnostic switch. false -> dag_dbg_take() returns false at
// compile time, so every `if (dag_dbg_take()) cout...` block (and their
// to_fen() calls) AND dag_debug_report() become eliminated dead code: zero
// search overhead. Switch back to true to re-instrument (e.g. Bug 1 opt 1
// work). The g_dag_* counters stay (1 increment, negligible, never printed
// imprimes quand off).
constexpr bool dag_debug = false;

static bool dag_dbg_take() {
	if constexpr (!dag_debug) return false;
	if (g_dag_dbg_emitted >= DAG_DBG_MAX) return false;
	g_dag_dbg_emitted++;
	return true;
}

void dag_debug_report() {
	if constexpr (!dag_debug) return; // zero overhead when diagnostics are off
	cout << "[DAG] node_map=" << node_map.size()
	     << " link_hit=" << g_dag_link_hits
	     << " link_miss=" << g_dag_link_misses
	     << " recheck=" << g_dag_recheck_hits
	     << " variant_cut=" << g_dag_variant_cuts
	     << " max_rec=" << g_dag_max_recursion_seen
	     << " (detail=" << g_dag_dbg_emitted << "/" << DAG_DBG_MAX << ")" << endl;
	g_dag_dbg_emitted = 0; // fresh detail window on the next batch
}

// Nouveau GrogrosZero
void Node::grogros_zero(BoardBuffer* board_buffer, Evaluator* eval, const double alpha, const double beta, const double gamma, int iterations, int quiescence_depth, Network* network, PositionHistory *path_history, const clock_t max_time) {
	// TODO:
	// Depth could be added
	// Keep the computation time

	// BUG: rn3rk1/pbppq1pp/1p2pb2/4N2Q/3PN3/3B4/PPP2PPP/R3K2R w KQ - 0 1
	// quiescence on it, then it hammers Rb1 after Qxh7... ??

	// FIXME: this should not happen
	if (iterations <= 0) {
		cout << "iterations <= 0 in grogros_zero" << endl;
		return;
	}

	// Recursion safety bound - ALWAYS ON (was DAG-only): refinement descents
	// recurse down existing tree edges, and a deep narrow line can exhaust the
	// main-thread stack (GUI crash: silent window death on the f6 analysis).
	// Repetition cuts normally fire long before this; the cap only catches
	// pathological non-repeating depth.
	constexpr int GROGROS_MAX_RECURSION_DEPTH = 96;
	if (g_dag_recursion_depth >= GROGROS_MAX_RECURSION_DEPTH) {
		return;
	}
	g_dag_recursion_depth += 1;
	if (g_dag_recursion_depth > g_dag_max_recursion_seen) {
		g_dag_max_recursion_seen = g_dag_recursion_depth;
	}
	struct DagRecGuard {
		~DagRecGuard() { g_dag_recursion_depth -= 1; }
	} _dag_rec_guard;

	// Computation time
	const clock_t begin_monte_time = clock();

	// #7 / B-1 - a single history owned at the root, threaded by pointer.
	// No more per-iteration clone: isolation between iterations is guaranteed
	// by the balanced push/pop (PathScope) in explore_new_move /
	// explore_random_child - every iteration restores the history to this state.
	PositionHistory local_path_history;
	PositionHistory* base_path_history = path_history != nullptr ? path_history : &local_path_history;
	ensure_position_in_history(*base_path_history, *_board);

	// INITIALISATION
	if (!_initialized) {
		quiescence(board_buffer, eval, quiescence_depth, alpha, beta, -INT32_MAX, INT32_MAX, network, true, 0, base_path_history);
		_iterations++;
	}

	// Nothing to do if the game is over
	if (_is_terminal) {
		_iterations++;
		_time_spent += clock() - begin_monte_time;

		return;
	}

	// #11 Plan A - frozen TT leaf: never re-evaluated, never expanded.
	// OFF-safe: every pre-existing _can_explore=false is also _is_terminal
	// (already caught above); only fires for the TT leaf (ON).
	if (!_can_explore) {
		_iterations++;
		_time_spent += clock() - begin_monte_time;

		return;
	}

	// FIXME: this should not happen
	if (_board->_got_moves <= 0) {
		cout << "no moves in grogros_zero" << endl;
		return;
	}

	// #11 Plan B - Bug 1 opt 3: per-traversal exclusion shared by ALL iterations
	// of THIS grogros_zero call (lives on this frame's stack only, never on a
	// shared node/edge). OFF: passed as nullptr, never consulted -> behaviour
	// byte-identical to the tree.
	DagExcl dag_excl;

	// Exploration
	int iteration_index = 0;
	while (iterations > 0) {

		// Wall-clock budget: stop cleanly when the deadline is reached
		// (max_time == 0 keeps the pure iteration-count behaviour)
		if (max_time != 0 && clock() - begin_monte_time >= max_time)
			break;

		// When the buffers are full we stop expanding and refine the existing tree.
		const bool can_expand = !monte_board_buffer.is_full() && !monte_node_buffer.is_full();

		// EXPLORING A NEW MOVE
#ifdef _WIN32
		const int fec = probe_fully_explored_count(this);
		if (fec < 0) {
			std::cerr << "[FEC-probe] SEH walking _children of node " << (void*)this
				<< " slot=" << (monte_node_buffer._init && monte_node_buffer._nodes != nullptr ? (long long)(this - monte_node_buffer._nodes) : -999999LL)
				<< " size=" << children_count() << " got_moves=" << (int)_board->_got_moves
				<< " fen=" << (_board != nullptr ? _board->to_fen() : string("<null>")) << std::endl;
			const unsigned char* p = reinterpret_cast<const unsigned char*>(this);
			std::cerr << std::hex;
			for (size_t off = 0; off < sizeof(Node); off += 16) {
				std::cerr << "  +" << off << ":";
				for (size_t k = 0; k < 16 && off + k < sizeof(Node); k++)
					std::cerr << " " << (int)p[off + k];
				std::cerr << "\n";
			}
			std::cerr << std::dec;
			cout << "FEC probe failed - stopping search gracefully" << endl;
			break;
		}
		if (can_expand && fec < _board->_got_moves) {
#else
		if (can_expand && get_fully_explored_children_count() < _board->_got_moves) {
#endif
			explore_new_move(board_buffer, eval, alpha, beta, gamma, quiescence_depth, network, base_path_history);
		}

		// EXPLORING AN ALREADY-EXPLORED MOVE (refinement)
		else if (children_count() > 0) {

			// Forced round-robin: every FORCED_EVERY-th refinement descends into
			// the LEAST-visited child. Early WDL verdicts are unreliable (a
			// sacrifice only proves itself several quiet plies deeper), so the
			// scheduler must not be allowed to starve a line to death before its
			// subtree had any chance to speak. The cost is negligible and the
			// guarantee is absolute: no root line can go unproven.
			constexpr int FORCED_EVERY = 1 << 30; // disabled: pure breadth destroys tactical focus
			Move forced;
			if (iteration_index % FORCED_EVERY == FORCED_EVERY - 1) {
				long long min_visits = LLONG_MAX;
				for (auto const& [move, link] : _children) {
					if (!link._node->_is_terminal && link._chosen_iterations < min_visits) {
						min_visits = link._chosen_iterations;
						forced = move;
					}
				}
			}

			explore_random_child(board_buffer, eval, alpha, beta, gamma, quiescence_depth, network, base_path_history, g_tt_node_dag ? &dag_excl : nullptr, forced);
		}

		// Buffers full AND nothing to refine here: clean stop + single log line
		else {
			if (!g_buffers_full_logged) {
				cout << "buffer full - tree capped at " << _nodes
				     << " nodes, refining the existing one" << endl;
				g_buffers_full_logged = true;
			}
			break;
		}

		iterations--;
	}
	
	// FIXME: this should not happen. Under DAG, _nodes is a per-edge proxy
	// bound (Bug 2 model A, clamped >=0): tree guard silenced (Task 4).
	if (!g_tt_node_dag && _nodes <= 0) {
		cout << "negative nodes in grogros zero???" << endl;
	}

	// Computation time
	_time_spent += clock() - begin_monte_time;

	return;
}

// Explores a new move
void Node::explore_new_move(BoardBuffer* board_buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network, PositionHistory *path_history) {

	// Take the first unexplored move
	const Move move = get_first_unexplored_move(true);

	// GUARD: a null return means the expansion bookkeeping disagreed with the
	// unexplored-move scan. Without this, the null move used to be "played"
	// (a self-capture on a1) and added as a phantom child whose garbage
	// evaluation NaN-poisoned every subsequent score.
	if (move.is_null_move()) {
		if (getenv("ENM_DEBUG") != nullptr) {
			std::cerr << "[ENM-null] children=" << children_count() << " got_moves=" << (int)_board->_got_moves << std::endl;
			for (int i = 25; i < (int)_board->_got_moves && i < max_moves; i++) {
				const Move& mv = _board->_moves[i];
				std::cerr << "   [" << i << "] (" << (int)mv.start_row << "," << (int)mv.start_col
					<< ")->(" << (int)mv.end_row << "," << (int)mv.end_col
					<< ") promo=" << (int)mv.get_promo_piece()
					<< " contained=" << (_children.contains(mv) ? 1 : 0) << std::endl;
			}
		}
		return;
	}

	// #7 / B-1 - single threaded history (no more per-move copy).
	PositionHistory& branch_history = *path_history;

	// Child node
	Node *child = nullptr;

	// #11 Plan A - true only in the "new node, not a draw" case: the only
	// case where the TT leaf may short-circuit quiescence.
	bool created_new_node = false;

	// If this move was already explored, but not completely
	bool already_explored = _children.contains(move);
	ChildLink* child_link = already_explored ? &_children[move] : nullptr;

	if (already_explored) {
		child = child_link->_node;

		if (!g_tt_node_dag && _nodes <= child_link->_propagated_nodes) {
			cout << "child nodes >= nodes???" << endl; // tree-only: false under DAG (multi-parent shared subtree)
		}

		// Bug 2 model A: under DAG do not pre-subtract (the delta clamped >=0 nets
		// out correctly at the end of the function, without underflow).
		if (!g_tt_node_dag) {
			_nodes -= child_link->_propagated_nodes;
		}
	}
	else {
		// Take a slot in the buffer
		Board* new_board = board_buffer->get_first_free_board();

		if (new_board == nullptr)
			return;

		new_board->copy_data(*_board, false, false);
		new_board->_is_active = true;
		new_board->make_move(move, false, true);

		// If the position already appears in the path history, treat it as a draw.
		// This is specific to the current path; it must not live in the parent nodes.
		if (position_is_draw_by_repetition(branch_history, *new_board)) {
			// Test position: 3Q2k1/5p1p/2p1p3/2p1P1pq/5P2/4K3/6PP/2r5 b - - 1 4
			// Test position with known bugs: 6kr/4K2p/7B/3bN3/8/8/8/8 b - - 19 10

			// Create the child node
			child = monte_node_buffer.get_first_free_node();

			if (child == nullptr)
				return;

			init_terminal_draw_child(child, new_board, eval, network);
			child->_nodes = 1;
			child->_iterations = 1;
		}

		// Otherwise create a new node normally
		else {
			// zobrist key is already computed incrementally by make_move()

			// #11 Plan B - link-on-create. If a position with the same Zobrist key is
			// already a LIVE Node, link the edge to that shared Node instead of
			// rebuilding a subtree. The draw branch (above) always produces a distinct
			// leaf - never shared. OFF: skips everything.
			Node* shared = nullptr;
			if (g_tt_node_dag) {
				const auto it = node_map.find(new_board->_zobrist_key);
				if (it != node_map.end()) {
					shared = it->second;
				}
			}

			if (shared != nullptr) {
				// Hit: no allocation. Give the buffer board back and point at the
				// existing Node; add_child (below) links the edge and increments
				// shared->_parent_count. created_new_node stays false -> no Plan A
				// TT probe on a shared node.
				if (new_board->_buffer_index >= 0 && !monte_board_buffer._bulk_resetting) {
					monte_board_buffer.free_index(new_board->_buffer_index);
				}
				child = shared;
				g_dag_link_hits++;
				// Bounded detail: WHICH position is reused, by how many parents
				// (before the add_child increment). Confirms sharing actually happens.
				if (dag_dbg_take()) {
					cout << "[DAG] link-hit key=" << std::hex << shared->_board->_zobrist_key
					     << std::dec << " pc=" << shared->_parent_count
					     << " nodes=" << shared->_nodes
					     << " @ " << shared->_board->to_fen() << endl;
				}
			}
			else {
				// Miss: normal creation + registration in node_map.
				child = monte_node_buffer.get_first_free_node();

				if (child == nullptr)
					return;

				child->_board = new_board;
				created_new_node = true;

				if (g_tt_node_dag) {
					node_map[new_board->_zobrist_key] = child;
					g_dag_link_misses++;
				}
			}
		}
	}

    if (child == nullptr) {
        cout << "null child and shouldn't be (explore_new_move)" << endl;
        return;
    }

	// #11 Plan A - TT probe (toggle OFF by default). Only on a freshly created
	// non-draw node: repetition draws are path-dependent (already short-
	// circuited above) and must never be read as a position value. Bounds
	// (STANDPAT/BETA/ALPHA) are never reused as a node value (bound semantics
	// without a window make no sense in MCTS). _depth is required to be in the
	// write-back band: a genuine refined subtree, not a quiescence leaf. Note:
	// child->_board is read (not new_board, out of scope here since it is
	// declared in the else branch); created_new_node guarantees child->_board
	// == the freshly created new_board, with its Zobrist key already computed.
	if (g_tt_main_search && created_new_node) {
		tt_set_probe_scale(1); // audit A1: consume MCTS-scale entries only
		const ZobristEntry* tt_entry = transposition_table.probe(child->_board->_zobrist_key);
		tt_set_probe_scale(0);
		if (tt_entry != nullptr
			&& tt_entry->_flag == TT_EXACT
			&& tt_entry->_depth >= QDEPTH_BAND + MIN_REUSE_LOG2) {
			const int tt_eval = tt_denormalize_mate(tt_entry->_eval, child->_board->_moves_count); // #3
			init_tt_leaf_child(child, child->_board, eval, network, tt_eval * child->_board->get_color());
		}
	}

	if (!child->_fully_explored) {

		bool test = false;

		PathScope _ps(branch_history, *child->_board); // push child position (popped on scope exit, all paths)

		if (test) {
			child->quiescence(board_buffer, eval, 2, alpha, beta, -INT32_MAX, INT32_MAX, network, true, 0, &branch_history); // TODO: cut off more eagerly when the base evaluation is already bad, relative to the static evaluation?
		}

		// If the evaluation beats the base one, run quiescence
		else if (!test || child->_static_evaluation._value * _board->get_color() > _static_evaluation._value * _board->get_color()) {

			child->quiescence(board_buffer, eval, quiescence_depth, alpha, beta, -INT32_MAX, INT32_MAX, network, true, 0, &branch_history);
		}

		child->_fully_explored = true;
	}

	// rnb1kbnr/ppp1pppp/2q5/1B6/8/2N5/PPPP1PPP/R1BQK1NR b KQkq - 3 4
	// rnb1kbnr/ppp1pppp/2q5/8/8/2N5/PPPP1PPP/R1BQKBNR w KQkq - 2 4: here Bb5 -> +114 instead of +895

	// Increase the node count. Bug 2 model A: under DAG do NOT absorb
	// child->_nodes (which may be a SHARED subtree created by another parent
	// -> desync then negative underflow). The per-edge accumulation clamped
	// >=0 is done below (reseed propagated). OFF: tree line unchanged ->
	// inchangee -> byte-identique.
	if (!g_tt_node_dag) {
		_nodes += child->_nodes;
	}

	if (!g_tt_node_dag && _nodes <= 0) {
		cout << "negative nodes in explore_new_move???" << endl;
	}

	if (!g_tt_node_dag && child->_nodes <= 0) {
		cout << "negative nodes in explore_new_move child???" << endl;
	}

	if (child->_iterations == 0) {
		child->_iterations = 1;
	}

	// Add the child
	if (!already_explored) {
		add_child(child, move);
		// Defensive: at() here once threw "Couldn't find key." deep into long
		// games (heap-state dependent). A missing link degrades gracefully.
		child_link = _children.contains(move) ? &_children[move] : nullptr;
	}

	if (child_link->_chosen_iterations == 0) {
		child_link->_chosen_iterations = 1;
	}

	// Bug 2 model A - per-edge _nodes accounting under DAG: delta clamped >=0,
	// never negative, never reseeded from a shared child->_nodes (baseline =
	// the previous _propagated_nodes; on a fresh link-on-create edge add_child
	// set baseline=child->_nodes -> delta 0, so the foreign subtree is not
	// absorbed). OFF: tree reseed unchanged -> byte-identical.
	if (g_tt_node_dag) {
		// model A, literally: +1 unit of work routed through this edge.
		// Bounded by the iteration budget -> never overflows; fully decoupled
		// from the self-inflating shared child->_nodes -> no more cascade.
		// _nodes becomes a bounded "visits" proxy (spec section 6; read neither
		// by selection nor by the budget).
		_nodes += 1;
		child_link->_propagated_nodes += 1;
	}
	else {
		child_link->_propagated_nodes = child->_nodes;
	}
	_iterations += child->_iterations;

	// Have all moves already been explored?
	bool all_moves_explored = (get_fully_explored_children_count()) == _board->_got_moves;

	// TEST: R3r1k1/1P3p2/3p2p1/5n2/4rb2/2P1p2P/4N3/2BK4 b - - 1 35
	//2R1r1k1/1P3p2/3p2p1/5nb1/4r3/2P1p2P/4N3/2BK4 b - - 3 36

	// TESTS: Q7/4p2p/3k4/5p2/4q3/6P1/P2PPK2/8 w - - 2 39

	// All moves explored, so update the board evaluation with the best move -
	// selected by SEARCHED VALUE (negamax hygiene; see quiescence post-loop and
	// explore_random_child): get_node_score ranking decoupled from value exactly
	// in tactical positions, propagating sibling evaluations over the best line.
	int color = _board->get_color();
	Move best_move;

	if (g_search_value_propagation) {
		long long best_value = LLONG_MIN;
		for (auto const& [move, link] : _children) {
			const long long v = link._node->_deep_evaluation._value * color;
			if (v > best_value) {
				best_value = v;
				best_move = move;
			}
		}

		// Stand pat is the best
		if (!all_moves_explored && _deep_evaluation._value * color >= best_value) {
			best_move = Move();
		}
	}
	else {
		best_move = get_best_score_move(alpha, beta, !all_moves_explored);
	}

	if (best_move.is_null_move()) {
		_is_stand_pat_eval = true;
	}
	else {
		_is_stand_pat_eval = false;
		_deep_evaluation = _children[best_move]._node->_deep_evaluation;

		// #11 Plan A - write-back of the refined value (the actual lever).
		// Guard: toggle ON, not terminal (excludes path-dependent draws), not
		// stand-pat (the lower bound is already stored as TT_STANDPAT by
		// quiescence), evaluation present. Proxy depth above the quiescence band.
		if (g_tt_main_search && !_is_terminal && !_is_stand_pat_eval && _deep_evaluation._evaluated) {
			transposition_table.store(_board->_zobrist_key,
				// side-to-move like the quiescence stores (stand_pat = _value*color,
				// exploration.cpp:902/912): otherwise the sign is flipped for Black.
				tt_normalize_mate(_deep_evaluation._value * _board->get_color(), _board->_moves_count), // #3
				tt_writeback_depth(_nodes), TT_EXACT);
		}
	}

	// FIXME: what to do when every child has been examined and no move improves the evaluation?
	// - Option 1: keep the evaluation with no move attached
	// - Option 2: keep the evaluation of the best move
	// Probably harmless: one ply deeper, explore_random_child() picks the best move anyway
}

// Explores a pseudo-random child board
void Node::explore_random_child(BoardBuffer* board_buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network, PositionHistory *path_history, DagExcl* dag_excl, Move forced) {

	// Pick a random child - or descend into the FORCED one (round-robin guard
	// against verdict-starvation of under-proven lines; see grogros_zero)
	Move move;
	if (!forced.is_null_move() && _children.contains(forced)) {
		move = forced;
	}
	else {
		move = pick_random_child(alpha, beta, gamma, dag_excl);
	}

	// Nothing acyclic/explorable left to refine here. Count the iteration
	// and return WITHOUT touching _children[move]: with DAG off this used to
	// fall through to operator[], which silently INSERTED a phantom child
	// keyed by Move() whose null _node crashed the descent one frame later.
	if (move.is_null_move()) {
		_iterations++;
		return;
	}

	ChildLink& child_link = _children[move];
	Node *child = child_link._node;

	// Broken-edge guard: a pooled/stale lifecycle can hand back an edge whose
	// node (or its board) is gone. Descending used to be a guaranteed null
	// deref; skip with a trace instead so the search survives diagnostics.
	if (child == nullptr || child->_board == nullptr) {
		if (getenv("ERC_DEBUG") != nullptr) {
			std::cerr << "[ERC-broken-edge] move=(" << (int)move.start_row << "," << (int)move.start_col
				<< ")->(" << (int)move.end_row << "," << (int)move.end_col
				<< ") promo=" << (int)move.get_promo_piece()
				<< " flags=" << (int)move.flags
				<< " node=" << (void*)child
				<< " children=" << children_count()
				<< " got_moves=" << (int)_board->_got_moves
				<< " parent_fen=" << (_board != nullptr ? _board->to_fen() : string("<null>"))
				<< std::endl;
		}
		_iterations++;
		return;
	}
	// #7 / B-1 - single threaded history; push the child position for the
	// duration of the recursion only, pop guaranteed on scope exit.
	PositionHistory& branch_history = *path_history;

	if (!g_tt_node_dag && child_link._propagated_nodes >= _nodes) {
		if constexpr (dag_debug)
			cout << "child nodes >= nodes in random exploration??? main position: " << _board->to_fen() << ", child position: " << child->_board->to_fen() << endl; // tree-only : faux sous DAG
	}

	// Child node count
	const int initial_child_nodes = child_link._propagated_nodes;

	// #11 Plan B section 3 - DAG soundness. The child subtree may be SHARED
	// (created through path A, descended here through path B). Repetition draw
	// status is PATH-LOCAL: it lives neither in the Node nor in the edge, both
	// of which are SHARED. Re-derive it against the CURRENT path history.
	// Draw on this path: cut the cycle by NOT descending into the shared
	// subtree. Mutate NOTHING shared (not child->_deep_evaluation, not
	// this->_deep_evaluation, not child_link, not node_map): writing a
	// path-local draw onto a shared structure would corrupt the other path
	// (that was the oscillation bug). Conservative: the iteration is counted,
	// the cyclic branch simply not deepened; the parent's normal backup
	// (below, through its other children) remains authoritative. Fully
	// propagating the path-local draw value (spec section 3) is a later
	// refinement, to be decided on measurement. OFF: skipped (current tree
	// behaviour - explore_random_child never re-tested repetition on an
	// existing child).
	if (g_tt_node_dag && position_is_draw_by_repetition(branch_history, *child->_board)) {
		g_dag_recheck_hits++;
		// Bounded detail - THE key boundary for "eval inconsistencies". The cycle
		// is cut (return) BEFORE the parent backup (:686): this iteration does not
		// refresh this->_deep_evaluation and the cyclic edge is NOT devalued
		// (spec section 3, "path-local draw value", not implemented). If the same
		// edge loops back here => spin + parent eval frozen/inconsistent depending
		// on the path. That is the evidence to confirm.
		if (dag_dbg_take()) {
			cout << "[DAG] ï¿½3-cut child_key=" << std::hex << child->_board->_zobrist_key
			     << std::dec << " child_pc=" << child->_parent_count
			     << " parent_eval=" << _deep_evaluation._value
			     << " (fige, edge non devaluee)\n"
			     << "      parent=" << _board->to_fen() << "\n"
			     << "      child =" << child->_board->to_fen() << endl;
		}
		// Bug 1 opt 3 - anti-spin: remember the cyclic edge so the remaining
		// iterations of this grogros_zero call do NOT re-select it (list on the
		// stack, never on a shared structure: invariant 772183a respected; still
		// mutates NOTHING shared).
		// Without the list (OFF / overflow): conservative cut, unchanged.
		if (dag_excl != nullptr) dag_excl->add(move);
		_iterations++;
		return;
	}

	// Explore the child
	{
		PathScope _ps(branch_history, *child->_board);
		child->grogros_zero(board_buffer, eval, alpha, beta, gamma, 1, quiescence_depth, network, &branch_history); // The child evaluation is updated here
	}

	// Update the board evaluation with the best move - by SEARCHED VALUE
	// (negamax hygiene, see quiescence post-loop): ranking by get_node_score
	// copied evaluations of children whose softmax rank disagreed with their
	// value, freezing tactical verdicts at stale levels.
	// Backup is UNCONDITIONAL: the parent's value IS its best child's value.
	if (g_search_value_propagation) {
		int color = _board->get_color();
		long long best_value = LLONG_MIN;
		Move best_value_move;
		for (auto const& [move, link] : _children) {
			const long long v = link._node->_deep_evaluation._value * color;
			if (v > best_value) {
				best_value = v;
				best_value_move = move;
			}
		}
		if (!best_value_move.is_null_move()) {
			_deep_evaluation = _children[best_value_move]._node->_deep_evaluation;
		}
	}
	else {
		// Legacy ranking. GUARD: on NaN scores every comparison fails and
		// get_best_score_move returns the null move - operator[] would then
		// INSERT a phantom child keyed by Move() whose _node is nullptr.
		const Move legacy_best = get_best_score_move(alpha, beta);
		if (!legacy_best.is_null_move()) {
			_deep_evaluation = _children[legacy_best]._node->_deep_evaluation;
		}
	}

	// #11 Plan A - write-back of the refined value (the actual lever).
	// Same guards as in explore_new_move.
	if (g_tt_main_search && !_is_terminal && !_is_stand_pat_eval && _deep_evaluation._evaluated) {
		transposition_table.store(_board->_zobrist_key,
			// side-to-move like the quiescence stores (stand_pat = _value*color,
			// exploration.cpp:902/912): otherwise the sign is flipped for Black.
			tt_normalize_mate(_deep_evaluation._value * _board->get_color(), _board->_moves_count), // #3
			tt_writeback_depth(_nodes), TT_EXACT);
	}

	// Increase the node count. Bug 2 model A: under DAG, per-edge delta
	// clamped >=0 (the tree path's initial_child_nodes is ignored; a SHARED
	// child can shrink through another path/reset -> the tree delta underflows
	// negative). OFF: tree delta unchanged -> byte-identical.
	if (g_tt_node_dag) {
		// model A, literally: +1 (same as explore_new_move). Bounded, decoupled
		// from the shared child->_nodes -> no cascade/overflow, N stays stable.
		_nodes += 1;
		child_link._propagated_nodes += 1;
	}
	else {
		_nodes += child->_nodes - initial_child_nodes;
		child_link._propagated_nodes = child->_nodes;
	}

	if (!g_tt_node_dag && _nodes <= 0) {
		cout << "negative nodes in explore_random_child???" << endl;
	}

	if (!g_tt_node_dag && child->_nodes <= 0) {
		cout << "negative nodes in explore_random_child child???" << endl;
	}

	// Increase the iteration count
	_iterations++;
}

// Returns the most explored child
Move Node::get_most_explored_child_move() {
	int max = -1;

	// Simple sort, ties are not broken
	Move best_move = Move();

	// A proven win buried in a zero-visit terminal child must still be played:
	// the UCT loop never credits _chosen_iterations to terminal children
	// (_can_explore=false excludes them from move_to_play by design - there is
	// nothing to refine behind a game-over position), so a mate-in-1 sits at 0
	// visits while quiet branches rack up thousands (BackRankMateIn1: Ra8#
	// val=+99900000 at 0 visits vs Ra7 at 2305). When a terminal child's value
	// is mate-scale FOR US, play it now; the largest value encodes the
	// shortest mate ((mate_value - D*mate_ply) encoding).
	Move proven_win_move = Move();
	int proven_win_value = 0;
	const bool has_board = _board != nullptr;
	const int node_color = has_board ? _board->get_color() : 1;

	for (auto const& [move, child_link] : _children) {
		if (child_link._chosen_iterations > max) {
			max = child_link._chosen_iterations;
			best_move = move;
		}

		if (has_board && child_link._node != nullptr && child_link._node->_is_terminal) {
			const int child_value = child_link._node->_deep_evaluation._value * node_color;
			if (child_value > proven_win_value && 2 * child_value > mate_value) {
				proven_win_value = child_value;
				proven_win_move = move;
			}
		}
	}

	if (!proven_win_move.is_null_move())
		return proven_win_move;

	return best_move;
}

// Free-list recycling of a detached node and its board (spec section 5).
// Guard: buffer index (>=0) and not during a global reset.
void recycle_detached_node(Node* node) {
	if (node == nullptr)
		return;

	// #11 Plan B - a detached, recycled node must no longer be reachable
	// through node_map (dangling pointer / resurrection). Erase the entry only
	// if it really points at THIS node (a miss may have rewritten it).
	if (g_tt_node_dag && node->_board != nullptr) {
		const auto it = node_map.find(node->_board->_zobrist_key);
		if (it != node_map.end() && it->second == node) {
			node_map.erase(it);
		}
	}

	Board* board = node->_board;
	if (board != nullptr && board->_buffer_index >= 0 && !monte_board_buffer._bulk_resetting)
		monte_board_buffer.free_index(board->_buffer_index);

	if (node->_buffer_index >= 0 && !monte_node_buffer._bulk_resetting)
		monte_node_buffer.free_index(node->_buffer_index);
}

// Resets the node and its children, and deletes them all
void Node::reset(bool recursive) {
	_latest_first_move_explored = -1;
	_nodes = 0;
	_iterations = 0;
	_parent_count = 0;

	if (_board != nullptr) {
		_board->reset_board();
	}

	_initialized = false;
	_is_terminal = false;
	_can_explore = true;
	_time_spent = 0;
	_fully_explored = false;
	_static_evaluation.reset();
	_deep_evaluation.reset();
	_quiescence_depth = 0;
	_is_active = false;
	_is_stand_pat_eval = true;

	if (recursive) {
		for (auto const& [_, child_link] : _children) {
			if (child_link._node == nullptr) {
				continue;
			}

			child_link._node->_parent_count--;

			if (child_link._node->_parent_count <= 0) {
				child_link._node->reset(true);
				// Approach B: the child is permanently detached -> recycle it (and its
				// board). NEVER `this` (reused in place).
				recycle_detached_node(child_link._node);
			}
		}
	}

	_children.clear();
}

// Returns the exploration variations
string Node::get_exploration_variants(const double alpha, const double beta, bool main, bool quiescence, int max_depth, PositionHistory* chain) {

	// Guard against transposition cycles
	if (max_depth <= 0) {
		return "...";
	}

	// End of the variation
	if (_board->_game_over_value) {
		return "";
	}

	// #11 Plan B - stops the displayed line on a transposition/repetition.
	// Under DAG _children is a graph and can loop back: without this the
	// variation unrolls to the max_depth cap (500) -> giant variations and a
	// very slow GUI (refreshing every ~3-4 s). Same idiom as get_main_depth;
	// one chain PER line (copied per child at the main node, threaded along a
	// single line). Without DAG no key ever repeats -> display strictly
	// -> affichage strictement inchange.
	PositionHistory local_chain;
	PositionHistory* c = chain != nullptr ? chain : &local_chain;
	_board->get_zobrist_key();
	// Cut at the real draw: threefold (FIDE) under DAG; twofold = historical
	// historique quand OFF -> strictement byte-identique (count+1>=limite,
	// same shape as position_is_draw_by_repetition).
	const auto _ch_it = c->find(_board->_zobrist_key);
	const int _ch_seen = _ch_it != c->end() ? static_cast<int>(_ch_it->second) : 0;
	const uint8_t _disp_limit = g_tt_node_dag ? display_repetition_limit : search_repetition_limit;
	if (_ch_seen + 1 >= _disp_limit) {
		// Bounded detail: WHERE the displayed line loops back (depth reached
		// before the cycle). Quantifies the "near-infinite variations" on the
		// display side. Guarded by g_tt_node_dag: OFF -> tree threshold unchanged.
		if (g_tt_node_dag) {
			g_dag_variant_cuts++;
			if (dag_dbg_take()) {
				cout << "[DAG] variant-cut depth_left=" << max_depth
				     << " key=" << std::hex << _board->_zobrist_key << std::dec
				     << " @ " << _board->to_fen() << endl;
			}
		}
		return "...";
	}
	(*c)[_board->_zobrist_key]++;

	string variants;

	// If there are explored moves
	if (children_count() > 0) {

		// At the main node, display every variation
		if (main) {
			// TODO: improve this sort

			// Sort the children by GrogrosZero iteration count
			// REVIEW: check whether the vector is too slow
			vector<pair<int, Move>> children_iterations;

			for (auto const& [move, child_link] : _children) {
				children_iterations.emplace_back(-child_link._chosen_iterations, move); // Negated to sort in descending order
			}

			std::ranges::sort(children_iterations.begin(), children_iterations.end());

			// TODO: use the move score as a secondary sort key

			for (auto const& [neg_child_iterations, move] : children_iterations) {
				Node* child = _children[move]._node;
				const int child_iterations = -neg_child_iterations;
				const int child_chosen_iterations = _children[move]._chosen_iterations;
				const bool new_quiescence = !quiescence && child_iterations == 0;

				string child_variants;
				child_variants.reserve(1024);

				char buf[128];

				// Ligne 1 : move + variant
				if (new_quiescence)
					child_variants += '(';

				child_variants += to_string(_board->_moves_count);
				child_variants += _board->_player ? ". " : "... ";
				child_variants += _board->move_label(move, true);
				child_variants += child->children_count() > 0 ? " " : "";
				PositionHistory child_chain = *c; // independent line: prefix copied, no cross-contamination between children
				child_variants += child->get_exploration_variants(alpha, beta, false, new_quiescence || quiescence, max_depth - 1, &child_chain);

				if (new_quiescence)
					child_variants += ')';

				child_variants += '\n';

				// Ligne 2 : Eval
				const int confidence = 100 - static_cast<int>(100.0 * child->_deep_evaluation._uncertainty);
				snprintf(buf, sizeof(buf), "Eval: %s (%d%%) | %s | Score: %s\n",
					_board->evaluation_to_string(child->_deep_evaluation._value).c_str(),
					confidence,
					child->_deep_evaluation._wdl.to_string().c_str(),
					score_string(child->_deep_evaluation._avg_score).c_str()
				);
				child_variants += buf;

				// Ratio computations
				const int nodes = _nodes;
				const int child_nodes = child->_nodes;
				const int nodes_ratio = nodes == 0 ? 0 : child_nodes * 100 / nodes;

				const int iterations = _iterations;
				const int iterations_ratio = iterations == 0 ? 0 : child_iterations * 100 / iterations;
				const int chosen_iterations_ratio = iterations == 0 ? 0 : child_chosen_iterations * 100 / iterations;

				if (nodes == 0) {
					cout << "nodes == 0?? the bug is probably here..." << std::endl;
				}

				// Ligne 3 : stats
				snprintf(buf, sizeof(buf),
					"C: %s (%s%%) | I: %s (%s%%) | N: %s (%s%%) | D: %s | T: %s\n\n",
					int_to_round_string(child_chosen_iterations).c_str(),
					int_to_round_string(chosen_iterations_ratio).c_str(),
					int_to_round_string(child_iterations).c_str(),
					int_to_round_string(iterations_ratio).c_str(),
					int_to_round_string(child_nodes).c_str(),
					int_to_round_string(nodes_ratio).c_str(),
					int_to_round_string(child->get_main_depth(alpha, beta) + 1).c_str(),
					clock_to_string(child->_time_spent, true).c_str()
				);
				child_variants += buf;

				variants += child_variants;
			}

		}

		// Otherwise display only the most explored move
		else {
			// Single-line continuation: follow the VALUE-argmax child (same truth
			// the propagation uses); the score-based pick truncated lines on
			// stand-pat-ranked nodes.
			int color = _board->get_color();
			long long best_value = LLONG_MIN;
			Move best_move;
			for (auto const& [move, link] : _children) {
				const long long v = link._node->_deep_evaluation._value * color;
				if (v > best_value) {
					best_value = v;
					best_move = move;
				}
			}

			// Standpat
			if (best_move.is_null_move()) {
				variants += "...";
			}
			else {
				Node* best_child = _children[best_move]._node;
				const bool new_quiescence = !quiescence && best_child->_iterations == 0;
				variants += (new_quiescence ? "(" : "") + (_board->_player ? to_string(_board->_moves_count) + ". " : "") + _board->move_label(best_move, true) + (best_child->children_count() > 0 ? " " : "") + best_child->get_exploration_variants(alpha, beta, false, new_quiescence || quiescence, max_depth - 1, c) + (new_quiescence ? ")" : "");
			}
		}
	}

	// If no move has been explored
	else {

		// Display the board evaluation at the end of the variation
		variants = "";
	}
	
	return variants;
}

// Returns the depth of the main variation
int Node::get_main_depth(const double alpha, const double beta, int max_depth, PositionHistory* chain) {
	if (max_depth <= 0) {
		return 0;
	}

	if (children_count() > 0) {
		// Walk the PV by SEARCHED VALUE (consistent with the propagation truth):
		// the score-based walk died on stand-pat-ranked nodes, displaying a
		// stuck "Depth: 2" no matter how deep the search actually went.
		int color = _board->get_color();
		long long best_value = LLONG_MIN;
		Move main_move;
		for (auto const& [move, link] : _children) {
			const long long v = link._node->_deep_evaluation._value * color;
			if (v > best_value) {
				best_value = v;
				main_move = move;
			}
		}

		if (main_move.is_null_move()) {
			return 0;
		}

		Node* main_child = _children[main_move]._node;

		if (main_child == nullptr) {
			return 0;
		}

		// #11 Plan B - stops the PV on a transposition/repetition. Under DAG the
		// best-move chain can loop back (it is a graph): without this
		// get_main_depth recurses to the max_depth=500 cap (observed 500/shallow
		// 500/peu profond observee). Idiome null-safe identique a grogros_zero :
		// call owns the chain history. Without DAG no key ever repeats -> same
		// result as before (byte-identical to the tree).
		PositionHistory local_chain;
		PositionHistory* c = chain != nullptr ? chain : &local_chain;
		_board->get_zobrist_key();
		(*c)[_board->_zobrist_key]++;
		main_child->_board->get_zobrist_key();
		// Same threshold as get_exploration_variants: threefold (FIDE) under DAG,
		// twofold when OFF -> displayed depth consistent with the variation text
		// and byte-identical to the tree when OFF.
		const auto _mc_it = c->find(main_child->_board->_zobrist_key);
		const int _mc_seen = _mc_it != c->end() ? static_cast<int>(_mc_it->second) : 0;
		const uint8_t _md_limit = g_tt_node_dag ? display_repetition_limit : search_repetition_limit;
		if (_mc_seen + 1 >= _md_limit) {
			return 0; // the PV reaches a repetition draw -> end of the variation
		}

		return main_child->get_main_depth(alpha, beta, max_depth - 1, c) + 1;
	}

	return 0;
}

// Destructor
Node::~Node() {
	//cout << "destructor not implemented" << endl;
	//for (int i = 0; i < children_count(); i++) {
	//	delete _children[i];
	//}
}

// Returns the most explored child
Node* Node::get_most_explored_child() {
	Move most_explored_move = get_most_explored_child_move();

	if (most_explored_move.is_null_move()) {
		return nullptr;
	}

	return _children[most_explored_move]._node;
}

// Returns the average computation speed in nodes per second
int Node::get_avg_nps() const {
	return _time_spent == 0 ? 0 : ((double)_nodes / (double)_time_spent * CLOCKS_PER_SEC);
}

// Returns the number of iterations per second
int Node::get_ips() const {
	return _time_spent == 0 ? 0 : ((double)_iterations / (double)_time_spent * CLOCKS_PER_SEC);
}

// Quiescence search, integrated into the exploration
int Node::quiescence(BoardBuffer* board_buffer, Evaluator* eval, int depth, double search_alpha, double search_beta, int alpha, int beta, Network* network, bool evaluate_threats, int beta_margin, PositionHistory *path_history) {
	// Backlog. Already implemented: no cutoff while in check, beta-cutoff margin,
	// delta pruning, emergency cutoff, threat-aware stand pat, late move reduction,
	// capture ordering (Board::sort_moves) and transposition reuse.

	// Pruning, still open:
	// - SEE filtering: reduce depth on bad captures, or skip them outright
	// - futility pruning
	// - history pruning
	// - stand-pat pruning, next to the alpha cut on the stand pat
	// - prune harder at low remaining depth (the delta factor is tunable)
	//
	// Move selection, still open:
	// - dedicated quiescence move generator (captures only) to go faster
	// - order check evasions best-first
	// - try the best quiet move first:
	//   https://medwinpublishers.com/OAJDA/an-optimization-method-for-quiescence-in-chess-playing-automata.pdf
	// - instead of captures only, take every move raising the eval by some factor,
	//   ordered by evaluation
	//
	// Depth, still open:
	// - check extensions are wired but disabled (check_extension is 0)
	// - should depth track branch importance rather than a fixed cap?
	// - lower base depth, extended only on the promising moves
	// - why does quiescence run so deep in endgames? it chains checks all the way
	// - apply the same work to minimal_quiescence

	// Problem positions under investigation, all reproduced from the GUI.
	//
	// Poor quiescence:
	//   1nb2rk1/r4ppp/p4q2/2p2N2/8/2bB1Q2/PPP2PPP/3RK2R w K - 0 18
	//   r1b2rk1/1p1p2pp/p3p3/2P1p3/nR1K1P2/6B1/P5PP/5B1R w - - 0 3
	//   4r3/4b1p1/p2B2k1/7p/1p4p1/1P6/P1P1RPP1/5K2 b - - 3 40 (no quiescence at all)
	//   3k4/3p1p2/5P2/8/3qP2P/8/PrPQ4/R4K2 b - - 0 33 (no quiescence at all)
	//   1r1q1r1k/2n4p/p2p1p1Q/2ppR3/7N/8/1PP1N1PP/5R1K b - - 0 24

	// Search stops before the end of the line:
	//   r3r3/P2k2pp/1Bb5/6N1/3P1n2/1Q1q4/PP3PPP/1KR5 w - - 3 27 (worse than stand pat?)
	//   1nb2rk1/r4ppp/p4q2/1Bp1bN2/8/2N2Q2/PPP2PPP/3RK2R w K - 1 17 (after Bxc3)
	//   1r1q1r1k/2n4p/p2p1p1Q/2ppn3/7N/4R3/1PP1N1PP/5R1K w - - 1 24 (after Rxe5)
	//   3k4/3p1p2/5P2/8/3qP2P/8/PrPQ1R2/R3K1r1 w Q - 4 32
	//
	// Mate not seen:
	//   2bk1r2/4b1Qp/8/1P6/3P4/1qp5/4NPPP/R1K2B1R b - - 0 25 (misses #2: Qb2+ Kd1 Qd2#)
	//   r1bqr1k1/1pp2p2/p3p1BQ/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 w - - 5 27 (misses #2)
	//   r1bqr1k1/1pp2p1Q/p3p1B1/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 b - - 6 27 (does not show #1)
	//   1knr4/8/Qp3R1p/1N1r4/1P4p1/5P2/6PP/R6K b - - 0 36 (does not consider Rd1+)
	//   r1bqr2k/1pp2p1B/p3p2Q/2Pn4/3P4/P1P4P/5PP1/1R2R1K1 w - - 3 26 (pruning suspected)
	//
	// Evaluation questioned, cause not yet isolated:
	//   2brr2k/1ppqbppB/p6p/2PP4/1n6/4B2P/3N1PP1/RQ2R1K1 w - - 1 27 (add hanging pieces?)
	//   2rqr1k1/pNbnnpp1/2p1p1p1/P2pP3/Q2P4/B1P4P/4BPP1/RR4K1 b - - 6 22 (queen threatened on d8)
	//   r4rk1/ppp2ppp/2nq1b2/2n5/2Pp4/2NBQ2P/PP1B1PP1/R3R1K1 w - - 0 16
	//   rnbqkbnr/pp2pppp/2p5/3p4/3PP3/8/PPP2PPP/RNBQKBNR w KQkq - 0 3
	//   r1bqk2r/ppp2ppp/1b6/1P1nP3/2B5/5P2/P5PP/RNBQK1NR b KQkq - 0 10
	//   rnb2bnr/ppp1pppp/2k1q3/8/8/3B4/PPPP1PPP/RNB1K1NR w KQ - 0 4
	//
	// Perpetual-check and draw test positions:
	//   3Q4/5pkp/2p1p3/2p1P1pq/5P2/4K3/6PP/2r5 w - - 2 5
	//   3k4/3p4/8/8/3q4/8/3Q1R2/4K1r1 w - - 4 32

	// The node board has been evaluated at least once
	if (_nodes <= 0) {
		_nodes = 1;
	}

	// Computation time
	const clock_t begin_monte_time = clock();

	// Node initialisation
	init_node();

	// #7 / B-1 ï¿½ null-safe path history (appel manuel quiescence via main_gui.h) :
	// a local history owned for the WHOLE duration of the call. It must be declared
	// at function scope (never per move, otherwise path_history dangles on the
	// destroyed local of the previous iteration). Same idiom as grogros_zero.
	// Search path: path_history is always non-null -> no-op.
	PositionHistory local_path_history;
	if (path_history == nullptr) {
		path_history = &local_path_history;
	}

	// Side to move
	int color = _board->get_color();

	// Evaluate the position
	if (!_static_evaluation._evaluated) {
		evaluate_position(eval, false, network, true);
	}
	else {
		_deep_evaluation = _static_evaluation;
	}

	_quiescence_depth = depth;

	// If the game is over
	if (_is_terminal) {
		_nodes = 1;
		_iterations = 1;
		_time_spent += clock() - begin_monte_time;

		return _deep_evaluation._value * color;
	}

	// Stand pat
	const int stand_pat = _deep_evaluation._value * color;
	const int original_alpha = alpha;

	// TT Probe
	const ZobristEntry* tt_entry = transposition_table.probe(_board->_zobrist_key);
	if (tt_entry != nullptr && tt_entry->_depth >= depth) {
		const int tt_eval = tt_denormalize_mate(tt_entry->_eval, _board->_moves_count); // #3: de-canonise the mate distance against the current node's _moves_count
		bool tt_cutoff = false;

		if (tt_entry->_flag == TT_EXACT) {
			_deep_evaluation._value = tt_eval * color;
			tt_cutoff = true;
		}
		else if ((tt_entry->_flag == TT_BETA || tt_entry->_flag == TT_STANDPAT) && tt_eval >= beta) {
			// TT_STANDPAT = static lower bound: consumed exactly like TT_BETA (cutoff
			// only when tt_eval >= beta -> a sound fail-high). Never a window-independent
			// "exact" cutoff on a static evaluation (BUGFIXES #4).
			_deep_evaluation._value = beta * color;
			tt_cutoff = true;
		}
		else if (tt_entry->_flag == TT_ALPHA && tt_eval <= alpha) {
			_deep_evaluation._value = alpha * color;
			tt_cutoff = true;
		}

		if (tt_cutoff) {
			// The cutoff only set _value. The derived fields (_wdl, _avg_score) still
			// came from the static evaluation: a parent inheriting this _deep_evaluation
			// saw an inconsistent evaluation (e.g. _value = mate but _avg_score from a
			// normal position), which skewed get_node_score. Recompute them from the
			// TT _value.
			// If _value is a mate score, get_WDL remixes it with _uncertainty and the
			// still-static _winnable_* (board.cpp:10061-10070) -> the score does not
			// converge to 0/1 (e.g. M4 displayed but score 0.934, confidence 87%).
			// Reset those fields the way the terminal path does (board.cpp:1575-1577).
			// #14: a value coming from a TT cutoff is a *searched* value, trusted enough
			// to stop the search. Its displayed score (_avg_score via get_WDL) must
			// derive from the TT _value, not from the *static* evaluation's _uncertainty
			// left in place (otherwise value and score diverge until re-exploration).
			// _uncertainty is forced to 0 for EVERY cutoff - the #1 v2 fix generalised
			// to the non-mate case, consistent with the terminal/mate/NN paths
			// (board.cpp:1558/1575/1592).
			// #1 v2 / #14: derived fields made consistent from the TT _value. The logic
			// is factored into tt_fixup_derived (reused by the main search's TT leaf,
			// #11 Plan A).
			tt_fixup_derived(_deep_evaluation);

			transposition_table._stats._cutoffs++;
			_time_spent += clock() - begin_monte_time;
			if (tt_entry->_flag == TT_EXACT) return tt_eval;
			if (tt_entry->_flag == TT_ALPHA) return alpha;
			return beta; // TT_BETA / TT_STANDPAT: lower bound (fail-high) -> beta
		}
	}

	// In check, so that variations do not end on a check
	const bool in_check = _board->_player_in_check;

	// Emergency cutoff: depth - 4
	if (depth <= -4) {
		transposition_table.store(_board->_zobrist_key, tt_normalize_mate(stand_pat, _board->_moves_count), depth, TT_STANDPAT); // #4 static lower bound; #3 canonised mate
		_time_spent += clock() - begin_monte_time;
		//cout << "emergency cutoff: " << _board->to_fen() << ", in_check: " << in_check << endl;
		return stand_pat;
	}

	// Depth exhausted, return the stand pat
	if (depth <= 0 && !in_check) {
		transposition_table.store(_board->_zobrist_key, tt_normalize_mate(stand_pat, _board->_moves_count), depth, TT_STANDPAT); // #4 static lower bound; #3 canonised mate
		_time_spent += clock() - begin_monte_time;
		return stand_pat;
	}

	// Margin depending on the opponent threat, judged from the quiescence value on the opponent's turn
	if (evaluate_threats && !in_check && depth >= 2) {
		// Run a short quiescence to evaluate the threat
		// Disabled for performance: this spawns a full secondary quiescence search with Board copies per capture
		//int new_stand_pat = -evaluate_quiescence_threat(eval, 2, search_alpha, search_beta, -INT32_MAX, INT32_MAX, network); // REVIEW: should this search deeper?
		int new_stand_pat = stand_pat; // fallback: use static eval as threat estimate

		// The side to move has flipped, which itself shifts the evaluation slightly
		// 100 less margin
		new_stand_pat += 100;

		beta_margin = max(0, stand_pat - new_stand_pat);
	}

	// If the branch is truly hopeless, a stand pat may still be acceptable
	double total_beta = in_check ? (double)beta + 500 : (double)beta + beta_margin;

	// FIXME: is it right to carry the beta margin down the whole depth?

	if (stand_pat >= total_beta) {
		transposition_table.store(_board->_zobrist_key, tt_normalize_mate(stand_pat, _board->_moves_count), depth, TT_BETA); // #5: store actual score, not beta
		_time_spent += clock() - begin_monte_time;
		//cout << "beta cutoff1: " << stand_pat << " >= " << beta << " + " << beta_margin << endl;
		return beta;
	}

	// TODO: try stand-pat pruning (futility pruning)
	//constexpr int standpat_pruning_threshold = 1000;

	//if (stand_pat + standpat_pruning_threshold <= alpha)
	//	_time_spent += clock() - begin_monte_time;
	//	return stand_pat;

	// Raise alpha if the static evaluation is higher
	if (stand_pat > alpha && !in_check) {
		alpha = stand_pat;
	}

	int move_index = 0;

	// Look at every capture
	for (int i = 0; i < _board->_got_moves; i++) {

		// Move
		const Move move = _board->_moves[i];
		
		// Already explored?
		bool already_explored = _children.contains(move);

		// *** SHOULD THIS MOVE BE EXPLORED? ***

		// While in check, every move is explored
		const bool should_explore = in_check || move.is_capture() || move.is_promotion() || move.is_checkmate() || move.is_check();

		// *** EXPLORATION ***
		// Captures, checks and promotions are explored
		if (should_explore) {

			// Adjusted depth
			int new_depth = depth;

			// Depth extension on a check: forcing moves deserve the extra ply -
			// without it, sequences like Nxf7 Kxf7 Qf3+ Ke6 die one ply short of
			// the quiet move that proves the compensation.
			constexpr int check_extension = 1;
			new_depth += move.is_check() ? check_extension : 0;

			// Depth reduction for the less promising moves (audit A3): the old
			// `move_index * 2` blindly skipped late big captures. Winning
			// captures (MVV-LVA) keep half the penalty so tactical shots stay
			// reachable even when ordered late.
			static constexpr int piece_vals_lmr[13] = { 0, 100, 320, 330, 500, 900, 10000, 100, 320, 330, 500, 900, 10000 };
			bool is_winning_capture = false;
			if (move.is_capture()) {
				const uint8_t cap = _board->_array[move.end_row][move.end_col];
				const uint8_t mover = _board->_array[move.start_row][move.start_col];
				if (cap != none && mover != none)
					is_winning_capture = piece_vals_lmr[cap] > piece_vals_lmr[mover];
			}
			new_depth -= in_check ? 0 : (is_winning_capture ? move_index : move_index * 2);

			if (new_depth <= 0 && !in_check) {
				continue; // Stop looking at moves once we are too deep
			}

			move_index++;
			
			// #7 / B-1 - single threaded history (no more per-move copy).
			// Repetition stays correct (Zobrist keys unique per reversible epoch);
			// balanced push/pop through PathScope below.
			PositionHistory& branch_history = *path_history;

			// Delta pruning
			if (!move.is_checkmate()) {

				constexpr int delta = 500;

				constexpr int piece_values[6] = { 100, 300, 300, 500, 900, 10000 }; // P, N, B, R, Q, K
				constexpr int promotion_value = 1000;
				constexpr int check_value = 500; // Value of a check

				// Quick estimate of what the move can bring
				int best_estimation = 0;

				// Promotion: add the promotion value
				if (move.is_promotion()) {
					best_estimation += promotion_value;
				}

				// Capture: add the value of the captured piece
				else if (move.is_capture()) {
					// En passant: the captured pawn is NOT on the target square.
					// Indexing with none(0)-1 wrapped to -1 and read out of bounds.
					const uint8_t captured = _board->_array[move.end_row][move.end_col];
					best_estimation += captured != none ? piece_values[(captured - 1) % 6] : piece_values[0];
				}

				// Check: add the check value
				else if (move.is_check()) {
					best_estimation += check_value;
				}

				if (stand_pat + best_estimation + delta < alpha) {
					continue; // Skip this move
				}
			}

			// Create the child node
			Node *child = nullptr;
			ChildLink* child_link = already_explored ? &_children[move] : nullptr;

			// If this move was already explored, but not completely
			if (already_explored) {
				child = child_link->_node;
				// position pushed for the duration of the recursion by the PathScope below
			}

			// Create a new node for this move
			else {
				// Take a slot in the buffer
				Board* new_board = board_buffer->get_first_free_board();

				// Buffer full
				if (new_board == nullptr) {
					_time_spent += clock() - begin_monte_time;
					return alpha;
				}

				new_board->copy_data(*_board, false, false);
				new_board->_is_active = true;
				new_board->make_move(move, false, true);

				// If the position already appears in the history, treat it as a draw.
				// Again, this belongs to the current path, not to the parent node.
				if (position_is_draw_by_repetition(branch_history, *new_board)) {
					// Create the child node
					child = monte_node_buffer.get_first_free_node();

					// Buffer full
					if (child == nullptr) {
						_time_spent += clock() - begin_monte_time;
						return alpha;
					}

					init_terminal_draw_child(child, new_board, eval, network);
				}

				else {
					// position pushed for the duration of the recursion by the PathScope below
					// zobrist key is already computed incrementally by make_move()

					// No TT sharing (same reason as in explore_new_move)
					child = monte_node_buffer.get_first_free_node();

					// Buffer full
					if (child == nullptr) {
						_time_spent += clock() - begin_monte_time;
						return alpha;
					}

					child->_board = new_board;
				}

				add_child(child, move);
				child_link = &_children[move];
			}

			// Recursive call on the child - #7/B-1: pushes the child position for
			// the duration of the recursion, pop guaranteed on scope exit.
			int score;
			{
				PathScope _ps(branch_history, *child->_board);
				score = - child->quiescence(board_buffer, eval, new_depth - 1, search_alpha, search_beta, -beta, -alpha, network, false, beta_margin, &branch_history);
			}
			const int previous_child_nodes = child_link != nullptr ? child_link->_propagated_nodes : 0;
			_nodes += child->_nodes - previous_child_nodes;
			if (child_link != nullptr) {
				child_link->_propagated_nodes = child->_nodes;
			}

			// Beta cut-off
			if (score >= beta) {
				// Fail-high: propagate the cutting child's searched evaluation BEFORE
				// returning, so the parent never reads a stale static value here.
				_deep_evaluation = child->_deep_evaluation;
				transposition_table.store(_board->_zobrist_key, tt_normalize_mate(score, _board->_moves_count), depth, TT_BETA); // #5: store actual score, not beta
				_time_spent += clock() - begin_monte_time;
				return beta;
			}

			// Raise alpha if the static evaluation is higher
			if (score > alpha) {
				alpha = score;
			}
		}
	}

	// Update the board evaluation with the best move (once, after all explored moves)
	// Guard: only if at least one move was explored THIS call. On a repeat visit
	// where every move got pruned (delta/depth), _children may still hold stale
	// entries from a previous visit of this pooled node; propagating them would
	// overwrite the fresh stand-pat evaluation (regression from NPS Opt #16).
	if (move_index > 0) {
		Move best_move;

		if (g_search_value_propagation) {
			// Negamax hygiene: the propagated evaluation must come from the child
			// with the best SEARCHED VALUE - that is what defines alpha. Ranking
			// candidates by get_node_score (WDL/softmax-flavoured, used for move
			// choice) instead copied whatever child happened to rank first there,
			// which decouples from value exactly in tactical positions: quiescence
			// returned +151 here while storing the -395 of a differently-ranked
			// sibling, freezing verdicts at garbage.
			int color = _board->get_color();
			long long best_value = LLONG_MIN;
			for (auto const& [move, child_link] : _children) {
				const long long v = child_link._node->_deep_evaluation._value * color;
				if (v > best_value) {
					best_value = v;
					best_move = move;
				}
			}

			bool all_moves_explored = children_count() == _board->_got_moves;
			if (!all_moves_explored && _deep_evaluation._value * color >= best_value) {
				best_move = Move(); // stand pat wins on value
			}
		}
		else {
			bool all_moves_explored = children_count() == _board->_got_moves;
			best_move = get_best_score_move(search_alpha, search_beta, !all_moves_explored);
		}

		if (!best_move.is_null_move()) {
			_deep_evaluation = _children[best_move]._node->_deep_evaluation;
		}
	}

	// TT Store
	// BUGFIXES #4: if the final value is the stand-pat floor (no child beat it, so
	// alpha == stand_pat while alpha > original_alpha), it is NOT an exact searched
	// value but a static lower bound. Marking it TT_STANDPAT (consumed like
	// TT_BETA) instead of TT_EXACT avoids window-independent false cutoffs
	// (this unblocked #11 plan A). On the rare exact tie
	// child==stand_pat, TT_EXACT degrades to TT_STANDPAT: conservative, so sound.
	TTFlag tt_flag;
	if (alpha <= original_alpha)
		tt_flag = TT_ALPHA;
	else if (alpha == stand_pat)
		tt_flag = TT_STANDPAT;
	else
		tt_flag = TT_EXACT;
	transposition_table.store(_board->_zobrist_key, tt_normalize_mate(alpha, _board->_moves_count), depth, tt_flag); // #3 canonised mate

	// Computation time
	_time_spent += clock() - begin_monte_time;

	return alpha;
}

// Returns the number of fully explored child nodes
int Node::get_fully_explored_children_count() const {
	int count = 0;

	for (auto const& [_, child_link] : _children) {
		if (child_link._node->_fully_explored) {
			count++;
		}
	}

	return count;
}

// Returns the sum of the children node counts
int Node::count_children_nodes() const {
	int sum = 0;

	for (auto& [move, child_link] : _children) {
		sum += child_link._node->get_total_nodes();
	}

	return sum;
}

// Returns the total node count
// TODO: should only be used to tell whether the buffer is full
int Node::get_total_nodes() const {
	return count_children_nodes() + 1;
}

// Evaluates the position
void Node::evaluate_position(Evaluator* evaluator, bool display, Network * network, bool game_over_check, bool static_only) {
	_board->evaluate(&_static_evaluation, evaluator, display, network, game_over_check);

	if (!static_only) {
		_deep_evaluation = _static_evaluation;
	}
}

// Returns a pseudo-random child node, weighted by evaluations and node counts
Move Node::pick_random_child(const double alpha, const double beta, const double gamma, const DagExcl* dag_excl) {
	// Positions where every move wins and the search wastes time separating them:
	//   8/8/8/1r5p/2p4k/2Kb4/8/8 b - - 1 69
	// r2qr1k1/3bbp1p/p2pn1p1/3QP3/3P4/3B1N2/1P1B1PPP/R3R1K1 w - - 1 24 : pareil
	//   3b2rk/3P2pp/8/p7/8/2Q1p3/PP1p1pPP/3RqR1K b - - 1 36 (should not spend 100% on one move)
	// 8/8/8/1r5p/2p2k2/2Kb4/8/8 b - - 5 71 : pareil...
	//   R3r1k1/1P3p2/3p2p1/5nb1/4r3/2P1p2P/4N3/2BK4 w - - 2 36 (Rc8 deserves a look)
	// r1r3k1/pp2bppp/3q4/3Pnp2/4nB2/2NB4/PP3PPP/R2QR1K1 w - - 5 16 : ???

	// Best explorable move
	Move move_to_play = Move();
	double explorable_best_score = -DBL_MAX;

	// Move scores

	int color = _board->get_color();

	// Best evaluation value
	int max_eval = -INT_MAX;

	// Best winning chance
	double max_avg_score = 0.0;

	for (auto const& [_, child_link] : _children) {
		Node* child = child_link._node;
		if (child->_deep_evaluation._value * color > max_eval) {
			max_eval = child->_deep_evaluation._value * color;
		}

		if (_board->_player ? child->_deep_evaluation._avg_score > max_avg_score : 1 - child->_deep_evaluation._avg_score > max_avg_score) {
			max_avg_score = _board->_player ? child->_deep_evaluation._avg_score : 1 - child->_deep_evaluation._avg_score;
		}
	}

	MoveScoreList move_scores = get_move_scores(alpha, beta, false, -100, max_eval, max_avg_score);

	struct ScoredMove {
		Move move;
		double score;
	};

	// Boost each move value according to its rank
	static constexpr double boost_table[5] = { 25.0, 8.0, 4.0, 3.0,	2.0 };

	ScoredMove top[5];
	int top_count = 0;

	// Insertion into a bounded top-5, descending by score.
	// The former decrement-without-shift version corrupted the ranking whenever
	// an incoming score had to travel more than one slot once the list was full
	// (elements were overwritten instead of shifted down).
	for (auto const& [move, score] : move_scores) {

		// Worse than (or equal to) the current 5th, list already full: reject
		if (top_count == 5 && score <= top[4].score) {
			continue;
		}

		// Insertion rank among the current entries
		int pos = 0;
		while (pos < top_count && top[pos].score >= score) {
			pos++;
		}

		if (pos >= 5) {
			continue;
		}

		// Shift the tail down, dropping the old 5th when the list is full
		const int last = min(top_count, 4);
		for (int k = last; k > pos; --k) {
			top[k] = top[k - 1];
		}
		top[pos] = { move, score };

		if (top_count < 5) {
			top_count++;
		}
	}

	// Apply the bonus
	for (int i = 0; i < top_count; ++i) {
		Move m = top[i].move;
		move_scores[m] = top[i].score * boost_table[i];
	}

	Move best_move;
	double best_score = 0.0;

	// Gamma (hoisted out of loop ï¿½ depends only on parent state)
	const double new_gamma = gamma / (1.0 - _static_evaluation._uncertainty / 2.0) / (1.0 - _board->_adv / 2.0);

	// Look at every move
	for (auto const& [move, child_link] : _children) {
		// Bug 1 opt 3 - edge section-3 excluded on this path: kept out of best_move
		// AND move_to_play, so the remaining iterations of this grogros_zero call
		// no longer spin on it. OFF: dag_excl == nullptr -> no effect ->
		// byte-identical to the tree.
		if (dag_excl != nullptr && dag_excl->contains(move)) continue;
		Node* child = child_link._node;

		// Move score
		double move_score = move_scores[move];

		// Facteur d'exploration
		int child_iterations = max(child_link._chosen_iterations, child->_iterations);

		// Exploration score
		double exploration_score = child_iterations == 0 ? _iterations * 2 : exp(new_gamma * log((double)_iterations / (double)child_iterations));

		// Final score
		double score = move_score * exploration_score;

		// rnbqkbnr/pppp1ppp/8/4p3/6P1/5P2/PPPPP2P/RNBQKBNR b KQkq - 0 2: Qh4 should stay at 99% chosen while the others are still examined normally

		// If not every move has been examined, raise the score sharply
		//if (child->get_fully_explored_children_count() < child->_board->_got_moves) {
		//	score *= 10.0;
		//}

		// r2qk2r/1pp1b3/p3p3/n2P1bp1/4N2p/P3QP1B/1P6/2KR3R w kq - 1 21: needs fixing
		// R3r1k1/1P3p2/3p2p1/5nb1/4r3/2P1p2P/4N3/2BK4 w - - 2 36 : Tc8...

		// Apply a bonus while some moves are still unexamined
		if (child->_is_stand_pat_eval) {
			Evaluation best_eval = Evaluation();
			//Evaluation best_eval = child->_deep_evaluation;

			for (auto const& [move2, child_link2] : child->_children) {
				if (child->_board->_player ? (child_link2._node->_deep_evaluation > best_eval) : (child_link2._node->_deep_evaluation < best_eval)) {
					best_eval = child_link2._node->_deep_evaluation;
				}
			}

			// FIXME *** what to do if there no fully explored move?
			// FIXME *** should we just change the overall score to the potential eval we are getting here instead of a multiplier?

			//const float multiplier = best_eval._evaluated ? 1.0f + abs(best_eval._value - child->_deep_evaluation._value) / 0.000001f : 1.0f;

			//cout << "move: " << _board->move_label(move) << " | move_score: " << score << " | moves explored " << child->get_fully_explored_children_count() << "/" << (int)child->_board->_got_moves << " = score : " << score << " | stand pat : " << child->_deep_evaluation._value << " | best eval : " << (best_eval._evaluated ? to_string(best_eval._value) : "N/A") << " | multiplier : " << multiplier << endl;
			//cout << "move: " << _board->move_label(move) << " | move_score: " << score << " | stand pat : " << child->_deep_evaluation._value << " | best eval : " << (best_eval._evaluated ? to_string(best_eval._value) : "N/A") << endl;
			//score *= 10.0;
			//score *= multiplier;

			if (best_eval._evaluated) {
				if (best_eval._value * color > max_eval) {
					max_eval = best_eval._value * color;
				}

				if (_board->_player ? best_eval._avg_score > max_avg_score : 1 - best_eval._avg_score > max_avg_score) {
					max_avg_score = _board->_player ? best_eval._avg_score : 1 - best_eval._avg_score;
				}

				score = get_node_score(alpha, beta, max_eval, max_avg_score, _board->_player, &best_eval) * exploration_score;
				//cout << "new score: " << score << endl;
			}
		}
		// Use the evaluation gap between the moves and the stand pat?

		//cout << "move: " << _board->move_label(move) << " | move_score: " << move_score << " | exploration_score : " << exploration_score << " | fully_explored : " << child->get_fully_explored_children_count() << " / " << child->children_count() << " = score : " << score << endl;

		// If the score is better
		if (score > best_score) {
			best_score = score;
			best_move = move;
		}

		// If the move is explorable
		if (child->_can_explore && score > explorable_best_score) {
			explorable_best_score = score;
			move_to_play = move;
		}
	}

	//cout << "best move: " << _board->move_label(best_move) << " | best score: " << best_score << endl << endl;

	// Bug 1 opt 3 - every explorable edge section-3 excluded on this path: no
	// candidate. Report "nothing" (null move); explore_random_child counts the
	// iteration and returns without spinning or touching _children[null].
	// OFF: dag_excl == nullptr -> this case never occurs, the tree path
	// strictement inchange.
	if (best_move.is_null_move()) {
		if (dag_excl != nullptr) return Move();
		if constexpr (dag_debug)
			cout << "null move considered to be the best??" << endl;
	}

	// Credit the branch that will actually be refined (true UCT visit counting).
	// The former version credited best_move even when the descent went to
	// move_to_play: whenever a non-explorable child (terminal/draw leaf) topped
	// the ranking, it absorbed every visit while OTHER branches were being
	// refined, so get_most_explored_child_move() - which selects the played
	// move - locked onto it regardless of what the search actually learned.
	if (move_to_play.is_null_move()) {
		if (!best_move.is_null_move()) {
			if (_children.contains(best_move)) _children[best_move]._chosen_iterations++;
		}
		return best_move;
	}

	if (move_to_play.is_null_move()) {
		if (!best_move.is_null_move()) {
			if (_children.contains(best_move)) _children[best_move]._chosen_iterations++;
		}
		return best_move;
	}

	// The key always exists here (move_to_play came from iterating _children);
	// avoid at()'s throw and any iterator const-quirks entirely.
	if (_children.contains(move_to_play)) {
		_children[move_to_play]._chosen_iterations++;
		return move_to_play;
	}
	if (!best_move.is_null_move()) {
		if (_children.contains(best_move)) _children[best_move]._chosen_iterations++;
	}
	return best_move;
}

// Returns the move scores
MoveScoreList Node::get_move_scores(const double alpha, const double beta, const bool consider_standpat, const int qdepth, int precomputed_max_eval, double precomputed_max_avg_score) const {

	// The stand pat is associated with the null move

	// TEST: 8/8/8/1r5p/2p2k2/2Kb4/8/8 b - - 5 71

	int color = _board->get_color();

	// Best evaluation value
	int max_eval = precomputed_max_eval != INT_MIN ? precomputed_max_eval : -INT_MAX;

	// Best winning chance
	double max_avg_score = precomputed_max_eval != INT_MIN ? precomputed_max_avg_score : 0.0;

	// Find the best evaluation and the best score among all possible moves
	if (precomputed_max_eval == INT_MIN) {
		for (auto const& [_, child_link] : _children) {
			Node* child = child_link._node;
			if (child->_deep_evaluation._value * color > max_eval) {
				max_eval = child->_deep_evaluation._value * color;
			}

			if (_board->_player ? child->_deep_evaluation._avg_score > max_avg_score : 1 - child->_deep_evaluation._avg_score > max_avg_score) {
				max_avg_score = _board->_player ? child->_deep_evaluation._avg_score : 1 - child->_deep_evaluation._avg_score;
			}
		}
	}

	if (consider_standpat) {
		// If the stand pat beats the best move
		if (_deep_evaluation._value * color > max_eval) {
			max_eval = _deep_evaluation._value * color;
		}
		if (_board->_player ? _deep_evaluation._avg_score > max_avg_score : 1 - _deep_evaluation._avg_score > max_avg_score) {
			max_avg_score = _board->_player ? _deep_evaluation._avg_score : 1 - _deep_evaluation._avg_score;
		}
	}

	MoveScoreList move_scores;

	// Stand pat value
	if (consider_standpat) {
		move_scores.emplace(Move(), get_node_score(alpha, beta, max_eval, max_avg_score, _board->_player));
	}

	// Look at every move
	for (auto const& [move, child_link] : _children) {
		Node* child = child_link._node;
		if (qdepth != -100 && child->_quiescence_depth != qdepth) {
			continue;
		}

		// Laplace-style prior on the WDL verdict of under-refined subtrees.
		// A child whose short-term evaluation is bad (e.g. a sacrifice that only
		// pays off several QUIET plies deeper) would otherwise see its avg_score
		// crater immediately and never be scheduled again, freezing its subtree
		// at its creation-time quiescence value forever. Blending avg_score
		// toward neutral with a strength that decays as the subtree earns visits
		// lets exploration - not a single early verdict - decide which lines get
		// proven. The trust scale tracks the size of the current search so the
		// prior stays meaningful at every budget.
		Evaluation child_eval;
		const int child_visits = max(child_link._chosen_iterations, child->_iterations);
		constexpr int TRUST_SCALE = 4096;
		if (g_search_trust_prior && child_visits < TRUST_SCALE) {
			child_eval = child->_deep_evaluation;
			const float weight = (float)TRUST_SCALE / (float)(TRUST_SCALE + child_visits);
			// Symmetric Laplace blend toward neutral: an under-refined subtree's
			// short-term WDL verdict is unreliable in BOTH directions, so defer
			// judgement until it has earned visits. Verdicts harden quickly
			// (shallow mates stay sharp) yet sacrificial lines remain schedulable.
			const float weight_inv = 1.0f - weight;
			child_eval._avg_score = child_eval._avg_score * weight_inv + 0.5f * weight;
			move_scores.emplace(move, child->get_node_score(alpha, beta, max_eval, max_avg_score, _board->_player, &child_eval));
		}
		else {
			move_scores.emplace(move, child->get_node_score(alpha, beta, max_eval, max_avg_score, _board->_player));
		}
	}

	return move_scores;
}

// Returns the node value
double Node::get_node_score(const double alpha, const double beta, const int max_eval, const double max_avg_score, const bool player, Evaluation *custom_eval) const {

	const double min_constant = 1E-100;
	//const double add_constant = 0.05f;
	//const double add_constant = 5.0E-5;
	const double add_constant = 0.000f;
	//const double pure_win_chance_adding = 0.001f; // Bonus for pure winning chances
	const double pure_win_chance_adding = 0.00025f; // Bonus for pure winning chances

	int color = player ? 1 : -1;

	// Evaluation to use
	Evaluation eval = custom_eval != nullptr ? *custom_eval : _deep_evaluation;

	// Factor 1: evaluation
	double eval_score = eval._value * color;
	//int is_eval_mate = _board->is_eval_mate(_deep_evaluation._value) * color;
	
	//eval_score = (is_eval_mate == 0 ? exp(alpha * (eval_score - max_eval)) : 1.0f / (float)is_eval_mate) + min_constant;
	eval_score = exp(alpha * (eval_score - max_eval)) + min_constant;

	// Factor 2: average score
	const double avg_score = player ? eval._avg_score : 1 - eval._avg_score;
	// Suppression cap on the avg-score softmax: without it a move whose
	// short-term WDL craters (e.g. a sacrifice that only pays off several
	// quiet plies deeper) gets exp(-beta*...) ~ 1e-17 relative weight, which
	// no UCT exploration bonus can ever overcome -> the line is mathematically
	// dead and its subtree never refined. The cap bounds the suppression at
	// exp(-AVG_TERM_CAP) so unproven lines stay schedulable while proven bad
	// ones are still strongly discouraged.
	constexpr double AVG_TERM_CAP = 12.0;
	const double avg_term = -beta * (1 - avg_score) / (1 - max_avg_score + min_constant) * max_avg_score / (avg_score + min_constant);
	const double capped = g_search_avg_cap ? max(avg_term, -AVG_TERM_CAP) : avg_term;
	const double score_score = exp(capped) + min_constant;

	// Bonus for pure winning chances? Positions to check:
	//   R4Q2/6pk/1q6/8/3P4/2N3r1/1K6/8 w - - 5 49 (should find Ra2)
	//   8/6pk/7p/8/4p2P/1R1r2P1/5PK1/8 w - - 5 33 (Rxd3 wins)
	//const float k_avg_score = 0.25f;
	const float k_avg_score = 0.25f;

	const double win_adding = ((player ? eval._wdl.win_chance : eval._wdl.lose_chance) + k_avg_score * avg_score) * pure_win_chance_adding;
	//const double win_adding = (player ? eval._wdl.win_chance : eval._wdl.lose_chance) * pure_win_chance_adding;
	//cout << "win chance: " << (player ? eval._wdl.win_chance : eval._wdl.lose_chance) << ", win_adding: " << win_adding << endl;

	//cout << "eval: " << eval_score << ", score: " << score_score << endl;

	// Factor 3: near-constant addend? to be tested
	const double adding = (avg_score == 0.0f) ? 0.0f : avg_score / max_avg_score * add_constant;

	// Final score
	const double score = eval_score * score_score + adding + win_adding;
	//double score = eval_score * score_score + adding;

	//score *= (1.0f + win_adding);

	//cout << "Node score: " << score << " | eval: " << eval_score << ", score: " << score_score << ", adding: " << adding << ", win_adding: " << win_adding << endl;

	return score;
}

// Returns the move with the best score
Move Node::get_best_score_move(const double alpha, const double beta, const bool consider_standpat, const int qdepth) {

	int color = _board->get_color();

	// Best evaluation value
	int max_eval = -INT_MAX;

	// Best winning chance
	double max_avg_score = 0.0;

	// Find the best evaluation and the best score among all possible moves
	for (auto const& [_, child_link] : _children) {
		Node* child = child_link._node;
		if (child->_deep_evaluation._value * color > max_eval) {
			max_eval = child->_deep_evaluation._value * color;
		}

		if (_board->_player ? child->_deep_evaluation._avg_score > max_avg_score : 1 - child->_deep_evaluation._avg_score > max_avg_score) {
			max_avg_score = _board->_player ? child->_deep_evaluation._avg_score : 1 - child->_deep_evaluation._avg_score;
		}
	}

	if (consider_standpat) {
		// If the stand pat beats the best move
		if (_deep_evaluation._value * color > max_eval) {
			max_eval = _deep_evaluation._value * color;
		}
		if (_board->_player ? _deep_evaluation._avg_score > max_avg_score : 1 - _deep_evaluation._avg_score > max_avg_score) {
			max_avg_score = _board->_player ? _deep_evaluation._avg_score : 1 - _deep_evaluation._avg_score;
		}
	}


	// Best move
	Move best_move = Move();
	double best_score = -DBL_MAX;

	if (consider_standpat) {
		const double standpat_score = get_node_score(alpha, beta, max_eval, max_avg_score, _board->_player);
		if (std::isfinite(standpat_score)) {
			best_score = standpat_score;
			best_move = Move();
		}
	}

	for (auto const& [move, child_link] : _children) {
		Node* child = child_link._node;
		if (qdepth != -100 && child->_quiescence_depth != qdepth) {
			continue;
		}
		double score = child->get_node_score(alpha, beta, max_eval, max_avg_score, _board->_player);
		//cout << "move: " << _board->move_label(move) << " | score: " << score << endl;
		// NaN scores (overflowed softmax terms) must never win: every comparison
		// against NaN is false, which would leave best_move as the null stand-pat
		// key and poison every caller that indexes _children with it.
		if (std::isfinite(score) && (score > best_score || (best_move.is_null_move() && score == best_score))) {
			best_score = score;
			best_move = move;
		}
	}

	//cout << "best move: " << _board->move_label(best_move) << " | best score: " << best_score << endl;

	return best_move;
}

// Returns a forecast node score, used by quiescence when the maximum evaluations are unknown
int Node::get_previsonal_node_score(const double alpha, const double beta, const bool player) const {
	return 1E6 * get_node_score(alpha, beta, _deep_evaluation._value, _deep_evaluation._avg_score, player);
}

// Evaluates the threat by running a quiescence on the opponent's turn
int Node::evaluate_quiescence_threat(Evaluator* eval, int depth, double search_alpha, double search_beta, int alpha, int beta, Network* network) const {

	Board b(*_board);
	b.switch_trait();

	Node stand_pat_node(&b);

	int stand_pat_value = stand_pat_node.minimal_quiescence(eval, depth, search_alpha, search_beta, alpha, beta, network);

	return stand_pat_value;
}

// Minimal quiescence (does not store nodes)
int Node::minimal_quiescence(Evaluator* eval, int depth, double search_alpha, double search_beta, int alpha, int beta, Network* network) {
	// REVIEW: still very basic, it only judges the evaluation

	// Node initialisation
	init_node();

	// Side to move
	int color = _board->get_color();

	// Evaluate the position
	evaluate_position(eval, false, network, true);

	// Stand pat
	int stand_pat = _deep_evaluation._value * color;

	// In check, so that variations do not end on a check
	// REVIEW: is this too much for a deliberately quick look?
	bool in_check = _board->_player_in_check;

	if (depth <= 0 && !in_check) {
		return stand_pat;
	}

	// Beta cut-off
	if (stand_pat >= beta) {
		return beta;
	}

	// Alpha cut-off
	if (stand_pat > alpha) {
		alpha = stand_pat;
	}

	// Look at every capture
	for (int i = 0; i < _board->_got_moves; i++) {

		// Move
		const Move move = _board->_moves[i];

		// Captures, checks and promotions are explored
		if (move.is_capture() || move.is_promotion() || move.is_checkmate()) {

			// Delta pruning, disabled here
			//constexpr int delta = 250;

			//constexpr int piece_values[6] = { 100, 300, 300, 500, 900, 10000 }; // P, N, B, R, Q, K
			//constexpr int promotion_value = 1000;

			//// Quick estimate of what the move can bring
			//const int best_estimation = move.is_promotion() ? promotion_value : move.is_capture() ? piece_values[(_board->_array[move.end_col][move.end_col] - 1) % 6] : 0;

			//if (!move.is_checkmate() && !in_check && stand_pat + best_estimation + delta < alpha) {
			//	// Skip this move
			//	continue;
			//}

			// Take a slot in the buffer
			Board new_board(*_board);
			new_board.make_move(move, false, true);
			Node child(&new_board);

			// Recursive call on the child
			int score = -child.minimal_quiescence(eval, depth - 1, search_alpha, search_beta, -beta, -alpha, network);

			// Beta cut-off
			if (score >= beta) {
				return beta;
			}

			// Raise alpha if the static evaluation is higher
			if (score > alpha) {
				alpha = score;
			}
		}
	}

	return alpha;
}

// Default constructor: allocates nothing, init() is mandatory
NodeBuffer::NodeBuffer() {
	_nodes = nullptr;
	_length = 0;
}

// Size constructor (bytes): allocates immediately
NodeBuffer::NodeBuffer(const size_t size_bytes) {
	init(static_cast<int>(size_bytes / sizeof(Node)), false);
}

// Initialises the allocation of n boards
void NodeBuffer::init(const int length, bool display) {
	if (_init) {
		if (display)
			cout << "node buffer already initialized" << endl;
		return;
	}

	if (display)
		cout << "\ninitializing node buffer..." << endl;

	_length = length;
	_nodes = new Node[_length];

	// Every node knows its index; the free list holds all free indices
	_free_indices.clear();
	_free_indices.reserve(_length);
	for (int i = _length - 1; i >= 0; i--) {
		_nodes[i]._buffer_index = i;
		_free_indices.push_back(i);
	}

	_init = true;

	if (display) {
		cout << "node buffer initialized" << endl;
		cout << "node size: " << int_to_round_string(sizeof(Node)) << "b" << endl;
		cout << "length: " << int_to_round_string(_length) << endl;
		cout << "approximate buffer size: " << long_int_to_round_string((long long int)_length * sizeof(Node)) << "b\n\n";
	}
}

// Pops a free index - O(1). Empty stack => -1 (buffer full)
int NodeBuffer::get_first_free_index() {
	if (_free_indices.empty())
		return -1;
	const int index = _free_indices.back();
	_free_indices.pop_back();
	return index;
}

// Frees all the memory
void NodeBuffer::remove() {
	g_buffers_full_logged = false;
	delete[] _nodes;
	_nodes = nullptr;
	_init = false;
	_length = 0;
	_iterator = -1;
	_free_indices.clear();
}

// Global buffer reset: rebuilds only the free-index stack.
// #12: do NOT re-sweep _length with reset(false) (each call cleared a
// robin_map => O(capacity) cost). No caller since the #12 fix.
bool NodeBuffer::reset() {
	g_buffers_full_logged = false;
	_free_indices.clear();
	_free_indices.reserve(_length);
	for (int i = _length - 1; i >= 0; i--)
		_free_indices.push_back(i);

	return true;
}

// Returns the first available node in the buffer
Node* NodeBuffer::get_first_free_node() {
	const int index = get_first_free_index();
	if (index == -1)
		return nullptr;

	Node* node = &_nodes[index];
	node->_is_active = true;
	return node;
}

// DEBUG: prints the buffer state (how many boards are in use)
void NodeBuffer::display_buffer_state() const {
	int used_boards = 0;
	for (int i = 0; i < _length; i++) {
		if (_nodes[i]._is_active)
			used_boards++;
	}
	cout << "Node buffer state: " << used_boards << " / " << _length << " boards used (" << (used_boards * 100.0 / _length) << "%)" << endl;
}

// Buffer for the Monte-Carlo algorithm
NodeBuffer monte_node_buffer;

// Logs "buffer full" once per saturation session; reset to false as soon
// as a reset/remove frees space.
bool g_buffers_full_logged = false;
bool g_tt_main_search = false;
bool g_tt_node_dag = false; // #11 Plan B ï¿½ voir exploration.h
robin_map<uint64_t, Node*> node_map; // #11 Plan B ï¿½ voir exploration.h
