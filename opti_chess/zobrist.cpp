#include "zobrist.h"
#include <random>
#include <iostream>
#include <sstream>
#include "useful_functions.h"

using namespace std;

// Fonction qui g�n�re les cl�s de Zobrist
void Zobrist::generate_zobrist_keys() {

	// Si les cl�s ont d�j� �t� g�n�r�es, on ne fait rien
	if (_keys_generated)
		return;
	
	// Initialisation du g�n�rateur al�atoire
	random_device rd;
	mt19937_64 gen(rd());
	uniform_int_distribution<uint_fast64_t> dis(0, UINT_FAST64_MAX);

	// G�n�ration des cl�s

	// Valeur initiale de la cl�
	_initial_key = dis(gen);
	
	// Cl�s des pi�ces
	for (int i = 0; i < 64; i++) {
		for (int j = 0; j < 12; j++) {
			_board_keys[i][j] = dis(gen);
		}
	}

	// Cl� du trait
	_player_key = dis(gen);

	// Cl�s des roques
	for (int i = 0; i < 16; i++) {
		_castling_keys[i] = dis(gen);
	}

	// Cl�s du en-passant
	for (int i = 0; i < 8; i++) {
		_en_passant_keys[i] = dis(gen);
	}

	_keys_generated = true;

}

// Constructeur par d�faut de Zobrist
Zobrist::Zobrist() {
}

// Constructeur par défaut de la table de transposition
TranspositionTable::TranspositionTable()
{
}

// Fonction qui initialise la table de transposition � une taille donn�e
void TranspositionTable::init(const int length, const Zobrist* zobrist, bool display)
{
	if (_init) {
		if (display)
			cout << "already initialized" << endl;
		return;
	}

	if (display)
		cout << "initializing transposition table..." << endl;

	// Initialisation de la table de transposition
	_hash_table.reserve(length);
	_length = length;

	// Initialisation du Zobrist (s'il n'est pas donn�)
	if (zobrist != nullptr)
		_zobrist = *zobrist;
	else
		_zobrist = Zobrist();

	// G�n�ration des cl�s de Zobrist
	_zobrist.generate_zobrist_keys();

	_init = true;

	if (display) {
		cout << "transposition table initialized" << endl;
		cout << _length << " entries (" << long_int_to_round_string(_hash_table.size()) << "b)" << endl;
	}
		
}

// Instance de la table de transposition
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
		// Remplacement : on garde l'entrée la plus profonde
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
	ss << "TT: " << long_int_to_round_string(_hash_table.size()) << " entrées"
	   << "\nProbes: " << long_int_to_round_string(_stats._lookups)
	   << " | Hits: " << long_int_to_round_string(_stats._hits) << " (" << (int)_stats.hit_rate() << "%)"
	   << "\nCutoffs: " << long_int_to_round_string(_stats._cutoffs) << " (" << (int)_stats.cutoff_rate() << "%)"
	   << "\nStores: " << long_int_to_round_string(_stats._stores)
	   << " | Overwrites: " << long_int_to_round_string(_stats._overwrites);
	return ss.str();
}
