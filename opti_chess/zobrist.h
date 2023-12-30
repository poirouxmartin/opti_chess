#pragma once
#include <cstdint>

// Possibilit�s d'optimisation
// -> Les pions ne peuvent pas �tre sur la premi�re et derni�re rang�e

// Classe qui g�re les cl�s de Zobrist
class Zobrist
{
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

	// Fonctions

	// Fonction qui g�n�re les cl�s de Zobrist
	void generate_zobrist_keys();

};

// Instance de la classe Zobrist
extern Zobrist zobrist;
