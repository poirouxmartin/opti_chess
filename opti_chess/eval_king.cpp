#include "board.h"
#include "exploration.h"
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
thread_local long long g_ks_hits = 0;
thread_local long long g_ks_miss = 0;
thread_local long long g_ks_clears = 0;
extern const bool g_qstats_on;
// Gated king-safety sub-timers (zero-cost unless OPTI_QSTATS): KST0 opens a
// span, KST_ACC closes into var. Single now() pair per span.
#define KST0(tag) auto _kst_##tag = g_qstats_on ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point()
#define KST_ACC(tag, var) do { if (g_qstats_on) var += std::chrono::duration<double>(std::chrono::steady_clock::now() - _kst_##tag).count(); } while (0)
int Board::get_king_safety(int activity_diff, float display_factor) {

	// ----------------------
	// *** POSITIONS TEST ***
	// ----------------------

	// TODO: factor mobility into king safety
	// Rework the whole potential as a non-linear function, with 0 = cannot mate and 1 = can mate?

	// r1bq1b1r/pp4pp/2p1k3/3np3/1nB5/2N2Q2/PPPP1PPP/R1B2RK1 w - - 0 10 vs r1b2bnr/pppp1k1p/2n2q2/8/5B2/2N2Q2/PPP3PP/R4RK1 b - - 2 12
	// 4rb1r/pp3kpp/2p1b3/3nB3/2BP4/P7/1PP2PPP/4RRK1 w - - 0 18
	// 4r3/p3bkp1/r7/1pPpBP1p/1P1P4/P2b2P1/5R1P/4R1K1 w - - 1 28: the king should be safe
	// r1bq1b1r/ppp3pp/2n1k3/3np3/2B5/5Q2/PPPP1PPP/RNB1K2R w KQ - 2 8
	// r1b2b1r/ppp3pp/8/3kp3/8/8/PPPP1PPP/R1B1K2R w KQ - 0 12
	// 8/2p1k1pp/p1Qb4/3P3q/4p3/N1P1BnPb/P4P2/5R1K w - - 1 25
	// 5rk1/6p1/pq1b3p/3p4/2p1n3/PP3N1P/4p1P1/RQR4K w - - 2 31: white king very weak (mated)
	// 3r1rk1/pp1bbp2/1qp1pn1Q/4N3/3P4/2PB4/PP3PPP/R3R1K1 b - - 0 16
	// 2k3r1/p1b4p/2p5/3P3r/8/5bP1/PP3P2/2R2RK1 w - - 0 7
	// r1bq1rk1/ppppnpp1/8/2bNp1PQ/1nB1P3/2P5/PP1P1PP1/R1B1K2R b KQ - 2 3
	// r1bq1rk1/pp2npp1/2n1p3/2ppP1NQ/3P4/P1P5/2P2PPP/R1B1K2R b KQ - 3 3
	// r3kb1r/pR2pppp/2p5/3p4/3P2b1/B3RN2/q1P2PPP/3Q2K1 b kq - 1 14 : overload++
	// r4k1r/pRQ3pp/2p1pp2/3p1b2/3P4/R4N1P/2q2PP1/6K1 b - - 2 20 : mat imparable
	// r1b1k2r/p1p2ppp/2p5/8/5P1q/3B1R1P/PBP3P1/Q5K1 w kq - 3 17: the black king is the weaker one
	// 1r4k1/p2n1pp1/2p1b2p/3p3P/4pQ2/2q1P3/P1P1BPP1/2KR3R w - - 1 23: Black is mating
	// rnbr2k1/ppq2p2/2pb1npQ/6N1/7R/3B2P1/PPP2P1P/2KR4 b - - 2 17: White is mating
	// 3rk2r/ppp2ppq/2p1b3/2P5/4P1P1/2P3P1/PPQ1B3/RNB2RK1 w k - 1 7: nearly equal
	// 3rk2r/ppp2pp1/2p5/2P5/4P3/2P3P1/PPQN1KR1/R1B4q b k - 2 12: Rh2 then perpetual
	// 2k2r2/ppp3pp/1bp1b3/8/4Pp1q/1N1B1Pn1/PP3RPP/R2QB1K1 w - - 8 6: white king not very safe
	// 2k2r2/ppp3pp/1bp1b3/8/4Pp1q/1N1B1Pn1/PPQ2RPP/R3B1K1 b - - 9 6 : Dxh2+!! #5
	// 2k5/ppp3pp/1bp1b2r/8/4Pp2/1N1B1Pn1/PPQ2RP1/R3B1K1 w - - 3 9 : #1 imparable
	// 8/p7/r3pk2/8/1P2Kp2/P1R2P2/5P2/8 b - - 3 39: white king not in danger
	// 2rk3q/1pp5/p4n2/1P1p1bp1/2PQ1b2/N2p4/P2P2PP/R1B1R2K w - - 0 23: white king is lost
	// r1b1k2r/pppp2pp/2n5/4Pp2/8/BB3N2/P1PQ2PP/5K2 b kq - 0 15: the king is actually in trouble
	// r1bq1b1r/pp4pp/2p1k3/3np3/1nBP4/2N2Q2/PPP2PPP/R1B2RK1 b - - 0 10: +2.5 / +5 for king safety
	// r3r1k1/2p2pp1/1p1p3p/pPn4q/2PN3n/P3PP1P/2Q2P1K/B2R2R1 w - - 7 6: already completely winning for White
	// r1bq1rk1/pp1nbpn1/2p1p3/8/2pP4/2N1PN2/PPQ2P1P/2KR1BR1 b - - 1 6: winning for White -> black king too weak, open files and diagonals, no pawns in front either; all 6 white pieces can attack while only 4 black pieces can defend
	// 1r1qr1k1/p2n1pn1/b1p1pb1Q/4N3/1ppPN3/4P3/PP3P1P/2KR1BR1 b - - 9 12: lost for Black
	// r1b3kr/pppp3p/2n2Q2/8/5N2/4p3/PPP3PP/6K1 b - - 2 19: winning for White
	// r1b3kr/ppp4p/2np1Q2/7N/8/4p3/PPP3PP/6K1 b - - 1 20 : #1 imparable
	// rnb2bnr/pppp1k1p/8/8/5p2/4BQ2/PqP3PP/RN3RK1 w - - 0 11: winning for White
	// 6k1/5pp1/5r2/7K/P5PP/2Nr1n2/1P6/8 b - - 0 38 : #1 imparable...
	// r3k2r/pp1n1pp1/2n1b2p/2p1P3/5P2/P4NP1/1PPKBB1P/3R3R w kq - 0 18: here the king is better on c1 than e3
	// r1b3kr/pppp3p/2n2Q2/3N4/8/4p3/PPP3PP/6K1 w - - 1 19: winning for White
	// r1b1r2k/pp3pp1/2n4n/3qp3/2Np4/3B1N1P/PP3PP1/RQ2R1K1 w - - 0 17: black king not that endangered; the bishop/queen battery achieves nothing, the queen is only half attacking
	// 8/1rp3p1/4k2p/8/7P/2R2KP1/5P2/8 w - - 6 58: king is fine
	// 6R1/5p2/5kp1/2q5/pp4B1/2n1R3/5PKP/8 b - - 5 45: black king is fine
	// 1r6/7p/p1P1p3/4kp2/1P1Rp3/4KPP1/8/8 b - - 0 49 : pareil...
	// 2bk1r2/4b1Qp/8/1p6/P2P4/1qp5/4NPPP/R1K2B1R w - - 1 25: winning for Black

	// 8/6PK/5k2/8/8/8/8/8 b - - 0 8


	// Update the king positions
	update_kings_pos();
	// Cache REMOVED (was pure but useless: 11% hits didn't pay for
	// unordered_map find+insert on every call + clear storms; bypass proved
	// bit-identical on NODES-150).

	// Number of files between the kings, to detect opposite-side castling for instance
	const int king_columns_diff = abs(_white_king_pos.col - _black_king_pos.col);


	// King weaknesses
	int w_king_weakness = 0;
	int b_king_weakness = 0;

	// ---------------------------
	// *** POTENTIEL D'ATTAQUE ***
	// ---------------------------

	// rnb2bnr/pppp1k1p/5q2/8/5B2/5Q2/PPP3PP/RN3RK1 b - - 0 11
	// 8/8/8/2r2pp1/1k5p/2b4P/4K3/1Q6 b - - 81 133: the queen has more potential than rook and bishop combined

	// Attacking potential of each piece (pawn, knight, bishop, rook, queen)
	static constexpr int attack_potentials[6] = { 5, 30, 35, 55, 125, 0 };
	constexpr int reference_attack_potential = 405; // If every starting piece is still on the board

	// Defensive potential
	static constexpr int defense_potentials[6] = { 5, 25, 20, 15, 10, 0 };
	constexpr int reference_defense_potential = 170; // If every starting piece is still on the board

	// Attacking potential required to mate comfortably
	// r1b3nr/ppppk2p/2n5/8/5N2/1Q6/PPP3PP/R6K w - - 1 18
	constexpr int needed_potential = 40;
	//constexpr int needed_potential = 0;

	// Attacking potential values
	int w_total_attack_potential = 0;
	int b_total_attack_potential = 0;

	// Defensive potential values
	int w_total_defense_potential = 0;
	int b_total_defense_potential = 0;

	// Bishops of each side
	bool w_bishop_w = false;
	bool w_bishop_b = false;
	bool b_bishop_w = false;
	bool b_bishop_b = false;

	uint64_t occ = _occupancies[2];
	while (occ) {
		const int sq = pop_lsb(occ);
		const uint8_t row = sq >> 3;
		const uint8_t col = sq & 7;
		const uint8_t p = _array[row][col];

		if (is_white(p)) {
			w_total_attack_potential += attack_potentials[p - 1];
			w_total_defense_potential += defense_potentials[p - 1];
		}
		else {
			b_total_attack_potential += attack_potentials[(p - 1) % 6];
			b_total_defense_potential += defense_potentials[(p - 1) % 6];
		}

		if (p == w_bishop) {
			if ((row + col) % 2 == 0)
				w_bishop_w = true;
			else
				w_bishop_b = true;
		}
		else if (p == b_bishop) {
			if ((row + col) % 2 == 0)
				b_bishop_w = true;
			else
				b_bishop_b = true;
		}
	}

	// With an opposite-coloured bishop, add attacking potential proportional to the current one
	//constexpr float opposite_bishop_potential = 1.25f;

	//cout << "bishops: " << w_bishop_w << " " << w_bishop_b << " " << b_bishop_w << " " << b_bishop_b << endl;

	if (((w_bishop_w && !w_bishop_b) && (!b_bishop_w && b_bishop_b)) || ((!w_bishop_w && w_bishop_b) && (b_bishop_w && !b_bishop_b))) {
		//cout << "opposite bishop" << endl;
		//w_total_attack_potential *= 1 + (opposite_bishop_potential - 1) * w_total_attack_potential / reference_attack_potential;
		//b_total_attack_potential *= 1 + (opposite_bishop_potential - 1) * b_total_attack_potential / reference_attack_potential;
		w_total_defense_potential -= defense_potentials[2] * 0.5f;
		b_total_defense_potential -= defense_potentials[2] * 0.5f;
	}

	//r1b3k1/pp3ppp/5q2/2Pr4/4p3/1NQ1K1N1/PP2B1PP/R7 b - - 1 24
	//8/8/2p4k/2Pp4/3P1n2/8/3K4/8 w - - 0 58

	//cout << "w_total_attack_potential: " << w_total_attack_potential << endl;
	//cout << "b_total_attack_potential: " << b_total_attack_potential << endl;

	//cout << "w_total_defense_potential: " << w_total_defense_potential << endl;
	//cout << "b_total_defense_potential: " << b_total_defense_potential << endl;

	// Normalised attacking potential
	const float w_attack_potential_normalized = (float)(w_total_attack_potential - needed_potential) / (reference_attack_potential - needed_potential);
	const float b_attack_potential_normalized = (float)(b_total_attack_potential - needed_potential) / (reference_attack_potential - needed_potential);

	//cout << "w_attack_potential_normalized: " << w_attack_potential_normalized << endl;
	//cout << "b_attack_potential_normalized: " << b_attack_potential_normalized << endl;

	// Normalised defensive potential
	const float w_defense_potential_normalized = (float)w_total_defense_potential / reference_defense_potential;
	const float b_defense_potential_normalized = (float)b_total_defense_potential / reference_defense_potential;

	//cout << "w_defense_potential_normalized: " << w_defense_potential_normalized << endl;
	//cout << "b_defense_potential_normalized: " << b_defense_potential_normalized << endl;

	// Potentiel minimum d'attaque
	//const float min_attack_potential = -needed_potential / (float)reference_attack_potential;

	// Potentiel final d'attaque
	float w_attacking_potential = 2.0f * max(0.0f, w_attack_potential_normalized) / (1.0f + w_defense_potential_normalized);
	float b_attacking_potential = 2.0f * max(0.0f, b_attack_potential_normalized) / (1.0f + b_defense_potential_normalized);

	//// Potentiel total
	//const int w_total_potential = max(0, w_total_attack_potential - b_total_defense_potential - needed_potential);
	//const int b_total_potential = max(0, b_total_attack_potential - w_total_defense_potential - needed_potential);

	//cout << "w_total_potential: " << w_total_potential << endl;
	//cout << "b_total_potential: " << b_total_potential << endl;

	//// Reference total potential
	//const int reference_potential = reference_attack_potential - reference_defense_potential - needed_potential;

	//// Normalised potential
	//float w_attacking_potential = (float)w_total_potential / reference_potential;
	//float b_attacking_potential = (float)b_total_potential / reference_potential;

	//cout << "w_attacking_potential: " << w_attacking_potential << endl;
	//cout << "b_attacking_potential: " << b_attacking_potential << endl;


	//1r1q1k2/2n5/p2p4/2pp4/6QN/8/1PP1N1PP/7K b - - 0 29
	// r3r1k1/5p2/2p2b1B/p2bpP1Q/8/1Pq4P/6PK/4RR2 w - - 2 29: capturing on b3 lowers White's potential???

	// 3rr1k1/2p2ppp/1bp2n2/pp6/4PB2/2PPN2q/PPQ1BP2/R4RK1 b - - 3 9: Black still has drawing potential here

	// Non-linear function
	constexpr double alpha = 2.0;
	w_attacking_potential *= w_attacking_potential;
	b_attacking_potential *= b_attacking_potential;

	//cout << "w_attacking_potential: " << w_attacking_potential << endl;
	//cout << "b_attacking_potential: " << b_attacking_potential << endl;

	// Constant keeping a floor of potential
	constexpr float min_potential = 0.0f;

	// 2k5/ppp3Bp/2p4r/8/b3Pp2/3B1Pn1/PP3KP1/RQ6 b - - 0 1

	// Always add a minimum potential, so any weakening is accounted for (currently min_potential=0, so this is a no-op)
	//w_attacking_potential = max(w_attacking_potential, min_potential * pow((float)w_total_attack_potential / reference_attack_potential, 0.35f));
	//b_attacking_potential = max(b_attacking_potential, min_potential * pow((float)b_total_attack_potential / reference_attack_potential, 0.35f));


	// Potentiel d'attaque
	//const float w_attacking_potential = ((float)w_total_attack_potential / reference_potential + min_potential) / (1 + min_potential);
	//const float b_attacking_potential = ((float)b_total_attack_potential / reference_potential + min_potential) / (1 + min_potential);

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "----------\n";
		main_GUI._eval_components += "Attacking potential: " + to_string(w_attacking_potential) + " / " + to_string(b_attacking_potential) + "\n";
	}

	// Facteurs multiplicatifs
	constexpr float piece_attack_factor = 1.0f;
	constexpr float piece_defense_factor = 1.0f;
	constexpr float pawn_protection_factor = 0.6f;

	// When the resultant is positive or negative
	constexpr float piece_overload_multiplicator = 1.0f; // TODO: put this to use
	constexpr float piece_defense_multiplicator = 1.0f;

	// --------------------------
	// *** ESPACE DE MOBILITE ***
	// --------------------------

	// TEST (unsure)
	constexpr float space_safety_factor = 0.35f;

	KST0(space);
	const int space = get_space();
	KST_ACC(space, g_t_ks_misc_s);

	const int w_space = space_safety_factor * space;
	const int b_space = -space_safety_factor * space;

	// ---------------------------
	// *** PIECE ACTIVITY ***
	// ---------------------------

	// TEST (unsure)
	constexpr float activity_attacking_factor = 1.0f;
	constexpr float activity_protection_factor = 0.5f;

	const int activity = activity_diff > 0 ? sqrt(activity_diff / 100.0) * 100 : -sqrt(-activity_diff / 100.0) * 100;

	const int w_activity = activity > 0 ? activity * activity_protection_factor : activity * activity_attacking_factor;
	const int b_activity = activity < 0 ? -activity * activity_protection_factor : -activity * activity_attacking_factor;

	//rnbqkbnr/ppp2ppp/3p4/4p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w kq - 3 7: g8 and similar moves are wrong even with more activity

	// -------------------------------------
	// *** POWER COMPUTATION ***
	// * ATTAQUES - DEFENSES - PROTECTIONS *
	// -------------------------------------

	// King shielding
	KST0(shield);
	int w_king_protection = get_pawn_shield_protection(true, b_attacking_potential, w_space) * pawn_protection_factor;
	int b_king_protection = get_pawn_shield_protection(false, w_attacking_potential, b_space) * pawn_protection_factor;
	KST_ACC(shield, g_t_ks_power_s);

	// Attaquants
	KST0(atk);
	int w_attacking_power = get_king_attackers(true);
	int b_attacking_power = get_king_attackers(false);
	KST_ACC(atk, g_t_ks_power_s);

	// The more attack there is, the harder it is to defend even with many defenders -> exponential?
	// Threshold above which attacking power counts double
	constexpr int doubled_attack = 800;
	float w_mult_attack = 1.0f + w_attacking_power * w_attacking_potential / static_cast<float>(doubled_attack);
	float b_mult_attack = 1.0f + b_attacking_power * b_attacking_potential/ static_cast<float>(doubled_attack);

	w_attacking_power *= w_mult_attack;
	b_attacking_power *= b_mult_attack;

	w_attacking_power *= piece_attack_factor;
	b_attacking_power *= piece_attack_factor;

	// Defenders
	//int w_defending_power = get_king_defenders(true) * piece_defense_factor * (1.0f + 0.5f * (1.0f - b_attacking_potential));
	//int b_defending_power = get_king_defenders(false) * piece_defense_factor * (1.0f + 0.5f * (1.0f - w_attacking_potential));

	KST0(def);
	int w_defending_power = get_king_defenders(true) * piece_defense_factor * (1.0f + 0.35f * (1.0f - b_attacking_potential));
	int b_defending_power = get_king_defenders(false) * piece_defense_factor * (1.0f + 0.35f * (1.0f - w_attacking_potential));
	KST_ACC(def, g_t_ks_power_s);

	// Defence by the king alone
	//constexpr int king_defense = 200;

	//w_defending_power += king_defense;
	//b_defending_power += king_defense;

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "Attacking power: " + to_string(w_attacking_power) + " / " + to_string(b_attacking_power) + "\n";
		main_GUI._eval_components += "Defending power: " + to_string(w_defending_power) + " / " + to_string(b_defending_power) + "\n";
		//main_GUI._eval_components += "King protection: " + to_string(w_king_protection) + " / " + to_string(b_king_protection) + "\n";
	}


	// -----------------
	// *** OVERLOADS ***
	// -----------------

	// TODO: extract a function for this
	//rnq1k2r/pp2bp2/2p5/3p4/5Pb1/P2P1NPp/1PP4K/R1BQ1R1N b kq - 0 17: overload on our own h3 pawn??
	// r3k2r/ppqn3n/3b1p2/2ppp1p1/4P2p/P2P1P1P/1PPBBN1K/R1NQ1R2 b kq - 5 22 : overload +495???

	// Fetch the square control maps
	KST0(ctrl);
	SquareMap white_controls_map = get_white_controls_map();
	SquareMap black_controls_map = get_black_controls_map();
	KST_ACC(ctrl, g_t_ks_maps_s);

	// Is this useful?
	//white_controls_map.print();
	//black_controls_map.print();

	// Net control
	SquareMap controls_map = white_controls_map - black_controls_map;

	//controls_map.print();

	// Overload danger: squares controlled in our favour near the enemy king
	constexpr uint8_t overloard_distance_dangers[8] = { 50, 35, 5, 1, 0, 0, 0, 0 };


	// Overload on the white king
	int w_king_overloaded = 0;

	// Controlled squares near the king
	for (uint8_t i = 0; i < 8; i++) {
		for (uint8_t j = 0; j < 8; j++) {
			// Piece on this square
			uint8_t p = _array[i][j];

			if (controls_map._array[i][j] < 0 && p <= w_king)
			{
				const uint8_t distance = max(abs(i - _white_king_pos.row), abs(j - _white_king_pos.col));
				w_king_overloaded -= overloard_distance_dangers[distance] * controls_map._array[i][j]; // minus, because the value is negative
				//cout << "square: " << square_name(i, j) << ", piece: " << piece_name(p) << " / distance: " << (int)distance << " / overload: " << overloard_distance_dangers[distance] * controls_map._array[i][j] << endl;
			}
		}
	}
	
	// Overload on the black king
	int b_king_overloaded = 0;

	// Attacks across the board
	for (uint8_t i = 0; i < 8; i++) {
		for (uint8_t j = 0; j < 8; j++) {
			// Piece on this square
			uint8_t p = _array[i][j];

			if (controls_map._array[i][j] > 0 && (p >= b_pawn || p == none))
			{
				const uint8_t distance = max(abs(i - _black_king_pos.row), abs(j - _black_king_pos.col));
				b_king_overloaded += overloard_distance_dangers[distance] * controls_map._array[i][j];
				//cout << "square: " << square_name(i, j) << ", piece: " << piece_name(p) << " / distance: " << (int)distance << " / overload: " << overloard_distance_dangers[distance] * controls_map._array[i][j] << endl;
			}
		}
	}

	const float overload_factor = 2.5f;
	//const float overload_factor = 0.0f;

	w_king_overloaded *= overload_factor * b_attacking_potential;
	b_king_overloaded *= overload_factor * w_attacking_potential;

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "Overloaded: " + to_string(w_king_overloaded) + " / " + to_string(b_king_overloaded) + "\n";
	//}


	// -------------------
	// *** PROTECTIONS ***
	// -------------------

	//rnq1k2r/pp2bpp1/2p1bn2/3pp3/7p/P2PP2P/1PPNBPP1/R1BQ1RKN w kq - 2 11: the king on h2 is not that bad
	//r3k2r/ppq2p2/2p1bP2/3pn3/8/P2PPB2/1PPNK2p/R1BQ3R b kq - 7 21: bug on black queenside castling???
	//r3k3/p1q2p1r/4b3/1p1p4/6P1/P2PPQb1/1P1NB1P1/R1B2RK1 b q - 6 22: castling is needed to bring another rook into the attack
	//Nnb2b1r/1p1k1p1p/p4p2/8/3p4/8/PP2PPPP/R3KB1R b KQ - 0 12

	// -----------------------
	// *** POSITION DU ROI ***
	// -----------------------

	// Proximity to the edge
	// Progress threshold beyond which sitting on an edge becomes more dangerous
	//constexpr float edge_adv = 0.85f;
	//constexpr float mult_endgame = 25.0f;
	//constexpr float safe_zone = 0.25f;

	//// Additive version, suited to the endgame
	//constexpr int edge_defense = 75;
	
	//8/8/1k6/3Q4/4K3/8/8/8 w - - 19 136
	//r1k2b1r/p5p1/2p4p/8/4p1b1/4B3/PPP2P1P/2KR2R1 w - - 0 21: 0 before taking the bishop, 200+ after

	// r1bq1b1r/ppp3pp/4k3/3np3/1nB5/2N3Q1/PPPP1PPP/R1B1K2R b KQ - 5 9 : Rf7 vs Rf5... analyser...

	// Distances to the edges
	/*int w_col_dist = min(_white_king_pos.col, 7 - _white_king_pos.col);
	int w_row_dist = min(_white_king_pos.row, 7 - _white_king_pos.row);
	int b_col_dist = min(_black_king_pos.col, 7 - _black_king_pos.col);
	int b_row_dist = min(_black_king_pos.row, 7 - _black_king_pos.row);

	int w_placement_weakness = edge_defense * ((edge_adv - _adv) * ((_adv < edge_adv) ? (max(0, w_col_dist - 1) + _white_king_pos.row * _white_king_pos.row / 2.0f) : (mult_endgame / (edge_adv - 1.0f) * (1.0f / ((w_col_dist + 1) * (w_row_dist + 1)) - safe_zone))));
	int b_placement_weakness = edge_defense * ((edge_adv - _adv) * ((_adv < edge_adv) ? (max(0, b_col_dist - 1) + (7 - _black_king_pos.row) * (7 - _black_king_pos.row) / 2.0f) : (mult_endgame / (edge_adv - 1.0f) * (1.0f / ((b_col_dist + 1) * (b_row_dist + 1)) - safe_zone))));*/

	const int w_placement_weakness = get_king_placement_weakness(true);
	const int b_placement_weakness = get_king_placement_weakness(false);

	//cout << b_placement_weakness << endl;

	// While castling is available, placement problems are ignored
	//if (_castling_rights.k_b || _castling_rights.q_b)
	//	b_placement_weakness = 0;
	//if (_castling_rights.k_w || _castling_rights.q_w)
	//	w_placement_weakness = 0;

	//const float placement_factor = 1.0f;

	//w_placement_weakness *= placement_factor;
	//b_placement_weakness *= placement_factor;

	//2k5/8/8/3QK3/8/8/8/8 b - - 26 139

	//cout << "distances: " << w_col_dist << " " << w_row_dist << " / " << b_col_dist << " " << b_row_dist << endl;
	//cout << (edge_adv - _adv) * (endgame_safe_zone - (b_col_dist + 1) * (b_row_dist + 1)) * mult_endgame / (edge_adv - 1.0f) << endl;
	//cout << "placement: " << w_placement_weakness << " / " << b_placement_weakness << endl;

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "King placement weakness: " + to_string(w_placement_weakness) + " / " + to_string(b_placement_weakness) + "\n";
	//}

	// ---------------------------------
	// *** VIRTUAL KING MOBILITY ***
	// ---------------------------------
	
	// FIXME: is this actually useful? it may break more than it fixes
	//constexpr int virtual_mobility_danger = 20;
	constexpr int virtual_mobility_danger = 0;

	// Mobility beyond which the king is in danger
	constexpr int virtual_mobility_threshold = 3;

	KST0(virt);
	const int w_virtual_mobility = 0;
	const int b_virtual_mobility = 0;
	KST_ACC(virt, g_t_ks_misc_s);

	// ---------------------
	// *** RANK WEAKNESS ***
	// ---------------------

	// TODO *********
	KST0(rank);
	const int w_rank_weakness = get_king_row_weakness(true);
	const int b_rank_weakness = get_king_row_weakness(false);
	KST_ACC(rank, g_t_ks_misc_s);

	// TODO

	// --------------------
	// *** WEAK SQUARES ***
	// --------------------

	KST0(weak);
	const int w_weak_squares = get_weak_squares(true, true) * b_attacking_potential;
	const int b_weak_squares = get_weak_squares(false, true) * w_attacking_potential;
	KST_ACC(weak, g_t_ks_maps_s);

	// ------------------
	// *** MATING NET ***
	// ------------------

	// TODO *********

	// ------------------
	// *** PAWN STORM ***
	// ------------------

	constexpr float pawn_storm_danger = 1.5f;

	KST0(storm);
	int w_pawn_storm = get_pawn_storm(true) * pawn_storm_danger * w_attacking_potential;
	int b_pawn_storm = get_pawn_storm(false) * pawn_storm_danger * b_attacking_potential;
	KST_ACC(storm, g_t_ks_maps_s);

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "Pawn storms: " + to_string(w_pawn_storm) + " / " + to_string(b_pawn_storm) + "\n";
	//}

	// ------------------
	// *** OPEN LINES ***
	// ------------------

	//5rk1/5ppp/4bn2/q2p4/2p5/b1P1BN2/3Q1PPP/1K1RRB2 w - - 2 9
	//r1b2k1r/ppp1qpp1/3bP1pn/3P4/4N3/8/PPPBQ1PP/2KR1R2 b - - 6 18

	constexpr float open_lines_danger = 2.25f;

	KST0(open);
	int w_open_lines = get_open_files_on_opponent_king(true) * open_lines_danger;
	int b_open_lines = get_open_files_on_opponent_king(false) * open_lines_danger;
	KST_ACC(open, g_t_ks_maps_s);

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "Open lines: " + to_string(w_open_lines) + " / " + to_string(b_open_lines) + "\n";
	//}


	// ----------------------
	// *** OPEN DIAGONALS ***
	// ----------------------

	constexpr float open_diagonals_danger = 0.0f;

	// Identical-value skip: multiplied by zero below; the calls walk the
	// board for nothing (perf).
	int w_open_diagonals = 0;
	int b_open_diagonals = 0;

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "Open diagonals: " + to_string(w_open_diagonals) + " / " + to_string(b_open_diagonals) + "\n";
	//}


	// --------------
	// *** CHECKS ***
	// --------------

	const int w_checks = get_checks_value(&white_controls_map, &black_controls_map, true) * w_attacking_potential;
	const int b_checks = get_checks_value(&white_controls_map, &black_controls_map, false) * b_attacking_potential;

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "Checks: " + to_string(w_checks) + " / " + to_string(b_checks) + "\n";
	//}

	// -------------------------------------
	// *** KING WEAKNESS COMPUTATION ***
	// -------------------------------------


	// Long-term weaknesses grow when the kings are far apart
	const float king_distance_factor = 1.0f * (1 - _adv);

	constexpr float col_diff_factors[8] = { 0.0f, 0.02f, 0.12f, 0.35f, 0.75f, 0.85f, 0.90f, 0.95f };

	// Weakness amplified when the kings are far apart
	const float long_term_weakness_distance_factor = 0.75f * (1 + king_distance_factor * col_diff_factors[king_columns_diff]);

	// Short-term attack amplified when the kings are far apart
	const float short_term_weakness_distance_factor = 0.75f * (1 + king_distance_factor * col_diff_factors[king_columns_diff]);



	// Multiplier on a negative weakness, balancing short against long term
	const float negative_long_term_factor = 1.0f;

	// Compensation
	// TESTS
	// In theory the short term can never fully repay the long-term weaknesses, so a small share always remains
	// When the short term turns positive, should it grow with the long-term weaknesses too?

	//const float base_compensation = 0.20f / (short_term_weakness_distance_factor * short_term_weakness_distance_factor);
	//const float w_negative_short_term_factor = base_compensation + (1 - b_attacking_potential) * 0.25f;
	//const float b_negative_short_term_factor = base_compensation + (1 - w_attacking_potential) * 0.25f;

	// Short term = k, short term = max(0, short_term) - factor * long_term / short_term
	// Short term = 0 -> factor = 0
	// Short term = -500 -> factor = -0.33
	// Short term = -1000 -> factor = -0.5
	// Short term = 1000 -> factor = 0.5

	// Value at which the compensation equals 0.5
	constexpr int short_term_compensation_value = 500;

	//const float w_short_term_compensation_factor = 1 - 1 / (1 + abs(w_short_term_weakness) / short_term_compensation_value);

	// rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w kq - 4 4

	// Black king (White attacking)

	// Attack/Defense overload
	int w_attacking_overload = w_attacking_power - b_defending_power;
	if (w_attacking_overload > 0) {
		w_attacking_overload *= piece_overload_multiplicator;
	}
	else {
		w_attacking_overload *= piece_defense_multiplicator;
	}
	

	// Faiblesses long terme:
	int b_long_term_weakness = w_pawn_storm + w_open_lines + w_open_diagonals - b_king_protection + b_placement_weakness + b_virtual_mobility + b_weak_squares + b_rank_weakness - b_space;

	// Weakness amplified when the kings are far apart
	b_long_term_weakness *= long_term_weakness_distance_factor;


	//if (b_long_term_weakness > 0) {
	//	b_long_term_weakness *= w_attacking_potential;
	//}

	// Reduce when castling is still available?
	if (_castling_rights.k_b || _castling_rights.q_b) {
		//b_long_term_weakness *= 0.5f;
	}

	if (b_long_term_weakness < 0) {
		b_long_term_weakness *= negative_long_term_factor;
	}

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "B LONG TERM WEAKNESS: (";
		main_GUI._eval_components += "Storm: " + to_string(w_pawn_storm);
		main_GUI._eval_components += " + Lines: " + to_string(w_open_lines);
		main_GUI._eval_components += " + Diags (REDO): " + to_string(w_open_diagonals);
		main_GUI._eval_components += " - Protec: " + to_string(b_king_protection);
		main_GUI._eval_components += " + Placement: " + to_string(b_placement_weakness);
		main_GUI._eval_components += " + Exposure (?): " + to_string(b_virtual_mobility);
		main_GUI._eval_components += " + Weak squares: " + to_string(b_weak_squares);
		main_GUI._eval_components += " + Rank weakness (TODO): " + to_string(b_rank_weakness);
		main_GUI._eval_components += " + Mating nets (TODO): " + to_string(0);
		main_GUI._eval_components += " - Space: " + to_string(b_space);
		main_GUI._eval_components += ") * Kings distance : " + to_string(long_term_weakness_distance_factor);
		main_GUI._eval_components += " = " + to_string(b_long_term_weakness) + "\n";
	}

	// Attaque court terme:
	int b_short_term_weakness = w_checks + w_attacking_overload + b_king_overloaded - b_activity;

	// Weakness amplified when the kings are far apart
	b_short_term_weakness *= short_term_weakness_distance_factor;

	// Short-term / long-term compensation, between 0 and 1
	const float b_short_term_compensation_factor = w_attacking_potential <= 0.0f ? 1.0f : 1.0f - 1.0f / (1.0f + abs(b_short_term_weakness) / static_cast<float>(short_term_compensation_value) / w_attacking_potential);
	//short term = max(0, short_term) - factor * long_term / short_term

	//cout << "b short term: " << b_short_term_weakness << " / b long term: " << b_long_term_weakness << " / b factor: " << b_short_term_compensation_factor << endl;

	b_short_term_weakness = max(0, b_short_term_weakness) + (b_short_term_weakness > 0 ? 0 : -1) * b_short_term_compensation_factor * max(0, b_long_term_weakness);

	//cout << "b final short term: " << b_short_term_weakness << endl;

	//if (b_short_term_weakness < 0) {
	//	b_short_term_weakness *= b_negative_short_term_factor;
	//}

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "B SHORT TERM WEAKNESS: (";
		main_GUI._eval_components += "Checks: " + to_string(w_checks);
		main_GUI._eval_components += " + Attack : " + to_string(w_attacking_overload);
		main_GUI._eval_components += " + Overload : " + to_string(b_king_overloaded);
		main_GUI._eval_components += " - Activity: " + to_string(b_activity);
		main_GUI._eval_components += ") * Kings distance : " + to_string(short_term_weakness_distance_factor);
		main_GUI._eval_components += " = " + to_string(b_short_term_weakness) + "\n";
	}

	b_king_weakness = b_long_term_weakness + b_short_term_weakness;

	// Based on the attacking potential
	//b_king_weakness *= w_attacking_potential;


	// White king (Black attacking)

	// Attack/Defense overload
	int b_attacking_overload = b_attacking_power - w_defending_power;
	if (b_attacking_overload > 0) {
		b_attacking_overload *= piece_overload_multiplicator;
	}
	else {
		b_attacking_overload *= piece_defense_multiplicator;
	}

	// Faiblesses long terme:
	int w_long_term_weakness = b_pawn_storm + b_open_lines + b_open_diagonals - w_king_protection + w_placement_weakness + w_virtual_mobility + w_weak_squares + w_rank_weakness - w_space;

	// Weakness amplified when the kings are far apart
	w_long_term_weakness *= long_term_weakness_distance_factor;

	//if (w_long_term_weakness > 0) {
	//	w_long_term_weakness *= b_attacking_potential;
	//}

	// Reduce when castling is still available?
	if (_castling_rights.k_w || _castling_rights.q_w) {
		//w_long_term_weakness *= 0.5f;
	}

	if (w_long_term_weakness < 0) {
		w_long_term_weakness *= negative_long_term_factor;
	}

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "W LONG TERM WEAKNESS: (";
		main_GUI._eval_components += "Storm: " + to_string(b_pawn_storm);
		main_GUI._eval_components += " + Lines: " + to_string(b_open_lines);
		main_GUI._eval_components += " + Diags (REDO): " + to_string(b_open_diagonals);
		main_GUI._eval_components += " - Protec: " + to_string(w_king_protection);
		main_GUI._eval_components += " + Placement: " + to_string(w_placement_weakness);
		main_GUI._eval_components += " + Exposure (?): " + to_string(w_virtual_mobility);
		main_GUI._eval_components += " + Weak squares: " + to_string(w_weak_squares);
		main_GUI._eval_components += " + Rank weakness (TODO): " + to_string(w_rank_weakness);
		main_GUI._eval_components += " + Mating nets (TODO): " + to_string(0);
		main_GUI._eval_components += " - Space: " + to_string(w_space);
		main_GUI._eval_components += ") * Kings distance : " + to_string(long_term_weakness_distance_factor);
		main_GUI._eval_components += " = " + to_string(w_long_term_weakness) + "\n";
	}

	// Attaque court terme:
	int w_short_term_weakness = b_checks + b_attacking_overload + w_king_overloaded - w_activity;

	// Weakness amplified when the kings are far apart
	w_short_term_weakness *= short_term_weakness_distance_factor;

	const float w_short_term_compensation_factor = b_attacking_potential <= 0.0f ? 1.0f : 1.0f - 1.0f / (1.0f + abs(w_short_term_weakness) / static_cast<float>(short_term_compensation_value) / b_attacking_potential);
	//short term = max(0, short_term) - factor * long_term / short_term

	//cout << "w short term: " << w_short_term_weakness << " / w long term: " << w_long_term_weakness << " / w factor: " << w_short_term_compensation_factor << endl;

	// 2k5/ppp3Bp/2p4r/8/b3Pp2/3B1Pn1/PP3KP1/RQ6 b - - 0 12 ?????

	w_short_term_weakness = max(0, w_short_term_weakness) + (w_short_term_weakness > 0 ? 0 : -1) * w_short_term_compensation_factor * max(0, w_long_term_weakness);

	//cout << "w final short term: " << w_short_term_weakness << endl;

	//if (w_short_term_weakness < 0) {
	//	w_short_term_weakness *= w_negative_short_term_factor;
	//}

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "W SHORT TERM WEAKNESS: (";
		main_GUI._eval_components += "Checks: " + to_string(b_checks);
		main_GUI._eval_components += " + Attack : " + to_string(b_attacking_overload);
		main_GUI._eval_components += " + Overload : " + to_string(w_king_overloaded);
		main_GUI._eval_components += " - Activity: " + to_string(w_activity);
		main_GUI._eval_components += ") * Kings distance : " + to_string(short_term_weakness_distance_factor);
		main_GUI._eval_components += " = " + to_string(w_short_term_weakness) + "\n";
	}

	w_king_weakness = w_long_term_weakness + w_short_term_weakness;

	// Based on the attacking potential
	//w_king_weakness *= b_attacking_potential;


	// Add the king shielding. King weakness cannot go negative; worth revisiting, but over-protection sometimes produced absurd values
	//float overprotection_factor = 0.15f;

	// TEST: non-linear function damping the differences around 0 and avoiding excessive over-protection

	if (w_king_weakness < 0) {
		float w_overprotection = 1.0f / (1.0f - w_king_weakness / 20.0f);
		w_king_weakness *= w_overprotection;
		//w_king_weakness = 0;
	}

	if (b_king_weakness < 0) {
		float b_overprotection = 1.0f / (1.0f - b_king_weakness / 20.0f);
		b_king_weakness *= b_overprotection;
		//b_king_weakness = 0;
	}


	// rnb1kb1r/pp2pppp/2p1q3/3n4/8/2N2N2/PPPPBPPP/R1BQ1RK1 w kq - 4 7 : test h3

	//w_king_weakness = max_int(0, w_king_weakness);
	//b_king_weakness = max_int(0, b_king_weakness);


	if (display_factor != 0.0f) {
		main_GUI._eval_components += "King weakness: " + to_string((int)(w_king_weakness)) + " / " + to_string((int)(b_king_weakness)) + "\n----------\n";
	}

	// Returns the weakness difference between the kings
	const int king_safety = b_king_weakness - w_king_weakness;

	return king_safety;
}

