#pragma once
#include <cstdint>
#include <unordered_map>

using namespace std;

// Possibilit�s d'optimisation
// -> Les pions ne peuvent pas �tre sur la premi�re et derni�re rang�e
// -> On utilise des entiers 32 bits au lieu de 64 bits (4.29e9 cl�s au lieu de 1.84e19)

// A g�rer:
// Quand y'a plus de place dans la table, on fait quoi? Quelles entr�es supprimer?
// Il faut un historique des positions; la mani�re qui prend le moins de place est de stocker les indices des positions dans la table de transpo?


// Classe qui g�re les cl�s de Zobrist
class Zobrist {
public:
	// Variables

	// Valeur de la cl� initiale de Zobrist
	uint_fast64_t _initial_key = 0; // TODO: � initialiser avec une valeur al�atoire

	// Cl�s des pi�ces
	uint_fast64_t _board_keys[64][12];

	// Cl� du trait
	uint_fast64_t _player_key;

	// Cl�s des roques
	uint_fast64_t _castling_keys[16];

	// Cl�s du en-passant
	uint_fast64_t _en_passant_keys[8];

	// Les cl�s sont-elles g�n�r�es ?
	bool _keys_generated = false;

	// Constructeur par d�faut
	Zobrist();

	// Fonction qui g�n�re les cl�s de Zobrist
	void generate_zobrist_keys();

};

// Entr�e dans la table de transposition
struct ZobristEntry {
public:

	// Indice du plateau dans le buffer
	int _board_index = -1;

	// Potentiellement utile pour le quiescence search

	// Profondeur de la recherche
	int _depth = 0;

	// Pruning?


	// Constructeur par d�faut
	ZobristEntry();

	// Constructeur � partir d'un indice
	ZobristEntry(const int board_index);

};

// Table de transposition (structure: unordered_map)
class TranspositionTable {
public:
	// Table de transposition
	unordered_map<uint64_t, ZobristEntry> _hash_table; // FIXME: il faut g�rer la taille de la table de transposition

	// Zobrist utilis�
	Zobrist _zobrist;

	// La table est-elle initialis�e?
	bool _init = false;

	// Taille de la table de transposition
	int _length = 0;

	// Constructeur par d�faut de la table de transposition
	TranspositionTable();

	// Fonction qui initialise la table de transposition d'une longueur donn�e (nombre d'�l�ments)
	void init(const int length = 5000000, const Zobrist* zobrist = nullptr, bool display = false);

	// Fonction qui renvoie l'indice du plateau dans le buffer (s'il existe)
	int get_zobrist_position_buffer_index(uint_fast64_t key);
};

// Instance de la table de transposition
extern TranspositionTable transposition_table;
