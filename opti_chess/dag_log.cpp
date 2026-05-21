#include "dag_log.h"
#include "exploration.h"   // Node, _board, _parent_count, _deep_evaluation
#include "board.h"         // Board, Move, to_fen, move_label

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>

namespace dag_log {

namespace {

std::ofstream g_log;
bool g_log_open = false;
std::string g_batch_buffer;
int g_events_this_batch = 0;
int g_events_dropped = 0;
int g_counters[(int)Counter::counter_count] = {};

void open_lazy() {
	if (g_log_open) return;
	// Chemin RELATIF au cwd. Visual Studio lance l'exe depuis opti_chess/opti_chess/
	// donc le log atterrit là ; un lancement depuis la racine workspace le ferait
	// atterrir à la racine. Le .gitignore couvre les deux via glob (**/dag_metrics.log).
	g_log.open("dag_metrics.log", std::ios::app);
	g_log_open = g_log.is_open();
	if (g_log_open && g_batch_buffer.capacity() < 64 * 1024) {
		g_batch_buffer.reserve(64 * 1024);
	}
}

void flush_batch_buffer() {
	if (!g_log_open || g_batch_buffer.empty()) return;
	g_log.write(g_batch_buffer.data(), (std::streamsize)g_batch_buffer.size());
	g_log.flush();
	g_batch_buffer.clear();
}

void iso_time_now(char* buf, size_t bufsize) {
	std::time_t now = std::time(nullptr);
	std::tm tm_buf{};
#ifdef _WIN32
	gmtime_s(&tm_buf, &now);
#else
	gmtime_r(&now, &tm_buf);
#endif
	std::strftime(buf, bufsize, "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
}

// Copy a string into out_buf with size limit + JSON-escape `"` and `\`.
// FENs and SAN are otherwise ASCII-safe.
void copy_json_safe(char* out_buf, size_t out_size, const char* in) {
	size_t j = 0;
	for (size_t i = 0; in && in[i] != 0 && j + 2 < out_size; ++i) {
		char c = in[i];
		if (c == '"' || c == '\\') {
			if (j + 3 >= out_size) break;
			out_buf[j++] = '\\';
		}
		out_buf[j++] = c;
	}
	out_buf[j] = 0;
}

} // anon

void session_start(const char* fen, bool dag_on, bool plan_a_on,
	int iter_budget, const char* repro_name) {
	if constexpr (!enabled) return;
	open_lazy();
	if (!g_log_open) return;

	char ts[64];
	iso_time_now(ts, sizeof(ts));
	char fen_safe[128]; copy_json_safe(fen_safe, sizeof(fen_safe), fen ? fen : "");
	char repro_safe[64]; copy_json_safe(repro_safe, sizeof(repro_safe),
		repro_name ? repro_name : "");

	char line[512];
	if (repro_name) {
		std::snprintf(line, sizeof(line),
			"{\"t\":\"session_start\",\"date\":\"%s\",\"fen\":\"%s\",\"dag\":%s,\"plan_a\":%s,\"iter_budget\":%d,\"repro_name\":\"%s\"}\n",
			ts, fen_safe,
			dag_on ? "true" : "false",
			plan_a_on ? "true" : "false",
			iter_budget, repro_safe);
	} else {
		std::snprintf(line, sizeof(line),
			"{\"t\":\"session_start\",\"date\":\"%s\",\"fen\":\"%s\",\"dag\":%s,\"plan_a\":%s,\"iter_budget\":%d,\"repro_name\":null}\n",
			ts, fen_safe,
			dag_on ? "true" : "false",
			plan_a_on ? "true" : "false",
			iter_budget);
	}
	g_batch_buffer += line;
	flush_batch_buffer();
}

void session_end(int batches, int final_root_eval, int final_root_pc) {
	if constexpr (!enabled) return;
	if (!g_log_open) return;

	char line[256];
	std::snprintf(line, sizeof(line),
		"{\"t\":\"session_end\",\"batches\":%d,\"final_root_eval\":%d,\"final_root_pc\":%d}\n",
		batches, final_root_eval, final_root_pc);
	g_batch_buffer += line;
	flush_batch_buffer();
}

void batch_start(int seq, int root_pc, int got_moves, int iter_budget) {
	if constexpr (!enabled) return;
	open_lazy();
	if (!g_log_open) return;

	g_events_this_batch = 0;
	g_events_dropped = 0;
	for (int i = 0; i < (int)Counter::counter_count; ++i) g_counters[i] = 0;

	char line[256];
	std::snprintf(line, sizeof(line),
		"{\"t\":\"batch_start\",\"seq\":%d,\"root_pc\":%d,\"got_moves\":%d,\"iter_budget\":%d}\n",
		seq, root_pc, got_moves, iter_budget);
	g_batch_buffer += line;
}

void batch_end(int seq, int iters_done, int root_eval, float root_avg_score) {
	if constexpr (!enabled) return;
	if (!g_log_open) return;

	char line[1024];
	std::snprintf(line, sizeof(line),
		"{\"t\":\"batch_end\",\"seq\":%d,\"iters_done\":%d,\"root_eval\":%d,\"root_eval_avg_score\":%.4f,"
		"\"counters\":{\"pred_total\":%d,\"pred_count_2\":%d,\"pred_count_3plus\":%d,"
		"\"dag_excl_adds\":%d,\"dag_excl_skips\":%d,"
		"\"nodes_terminal\":%d,\"nodes_via_explore_new\":%d,\"nodes_via_explore_random\":%d,"
		"\"all_cycle_verdicts_emitted\":%d,\"all_cycle_persisted\":%d,"
		"\"enum_gate_blocks\":%d,"
		"\"events_dropped\":%d}}\n",
		seq, iters_done, root_eval, (double)root_avg_score,
		g_counters[(int)Counter::pred_total],
		g_counters[(int)Counter::pred_count_2],
		g_counters[(int)Counter::pred_count_3plus],
		g_counters[(int)Counter::dag_excl_adds],
		g_counters[(int)Counter::dag_excl_skips],
		g_counters[(int)Counter::nodes_terminal],
		g_counters[(int)Counter::nodes_via_explore_new],
		g_counters[(int)Counter::nodes_via_explore_random],
		g_counters[(int)Counter::all_cycle_verdicts_emitted],
		g_counters[(int)Counter::all_cycle_persisted],
		g_counters[(int)Counter::enum_gate_blocks],
		g_events_dropped);
	g_batch_buffer += line;
	flush_batch_buffer();
}

void pred_fire(int depth, int count_at_fire, int path_size,
	const Node* parent, const Node* child, const Move& m) {
	if constexpr (!enabled) return;
	if (!g_log_open) return;
	if (g_events_this_batch >= max_events_per_batch) {
		g_events_dropped++;
		return;
	}
	g_events_this_batch++;

	char node_fen[128] = "";
	char child_fen[128] = "";
	char move_str[16] = "";

	// Board::to_fen() returns std::string (verified Task 2 Step 0).
	// Board::move_label(Move, bool=false) takes Move by value (verified Step 0);
	// passing m (const Move&) implicitly copies — well-formed.
	if (parent && parent->_board) {
		std::string s = parent->_board->to_fen();
		copy_json_safe(node_fen, sizeof(node_fen), s.c_str());
	}
	if (child && child->_board) {
		std::string s = child->_board->to_fen();
		copy_json_safe(child_fen, sizeof(child_fen), s.c_str());
	}
	if (parent && parent->_board) {
		std::string s = parent->_board->move_label(m);
		copy_json_safe(move_str, sizeof(move_str), s.c_str());
	}

	int child_eval = (child && child->_deep_evaluation._evaluated)
		? child->_deep_evaluation._value : 0;
	float child_avg = (child && child->_deep_evaluation._evaluated)
		? child->_deep_evaluation._avg_score : 0.0f;
	int child_pc = child ? child->_parent_count : 0;

	char line[1024];
	std::snprintf(line, sizeof(line),
		"{\"t\":\"pred_fire\",\"depth\":%d,\"count_at_fire\":%d,\"path_size\":%d,"
		"\"child_pc\":%d,\"child_eval\":%d,\"child_avg\":%.4f,"
		"\"node_fen\":\"%s\",\"child_fen\":\"%s\",\"child_move\":\"%s\"}\n",
		depth, count_at_fire, path_size,
		child_pc, child_eval, (double)child_avg,
		node_fen, child_fen, move_str);
	g_batch_buffer += line;
}

void dag_excl_skip(int depth, const Node* node, const Move& m) {
	if constexpr (!enabled) return;
	if (!g_log_open) return;
	if (g_events_this_batch >= max_events_per_batch) {
		g_events_dropped++;
		return;
	}
	g_events_this_batch++;

	char move_str[16] = "";
	int node_pc = node ? node->_parent_count : 0;
	if (node && node->_board) {
		// move_label takes Move by value (verified Step 0); implicit copy from ref.
		std::string s = node->_board->move_label(m);
		copy_json_safe(move_str, sizeof(move_str), s.c_str());
	}

	char line[256];
	std::snprintf(line, sizeof(line),
		"{\"t\":\"dag_excl_skip\",\"depth\":%d,\"node_pc\":%d,\"move\":\"%s\",\"reason\":\"in_excl\"}\n",
		depth, node_pc, move_str);
	g_batch_buffer += line;
}

void bump(Counter c) {
	if constexpr (!enabled) return;
	g_counters[(int)c]++;
}

} // namespace dag_log
