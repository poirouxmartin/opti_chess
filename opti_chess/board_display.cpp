#include "board.h"
#include "gui.h"
#include "useful_functions.h"

void Board::display_moves() {
	if (_got_moves == -1)
		get_moves();

	if (_got_moves == 0) {
		cout << "no legal moves" << endl;
		return;
	}

	sort_moves();
	cout << "total moves : " << (int)_got_moves << endl;
	cout << "moves : [|";
	for (int i = 0; i < _got_moves; i++)
		cout << " " << move_label(_moves[i]) << " |";
	cout << "]" << endl;
}

// Prints the board
void Board::display() const {
	cout << "  +-----------------+" << endl;
	for (int row = 7; row >= 0; row--) {
		cout << row + 1 << " | ";
		for (int col = 0; col < 8; col++) {
			cout << short_piece_name(_array[row][col]) << " ";
		}
		cout << "|" << endl;
	}
	cout << "  +-----------------+" << endl;
	cout << "    a b c d e f g h" << endl;
}

// Plays a move

string Board::move_label(Move move, bool use_uft8)
{
	assign_move_flags(&move);

	const uint8_t start_row = move.start_row;
	const uint8_t start_col = move.start_col;
	const uint8_t end_row = move.end_row;
	const uint8_t end_col = move.end_col;

	const uint8_t p1 = _array[start_row][start_col]; // Piece being moved
	const uint8_t p2 = _array[end_row][end_col];

	// To tell whether another similar piece can reach the same square
	bool spec_col = false;
	bool spec_row = false;

	if (_got_moves == -1) {
		get_moves();
	}

	uint8_t new_start_row; uint8_t new_start_col; uint8_t new_end_row; uint8_t new_end_col; uint8_t new_p;
	for (int m = 0; m < _got_moves; m++) {
		new_start_row = _moves[m].start_row;
		new_start_col = _moves[m].start_col;
		new_end_row = _moves[m].end_row;
		new_end_col = _moves[m].end_col;
		new_p = _array[new_start_row][new_start_col];

		// A different piece of the same type that can reach the same square
		if ((new_start_row != start_row || new_start_col != start_col) && new_p == p1 && new_end_row == end_row && new_end_col == end_col) {

			// Same file, so the rank has to be spelled out
			if (new_start_col == start_col)
				spec_row = true;
			else {
				spec_col = true;
			}
		}
	}

	string s;

	bool is_castle = false;

	switch (p1)	{
	case w_knight: case b_knight: s += use_uft8 ? (_player ? main_GUI.N_symbol : main_GUI.n_symbol) : "N"; if (spec_col) s += main_GUI._abc8[start_col]; if (spec_row) s += static_cast<char>(start_row + 1 + 48); break;
	case w_bishop: case b_bishop: s += use_uft8 ? (_player ? main_GUI.B_symbol : main_GUI.b_symbol) : "B"; if (spec_col) s += main_GUI._abc8[start_col]; if (spec_row) s += static_cast<char>(start_row + 1 + 48); break;
	case w_rook: case b_rook: s += use_uft8 ? (_player ? main_GUI.R_symbol : main_GUI.r_symbol) : "R"; if (spec_col) s += main_GUI._abc8[start_col]; if (spec_row) s += static_cast<char>(start_row + 1 + 48); break;
	case w_queen: case b_queen: s += use_uft8 ? (_player ? main_GUI.Q_symbol : main_GUI.q_symbol) : "Q"; if (spec_col) s += main_GUI._abc8[start_col]; if (spec_row) s += static_cast<char>(start_row + 1 + 48); break;
	case w_king: case b_king:
		if (end_col - start_col == 2) {
			s += "O-O"; is_castle = true; break;
		}
		if (start_col - end_col == 2) {
			s += "O-O-O"; is_castle = true; break;
		}
		s += use_uft8 ? (_player ? main_GUI.K_symbol : main_GUI.k_symbol) : "K"; break;
	}

	// Capture (or en passant)
	if (move.is_capture()) {
		if (is_pawn(p1))
			s += main_GUI._abc8[start_col];
		s += "x";
		s += main_GUI._abc8[end_col];
		s += static_cast<char>(end_row + 1 + 48);
	}

	else if (!is_castle) {
		s += main_GUI._abc8[end_col];
		s += static_cast<char>(end_row + 1 + 48);
	}

	// Promotion (queen only for now)
	if ((p1 == w_pawn && end_row == 7) || (p1 == b_pawn && end_row == 0)) {
		s += "=";
		s += use_uft8 ? (_player ? main_GUI.Q_symbol : main_GUI.q_symbol) : "Q";
	}

	if (move.is_checkmate())
		return _player ? s + "# 1-0" : s + "# 0-1";

	if (move.is_check())
		return s + "+";

	Board temp_board = *this;
	temp_board.make_move(move, false);

	if (temp_board.is_game_over() == draw) {
		return s + " 1/2-1/2";
	}

	return s;
}

