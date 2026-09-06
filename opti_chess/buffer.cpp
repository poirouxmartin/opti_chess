#include "buffer.h"
#include "useful_functions.h"
#include "exploration.h"     // sizeof(Node)
#include "zobrist.h"         // sizeof(ZobristEntry)
#include "windows_tests.h"   // get_available_physical_memory() - isolated RAM probe
                             // (no <windows.h> here: board.h pulls in raylib.h)

// Defined in exploration.cpp: logs "buffer full" exactly once.
// Reset to false here as soon as a BoardBuffer reset/remove frees room again.
extern thread_local bool g_buffers_full_logged;

// Default constructor: allocates nothing, init() is mandatory
BoardBuffer::BoardBuffer() {
	_boards = nullptr;
	_length = 0;
}

// Size constructor (bytes): allocates immediately
BoardBuffer::BoardBuffer(const size_t size_bytes) {
	init(static_cast<int>(size_bytes / sizeof(Board)), false);
}

// Allocates n boards
void BoardBuffer::init(const int length, bool display) {
	if (_init) {
		if (display)
			cout << "board buffer already initialized" << endl;
		return;
	}

	if (display)
		cout << "\ninitializing board buffer..." << endl;

	_length = length;
	_boards = new Board[_length];

	// Every board knows its own index; the free list holds all free indices
	_free_indices.clear();
	_free_indices.reserve(_length);
	for (int i = _length - 1; i >= 0; i--) {
		_boards[i]._buffer_index = i;
		_boards[i]._home_boards = this; // home arena (cross-thread frees route here)
		_free_indices.push_back(i);
	}

	_init = true;

	if (display) {
		cout << "board buffer initialized" << endl;
		cout << "board size: " << int_to_round_string(sizeof(Board)) << "b" << endl;
		cout << "length: " << int_to_round_string(_length) << endl;
		cout << "approximate buffer size: " << long_int_to_round_string((long long int)_length * sizeof(Board)) << "b\n\n";
	}
}

// Pops a free index - O(1). Empty stack => -1 (buffer full)
int BoardBuffer::get_first_free_index() {
	if (_free_indices.empty())
		return -1;
	const int index = _free_indices.back();
	_free_indices.pop_back();
	return index;
}

// Frees all the memory
void BoardBuffer::remove() {
	g_buffers_full_logged = false;
	delete[] _boards;
	_boards = nullptr;
	_init = false;
	_length = 0;
	_iterator = -1;
	_free_indices.clear();
}

// Global buffer reset: only rebuilds the stack of free indices.
// #12: do NOT sweep _length calling reset_board() (each call cleared a
// robin_map => an O(capacity) cost that made load_FEN slow or endless). The
// object contents are reinitialized lazily on reuse. No caller since the #12
// fix (reset_buffers no longer calls it) - kept consistent.
bool BoardBuffer::reset() {
	g_buffers_full_logged = false;
	_free_indices.clear();
	_free_indices.reserve(_length);
	for (int i = _length - 1; i >= 0; i--)
		_free_indices.push_back(i);

	return true;
}

// Returns the first board available in the buffer
Board* BoardBuffer::get_first_free_board() {
	const int index = get_first_free_index();
	if (index == -1)
		return nullptr;

	Board* board = &_boards[index];
	board->_is_active = true;
	return board;
}

// DEBUG *** displays the state of the buffer (how many boards are in use)
void BoardBuffer::display_buffer_state() const {
	int used_boards = 0;
	for (int i = 0; i < _length; i++) {
		if (_boards[i]._is_active)
			used_boards++;

		// Display this particular board
		cout << "Board " << i << ", active: " << (_boards[i]._is_active ? "yes" : "no") << endl;
		_boards[i].display();
	}
	cout << "Board buffer state: " << used_boards << " / " << _length << " boards used (" << (used_boards * 100.0 / _length) << "%)" << endl;
}

// Buffer for the Monte Carlo algorithm
thread_local BoardBuffer monte_board_buffer;

// Adaptive pool sizing from the physical RAM available.
// budget = min(ram_fraction * available RAM, hard_cap); TT capped at budget/4;
// the rest split evenly by NUMBER of Node and Board entries.
// The whole computation uses unsigned long long (portable Win32/x64).
PoolSizing compute_pool_sizing(double ram_fraction, unsigned long long hard_cap_bytes, int tt_max_entries, double rss_overhead_factor) {
	const unsigned long long avail = get_available_physical_memory();

	// Budget = target TOTAL RSS of the process.
	unsigned long long budget = (unsigned long long)((double)avail * ram_fraction);
	if (budget > hard_cap_bytes)
		budget = hard_cap_bytes;

	// Minimum floor: even on battery with low reported RAM, guarantee at least
	// 1 GB for the flat arrays so the engine can still do meaningful analysis.
	constexpr unsigned long long min_budget_bytes = 1024ULL * 1024 * 1024;
	if (budget < min_budget_bytes)
		budget = min_budget_bytes;

	// The real RSS is ~ rss_overhead_factor * (flat arrays + flat TT), because
	// the robin_map _children/_positions_history/TT grow on the heap outside of
	// sizeof. The flat structures are therefore allocated at budget/factor, so
	// the total RSS (flat + dynamic heap) converges towards `budget`. Bounded on
	// any machine, not just the arrays (the real fix for #13).
	if (rss_overhead_factor < 1.0)
		rss_overhead_factor = 1.0;
	budget = (unsigned long long)((double)budget / rss_overhead_factor);

	// Approximate cost of one robin_map<uint64_t, ZobristEntry> entry (~2x overhead)
	const unsigned long long tt_entry_bytes = (unsigned long long)(sizeof(uint64_t) + sizeof(ZobristEntry)) * 2;
	unsigned long long tt_bytes = (unsigned long long)tt_max_entries * tt_entry_bytes;
	if (tt_bytes > budget / 4)
		tt_bytes = budget / 4;
	const int tt_length = (int)(tt_bytes / tt_entry_bytes);

	// The rest is split evenly: as many Node entries as Board entries (one Board ~ one expanded Node)
	const unsigned long long rest = budget - tt_bytes;
	const unsigned long long pair_bytes = (unsigned long long)(sizeof(Board) + sizeof(Node));
	const int count = (int)(rest / pair_bytes);

	PoolSizing ps;
	ps.board_length = count;
	ps.node_length = count;
	ps.tt_length = tt_length;
	return ps;
}