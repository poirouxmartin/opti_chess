#pragma once
//#include "board.h"

// Evaluateur de position (heuristiques)
class Evaluator {
public:

	// Paramètres d'évaluation

	// Coefficients des heuristiques
	float _piece_value = 1.0f;
	float _piece_mobility = 0.0f; // Redondant
	float _piece_positioning = 0.15f;
	float _bishop_pair = 35.0f;
	float _castling_rights = 0.0f; // Redondant
	float _player_trait = 35.0f;
	float _king_safety = 1.0f;
	float _pawn_structure = 0.25f;
	float _attacks = 0.65f;
	float _kings_opposition = 50.0f;
	float _push = 1.0f;
	float _open_files = 0.75f;
	float _square_controls = 0.3f;
	float _space_advantage = 1.0f;
	float _alignments = 0.75f;
	float _piece_activity = 1.0f;
	float _fianchetto = 1.0f;
	float _pawn_push_threats = 0.15f;
	float _king_proximity = 0.35f;
	float _king_centralization = 3.5f;
	float _rook_activity = 0.05f;
	float _bishop_pawns = 3.0f;
	float _weak_squares = 0.25f;
	float _castling_distance = 1.0f;
	float _bishop_activity = 4.0f;
	float _trapped_pieces = 0.55f;
	float _knight_activity = 0.75f;
	float _short_term_piece_mobility = 0.05f;
	float _long_term_piece_mobility = 0.13f;
	float _queen_safety = 1.0f;

	// Valeurs des pièces en début de partie (pion, cavalier, fou, tour, dame, roi), en position ouverte
	int _pieces_value_begin_open[6] = { 90, 400, 430, 600, 1185, 0 };

	// Valeurs des pièces en début de partie (pion, cavalier, fou, tour, dame, roi), en position fermée
	int _pieces_value_begin_closed[6] = { 90, 420, 410, 550, 1100, 0 };

	// Valeurs en fin de partie, en position ouverte
	int _pieces_value_end_open[6] = { 95, 390, 440, 690, 1350, 0 };

	// Valeurs des pièces en fin de partie, en position fermée
	int _pieces_value_end_closed[6] = { 95, 430, 410, 600, 1150, 0 };

	// Positionnement des pièces

	// En début de partie
	//int _pieces_pos_begin[6][8][8]{
	//	// pion
	//	{
	//		{   0,    0,    0,    0,    0,    0,    0,    0},
	//		{  78,   83,   86,  130,  130,   82,   85,   90},
	//		{  37,   29,   21,  120,  125,   60,   44,   57},
	//		{  10,   16,   20,   75,   80,   30,   35,   33},
	//		{  -5,   10,   45,   50,   65,   10,  -30,  -23},
	//		{ -25,    0,   30,   25,   35,  -35,   20,  -10},
	//		{ -30,   20,  -40,  -15,  -20,   20,   20,  -20},
	//		{   0,    0,    0,    0,    0,    0,    0,    0}   },

	//		// cavalier
	//		{
	//			{ -80,  -53,  -75,  -75,  -10,  -55,  -58,  -70},
	//			{  -3,   -6,   50,  -36,    4,   62,   -4,  -14},
	//			{ -10,   67,   25,   70,   80,   60,   62,   40},
	//			{ -10,   24,   40,   65,   65,   50,   15,   20},
	//			{ -10,    5,   31,   45,   40,   30,    2,    0},
	//			{ -18,   10,   25,   22,   18,   45,   20,  -14},
	//			{ -40,  -30,    2,    5,    2,    0,  -23,  -25},
	//			{ -74,  -35,  -36,  -34,  -19,  -25,  -75,  -59}   },

	//			// fou
	//			{
	//				{ -29,  -39,  -41,  -38,  -12,  -53,  -18,  -25},
	//				{  -5,   10,   17,  -21,  -19,   15,    1,  -11},
	//				{  -4,   19,  -16,   20,   26,   -5,   14,   -7},
	//				{  12,   10,   10,   17,   13,   12,   10,    5},
	//				{   6,    5,   17,   11,    8,   15,    0,    8},
	//				{   7,   12,   17,   15,   12,   22,   10,    7},
	//				{   9,   25,    5,    5,   10,    3,   32,    8},
	//				{ -15,    1,  -37,   -6,   -7,  -50,  -10,  -10}    },

	//				// tour
	//				{
	//					{   7,    5,    6,    6,    7,    6,   11,   10 },
	//					{  35,   34,   35,   38,   38,   35,   34,   35 },
	//					{   3,   -2,   -1,   -2,   -2,    1,   -2,    3 },
	//					{   0,   -3,   -2,   -3,   -4,   -6,   -3,    2 },
	//					{  -5,   -7,   -3,   -4,   -5,   -5,   -9,   -6 },
	//					{  -8,   -3,   -8,    4,    4,   -7,    2,    2 },
	//					{ -10,   -7,   -6,   -3,   -4,   -8,   -9,  -10 },
	//					{  -6,   -5,    3,    9,    9,    3,   -6,   -4 }   },