// Tells whether a piece can be captured by the enemy, for GUI display
int Board::get_king_virtual_mobility(bool color) {
	// FIXME: base this on the pawns only?

	// The king is replaced by a queen, and the number of possible moves is counted
	update_kings_pos();
	const int i = color ? _white_king_pos.row : _black_king_pos.row;
	const int j = color ? _white_king_pos.col : _black_king_pos.col;
	
	// Count the number of possible moves for the new queen
	int mobility = 0;

	for (uint8_t k = 0; k < 8; k++) {
		const uint8_t mi = all_directions[k][0];
		const uint8_t mj = all_directions[k][1];
		uint8_t i2 = i + mi;
		uint8_t j2 = j + mj;

		while (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8) {
			if (_array[i2][j2] != 0)
				break;
			mobility++;
			i2 += mi;
			j2 += mj;
		}
	}


	return mobility;
}

// Returns the number of safe checks in the position, for both sides
int Board::get_checks_value(SquareMap* white_controls, SquareMap* black_controls, bool color)
{
	constexpr int initial_safe_check_value = 250;
	constexpr int initial_unsafe_check_value = 25;
	constexpr float no_escape_multiplier = 2.5f;
	constexpr float inital_division = 1.0f;
	constexpr float king_escape_division_add = 0.35f;
	constexpr float piece_block_division_add = 1.00f;

	// Raise the value when the side is to move? To be tested
	//constexpr float has_trait_multiplier = 2.0f;
	constexpr float has_trait_multiplier = 1.0f;

	//3r2k1/pp3r2/2q2pp1/3n3P/7Q/7R/1B5P/4R2K b - - 0 33: an enormous number of discoveries here
	//rnb2bnr/pppp1k1p/5q2/8/5B2/5Q2/PPP3PP/RN3RK1 b - - 0 11: why are the checks better for Black?

	int safe_checks_value = 0;
	int unsafe_checks_value = 0;

	// Position of the opposing king
	update_kings_pos();
	const Pos king_pos = color ? _black_king_pos : _white_king_pos;

	// Look at every possible move for the player
	Board b(*this);
	b._player = !color;
	if (b.in_check()) // FIXME?
		return 0;


	// FIXME *** why is another board used here?
	// plus: use the move flags to tell whether it gives check
	b._player = color;
	b.get_moves();

	// Save the move count before the loop (make_move sets _got_moves = -1)
	const uint8_t num_moves = b._got_moves;

	for (uint8_t i = 0; i < num_moves; i++) {
		
		// Move
		const Move& move = b._moves[i];

		// Destination square of the move
		const uint8_t i2 = move.end_row;
		const uint8_t j2 = move.end_col;

		// Control counts of the destination square, for White and for Black
		//const uint8_t controls_ally = color ? white_controls._array[i2][j2] : black_controls._array[i2][j2];
		//const uint8_t controls_enemy = color ? black_controls._array[i2][j2] : white_controls._array[i2][j2];

		// If the destination is uncontrolled by White, or controlled only by the white king plus at least one black piece
		// FIXME: pas ouf
		//if (controls_enemy == 0 || (controls_enemy == 1 && controls_ally > 1 && abs(king_pos.i - i2) <= 1 && abs(king_pos.j - j2) <= 1)) {
		if (true) {
			// Save minimal state needed for in_check() test and loop continuation
			uint8_t saved_array[8][8];
			memcpy(saved_array, b._array, sizeof(saved_array));
			const bool saved_player = b._player;
			const Pos saved_wk = b._white_king_pos;
			const Pos saved_bk = b._black_king_pos;
			const CastlingRights saved_castling = b._castling_rights;
			const int saved_ep = b._en_passant_col;
			const int saved_half = b._half_moves_count;
			const int saved_moves_count = b._moves_count;
			uint64_t saved_bitboards[sizeof(b._bitboards) / sizeof(uint64_t)];
			memcpy(saved_bitboards, b._bitboards, sizeof(saved_bitboards));
			uint64_t saved_occupancies[sizeof(b._occupancies) / sizeof(uint64_t)];
			memcpy(saved_occupancies, b._occupancies, sizeof(saved_occupancies));

			// Play the move and see whether it gives check
			b.make_move(move);

			// TODO: replace with "does the move attack the king"?
			if (b.in_check()) {
				// Check path (rare): restore state, then use full copy for get_moves
				memcpy(b._array, saved_array, sizeof(saved_array));
				b._player = saved_player;
				b._white_king_pos = saved_wk;
				b._black_king_pos = saved_bk;
				b._castling_rights = saved_castling;
				b._en_passant_col = saved_ep;
				b._half_moves_count = saved_half;
				b._moves_count = saved_moves_count;
				memcpy(b._bitboards, saved_bitboards, sizeof(saved_bitboards));
				memcpy(b._occupancies, saved_occupancies, sizeof(saved_occupancies));
				b._got_moves = num_moves;

				Board b_check(b);
				b_check.make_move(move);
				b_check.get_moves();

				// Number of escape squares for the king
				int king_escapes = 0;

				// Number of pieces able to block the check
				int piece_blocks = 0;

				// Is the check safe?
				bool is_safe_check = true;

				for (uint8_t j = 0; j < b_check._got_moves; j++) {
					// Any capture available to the opponent makes the check unsafe (should this be restricted to the checking piece?) 
					// FIXME: bof
					uint8_t eaten_piece = b_check._array[b_check._moves[j].end_row][b_check._moves[j].end_col];
					if (eaten_piece != none) {
						is_safe_check = false;
						break;
					}

					// Piece able to prevent the check
					uint8_t piece = b_check._array[b_check._moves[j].start_row][b_check._moves[j].start_col];

					// Escape square for the king
					if (piece == (color ? b_king : w_king)) {
						king_escapes++;
					}
					else {
						piece_blocks++;
					}
				}

				// Value of the division
				float division = inital_division + king_escapes * king_escape_division_add + piece_blocks * piece_block_division_add;

				// Value of the multiplication
				float multiplier = (king_escapes == 0 && piece_blocks == 0) ? no_escape_multiplier : 1.0f;

				//cout << "is safe check: " << is_safe_check;

				if (is_safe_check) {
					// Add the safe check value
					//cout << "color: " << color << ", king_escapes : " << king_escapes << ", piece_blocks : " << piece_blocks << ", division : " << division << ", value : " << initial_safe_check_value / division << endl;
					safe_checks_value += max(multiplier * initial_safe_check_value / division, (float)initial_unsafe_check_value); // A safe check always beats an unsafe one
				}
				else {
					// Add the unsafe check value
					//cout << "color: " << color << "value : " << initial_unsafe_check_value << endl;
					unsafe_checks_value += initial_unsafe_check_value;
				}

			}

			// Restore b for the next iteration
			memcpy(b._array, saved_array, sizeof(saved_array));
			b._player = saved_player;
			b._white_king_pos = saved_wk;
			b._black_king_pos = saved_bk;
			b._castling_rights = saved_castling;
			b._en_passant_col = saved_ep;
			b._half_moves_count = saved_half;
			b._moves_count = saved_moves_count;
			memcpy(b._bitboards, saved_bitboards, sizeof(saved_bitboards));
			memcpy(b._occupancies, saved_occupancies, sizeof(saved_occupancies));
			b._got_moves = num_moves;
		}
	}

	return (safe_checks_value + unsafe_checks_value) * (_player == color ? has_trait_multiplier : 1.0f);
}

