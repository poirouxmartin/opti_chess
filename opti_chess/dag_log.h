#pragma once

// #11 DAG metrics logging (cf. design 2026-05-20).
// Persistent file-based structured event log for runtime evidence of DAG
// search behavior on the two reference repros. Compile-time gated for zero
// runtime cost when disabled. File output is JSON-lines, one event per line,
// at opti_chess/dag_metrics.log (gitignored).

struct Move;      // fwd: opti_chess/board.h (struct, not class — name-mangling must match)
class Node;       // fwd: opti_chess/exploration.h

namespace dag_log {

	// Compile-time toggle. When false, every function below short-circuits
	// to a no-op at entry; call sites also wrap in if constexpr to elide
	// argument evaluation in release builds.
	constexpr bool enabled = true;

	// Cap on per-event detail events emitted per batch. Beyond this cap,
	// counter increments still run but detail events are dropped (counted
	// in `events_dropped`). Raised from 200 → 2000 after first-run analysis :
	// Repro 1 produced 326-599 pred_fires per batch, dropping 126-399 ; 2000
	// covers all observed volumes with margin.
	constexpr int max_events_per_batch = 2000;

	enum class Counter {
		pred_total,
		pred_count_2,
		pred_count_3plus,
		dag_excl_adds,
		dag_excl_skips,
		nodes_terminal,
		nodes_via_explore_new,
		nodes_via_explore_random,
		// #11 attempt-7 (cf. design 2026-05-21 §4) — métriques du verdict all-cycle.
		all_cycle_verdicts_emitted, // nœud all-cycle en fin de grogros_zero
		all_cycle_persisted,        // verdict écrit dans _deep_evaluation (non partagé)
		enum_gate_blocks,           // verdict supprimé : children_count() < _got_moves
		counter_count
	};

	void session_start(const char* fen, bool dag_on, bool plan_a_on,
		int iter_budget, const char* repro_name);
	void session_end(int batches, int final_root_eval, int final_root_pc);

	void batch_start(int seq, int root_pc, int got_moves, int iter_budget);
	void batch_end(int seq, int iters_done, int root_eval, float root_avg_score);

	// Detail event. Capped at max_events_per_batch per batch.
	// count_at_fire = current count(child key) + 1 (the would-be count after
	// a hypothetical push at the §3 cut site).
	// path_size = path_history.size() at moment of fire.
	void pred_fire(int depth, int count_at_fire, int path_size,
		const Node* parent, const Node* child, const Move& m);

	void dag_excl_skip(int depth, const Node* node, const Move& m);

	void bump(Counter c);

} // namespace dag_log