	//					// dame
	//					{
	//						{ -10,  -10,  -10,  -10,   10,    8,    8,    8 },
	//						{  -4,   -6,    0,    0,    0,    8,    8,   10 },
	//						{  -4,    0,    6,   10,    8,   10,    6,   12 },
	//						{   6,   -4,   -6,   -6,   -8,    4,    4,    2 },
	//						{   4,   -6,   -4,    2,   -6,    0,    4,    0 },
	//						{   0,   10,   -2,   14,   -6,    4,    0,    0 },
	//						{  -6,   -4,   12,   12,    6,    0,   -4,   -8 },
	//						{  -8,   -4,    0,   10,    4,   -4,   -6,   -8 }   },

	//						// roi
	//						{
	//							{ -60,   54,   47,  -99,  -99,   60,   83,  -62},
	//							{ -32,   10,   55,   56,   56,   55,   10,    3},
	//							{ -62,   12,  -57,  -50,  -67,   28,   37,  -31},
	//							{ -55,   50,   11,   -4,  -19,   13,  -35,  -49},
	//							{ -55,  -43,  -52,  -28,  -51,  -47,  -40,  -50},
	//							{ -47,  -42,  -43,  -79,  -64,  -42,  -59,  -42},
	//							{  -4,    3,  -14,  -90, -100,  -38,   13,    4},
	//							{  37,   60,   40,  -120, -100,  -30,   60,   48}   }
	//};

	// En fin de partie
	//int _pieces_pos_end[6][8][8]{
	//	// pion
	//	{
	//		{   0,    0,    0,    0,    0,    0,    0,    0},
	//		{ 150,  150,  150,  150,  150,  150,  150,  150},
	//		{  75,   75,   75,   75,   75,   75,   75,   75},
	//		{  40,   40,   40,   40,   40,   40,   40,   40},
	//		{  20,   20,   20,   20,   20,   20,   20,   20},
	//		{  10,   10,   10,   10,   10,   10,   10,   10},
	//		{ -20,  -40,  -30,    0,    0,  -30,  -40,  -20},
	//		{   0,    0,    0,    0,    0,    0,    0,    0}    },

	//		// cavalier
	//		{
	//			{ -50,  -25,  -25,  -25,  -25,  -25,  -25,  -50},
	//			{ -25,    0,    0,    0,    0,    0,    0,  -25},
	//			{ -25,    0,   10,   25,   25,   10,    0,  -25},
	//			{ -25,    0,   25,   50,   50,   25,    0,  -25},
	//			{ -25,    0,   25,   50,   50,   25,    0,  -25},
	//			{ -25,    0,   10,   25,   25,   10,    0,  -25},
	//			{ -25,    0,    0,    0,    0,    0,    0,  -25},
	//			{ -50,  -25,  -25,  -25,  -25,  -25,  -25,  -50}   },

	//			// fou
	//			{
	//				{ -50,  -25,  -25,  -25,  -25,  -25,  -25,  -50},
	//				{ -25,    0,    0,    0,    0,    0,    0,  -25},
	//				{ -25,    0,    5,    5,    5,    5,    0,  -25},
	//				{ -25,    0,    5,   10,   10,    5,    0,  -25},
	//				{ -25,    0,    5,   10,   10,    5,    0,  -25},
	//				{ -25,    0,    5,    5,    5,    5,    0,  -25},
	//				{ -25,    0,    0,    0,    0,    0,    0,  -25},
	//				{ -50,  -25,  -25,  -25,  -25,  -25,  -25,  -50}   },

	//				// tour
	//				{
	//					{   5,   15,   15,   15,   15,   15,   15,    5},
	//					{  50,   50,   50,   50,   50,   50,   50,   50},
	//					{  -5,    0,    0,    0,    0,    0,    0,   -5},
	//					{  -5,    0,    0,    0,    0,    0,    0,   -5},
	//					{  -5,    0,    0,    0,    0,    0,    0,   -5},
	//					{  -5,    0,    0,    0,    0,    0,    0,   -5},
	//					{  -5,    0,    0,    0,    0,    0,    0,   -5},
	//					{  10,   10,   10,   10,   10,   10,   10,   10}   },