// Returns the move generation speed
int Board::get_king_proximity()
{
	// TEST: 8/8/8/1k1K3p/6p1/6P1/7P/8 w - - 0 25
	// TEST: 8/8/3k2b1/1p5p/1P1K2p1/1B4P1/7P/8 w - - 6 12
	// 8/8/8/3k3p/5Kp1/6P1/7P/8 w - - 4 4: Kg5 should bring it closer to the unprotected pawn
	// 4k3/2p5/1p1p4/pP1Pp1p1/P3P1P1/2P1P1K1/8/8 b - - 0 41: the king cannot reach a single enemy pawn here

	// TODO: use the controlled squares to find the fastest route; hard to do, but it could help a lot

	// TODO: take into account whether the pawn is passed

	// Update the king positions
	update_kings_pos();

	// King proximity
	// double, not float: the per-pawn contributions are summed in SQUARE
	// order, which differs between mirrored boards; float rounding noise
	// (1e-7) used to flip the final int truncation by +-1cp on one side.
	double proximity = 0.0;

	// Proximity bonus for enemy pawns unprotected by another pawn
	// TODO
	constexpr float unprotected_pawn_bonus = 1.0f;

	// Proximity bonus for passed pawns
	// TODO
	//constexpr float passed_pawn_bonus = 0.5f;

	// 8/8/8/4K2p/2k3p1/6P1/7P/8 w - - 2 26 : ??
	// 8/8/8/5k2/8/8/2p1p1p1/2R3K1 w - - 0 1 : ??

	// Progress percentage from which this starts to count
	const float min_advancement = 0.65f;

	if (_adv <= min_advancement)
		return 0;


	//int w_best_bonus = 0;
	//int b_best_bonus = 0;

	// 8/7p/p3K1k1/P4p2/5P1p/2p5/2P3P1/8 b - - 9 13: why did Kg7 raise the black king proximity? (FIXED)

	// 6k1/p3r2p/3R3p/2p2N2/5P2/2P5/P5PP/6K1 b - - 0 31: after Re2 and Rd7

	constexpr float innaccessibility_multiplier = 0.5f;

	constexpr float self_pawn_multiplier = 0.25f;

	SquareMap white_king_distances = get_king_squares_distance(true);
	SquareMap black_king_distances = get_king_squares_distance(false);

	int n_pawns = 0;

	for (uint8_t row = 1; row < 7; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			const uint8_t p = _array[row][col];

			// White pawn
			if (p == w_pawn) {
				//int w_distance = max(abs(row - _white_king_pos.row), abs(col - _white_king_pos.col));
				//int b_distance = max(abs(row - _black_king_pos.row), abs(col - _black_king_pos.col));

				float w_distance = white_king_distances._array[row][col];
				float b_distance = black_king_distances._array[row][col];

				// Is the square reachable?
				if (w_distance < 0) {
					w_distance = -w_distance;
				}

				if (b_distance < 0) {
					b_distance = -b_distance / innaccessibility_multiplier;
				}

				// Pawn value (rises with advancement, when unprotected, and when passed)
				// Precomputed: pow(i, 1.1) for i in [0..7]
				static constexpr float row_pow_1_1[8] = { 0.0f, 1.0f, 2.14f, 3.35f, 4.59f, 5.87f, 7.17f, 8.48f };
				float w_pawn_value = row_pow_1_1[row] * self_pawn_multiplier;
				float b_pawn_value = 1;
				//float b_pawn_value = sqrt(row);

				// Larger penalty by rank; needs revisiting. Use a true proximity rather than a distance?
				//8/8/8/4K2p/2k3p1/6P1/7P/8 w - - 2 26

				// Is the pawn unprotected by another pawn?
				bool protected_pawn = (col > 0 && _array[row - 1][col - 1] == w_pawn) || (col < 7 && _array[row - 1][col + 1] == w_pawn);
				if (!protected_pawn) {
					//w_pawn_value *= unprotected_pawn_bonus;
					b_pawn_value *= unprotected_pawn_bonus;
				}

				float w_proximity = w_distance == 0.0f ? 0.0f : w_pawn_value / w_distance; // FIXME: subtract instead of divide?
				float b_proximity = b_distance == 0.0f ? 0.0f : b_pawn_value / b_distance;

				//cout << endl << "w_pawn on " << square_name(row, col) << ": " << endl
				//	<< "WHITE: value: " << w_pawn_value << " / distance: " << w_distance << ": proximity = " << w_proximity << endl
				//	<< "BLACK: value: " << b_pawn_value << " / distance: " << b_distance << ": proximity = " << b_proximity << endl;

				//8/8/8/6Kp/3k2p1/6P1/7P/8 b - - 5 27
				
				proximity += w_proximity;
				proximity -= b_proximity;

				//8/8/8/7p/3k1Kp1/6P1/7P/8 w - - 4 27

				// 8/3k4/3p1K2/p2P1p2/P2P1P2/8/8/8 b - - 27 14: the white king is the closer one

				n_pawns++;
			}

			// Black pawn
			else if (p == b_pawn) {
				//int w_distance = max(abs(row - _white_king_pos.row), abs(col - _white_king_pos.col));
				//int b_distance = max(abs(row - _black_king_pos.row), abs(col - _black_king_pos.col));

				float w_distance = white_king_distances._array[row][col];
				float b_distance = black_king_distances._array[row][col];

				// Is the square reachable?
				if (w_distance < 0) {
					w_distance = -w_distance / innaccessibility_multiplier;
				}

				if (b_distance < 0) {
					b_distance = -b_distance;
				}

				// Pawn value (rises with advancement, when unprotected, and when passed)
				//float w_pawn_value = sqrt(7 - row);
				static constexpr float row_pow_1_1_r[8] = { 8.48f, 7.17f, 5.87f, 4.59f, 3.35f, 2.14f, 1.0f, 0.0f };
				float b_pawn_value = row_pow_1_1_r[row] * self_pawn_multiplier;
				float w_pawn_value = 1;

				// Is the pawn unprotected by another pawn?
				bool protected_pawn = (col > 0 && _array[row + 1][col - 1] == b_pawn) || (col < 7 && _array[row + 1][col + 1] == b_pawn);
				if (!protected_pawn) {
					w_pawn_value *= unprotected_pawn_bonus;
					//b_pawn_value *= unprotected_pawn_bonus;
				}

				float w_proximity = w_distance == 0.0f ? 0.0f : w_pawn_value / w_distance; // FIXME: subtract instead of divide?
				float b_proximity = b_distance == 0.0f ? 0.0f : b_pawn_value / b_distance;

				//cout << endl << "b_pawn on " << square_name(row, col) << ": " << endl
				//	<< "WHITE: value: " << w_pawn_value << " / distance: " << w_distance << ": proximity = " << w_proximity << endl
				//	<< "BLACK: value: " << b_pawn_value << " / distance: " << b_distance << ": proximity = " << b_proximity << endl;

				proximity += w_proximity;
				proximity -= b_proximity;

				n_pawns++;
			}

		}
	}

	// Delete the maps
	white_king_distances.~SquareMap();
	black_king_distances.~SquareMap();

	const int multiplier = 300;
	const double average_proximity = n_pawns == 0 ? 0.0f : proximity / sqrt((n_pawns / 2.0) * sqrt(n_pawns / 2.0));
	
	return multiplier * average_proximity * (_adv - min_advancement) / (1.0f - min_advancement);
}


