#include "board.h"
#include "gui.h"
#include "useful_functions.h"
#include "zobrist.h"

#include <algorithm>
#include <chrono>
#include <ranges>
#include <string>
#include <sstream>
#include <cmath>
#include <utility>
#include <iomanip>
#include <vector>
void Board::game_advancement() {
	if (_advancement)
		return;

	_adv = 0;

	// Local definition of game progress: (p_tot - p) / p_tot, where p_tot is the starting material total (opponent only? both?) and p the current one
	static constexpr int adv_pawn = 2;
	static constexpr int adv_knight = 10;
	static constexpr int adv_bishop = 10;
	static constexpr int adv_rook = 10;
	static constexpr int adv_queen = 50;
	static constexpr int adv_castle = 5;

	// Threshold above which the position counts as an endgame
	static constexpr int endgame_adv = 35;

	static constexpr int p_tot = 2 * (8 * adv_pawn + 2 * adv_knight + 2 * adv_bishop + 2 * adv_rook + 1 * adv_queen + 2 * adv_castle);
	int p = 0;

	static constexpr int values[6] = { 0, adv_pawn, adv_knight, adv_bishop, adv_rook, adv_queen };

	// Pieces
	uint64_t occ = _occupancies[2];
	while (occ) {
		const int sq = pop_lsb(occ);
		const uint8_t piece = _array[sq >> 3][sq & 7];
		piece && (p += values[piece % 6]);
	}

	// Roques
	p += (_castling_rights.k_w + _castling_rights.q_w + _castling_rights.k_b + _castling_rights.q_b) * adv_castle;

	_adv = min(1.0f, static_cast<float>(p_tot - p) / (p_tot - endgame_adv));

	return;
}

// Counts the material on the board and returns its value
// Phase 7a component timers (seconds). Defined here, dumped in dump_qstats.
thread_local double g_t_king_safety_s = 0.0;
thread_local double g_t_mobility_s = 0.0;
thread_local double g_t_matpos_s = 0.0;
thread_local double g_t_pawns_s = 0.0;
thread_local double g_t_endgame_s = 0.0;

int Board::count_material(const Evaluator* eval, float closed_factor) const
{
	int material_count = 0;

	uint64_t occ = _occupancies[2];
	while (occ) {
		const int sq = pop_lsb(occ);
		const uint8_t piece = _array[sq >> 3][sq & 7];
		const int piece_number = (piece - 1) % 6;
		const int piece_begin_value = (1.0f - closed_factor) * eval->_pieces_value_begin_open[piece_number] + closed_factor * eval->_pieces_value_begin_closed[piece_number];
		const int piece_end_value = (1.0f - closed_factor) * eval->_pieces_value_end_open[piece_number] + closed_factor * eval->_pieces_value_end_closed[piece_number];

		const int value = static_cast<int>(static_cast<float>(piece_begin_value) * (1.0f - _adv) + static_cast<float>(piece_end_value) * _adv);

		material_count += (piece < 7) ? value : -value;
	}

	return material_count;
}

// Counts the bishop pairs and returns their value
int Board::count_bishop_pairs() const
{
	//rnnqk2r/ppp1nppp/4p1n1/3pP3/3P1P2/8/PPP3PP/RBBQKBBR w KQkq - 1 5: two bishop pairs for White

	uint8_t w_bishop_light = 0; uint8_t w_bishop_dark = 0;
	uint8_t b_bishop_light = 0; uint8_t b_bishop_dark = 0;

	// Iterate only bishops using bitboards
	uint64_t wb = _bitboards[w_bishop];
	while (wb) {
		const int sq = pop_lsb(wb);
		if ((sq >> 3) + (sq & 7) & 1)
			w_bishop_dark++;
		else
			w_bishop_light++;
	}
	uint64_t bb = _bitboards[b_bishop];
	while (bb) {
		const int sq = pop_lsb(bb);
		if ((sq >> 3) + (sq & 7) & 1)
			b_bishop_dark++;
		else
			b_bishop_light++;
	}

	//cout << "w_bishop_light: " << (int)w_bishop_light << " w_bishop_dark: " << (int)w_bishop_dark << endl;
	//cout << "b_bishop_light: " << (int)b_bishop_light << " b_bishop_dark: " << (int)b_bishop_dark << endl;

	return min(w_bishop_light, w_bishop_dark) - min(b_bishop_light, b_bishop_dark);
}