	//					// dame
	//					{
	//						{ -40,  -30,  -20,  -10,  -10,  -20,  -30,  -40},
	//						{ -30,   37,   50,   50,   50,   50,   37,  -30},
	//						{   0,   45,   60,   75,   75,   60,   45,    0},
	//						{   0,   50,   60,  100,  100,   60,   50,    0},
	//						{ -10,   50,   50,  100,  100,   50,   50,  -10},
	//						{ -20,   50,   50,   50,   50,   50,   50,  -20},
	//						{ -40,   25,   25,   40,   40,   25,   25,  -40},
	//						{ -60,  -50,  -40,  -30,  -30,  -40,  -50,  -60}   },

	//						// roi
	//						{
	//							{-60, -45, -30, -15, -15, -30, -45, -60},
	//							{ -45, 54, 75, 75, 75, 75, 54, -45},
	//							{ 0, 66, 90, 111, 111, 90, 66, 0},
	//							{ 0, 75, 90, 150, 150, 90, 75, 0},
	//							{ -15, 75, 75, 150, 150, 75, 75, -15},
	//							{ -30, 75, 75, 75, 75, 75, 75, -30},
	//							{ -60, 36, 36, 60, 60, 36, 36, -60},
	//							{ -90, -75, -60, -45, -45, -60, -75, -90}	}
	//};

	int _pieces_pos_begin[6][8][8]{
		// Pawn MG
		{
			{0, 0, 0, 0, 0, 0, 0, 0},
			{-7, 7, -3, -13, 5, -16, 10, -8},
			{5, -12, -7, 22, -8, -5, -15, -8},
			{13, 0,   50, 100, 100, 25, -13, 5},
			{-4, -23, 100, 120, 120, 40, 4, -8}, 
			{-9, -15, 50, 40, 50, -35, 5, -22},
			{-50, -30, -10, 30, 30, -10, -20, -25},
			{0, 0, 0, 0, 0, 0, 0, 0}
		},

		// Knight MG
		{
			{-201, -83, -56, -26, -26, -56, 83, -201},
			{-67, -27, 4, 37, 37, 4, -27, -67},
			{-9, 22, 58, 53, 53, 58, 22, -9},
			{-34, 13, 44, 51, 51, 44, 13, -34},
			{-35, 8, 40, 49, 49, 40, 8, -35},
			{-61, -17, 6, 12, 12, 26, -17, -61},
			{-77, -41, -27, -5, -5, -27, -41, -77},
			{-175, -82, -74, -73, -73, -74, -130, -175}
		},

		// Bishop MG
		{
			{-48, 1, -14, -23, -23, -14, 1, -48},
			{-17, -14, 5, 0, 0, 5, -14, -17},
			{-16, 6, 1, 11, 11, 1, 6, -16},
			{-12, 20, 22, 31, 31, 22, 20, -12},
			{-5, 11, 35, 39, 39, 35, 11, -5},
			{-7, 21, 15, 30, 30, 15, 21, -7},
			{-15, 20, 19, 4, 4, 19, 20, -15},
			{-20, -5, -8, -23, -23, -50, -5, -20}
		},

		// Rook MG
		{
			{-17, -19, -1, 9, 9, -1, -19, -17},
			{-2, 12, 16, 18, 18, 16, 12, -2},
			{-22, -2, 6, 12, 12, 6, -2, -22},
			{-27, -15, -4, 3, 3, -4, -15, -27},
			{-13, -5, -4, -6, -6, -4, -5, -13},
			{-25, -11, -1, 3, 3, -1, -11, -25},
			{-21, -13, -8, 6, 6, -8, -13, -21},
			{-15, -10, 5, 25, 25, 5, -10, -15}
		},

		// Queen MG
		{
			{-2, -2, 1, -2, -2, 1, -2, -2},
			{-5, 6, 10, 8, 8, 10, 6, -5},
			{-4, 10, 6, 8, 8, 6, 10, -4},
			{0, 14, 12, 5, 5, 12, 14, 0},
			{4, 5, 9, 8, 8, 9, 5, 4},
			{-3, 6, 8, 7, 7, 8, 6, -3},
			{-3, 5, 15, 25, 25, 10, 5, -3},
			{3, -5, -5, 4, 4, -5, -5, 3}
		},

		// King MG
		{
			{59, 89, 45, -1, -1, 45, 89, 59},
			{88, 120, 65, 33, 33, 65, 120, 88},
			{123, 145, 81, 31, 31, 81, 145, 123},
			{154, 179, 105, 70, 70, 105, 179, 154},
			{164, 190, 138, 98, 98, 138, 190, 164},
			{195, 258, 169, 120, 120, 169, 258, 195},
			{350, 303, 200, 100, 100, 200, 303, 400},
			{450, 500, 300, 150, 250, 350, 500, 500}
		}
	};