// Computes rook activity and mobility
int Board::get_king_escape_squares(bool color) {

	// Square control by the enemy pieces
	SquareMap control_map = color ? get_black_controls_map() : get_white_controls_map();

	// King position
	update_kings_pos();

	Pos king_pos = color ? _white_king_pos : _black_king_pos;

	// Number of retreat squares
	int escape_squares = 0;

	// For each square around the king
	for (int i = -1; i < 2; i++) {
		for (int j = -1; j < 2; j++) {

			// Square coordinates
			uint8_t new_i = king_pos.row + i;
			uint8_t new_j = king_pos.col + j;

			// If the square is off the board
			if (new_i < 0 || new_i > 7 || new_j < 0 || new_j > 7)
				continue;

			// If a friendly piece stands on the square
			uint8_t p = _array[new_i][new_j];
			if (p != 0 && (color ? p <= w_king : p <= b_king))
				continue;


			// If the square is controlled by an enemy piece
			if (control_map._array[new_i][new_j] != 0)
				continue;

			escape_squares++;
		}
	}

	return escape_squares;
}

// Returns a value for the pieces attacking the enemy king
int Board::get_king_attackers(bool color) {
	// Sliding pieces: just walk the rank/file/diagonal. If a pawn blocks, is it a pawn near the king? Otherwise, does it control squares around the king?

	// FIXME: should only the piece count matter?
	// Should each piece type carry its own value?
	// Should this be weighted by the distance to the king?

	// TODO: prendre en compte distance 2??

	//8/8/8/2r2pp1/1k5p/2b4P/4K3/1Q6 b - - 81 133
	//rnbr3k/ppp1qppB/4p2p/1P2P3/2Pn4/P4N2/2Q2PPP/RN2K2R w KQ - 3 15
	//6rk/1p3p1p/2nN1q2/2Q2p2/3p4/PP5P/5PP1/2R3K1 b - - 1 28: the queen attacks when it is placed on e6??
	//6rk/1p3p1p/2nNq3/2Q2p2/3p4/PP5P/5PP1/2R3K1 w - - 2 29 : bug?
	//r1bqk2r/pppp1ppp/2n5/2b1p3/2BPP1n1/5N2/PPP2P1P/RNBQ1RK1 b kq - 0 6 ??
	//r1br2k1/pp2Rp2/6nB/7Q/3p4/8/5PP1/6K1 b - - 0 5: 300 here?
	//6R1/5p2/5kp1/2q5/pp4B1/2n1R3/5PKP/8 b - - 5 45: 700 here?
	//1r6/7p/p1P1p3/4kp2/1P1Rp3/4KPP1/8/8 b - - 0 49 ...
	//rnb4r/ppppbk1p/5n2/6Q1/8/2N1p3/PPP3PP/5RK1 w - - 4 16: Nd5 brings a major attacker in
	//1rbq1r2/2p2pk1/p2p1nn1/4p1N1/p3P2p/2PPP3/RPB3PP/3QBRK1 b - - 1 2: h3 lowers Black's attack? because it denies the bishop the h3 square?
	// 1rbq3r/b4pk1/p1p3n1/4p1PQ/P3P3/1BP3P1/3N1P2/R4K1R w - - 1 8

	// rn1q1rkn/pb2bpp1/1ppp4/5P2/3P4/2N2B2/PPP3PP/R1BQR1K1 b - - 4 14 vs rn1q1rkn/pb2bpp1/1ppp4/5P2/3P4/2N2B1R/PPP3PP/R1BQ2K1 b - - 4 14

	// 2r5/3r4/p1p1pk2/PpRnR3/3P2pp/4P3/7P/1B5K w - - 0 38

	// Value of a piece attacking the enemy king
	constexpr int attacking_value[7] = { 0, 100, 110, 114, 117, 119, 120 };

	// Attack value of a piece hitting the outer ring around the king
	constexpr int semi_attack_value = 40;

	// Attack factor per piece (pawn, knight, bishop, rook, queen, king)
	constexpr float piece_attack_factor[6] = { 0.60f, 1.00f, 0.95f, 1.10f, 1.30f, 0.85f };

	// Semi-attack factor per piece (pawn, knight, bishop, rook, queen, king)
	constexpr float piece_semi_attack_factor[6] = { 0.75f, 1.00f, 0.95f, 1.10f, 1.01f, 0.70f };

	// Attack factor based on the distance to the king


	// Update the king positions
	update_kings_pos();

	// King position
	Pos king_pos = color ? _black_king_pos : _white_king_pos;
	Pos opponent_king_pos = color ? _white_king_pos : _black_king_pos;

	// Number of controls around the king
	int king_attackers = 0;

	//1k1rr3/1pp1q3/pnn1b3/4p3/3pP1p1/PP1P3p/1BPNN2K/R3QR1B b - - 1 46

	// Look at every friendly piece on the board
	uint64_t occ = _occupancies[color ? 0 : 1];
	while (occ) {
		const int sq = pop_lsb(occ);
		const uint8_t row = sq >> 3;
		const uint8_t col = sq & 7;
		const uint8_t p = _array[row][col];

			uint8_t attacks = 0;
			uint8_t semi_attacks = 0;

			// Pawn
			if (p == (color ? w_pawn : b_pawn)) {

				// Squares controlled by the pawn
				uint8_t di = abs(row + (color ? 1 : -1) - king_pos.row);
				uint8_t dj1 = abs(col - 1 - king_pos.col);
				uint8_t dj2 = abs(col + 1 - king_pos.col);

				uint8_t p2a = _array[row + (color ? 1 : -1)][col - 1];

				// If the pawn controls a square around the king
				if (col > 0 && di <= 2 && dj1 <= 2 && p2a != (color ? w_pawn : b_pawn)) {
					if (di <= 1 && dj1 <= 1) {
						attacks++;
					}
					else {
						semi_attacks++;
					}
				}

				uint8_t p2b = _array[row + (color ? 1 : -1)][col + 1];

				if (col < 7 && di <= 2 && dj2 <= 2 && p2b != (color ? w_pawn : b_pawn)) {
					if (di <= 1 && dj2 <= 1) {
						attacks++;
					}
					else {
						semi_attacks++;
					}
				}
			}

			// Knight
			if (p == (color ? w_knight : b_knight)) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_i = row + knight_directions[m][0];
					int new_j = col + knight_directions[m][1];

					if (!is_in(new_i, 0, 7) || !is_in(new_j, 0, 7))
						continue;

					uint8_t p2 = _array[new_i][new_j];

					// The square cannot be attacked
					if (p2 == (color ? w_pawn : b_pawn))
						continue;

					uint8_t di = abs(new_i - king_pos.row);
					uint8_t dj = abs(new_j - king_pos.col);

					// If the knight controls a square around the king
					if (di <= 2 && dj <= 2) {
						if (di <= 1 && dj <= 1) {
							attacks++;
						}
						else {
							semi_attacks++;
						}
					}
				}
			}

			// Straight-line sliders
			if ((p == (color ? w_rook : b_rook)) || (p == (color ? w_queen : b_queen))) {

				for (uint8_t m = 0; m < 4; m++) {

					// Is the piece obstructed by another piece in this direction?
					bool blocked = false;

					int mi = rect_directions[m][0];
					int mj = rect_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint8_t p2 = _array[new_i][new_j];

						uint8_t di = abs(new_i - king_pos.row);
						uint8_t dj = abs(new_j - king_pos.col);

						// The square cannot be attacked
						if (p2 == (color ? w_pawn : b_pawn))
							break;

						// If the piece controls a square around the king
						if (di <= 2 && dj <= 2) {
							if (di <= 1 && dj <= 1 && !blocked) {
								attacks++;
							}
							else {
								semi_attacks++;
							}
						}

						// If a piece blocks the square
						if (p2 != none) {
							blocked = true;
						}

						// If a pawn blocks the square
						if (p2 == w_pawn || p2 == b_pawn)
							break;

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// Diagonal sliders
			if ((p == (color ? w_bishop : b_bishop)) || (p == (color ? w_queen : b_queen))) {

				for (uint8_t m = 0; m < 4; m++) {

					// Is the piece obstructed by another piece in this direction?
					bool blocked = false;

					int mi = diag_directions[m][0];
					int mj = diag_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint8_t p2 = _array[new_i][new_j];

						uint8_t di = abs(new_i - king_pos.row);
						uint8_t dj = abs(new_j - king_pos.col);

						// The square cannot be attacked
						if (p2 == (color ? w_pawn : b_pawn))
							break;

						// If the piece controls a square around the king
						if (di <= 2 && dj <= 2) {
							if (di <= 1 && dj <= 1 && !blocked) {
								attacks++;
							}
							else {
								semi_attacks++;
							}
						}

						// If a piece blocks the square
						if (p2 != none) {
							blocked = true;
						}

						// If a pawn blocks the square
						if (p2 == w_pawn || p2 == b_pawn)
							break;

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// King
			if (p == (color ? w_king : b_king)) {
				for (int i = -1; i < 2; i++) {
					for (int j = -1; j < 2; j++) {
						int new_i = i + opponent_king_pos.row;
						int new_j = j + opponent_king_pos.col;

						if (!is_in(new_i, 0, 7) || !is_in(new_j, 0, 7))
							continue;

						uint8_t p2 = _array[new_i][new_j];

						// The square cannot be attacked
						if (p2 == (color ? w_pawn : b_pawn))
							break;

						uint8_t di = abs(new_i - king_pos.row);
						uint8_t dj = abs(new_j - king_pos.col);

						// If the king controls a square around the king
						if (di <= 2 && dj <= 2) {
							if (di <= 1 && dj <= 1) {
								attacks++;
							}
							else {
								semi_attacks++;
							}
						}
					}
				}
			}

			if (attacks > 6) {
				cout << "BUG: too many attacks from a single piece... check get_king_attackers()" << endl;
			}
			else {
				if (attacks > 0) {
					king_attackers += attacking_value[attacks] * piece_attack_factor[(p - 1) % 6];
					//cout << "color: " << color << ", piece: " << piece_name(p) << "(" << square_name(row, col) << "), attacks : " << (int)attacks << ", value : " << attacking_value[attacks] << ", piece factor : " << piece_attack_factor[(p - 1) % 6] << ", total : " << attacking_value[attacks] * piece_attack_factor[(p - 1) % 6] << endl;
				}
				else if (semi_attacks > 0) {
					// Precomputed: pow(i, 0.3) for i in [0..8]. A slider can cover
					// up to 16 outer-ring squares -> saturate at the table end.
					static constexpr float semi_pow_0_3[9] = { 0.0f, 1.0f, 1.23f, 1.39f, 1.52f, 1.62f, 1.71f, 1.79f, 1.87f };
					king_attackers += semi_attack_value * piece_semi_attack_factor[(p - 1) % 6] * semi_pow_0_3[min<uint8_t>(semi_attacks, 8)];
					//cout << "color: " << color << ", piece: " << piece_name(p) << "(" << square_name(row, col) << "), semi-attacks : " << (int)semi_attacks << ", value : " << semi_attack_value * piece_semi_attack_factor[(p - 1) % 6] * pow(semi_attacks, 0.3) << endl;
				}
			}
	}

	//cout << "king_attackers: " << king_attackers << endl;

	//1r1qr3/5p1k/3p1Ppb/p2N3p/2pPP2P/2Pn3B/P3QR2/5RK1 w - - 1 22

	return king_attackers;
}

int Board::get_king_defenders(bool color) {
	// Sliding pieces: just walk the rank/file/diagonal. If a pawn blocks, is it a pawn near the king? Otherwise, does it control squares around the king?

	// r1b2b1r/ppN3pp/1k6/2p5/3Q1B2/8/PP3PPP/n1R3K1 w - - 0 20: Black has few defenders here

	// 5r2/1pk3p1/2pr3p/p1n2P2/2PN4/2P1pBP1/6KP/1R1R4 b - - 0 29 : y'a r

	// Value of a piece defending the king
	constexpr int defending_value[9] = { 0, 100, 110, 115, 119, 122, 125, 128, 130 };

	// Defence value of a piece covering the outer ring around the king
	constexpr int semi_defense_value = 30;

	// Defence factor per piece (pawn, knight, bishop, rook, queen, king)
	constexpr float piece_defense_factor[6] = { 0.5f, 1.5f, 1.35f, 1.2f, 0.75f, 1.0f };

	// The queen is a poor defender, being so easily exposed

	// Update the king positions
	update_kings_pos();

	// King position
	Pos king_pos = color ? _white_king_pos : _black_king_pos;

	// Number of controls around the king
	int king_defenders = 0;

	// Look at every friendly piece on the board
	uint64_t occ = _occupancies[color ? 0 : 1];
	while (occ) {
		const int sq = pop_lsb(occ);
		const uint8_t row = sq >> 3;
		const uint8_t col = sq & 7;
		const uint8_t p = _array[row][col];

			uint8_t defenses = 0;
			uint8_t semi_defenses = 0;

			// Pawn: TODO, revisit
			if (p == (color ? w_pawn : b_pawn)) {

				// Squares controlled by the pawn
				uint8_t di = abs(row + (color ? 1 : -1) - king_pos.row);
				uint8_t dj1 = abs(col - 1 - king_pos.col);
				uint8_t dj2 = abs(col + 1 - king_pos.col);
				bool front_square = color ? row >= king_pos.row : row <= king_pos.row;

				// If the pawn controls a square around the king
				if (col > 0 && di <= 2 && dj1 <= 2) {
					if (front_square && di <= 1 && dj1 <= 1) {
						defenses++;
					}
					else {
						semi_defenses++;
					}
				}

				if (col < 7 && di <= 2 && dj2 <= 2) {
					if (di <= 1 && dj2 <= 1) {
						defenses++;
					}
					else {
						semi_defenses++;
					}
				}
			}

			// Knight
			if (p == (color ? w_knight : b_knight)) {
				for (uint8_t m = 0; m < 8; m++) {
					int new_i = row + knight_directions[m][0];
					int new_j = col + knight_directions[m][1];

					if (!is_in(new_i, 0, 7) || !is_in(new_j, 0, 7))
						continue;

					uint8_t di = abs(new_i - king_pos.row);
					uint8_t dj = abs(new_j - king_pos.col);
					bool front_square = color ? new_i > king_pos.row : new_i < king_pos.row;

					// If the knight controls a square around the king
					if (di <= 2 && dj <= 2) {
						if (front_square && di <= 1 && dj <= 1) {
							defenses++;
						}
						else {
							semi_defenses++;
						}
					}
				}
			}

			// Straight-line sliders
			if ((p == (color ? w_rook : b_rook)) || (p == (color ? w_queen : b_queen))) {

				for (uint8_t m = 0; m < 4; m++) {
					int mi = rect_directions[m][0];
					int mj = rect_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint8_t p2 = _array[new_i][new_j];

						uint8_t di = abs(new_i - king_pos.row);
						uint8_t dj = abs(new_j - king_pos.col);
						bool front_square = color ? new_i > king_pos.row : new_i < king_pos.row;

						// If the piece controls a square around the king
						if (di <= 2 && dj <= 2) {
							if (front_square && di <= 1 && dj <= 1) {
								defenses++;
							}
							else {
								semi_defenses++;
							}
						}

						// If a piece blocks the square
						if (p2 != none)
							break;

						// Si 

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// Diagonal sliders
			if ((p == (color ? w_bishop : b_bishop)) || (p == (color ? w_queen : b_queen))) {

				for (uint8_t m = 0; m < 4; m++) {
					int mi = diag_directions[m][0];
					int mj = diag_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint8_t p2 = _array[new_i][new_j];

						uint8_t di = abs(new_i - king_pos.row);
						uint8_t dj = abs(new_j - king_pos.col);
						bool front_square = color ? new_i > king_pos.row : new_i < king_pos.row;

						// If the piece controls a square around the king
						if (di <= 2 && dj <= 2) {
							if (front_square && di <= 1 && dj <= 1) {
								defenses++;
							}
							else {
								semi_defenses++;
							}
						}

						// If a pawn blocks the square
						if (p2 == w_pawn || p2 == b_pawn)
							break;

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// King
			if (p == (color ? w_king : b_king)) {
				for (int i = -1; i < 2; i++) {
					for (int j = -1; j < 2; j++) {

						int new_i = i + king_pos.row;
						int new_j = j + king_pos.col;

						if (i == 0 && j == 0)
							continue;

						if (!is_in(new_i, 0, 7) || !is_in(new_j, 0, 7))
							continue;

						uint8_t di = abs(new_i - king_pos.row);
						uint8_t dj = abs(new_j - king_pos.col);
						bool front_square = color ? new_i > king_pos.row : new_i < king_pos.row;

						// If the king controls a square around the king
						if (di <= 2 && dj <= 2) {
							if (front_square && di <= 1 && dj <= 1) {
								defenses++;
							}
							else {
								semi_defenses++;
							}
						}
					}
				}
			}

			if (defenses > 8) {
				cout << "BUG: too many defenses from a single piece... check get_king_defenders()" << endl;
			}
			else {
				if (defenses > 0) {
					king_defenders += defending_value[defenses] * piece_defense_factor[(p - 1) % 6];
					//cout << "color: " << color << ", piece: " << piece_name(p) << "(" << square_name(row, col) << "), defenses : " << (int)defenses << ", value : " << defending_value[defenses] << ", piece factor : " << piece_defense_factor[(p - 1) % 6] << ", total : " << defending_value[defenses] * piece_defense_factor[(p - 1) % 6] << endl;
				}
				else if (semi_defenses > 0) { // TODO: improve by counting the semi-defences?
					king_defenders += semi_defense_value * piece_defense_factor[(p - 1) % 6];
					//cout << "color: " << color << ", piece: " << piece_name(p) << "(" << square_name(row, col) << "), semi-defenses : " << (int)semi_defenses << ", value : " << semi_defense_value * piece_defense_factor[(p - 1) % 6] << endl;
				}
			}
	}

	//cout << "total defenders: " << king_defenders << endl;

	return king_defenders;
}

// Returns the pawn storm bonus against the enemy king on a given file
void Board::get_uncertainty(Evaluation* eval, int material_eval, int winning_eval) const {

	// TODO ***
	// Number of checks available in the position?
	// Contre jeu

	// r2r4/8/1p1q2P1/2b5/3k1pP1/pP2pP2/2Q4P/1K6 w - - 9 11: example of a very uncertain position
	// r2r4/4q3/1p4P1/2b5/5pPk/pP2pP2/8/1K6 w - - 0 18 : plus du tout d'incertitude !!
	// rnb2bnr/pppp1k1p/5q2/8/5p2/4BQ2/PPP3PP/RN3RK1 w - - 2 11 : grosse incertitude
	// r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4 : assez peu...
	// r1bq1b1r/ppp3pp/2n1k3/3np3/2B5/5Q2/PPPP1PPP/RNB1K2R w KQ - 2 8 : grosse incertitude
	// 8/8/7P/1p2Kp2/3P4/P2k2P1/P7/8 b - - 0 41 : plus aucune incertitude

	// rnb2bnr/pppp1k1p/5q2/8/5p2/4BQ2/PPP3PP/RN3RK1 w - - 2 11: how does uncertainty jump from 14% to 92% on Bxf4?
	// et avg score 0.7 -> 0.42??


	// TODO: should the search GrogrosZero has already done be taken into account?
	// For instance, when the evaluation swings quickly
	// Depends on how many pieces remain

	// TODO: should the number of available captures be taken into account?

	// FIXME: improvable. When behind both materially and non-materially, it avoids regaining activity and pulling the non-material term back to 0, because that would lower the uncertainty
	//5rk1/1B4p1/7p/3p4/5n2/4n2P/1R4PK/8 b - - 3 36: Black has activity here, hence extra uncertainty, which is wrong

	// rnb3r1/ppp2k1p/1b1p1N2/4P3/1P6/2P3P1/P3PP1P/RN1QK2R w KQ - 2 13: should be nearly 100%

	// 4r1k1/p1p2ppp/1p2bn2/8/1r6/1PN1B3/P3BPPP/R2R2K1 b - - 1 16: it only reaches 50% after c5?

	float raw_incertitude = 0.0f;

	// TODO: a proper formula for the uncertainty is still needed
	int non_material_eval = eval->_value - material_eval;

	if (non_material_eval != 0) {
		int abs_non_material_eval = abs(non_material_eval);

		// How far the non-material evaluation opposes the material one (0 = not at all, 1 = completely)
		//float opposite_material_factor = 1.0f / (1.0f + abs(_evaluation) / (abs_non_material_eval));

		// Note: normalised dot product. 1 = same direction, -1 = opposite direction
		int max_eval = max(abs(material_eval), abs_non_material_eval);
		float colinear_evals = static_cast<float>(material_eval) * non_material_eval / (max_eval * max_eval);

		// Value of the opposite material factor (0 = not at all, 1 = completely)
		float opposite_material_factor = 0.5f - colinear_evals / 2.0f;

		//// Value of the opposite material factor when the material evaluation is zero
		constexpr float thresold = 0.5f;

		// New opposite material factor
		float new_opposite_material_factor = opposite_material_factor - thresold;

		//cout << "old value: " << new_opposite_material_factor << endl;

		// Pull the value towards the bounds (-0.5, 0.5)
		//float rapprochement = 3.0f;
		//const float rapprochement = abs(material_eval) / 100.0f;
		const float rapprochement = abs(non_material_eval) / 100.0f;
		new_opposite_material_factor = new_opposite_material_factor >= 0 ? pow(new_opposite_material_factor * 2, 1 / rapprochement) / 2 : -pow(-new_opposite_material_factor * 2, 1 / rapprochement) / 2;

		new_opposite_material_factor += thresold;

		// TODO: opposed should reinforce itself, and non-opposed likewise

		// Constant giving a baseline uncertainty of 0.5 in the messiest case
		constexpr int half_uncertainty_constant = 50;

		// Normalise this non-material factor to [0, 1] through a non-linear function
		//float norm_non_material_eval = abs_non_material_eval / (static_cast<float>(half_uncertainty_constant) + abs_non_material_eval + abs(material_eval) / 2.0f);
		float norm_non_material_eval = abs_non_material_eval / (static_cast<float>(half_uncertainty_constant) + abs_non_material_eval + abs(eval->_value));
		//float norm_non_material_eval = abs_non_material_eval / (static_cast<float>(half_uncertainty_constant) + abs_non_material_eval);


		// 5rk1/1B4p1/7p/3p4/5n2/4n2P/1R4PK/8 b - - 3 36

		// Combine the two factors
		float new_incertitude = norm_non_material_eval * new_opposite_material_factor;
		//float new_incertitude = norm_non_material_eval * opposite_material_factor;

		// Bring the uncertainty back into [0, 1]
		//raw_incertitude = new_incertitude + thresold;
		raw_incertitude = new_incertitude;

		//cout << "eval: " << _evaluation << ", material eval: " << material_eval << ", non-material eval: " << non_material_eval << ", opposite material factor: " << opposite_material_factor << ", new opposite material factor: " << new_opposite_material_factor << ", norm non-material eval: " << norm_non_material_eval << ", raw incertitude: " << raw_incertitude << endl;
	}

	// Weight of the non-material uncertainty
	//float non_material_factor = 0.75f;


	// Uncertainty per piece type
	constexpr int piece_uncertitudes[6] = { 1, 5, 7, 10, 50, 0 };

	// Incertitude max (environ... si y'a plusieurs dames?)
	//constexpr int max_piece_uncertainty = 204;
	constexpr int max_piece_uncertainty_per_side = 102;

	// Uncertainty per piece
	//int total_piece_uncertitude = 0;
	int white_piece_uncertainty = 0;
	int black_piece_uncertainty = 0;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t piece = _array[row][col];
			if (piece == none) {
				continue;
			}

			//total_piece_uncertitude += piece_uncertitudes[(piece - 1) % 6];

			if (is_white(piece)) {
				white_piece_uncertainty += piece_uncertitudes[(piece - 1) % 6];
			}
			else {
				black_piece_uncertainty += piece_uncertitudes[(piece - 1) % 6];
			}
		}
	}

	// Uncertainty as a function of the piece count
	//float piece_uncertainty = total_piece_uncertitude / static_cast<float>(max_piece_uncertainty);
	float piece_uncertainty = eval->_value > 0 ? black_piece_uncertainty / static_cast<float>(max_piece_uncertainty_per_side) : white_piece_uncertainty / static_cast<float>(max_piece_uncertainty_per_side);

	// Weight of this uncertainty
	int eval_val = abs(eval->_value);
	//float piece_uncertainty_factor = 0.25f;
	//float piece_uncertainty_factor = 0.5 * raw_incertitude;
	float piece_uncertainty_factor = 0.65f / (1 + static_cast<float>(eval_val) / winning_eval);


	// Other factors to account for:
	// - number of pieces left (raises uncertainty)
	// - complexity of the position (raises uncertainty)
	// - material imbalance (raises uncertainty)
	// - symmetry of the position (lowers uncertainty)

	// Account for the game progress

	float alpha = 1 + abs(eval->_value) / static_cast<float>(winning_eval) / 2.0f;
	float new_raw_incertitude = pow(raw_incertitude, alpha);

	//float value = raw_incertitude * piece_uncertainty;
	//float value = raw_incertitude * non_material_factor + piece_uncertainty * piece_uncertainty_factor;
	float value = new_raw_incertitude * (1 - piece_uncertainty_factor) + piece_uncertainty * piece_uncertainty_factor;
	
	//float alpha = 1 + abs(_evaluation) / static_cast<float>(winning_eval) / 10.0f;
	//float relative_uncertainty = pow(value, alpha);


	eval->_uncertainty = value;
}

// Stores the WDL of the position (for White)
void Evaluation::get_WDL(int winning_eval, float beta) {
	
	// r2r4/8/1p1q2P1/2b5/3k1pP1/pP2pP2/2Q4P/1K6 w - - 9 11: down the drawing lines this should read 0, 1000, 0, not 333, 333, 333

	// Winning eval: the evaluation at which the winning and drawing chances are equal, at zero uncertainty.

	// beta controls how slowly it converges.
	// For beta = 0.25, f(2 * winning_eval) = 0.84
	// for beta = 0.5,  f(2 * winning_eval) = 0.707
	// for beta = 0.75,  f(2 * winning_eval) = 0.62

	// TEST
	//const float up_beta = 0.35f;
	//const float down_beta = 1.0f;

	// rnbqkb1r/ppp1pp1p/5p2/3p4/3P4/8/PPP2PPP/RNBQKBNR b KQkq - 0 4: close to a 100% win rate at maximum confidence

	// Winning eval = the evaluation at which the certain winning chance equals the certain drawing chance (0.5)

	bool is_eval_positive = _value > 0;
	float eval = abs(_value);

	//cout << "eval: " << eval << ", winning_eval: " << winning_eval << ", beta: " << beta << endl;

	constexpr float beta_up = 2.5f;
	constexpr float beta_down = 1.5f;

	// TEST
	const float white_winning_eval = _winnable_white == 0.0f ? FLT_MAX : winning_eval / _winnable_white;
	const float black_winning_eval = _winnable_black == 0.0f ? FLT_MAX : winning_eval / _winnable_black;


	const float base_win_chance_factor = is_eval_positive ? eval / white_winning_eval : eval / black_winning_eval;
	//const float base_win_chance_factor = eval / winning_eval;
	const float win_chance_factor = base_win_chance_factor > 1.0f ? base_win_chance_factor * base_win_chance_factor * sqrt(base_win_chance_factor) : base_win_chance_factor * sqrt(base_win_chance_factor);
	const float base_win_chance = 1.0f - 1.0f / (1.0f + win_chance_factor);

	//const float base_win_chance = (eval / (eval + winning_eval));

	// Flatten it slightly when completely winning
	const float threshold_win_chance = base_win_chance - 0.5f;
	//const float updated_threshold = threshold_win_chance > 0 ? pow(abs(threshold_win_chance) * 2, up_beta) / 2 : -pow(abs(threshold_win_chance) * 2, down_beta) / 2;
	const float updated_threshold = threshold_win_chance;

	const float certain_win_chance = updated_threshold + 0.5f;

	//float certain_win_chance = pow((eval / (eval + winning_eval)), beta);
	const float certain_draw_chance = 1 - certain_win_chance;

	//cout << "certain win chance: " << certain_win_chance << ", certain draw chance: " << certain_draw_chance << endl;

	const float white_win_chance = (is_eval_positive ? certain_win_chance : 0.0f);
	const float white_lose_chance = (is_eval_positive ? 0.0f : certain_win_chance);

	// Non-linear function of the uncertainty
	//constexpr float alpha = 1 + eval / winning_eval / 25.0f;
	// constexpr alpha=1.0 -> pow(x,1) == x, skip the pow() call entirely
	constexpr float alpha = 1.0f;
	const float relative_uncertainty = _uncertainty; // pow(_uncertainty, alpha) when alpha==1

	//cout << "relative uncertainty: " << relative_uncertainty << endl;

	//const float win_chance = white_win_chance * (1 - relative_uncertainty) + relative_uncertainty / 3.0f;
	//const float lose_chance = white_lose_chance * (1 - relative_uncertainty) + relative_uncertainty / 3.0f;
	//const float draw_chance = certain_draw_chance * (1 - relative_uncertainty) + relative_uncertainty / 3.0f;

	const float win_chance = white_win_chance * (1 - relative_uncertainty) + relative_uncertainty * _winnable_white / 3.0f;
	const float lose_chance = white_lose_chance * (1 - relative_uncertainty) + relative_uncertainty * _winnable_black / 3.0f;
	const float draw_chance = 1 - win_chance - lose_chance;

	//7R/8/1p2kp2/pKr5/P7/1P3P2/5P2/8 w - - 0 49

	// Evaluation of the real winning chances
	//const float 
	// _white = get_winnable(true);
	//const float winnable_black = get_winnable(false);

	// FIXME: this may need to be applied before the uncertainty
	//const float total_win_chance = win_chance * _winnable_white;
	//const float total_lose_chance = lose_chance * _winnable_black;
	//const float total_draw_chance = 1 - total_win_chance - total_lose_chance;

	//cout << "win chance: " << win_chance << ", draw chance: " << draw_chance << ", lose chance: " << lose_chance << endl;

	//_uncertainty = uncertainty;
	_wdl = WDL(win_chance, draw_chance, lose_chance);
	//_wdl = WDL(total_win_chance, total_draw_chance, total_lose_chance);
}

// Returns the expected score of the position, in points, from the WDL probabilities
void Evaluation::get_average_score(float draw_score) {
	// TODO: pick the move from this score rather than from the evaluation?

	_avg_score = _wdl.win_chance + draw_score * _wdl.draw_chance;
}

// Returns the evaluation renormalised against the average score
string get_renormalized_evaluation(float avg_score, float winning_eval, float winning_score) {

	// avg_score = 0.5 -> eval = 0
	// avg_score = 1.0 -> eval = +inf
	// avg_score = 0.0 -> eval = -inf
	// avg_score = 0.67 -> eval = winning_eval
	// avg_score = 0.33 -> eval = -winning_eval

	if (avg_score == 1.0f) {
		return "+Inf";
	}
	if (avg_score == 0.0f) {
		return "-Inf";
	}

	// Function symmetric about 0.5
	const float winning_score_diff = 1.0f - winning_score;
	const float score_diff = min(avg_score, 1.0f - avg_score);

	float eval = winning_eval * (avg_score - 0.5f) / (winning_score - 0.5f) * pow(winning_score_diff / score_diff, 0.5f);

	stringstream stream;
	stream << fixed << setprecision(1) << eval;
	return eval > 0 ? "+" + stream.str() : stream.str();
}

// Returns the score of a WDL triple, to a precision of 0.01
string score_string(float avg_score) {
	//float score = get_average_score(wdl, draw_score);
	//stringstream stream;
	//stream << fixed << setprecision(3) << avg_score;
	//return stream.str();

	char buffer[32];
	int len = snprintf(buffer, sizeof(buffer), "%.3f", avg_score);
	return std::string(buffer, len);
}

// Swaps the colours of the two sides, side to move and castling rights included
int Board::get_next_king_squares(SquareMap& map, Pos start_pos, int distance, bool color, Pos* out) const {

	// Newly reached squares (at most 8 neighbours)
	int n = 0;

	// Look at the 8 possible directions
	for (uint8_t m = 0; m < 8; m++) {

		// Nouvelle position
		int new_i = start_pos.row + all_directions[m][0];
		int new_j = start_pos.col + all_directions[m][1];

		// If the square is on the board
		if (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {

			// If this is a newly explored square
			if (map._array[new_i][new_j] == 0) {
				map._array[new_i][new_j] = distance + 1;
				out[n++] = Pos(new_i, new_j);
			}

			// If the square is controlled by the opponent
			else if (map._array[new_i][new_j] == -64) {
				map._array[new_i][new_j] = -distance - 1;
			}
		}
	}

	return n;
}

// Returns a map of the distances from the king to every square, as the number of moves needed given the current controls
SquareMap Board::get_king_squares_distance(bool color) {
	// TODO: tedious, but probably very strong

	//8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7: the black king can reach neither a4, d5 nor d4, short of going all the way round

	// 8/8/3k2b1/1p5p/1P1K2p1/1B4P1/7P/8 w - - 6 12
	// White king: distance 3 to h5, via e3 f4 g5
	// Cannot reach b5 as things stand

	// 8/8/6K1/1p2k2p/1P4p1/6P1/7P/8 b - - 0 19

	// 8/5b2/8/1p1k1BKp/1P4p1/6P1/7P/8 b - - 17 17: the white king is closer


	// Enemy control map
	SquareMap control_map = color ? get_black_controls_map() : get_white_controls_map();

	// Map initialisation

	// -64 = unreachable square (friendly piece, or controlled by the opponent)
	// k = square at distance k from the king
	// -k = square controlled by the opponent, or a friendly piece, at distance k
	SquareMap distance_map;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {

			uint8_t piece = _array[row][col];

			// The king is assumed unable to pass through:
			// a square controlled by the opponent
			// a friendly piece
			// an enemy non-pawn piece, on the assumption that it can step back and keep control of the square (TEST)
			if (control_map._array[row][col] || !(piece == none || (is_pawn(piece) && is_white(piece) != color))) {
				distance_map._array[row][col] = -64;
			}
		}
	}

	// Supprime la map
	control_map.~SquareMap();

	// King position
	update_kings_pos();
	Pos king_pos = color ? _white_king_pos : _black_king_pos;


// Build the distance map iteratively (stack BFS: at most 64 squares ever
// enqueued, each exactly once - zero allocation, was vector-per-square).
	Pos frontier[64] = { king_pos };
	Pos next[64];
	int frontier_n = 1;
	int distance = 0;
	while (frontier_n > 0) {
		int next_n = 0;
		for (int k = 0; k < frontier_n; k++) {
			next_n += get_next_king_squares(distance_map, frontier[k], distance, color, next + next_n);
		}
		for (int k = 0; k < next_n; k++) frontier[k] = next[k];
		frontier_n = next_n;
		distance++;
	}

	// Reset the -64 entries to 0
	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			if (distance_map._array[row][col] == -64) {
				distance_map._array[row][col] = 0;
			}
		}
	}

	// King square
	distance_map._array[king_pos.row][king_pos.col] = 0;

	return distance_map;
}


// Returns the weakness along the king's ranks
int Board::get_king_row_weakness(bool color) {
	update_kings_pos();

	return 0;
}

// Returns the king centralisation value in the endgame
int Board::get_king_centralization(bool color) {

	update_kings_pos();

	// King position
	Pos king_pos = color ? _white_king_pos : _black_king_pos;

	// Distance from the king to the centre
	int row_distance = min(abs(king_pos.row - 3), abs(king_pos.row - 4));
	int col_distance = min(abs(king_pos.col - 3), abs(king_pos.col - 4));

	int distance = row_distance * row_distance + col_distance * col_distance;

	// The value grows with the number of pawns left
	int pawns_count = 0;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t piece = _array[row][col];
			if (is_pawn(piece)) {
				pawns_count++;
			}
		}
	}

	float pawns_factor = (2.0f + pawns_count) / 10.0f;

	// Progress at which this starts to matter
	constexpr float begin_adv = 0.65f;

	// Penalty based on the distance
	return (10 - distance) * pawns_factor * max(0.0f, (_adv - begin_adv) / (1 - begin_adv));
}

