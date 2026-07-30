#pragma once
#include "board.h"
#include <vector>
#include <cstddef>

// Result of pool sizing (number of entries)
struct PoolSizing {
	int board_length;
	int node_length;
	int tt_length;
};

// Computes the pool sizes from the physical RAM available at startup.
// ram_fraction: share of the available RAM = target TOTAL RSS of the process.
// hard_cap_bytes: hard ceiling on the targeted total RSS.
// tt_max_entries: ceiling on the number of transposition table entries.
// rss_overhead_factor: real RSS / size of the flat arrays. The Board[]/Node[]
//     arrays are not the only cost: every Node owns a robin_map _children,
//     every Board a robin_map _positions_history, and the TT itself grows up
//     to tt_max_entries - dynamic heap NOT modelled by sizeof(Board)+sizeof(Node).
//     Measured at ~2.0 (4 GB of arrays => 8 GB RSS). The arrays are therefore
//     sized at budget/factor so that the TOTAL RSS (arrays + dynamic maps) fits
//     the budget on any machine (#13 bounded by construction, not just the flat
//     arrays).
// NB: the whole computation uses unsigned long long - 4 GiB would overflow a
//     32-bit size_t (exactly the memory bug this cleanup fixes).
PoolSizing compute_pool_sizing(double ram_fraction = 0.5,
                               unsigned long long hard_cap_bytes = 4ull * 1024 * 1024 * 1024,
                               int tt_max_entries = 5000000,
                               double rss_overhead_factor = 2.0);

class BoardBuffer {
public:

	// Has the buffer been initialized?
	bool _init = false;

	// Length of the buffer
	int _length = 0;

	// Array of boards
	Board* _boards;

	// Iterator, to shorten the search for a free board index
	int _iterator = -1;

	// Free list: stack of free board indices (O(1) allocation and release)
	std::vector<int> _free_indices;

	// True during reset()/remove(): the release hooks must not push back
	bool _bulk_resetting = false;

	// Is the buffer full? (O(1))
	bool is_full() const { return _free_indices.empty(); }

	// Pushes a released index back (called from the recycling hooks)
	void free_index(int index) { _free_indices.push_back(index); }

	// Default constructor
	BoardBuffer();

	// Constructor taking the size of the buffer (in bytes)
	explicit BoardBuffer(size_t);

	// Allocates n boards
	void init(int length = 5000000, bool display = true);

	// Returns the index of the first free board in the buffer
	int get_first_free_index();

	// Frees all the memory
	void remove();

	// Resets the buffer
	bool reset();

	// Returns the first board available in the buffer
	Board* get_first_free_board();

	// DEBUG *** displays the state of the buffer (how many boards are in use)
	void display_buffer_state() const;
};

// Buffer for Monte Carlo
extern BoardBuffer monte_board_buffer;
