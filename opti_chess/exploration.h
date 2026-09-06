#pragma once

#include "board.h"
#include "buffer.h"
#include <robin_map.h>
#include <robin_hash.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <vector>

using namespace tsl;
using PositionHistory = RepetitionHistory;

// ---------------------------------------------------------------------------
// Search feature toggles - self-play A/B testing (defaults = current best).
// Flip per game from the self-play runner to measure non-regression:
//   g_search_value_propagation: value-argmax backup (false = legacy
//     get_best_score_move ranking, pre-audit behaviour)
//   g_search_trust_prior: Laplace blend of under-refined subtrees' WDL
//   g_search_avg_cap: cap the avg-score softmax suppression (AVG_TERM_CAP)
// ---------------------------------------------------------------------------
extern bool g_search_value_propagation;
extern bool g_search_trust_prior;
extern bool g_search_avg_cap;

// Adaptive quiescence depth: reduce depth for moves that look bad statically.
// Runtime toggle via OPTI_ADAPTIVE env var (default: OFF for baseline comparison).
extern bool g_adaptive_quiescence;

// Selective deepening: reduce depth for later-discovered moves.
// Runtime toggle via OPTI_SELECTIVE env var (default: OFF).
extern bool g_selective_deepening;

// Check extension (audit A4): +N ply on check-giving moves in quiescence.
// Via OPTI_CHECK_EXT (default 0 = off, wired but disabled).
extern int g_check_extension;

// Phase 6: shared-tree mode (OPTI_SHARED=1 with OPTI_THREADS>1). All workers
// refine ONE tree (main thread's arenas): node locks guard _children,
// atomics carry the counters, TT is sharded-shared. Default OFF.
extern bool g_shared_tree;

// Quiescence exit census dump (OPTI_QSTATS to print).
void dump_qstats();
// Census gate (defined in exploration_diag.cpp).
extern const bool g_qstats_on;

// Phase 7a component timers (defined in eval_core.cpp).
extern thread_local double g_t_king_safety_s;
extern thread_local double g_t_ks_power_s;
extern thread_local double g_t_ks_maps_s;
extern thread_local double g_t_ks_misc_s;
extern thread_local double g_t_mobility_s;
extern thread_local double g_t_matpos_s;
extern thread_local double g_t_pawns_s;
extern thread_local double g_t_endgame_s;
// King-safety cache census (defined in eval_king.cpp).
extern thread_local long long g_ks_hits;
extern thread_local long long g_ks_miss;
extern thread_local long long g_ks_clears;

// Hard time deadline for budgeted search (PuzzleRunner TIME mode).
// 0 = none. Set (with g_search_abort=false) at puzzle start; quiescence
// samples the clock (throttled 1/1024) and raises g_search_abort past due;
// quiescence + grogros_zero then unwind promptly so overrun stays in ms.
// Return value on abort is alpha (sound fail-soft bound, same as the
// buffer-full early exits).
// Phase 6: atomics shared by all worker threads (set once, sampled hot).
extern std::atomic<clock_t> g_search_deadline;
extern std::atomic<bool> g_search_abort;

class Node;

struct ChildLink {
	Node* _node = nullptr;
	std::atomic<int> _chosen_iterations{ 0 };
	int _propagated_nodes = 0;
	// Virtual loss: descents currently inside this edge (Lazy SMP). Added to
	// the visit count in pick_random_child so sibling threads spread out;
	// RAII-guarded in explore_random_child (net zero on legacy path).
	std::atomic<int> _inflight{ 0 };
	ChildLink() = default;
	// atomic is not copyable: copy by value snapshot (map rehash/insert).
	ChildLink(const ChildLink& o) : _node(o._node), _chosen_iterations(o._chosen_iterations.load(std::memory_order_relaxed)), _propagated_nodes(o._propagated_nodes), _inflight(o._inflight.load(std::memory_order_relaxed)) {}
	ChildLink& operator=(const ChildLink& o) {
		_node = o._node;
		_chosen_iterations.store(o._chosen_iterations.load(std::memory_order_relaxed), std::memory_order_relaxed);
		_propagated_nodes = o._propagated_nodes;
		_inflight.store(o._inflight.load(std::memory_order_relaxed), std::memory_order_relaxed);
		return *this;
	}
};