// Returns the value of the undefended pieces
int Board::get_unprotected_pieces(bool color) const {
	// TODO
	return 0;
}

// Tells whether the king is inside the square of the pawn
bool Board::in_king_square(Pos pos, bool king_color) {

	// FIXME: this does not cover everything; the king may fail to reach the pawn for other reasons, controlled squares among them

	// King position
	update_kings_pos();
	Pos king_pos = king_color ? _white_king_pos : _black_king_pos;

	// TODO: account for the +1 at the boundaries when the king is to move

	// color: colour of the king, which differs from the pawn's
	bool pawn_color = !king_color;

	// Distance to promotion
	int distance = pawn_color ? 7 - pos.row + !_player : pos.row + _player;

	//cout << "distance: " << distance << endl;

	// Promotion square
	Pos promotion_pos = Pos(pawn_color ? 7 : 0, pos.col);

	//cout << "king pos: " << king_pos.row << " " << king_pos.col << endl;
	//cout << "promotion pos: " << promotion_pos.row << " " << promotion_pos.col << endl;

	// King distance to the promotion square
	int row_distance = abs(king_pos.row - promotion_pos.row);
	int col_distance = abs(king_pos.col - promotion_pos.col);

	//cout << "row distance: " << row_distance << ", col distance: " << col_distance << endl;

	// If the king is inside the square
	return row_distance <= distance && col_distance <= distance;
}

