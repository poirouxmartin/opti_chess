#include "evaluation.h"

// Constructeur par défaut
Evaluator::Evaluator() {
}

// Constructeur par copie
Evaluator::Evaluator(const Evaluator& other) {
	// Recopie les paramètres d'évaluation
	_piece_value = other._piece_value;
	_piece_mobility = other._piece_mobility;
	_piece_positioning = other._piece_positioning;
	_random_add = other._random_add;
	_bishop_pair = other._bishop_pair;
	_castling_rights = other._castling_rights;
	_player_trait = other._player_trait;
	_king_safety = other._king_safety;
	_pawn_structure = other._pawn_structure;
	_attacks = other._attacks;
	_defenses = other._defenses;
	_kings_opposition = other._kings_opposition;
	_push = other._push;
	_rook_open = other._rook_open;
	_square_controls = other._square_controls;
	_space_advantage = other._space_advantage;
	_alignments = other._alignments;
	_piece_activity = other._piece_activity;
}

// Opérateur de copie
Evaluator& Evaluator::operator=(const Evaluator& other) {
	// Recopie les paramètres d'évaluation
	_piece_value = other._piece_value;
	_piece_mobility = other._piece_mobility;
	_piece_positioning = other._piece_positioning;
	_random_add = other._random_add;
	_bishop_pair = other._bishop_pair;
	_castling_rights = other._castling_rights;
	_player_trait = other._player_trait;
	_king_safety = other._king_safety;
	_pawn_structure = other._pawn_structure;
	_attacks = other._attacks;
	_defenses = other._defenses;
	_kings_opposition = other._kings_opposition;
	_push = other._push;
	_rook_open = other._rook_open;
	_square_controls = other._square_controls;
	_space_advantage = other._space_advantage;
	_alignments = other._alignments;
	_piece_activity = other._piece_activity;
	return *this;
}