#include "zobrist.h"
#include <random>
#include <iostream>
#include "useful_functions.h"

using namespace std;

// Fonction qui génère les clés de Zobrist
void Zobrist::generate_zobrist_keys() {

	// Si les clés ont déjà été générées, on ne fait rien
	if (_keys_generated)
		return;
	
	// Initialisation du générateur aléatoire
	random_device rd;
	mt19937_64 gen(rd());
	uniform_int_distribution<uint_fast64_t> dis(0, UINT_FAST64_MAX);

	// Génération des clés

	// Valeur initiale de la clé
	uint_fast64_t _initial_key = dis(gen);
	
	// Clés des pièces
	for (int i = 0; i < 64; i++) {
		for (int j = 0; j < 12; j++) {
			_board_keys[i][j] = dis(gen);
		}
	}

	// Clé du trait
	_player_key = dis(gen);

	// Clés des roques
	for (int i = 0; i < 16; i++) {
		_castling_keys[i] = dis(gen);
	}

	// Clés du en-passant
	for (int i = 0; i < 8; i++) {
		_en_passant_keys[i] = dis(gen);
	}

	_keys_generated = true;

}

// Constructeur par défaut de Zobrist
Zobrist::Zobrist() {
}

// Constructeur par défaut de ZobristEntry
ZobristEntry::ZobristEntry()
{
}

// Constructeur à partir d'un indice
//ZobristEntry::ZobristEntry(const int board_index)
//{
//	_board_index = board_index;
//}

// Constructeur à partir d'un noeud
ZobristEntry::ZobristEntry(Node *node)
{
	_node = node;
}

// Constructeur par défaut de la table de transposition
TranspositionTable::TranspositionTable()
{
}

// Fonction qui initialise la table de transposition à une taille donnée
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

	// Initialisation du Zobrist (s'il n'est pas donné)
	if (zobrist != nullptr)
		_zobrist = *zobrist;
	else
		_zobrist = Zobrist();

	// Génération des clés de Zobrist
	_zobrist.generate_zobrist_keys();

	_init = true;

	if (display) {
		cout << "transposition table initialized" << endl;
		cout << _length << " entries (" << int_to_round_string(_hash_table.max_size()) << "b)" << endl;
	}
		
}

// Instance de la table de transposition
TranspositionTable transposition_table;

// Fonction qui renvoie l'indice de la position dans le buffer si elle existe déjà
//int TranspositionTable::get_zobrist_position_buffer_index(uint_fast64_t key) {
//	auto it = _hash_table.find(key);
//
//	// Si l'entrée existe, renvoie l'indice du plateau dans le buffer
//	if (it != _hash_table.end())
//		return it->second._board_index;
//
//	// Sinon, renvoie -1
//	return -1;
//}

// Fonction qui renvoie si une position est déjà dans la table de transposition
bool TranspositionTable::contains(uint_fast64_t key) {
	return _hash_table.find(key) != transposition_table._hash_table.end();
}