// Counts and returns the penalty value for doubled pieces
int Board::count_doubled_pieces(const Evaluator* eval) const
{
	int penalties = 0;

	// Piece counters by type
	uint8_t piece_counts[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	uint64_t occ = _occupancies[2];
	while (occ) {
		const int sq = pop_lsb(occ);
		const uint8_t piece = _array[sq >> 3][sq & 7];
		piece_counts[piece - 1]++;
	}

	for (uint8_t i = 0; i < 12; i++) {
		if (piece_counts[i] > 1) {
			const int piece_number = i % 6;
			const int penalty = eval->_doubled_piece_penalty[piece_number];
			penalties += (piece_counts[i] - 1) * penalty * ((i < 6) ? -1 : 1);
		}
	}

	return penalties;
}

// Computes and returns the piece placement value
int Board::pieces_positioning(const Evaluator* eval) const
{
	int pos = 0;

	uint64_t occ = _occupancies[2];
	while (occ) {
		const int sq = pop_lsb(occ);
		const uint8_t i = sq >> 3;
		const uint8_t j = sq & 7;
		if (const uint8_t piece = _array[i][j]) {
			const int value = static_cast<int>(static_cast<float>(eval->_pieces_pos_begin[(piece - 1) % 6][(piece < 7) ? 7 - i : i][j]) * (1.0f - _adv) + static_cast<float>(eval->_pieces_pos_end[(piece - 1) % 6][(piece < 7) ? 7 - i : i][j]) * _adv);
			pos += (piece < 7) ? value : -value;
		}
	}

	return pos;
}

// Evaluates the position using heuristics
void Board::evaluate(Evaluation* eval, Evaluator* evaluator, bool display, Network* n, bool check_game_over)
{
	/*if (_evaluated)
		return false;*/

	// If the transposition table already has it evaluated
	//if (_zobrist_key == 0) {
	//	cout << "Zobrist key is 0" << endl;
	//	get_zobrist_key();
	//}

	//if (transposition_table.contains(_zobrist_key)) {
	//	cout << "Transposition table hit" << endl;
	//	if (transposition_table._hash_table[_zobrist_key]._node->_board->_evaluated) {
	//		_evaluation = transposition_table._hash_table[_zobrist_key]._node->_deep_evaluation._value;
	//		cout << "Evaluation: " << _evaluation << endl;
	//		_evaluated = true;
	//		return true;
	//	}
	//}

	//if (_evaluated) {
	//	cout << "already evaluated: " << to_fen() << ", eval: " << _evaluation << endl;
	//}

	if (check_game_over) {
		//is_game_over();

		// Draw
		if (_game_over_value == draw) {
			eval->_value = 0;
			eval->_evaluated = true;

			if (display)
				main_GUI._eval_components = "DRAW\n";

			eval->_uncertainty = 0;
			eval->_winnable_black = 0;
			eval->_winnable_white = 0;
			eval->get_WDL();
			eval->get_average_score();

			return;
		}

		// Mat
		if (_game_over_value != unterminated) {
			eval->_value = (-mate_value + _moves_count * mate_ply) * get_color();
			eval->_evaluated = true;

			if (display)
				main_GUI._eval_components = "CHECKMATE\n";

			eval->_uncertainty = 0;
			eval->_winnable_white = (_game_over_value == white_win) ? 1 : 0;
			eval->_winnable_black = (_game_over_value == black_win) ? 1 : 0;
			eval->get_WDL();
			eval->get_average_score();

			return;
		}
	}

	// If a neural network is available
	if (n != nullptr) {
		n->input_from_fen(to_fen());
		n->calculate_output();
		//_evaluation = n->_output;
		eval->_value = n->output_eval(mate_value);

		eval->_uncertainty = 0;
		eval->_winnable_black = 1;
		eval->_winnable_white = 1;
		eval->get_WDL();
		eval->get_average_score();

		// The evaluation has been performed
		eval->_evaluated = true;

		// Game not over
		return;
	}

	_displayed_components = display;
	if (display)
		main_GUI._eval_components = "";

	// Reset the evaluation
	eval->_value = 0;

	// Game progress
	game_advancement();
	if (display)
		main_GUI._eval_components += "ADVANCEMENT: " + to_string(static_cast<int>(round(100 * _adv))) + "%\n";

	// Nature of the position (open/closed)
	auto t_matpos0 = std::chrono::steady_clock::now();
	const float position_nature = get_position_nature();
	if (display)
		main_GUI._eval_components += "CLOSED: " + to_string(static_cast<int>(position_nature * 100.0f)) + "%\n";

	// *** MATERIEL ***

	if (display)
		main_GUI._eval_components += "\nMATERIAL\n";

	int total_material = 0;

	// Material
	if (evaluator->_piece_value != 0.0f) {
		const int material = count_material(evaluator, position_nature) * evaluator->_piece_value;
		if (display)
			main_GUI._eval_components += "material: " + (material >= 0 ? string("+") : string()) + to_string(material) + "\n";
		total_material += material;
	}	

	// Paire de oufs
	if (evaluator->_bishop_pair != 0.0f) {
		const int bishop_pair = count_bishop_pairs() * evaluator->_bishop_pair * (1 - position_nature);
		if (display)
			main_GUI._eval_components += "bishop pair: " + (bishop_pair >= 0 ? string("+") : string()) + to_string(bishop_pair) + "\n";
		total_material += bishop_pair;
	}

	// Doubled pieces
	if (evaluator->_doubled_pieces != 0.0f) {
		const int doubled_pieces = count_doubled_pieces(evaluator) * evaluator->_doubled_pieces;
		if (display)
			main_GUI._eval_components += "doubled pieces: " + (doubled_pieces >= 0 ? string("+") : string()) + to_string(doubled_pieces) + "\n";
		total_material += doubled_pieces;
	}

	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_material >= 0 ? string("+") : string()) + to_string(total_material) + " ---\n";

	eval->_value += total_material;

	// *** POSITIONNEMENT ***

	if (display)
		main_GUI._eval_components += "\nPOSITIONING\n";

	int total_positioning = 0;

	// Piece placement
	if (evaluator->_piece_positioning != 0.0f) {
		const int positioning = pieces_positioning(evaluator) * evaluator->_piece_positioning;
		if (display)
			main_GUI._eval_components += "piece positioning: " + (positioning >= 0 ? string("+") : string()) + to_string(positioning) + "\n";
		total_positioning += positioning;
	}

	// Rooks on open and semi-open files
	if (evaluator->_open_files != 0.0f) {
		const int rook_open = get_sliders_on_open_file() * evaluator->_open_files;
		if (display)
			main_GUI._eval_components += "sliders on open/semi files: " + (rook_open >= 0 ? string("+") : string()) + to_string(rook_open) + "\n";
		total_positioning += rook_open;
	}

	// Fianchettoed bishops
	if (evaluator->_fianchetto != 0.0f) {
		const int fianchetto = get_fianchetto_value() * evaluator->_fianchetto * (1.0f - position_nature);
		if (display)
			main_GUI._eval_components += "fianchetto bishops: " + (fianchetto >= 0 ? string("+") : string()) + to_string(fianchetto) + "\n";
		total_positioning += fianchetto;
	}

	// Piece alignments (bishop-rook / queen-king)
	if (evaluator->_alignments != 0.0f)
	{
		const int pieces_alignment = get_alignments() * evaluator->_alignments;
		if (display)
			main_GUI._eval_components += "pieces alignment: " + (pieces_alignment >= 0 ? string("+") : string()) + to_string(pieces_alignment) + "\n";
		total_positioning += pieces_alignment;
	}

	// Trapped pieces
	if (evaluator->_trapped_pieces != 0.0f) {
		const int trapped_pieces = get_trapped_pieces() * evaluator->_trapped_pieces;
		if (display)
			main_GUI._eval_components += "trapped pieces: " + (trapped_pieces >= 0 ? string("+") : string()) + to_string(trapped_pieces) + "\n";
		total_positioning += trapped_pieces;
	}

	// Pawn push threatening an enemy piece
	if (evaluator->_pawn_push_threats != 0.0f) {
		const int pawn_push_threat = get_pawn_push_threats() * evaluator->_pawn_push_threats;
		if (display)
			main_GUI._eval_components += "pawn push threats: " + (pawn_push_threat >= 0 ? string("+") : string()) + to_string(pawn_push_threat) + "\n";
		total_positioning += pawn_push_threat;
	}

	// Queen safety
	if (evaluator->_queen_safety != 0.0f) {
		const int queen_safety = (get_queen_safety(true) - get_queen_safety(false)) * evaluator->_queen_safety;
		if (display)
			main_GUI._eval_components += "queen safety: " + (queen_safety >= 0 ? string("+") : string()) + to_string(queen_safety) + "\n";
		total_positioning += queen_safety;
	}

	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_positioning >= 0 ? string("+") : string()) + to_string(total_positioning) + " ---\n";

	eval->_value += total_positioning;
	g_t_matpos_s += std::chrono::duration<double>(std::chrono::steady_clock::now() - t_matpos0).count();
	auto t_pawns0 = std::chrono::steady_clock::now();


	// *** ACTIVITE ***

	if (display)
		main_GUI._eval_components += "\nACTIVITY\n";

	int total_activity = 0;

	// Piece mobility
	//if (eval->_piece_mobility) {
	//	const int piece_mobility = get_piece_mobility() * eval->_piece_mobility;
	//	if (display)
	//		main_GUI._eval_components += "piece mobility: " + (piece_mobility >= 0 ? string("+") : string()) + to_string(piece_mobility) + "\n";
	//	total_activity += piece_mobility;
	//}

	// Long-term piece mobility
	if (evaluator->_long_term_piece_mobility != 0.0f) {
		auto t_mob0 = std::chrono::steady_clock::now();
		const int long_term_mobility = get_long_term_piece_mobility() * evaluator->_long_term_piece_mobility;
		g_t_mobility_s += std::chrono::duration<double>(std::chrono::steady_clock::now() - t_mob0).count();
		if (display)
			main_GUI._eval_components += "long-term piece mobility: " + (long_term_mobility >= 0 ? string("+") : string()) + to_string(long_term_mobility) + "\n";
		total_activity += long_term_mobility;
	}

	// Short-term piece mobility
	if (evaluator->_short_term_piece_mobility != 0.0f) {
		auto t_mob0 = std::chrono::steady_clock::now();
		const int short_term_mobility = get_short_term_piece_mobility() * evaluator->_short_term_piece_mobility;
		g_t_mobility_s += std::chrono::duration<double>(std::chrono::steady_clock::now() - t_mob0).count();
		if (display)
			main_GUI._eval_components += "short-term piece mobility: " + (short_term_mobility >= 0 ? string("+") : string()) + to_string(short_term_mobility) + "\n";
		total_activity += short_term_mobility;
	}

	// Piece activity
	if (evaluator->_piece_activity != 0.0f) {
		auto t_mob0 = std::chrono::steady_clock::now();
		const int piece_activity = get_piece_activity() * evaluator->_piece_activity;
		g_t_mobility_s += std::chrono::duration<double>(std::chrono::steady_clock::now() - t_mob0).count();
		if (display)
			main_GUI._eval_components += "piece activity: " + (piece_activity >= 0 ? string("+") : string()) + to_string(piece_activity) + "\n";
		total_activity += piece_activity;
	}

	// Knight activity
	if (evaluator->_knight_activity != 0.0f) {
		const int knight_activity = get_knight_activity() * evaluator->_knight_activity;
		if (display)
			main_GUI._eval_components += "knight activity: " + (knight_activity >= 0 ? string("+") : string()) + to_string(knight_activity) + "\n";
		total_activity += knight_activity;
	}

	// Bishop activity
	if (evaluator->_bishop_activity != 0.0f) {
		const int bishop_activity = get_bishop_activity() * evaluator->_bishop_activity;
		if (display)
			main_GUI._eval_components += "bishop activity: " + (bishop_activity >= 0 ? string("+") : string()) + to_string(bishop_activity) + "\n";
		total_activity += bishop_activity;
	}

	// Rook activity
	if (evaluator->_rook_activity != 0.0f) {
		const int rook_activity = get_rook_activity() * evaluator->_rook_activity;
		if (display)
			main_GUI._eval_components += "rook activity: " + (rook_activity >= 0 ? string("+") : string()) + to_string(rook_activity) + "\n";
		total_activity += rook_activity;
	}

	// Piece attacks and defences
	if (evaluator->_attacks != 0.0f) {
		const int pieces_attacks_and_defenses = get_attacks_and_defenses() * evaluator->_attacks;
		if (display)
			main_GUI._eval_components += "attacks/defenses: " + (pieces_attacks_and_defenses >= 0 ? string("+") : string()) + to_string(pieces_attacks_and_defenses) + "\n";
		total_activity += pieces_attacks_and_defenses;
	}

	// Side to move
	if (evaluator->_player_trait != 0.0f) {
		const int player_trait = evaluator->_player_trait * get_color() * (1 - position_nature);
		//const int player_trait = eval->_player_trait * get_color() * (1 - position_nature) * (1.0f + 1.0f * _adv);
		if (display)
			main_GUI._eval_components += "player trait: " + (player_trait >= 0 ? string("+") : string()) + to_string(player_trait) + "\n";
		total_activity += player_trait;
	}

	//if (display)
	//	main_GUI._eval_components += "SUB-TOTAL: " + (total_activity >= 0 ? string("+") : string()) + to_string(total_activity) + "\n";

	//// Adjustment based on the nature of the position
	//if (display)
	//	main_GUI._eval_components += "position nature: x" + to_string((int)(100 - 100 * position_nature)) + "%\n";
	//total_activity *= 1 - position_nature;


	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_activity >= 0 ? string("+") : string()) + to_string(total_activity) + " ---\n";

	eval->_value += total_activity;

	// *** TACTIQUE ***

	if (display)
		main_GUI._eval_components += "\nTACTICS\n";

	int total_tactics = 0;

	// Fourchettes
	if (display)
		main_GUI._eval_components += "forks: TODO\n";
	//if (eval->_forks != 0.0f) {
	//	const int forks = get_forks() * eval->_forks;
	//	if (display)
	//		main_GUI._eval_components += "forks: " + (forks >= 0 ? string("+") : string()) + to_string(forks) + "\n";
	//	total_tactics += forks;
	//}

	// Pins (TODO: move them here)

	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_tactics >= 0 ? string("+") : string()) + to_string(total_tactics) + " ---\n";

	// *** PAWN STRUCTURE ***

	if (display)
		main_GUI._eval_components += "\nPAWN STRUCTURE\n";

	int total_pawn_structure = 0;

	// Square control
	if (evaluator->_square_controls != 0.0f) {
		const int square_controls = get_square_controls() * evaluator->_square_controls;
		if (display)
			main_GUI._eval_components += "square controls: " + (square_controls >= 0 ? string("+") : string()) + to_string(square_controls) + "\n";
		total_pawn_structure += square_controls;
	}

	// Avantage d'espace
	if (evaluator->_space_advantage != 0.0f)
	{
		const int space = get_space() * evaluator->_space_advantage * position_nature;
		if (display)
			main_GUI._eval_components += "space: " + (space >= 0 ? string("+") : string()) + to_string(space) + "\n";
		total_pawn_structure += space;
	}

	// Pawn structure
	if (evaluator->_pawn_structure != 0.0f) {
		const int pawn_structure = get_pawn_structure(display * evaluator->_pawn_structure) * evaluator->_pawn_structure;
		//if (display)
		//	main_GUI._eval_components += "pawn structure: " + (pawn_structure >= 0 ? string("+") : string()) + to_string(pawn_structure) + "\n";
		total_pawn_structure += pawn_structure;
	}

	// Good and bad bishops
	if (evaluator->_bishop_pawns != 0.0f) {
		const int bishop_pawns = get_bishop_pawns() * evaluator->_bishop_pawns;
		if (display)
			main_GUI._eval_components += "bishop pawns: " + (bishop_pawns >= 0 ? string("+") : string()) + to_string(bishop_pawns) + "\n";
		total_pawn_structure += bishop_pawns;
	}

	// Weak squares and outposts
	if (evaluator->_weak_squares != 0.0f) {
		const int weak_squares = (-get_weak_squares(true) + get_weak_squares(false)) * evaluator->_weak_squares * (1.0f + position_nature);
		if (display)
			main_GUI._eval_components += "weak squares: " + (weak_squares >= 0 ? string("+") : string()) + to_string(weak_squares) + "\n";
		total_pawn_structure += weak_squares;
	}

	// Evaluation with all of its components
	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_pawn_structure >= 0 ? string("+") : string()) + to_string(total_pawn_structure) + " ---\n";

	g_t_pawns_s += std::chrono::duration<double>(std::chrono::steady_clock::now() - t_pawns0).count();
	auto t_endgame0 = std::chrono::steady_clock::now();
	eval->_value += total_pawn_structure;


	// *** KING ***

	if (display)
		main_GUI._eval_components += "\nKING\n";

	int total_king = 0;

	// King safety
	if (evaluator->_king_safety != 0.0f) {
		auto t_king0 = std::chrono::steady_clock::now();
		const int king_safety = get_king_safety(total_activity, display * evaluator->_king_safety) * evaluator->_king_safety;
		g_t_king_safety_s += std::chrono::duration<double>(std::chrono::steady_clock::now() - t_king0).count();
		if (display)
			main_GUI._eval_components += "king safety: " + (king_safety >= 0 ? string("+") : string()) + to_string(king_safety) + "\n";
		total_king += king_safety;
	}

	// Droits de roques
	if (evaluator->_castling_rights != 0.0f) {
		const int castling_rights = evaluator->_castling_rights * (_castling_rights.k_w + _castling_rights.q_w - _castling_rights.k_b - _castling_rights.q_b) * (1 - _adv);
		if (display)
			main_GUI._eval_components += "castling rights: " + (castling_rights >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(castling_rights))) + "\n";
		total_king += castling_rights;
	}

	// Distance to castling
	//if (eval->_castling_distance != 0.0f) {
	//	const int castling_distance = get_castling_distance() * eval->_castling_distance;
	//	if (display)
	//		main_GUI._eval_components += "castling distance: " + (castling_distance >= 0 ? string("+") : string()) + to_string(castling_distance) + "\n";
	//	total_king += castling_distance;
	//}

	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_king >= 0 ? string("+") : string()) + to_string(total_king) + " ---\n";

	eval->_value += total_king;


	// *** FINALES ***

	if (display)
		main_GUI._eval_components += "\nENDGAME\n";

	int total_endgame = 0;

	// King opposition
	if (evaluator->_kings_opposition != 0.0f) {
		const int kings_opposition = get_kings_opposition() * evaluator->_kings_opposition;
		if (display)
			main_GUI._eval_components += "king opposition: " + (kings_opposition >= 0 ? string("+") : string()) + to_string(kings_opposition) + "\n";
		total_endgame += kings_opposition;
	}

	// King proximity to the pawns in the endgame
	if (evaluator->_king_proximity != 0.0f) {
		const int king_proximity = get_king_proximity() * evaluator->_king_proximity;
		if (display)
			main_GUI._eval_components += "king proximity: " + (king_proximity >= 0 ? string("+") : string()) + to_string(king_proximity) + "\n";
		total_endgame += king_proximity;
	}

	// King centralisation
	if (evaluator->_king_centralization != 0.0f) {
		const int king_centralization = (get_king_centralization(true) - get_king_centralization(false)) * evaluator->_king_centralization;
		if (display)
			main_GUI._eval_components += "king centralization: " + (king_centralization >= 0 ? string("+") : string()) + to_string(king_centralization) + "\n";
		total_endgame += king_centralization;
	}

	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_endgame >= 0 ? string("+") : string()) + to_string(total_endgame) + " ---\n";

	eval->_value += total_endgame;

	// *** NATURE OF THE POSITION ***

	if (display)
		main_GUI._eval_components += "\nPOSITION NATURE\n";

	int total_nature = 0;

	// Forteresse
	if (evaluator->_push != 0.0f) {
		const float push = 1 - static_cast<float>(_half_moves_count) * evaluator->_push / max_half_moves;
		const int fortress = 100.0f - push * 100.0f;
		const int fortress_value = eval->_value * (push - 1);
		if (display)
			main_GUI._eval_components += "fortress: " + to_string(fortress) + "% (" + (fortress_value >= 0 ? string("+") : string()) + to_string(fortress_value) + ")\n";
		total_nature += fortress_value;
	}

	// Evaluation uncertainty
	get_uncertainty(eval, total_material);
	const int uncertainty_percent = (int)(100 * eval->_uncertainty);
	if (display)
		main_GUI._eval_components += "uncertainty: " + to_string(uncertainty_percent) + "%\n";

	// Is the position winnable?
	get_winnable_values(eval, position_nature);

	if (display)
		main_GUI._eval_components += "winnable: " + to_string(static_cast<int>(eval->_winnable_white * 100)) + "% / " + to_string(static_cast<int>(eval->_winnable_black * 100)) + "%\n";

	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_nature >= 0 ? string("+") : string()) + to_string(total_nature) + " ---\n";

	eval->_value += total_nature;


	// *** TOTAL ***
	if (display) {
		main_GUI._eval_components += "\nTOTAL COMPONENTS\n";
		main_GUI._eval_components += "Material: " + (total_material >= 0 ? string("+") : string()) + to_string(total_material) + "\n";
		main_GUI._eval_components += "Positioning: " + (total_positioning >= 0 ? string("+") : string()) + to_string(total_positioning) + "\n";
		main_GUI._eval_components += "Activity: " + (total_activity >= 0 ? string("+") : string()) + to_string(total_activity) + "\n";
		main_GUI._eval_components += "Tactics: " + (total_tactics >= 0 ? string("+") : string()) + to_string(total_tactics) + "\n";
		main_GUI._eval_components += "Pawn structure: " + (total_pawn_structure >= 0 ? string("+") : string()) + to_string(total_pawn_structure) + "\n";
		main_GUI._eval_components += "King: " + (total_king >= 0 ? string("+") : string()) + to_string(total_king) + "\n";
		main_GUI._eval_components += "Endgame: " + (total_endgame >= 0 ? string("+") : string()) + to_string(total_endgame) + "\n";
		main_GUI._eval_components += "Nature: " + (total_nature >= 0 ? string("+") : string()) + to_string(total_nature) + "\n";

		main_GUI._eval_components += "_______________\nTOTAL: " + (eval->_value >= 0 ? string("+") : string()) + to_string(eval->_value) + "\n";

	}

	// Chances de gain
	//const float win_chance = get_winning_chances_from_eval(_evaluation, true);
	//if (display)
	//	main_GUI._eval_components += "W/D/L: " + to_string(static_cast<int>(100 * win_chance)) + "/" + to_string(static_cast<int>(100 * 0)) + "/" + to_string(static_cast<int>(100 * (1.0f - win_chance))) + "%\n";

	eval->get_WDL();
	g_t_endgame_s += std::chrono::duration<double>(std::chrono::steady_clock::now() - t_endgame0).count();
	eval->get_average_score();

	if (display) {
		main_GUI._eval_components += "Confidence: " + to_string(100 - uncertainty_percent) + "%\n";
		main_GUI._eval_components += eval->_wdl.to_string() + "\n";
		main_GUI._eval_components += "Score: " + score_string(eval->_avg_score) + "\n";
	}

	// The evaluation has been performed
	eval->_evaluated = true;

	// EXPERIMENTAL
	//Node node(this);
	//transposition_table._hash_table[_zobrist_key] = &node;


	// Game not over
	return;
}

