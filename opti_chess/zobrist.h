#pragma once
#include <cstdint>



// Classe qui g�re les cl�s de Zobrist
class Zobrist
{
public:
	// Variables

	// Cl�s du plateau
	uint_fast64_t _board_keys[64][12];

	// Cl� du trait
	uint_fast64_t _player_key;

	// Cl�s des roques
	uint_fast64_t _castling_keys[16];

	// Cl�s du en-passant
	uint_fast64_t _en_passant_keys[8];

	// Fonctions

	// Fonction qui g�n�re les cl�s du plateau
	uint_fast64_t generate_board_keys();
};