// Draws a text inside a given area
void Board::draw_text_rect(const string& s, const float pos_x, const float pos_y, const float width, const float height, const float size) {

	// Text splitting
	const int sub_div = (1.5f * width) / size;

	if (width <= 0 || height <= 0 || sub_div <= 0)
		return;

	const Rectangle rect_text = { pos_x, pos_y, width, height };
	DrawRectangleRec(rect_text, main_GUI._background_text_color);

	const size_t string_size = s.length();
	int i = 0;
	while (sub_div * i <= string_size) {
		const char* c = s.substr(i * sub_div, sub_div).c_str();
		DrawTextEx(main_GUI._text_font, c, { pos_x, pos_y + i * size }, size, main_GUI._font_spacing * size, main_GUI._text_color);
		i++;
	}
}

// Plays the sound of a move
void Board::play_move_sound(Move move) {
	assign_move_flags(&move);

	//cout << "Flags: " << move.is_capture << " " << move.is_check << " " << move.is_promotion << endl;

	const uint8_t i = move.start_row;
	const uint8_t j = move.start_col;
	const uint8_t k = move.end_row;
	const uint8_t l = move.end_col;

	// Pieces
	const uint8_t p1 = _array[i][j];
	const uint8_t p2 = _array[k][l];


	// End-of-game sounds

	// Mat
	if (move.is_checkmate())
		PlaySound(main_GUI._checkmate_sound);

	// Check
	else if (move.is_check()) {
		PlaySound(main_GUI._check_sound);
	}

	else {
		Board temp_board = *this;
		temp_board.make_move(move, false);

		if (temp_board.is_game_over() == draw) {
			PlaySound(main_GUI._stalemate_sound);
		}
	}

	// Promotion
	if (move.is_promotion())
		PlaySound(main_GUI._promotion_sound);

	// Capture
	if (move.is_capture()) {
		PlaySound(main_GUI._capture_sound);
	}

	// Roques
	if (p1 == w_king && abs(j - l) == 2 || (p1 == b_king && abs(j - l) == 2))
		PlaySound(main_GUI._castle_sound);

	// "Normal" move
	if (!move.is_check()) {
		PlaySound(main_GUI._move_sound);
	}

	return;
}

void Board::display_pgn() const
{
	cout << "\n***** PGN *****\n" << main_GUI._pgn << "\n***** PGN *****" << endl;
}

// Tells from an evaluation whether it is a mate: 0 if not, otherwise the move count to mate, positive for White and negative for Black
int Board::is_eval_mate(const int e) const
{
	int abs_eval = abs(e);

	if (10 * abs_eval > mate_value) {
		int mate_moves = static_cast<int>(mate_value - abs_eval - _moves_count * mate_ply) * (e > 0 ? 1 : -1) / mate_ply + (_player && e > 0);
		return mate_moves != 0 ? mate_moves : 1;
	}
	else
		return 0;
}

// Generates the opening book
void Board::generate_opening_book(int nodes) {
	//// Lit le livre d'ouvertures actuel
	//string book = LoadFileText("resources/data/opening_book.txt");
	//cout << "Book : " << book << endl;

	//// Seek to the relevant spot in the book ----> store FENs in the book and search them?
	//to_fen();
	//size_t pos = book.find(_fen); // What if there are several? Build an array of positions, split the book into more parts, then insert in the middle
	//const string book_part_1;
	//const string book_part_2;

	//const string add_to_book = "()";

	//// Check whether every move has been tested; otherwise test one of the remaining ones with `nodes` nodes

	//const string new_book = book_part_1 + add_to_book + book_part_2;

	//SaveFileText("resources/data/opening_book.txt", const_cast<char*>(new_book.c_str()));
}

int time_to_play_move(const int t1, int t2, const float k) {
	return t1 * k;

	// Still to improve:
	// Prendre en compte le temps de l'adversaire
	// account for the number of moves left in the game, or an approximation -> spend longer when mating or nearly lost
	// account for evaluation swings, or rising moves
	// increments are still unhandled
	// a minimum node count before moving?
}

bool Board::click_m_move(const Move m, const bool orientation) const
{
	simulate_mouse_release();
	click_move(m.start_row, m.start_col, m.end_row, m.end_col , main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom, orientation, m.is_promotion());
	SetWindowFocused();

	return true;
}