// #11 Plan B - Bug 1 option 3: per-traversal exclusion (anti-spin, section 3).
// A path-local list living ONLY on the stack of the current grogros_zero
// frame - never stored on a Node/ChildLink (both are SHARED under the DAG,
// cf. invariant 772183a). Fixed size: zero allocation on the hottest path
// (perf #1). On overflow we fall back to the former behaviour (a conservative
// cut) - sound, merely sub-optimal.
struct DagExcl {
	static constexpr int CAP = 24;
	Move moves[CAP];
	int count = 0;
	bool contains(const Move& m) const {
		for (int i = 0; i < count; ++i) if (moves[i] == m) return true;
		return false;
	}
	void add(const Move& m) {
		if (count < CAP && !contains(m)) moves[count++] = m;
	}
};

// Stack-allocated move score list replacing robin_map<Move, double>.
// Zero heap allocation; O(n) lookup is fine for ~30 children.
struct MoveScoreList {
	struct Entry {
		Move move;
		double score;
	};
	static constexpr int CAP = max_moves;
	Entry entries[CAP];
	int count = 0;

	void emplace(const Move& m, double s) {
		if (count < CAP) entries[count++] = { m, s };
	}

	double& operator[](const Move& m) {
		for (int i = 0; i < count; ++i)
			if (entries[i].move == m) return entries[i].score;
		static double fallback = 0.0;
		return fallback;
	}

	const Entry* begin() const { return entries; }
	const Entry* end() const { return entries + count; }
	Entry* begin() { return entries; }
	Entry* end() { return entries + count; }
};

// TODO:
// Instead of holding a board, store only the index of the board in the buffer?

// Node of the exploration tree.
//
// Important for repetitions and future transpositions:
// - the node stores the "positional" state of the exploration (evaluations, children, counters),
// - the history of repeated positions deliberately stays outside the node and is passed
//   through the call stack, because it depends on the current path and not on the position alone.
class Node {
public:

	// Variables

	// Board: FIXME -> index of the board in the buffer?
	Board* _board = nullptr;

	// Move played to reach this board (FIXME: is it already stored in the board?)
	//Move _move;

	// Children with their associated move.
	// Selection statistics and propagated node counters live on the edge, so they stay
	// correct when several parents share the same node.
	robin_map<Move, ChildLink> _children;

	// Phase 6: per-node spinlock guarding _children mutation + iteration under
	// the shared tree (expand claim/publish, pick/score iteration). Only used
	// when g_shared_tree is set; legacy path never touches it (zero cost).
	// Cleared in get_first_free_node + NodeBuffer::init (pooled storage).
	std::atomic_flag _mtx;
	void lock_node() {
		while (_mtx.test_and_set(std::memory_order_acquire)) {
			// brief holds: plain spin (no pause intrinsic to avoid headers)
		}
	}
	void unlock_node() { _mtx.clear(std::memory_order_release); }

	// Children
	//vector<Node*> _children;

	// To speed up the search for the first unexplored move
	int _latest_first_move_explored = -1;

	// Number of nodes in the current subtree.
	// This counter is tree-local: it is only correct as long as a node is not shared
	// between several parents.
	// Phase 6: atomic (shared-tree concurrent adds; stats/TT-depth only).
	std::atomic<int> _nodes{ 0 };
	//int _nodes = count_children_nodes() + 1;

	// Number of explorations by the GrogrosZero algorithm
	// Phase 6: atomic (fetch_add) under shared tree; plain ops stay valid.
	std::atomic<int> _iterations{ 0 };

	// Number of parents referencing this node.
	int _parent_count = 0;

	// TODO: several node types are needed: quiet nodes (the ones searched by GrogrosZero), quiescence nodes, transposition nodes...
	// The research_nodes will be used for Grogros's thinking time, arrow drawing and so on...

	// Computation time
	clock_t _time_spent = 0;

	// Depth of the quiescence search
	int _quiescence_depth = 0;

	// Has this node been explored completely?
	bool _fully_explored = false;

	// Is there anything left to explore?
	bool _can_explore = true;

	// Static evaluation of the position
	Evaluation _static_evaluation;

	// Evaluation of the position after search
	Evaluation _deep_evaluation;

	// Is this a terminal node?
	bool _is_terminal = false;

	// Node initialized?
	bool _initialized = false;

	// The evaluation value is the stand pat
	bool _is_stand_pat_eval = true;

	// Whether it is active in the buffer
	bool _is_active = false;

	// Index in monte_node_buffer (-1 = object outside the buffer: do not recycle)
	int _buffer_index = -1;

	// Zobrist key snapshot taken before Node::reset() zeroes the board.
	// Used by recycle_detached_node to erase the correct node_map entry
	// even after reset_board() has cleared _board->_zobrist_key.
	uint64_t _saved_zobrist = 0;

