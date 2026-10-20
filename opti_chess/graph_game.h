#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include <utility>
// Finite two-player game graph with heuristic payoffs, infinite play = draw (0).
// Node i: maximizer[i] (1 = side to move maximises), terminal[i] (value = payoff[i], edges ignored),
// has_floor[i] (non-terminal: payoff[i] is one more option, the stand pat), children in CSR first/child.
// Nodes are added in order; the children of a node are appended right after its add_node call.
// Children may name nodes added later (forward references): indices are only checked in solve.
struct GraphGame {
	std::vector<uint8_t> maximizer, terminal, has_floor;
	std::vector<int> payoff;
	std::vector<int> first{ 0 }; // size n+1: children of node i are child[first[i]..first[i+1])
	std::vector<int> child;
	int size() const { return (int)maximizer.size(); }
	void clear() { maximizer.clear(); terminal.clear(); has_floor.clear(); payoff.clear(); first.assign(1, 0); child.clear(); }
	// Opens the child range of the new node; returns its index
	int add_node(bool maximizer_node, bool is_terminal, bool floor, int value) {
		maximizer.push_back(maximizer_node);
		terminal.push_back(is_terminal);
		has_floor.push_back(floor);
		payoff.push_back(value);
		first.push_back((int)child.size());
		return size() - 1;
	}
	// Appends a child to the last added node (first.back() is that node's range end)
	void add_child(int c) {
		child.push_back(c);
		first.back() = (int)child.size();
	}
};

// Reusable buffers: capacity is kept between calls
struct GraphGameScratch {
	std::vector<int> pfirst, parent, pos; // reverse edges (CSR): parents of c are parent[pfirst[c]..pfirst[c+1])
	std::vector<int> deg, need, stack;    // valid out-degree, remaining options to hit, DFS stack
	std::vector<uint8_t> joined;
	std::vector<std::pair<int, int>> sources; // (payoff, node)
};

// Game value with draw on infinite play. Positive pass: Max forces payoff >= p > 0 (sources
// by payoff descending); negative pass: Min forces payoff <= p < 0 (ascending). Unjoined
// non-terminal nodes keep 0. It equals the first-repetition-draw minimax value at the root,
// and unlike max/min backups on a cyclic graph it has no self-supporting (ghost) values.
inline void solve_graph_game(const GraphGame& g, std::vector<int>& value, GraphGameScratch& s) {
	const int n = g.size();
	value.assign(n, 0);
	for (int i = 0; i < n; i++)
		if (g.terminal[i]) value[i] = g.payoff[i];

	// Reverse edges over the non-terminal nodes; out-of-range children ignored
	s.deg.assign(n, 0);
	s.pfirst.assign(n + 1, 0);
	for (int q = 0; q < n; q++) {
		if (g.terminal[q]) continue;
		for (int k = g.first[q]; k < g.first[q + 1]; k++) {
			const int c = g.child[k];
			if (c >= 0 && c < n) {
				s.deg[q]++;
				s.pfirst[c + 1]++;
			}
		}
	}
	for (int i = 0; i < n; i++) s.pfirst[i + 1] += s.pfirst[i];
	s.parent.resize(s.pfirst[n]);
	s.pos.assign(s.pfirst.begin(), s.pfirst.begin() + n);
	for (int q = 0; q < n; q++) {
		if (g.terminal[q]) continue;
		for (int k = g.first[q]; k < g.first[q + 1]; k++) {
			const int c = g.child[k];
			if (c >= 0 && c < n) s.parent[s.pos[c]++] = q;
		}
	}

	for (int pass = 0; pass < 2; pass++) {
		const bool positive = pass == 0;
		s.joined.assign(n, 0);
		s.need.resize(n);
		s.sources.clear();
		for (int i = 0; i < n; i++) {
			if (!g.terminal[i]) {
				// The side that must reach the bound needs one option, the other needs all
				const bool one = positive ? g.maximizer[i] : !g.maximizer[i];
				s.need[i] = one ? 1 : s.deg[i] + g.has_floor[i];
			}
			if ((g.terminal[i] || g.has_floor[i]) && (positive ? g.payoff[i] > 0 : g.payoff[i] < 0))
				s.sources.push_back({ g.payoff[i], i });
		}
		if (positive)
			std::sort(s.sources.begin(), s.sources.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
		else
			std::sort(s.sources.begin(), s.sources.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

		// One option of q is hit at threshold p; q joins when its last needed option is hit
		auto hit = [&](int q, int p) {
			if (s.joined[q] || --s.need[q] != 0) return;
			s.joined[q] = 1;
			if (!g.terminal[q] && value[q] <= 0) value[q] = p; // never overwrite the positive pass
			s.stack.push_back(q);
		};
		// Propagates the joins on the stack to their parents
		auto drain = [&](int p) {
			while (!s.stack.empty()) {
				const int c = s.stack.back();
				s.stack.pop_back();
				for (int k = s.pfirst[c]; k < s.pfirst[c + 1]; k++)
					hit(s.parent[k], p);
			}
		};

		for (const auto& [p, src] : s.sources) {
			if (g.terminal[src]) {
				if (s.joined[src]) continue;
				s.joined[src] = 1;
				value[src] = g.payoff[src];
				s.stack.push_back(src);
			}
			else {
				hit(src, p);
			}
			drain(p);
		}
	}
}
