#include "zobrist.h"
#include <random>

using namespace std;

// Fonction qui g�n�re les cl�s de Zobrist
void Zobrist::generate_zobrist_keys() {
	
	// Initialisation du g�n�rateur al�atoire
	random_device rd;
	mt19937_64 gen(rd());
	uniform_int_distribution<uint_fast64_t> dis(0, UINT_FAST64_MAX);

	// G�n�ration des cl�s

	// Valeur initiale de la cl�
	uint_fast64_t _initial_key = dis(gen);
	
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

// Instance de Zobrist
Zobrist zobrist;