// Updates a text box
void update_text_box(TextBox& text_box) {
	// If a click happens
	if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
		const Vector2 mouse_pos = GetMousePosition();
		const Rectangle rect = { text_box.x, text_box.y, text_box.width, text_box.height };

		// Over the text box
		if (CheckCollisionPointRec(mouse_pos, rect)) {
			// Activate, and show the value
			if (!text_box.active) {
				text_box.active = true;
				text_box.text = to_string(text_box.value);
			}
		}

		else {
			// Deactivate, and update the value
			if (text_box.active) {
				text_box.value = stoi(text_box.text);
				text_box.active = false;
			}
		}
	}

	// If it is active
	if (text_box.active) {
		// Take the keyboard input
		const int key = GetKeyPressed();

		// Only the relevant keys, the numeric ones
		if ((key >= 320 && key <= 329) || (key == KEY_BACKSPACE) || (key == KEY_ENTER)) {
			if (key == KEY_BACKSPACE) {
				if (!text_box.text.empty())
					text_box.text.pop_back();
			}
			else if (key == KEY_ENTER) {
				text_box.value = stoi(text_box.text);
				text_box.active = false;
			}

			else {
				const int pressed_value = key - 320;
				text_box.text += static_cast<char>(pressed_value + 48);
			}
		}
	}
}

// Draws a text box
void draw_text_box(const TextBox& text_box) {
	const Rectangle rect = { text_box.x, text_box.y, text_box.width, text_box.height };
	DrawRectangleRec(rect, text_box.main_color);

	//Vector2 text_dimensions = MeasureTextEx(textBox.text_font, textBox.text.c_str(), textBox.text_size, font_spacing * textBox.text_size); // for centred drawing
	const float text_x = text_box.x + text_box.text_size / 2;
	const float text_y = text_box.y + text_box.text_size / 4;
	DrawTextEx(text_box.text_font, text_box.text.c_str(), { text_x, text_y }, text_box.text_size, main_GUI._font_spacing * text_box.text_size, text_box.text_color);
}

string Board::algebric_notation(Move move) const {
	string move_notation = main_GUI._abc8[move.start_col] + to_string(move.start_row + 1) + main_GUI._abc8[move.end_col] + to_string(move.end_row + 1);

	// Promotion
	if (move.end_row == 7 && _array[move.start_row][move.start_col] == 1)
		move_notation += "q";
	else if (move.end_row == 0 && _array[move.start_row][move.start_col] == 7)
		move_notation += "q";

	return move_notation;
}

// Converts an algebraic notation into a move
Move Board::move_from_algebric_notation(string notation) {
	return Move(notation[1] - '1', notation[0] - 'a', notation[3] - '1', notation[2] - 'a');
}


string Board::evaluation_to_string(int eval) const {
	string eval_string = "";

	if (eval > 0)
		eval_string += "+";

	// Is this a mate?
	if (int mate = is_eval_mate(eval); mate != 0) {
		if (eval < 0)
			eval_string += "-";

		eval_string += "M";
		eval_string += to_string(abs(mate));
	}
	else {
		eval_string += to_string(eval);
	}

	return eval_string;
}

// Returns the trapped-piece evaluation

string square_name(uint8_t row, uint8_t col) {
	return string(1, 'a' + col) + string(1, '1' + row);
}

// Returns the name of a piece
string piece_name(uint8_t piece) {
	switch (piece) {
	case w_pawn:
		return "w_pawn";
	case w_knight:
		return "w_knight";
	case w_bishop:
		return "w_bishop";
	case w_rook:
		return "w_rook";
	case w_queen:
		return "w_queen";
	case w_king:
		return "w_king";
	case b_pawn:
		return "b_pawn";
	case b_knight:
		return "b_knight";
	case b_bishop:
		return "b_bishop";
	case b_rook:
		return "b_rook";
	case b_queen:
		return "b_queen";
	case b_king:
		return "b_king";
	default:
		return "none";
	}
}

// Returns the name of a piece
string short_piece_name(uint8_t piece) {
	switch (piece) {
	case w_pawn:
		return "P";
	case w_knight:
		return "N";
	case w_bishop:
		return "B";
	case w_rook:
		return "R";
	case w_queen:
		return "Q";
	case w_king:
		return "K";
	case b_pawn:
		return "p";
	case b_knight:
		return "n";
	case b_bishop:
		return "b";
	case b_rook:
		return "r";
	case b_queen:
		return "q";
	case b_king:
		return "k";
	default:
		return ".";
	}
}

// Returns an activity bonus for the knights