// Returns whether this is a pawn endgame
int Board::get_long_term_king_weakness(bool player, int current_weakness, int kingside_weakness, int queenside_weakness) {

	// Update the king positions
	update_kings_pos();

	// King position
	Pos king_pos = player ? _white_king_pos : _black_king_pos;

	// Castling rights
	bool can_kingside_castle = player ? _castling_rights.k_w : _castling_rights.k_b;
	bool can_queenside_castle = player ? _castling_rights.q_w : _castling_rights.q_b;

	uint8_t king_row = player ? 0 : 7;
	uint8_t above_row = player ? 1 : 6;
	uint8_t blocking_bishop = player ? w_bishop : b_bishop;

	// TODO: dedicated functions for the distances to castling

	// Distance added when the opponent controls the square
	constexpr int control_distance_add = 3;

	// Distance to kingside castling
	uint8_t kingside_castle_distance = 0;

	if (can_kingside_castle) {
		kingside_castle_distance = (_array[king_row][5] != none) + (_array[king_row][6] != none)
			+ (_array[king_row][5] == blocking_bishop && _array[above_row][4] != none && _array[above_row][6] != none)
			+ is_controlled(king_row, 5, player) * control_distance_add + is_controlled(king_row, 6, player) * control_distance_add;
	}

	// r1bqk2r/pppp1ppp/1bn2n2/8/4P3/1N3P2/PPP3PP/RNBQKB1R w KQkq - 3 7: kingside castling is controlled here -> much larger distance

	// Distance to queenside castling
	uint8_t queenside_castle_distance = 0;

	if (can_queenside_castle) {
		queenside_castle_distance = (_array[king_row][3] != none) + (_array[king_row][2] != none) + (_array[king_row][1] != none)
			+ (_array[king_row][2] == blocking_bishop && _array[above_row][1] != none && _array[above_row][3] != none)
			+ is_controlled(king_row, 2, player) * control_distance_add + is_controlled(king_row, 3, player) * control_distance_add;
	}

	//cout << "kingside: " << (int)kingside_castle_distance << ", queenside: " << (int)queenside_castle_distance << endl;
	//cout << "castling distance factor, kingside: " << max(0.0, 1.0 - (1.0 + kingside_castle_distance) / 7.0) << ", queenside: " << max(0.0, 1.0 - (1.0 + queenside_castle_distance) / 7.0) << endl;

	// TODO: could be improved when pieces other than the original bishop block the castle

	// TOTAL: main / C + K / (Dk + c) + Q / (Dq + c)

	int total_weakness = current_weakness;

	// Potential gain from castling
	const int kingside_castling_bonus = max(0, current_weakness - kingside_weakness);
	const int queenside_castling_bonus = max(0, current_weakness - queenside_weakness);

	//3qkb1r/p1pbnppp/2p5/4N3/Q7/2N5/Pr3PPP/3RR1K1 b k - 1 14: distance to castling is at least 3

	// FIXME: fairly arbitrary. 0.9 when castling is available, decaying linearly to 0 otherwise, assuming the distance never exceeds 8
	// Plenty of room to improve this
	double kingside_castling_factor = can_kingside_castle ? max(0.0, 1.0 - (1.5 + kingside_castle_distance) / 6.0) : 0.0;
	double queenside_castling_factor = can_queenside_castle ? max(0.0, 1.0 - (1.5 + queenside_castle_distance) / 6.0) : 0.0;

	int total_kingside_bonus = kingside_castling_bonus * kingside_castling_factor;
	int total_queenside_bonus = queenside_castling_bonus * queenside_castling_factor;

	int best_castle_bonus = max(total_kingside_bonus, total_queenside_bonus);

	total_weakness -= best_castle_bonus;

	//cout << "current weakness: " << current_weakness << ", kingside weakness: " << kingside_weakness << ", queenside weakness: " << queenside_weakness << endl;
	//cout << "kingside distance: " << (int)kingside_castle_distance << ", queenside distance: " << (int)queenside_castle_distance << endl;
	//cout << "kingside bonus: " << total_kingside_bonus << ", queenside bonus: " << total_queenside_bonus << endl;
	//cout << "kingside factor: " << kingside_castling_factor << ", queenside factor: " << queenside_castling_factor << endl;
	//cout << "best castle bonus: " << best_castle_bonus << endl;
	//cout << "total weakness: " << total_weakness << endl;

	return total_weakness;
}

