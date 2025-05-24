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
	_bishop_pair = other._bishop_pair;
	_castling_rights = other._castling_rights;
	_player_trait = other._player_trait;
	_king_safety = other._king_safety;
	_pawn_structure = other._pawn_structure;
	_attacks = other._attacks;
	_kings_opposition = other._kings_opposition;
	_push = other._push;
	_open_files = other._open_files;
	_square_controls = other._square_controls;
	_space_advantage = other._space_advantage;
	_alignments = other._alignments;
	_piece_activity = other._piece_activity;
	_fianchetto = other._fianchetto;
	_pawn_push_threats = other._pawn_push_threats;
	_king_proximity = other._king_proximity;
	_rook_activity = other._rook_activity;
	_bishop_pawns = other._bishop_pawns;
	_weak_squares = other._weak_squares;
	_castling_distance = other._castling_distance;
	_bishop_activity = other._bishop_activity;
	_trapped_pieces = other._trapped_pieces;
	_knight_activity = other._knight_activity;
	_king_centralization = other._king_centralization;
	_short_term_piece_mobility = other._short_term_piece_mobility;
	_long_term_piece_mobility = other._long_term_piece_mobility;
	_queen_safety = other._queen_safety;
}

// Constructeur avec paramètres
Evaluator::Evaluator(const float piece_value, const float piece_mobility, const float piece_positioning, const float bishop_pair, const float castling_rights, const float player_trait, const float king_safety, const float pawn_structure, const float attacks, const float defenses, const float kings_opposition, const float push, const float rook_open, const float square_controls, const float space_advantage, const float alignments, const float piece_activity, const float fianchetto, const float pawn_push_threats, const float king_proximity, const float rook_activity, const float bishop_pawns, const float weak_squares, const float castling_distance, const float bishop_activity, const float trapped_pieces, const float knight_activity, const float king_centralization, const float short_term_piece_mobility, const float long_term_piece_mobility, const float queen_safety) {
	// Initialise les paramètres d'évaluation
	_piece_value = piece_value;
	_piece_mobility = piece_mobility;
	_piece_positioning = piece_positioning;
	_bishop_pair = bishop_pair;
	_castling_rights = castling_rights;
	_player_trait = player_trait;
	_king_safety = king_safety;
	_pawn_structure = pawn_structure;
	_attacks = attacks;
	_kings_opposition = kings_opposition;
	_push = push;
	_open_files = rook_open;
	_square_controls = square_controls;
	_space_advantage = space_advantage;
	_alignments = alignments;
	_piece_activity = piece_activity;
	_fianchetto = fianchetto;
	_pawn_push_threats = pawn_push_threats;
	_king_proximity = king_proximity;
	_rook_activity = rook_activity;
	_bishop_pawns = bishop_pawns;
	_weak_squares = weak_squares;
	_castling_distance = castling_distance;
	_bishop_activity = bishop_activity;
	_trapped_pieces = trapped_pieces;
	_knight_activity = knight_activity;
	_king_centralization = king_centralization;
	_short_term_piece_mobility = short_term_piece_mobility;
	_long_term_piece_mobility = long_term_piece_mobility;
	_queen_safety = queen_safety;
}

// Opérateur de copie
Evaluator& Evaluator::operator=(const Evaluator& other) {
	// Recopie les paramètres d'évaluation
	_piece_value = other._piece_value;
	_piece_mobility = other._piece_mobility;
	_piece_positioning = other._piece_positioning;
	_bishop_pair = other._bishop_pair;
	_castling_rights = other._castling_rights;
	_player_trait = other._player_trait;
	_king_safety = other._king_safety;
	_pawn_structure = other._pawn_structure;
	_attacks = other._attacks;
	_kings_opposition = other._kings_opposition;
	_push = other._push;
	_open_files = other._open_files;
	_square_controls = other._square_controls;
	_space_advantage = other._space_advantage;
	_alignments = other._alignments;
	_piece_activity = other._piece_activity;
	_fianchetto = other._fianchetto;
	_pawn_push_threats = other._pawn_push_threats;
	_king_proximity = other._king_proximity;
	_rook_activity = other._rook_activity;
	_bishop_pawns = other._bishop_pawns;
	_weak_squares = other._weak_squares;
	_castling_distance = other._castling_distance;
	_bishop_activity = other._bishop_activity;
	_trapped_pieces = other._trapped_pieces;
	_knight_activity = other._knight_activity;
	_king_centralization = other._king_centralization;
	_short_term_piece_mobility = other._short_term_piece_mobility;
	_long_term_piece_mobility = other._long_term_piece_mobility;
	_queen_safety = other._queen_safety;

	return *this;
}

// Evaluation de la position
// TODO !!!
//float Evaluator::evaluate(const Board& board)
//{
//	return 0.0f;
//}