	// To add: evaluation?, node count?...

	// Constructors

	// Constructor
	Node();

	// Constructor taking a board
	Node(Board* board);


	// Functions

	// Adds a child
	void add_child(Node* child, Move move);

	// Returns the number of children
	size_t children_count() const;

	// Returns the index of the child associated with the move if it exists, -1 otherwise (in the _children vector)
	//int get_child_index(Move move) const;

	// Returns the index of the first move that has not been added yet, -1 otherwise
	//int get_first_unexplored_move_index(bool fully_explored = false);

	// Returns the first move that has not been added yet
	Move get_first_unexplored_move(bool fully_explored = false);

	// Initializes the node from its board
	void init_node();

	// New GrogrosZero
	void grogros_zero(BoardBuffer* board_buffer, Evaluator* eval, const double alpha, const double beta, const double gamma, int nodes, int quiescence_depth, Network* network = nullptr, PositionHistory *path_history = nullptr, const clock_t max_time = 0);

	// Explores a new move
	void explore_new_move(BoardBuffer* board_buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network = nullptr, PositionHistory *path_history = nullptr);

	// Explores a pseudo-random child board
	void explore_random_child(BoardBuffer* board_buffer, Evaluator* eval, double alpha, double beta, double gamma, int quiescence_depth, Network* network = nullptr, PositionHistory *path_history = nullptr, DagExcl* dag_excl = nullptr, Move forced = Move());

	// Returns the most explored child move
	Move get_most_explored_child_move();

	// Resets the node and its children, and deletes them all
	void reset(bool recursive = true);

	// Returns the exploration variations
	string get_exploration_variants(const double alpha, const double beta, bool main = true, bool quiescence = false, int max_depth = 500, PositionHistory* chain = nullptr);

	// Returns the depth of the principal variation
	int get_main_depth(const double alpha, const double beta, int max_depth = 500, PositionHistory* chain = nullptr);

	// Returns the most explored child
	Node* get_most_explored_child();

	// Returns the average search speed in nodes per second
	int get_avg_nps() const;

	// Returns the number of iterations per second
	int get_ips() const;

	// Quiescence search integrated into the exploration
	int quiescence(BoardBuffer* board_buffer, Evaluator* evaluator, int depth, double search_alpha, double search_beta, int alpha = -INT_MAX, int beta = INT_MAX, Network* network = nullptr, bool evaluate_threats = true, int beta_margin = 0, PositionHistory *path_history = nullptr);
	//void grogros_quiescence(Buffer* buffer, Evaluator* eval, int depth);

	// Returns the number of fully explored child nodes
	int get_fully_explored_children_count() const;

	// Returns the sum of the children's node counts
	int count_children_nodes() const;

	// Returns the total node count
	int get_total_nodes() const;

	// Evaluates the position
	void evaluate_position(Evaluator* evaluator, bool display = false, Network* network = nullptr, bool game_over_check = true, bool static_only = false);

	// Returns a pseudo-random child node (depending on the evaluations and the node count)
	Move pick_random_child(const double alpha, const double beta, const double gamma, const DagExcl* dag_excl = nullptr);

	// Returns the score of a move. Alpha raises the weight of the evaluation, beta raises the weight of the win rate
	MoveScoreList get_move_scores(const double alpha, const double beta, const bool consider_standpat = false, const int qdepth = -100, int precomputed_max_eval = INT_MIN, double precomputed_max_avg_score = 0.0) const;

	// Returns the value of the node
	double get_node_score(const double alpha, const double beta, const int max_eval, const double max_avg_score, const bool player, Evaluation *custom_eval = nullptr) const;

	// Returns the move with the best score
	Move get_best_score_move(const double alpha, const double beta, const bool consider_standpat = false, const int qdepth = -100);

	// Returns a forecast value of the node score, when the maximum evaluations are unknown (for the quiescence)
	int get_previsonal_node_score(const double alpha, const double beta, const bool player) const;

	// Evaluates the threat using a quiescence search on the opponent's turn
	int evaluate_quiescence_threat(Evaluator* eval, int depth, double search_alpha, double search_beta, int alpha = -INT_MAX, int beta = INT_MAX, Network* network = nullptr) const;

	// Minimal quiescence (without storing the nodes)
	int minimal_quiescence(Evaluator* eval, int depth, double search_alpha, double search_beta, int alpha = -INT_MAX, int beta = INT_MAX, Network* network = nullptr);

	// Functions to add: destruction of the children and of itself...