	int _pieces_pos_end[6][8][8]{
		// Pawn EG
		{
			{0, 0, 0, 0, 0, 0, 0, 0},
			{0, -11, 12, 21, 25, 19, 4, 7},
			{28, 20, 21, 28, 30, 7, 6, 13},
			{10, 5, 4, -5, -5, -5, 14, 9},
			{6, -2, -8, -4, -13, -12, -10, -9},
			{-10, -10, -10, 4, 4, 3, -6, -4},
			{-10, -6, 10, 0, 14, 7, -5, -19},
			{0, 0, 0, 0, 0, 0, 0, 0}
		},

		// Knight EG
		{
			{-100, -88, -56, -17, -17, -56, -88, -100},
			{-69, -50, -51, 12, 12, -51, -50, -69},
			{-51, -44, -16, 17, 17, -16, -44, -51},
			{-45, -16, 9, 39, 39, 9, -16, -45},
			{-35, -2, 13, 28, 28, 13, -2, -35},
			{-40, -27, -8, 29, 29, -8, -27, -40},
			{-67, -54, -18, 8, 8, -18, -54, -67},
			{-96, -65, -49, -21, -21, -49, -65, -96}
		},

		// Bishop EG
		{
			{-46, -42, -37, -24, -24, -37, -42, -46},
			{-31, -20, -1, 1, 1, -1, -20, -31},
			{-30, 6, 4, 6, 6, 4, 6, -30},
			{-17, -1, -14, 15, 15, -14, -1, -17},
			{-20, -6, 0, 17, 17, 0, -6, -20},
			{-16, -1, -2, 10, 10, -2, -1, -16},
			{-37, -13, -17, 1, 1, -17, -13, -37},
			{-57, -30, -37, -12, -12, -37, -30, -57}
		},

		// Rook EG
		{
			{18, 0, 19, 13, 13, 19, 0, 18},
			{4, 5, 20, -5, -5, 20, 5, 4},
			{6, 1, -7, 10, 10, -7, 1, 6},
			{-5, 8, 7, -6, -6, 7, 8, -5},
			{-6, 1, -9, 7, 7, -9, 1, -6},
			{6, -8, -2, -6, -6, -2, -8, 6},
			{-12, -9, -1, -2, -2, -1, -9, -12},
			{-9, -13, -10, -9, -9, -10, -13, -9}
		},

		// Queen EG
		{
			{-75, -52, -43, -36, -36, -43, -52, -75},
			{-50, -27, -24, -8, -8, -24, -27, -50},
			{-38, -18, -12, 1, 1, -12, -18, -38},
			{-29, -6, 9, 21, 21, 9, -6, -29},
			{-23, -3, 13, 24, 24, 13, -3, -23},
			{-39, -18, -9, 3, 3, -9, -18, -39},
			{-55, -31, -22, -4, -4, -22, -31, -55},
			{-69, -57, -47, -26, -26, -47, -57, -69}
		},

		// King EG
		{
			{11, 59, 73, 78, 78, 73, 59, 11},
			{47, 121, 116, 131, 131, 116, 121, 47},
			{92, 172, 184, 191, 191, 184, 172, 92},
			{96, 166, 199, 199, 199, 199, 166, 96},
			{103, 156, 172, 172, 172, 172, 156, 103},
			{88, 130, 169, 175, 175, 169, 130, 88},
			{53, 100, 133, 135, 135, 133, 100, 53},
			{1, 45, 85, 76, 76, 85, 45, 1}
		}
	};



	// Constructeur par défaut
	Evaluator();

	// Constructeur par copie
	Evaluator(const Evaluator &evaluator);

	// Constructeur avec paramètres
	Evaluator(const float piece_value, const float piece_mobility = 0.0f, const float piece_positioning = 0.0f, const float bishop_pair = 0.0f, const float castling_rights = 0.0f, const float player_trait = 0.0f, const float king_safety = 0.0f, const float pawn_structure = 0.0f, const float attacks = 0.0f, const float defenses = 0.0f, const float kings_opposition = 0.0f, const float push = 0.0f, const float rook_open = 0.0f, const float square_controls = 0.0f, const float space_advantage = 0.0f, const float alignments = 0.0f, const float piece_activity = 0.0f, const float fianchetto = 0.0f, const float pawn_push_threats = 0.0f, const float king_proximity = 0.0f, const float rook_activity = 0.0f, const float bishop_pawns = 0.0f, const float weak_squares = 0.0f, const float castling_distance = 0.0f, const float bishop_activity = 0.0f, const float trapped_pieces = 0.0f, const float knight_activity = 0.0f, const float king_centralization = 0.0f, const float short_term_piece_mobility = 0.0f, const float long_term_piece_mobility = 0.0f, const float queen_safety = 0.0f);

	// Opérateur de copie
	Evaluator& operator=(const Evaluator &evaluator);


	// Méthodes

	// Evaluation de la position
	// TODO !!
	//float evaluate(const Board& board);
};