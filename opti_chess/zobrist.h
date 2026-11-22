#pragma once

// Zobrist hashing and the evaluation transposition table.
//  - Zobrist: random keys for pieces, side to move, castling rights and en passant (not the
//    move counter). shared_zobrist() is the single process-wide key set; Board::make_move
//    updates the key incrementally from it (copy-make, no unmake).
//  - TranspositionTable (thread_local `transposition_table`): key -> ZobristEntry
//    {_eval, _depth, TTFlag}. Quiescence values are stored from the SIDE-TO-MOVE point of view
//    (Evaluation::_value is white-positive: convert at the boundary).
//  - TTFlag: TT_EXACT, TT_ALPHA (upper bound), TT_BETA (lower bound), TT_STANDPAT (static floor,
//    consumed like TT_BETA, never an exact value).
//  - OPTI_SEED makes the keys deterministic; OPTI_NOTT makes probe() always miss (see zobrist.cpp
//    and docs/ENVIRONMENT.md). This table is distinct from the Node DAG map (node_map, exploration.h).
// See also: zobrist.cpp, exploration_diag.cpp (probe/store in quiescence), opti_chess/ALGORITHMS.md.

#include <cstdint>
#include <string>
#include <robin_map.h>

using namespace std;
using namespace tsl;

class Zobrist {
public:
	uint_fast64_t _initial_key = 0;
	uint_fast64_t _board_keys[64][12];
	uint_fast64_t _player_key;
	uint_fast64_t _castling_keys[16];
	uint_fast64_t _en_passant_keys[8];
	bool _keys_generated = false;

	Zobrist();
	void generate_zobrist_keys();
};

// Process-wide shared Zobrist keys, generated once for every thread.
// Per-thread random keys silently break cross-thread repetition detection
// (game history vs adopted search boards) and cross-thread TT sharing.
Zobrist& shared_zobrist();


enum TTFlag : uint8_t {
	TT_EXACT = 0,
	TT_ALPHA = 1,  // upper bound (fail-low)
	TT_BETA  = 2,  // lower bound (fail-high)
	TT_STANDPAT = 3,  // lower bound: a static stand-pat floor, never an exact searched value (cf. BUGFIXES #4) - consumed like TT_BETA
};

struct ZobristEntry {
public:
	int _eval = 0;
	int _depth = 0;
	TTFlag _flag = TT_EXACT;

	ZobristEntry() = default;
	ZobristEntry(int eval, int depth, TTFlag flag) : _eval(eval), _depth(depth), _flag(flag) {}
};

struct TTStats {
	uint64_t _lookups = 0;
	uint64_t _hits = 0;
	uint64_t _cutoffs = 0;
	uint64_t _stores = 0;
	uint64_t _overwrites = 0;

	void reset() { _lookups = 0; _hits = 0; _cutoffs = 0; _stores = 0; _overwrites = 0; }
	double hit_rate() const { return _lookups == 0 ? 0.0 : 100.0 * _hits / _lookups; }
	double cutoff_rate() const { return _lookups == 0 ? 0.0 : 100.0 * _cutoffs / _lookups; }
};

class TranspositionTable {
public:
	robin_map<uint64_t, ZobristEntry> _hash_table;
	Zobrist _zobrist;
	bool _init = false;
	int _length = 0;
	TTStats _stats;

	TranspositionTable();
	void init(const int length = 5000000, const Zobrist* zobrist = nullptr, bool display = false);
	bool contains(uint64_t key) const;
	const ZobristEntry* probe(uint64_t key);
	void store(uint64_t key, int eval, int depth, TTFlag flag);
	void clear();
	string stats_string() const;
};

extern thread_local TranspositionTable transposition_table;