	// Destructor: TODO (delete every dynamic array...)
	// Every board of the buffer will have to be freed

	// Destructor
	~Node();
};

// Phase 6: RAII guard for the per-node spinlock. Engaged ONLY when
// g_shared_tree is set (zero-cost branch otherwise). Protocol: a thread
// holds AT MOST ONE node lock at a time (never nested: no lock is held
// across quiescence/grogros recursion), so DAG cycles cannot deadlock.
// Every iteration/mutation of a node's _children map happens under that
// node's lock in shared mode; child NODE fields are read lock-free
// (racy-benign aligned words, SMP-standard last-writer-wins).
struct NodeLock {
	Node* n;
	bool on;
	explicit NodeLock(Node* node) : n(node), on(g_shared_tree) { if (on) n->lock_node(); }
	explicit NodeLock(const Node* node) : n(const_cast<Node*>(node)), on(g_shared_tree) { if (on) n->lock_node(); }
	~NodeLock() { if (on) n->unlock_node(); }
	NodeLock(const NodeLock&) = delete;
	NodeLock& operator=(const NodeLock&) = delete;
};


class NodeBuffer {
public:

	// Has the buffer been initialized?
	bool _init = false;

	// Length of the buffer
	int _length = 0;

	// Array of nodes
	Node* _nodes;

	// Iterator, to shorten the search for a free node index
	int _iterator = -1;

	// Free list: stack of free node indices (O(1) allocation and release)
	std::vector<int> _free_indices;

	// True during reset()/remove(): the release hooks must not push back
	bool _bulk_resetting = false;

	// Is the buffer full? (O(1))
	bool is_full() const { return _free_indices.empty(); }

	// Pushes a released index back (called from the recycling hooks)
	void free_index(int index) { _free_indices.push_back(index); }

	// Default constructor
	NodeBuffer();

	// Constructor taking the size of the buffer (in bytes)
	explicit NodeBuffer(size_t);

	// Allocates n nodes
	void init(int length = 10000000, bool display = true);

	// Returns the index of the first free node in the buffer
	int get_first_free_index();

	// Frees all the memory
	void remove();

	// Resets the buffer
	bool reset();

	// Returns the first node available in the buffer
	Node* get_first_free_node();

	// DEBUG *** displays the state of the buffer (how many nodes are in use)
	void display_buffer_state() const;
};

extern thread_local NodeBuffer monte_node_buffer;

// Logs "buffer full" once per saturation session
extern thread_local bool g_buffers_full_logged;
// #11 Plan A - active probe + TT write-back in the main search.
// Default OFF: current behaviour byte for byte, A/B on the same binary.
extern bool g_tt_main_search;

// #11 Plan B - transposition DAG (node sharing). Runtime A/B, default OFF:
// OFF = the current tree byte for byte. Not meant to be ON with g_tt_main_search.
extern bool g_tt_node_dag;

// #11 Plan B - Zobrist key -> LIVE Node*. Distinct from transposition_table
// (the evaluation TT). Read and populated only when g_tt_node_dag is set. Same
// kind of map as Node::_children (robin_map, exploration.h:43).
// thread_local: parallel workers own separate trees (Phase 6).
extern thread_local robin_map<uint64_t, Node*> node_map;
// Phase 6: guards node_map find/insert/erase under the shared tree
// (per-thread maps need no lock, but the mutex makes shared use safe;
// uncontended cost is negligible vs a quiescence).
extern std::mutex g_node_map_mutex;
// TEMPORARY claim counter (remove after).
extern std::atomic<long long> g_dbg_claims;
// TEMPORARY shared-mode serialization probe (remove after): when
// OPTI_SERIALIZE is set, one global mutex serializes whole grogros_zero
// calls. If shared results recover, corruption is concurrency-induced;
// if still broken, it's a shared-protocol logic bug.
extern std::recursive_mutex g_serialize_mutex;

// Virtual loss weight (Lazy SMP): each in-flight descent counts as this many
// visits in pick_random_child. OPTI_VLOSS=N overrides (default 4).
extern int g_virtual_loss;


// #11 Plan B - diagnostic report (one line, toggle-gated). Called after every
// grogros_zero batch from the GUI when g_tt_node_dag is set.
void dag_debug_report();

// O(1) free-list recycling of a DETACHED node (no parent left) and of its
// board. The single point of passage (spec section 5). To be called ONLY on a
// node that is detached for good, NEVER on a node that is reset and then reused
// in place (the load_FEN root / the "else" branch of play-move) -> double free otherwise.
void recycle_detached_node(Node* node);