// Loads the board from a FEN
// TODO: rewrite this

// Returns the winner when the game is over
// Also generates the legal moves, when there are any
int Board::material_difference() const
{
	int mat = 0;
	int w_material[6] = { 0, 0, 0, 0, 0, 0 };
	int b_material[6] = { 0, 0, 0, 0, 0, 0 };

	// Initialize mat for empty squares
	mat += main_GUI._piece_GUI_values[0] * 64;

	uint64_t occ = _occupancies[2];
	while (occ) {
		const int sq = pop_lsb(occ);
		const uint8_t p = _array[sq >> 3][sq & 7];
		if (p < 6)
			w_material[p]++;
		else
			b_material[p % 6]++;

		mat += main_GUI._piece_GUI_values[p % 6] * (1 - (p / 6) * 2) - main_GUI._piece_GUI_values[0];
	}

	for (uint8_t i = 0; i < 6; i++) {
		main_GUI._missing_w_material[i] = max(0, main_GUI._base_material[i] - w_material[i]);
		main_GUI._missing_b_material[i] = max(0, main_GUI._base_material[i] - b_material[i]);
	}

	return mat;
}

// Resets the evaluation components
void Board::reset_eval() {
	_displayed_components = false;
	_advancement = false; _adv = 0;
	_controls_map_valid = false;
	_pawns_controls_valid = false;
}
int Board::get_updated_piece_values() const {
	// Rook penalty based on the number of non-open files
	// Penalty for bishops in a closed position, with the diagonals shut
	// Same for the queen. Bonus in the opposite cases

	// *** TODO ***
	return 0;
}

// Returns the nature of the position as a number: 0 = open, 1 = closed

