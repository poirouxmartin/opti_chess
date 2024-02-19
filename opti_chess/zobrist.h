#pragma once
#include <cstdint>
#include <unordered_map>

using namespace std;

// Possibilités d'optimisation
// -> Les pions ne peuvent pas être sur la première et dernière rangée
// -> On utilise des entiers 32 bits au lieu de 64 bits (4.29e9 clés au lieu de 1.84e19)

// A gérer:
// Quand y'a plus de place dans la table, on fait quoi? Quelles entrées supprimer?
// Il faut un historique des positions; la manière qui prend le moins de place est de stocker les indices des positions dans la table de transpo?


// Classe qui gère les clés de Zobrist
class Zobrist {
public:
	// Variables

	// Valeur de la clé initiale de Zobrist
	uint_fast64_t _initial_key = 0; // TODO: à initialiser avec une valeur aléatoire

	// Clés des pièces
	uint_fast64_t _board_keys[64][12];

	// Clé du trait
	uint_fast64_t _player_key;

	// Clés des roques
	uint_fast64_t _castling_keys[16];

	// Clés du en-passant
	uint_fast64_t _en_passant_keys[8];

	// Les clés sont-elles générées ?
	bool _keys_generated = false;

	// Constructeur par défaut
	Zobrist();

	// Fonction qui génère les clés de Zobrist
	void generate_zobrist_keys();

};

// Entrée dans la table de transposition
struct ZobristEntry {
public:

	// Indice du plateau dans le buffer
	int _board_index = -1;

	// Potentiellement utile pour le quiescence search

	// Profondeur de la recherche
	int _depth = 0;

	// Pruning?


	// Constructeur par défaut
	ZobristEntry();

	// Constructeur à partir d'un indice
	ZobristEntry(const int board_index);

};

// Table de transposition (structure: unordered_map)
class TranspositionTable {
public:
	// Table de transposition
	unordered_map<uint64_t, ZobristEntry> _hash_table; // FIXME: il faut gérer la taille de la table de transposition

	// Zobrist utilisé
	Zobrist _zobrist;

	// La table est-elle initialisée?
	bool _init = false;

	// Taille de la table de transposition
	int _length = 0;

	// Constructeur par défaut de la table de transposition
	TranspositionTable();

	// Fonction qui initialise la table de transposition d'une longueur donnée (nombre d'éléments)
	void init(const int length = 5000000, const Zobrist* zobrist = nullptr, bool display = false);

	// Fonction qui renvoie l'indice du plateau dans le buffer (s'il existe)
	int get_zobrist_position_buffer_index(uint_fast64_t key);
};

// Instance de la table de transposition
extern TranspositionTable transposition_table;
