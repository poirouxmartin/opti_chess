#include "board.h"
#include "gui.h"

void Board::from_fen(string fen)
{
	string pgn;
	//reset_all();
	reset_board();

	// Deterministic failure state: on any parse error leave an EMPTY board
	// instead of a half-parsed position mixing stale constructor content with
	// partial FEN data (a half-parsed board once made a puzzle test flaky).
	auto fen_fail = [this](const char* msg) {
		cout << "invalid FEN: " << msg << endl;
		for (int r = 0; r < 8; r++)
			for (int c = 0; c < 8; c++)
				_array[r][c] = none;
	};

	// PGN
	main_GUI._initial_fen = fen;
	main_GUI._pgn = "";

	// Iterator walking the character string
	int iterator = 0;

	// Board position being iterated
	int row = 7;
	int col = 0;

	char c;

	// Piece placement
	while (row >= 0) {
		if (iterator >= static_cast<int>(fen.size())) {
			fen_fail("truncated");
			return;
		}
		c = fen[iterator];
		switch (c) {
		case '/': case ' ': row -= 1; col = 0; break;
		case 'P': _array[row][col] = w_pawn; col++; break;
		case 'N': _array[row][col] = w_knight; col++; break;
		case 'B': _array[row][col] = w_bishop; col++; break;
		case 'R': _array[row][col] = w_rook; col++; break;
		case 'Q': _array[row][col] = w_queen; col++; break;
		case 'K': _array[row][col] = w_king; col++; break;
		case 'p': _array[row][col] = b_pawn; col++; break;
		case 'n': _array[row][col] = b_knight; col++; break;
		case 'b': _array[row][col] = b_bishop; col++; break;
		case 'r': _array[row][col] = b_rook; col++; break;
		case 'q': _array[row][col] = b_queen; col++; break;
		case 'k': _array[row][col] = b_king; col++; break;
		default:
			if (isdigit(c)) {
				const int digit = (static_cast<int>(c)) - (static_cast<int>('0'));
				if (col + digit > 8) {
					fen_fail("row overflow");
					return;
				}
				for (int k = col; k < col + digit; k++) {
					_array[row][k] = 0;
				}
				col += digit;
				break;
			}

			else {
				fen_fail("bad character");
				return;
			}
		}

		iterator++;
	}

	// Validate exactly one king per side
	int w_kings = 0, b_kings = 0;
	for (int r = 0; r < 8; r++) {
		for (int c2 = 0; c2 < 8; c2++) {
			if (_array[r][c2] == w_king) w_kings++;
			if (_array[r][c2] == b_king) b_kings++;
		}
	}
	if (w_kings != 1 || b_kings != 1) {
		fen_fail("must have exactly one king per side");
		return;
	}

	// Side to move
	if (iterator >= static_cast<int>(fen.size())) {
		fen_fail("missing side to move");
		return;
	}
	c = fen[iterator];

	_player = c == 'w';

	iterator += 2;

	bool next = true;

	// Roques
	_castling_rights.k_w = false; _castling_rights.q_w = false; _castling_rights.k_b = false; _castling_rights.q_b = false;

	while (next) {
		if (iterator >= static_cast<int>(fen.size())) {
			fen_fail("truncated castling");
			return;
		}
		c = fen[iterator];

		switch (c) {
		case '-': iterator += 1; next = false; break;
		case 'K': _castling_rights.k_w = true; break;
		case 'Q': _castling_rights.q_w = true; break;
		case 'k': _castling_rights.k_b = true; break;
		case 'q': _castling_rights.q_b = true; iterator += 1; next = false; break;
		default: next = false; break;
		}

		iterator++;
	}

	if (iterator >= static_cast<int>(fen.size())) {
		fen_fail("missing en passant");
		return;
	}
	c = fen[iterator];

	// En passant
	if (c == '-')
		_en_passant_col = -1;
	else {
		_en_passant_col = fen[iterator] - 'a';
		iterator++;
	}

	iterator += 2;
	string s;
	while (iterator < static_cast<int>(fen.size()) && fen[iterator] != ' ') {
		s += fen[iterator];
		iterator++;
	}
	if (!s.empty()) _half_moves_count = stoi(s);

	iterator++;
	fen += ' ';
	s = "";
	while (iterator < static_cast<int>(fen.size()) && fen[iterator] != ' ') {
		s += fen[iterator];
		iterator++;
	}
	if (!s.empty()) _moves_count = stoi(s);

	_got_moves = -1;

	reset_eval();
	reset_positions_history();
	get_zobrist_key();
	_positions_history[_zobrist_key] = 1;

	update_kings_pos();

	update_bitboards();

	// Orient the board for the side to move
	main_GUI._board_orientation = _player;

	// Update the position FEN in the GUI
	main_GUI._initial_fen = fen;

	main_GUI._game_tree.new_tree(*this);
}

// Returns the FEN of the board
string Board::to_fen() const
{
	string s;
	int it = 0;

	for (int i = 7; i >= 0; i--) {
		for (int j = 0; j < 8; j++) {
			if (const uint8_t p = _array[i][j]; p == none)
				it++;
			else {
				constexpr auto piece_letters = "PNBRQKpnbrqk";

				if (it > 0) {
					s += static_cast<char>(it + 48);
					it = 0;
				}

				s += piece_letters[p - 1];
			}
		}

		if (it > 0) {
			s += static_cast<char>(it + 48);
			it = 0;
		}

		if (i > 0)
			s += "/";
	}

	if (_player)
		s += " w ";
	else
		s += " b ";

	if (_castling_rights.k_w)
		s += "K";
	if (_castling_rights.q_w)
		s += "Q";
	if (_castling_rights.k_b)
		s += "k";
	if (_castling_rights.q_b)
		s += "q";
	if (!(_castling_rights.k_w || _castling_rights.q_w || _castling_rights.k_b || _castling_rights.q_b))
		s += "-";

	string en_passant = "-";
	if (_en_passant_col != -1)
		en_passant = main_GUI._abc8[_en_passant_col] + static_cast<string>(_player ? "6" : "3");

	s += " " + en_passant + " " + to_string(_half_moves_count) + " " + to_string(_moves_count);

	return s;
}

