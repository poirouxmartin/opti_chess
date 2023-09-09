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
	_kings_opposition = other._kings_opposition;
	_push = other._push;
	_rook_open = other._rook_open;
	_square_controls = other._square_controls;
	_space_advantage = other._space_advantage;
	_alignments = other._alignments;
	_piece_activity = other._piece_activity;
	_fianchetto = other._fianchetto;
}

// Constructeur avec paramètres
Evaluator::Evaluator(const double piece_value, const double piece_mobility, const double piece_positioning, const double random_add, const double bishop_pair, const double castling_rights, const double player_trait, const double king_safety, const double pawn_structure, const double attacks, const double defenses, const double kings_opposition, const double push, const double rook_open, const double square_controls, const double space_advantage, const double alignments, const double piece_activity, const double fianchetto) {
	// Initialise les paramètres d'évaluation
	_piece_value = piece_value;
	_piece_mobility = piece_mobility;
	_piece_positioning = piece_positioning;
	_random_add = random_add;
	_bishop_pair = bishop_pair;
	_castling_rights = castling_rights;
	_player_trait = player_trait;
	_king_safety = king_safety;
	_pawn_structure = pawn_structure;
	_attacks = attacks;
	_kings_opposition = kings_opposition;
	_push = push;
	_rook_open = rook_open;
	_square_controls = square_controls;
	_space_advantage = space_advantage;
	_alignments = alignments;
	_piece_activity = piece_activity;
	_fianchetto = fianchetto;
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
	_kings_opposition = other._kings_opposition;
	_push = other._push;
	_rook_open = other._rook_open;
	_square_controls = other._square_controls;
	_space_advantage = other._space_advantage;
	_alignments = other._alignments;
	_piece_activity = other._piece_activity;
	_fianchetto = other._fianchetto;
	return *this;
}