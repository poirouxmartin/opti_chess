#pragma once
#include <cstdint>

// Possibilités d'optimisation
// -> Les pions ne peuvent pas être sur la première et dernière rangée

// Classe qui gère les clés de Zobrist
class Zobrist
{
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

	// Fonctions

	// Fonction qui génère les clés de Zobrist
	void generate_zobrist_keys();

};

// Instance de la classe Zobrist
extern Zobrist zobrist;
