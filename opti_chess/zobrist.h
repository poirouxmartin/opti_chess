#pragma once
#include <cstdint>



// Classe qui gère les clés de Zobrist
class Zobrist
{
public:
	// Variables

	// Clés du plateau
	uint_fast64_t _board_keys[64][12];

	// Clé du trait
	uint_fast64_t _player_key;

	// Clés des roques
	uint_fast64_t _castling_keys[16];

	// Clés du en-passant
	uint_fast64_t _en_passant_keys[8];

	// Fonctions

	// Fonction qui génère les clés du plateau
	uint_fast64_t generate_board_keys();
};