// Returns the bonus value for open and semi-open files bearing on the enemy king, were it on a given file
int Board::get_open_files_on_opponent_king_at_column(bool player, int king_col) const {

	// Bonus for the open and semi-open files
	constexpr int open_file_bonus = 25;
	constexpr int semi_open_file_bonus = 15;

	// Factor based on proximity to the enemy king's file
	// If the king is on the file, the bonus is maximal
	constexpr float king_file_bonus = 1.0f;

	// On an adjacent file, the bonus is reduced
	constexpr float king_adjacent_file_bonus = 0.65f;

	// Extra bonus for the pieces standing on it (rooks, queen)
	constexpr int rook_open_bonus = 35;
	constexpr int queen_open_bonus = 35;

	constexpr int rook_semi_open_bonus = 25;
	constexpr int queen_semi_open_bonus = 20;

	constexpr int opponent_guarding_malus = 20;

	// Bonus for the player
	int total_bonus = 0;

	// Friendly pawn
	const int player_pawn = player ? w_pawn : b_pawn;

	// Enemy pawn
	const int opponent_pawn = player ? b_pawn : w_pawn;

	//r1r1b1k1/pp3p1p/1q2p1nQ/3pP1N1/3n2P1/2N5/PP2BPK1/1R5R w - - 2 24

	// int loop: the former uint8_t col underflowed at king_col=0 (col-1=255,
	// "col < 0" is always false for unsigned) and the loop condition then
	// rejected everything: a king on the a-file had NO open-file scan at all.
	for (int col = king_col - 1; col <= king_col + 1; col++) {

		// If the file is off the board
		if (col < 0 || col > 7)
			continue;

		// Nature of the file
		bool semi_open = true;
		bool open = true;

		for (uint8_t row = 0; row < 8; row++) {
			uint8_t p = _array[row][col];

			if (p == player_pawn) {
				semi_open = false;
				open = false;
				break;
			}
			else if (p == opponent_pawn) {
				open = false;
			}
		}

		// Bonus
		int bonus = (open ? open_file_bonus : (semi_open ? semi_open_file_bonus : 0));

		// Bonus for the pieces standing on the file
		if (open || semi_open) {
			for (uint8_t row = 0; row < 8; row++) {
				uint8_t p = _array[row][col];

				if (p == (player ? w_rook : b_rook))
					bonus += open ? rook_open_bonus : rook_semi_open_bonus;
				else if (p == (player ? w_queen : b_queen))
					bonus += open ? queen_open_bonus : queen_semi_open_bonus;
				else if (is_rectilinear(p))
					bonus -= opponent_guarding_malus;
			}

			//cout << "col: " << (int)col << ", open: " << open << ", semi_open: " << semi_open << ", bonus: " << bonus << endl;
		}

		// 4k1r1/2pp4/1p2pq2/6r1/p2P3p/2PB1b1P/PPQ3P1/R4RK1 w - - 2 24
		// 4k2r/2pp3B/1p2pq2/6r1/p2P4/2P2b1P/PPQ2Rp1/4R1K1 b - - 3 27
		// r2q1rk1/pb2bppp/1pn1p3/2p4n/4P3/2NBBN2/PPP1QPPP/2KR3R b - - 11 11

		// Bonus based on proximity to the king
		bonus *= (col == king_col ? king_file_bonus : king_adjacent_file_bonus);

		total_bonus += bonus;
	}

	// Depending on how far the game has progressed
	constexpr float advancement_factor = 0.0f;

	return eval_from_progress(max(0, total_bonus), _adv, advancement_factor);
}

