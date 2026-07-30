#include "zobrist.h"
#include <random>
#include <iostream>
#include <sstream>
#include "useful_functions.h"

using namespace std;

// Generates the Zobrist keys
void Zobrist::generate_zobrist_keys() {

	// If the keys have already been generated, do nothing
	if (_keys_generated)
		return;
	
	// Initialization of the random generator
	random_device rd;
	mt19937_64 gen(rd());
	uniform_int_distribution<uint_fast64_t> dis(0, UINT_FAST64_MAX);

	// Key generation

	// Initial value of the key
	_initial_key = dis(gen);
	
	// Piece keys
	for (int i = 0; i < 64; i++) {
		for (int j = 0; j < 12; j++) {
			_board_keys[i][j] = dis(gen);
		}
	}

	// Side-to-move key
	_player_key = dis(gen);

	// Castling keys
	for (int i = 0; i < 16; i++) {
		_castling_keys[i] = dis(gen);
	}

	// En passant keys
	for (int i = 0; i < 8; i++) {
		_en_passant_keys[i] = dis(gen);
	}

	_keys_generated = true;

}

// Default constructor of Zobrist
Zobrist::Zobrist() {
}

// Default constructor of the transposition table
TranspositionTable::TranspositionTable()
{
}

// Initializes the transposition table to a given size
void TranspositionTable::init(const int length, const Zobrist* zobrist, bool display)
{
	if (_init) {
		if (display)
			cout << "already initialized" << endl;
		return;
	}

	if (display)
		cout << "initializing transposition table..." << endl;

	// Initialization of the transposition table
	_hash_table.reserve(length);
	_length = length;

	// Initialization of the Zobrist keys (if none is given)
	if (zobrist != nullptr)
		_zobrist = *zobrist;
	else
		_zobrist = Zobrist();

	// Generation of the Zobrist keys
	_zobrist.generate_zobrist_keys();

	_init = true;

	if (display) {
		cout << "transposition table initialized" << endl;
		cout << _length << " entries (" << long_int_to_round_string(_hash_table.size()) << "b)" << endl;
	}
		
}

// Instance of the transposition table
TranspositionTable transposition_table;

bool TranspositionTable::contains(uint64_t key) const {
	return _hash_table.find(key) != _hash_table.end();
}

const ZobristEntry* TranspositionTable::probe(uint64_t key) {
	_stats._lookups++;
	const auto it = _hash_table.find(key);
	if (it == _hash_table.end())
		return nullptr;
	_stats._hits++;
	return &it->second;
}

void TranspositionTable::store(uint64_t key, int eval, int depth, TTFlag flag) {
	const auto it = _hash_table.find(key);
	if (it != _hash_table.end()) {
		// Replacement policy: keep the deepest entry
		if (it->second._depth > depth)
			return;
		_stats._overwrites++;
	}
	_stats._stores++;
	_hash_table[key] = ZobristEntry(eval, depth, flag);
}

void TranspositionTable::clear() {
	_hash_table.clear();
	_stats.reset();
}

string TranspositionTable::stats_string() const {
	ostringstream ss;
	ss << "TT: " << long_int_to_round_string(_hash_table.size()) << " entries"
	   << "\nProbes: " << long_int_to_round_string(_stats._lookups)
	   << " | Hits: " << long_int_to_round_string(_stats._hits) << " (" << (int)_stats.hit_rate() << "%)"
	   << "\nCutoffs: " << long_int_to_round_string(_stats._cutoffs) << " (" << (int)_stats.cutoff_rate() << "%)"
	   << "\nStores: " << long_int_to_round_string(_stats._stores)
	   << " | Overwrites: " << long_int_to_round_string(_stats._overwrites);
	return ss.str();
}