// Returns the king placement bonus, were it on a given file
int Board::get_king_placement_weakness_at_column(bool player, Pos king_pos) const {

	// Tuning
	constexpr float edge_adv = 0.7f;
	constexpr float mult_endgame = 0.25f;

	// Additive version, suited to the endgame
	constexpr int edge_defense = 50;

	const int col_dist = min(king_pos.col, 7 - king_pos.col);
	const int row_dist = min(king_pos.row, 7 - king_pos.row);

	const int center_col_dist = min(abs(king_pos.col - 3), abs(king_pos.col - 4));
	const int center_row_dist = min(abs(king_pos.row - 3), abs(king_pos.row - 4));

	const double base_factor = edge_defense * (edge_adv - _adv);

	const double row_malus = 0.5f;
	const double row_factor = row_malus * (player ? (king_pos.row * king_pos.row) : ((7 - king_pos.row) * (7 - king_pos.row)));

	const double placement_weakness = base_factor * ((_adv < edge_adv) ? (max(0.0, col_dist - 1.35) + row_factor / 2.0f) : (mult_endgame / (edge_adv - 1.0f) * (center_col_dist * center_col_dist + center_row_dist * center_row_dist)));

	//cout << "king pos: " << (int)king_pos.row << " " << (int)king_pos.col << ", row dist: " << row_dist << ", col dist: " << col_dist << ", row factor: " << row_factor << ", placement weakness: " << placement_weakness << endl;

	return placement_weakness;
}


// Returns the king placement bonus
int Board::get_king_placement_weakness(bool player) {

	// King position
	update_kings_pos();
	Pos king_pos = player ? _white_king_pos : _black_king_pos;

	// Castling rights
	bool can_kingside_castle = player ? _castling_rights.k_w : _castling_rights.k_b;
	bool can_queenside_castle = player ? _castling_rights.q_w : _castling_rights.q_b;
	
	int current_weakness = get_king_placement_weakness_at_column(player, king_pos);
	int kingside_weakness = can_kingside_castle ? get_king_placement_weakness_at_column(player, { player ? 0 : 7, 6 }) : 0;
	int queenside_weakness = can_queenside_castle ? get_king_placement_weakness_at_column(player, { player ? 0 : 7, 2 }) : 0;

	return get_long_term_king_weakness(player, current_weakness, kingside_weakness, queenside_weakness);
	//return current_weakness;
}
int Board::get_queen_safety(bool color) const {

	// Factors to evaluate:
	// - tempi that can be gained against the queen
	// - isolated queen
	// - queen surrounded by enemy pieces
	// - trapped queen, or one with few escape squares

	// Total value
	int queen_safety_value = 0;

	// Plural, since there can theoretically be several
	int queens_safety = 0;

	// Queen positions
	// Theoretical maximum of 9 queens per side
	Pos queens_pos[9]{};
	uint8_t queens_count = 0;

	for (uint8_t row = 0; row < 8; row++) {
		for (uint8_t col = 0; col < 8; col++) {
			uint8_t piece = _array[row][col];

			if (piece == (color ? w_queen : b_queen)) {
				queens_pos[queens_count] = { row, col };
				queens_count++;
			}
		}
	}

	// Look at every opponent move
	Board b(*this);
	b._player = !color;
	b.get_moves();

	// Value of a tempo per piece type (pawn, knight, bishop, rook, queen, king)
	constexpr int tempo_values[6] = { 35, 100, 65, 50, 0, 0 };

	// REVIEW: to simplify every capture computation
	constexpr int unsafe_attack_values[6] = { 15, 12, 10, 5, 0, 0 };

	// Total value of the moves able to attack each queen
	int queens_attacks_value[9] = { 0 };

	// TODO: consider only the safe moves
	//Map base_controls = color ? get_black_controls_map() : get_white_controls_map();

	SquareMap opponent_controls = color ? get_white_controls_map() : get_black_controls_map();

	// Count the number of moves attacking the queen
	// Instead of copying the full Board per move, patch _array in place
	for (uint8_t m = 0; m < b._got_moves; m++) {
		Move& move = b._moves[m];

		// Determine the piece (with possible promotion)
		const uint8_t saved_start = b._array[move.start_row][move.start_col];
		uint8_t piece = saved_start;
		// Raw get_moves() output carries unassigned flag bits: detect promotion
		// GEOMETRICALLY (pawn reaching last rank) exactly like make_move does.
		if (is_pawn(saved_start) && move.end_row == (saved_start == w_pawn ? 7 : 0)) {
			piece = promo_to_piece(move.get_promo_piece(), saved_start == w_pawn);
		}

		// Minimal _array patch for slider blocking
		const uint8_t saved_end = b._array[move.end_row][move.end_col];
		b._array[move.start_row][move.start_col] = none;
		b._array[move.end_row][move.end_col] = piece;

		// Look at the side's controls after the move
		SquareMap controls;
		add_piece_controls(&controls, move.end_row, move.end_col, piece);

		// Restore _array
		b._array[move.start_row][move.start_col] = saved_start;
		b._array[move.end_row][move.end_col] = saved_end;

		// Check whether any queen is attacked
		for (uint8_t q = 0; q < queens_count; q++) {
			if (controls._array[queens_pos[q].row][queens_pos[q].col]) {
				queens_attacks_value[q] += opponent_controls._array[move.end_row][move.end_col] ? unsafe_attack_values[(piece - 1) % 6] : tempo_values[(piece - 1) % 6];
			}
		}
	}

	// For now this stays linear in the number of moves able to attack the queen
	// TODO: use a richer function accounting for the queen's placement and the enemy pieces around it
	//constexpr int tempo_on_queen_malus = 50;

	// Value of an exact tempo, when the queen is attacked
	//constexpr int max_tempo = 100;

	int queens_tempo_penalty = 0;

	for (uint8_t q = 0; q < queens_count; q++) {
		//queens_tempo_penalty += queens_attacks_value[q] * tempo_on_queen_malus;
		queens_tempo_penalty += queens_attacks_value[q];
		//queens_tempo_penalty += max_tempo * (1.0f - 1.0f / (queens_attacks_value[q] + 1.0f));
	}

	// Add the term's value
	queen_safety_value -= queens_tempo_penalty * (1 - _adv);


	// TODO: proximity to the king should carry a penalty
	// Much larger on the same file at close range, and somewhat on a diagonal too


	return queen_safety_value;
}

