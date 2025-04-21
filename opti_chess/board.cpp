#include "board.h"
#include "gui.h"
#include "useful_functions.h"
#include "windows_tests.h"
#include "buffer.h"
#include "game_tree.h"
#include "zobrist.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <sstream>
#include <thread>
#include <cmath>
#include <utility>
#include <iomanip>


// Tests pour la parallélisation
//vector<thread> threads;

// Constructeur par défaut
Board::Board() {
}

// Constructeur de copie
Board::Board(const Board& b, bool full, bool copy_history) {
	copy_data(b, full, copy_history);
}

// Fonction qui copie les attributs d'un tableau
void Board::copy_data(const Board& b, bool full, bool copy_history) {

	// Copie du plateau
	memcpy(_array, b._array, sizeof(_array));
	_got_moves = b._got_moves;
	_player = b._player;
	memcpy(_moves, b._moves, sizeof(_moves));
	_sorted_moves = b._sorted_moves;
	_evaluation = b._evaluation;
	_castling_rights = b._castling_rights;
	_half_moves_count = b._half_moves_count;
	_moves_count = b._moves_count;
	_white_king_pos = b._white_king_pos;
	_black_king_pos = b._black_king_pos;
	_en_passant_col = b._en_passant_col;
	_evaluated = b._evaluated;
	_static_evaluation = b._static_evaluation;
	_zobrist_key = b._zobrist_key;

	if (copy_history) {
		_positions_history = b._positions_history;
	}

	if (full) {
		_is_active = b._is_active;
		_adv = b._adv;
		_advancement = b._advancement;
		_game_over_checked = b._game_over_checked;
		_game_over_value = b._game_over_value;
		_displayed_components = b._displayed_components;
	}
}

// Fonction qui ajoute un coup dans une liste de coups
bool Board::add_move(const uint_fast8_t start_row, const uint_fast8_t start_col, const uint_fast8_t end_row, const uint_fast8_t end_col, uint_fast8_t* iterator, const uint_fast8_t piece)
{
	// Si on dépasse le nombre de coups que l'on pensait possible dans une position
	if (*iterator >= max_moves)
		return false;

	const Move m(start_row, start_col, end_row, end_col); // Si on utilise pas les flag, autant éviter les calculs inutiles
	//const Move m(i, j, k, l, _array[k][l] != 0, (piece == 1 && i == 7) || (piece == 7 && i == 1));
	_moves[*iterator] = m;

	// Incrémentation du nombre de coups
	(*iterator)++;

	return true;
}

// Fonction qui ajoute les coups "pions" dans la liste de coups
bool Board::add_pawn_moves(const uint_fast8_t row, const uint_fast8_t col, uint_fast8_t* iterator) {

	// Joueur avec les pièces blanches
	if (_player) {
		_array[row + 1][col] == none && add_move(row, col, row + 1, col, iterator, w_pawn) && row == 1 && _array[row + 2][col] == none && add_move(row, col, row + 2, col, iterator, w_pawn); // Poussées
		col > 0 && (is_black(_array[row + 1][col - 1]) || (_en_passant_col == col - 1 && row == 4)) && add_move(row, col, row + 1, col - 1, iterator, w_pawn); // Prise (gauche)
		col < 7 && (is_black(_array[row + 1][col + 1]) || (_en_passant_col == col + 1 && row == 4)) && add_move(row, col, row + 1, col + 1, iterator, w_pawn); // Prise (droite)
	}

	// Joueur avec les pièces noires
	else {
		_array[row - 1][col] == none && add_move(row, col, row - 1, col, iterator, b_pawn) && row == 6 && _array[row - 2][col] == none && add_move(row, col, row - 2, col, iterator, b_pawn); // Poussées
		col > 0 && (is_white(_array[row - 1][col - 1]) || (_en_passant_col == col - 1 && row == 3)) && add_move(row, col, row - 1, col - 1, iterator, b_pawn); // Prise (gauche)
		col < 7 && (is_white(_array[row - 1][col + 1]) || (_en_passant_col == col + 1 && row == 3)) && add_move(row, col, row - 1, col + 1, iterator, b_pawn); // Prise (droite)
	}

	return true;
}

// Fonction qui ajoute les coups "cavaliers" dans la liste de coups
bool Board::add_knight_moves(const uint_fast8_t row, const uint_fast8_t col, uint_fast8_t* iterator, const uint_fast8_t piece) {

	for (uint_fast8_t m = 0; m < 8; m++) {
		const uint_fast8_t new_row = row + knight_directions[m][0];
		if (!is_in_fast(new_row, 0, 7))
			continue;

		const uint_fast8_t new_col = col + knight_directions[m][1];
		if (!is_in_fast(new_col, 0, 7))
			continue;

		((_player && !is_white(_array[new_row][new_col])) || (!_player && !is_black(_array[new_row][new_col]))) && add_move(row, col, new_row, new_col, iterator, piece);
	}

	return true;
}

// Fonction qui ajoute les coups diagonaux dans la liste de coups
bool Board::add_diag_moves(const uint_fast8_t row, const uint_fast8_t col, uint_fast8_t* iterator, const uint_fast8_t piece) {

	const uint_fast8_t ally_min = _player ? w_pawn : b_pawn;
	const uint_fast8_t ally_max = _player ? w_king : b_king;

	for (uint_fast8_t m = 0; m < 4; m++) {

		// Direction
		const uint_fast8_t d_row = diag_directions[m][0];
		const uint_fast8_t d_col = diag_directions[m][1];

		uint_fast8_t current_row = row + d_row;
		uint_fast8_t current_col = col + d_col;

		while (current_row >= 0 && current_row < 8 && current_col >= 0 && current_col < 8) {
			const uint_fast8_t p2 = _array[current_row][current_col];

			// Si y'a une pièce alliée, on arrête
			if (is_in_fast(p2, ally_min, ally_max))
				break;

			// Coup possible
			add_move(row, col, current_row, current_col, iterator, piece);

			// Si y'a une pièce ennemie, on arrête
			if (p2 != none)
				break;

			current_row += d_row;
			current_col += d_col;
		}
	}

	return true;
}

// Fonction qui ajoute les coups horizontaux et verticaux dans la liste de coups
bool Board::add_rect_moves(const uint_fast8_t row, const uint_fast8_t col, uint_fast8_t* iterator, const uint_fast8_t piece) {

	const uint_fast8_t ally_min = _player ? w_pawn : b_pawn;
	const uint_fast8_t ally_max = _player ? w_king : b_king;

	for (uint_fast8_t m = 0; m < 4; m++) {

		// Direction
		const uint_fast8_t d_row = rect_directions[m][0];
		const uint_fast8_t d_col = rect_directions[m][1];

		uint_fast8_t current_row = row + d_row;
		uint_fast8_t current_col = col + d_col;

		while (current_row >= 0 && current_row < 8 && current_col >= 0 && current_col < 8) {
			const uint_fast8_t p2 = _array[current_row][current_col];

			// Si y'a une pièce alliée, on arrête
			if (is_in_fast(p2, ally_min, ally_max))
				break;

			// Coup possible
			add_move(row, col, current_row, current_col, iterator, piece);

			// Si y'a une pièce ennemie, on arrête
			if (p2 != none)
				break;

			current_row += d_row;
			current_col += d_col;
		}
	}

	return true;
}

// Fonction qui ajoute les coups "roi" dans la liste de coups
bool Board::add_king_moves(const uint_fast8_t row, const uint_fast8_t col, uint_fast8_t* iterator, const uint_fast8_t piece) {

	const uint_fast8_t ally_min = _player ? w_pawn : b_pawn;
	const uint_fast8_t ally_max = _player ? w_king : b_king;

	// TODO *** utiliser les all_directions?
	for (uint_fast8_t m = 0; m < 8; m++) {

		// Direction
		const uint_fast8_t new_row = row + all_directions[m][0];
		if (!is_in_fast(new_row, 0, 7))
			continue;

		const uint_fast8_t new_col = col + all_directions[m][1];
		if (!is_in_fast(new_col, 0, 7))
			continue;

		((_player && !is_white(_array[new_row][new_col])) || (!_player && !is_black(_array[new_row][new_col]))) && add_move(row, col, new_row, new_col, iterator, piece);
	}

	return true;
}

// Calcule la liste des coups possibles. pseudo ici fait référence au droit de roquer en passant par une position illégale.
bool Board::get_moves(const bool forbide_check) {

	// Itérateur
	uint_fast8_t iterator = 0;

	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {

			// Pièce
			const uint_fast8_t piece = _array[row][col];

			// Si on dépasse le nombre de coups que l'on pensait possible dans une position
			if (iterator >= max_moves) {
				cout << "Too many moves in the position : " << to_fen() << " : moves > " << (int)iterator << endl;
				return false;
			}

			// Le joueur doit correspondre à la couleur de la pièce
			if ((_player && !is_white(piece) || (!_player && !is_black(piece))))
				continue;

			switch (piece) {
				case none:
					break;
				case w_pawn:
					add_pawn_moves(row, col, &iterator);
					break;
				case w_knight:
					add_knight_moves(row, col, &iterator, w_knight);
					break;
				case w_bishop:
					add_diag_moves(row, col, &iterator, w_bishop);
					break;
				case w_rook:
					add_rect_moves(row, col, &iterator, w_rook);
					break;
				case w_queen:
					add_diag_moves(row, col, &iterator, w_queen) && add_rect_moves(row, col, &iterator, w_queen);
					break;
				case w_king:
					add_king_moves(row, col, &iterator, w_king);

					// Roques (TODO *** optimisable)
					// Grand
					if (_castling_rights.q_w && _array[row][col - 1] == none && _array[row][col - 2] == none && _array[row][col - 3] == none && (!is_controlled(row, col, true) && !is_controlled(row, col - 1, true) && !is_controlled(row, col - 2, true)))
						add_move(row, col, row, col - 2, &iterator, w_king);
					// Petit
					if (_castling_rights.k_w && _array[row][col + 1] == none && _array[row][col + 2] == none && (!is_controlled(row, col, true) && !is_controlled(row, col + 1, true) && !is_controlled(row, col + 2, true)))
						add_move(row, col, row, col + 2, &iterator, w_king);
					break;
				case b_pawn: // Pion noir
					add_pawn_moves(row, col, &iterator);
					break;
				case b_knight: // Cavalier noir
					add_knight_moves(row, col, &iterator, b_knight);
					break;
				case b_bishop: // Fou noir
					add_diag_moves(row, col, &iterator, b_bishop);
					break;
				case b_rook: // Tour noire
					add_rect_moves(row, col, &iterator, b_rook);
					break;
				case b_queen: // Dame noire
					add_diag_moves(row, col, &iterator, b_queen) && add_rect_moves(row, col, &iterator, b_queen);
					break;
				case b_king: // Roi noir
					add_king_moves(row, col, &iterator, b_king);
					// Roques
					// Grand
					if (_castling_rights.q_b && _array[row][col - 1] == none && _array[row][col - 2] == none && _array[row][col - 3] == none && (!is_controlled(row, col, false) && !is_controlled(row, col - 1, false) && !is_controlled(row, col - 2, false)))
						add_move(row, col, row, col - 2, &iterator, b_king);
					// Petit
					if (_castling_rights.k_b && _array[row][col + 1] == none && _array[row][col + 2] == none && (!is_controlled(row, col, false) && !is_controlled(row, col + 1, false) && !is_controlled(row, col + 2, false)))
						add_move(row, col, row, col + 2, &iterator, b_king);
					break;
			}
		}
	}

	_got_moves = iterator;

	// Vérification échecs (TODO *** optimisable)
	if (forbide_check) {
		uint_fast8_t n_moves = 0;
		Board b;

		for (uint_fast8_t i = 0; i < _got_moves; i++) {
			b.copy_data(*this);
			b.make_move(_moves[i]);
			b._player = !b._player;

			if (!b.in_check()) {
				_moves[n_moves] = _moves[i];  // On écrase les coups invalides
				n_moves++;
			}
		}

		_got_moves = n_moves;
	}


	return true;
}

// Fonction qui dit s'il y'a échec
bool Board::in_check()
{
	update_kings_pos();

	const int king_i = _player ? _white_king_pos.row : _black_king_pos.row;
	const int king_j = _player ? _white_king_pos.col : _black_king_pos.col;

	// Comment aller plus vite : partir du roi, pour trouver les potentiels attaquants :
	// Regarder les diagonales, les lignes/colonnes, et voit si une pièce adverse attaque le roi par cette direction

	//return attacked(king_i, king_j);

	// Regarde les potentielles attaques de cavalier
	static constexpr int knight_offsets[8][2] = { {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}, {1, -2}, {2, -1}, {2, 1}, {1, 2} };
	// TODO : regrouper avec ceux des autres fonctions?

	const int enemy_knight = 2 + _player * 6;

	for (int k = 0; k < 8; k++) {

		// Si le cavalier est hors du plateau, on passe
		const int ni = king_i + knight_offsets[k][0];
		if (!is_in(ni, 0, 7))
			continue;

		const int nj = king_j + knight_offsets[k][1];
		if (!is_in(nj, 0, 7))
			continue;

		// S'il y a un cavalier qui attaque, renvoie vrai (en échec)
		if (_array[ni][nj] == enemy_knight)
			return true;
	}

	// TODO : Faut-il regarder les lignes dans un certain ordre, pour faire moins de calcul (car l'adversaire a plus de chances d'attaquer par le milieu de l'échiquier?)

	// Regarde les lignes horizontales et verticales

	// Gauche
	for (int j = king_j - 1; j >= 0; j--)
	{
		// Si y'a une pièce
		if (const uint_fast8_t piece = _array[king_i][j]; piece != none)
		{
			// Si la pièce n'est pas au joueur, regarde si c'est une tour, une dame, ou un roi avec une distance de 1
			if (piece < 7 != _player)
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 4 || simple_piece == 5 || (simple_piece == 6 && j == king_j - 1))
					return true;

			break;
		}
	}

	// Droite
	for (int j = king_j + 1; j < 8; j++)
	{
		if (const uint_fast8_t piece = _array[king_i][j]; piece != 0)
		{
			if (piece < 7 != _player)
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 4 || simple_piece == 5 || (simple_piece == 6 && j == king_j + 1))
					return true;

			break;
		}
	}

	// Haut
	for (int i = king_i - 1; i >= 0; i--)
	{
		if (const uint_fast8_t piece = _array[i][king_j]; piece != 0)
		{
			if (piece < 7 != _player)
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 4 || simple_piece == 5 || (simple_piece == 6 && i == king_i - 1))
					return true;

			break;
		}
	}

	// Bas
	for (int i = king_i + 1; i < 8; i++)
	{
		if (const uint_fast8_t piece = _array[i][king_j]; piece != 0)
		{
			if (piece < 7 != _player)
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 4 || simple_piece == 5 || (simple_piece == 6 && i == king_i + 1))
					return true;

			break;
		}
	}

	// Regarde les diagonales

	// Diagonale bas-gauche
	for (int i = king_i - 1, j = king_j - 1; i >= 0 && j >= 0; i--, j--)
	{
		if (const uint_fast8_t piece = _array[i][j]; piece != 0)
		{
			if (piece < 7 != _player)
			{
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 3 || simple_piece == 5 || (simple_piece == 6 && (abs(king_i - i) == 1)))
					return true;

				// Cas spécial pour les pions
				if (piece == 1 && abs(king_j - j) == 1)
					return true;
			}

			break;
		}
	}

	// Diagonal bas-droite
	for (int i = king_i - 1, j = king_j + 1; i >= 0 && j < 8; i--, j++)
	{
		if (const uint_fast8_t piece = _array[i][j]; piece != 0)
		{
			if ((piece < 7) != _player)
			{
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 3 || simple_piece == 5 || (simple_piece == 6 && abs(king_i - i) == 1))
					return true;

				// Special case for pawns
				if (piece == 1 && abs(king_j - j) == 1)
					return true;
			}
			break;
		}
	}

	// Diagonale haut-gauche
	for (int i = king_i + 1, j = king_j - 1; i < 8 && j >= 0; i++, j--)
	{
		if (const uint_fast8_t piece = _array[i][j]; piece != 0)
		{
			if ((piece < 7) != _player)
			{
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 3 || simple_piece == 5 || (simple_piece == 6 && abs(king_i - i) == 1))
					return true;

				// Special case for pawns
				if (piece == 7 && abs(king_j - j) == 1)
					return true;
			}
			break;
		}
	}

	// Diagonale haut-droite
	for (int i = king_i + 1, j = king_j + 1; i < 8 && j < 8; i++, j++)
	{
		if (const uint_fast8_t piece = _array[i][j]; piece != 0)
		{
			if ((piece < 7) != _player)
			{
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 3 || simple_piece == 5 || (simple_piece == 6 && abs(king_i - i) == 1))
					return true;

				// Special case for pawns
				if (piece == 7 && abs(king_j - j) == 1)
					return true;
			}
			break;
		}
	}

	return false;
}

// Fonction qui affiche la liste des coups donnée en argument
void Board::display_moves(const bool pseudo) {
	if (_got_moves == -1)
		get_moves(!pseudo);

	if (_got_moves == 0) {
		cout << "no legal moves" << endl;
		return;
	}

	cout << "[|";
	for (int i = 0; i < _got_moves; i++)
		cout << " " << move_label(_moves[i]) << " |";
	cout << "]" << endl;
}

// Fonction qui joue un coup
void Board::make_move(Move move, const bool pgn, const bool new_board, const bool add_to_history)
{
	// TODO : à voir si ça rend plus rapide ou non
	const uint_fast8_t row1 = move.start_row;
	const uint_fast8_t col1 = move.start_col;
	const uint_fast8_t row2 = move.end_row;
	const uint_fast8_t col2 = move.end_col;
	const uint_fast8_t p = _array[row1][col1];
	const uint_fast8_t p_last = _array[row2][col2];


	// TODO : rendre plus efficace
	if (pgn) {
		if (_moves_count != 0 || _half_moves_count != 0)
			main_GUI._pgn += " ";
		if (_player) {
			stringstream ss;
			ss << _moves_count;
			string s;
			ss >> s;
			main_GUI._pgn += s;
			main_GUI._pgn += ". ";
		}
		main_GUI._pgn += move_label(move);
	}



	// Incrémentation des demi-coups
	_half_moves_count++;

	// Reset des demi-coups si un pion est bougé ou si une pièce est prise
	if (is_pawn(p) || p_last) {
		_half_moves_count = 0;
		reset_positions_history();
	}
	else {
		// Ajoute la position actuelle dans l'historique
		if (add_to_history) {
			get_zobrist_key();
			_positions_history.push_back(_zobrist_key);
			//_positions_history[_zobrist_key] += 1;
		}
	}


	// Coups donnant la possibilité d'un en passant
	_en_passant_col = -1;

	// Pion qui avance de 2 cases, et pion adverse à gauche ou à droite -> possibilité d'en passant
	(p == w_pawn && row2 == row1 + 2 && (_array[row2][col2 - 1] == b_pawn || _array[row2][col2 + 1] == b_pawn)) && ((_en_passant_col = col1)); // FIXME: si l - 1 ou l + 1 est hors du plateau?
	(p == b_pawn && row2 == row1 - 2 && (_array[row2][col2 - 1] == w_pawn || _array[row2][col2 + 1] == w_pawn)) && ((_en_passant_col = col1));

	// En passant
	(p == w_pawn && col1 != col2 && p_last == 0) && ((_array[row2 - 1][col2] = 0));
	(p == b_pawn && col1 != col2 && p_last == 0) && ((_array[row2 + 1][col2] = 0));


	// Roi blanc
	if (p == w_king) {
		_castling_rights.q_w = false;
		_castling_rights.k_w = false;
		_white_king_pos = { row2, col2 }; // Met à jour la position du roi

		(col2 == col1 + 2) && ((_array[0][7] = none), (_array[0][5] = w_rook)); // Petit roque
		(col2 == col1 - 2) && ((_array[0][0] = none), (_array[0][3] = w_rook)); // Grand roque
	}

	// Roi noir
	else if (p == b_king) {
		_castling_rights.q_b = false;
		_castling_rights.k_b = false;
		_black_king_pos = { row2, col2 };

		(col2 == col1 + 2) && ((_array[7][7] = none), (_array[7][5] = b_rook)); // Petit roque
		(col2 == col1 - 2) && ((_array[7][0] = none), (_array[7][3] = b_rook)); // Grand roque
	}

	// Perte des droits de roque

	// Tour blanche
	(p == w_rook) && ((col1 == 0) && (row1 == 0) && (_castling_rights.q_w = false) || (col1 == 7) && (row1 == 0) && (_castling_rights.k_w = false));

	// Tour noire
	(p == b_rook) && ((col1 == 0) && (row1 == 7) && (_castling_rights.q_b = false) || (col1 == 7) && (row1 == 7) && (_castling_rights.k_b = false));

	// Tour blanche mangée
	(p_last == w_rook) && ((col2 == 0) && (row2 == 0) && (_castling_rights.q_w = false) || (col2 == 7) && (row2 == 0) && (_castling_rights.k_w = false));

	// Tour noire mangée
	(p_last == b_rook) && ((col2 == 0) && (row2 == 7) && (_castling_rights.q_b = false) || (col2 == 7) && (row2 == 7) && (_castling_rights.k_b = false));

	// Actualise la case d'arrivée
	_array[row2][col2] = p;

	// Promotion (en dame seulement pour le moment)
	(p == w_pawn && row2 == 7) && (_array[row2][col2] = w_queen);
	(p == b_pawn && row2 == 0) && (_array[row2][col2] = b_queen);

	// Vide la case de départ
	_array[row1][col1] = none;

	// Change le trait du joueur
	_player = !_player;

	// Imcémentation des coups
	_player && _moves_count++;

	// Reset le nombre de coups possibles
	_got_moves = -1;

	// Les coups ne sont plus triés
	_sorted_moves = false;

	// Il faut regarder de nouveau les fins de partie
	_game_over_checked = false;

	reset_eval();

	return;
}

// Fonction qui joue le coup i
void Board::make_index_move(const int i, const bool pgn, const bool new_board, const bool add_to_history) {
	make_move(_moves[i], pgn, new_board, add_to_history);
}

// Fonction qui renvoie l'avancement de la partie (0 = début de partie, 1 = fin de partie)
void Board::game_advancement() {
	if (_advancement)
		return;

	_adv = 0;

	// Définition personnelle de l'avancement d'une partie : (p_tot - p) / p_tot, où p_tot = le total matériel (du joueur adverse? ou les deux?) en début de partie, et p = le total matériel (du joueur adverse? ou les deux?) actuellement
	static constexpr int adv_pawn = 2;
	static constexpr int adv_knight = 10;
	static constexpr int adv_bishop = 10;
	static constexpr int adv_rook = 10;
	static constexpr int adv_queen = 50;
	static constexpr int adv_castle = 5;

	// Valeur à partir de laquelle on peut considérer que c'est la fin de la partie
	static constexpr int endgame_adv = 35;

	static constexpr int p_tot = 2 * (8 * adv_pawn + 2 * adv_knight + 2 * adv_bishop + 2 * adv_rook + 1 * adv_queen + 2 * adv_castle);
	int p = 0;

	static constexpr int values[6] = { 0, adv_pawn, adv_knight, adv_bishop, adv_rook, adv_queen };

	// Pièces
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			const uint_fast8_t piece = _array[i][j];
			piece && (p += values[piece % 6]);
		}
	}

	// Roques
	p += (_castling_rights.k_w + _castling_rights.q_w + _castling_rights.k_b + _castling_rights.q_b) * adv_castle;

	_adv = min(1.0f, static_cast<float>(p_tot - p) / (p_tot - endgame_adv));

	return;
}

// Fonction qui compte le matériel sur l'échiquier et renvoie sa valeur
int Board::count_material(const Evaluator* eval, float closed_factor) const
{
	int material_count = 0;

	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			if (const uint_fast8_t piece = _array[i][j]) {
				const int piece_number = (piece - 1) % 6;
				const float closed_mult = closed_factor * eval->_pieces_value_closed[piece_number] + (1.0f - closed_factor);
				const int value = static_cast<int>((static_cast<float>(eval->_pieces_value_begin[piece_number]) * (1.0f - _adv) + static_cast<float>(eval->_pieces_value_end[piece_number]) * _adv) * closed_mult);
				material_count += (piece < 7) ? value : -value;
			}
		}
	}

	return material_count;
}

// Fonction qui compte les paires de fous et renvoie la valeur
int Board::count_bishop_pairs() const
{
	uint_fast8_t bishop_w = 0; uint_fast8_t bishop_b = 0;

	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			const uint_fast8_t p = _array[i][j];
			(p == 3) && bishop_w++;
			(p == 9) && bishop_b++;
		}
	}

	return (bishop_w >= 2) - (bishop_b >= 2);
}

// Fonction qui calcule et renvoie la valeur de positionnement des pièces sur l'échiquier
int Board::pieces_positioning(const Evaluator* eval) const
{
	int pos = 0;

	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			if (const uint_fast8_t piece = _array[i][j]) {
				const int value = static_cast<int>(static_cast<float>(eval->_pieces_pos_begin[(piece - 1) % 6][(piece < 7) ? 7 - i : i][j]) * (1.0f - _adv) + static_cast<float>(eval->_pieces_pos_end[(piece - 1) % 6][(piece < 7) ? 7 - i : i][j]) * _adv);
				pos += (piece < 7) ? value : -value;
			}
		}
	}

	return pos;
}

// Fonction qui évalue la position à l'aide d'heuristiques
bool Board::evaluate(Evaluator* eval, const bool display, Network* n, bool check_game_over)
{
	/*if (_evaluated)
		return false;*/

	// Si c'est déjà évalué par la table de transposition
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




	if (check_game_over) {
		//is_game_over();

		// Nulle
		if (_game_over_value == draw) {
			_evaluation = 0;
			_static_evaluation = 0;
			_evaluated = true;

			if (display)
				main_GUI._eval_components = "DRAW\n";

			get_WDL();

			return true;
		}

		// Mat
		if (_game_over_value != unterminated) {
			_evaluation = (-mate_value + _moves_count * mate_ply) * get_color();
			_static_evaluation = _evaluation;
			_evaluated = true;

			if (display)
				main_GUI._eval_components = "CHECKMATE\n";

			get_WDL();

			return true;
		}
	}

	// Si on a un réseau de neurones
	if (n != nullptr) {
		n->input_from_fen(to_fen());
		n->calculate_output();
		//_evaluation = n->_output;
		_evaluation = n->output_eval(mate_value);

		// L'évaluation a été effectuée
		_evaluated = true;

		// Met à jour l'évaluation statique
		_static_evaluation = _evaluation;

		// Partie non finie
		return false;
	}

	_displayed_components = display;
	if (display)
		main_GUI._eval_components = "";

	// Reset l'évaluation
	_evaluation = 0;

	// Avancement de la partie
	game_advancement();
	if (display)
		main_GUI._eval_components += "ADVANCEMENT: " + to_string(static_cast<int>(round(100 * _adv))) + "%\n";

	// Nature de la position (ouverte/fermée)
	const float position_nature = get_position_nature();
	if (display)
		main_GUI._eval_components += "CLOSED: " + to_string(static_cast<int>(position_nature * 100.0f)) + "%\n";

	// *** MATERIEL ***

	if (display)
		main_GUI._eval_components += "\nMATERIAL\n";

	int total_material = 0;

	// Matériel
	if (eval->_piece_value != 0.0f) {
		const int material = count_material(eval, position_nature) * eval->_piece_value;
		if (display)
			main_GUI._eval_components += "material: " + (material >= 0 ? string("+") : string()) + to_string(material) + "\n";
		total_material += material;
	}	

	// Paire de oufs
	if (eval->_bishop_pair != 0.0f) {
		const int bishop_pair = count_bishop_pairs() * eval->_bishop_pair * (1 - position_nature);
		if (display)
			main_GUI._eval_components += "bishop pair: " + (bishop_pair >= 0 ? string("+") : string()) + to_string(bishop_pair) + "\n";
		total_material += bishop_pair;
	}

	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_material >= 0 ? string("+") : string()) + to_string(total_material) + " ---\n";

	_evaluation += total_material;

	// *** POSITIONNEMENT ***

	if (display)
		main_GUI._eval_components += "\nPOSITIONING\n";

	int total_positioning = 0;

	// Positionnement des pièces
	if (eval->_piece_positioning != 0.0f) {
		const int positioning = pieces_positioning(eval) * eval->_piece_positioning;
		if (display)
			main_GUI._eval_components += "piece positioning: " + (positioning >= 0 ? string("+") : string()) + to_string(positioning) + "\n";
		total_positioning += positioning;
	}

	// Tours sur les colonnes ouvertes / semi-ouvertes
	if (eval->_open_files != 0.0f) {
		const int rook_open = get_sliders_on_open_file() * eval->_open_files;
		if (display)
			main_GUI._eval_components += "sliders on open/semi files: " + (rook_open >= 0 ? string("+") : string()) + to_string(rook_open) + "\n";
		total_positioning += rook_open;
	}

	// Fous en fianchetto
	if (eval->_fianchetto != 0.0f) {
		const int fianchetto = get_fianchetto_value() * eval->_fianchetto * (1.0f - position_nature);
		if (display)
			main_GUI._eval_components += "fianchetto bishops: " + (fianchetto >= 0 ? string("+") : string()) + to_string(fianchetto) + "\n";
		total_positioning += fianchetto;
	}

	// Alignement des pièces (fou-tour/dame-roi)
	if (eval->_alignments != 0.0f)
	{
		const int pieces_alignment = get_alignments() * eval->_alignments;
		if (display)
			main_GUI._eval_components += "pieces alignment: " + (pieces_alignment >= 0 ? string("+") : string()) + to_string(pieces_alignment) + "\n";
		total_positioning += pieces_alignment;
	}

	// Pièces enfermées
	if (eval->_trapped_pieces != 0.0f) {
		const int trapped_pieces = get_trapped_pieces() * eval->_trapped_pieces;
		if (display)
			main_GUI._eval_components += "trapped pieces: " + (trapped_pieces >= 0 ? string("+") : string()) + to_string(trapped_pieces) + "\n";
		total_positioning += trapped_pieces;
	}

	// Menace de poussée de pion sur une pièce adverse
	if (eval->_pawn_push_threats != 0.0f) {
		const int pawn_push_threat = get_pawn_push_threats() * eval->_pawn_push_threats;
		if (display)
			main_GUI._eval_components += "pawn push threats: " + (pawn_push_threat >= 0 ? string("+") : string()) + to_string(pawn_push_threat) + "\n";
		total_positioning += pawn_push_threat;
	}

	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_positioning >= 0 ? string("+") : string()) + to_string(total_positioning) + " ---\n";

	_evaluation += total_positioning;


	// *** ACTIVITE ***

	if (display)
		main_GUI._eval_components += "\nACTIVITY\n";

	int total_activity = 0;

	// Mobilité des pièces
	//if (eval->_piece_mobility) {
	//	const int piece_mobility = get_piece_mobility() * eval->_piece_mobility;
	//	if (display)
	//		main_GUI._eval_components += "piece mobility: " + (piece_mobility >= 0 ? string("+") : string()) + to_string(piece_mobility) + "\n";
	//	total_activity += piece_mobility;
	//}

	// Mobilité long-terme des pièces
	if (eval->_long_term_piece_mobility != 0.0f) {
		const int long_term_mobility = get_long_term_piece_mobility() * eval->_long_term_piece_mobility;
		if (display)
			main_GUI._eval_components += "long-term piece mobility: " + (long_term_mobility >= 0 ? string("+") : string()) + to_string(long_term_mobility) + "\n";
		total_activity += long_term_mobility;
	}

	// Mobilité court-terme des pièces
	if (eval->_short_term_piece_mobility != 0.0f) {
		const int short_term_mobility = get_short_term_piece_mobility() * eval->_short_term_piece_mobility;
		if (display)
			main_GUI._eval_components += "short-term piece mobility: " + (short_term_mobility >= 0 ? string("+") : string()) + to_string(short_term_mobility) + "\n";
		total_activity += short_term_mobility;
	}

	// Activité des pièces
	if (eval->_piece_activity != 0.0f) {
		const int piece_activity = get_piece_activity() * eval->_piece_activity;
		if (display)
			main_GUI._eval_components += "piece activity: " + (piece_activity >= 0 ? string("+") : string()) + to_string(piece_activity) + "\n";
		total_activity += piece_activity;
	}

	// Activité des cavaliers
	if (eval->_knight_activity != 0.0f) {
		const int knight_activity = get_knight_activity() * eval->_knight_activity;
		if (display)
			main_GUI._eval_components += "knight activity: " + (knight_activity >= 0 ? string("+") : string()) + to_string(knight_activity) + "\n";
		total_activity += knight_activity;
	}

	// Activité des fous
	if (eval->_bishop_activity != 0.0f) {
		const int bishop_activity = get_bishop_activity() * eval->_bishop_activity;
		if (display)
			main_GUI._eval_components += "bishop activity: " + (bishop_activity >= 0 ? string("+") : string()) + to_string(bishop_activity) + "\n";
		total_activity += bishop_activity;
	}

	// Activité des tours
	if (eval->_rook_activity != 0.0f) {
		const int rook_activity = get_rook_activity() * eval->_rook_activity;
		if (display)
			main_GUI._eval_components += "rook activity: " + (rook_activity >= 0 ? string("+") : string()) + to_string(rook_activity) + "\n";
		total_activity += rook_activity;
	}

	// Attaques et défenses de pièces
	if (eval->_attacks != 0.0f) {
		const int pieces_attacks_and_defenses = get_attacks_and_defenses() * eval->_attacks;
		if (display)
			main_GUI._eval_components += "attacks/defenses: " + (pieces_attacks_and_defenses >= 0 ? string("+") : string()) + to_string(pieces_attacks_and_defenses) + "\n";
		total_activity += pieces_attacks_and_defenses;
	}

	// Trait du joueur
	if (eval->_player_trait != 0.0f) {
		const int player_trait = eval->_player_trait * get_color() * (1 - position_nature);
		if (display)
			main_GUI._eval_components += "player trait: " + (player_trait >= 0 ? string("+") : string()) + to_string(player_trait) + "\n";
		total_activity += player_trait;
	}

	//if (display)
	//	main_GUI._eval_components += "SUB-TOTAL: " + (total_activity >= 0 ? string("+") : string()) + to_string(total_activity) + "\n";

	//// Ajustement en fonction de la nature de la position
	//if (display)
	//	main_GUI._eval_components += "position nature: x" + to_string((int)(100 - 100 * position_nature)) + "%\n";
	//total_activity *= 1 - position_nature;


	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_activity >= 0 ? string("+") : string()) + to_string(total_activity) + " ---\n";

	_evaluation += total_activity;


	// *** STRUCTURE DE PIONS ***

	if (display)
		main_GUI._eval_components += "\nPAWN STRUCTURE\n";

	int total_pawn_structure = 0;

	// Contrôle des cases
	if (eval->_square_controls != 0.0f) {
		const int square_controls = get_square_controls() * eval->_square_controls;
		if (display)
			main_GUI._eval_components += "square controls: " + (square_controls >= 0 ? string("+") : string()) + to_string(square_controls) + "\n";
		total_pawn_structure += square_controls;
	}

	// Avantage d'espace
	if (eval->_space_advantage != 0.0f)
	{
		const int space = get_space() * eval->_space_advantage * position_nature;
		if (display)
			main_GUI._eval_components += "space: " + (space >= 0 ? string("+") : string()) + to_string(space) + "\n";
		total_pawn_structure += space;
	}

	// Structure de pions
	if (eval->_pawn_structure != 0.0f) {
		const int pawn_structure = get_pawn_structure(display * eval->_pawn_structure) * eval->_pawn_structure;
		//if (display)
		//	main_GUI._eval_components += "pawn structure: " + (pawn_structure >= 0 ? string("+") : string()) + to_string(pawn_structure) + "\n";
		total_pawn_structure += pawn_structure;
	}

	// Bons/Mauvais fous
	if (eval->_bishop_pawns != 0.0f) {
		const int bishop_pawns = get_bishop_pawns() * eval->_bishop_pawns;
		if (display)
			main_GUI._eval_components += "bishop pawns: " + (bishop_pawns >= 0 ? string("+") : string()) + to_string(bishop_pawns) + "\n";
		total_pawn_structure += bishop_pawns;
	}

	// Cases faibles et avant-postes
	if (eval->_weak_squares != 0.0f) {
		const int weak_squares = (-get_weak_squares(true) + get_weak_squares(false)) * eval->_weak_squares * position_nature;
		if (display)
			main_GUI._eval_components += "weak squares: " + (weak_squares >= 0 ? string("+") : string()) + to_string(weak_squares) + "\n";
		total_pawn_structure += weak_squares;
	}

	// Evaluation avec toutes ses composantes
	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_pawn_structure >= 0 ? string("+") : string()) + to_string(total_pawn_structure) + " ---\n";

	_evaluation += total_pawn_structure;


	// *** ROI ***

	if (display)
		main_GUI._eval_components += "\nKING\n";

	int total_king = 0;

	// Sécurité du roi
	if (eval->_king_safety != 0.0f) {
		const int king_safety = get_king_safety(display * eval->_king_safety) * eval->_king_safety;
		if (display)
			main_GUI._eval_components += "king safety: " + (king_safety >= 0 ? string("+") : string()) + to_string(king_safety) + "\n";
		total_king += king_safety;
	}

	// Droits de roques
	if (eval->_castling_rights != 0.0f) {
		const int castling_rights = eval->_castling_rights * (_castling_rights.k_w + _castling_rights.q_w - _castling_rights.k_b - _castling_rights.q_b) * (1 - _adv);
		if (display)
			main_GUI._eval_components += "castling rights: " + (castling_rights >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(castling_rights))) + "\n";
		total_king += castling_rights;
	}

	// Distance au roque
	//if (eval->_castling_distance != 0.0f) {
	//	const int castling_distance = get_castling_distance() * eval->_castling_distance;
	//	if (display)
	//		main_GUI._eval_components += "castling distance: " + (castling_distance >= 0 ? string("+") : string()) + to_string(castling_distance) + "\n";
	//	total_king += castling_distance;
	//}

	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_king >= 0 ? string("+") : string()) + to_string(total_king) + " ---\n";

	_evaluation += total_king;


	// *** FINALES ***

	if (display)
		main_GUI._eval_components += "\nENDGAME\n";

	int total_endgame = 0;

	// Opposition des rois
	if (eval->_kings_opposition != 0.0f) {
		const int kings_opposition = get_kings_opposition() * eval->_kings_opposition;
		if (display)
			main_GUI._eval_components += "king opposition: " + (kings_opposition >= 0 ? string("+") : string()) + to_string(kings_opposition) + "\n";
		total_endgame += kings_opposition;
	}

	// Proximité du roi avec les pions en finale
	if (eval->_king_proximity != 0.0f) {
		const int king_proximity = get_king_proximity() * eval->_king_proximity;
		if (display)
			main_GUI._eval_components += "king proximity: " + (king_proximity >= 0 ? string("+") : string()) + to_string(king_proximity) + "\n";
		total_endgame += king_proximity;
	}

	// Centralisation du roi
	if (eval->_king_centralization != 0.0f) {
		const int king_centralization = (get_king_centralization(true) - get_king_centralization(false)) * eval->_king_centralization;
		if (display)
			main_GUI._eval_components += "king centralization: " + (king_centralization >= 0 ? string("+") : string()) + to_string(king_centralization) + "\n";
		total_endgame += king_centralization;
	}

	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_endgame >= 0 ? string("+") : string()) + to_string(total_endgame) + " ---\n";

	_evaluation += total_endgame;

	// *** NATURE DE LA POSITION ***

	if (display)
		main_GUI._eval_components += "\nPOSITION NATURE\n";

	int total_nature = 0;

	// Forteresse
	if (eval->_push != 0.0f) {
		const float push = 1 - static_cast<float>(_half_moves_count) * eval->_push / max_half_moves;
		const int fortress = 100.0f - push * 100.0f;
		const int fortress_value = _evaluation * (push - 1);
		if (display)
			main_GUI._eval_components += "fortress: " + to_string(fortress) + "% (" + (fortress_value >= 0 ? string("+") : string()) + to_string(fortress_value) + ")\n";
		total_nature += fortress_value;
	}

	// Winnable
	if (display)
		main_GUI._eval_components += "winnable: TODO\n";

	// Incertitude de l'évaluation
	const float uncertainty = get_uncertainty(total_material);
	const int uncertainity_percent = (int)(100 * uncertainty);
	if (display)
		main_GUI._eval_components += "uncertainity: " + to_string(uncertainity_percent) + "%\n";


	if (display)
		main_GUI._eval_components += "--- TOTAL: " + (total_nature >= 0 ? string("+") : string()) + to_string(total_nature) + " ---\n";

	_evaluation += total_nature;


	// *** TOTAL ***
	if (display) {
		main_GUI._eval_components += "\nTOTAL COMPONENTS\n";
		main_GUI._eval_components += "Material: " + (total_material >= 0 ? string("+") : string()) + to_string(total_material) + "\n";
		main_GUI._eval_components += "Positioning: " + (total_positioning >= 0 ? string("+") : string()) + to_string(total_positioning) + "\n";
		main_GUI._eval_components += "Activity: " + (total_activity >= 0 ? string("+") : string()) + to_string(total_activity) + "\n";
		main_GUI._eval_components += "Pawn structure: " + (total_pawn_structure >= 0 ? string("+") : string()) + to_string(total_pawn_structure) + "\n";
		main_GUI._eval_components += "King: " + (total_king >= 0 ? string("+") : string()) + to_string(total_king) + "\n";
		main_GUI._eval_components += "Endgame: " + (total_endgame >= 0 ? string("+") : string()) + to_string(total_endgame) + "\n";
		main_GUI._eval_components += "Nature: " + (total_nature >= 0 ? string("+") : string()) + to_string(total_nature) + "\n";

		main_GUI._eval_components += "_______________\nTOTAL: " + (_evaluation >= 0 ? string("+") : string()) + to_string(_evaluation) + "\n";

	}

	// Chances de gain
	//const float win_chance = get_winning_chances_from_eval(_evaluation, true);
	//if (display)
	//	main_GUI._eval_components += "W/D/L: " + to_string(static_cast<int>(100 * win_chance)) + "/" + to_string(static_cast<int>(100 * 0)) + "/" + to_string(static_cast<int>(100 * (1.0f - win_chance))) + "%\n";

	get_WDL(uncertainty);
	if (display) {
		main_GUI._eval_components += "Confidence: " + to_string(100 - uncertainity_percent) + "%\n";
		main_GUI._eval_components += _wdl.to_string() + "\n";
		main_GUI._eval_components += "Score: " + score_string(get_average_score()) + "\n";
	}


	// L'évaluation a été effectuée
	_evaluated = true;

	// Met à jour l'évaluation statique
	_static_evaluation = _evaluation;


	// EXPERIMENTAL
	//Node node(this);
	//transposition_table._hash_table[_zobrist_key] = &node;


	// Partie non finie
	return false;
}

// Fonction qui joue le coup d'une position, renvoyant la meilleure évaluation à l'aide d'un negamax (similaire à un minimax)
//int Board::negamax(const int depth, int alpha, const int beta, const bool max_depth, Evaluator* eval, const bool play, const bool display, const int quiescence_depth, const int null_depth) 
//{
//	// Nombre de noeuds
//	if (max_depth) {
//		main_GUI._visited_nodes = 1;
//		main_GUI._begin_time = clock();
//	}
//	else {
//		main_GUI._visited_nodes++;
//	}
//
//	// Vérifie si la position est terminée
//	is_game_over();
//
//	if (_game_over_value == 2)
//		return 0;
//	if (_game_over_value != 0)
//		return -mate_value + _moves_count * mate_ply;
//
//	// Evaluation de la position via quiescence
//	if (depth <= 0)
//		return quiescence(eval, -INT_MAX, INT_MAX, quiescence_depth, false);
//
//
//	// Null move pruning
//	/*if (null_depth > 0 && depth > 1 && !in_check())
//	{
//		_player = !_player;
//		int null_move_value = -negamax(depth - 1 - null_depth, -beta, -beta + 1, false, eval, false, false, quiescence_depth, 0);
//		_player = !_player;
//
//		if (null_move_value >= beta)
//			return null_move_value;
//	}*/
//
//
//	int value = -1e9;
//	Board b;
//
//	int best_move = 0;
//
//	if (depth > 1)
//		sort_moves();
//		
//	if (max_depth)
//		display_moves();
//		
//
//	for (int i = 0; i < _got_moves; i++) {
//		b.copy_data(*this, false, true);
//		b.make_index_move(i);
//
//		int tmp_value = -b.negamax(depth - 1, -beta, -alpha, false, eval, false, false, quiescence_depth);
//		// threads.emplace_back(std::thread([&]() {
//		//     tmp_value = -b.negamax(depth - 1, -beta, -alpha, -color, false, eval, a, use_agent);
//		// })); // Test de OpenAI
//
//		if (max_depth) {
//			if (display)
//				cout << "move : " << move_label_from_index(i) << ", value : " << tmp_value << endl;
//			if (tmp_value > value)
//				best_move = i;
//		}
//
//		value = max(value, tmp_value);
//		alpha = max(alpha, value);
//		// undo move
//		// b.undo();
//
//		if (alpha >= beta)
//			break;
//	}
//
//	// Attendre la fin des threads
//	// for (auto &thread : threads) {
//	//     thread.join();
//	// } // Test de OpenAI
//
//	if (max_depth) {
//		if (display) {
//			cout << "visited nodes : " << main_GUI._visited_nodes / 1000 << "k" << endl;
//			const auto spent_time = static_cast<double>(clock() - main_GUI._begin_time);
//			cout << "time spend : " << spent_time << "ms" << endl;
//			cout << "speed : " << main_GUI._visited_nodes / spent_time << "kN/s" << endl;
//		}
//		if (play) {
//			//play_index_move_sound(best_move);
//			if (display)
//				if (_tested_moves > 0)
//					((main_GUI._click_bind && main_GUI._board.click_m_move(main_GUI._board._moves[main_GUI._board.best_monte_carlo_move()], main_GUI.get_board_orientation())) || true) && play_monte_carlo_move_keep(_moves[best_move], true, true, true);
//				else
//					make_index_move(best_move, true);
//		}
//
//		return value;
//	}
//
//	return value;
//}

// Grogrosfish
//bool Board::grogrosfish(const int depth, Evaluator* eval, const bool display = false) {
//	negamax(depth, -1e9, 1e9, true, eval, true, display);
//	if (display) {
//		evaluate(eval);
//		cout << main_GUI._current_fen << endl;
//		cout << main_GUI._global_pgn << endl;
//	}
//
//	return true;
//}

// Fonction qui récupère le plateau d'un FEN
// TODO : à refaire
void Board::from_fen(string fen)
{
	string pgn;
	//reset_all();
	reset_board();

	// PGN
	main_GUI._initial_fen = fen;
	main_GUI._pgn = "";

	// Iterateur qui permet de parcourir la chaine de caractères
	int iterator = 0;

	// Position à itérer dans le plateau
	int i = 7;
	int j = 0;

	char c;

	// Positionnement des pièces
	while (i >= 0) {
		c = fen[iterator];
		switch (c) {
		case '/': case ' ': i -= 1; j = 0; break;
		case 'P': _array[i][j] = 1; j += 1; break;
		case 'N': _array[i][j] = 2; j += 1; break;
		case 'B': _array[i][j] = 3; j += 1; break;
		case 'R': _array[i][j] = 4; j += 1; break;
		case 'Q': _array[i][j] = 5; j += 1; break;
		case 'K': _array[i][j] = 6; j += 1; break;
		case 'p': _array[i][j] = 7; j += 1; break;
		case 'n': _array[i][j] = 8; j += 1; break;
		case 'b': _array[i][j] = 9; j += 1; break;
		case 'r': _array[i][j] = 10; j += 1; break;
		case 'q': _array[i][j] = 11; j += 1; break;
		case 'k': _array[i][j] = 12; j += 1; break;
		default:
			if (isdigit(c)) {
				const int digit = (static_cast<int>(c)) - (static_cast<int>('0'));
				for (int k = j; k < j + digit; k++) {
					_array[i][k] = 0;
				}
				j += digit;
				break;
			}

			else {
				cout << "invalid FEN" << endl;
				return;
			}
		}

		iterator += 1;
	}

	// A qui de jouer
	c = fen[iterator];

	if (c == 'w') {
		_player = true;
	}
	else {
		_player = false;
	}

	iterator += 2;

	bool next = true;

	// Roques
	_castling_rights.k_w = false; _castling_rights.q_w = false; _castling_rights.k_b = false; _castling_rights.q_b = false;

	while (next) {
		c = fen[iterator];

		switch (c) {
		case '-': iterator += 1; next = false; break;
		case 'K': _castling_rights.k_w = true; break;
		case 'Q': _castling_rights.q_w = true; break;
		case 'k': _castling_rights.k_b = true; break;
		case 'q': _castling_rights.q_b = true; iterator += 1; next = false; break;
		default: next = false; break;
		}

		iterator += 1;
	}

	c = fen[iterator];

	// En passant
	if (c == '-')
		_en_passant_col = -1;
	else {
		_en_passant_col = fen[iterator] - 'a';
		iterator += 1;
	}

	iterator += 2;
	string s;
	while (fen[iterator] != ' ') {
		s += fen[iterator];
		iterator += 1;
	}
	_half_moves_count = stoi(s);

	iterator += 1;
	fen += ' ';
	s = "";
	while (fen[iterator] != ' ') {
		s += fen[iterator];
		iterator += 1;
	}
	_moves_count = stoi(s);

	_got_moves = -1;

	//_new_board = true;
	//_quiescence_nodes = 0;
	//_nodes = 0;
	//_transpositions = 0;

	reset_eval();

	// Oriente le plateau dans pour le joueur qui joue
	main_GUI._board_orientation = _player;

	// Met à jour le FEN de la position dans la GUI
	main_GUI._initial_fen = fen;

	main_GUI._game_tree.new_tree(*this);
}

// Fonction qui renvoie le FEN du plateau
string Board::to_fen() const
{
	string s;
	int it = 0;

	for (int i = 7; i >= 0; i--) {
		for (int j = 0; j < 8; j++) {
			if (const int p = _array[i][j]; p == 0)
				it += 1;
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

// Fonction qui renvoie le gagnant si la partie est finie (-1/1, et 2 pour nulle), et 0 sinon
// Génère également les coups légaux, s'il y en a
int Board::game_over(int max_repetitions) {

	// Ne pas recalculer si déjà fait
	if (_game_over_checked)
		return _game_over_value;
		
	// Pour ne pas le recalculer
	_game_over_checked = true;

	// Règle des 50 coups
	if (_half_moves_count >= max_half_moves)
		return 2;

	// Règle des 3 répétitions
	//if (repetition_count() >= main_GUI._max_repetition)
	if (repetition_count() >= max_repetitions)
		return 2;

	// Calcule les coups légaux
	if (_got_moves == -1)
		get_moves(true);

	// S'il n'y a pas de coups légaux, c'est soit mat, soit pat
	if (_got_moves == 0) {
		
		// Mat
		if (in_check())
			return _player ? -1 : 1;

		// Pat
		return 2;
	}

	// Matériel insuffisant
	uint_fast8_t count_w_knight = 0;
	uint_fast8_t count_w_bishop = 0;
	uint_fast8_t count_b_knight = 0;
	uint_fast8_t count_b_bishop = 0;

	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			const uint_fast8_t p = _array[i][j];
			if (p == 2)
				count_w_knight++;
			else if (p == 3)
				count_w_bishop++;
			else if (p == 8)
				count_b_knight++;
			else if (p == 9)
				count_b_bishop++;
			// Pièces majeures ou pion -> possibilité de mater
			else if (p != 6 && p != 12 && p != 0)
				return 0;

			// Si on a au moins 1 fou, et un cheval/fou ou plus -> plus de nulle par manque de matériel
			if ((count_w_bishop > 0) && (count_w_knight > 0 || count_w_bishop > 1))
				return 0;
		}
	}

	// Possibilités de nulles par manque de matériel
	if (count_w_knight + count_w_bishop < 2 && count_b_knight + count_b_bishop < 2)
		return 2;

	// On ne peut pas mater avec seulement 2 cavaliers
	// TODO: est-ce que la partie est déclarée nulle?
	/*if (count_w_knight == 2 || count_b_knight == 2) {
		_game_over_value = 2;
		return 2;
	}*/

	return 0;
}

// Fonction qui renvoie le gagnant si la partie est finie (-1/1, et 2 pour nulle), et 0 sinon
int Board::is_game_over(int max_repetitions) {
	_game_over_value = game_over(max_repetitions);
	return _game_over_value;
}

// Fonction qui renvoie le label d'un coup
// En passant manquant... échecs aussi, puis roques, promotions, mats/pats
string Board::move_label(Move move, bool use_uft8)
{
	const uint_fast8_t i = move.start_row;
	const uint_fast8_t j = move.start_col;
	const uint_fast8_t k = move.end_row;
	const uint_fast8_t l = move.end_col;

	const uint_fast8_t p1 = _array[i][j]; // Pièce qui bouge
	const uint_fast8_t p2 = _array[k][l];

	// Pour savoir si une autre pièce similaire peut aller sur la même case
	bool spec_col = false;
	bool spec_line = false;
	if (_got_moves == -1)
		get_moves(true);

	uint_fast8_t i1; uint_fast8_t j1; uint_fast8_t k1; uint_fast8_t l1; uint_fast8_t p11;
	for (int m = 0; m < _got_moves; m++) {
		i1 = _moves[m].start_row;
		j1 = _moves[m].start_col;
		k1 = _moves[m].end_row;
		l1 = _moves[m].end_col;
		p11 = _array[i1][j1];
		// Si c'est une pièce différente que celle à bouger, mais du même type, et peut aller sur la même case
		if ((i1 != i || j1 != j) && p11 == p1 && k1 == k && l1 == l) {
			if (i1 != i)
				spec_col = true;
			if (j1 != j)
				spec_line = true;
		}
	}

	string s;

	switch (p1)
	{
	case 2: case 8: s += use_uft8 ? (_player ? main_GUI.N_symbol : main_GUI.n_symbol) : "N"; if (spec_line) s += main_GUI._abc8[j]; if (spec_col) s += static_cast<char>(i + 1 + 48); break;
	case 3: case 9: s += use_uft8 ? (_player ? main_GUI.B_symbol : main_GUI.b_symbol) : "B"; if (spec_line) s += main_GUI._abc8[j]; if (spec_col) s += static_cast<char>(i + 1 + 48); break;
	case 4: case 10: s += use_uft8 ? (_player ? main_GUI.R_symbol : main_GUI.r_symbol) : "R"; if (spec_line) s += main_GUI._abc8[j]; if (spec_col) s += static_cast<char>(i + 1 + 48); break;
	case 5: case 11: s += use_uft8 ? (_player ? main_GUI.Q_symbol : main_GUI.q_symbol) : "Q"; if (spec_line) s += main_GUI._abc8[j]; if (spec_col) s += static_cast<char>(i + 1 + 48); break;
	case 6: case 12:
		if (l - j == 2) {
			s += "O-O"; return s;
		}
		if (j - l == 2) {
			s += "O-O-O"; return s;
		}
		s += use_uft8 ? (_player ? main_GUI.K_symbol : main_GUI.k_symbol) : "K"; break;
	}

	if (p2 || ((p1 == 1 || p1 == 7) && j != l)) {
		if (p1 == 1 || p1 == 7)
			s += main_GUI._abc8[j];
		s += "x";
		s += main_GUI._abc8[l];
		s += static_cast<char>(k + 1 + 48);
	}

	else {
		s += main_GUI._abc8[l];
		s += static_cast<char>(k + 1 + 48);
	}

	// Promotion (seulement en dame pour le moment)
	if ((p1 == 1 && k == 7) || (p1 == 7 && k == 0)) {
		s += "=";
		s += use_uft8 ? (_player ? main_GUI.Q_symbol : main_GUI.q_symbol) : "Q";
	}

	// Mats, pats, échecs...
	Board b(*this);
	b.make_move(move);

	// Le coup termine-t-il la partie?
	b.is_game_over();

	// En cas de mat pour les blancs
	if (b._game_over_value == 1)
		return s + "# 1-0";

	// En cas de mat pour les noirs
	else if (b._game_over_value == -1)
		return s + "# 0-1";

	// En cas d'égalité
	else if (b._game_over_value == 2)
		return s + (b._got_moves == 0 ? "@ 1/2-1/2" : " 1/2-1/2"); // Pat -> "@"

	// Échec
	if (b.in_check())
		s += "+";

	return s;
}

// Fonction qui affiche un texte dans une zone donnée
void Board::draw_text_rect(const string& s, const float pos_x, const float pos_y, const float width, const float height, const float size) {
	// Division du texte
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

// Fonction qui joue le son d'un coup
void Board::play_move_sound(Move move) const
{
	const uint_fast8_t i = move.start_row;
	const uint_fast8_t j = move.start_col;
	const uint_fast8_t k = move.end_row;
	const uint_fast8_t l = move.end_col;

	// Pièces
	const uint_fast8_t p1 = _array[i][j];
	const uint_fast8_t p2 = _array[k][l];

	// Echecs
	Board b(*this);
	b.make_move(move);

	const int mate = b.is_game_over();

	if (mate == 2)
		return PlaySound(main_GUI._stealmate_sound);
	if (mate != 0)
		return PlaySound(main_GUI._checkmate_sound);

	if (b.in_check()) {
		if (_player)
			PlaySound(main_GUI._check_1_sound);
		else
			PlaySound(main_GUI._check_2_sound);
	}

	// Si pas d'échecs
	else {
		// Promotions
		if ((p1 == 1 || p1 == 7) && (k == 0 || k == 7))
			return PlaySound(main_GUI._promotion_sound);

		// Prises (ou en passant)
		if (p2 != 0 || ((p1 == 1 || p1 == 7) && j != l)) {
			if (_player)
				return PlaySound(main_GUI._capture_1_sound);
			else
				return PlaySound(main_GUI._capture_2_sound);
		}

		// Roques
		if (p1 == 6 && abs(j - l) == 2)
			return PlaySound(main_GUI._castle_1_sound);
		if (p1 == 12 && abs(j - l) == 2)
			return PlaySound(main_GUI._castle_2_sound);

		// Coup "normal"
		if (_player)
			return PlaySound(main_GUI._move_1_sound);
		if (!_player)
			return PlaySound(main_GUI._move_2_sound);
	}

	// Mats à rajouter

	return;
}

// Fonction qui réinitialise le plateau dans son état de base (pour le buffer)
// FIXME? plus rapide d'instancier un nouveau plateau? et plus safe niveau mémoire?
void Board::reset_board(const bool display) {
	_got_moves = -1;
	_is_active = false;
	_evaluated = false;
	_game_over_checked = false;
	_static_evaluation = 0;
	_evaluation = 0;
	_sorted_moves = false;
	_zobrist_key = 0;

	reset_positions_history();

	if (display)
		cout << "board reset done" << endl;

	return;
}

// Fonction qui calcule et renvoie la valeur correspondante à la sécurité des rois
int Board::get_king_safety(float display_factor) {

	// ----------------------
	// *** POSITIONS TEST ***
	// ----------------------

	// 4rb1r/pp3kpp/2p1b3/3nB3/2BP4/P7/1PP2PPP/4RRK1 w - - 0 18
	// 4r3/p3bkp1/r7/1pPpBP1p/1P1P4/P2b2P1/5R1P/4R1K1 w - - 1 28 : le roi devrait être safe
	// r1bq1b1r/ppp3pp/2n1k3/3np3/2B5/5Q2/PPPP1PPP/RNB1K2R w KQ - 2 8
	// r1b2b1r/ppp3pp/8/3kp3/8/8/PPPP1PPP/R1B1K2R w KQ - 0 12
	// 8/2p1k1pp/p1Qb4/3P3q/4p3/N1P1BnPb/P4P2/5R1K w - - 1 25
	// 5rk1/6p1/pq1b3p/3p4/2p1n3/PP3N1P/4p1P1/RQR4K w - - 2 31 : roi blanc très faible (mat)
	// 3r1rk1/pp1bbp2/1qp1pn1Q/4N3/3P4/2PB4/PP3PPP/R3R1K1 b - - 0 16
	// 2k3r1/p1b4p/2p5/3P3r/8/5bP1/PP3P2/2R2RK1 w - - 0 7
	// r1bq1rk1/ppppnpp1/8/2bNp1PQ/1nB1P3/2P5/PP1P1PP1/R1B1K2R b KQ - 2 3
	// r1bq1rk1/pp2npp1/2n1p3/2ppP1NQ/3P4/P1P5/2P2PPP/R1B1K2R b KQ - 3 3
	// r3kb1r/pR2pppp/2p5/3p4/3P2b1/B3RN2/q1P2PPP/3Q2K1 b kq - 1 14 : overload++
	// r4k1r/pRQ3pp/2p1pp2/3p1b2/3P4/R4N1P/2q2PP1/6K1 b - - 2 20 : mat imparable
	// r1b1k2r/p1p2ppp/2p5/8/5P1q/3B1R1P/PBP3P1/Q5K1 w kq - 3 17 : le roi noir est le plus faible
	// 1r4k1/p2n1pp1/2p1b2p/3p3P/4pQ2/2q1P3/P1P1BPP1/2KR3R w - - 1 23 : c'est mat pour les noirs
	// rnbr2k1/ppq2p2/2pb1npQ/6N1/7R/3B2P1/PPP2P1P/2KR4 b - - 2 17 : mat pour les blancs
	// 3rk2r/ppp2ppq/2p1b3/2P5/4P1P1/2P3P1/PPQ1B3/RNB2RK1 w k - 1 7 : quasi égal
	// 3rk2r/ppp2pp1/2p5/2P5/4P3/2P3P1/PPQN1KR1/R1B4q b k - 2 12 : Th2 puis perpet
	// 2k2r2/ppp3pp/1bp1b3/8/4Pp1q/1N1B1Pn1/PP3RPP/R2QB1K1 w - - 8 6 : roi blanc pas très safe
	// 2k2r2/ppp3pp/1bp1b3/8/4Pp1q/1N1B1Pn1/PPQ2RPP/R3B1K1 b - - 9 6 : Dxh2+!! #5
	// 2k5/ppp3pp/1bp1b2r/8/4Pp2/1N1B1Pn1/PPQ2RP1/R3B1K1 w - - 3 9 : #1 imparable
	// 8/p7/r3pk2/8/1P2Kp2/P1R2P2/5P2/8 b - - 3 39 : roi blanc pas en danger
	// 2rk3q/1pp5/p4n2/1P1p1bp1/2PQ1b2/N2p4/P2P2PP/R1B1R2K w - - 0 23 : roi blanc foutu
	// r1b1k2r/pppp2pp/2n5/4Pp2/8/BB3N2/P1PQ2PP/5K2 b kq - 0 15 : le roi est pas bien en fait
	// r1bq1b1r/pp4pp/2p1k3/3np3/1nBP4/2N2Q2/PPP2PPP/R1B2RK1 b - - 0 10 : +2.5 / +5 pour king safety
	// r3r1k1/2p2pp1/1p1p3p/pPn4q/2PN3n/P3PP1P/2Q2P1K/B2R2R1 w - - 7 6 : déjà complètement gagnant pour les blancs
	// r1bq1rk1/pp1nbpn1/2p1p3/8/2pP4/2N1PN2/PPQ2P1P/2KR1BR1 b - - 1 6 : gagnant pour les blancs -> roi noir trop faible, colonnes et diagonales ouvertes, pas de pions devant non plus. toutes les pièces peuvent attaquer (les 6), tandis que seules 4 pièces noires peuvent défendre
	// 1r1qr1k1/p2n1pn1/b1p1pb1Q/4N3/1ppPN3/4P3/PP3P1P/2KR1BR1 b - - 9 12 : foutu pour les noirs
	// r1b3kr/pppp3p/2n2Q2/8/5N2/4p3/PPP3PP/6K1 b - - 2 19 : blancs gagnants
	// r1b3kr/ppp4p/2np1Q2/7N/8/4p3/PPP3PP/6K1 b - - 1 20 : #1 imparable
	// rnb2bnr/pppp1k1p/8/8/5p2/4BQ2/PqP3PP/RN3RK1 w - - 0 11 : blancs gagnants
	// 6k1/5pp1/5r2/7K/P5PP/2Nr1n2/1P6/8 b - - 0 38 : #1 imparable...
	// r3k2r/pp1n1pp1/2n1b2p/2p1P3/5P2/P4NP1/1PPKBB1P/3R3R w kq - 0 18 : ici le roi est mieux en c1 que e3...
	// r1b3kr/pppp3p/2n2Q2/3N4/8/4p3/PPP3PP/6K1 w - - 1 19 : gagnant pour les blancs
	// r1b1r2k/pp3pp1/2n4n/3qp3/2Np4/3B1N1P/PP3PP1/RQ2R1K1 w - - 0 17 : roi noir pas tant en danger que ça... la batterie fou/dame ne sert en fait à rien, la dame n'est qu'a moitié en attaque
	// 8/1rp3p1/4k2p/8/7P/2R2KP1/5P2/8 w - - 6 58 : roi tranquille
	// 6R1/5p2/5kp1/2q5/pp4B1/2n1R3/5PKP/8 b - - 5 45 : roi noir tranquille
	// 1r6/7p/p1P1p3/4kp2/1P1Rp3/4KPP1/8/8 b - - 0 49 : pareil...
	// 2bk1r2/4b1Qp/8/1p6/P2P4/1qp5/4NPPP/R1K2B1R w - - 1 25 : gagnant pour les noirs

	// 8/6PK/5k2/8/8/8/8/8 b - - 0 8


	// Met à jour la position des rois
	update_kings_pos();

	// Nombre de colonnes qui séparent les rois (pour savoir si c'est des roques opposés par exemple)
	const int king_columns_diff = abs(_white_king_pos.col - _black_king_pos.col);


	// Faiblesses des rois
	int w_king_weakness = 0;
	int b_king_weakness = 0;

	// ---------------------------
	// *** POTENTIEL D'ATTAQUE ***
	// ---------------------------

	// Potentiel d'attaque de chaque pièce (pion, caval, fou, tour, dame)
	static constexpr int attack_potentials[6] = { 5, 30, 35, 50, 125, 0 };
	//constexpr int reference_mating_potential = 40; // Matériel minimum pour mater (en général)
	constexpr int reference_mating_potential = 0; // Matériel minimum pour mater (en général)
	constexpr int reference_potential = 395 - reference_mating_potential; // Si y'a toutes les pièces de base sur l'échiquier

	int w_total_potential = 0;
	int b_total_potential = 0;

	// Fous de chaque joueur
	bool w_bishop_w = false;
	bool w_bishop_b = false;
	bool b_bishop_w = false;
	bool b_bishop_b = false;

	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			if (const uint_fast8_t p = _array[i][j]; p > 0) {
				if (is_white(p))
					w_total_potential += attack_potentials[p - 1];
				else
					b_total_potential += attack_potentials[(p - 1) % 6];

				if (p == w_bishop) {
					if ((i + j) % 2 == 0)
						w_bishop_w = true;
					else
						w_bishop_b = true;
				}
				else if (p == b_bishop) {
					if ((i + j) % 2 == 0)
						b_bishop_w = true;
					else
						b_bishop_b = true;
				}
			}
		}
	}

	w_total_potential = max(0, w_total_potential - reference_mating_potential);
	b_total_potential = max(0, b_total_potential - reference_mating_potential);

	// S'il y a un fou de couleur opposée, on ajoute un potentiel d'attaque en fonction du potential actuel
	constexpr float opposite_bishop_potential = 1.25f;

	//cout << "bishops: " << w_bishop_w << " " << w_bishop_b << " " << b_bishop_w << " " << b_bishop_b << endl;

	if (((w_bishop_w && !w_bishop_b) && (!b_bishop_w && b_bishop_b)) || ((!w_bishop_w && w_bishop_b) && (b_bishop_w && !b_bishop_b))) {
		//cout << "opposite bishop" << endl;
		w_total_potential *= 1 + (opposite_bishop_potential - 1) * w_total_potential / reference_potential;
		b_total_potential *= 1 + (opposite_bishop_potential - 1) * b_total_potential / reference_potential;
	}

	// Constante pour garder un minimum de potentiel...
	//constexpr float min_potential = 0.1f;
	constexpr float min_potential = 0.0f;

	//r1b3k1/pp3ppp/5q2/2Pr4/4p3/1NQ1K1N1/PP2B1PP/R7 b - - 1 24
	//8/8/2p4k/2Pp4/3P1n2/8/3K4/8 w - - 0 58

	// Potentiel d'attaque
	const float w_attacking_potential = ((float)w_total_potential / reference_potential + min_potential) / (1 + min_potential);
	const float b_attacking_potential = ((float)b_total_potential / reference_potential + min_potential) / (1 + min_potential);

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "----------\n";
		main_GUI._eval_components += "Attacking potential: " + to_string(w_attacking_potential) + " / " + to_string(b_attacking_potential) + "\n";
	}

	// Facteurs multiplicatifs
	constexpr float piece_attack_factor = 1.0f;
	constexpr float piece_defense_factor = 1.0f;
	constexpr float pawn_protection_factor = 0.75f;

	// En cas de résultante positive ou négative...
	constexpr float piece_overload_multiplicator = 1.0f; // TODO: à utiliser
	constexpr float piece_defense_multiplicator = 1.0f;


	// -------------------------------------
	// *** CALCUL DES PUISSANCES ***
	// * ATTAQUES - DEFENSES - PROTECTIONS *
	// -------------------------------------

	// Protection des rois
	int w_king_protection = get_pawn_shield_protection(true, b_attacking_potential) * pawn_protection_factor;
	int b_king_protection = get_pawn_shield_protection(false, w_attacking_potential) * pawn_protection_factor;

	// Attaquants
	int w_attacking_power = get_king_attackers(true);
	int b_attacking_power = get_king_attackers(false);

	// Plus y'a d'attaque, plus c'est difficile de défendre (même s'il y a beaucoup de défenseurs) -> exponentielle?
	// Constante à partir de laquelle on considère un *2 sur la puissance d'attaque
	constexpr int double_attack = 700;
	float w_mult_attack = 1.0f + w_attacking_power * w_attacking_potential / static_cast<float>(double_attack);
	float b_mult_attack = 1.0f + b_attacking_power * b_attacking_potential/ static_cast<float>(double_attack);

	w_attacking_power *= w_mult_attack;
	b_attacking_power *= b_mult_attack;

	w_attacking_power *= piece_attack_factor;
	b_attacking_power *= piece_attack_factor;

	// Défenseurs
	int w_defending_power = get_king_defenders(true) * piece_defense_factor;
	int b_defending_power = get_king_defenders(false) * piece_defense_factor;

	// Défense du roi seul
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

	//rnq1k2r/pp2bp2/2p5/3p4/5Pb1/P2P1NPp/1PP4K/R1BQ1R1N b kq - 0 17 : overload sur notre propre pion h3??...
	// r3k2r/ppqn3n/3b1p2/2ppp1p1/4P2p/P2P1P1P/1PPBBN1K/R1NQ1R2 b kq - 5 22 : overload +495???

	// Récupère les maps de contrôle des cases
	Map white_controls_map = get_white_controls_map();
	Map black_controls_map = get_black_controls_map();

	// Est-ce utile?
	//white_controls_map.print();
	//black_controls_map.print();

	// Résultante des contrôles
	Map controls_map = white_controls_map - black_controls_map;

	//controls_map.print();

	// Danger des surcharges (cases controlées en supériorité par l'allié), proche du roi adverse
	constexpr uint_fast8_t overloard_distance_dangers[8] = { 100, 50, 5, 1, 0, 0, 0, 0 };


	// Overload sur le roi blanc
	int w_king_overloaded = 0;

	// Cases controllées proches du roi
	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			// Pièce sur cette case
			uint_fast8_t p = _array[i][j];

			if (controls_map._array[i][j] < 0 && p <= w_king)
			{
				const uint_fast8_t distance = max(abs(i - _white_king_pos.row), abs(j - _white_king_pos.col));
				w_king_overloaded -= overloard_distance_dangers[distance] * controls_map._array[i][j]; // - car valeur négative
				//cout << "square: " << square_name(i, j) << ", piece: " << piece_name(p) << " / distance: " << (int)distance << " / overload: " << overloard_distance_dangers[distance] * controls_map._array[i][j] << endl;
			}
		}
	}
	
	// Overload sur le roi noir
	int b_king_overloaded = 0;

	// Attaques sur le plateau
	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			// Pièce sur cette case
			uint_fast8_t p = _array[i][j];

			if (controls_map._array[i][j] > 0 && (p >= b_pawn || p == none))
			{
				const uint_fast8_t distance = max(abs(i - _black_king_pos.row), abs(j - _black_king_pos.col));
				b_king_overloaded += overloard_distance_dangers[distance] * controls_map._array[i][j];
				//cout << "square: " << square_name(i, j) << ", piece: " << piece_name(p) << " / distance: " << (int)distance << " / overload: " << overloard_distance_dangers[distance] * controls_map._array[i][j] << endl;
			}
		}
	}

	const float overload_factor = 1.5f;
	//const float overload_factor = 0.0f;

	w_king_overloaded *= overload_factor * b_attacking_potential;
	b_king_overloaded *= overload_factor * w_attacking_potential;

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "Overloaded: " + to_string(w_king_overloaded) + " / " + to_string(b_king_overloaded) + "\n";
	//}


	// -------------------
	// *** PROTECTIONS ***
	// -------------------

	//rnq1k2r/pp2bpp1/2p1bn2/3pp3/7p/P2PP2P/1PPNBPP1/R1BQ1RKN w kq - 2 11 : le roi en h2 n'est pas si horrible
	//r3k2r/ppq2p2/2p1bP2/3pn3/8/P2PPB2/1PPNK2p/R1BQ3R b kq - 7 21 : bug sur grand roque des noirs???
	//r3k3/p1q2p1r/4b3/1p1p4/6P1/P2PPQb1/1P1NB1P1/R1B2RK1 b q - 6 22 : il faut roquer pour ramener une autre tour à l'attaque
	//Nnb2b1r/1p1k1p1p/p4p2/8/3p4/8/PP2PPPP/R3KB1R b KQ - 0 12

	// -----------------------
	// *** POSITION DU ROI ***
	// -----------------------

	// Proximité avec le bord
	// Avancement à partir duquel il est plus dangereux d'être sur un bord
	//constexpr float edge_adv = 0.85f;
	//constexpr float mult_endgame = 25.0f;
	//constexpr float safe_zone = 0.25f;

	//// Version additive, adaptée pour l'endgame
	//constexpr int edge_defense = 75;
	
	//8/8/1k6/3Q4/4K3/8/8/8 w - - 19 136
	//r1k2b1r/p5p1/2p4p/8/4p1b1/4B3/PPP2P1P/2KR2R1 w - - 0 21 : avant manger le fou: 0, après: 200+...

	// r1bq1b1r/ppp3pp/4k3/3np3/1nB5/2N3Q1/PPPP1PPP/R1B1K2R b KQ - 5 9 : Rf7 vs Rf5... analyser...

	// Distances aux bords
	/*int w_col_dist = min(_white_king_pos.col, 7 - _white_king_pos.col);
	int w_row_dist = min(_white_king_pos.row, 7 - _white_king_pos.row);
	int b_col_dist = min(_black_king_pos.col, 7 - _black_king_pos.col);
	int b_row_dist = min(_black_king_pos.row, 7 - _black_king_pos.row);

	int w_placement_weakness = edge_defense * ((edge_adv - _adv) * ((_adv < edge_adv) ? (max(0, w_col_dist - 1) + _white_king_pos.row * _white_king_pos.row / 2.0f) : (mult_endgame / (edge_adv - 1.0f) * (1.0f / ((w_col_dist + 1) * (w_row_dist + 1)) - safe_zone))));
	int b_placement_weakness = edge_defense * ((edge_adv - _adv) * ((_adv < edge_adv) ? (max(0, b_col_dist - 1) + (7 - _black_king_pos.row) * (7 - _black_king_pos.row) / 2.0f) : (mult_endgame / (edge_adv - 1.0f) * (1.0f / ((b_col_dist + 1) * (b_row_dist + 1)) - safe_zone))));*/

	const int w_placement_weakness = get_king_placement_weakness(true);
	const int b_placement_weakness = get_king_placement_weakness(false);

	//cout << b_placement_weakness << endl;

	// Si le roi peut roquer, on ne considère pas les problèmes de placement
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


	// --------------------------
	// *** ESPACE DE MOBILITE ***
	// --------------------------

	// TEST (pas sûr)
	constexpr float space_safety_factor = 0.0f;

	const int space = get_space();

	const int w_space = space_safety_factor * space;
	const int b_space = - space_safety_factor * space;

	// ---------------------------------
	// *** MOBILITE VIRTUELLE DU ROI ***
	// ---------------------------------
	
	// FIXME: est-ce vraiment utile? ça casse peut-être tout en fait
	//constexpr int virtual_mobility_danger = 20;
	constexpr int virtual_mobility_danger = 0;

	// Mobilité à partir de laquelle le roi est en danger
	constexpr int virtual_mobility_threshold = 3;

	const int w_virtual_mobility = virtual_mobility_danger * (max(0, get_king_virtual_mobility(true) - virtual_mobility_threshold)) * (1 - _adv);
	const int b_virtual_mobility = virtual_mobility_danger * (max(0, get_king_virtual_mobility(false) - virtual_mobility_threshold)) * (1 - _adv);

	// ---------------------
	// *** RANK WEAKNESS ***
	// ---------------------

	// TODO *********
	const int w_rank_weakness = get_king_row_weakness(true);
	const int b_rank_weakness = get_king_row_weakness(false);

	// TODO

	// --------------------
	// *** WEAK SQUARES ***
	// --------------------

	const int w_weak_squares = get_weak_squares(true, true) * b_attacking_potential;
	const int b_weak_squares = get_weak_squares(false, true) * w_attacking_potential;

	// ------------------
	// *** MATING NET ***
	// ------------------

	// TODO *********

	// ------------------
	// *** PAWN STORM ***
	// ------------------

	constexpr float pawn_storm_danger = 1.5f;

	int w_pawn_storm = get_pawn_storm(true) * pawn_storm_danger;
	int b_pawn_storm = get_pawn_storm(false) * pawn_storm_danger;

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "Pawn storms: " + to_string(w_pawn_storm) + " / " + to_string(b_pawn_storm) + "\n";
	//}

	// ------------------
	// *** OPEN LINES ***
	// ------------------

	//5rk1/5ppp/4bn2/q2p4/2p5/b1P1BN2/3Q1PPP/1K1RRB2 w - - 2 9
	//r1b2k1r/ppp1qpp1/3bP1pn/3P4/4N3/8/PPPBQ1PP/2KR1R2 b - - 6 18

	constexpr float open_lines_danger = 3.5f;

	int w_open_lines = get_open_files_on_opponent_king(true) * open_lines_danger;
	int b_open_lines = get_open_files_on_opponent_king(false) * open_lines_danger;

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "Open lines: " + to_string(w_open_lines) + " / " + to_string(b_open_lines) + "\n";
	//}


	// ----------------------
	// *** OPEN DIAGONALS ***
	// ----------------------

	constexpr float open_diagonals_danger = 0.0f;

	int w_open_diagonals = get_open_diagonals_on_opponent_king(true) * open_diagonals_danger;
	int b_open_diagonals = get_open_diagonals_on_opponent_king(false) * open_diagonals_danger;

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "Open diagonals: " + to_string(w_open_diagonals) + " / " + to_string(b_open_diagonals) + "\n";
	//}


	// --------------
	// *** CHECKS ***
	// --------------

	const int w_checks = get_checks_value(white_controls_map, black_controls_map, true) * w_attacking_potential;
	const int b_checks = get_checks_value(white_controls_map, black_controls_map, false) * b_attacking_potential;

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "Checks: " + to_string(w_checks) + " / " + to_string(b_checks) + "\n";
	//}

	// -------------------------------------
	// *** CALCUL DE LA FAIBLESSE DU ROI ***
	// -------------------------------------


	// Augmentation des faiblesses long-terme quand les rois sont éloignés
	const float king_distance_factor = 0.25f * (1 - _adv);

	// Faiblesse accentuée quand les rois sont éloignés
	const float long_term_weakness_distance_factor = 0.75 * (1 + king_distance_factor * sqrt(king_columns_diff));

	// Attaque court terme accentuée quand les rois sont éloignés
	const float short_term_weakness_distance_factor = 0.75 * (1 + king_distance_factor * sqrt(king_columns_diff));



	// Facteur multiplicatif en cas de faiblesse négative (pour compenser le court/long terme)
	const float negative_long_term_factor = 1.0f;

	// Compensation
	// TESTS
	// En théorie, le court terme ne peut jamais complètement rembourser les faiblesses long-terme... On doit toujours en garder une petite partie
	// Dans le cas où le court terme devient positif, il doit aussi augmenter avec les faiblesses long-terme?

	//const float base_compensation = 0.20f / (short_term_weakness_distance_factor * short_term_weakness_distance_factor);
	//const float w_negative_short_term_factor = base_compensation + (1 - b_attacking_potential) * 0.25f;
	//const float b_negative_short_term_factor = base_compensation + (1 - w_attacking_potential) * 0.25f;

	// Short term = k, short term = max(0, short_term) - factor * long_term / short_term
	// Short term = 0 -> factor = 0
	// Short term = -500 -> factor = -0.33
	// Short term = -1000 -> factor = -0.5
	// Short term = 1000 -> factor = 0.5

	// Valeur pour laquelle la compensation est de 0.5
	constexpr int short_term_compensation_value = 850;

	//const float w_short_term_compensation_factor = 1 - 1 / (1 + abs(w_short_term_weakness) / short_term_compensation_value);

	// rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w kq - 4 4

	// Roi noir (attaque des blancs)

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

	// Faiblesse accentuée quand les rois sont éloignés
	b_long_term_weakness *= long_term_weakness_distance_factor;


	//if (b_long_term_weakness > 0) {
	//	b_long_term_weakness *= w_attacking_potential;
	//}

	// Réduction si l'on peut encore roquer?
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
	int b_short_term_weakness = w_checks + w_attacking_overload + b_king_overloaded;

	// Faiblesse accentuée quand les rois sont éloignés
	b_short_term_weakness *= short_term_weakness_distance_factor;

	// Compensation court terme / long terme (entre 0 et 1)
	const float b_short_term_compensation_factor = 1.0f - 1.0f / (1.0f + abs(b_short_term_weakness) / static_cast<float>(short_term_compensation_value) / w_attacking_potential);
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
		main_GUI._eval_components += ") * Kings distance : " + to_string(short_term_weakness_distance_factor);
		main_GUI._eval_components += " = " + to_string(b_short_term_weakness) + "\n";
	}

	b_king_weakness = b_long_term_weakness + b_short_term_weakness;

	// En fonction du potentiel d'attaque
	//b_king_weakness *= w_attacking_potential;


	// Roi blanc (attaque des noirs)

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

	// Faiblesse accentuée quand les rois sont éloignés
	w_long_term_weakness *= long_term_weakness_distance_factor;

	//if (w_long_term_weakness > 0) {
	//	w_long_term_weakness *= b_attacking_potential;
	//}

	// Réduction si l'on peut encore roquer?
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
	int w_short_term_weakness = b_checks + b_attacking_overload + w_king_overloaded;

	// Faiblesse accentuée quand les rois sont éloignés
	w_short_term_weakness *= short_term_weakness_distance_factor;

	const float w_short_term_compensation_factor = 1.0f - 1.0f / (1.0f + abs(w_short_term_weakness) / static_cast<float>(short_term_compensation_value) / b_attacking_potential);
	//short term = max(0, short_term) - factor * long_term / short_term

	//cout << "w short term: " << w_short_term_weakness << " / w long term: " << w_long_term_weakness << " / w factor: " << w_short_term_compensation_factor << endl;

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
		main_GUI._eval_components += ") * Kings distance : " + to_string(short_term_weakness_distance_factor);
		main_GUI._eval_components += " = " + to_string(w_short_term_weakness) + "\n";
	}

	w_king_weakness = w_long_term_weakness + w_short_term_weakness;

	// En fonction du potentiel d'attaque
	//w_king_weakness *= b_attacking_potential;


	// Ajout de la protection du roi... la faiblesse du roi ne peut pas être négative (potentiellement à revoir, mais parfois la surprotection donne des valeurs délirantes)
	//float overprotection_factor = 0.15f;

	// TEST: fonction non linéaire, pour amortir les différences autour de 0, et éviter les trop grosses surprotections

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

	// Renvoie la différence de faiblesse entre les rois
	const int king_safety = b_king_weakness - w_king_weakness;

	return king_safety;
}

// Fonction qui dit si une pièce est capturable par l'ennemi (pour les affichages GUI)
bool Board::is_capturable(const int i, const int j) {
	_got_moves == -1 && get_moves(true);

	for (int k = 0; k < _got_moves; k++)
		if (_moves[k].end_row == i && _moves[k].end_col == j)
			return true;

	return false;
}

// Fonction qui affiche le PGN
void Board::display_pgn() const
{
	cout << "\n***** PGN *****\n" << main_GUI._pgn << "\n***** PGN *****" << endl;
}

// Fonction qui renvoie selon l'évaluation si c'est un mat ou non (0 si non, sinon le nombre de coups pour le mat, positif pour les blancs, négatif pour les noirs)
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

// Fonction qui génère le livre d'ouvertures
void Board::generate_opening_book(int nodes) {
	//// Lit le livre d'ouvertures actuel
	//string book = LoadFileText("resources/data/opening_book.txt");
	//cout << "Book : " << book << endl;

	//// Se place à l'endroit concerné dans le livre ----> mettre des FEN dans le livre et chercher?
	//to_fen();
	//size_t pos = book.find(_fen); // Que faire si y'en a plusieurs? Fabriquer un tableau avec les positions puis diviser le livre en plus de parties? puis insérer au milieu...
	//const string book_part_1;
	//const string book_part_2;

	//const string add_to_book = "()";

	//// Regarde si tous les coups ont été testés. Sinon, teste un des coups restants -> avec nodes noeuds

	//const string new_book = book_part_1 + add_to_book + book_part_2;

	//SaveFileText("resources/data/opening_book.txt", const_cast<char*>(new_book.c_str()));
}

// Fonction qui renvoie si deux positions (en format FEN) sont les mêmes
bool equal_fen(const string& fen_a, const string& fen_b) {
	size_t k = fen_a.find(' ');
	k = fen_a.find(' ', k + 1);
	k = fen_a.find(' ', k + 1);
	k = fen_a.find(' ', k + 1);
	const string simple_fen_a = fen_a.substr(0, k);

	k = fen_b.find(' ');
	k = fen_b.find(' ', k + 1);
	k = fen_b.find(' ', k + 1);
	k = fen_b.find(' ', k + 1);
	const string simple_fen_b = fen_b.substr(0, k);

	return (simple_fen_a == simple_fen_b);
}

// Fonction qui renvoie si deux positions (en format FEN) sont les mêmes (pour les répétitions)
bool equal_positions(const Board& a, const Board& b) {
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++)
			if (a._array[i][j] != b._array[i][j])
				return false;

	return (a._player == b._player && a._castling_rights == b._castling_rights && a._en_passant_col == b._en_passant_col);
}

// Fonction qui renvoie une représentation simple et rapide de la position
string Board::simple_position() const
{
	string s;
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++)
			s += _array[i][j];

	s += _player + _castling_rights.k_b + _castling_rights.k_w + _castling_rights.q_b + _castling_rights.q_w;
	s += _en_passant_col;

	return s;
}

// Fonction qui calcule la structure de pions et renvoie sa valeur
int Board::get_pawn_structure(float display_factor)
{
	// Améliorations :
	// Nombre d'ilots de pions
	// Pions faibles
	// Contrôle des cases
	// Pions passés
	// Candidats pions passés

	int pawn_structure = 0;

	// Liste des pions par colonne
	int s_white[8] = { 0 };
	int s_black[8] = { 0 };

	// Placement des pions (6 lignes suffiraient théoriquement... car on ne peut pas avoir de pions sur la première ou la dernière rangée...)
	bool pawns_white[8][8] = { { 0 } };
	bool pawns_black[8][8] = { { 0 } };

	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			s_white[j] += (_array[i][j] == w_pawn);
			s_black[j] += (_array[i][j] == b_pawn);
			pawns_white[i][j] = (_array[i][j] == w_pawn);
			pawns_black[i][j] = (_array[i][j] == b_pawn);
		}
	}

	// TEST: r4rk1/pp2qppp/3b1n2/8/P2pP3/3B1P2/P2B2PP/2RQ1RK1 b - - 0 17 : pourquoi isolated positif?

	// Pions isolés
	constexpr int isolated_pawn = -35;
	constexpr float open_row_factor = 1.5f; // Si le pion isolé est sur une colonne ouverte, il est beaucoup plus faible
	constexpr float isolated_adv_factor = 0.25f; // En fonction de l'advancement de la partie
	const float isolated_adv = eval_from_progress(1, _adv, isolated_adv_factor);
	float isolated_pawns = 0.0f;

	for (uint_fast8_t col = 0; col < 8; col++) {

		// Pion isolé blanc sur la colonne
		if (s_white[col] > 0 && (col == 0 || s_white[col - 1] == 0) && (col == 7 || s_white[col + 1] == 0)) {
			bool is_open_col = true;

			for (uint_fast8_t row = 7; row > 0; row--) {
				if (_array[row][col] == w_pawn) {
					break;
				}
				else if (_array[row][col] == b_pawn) {
					is_open_col = false;
					break;
				}
			}

			//cout << "W col: " << (int)col << " | " << is_open_col << ", isolated pawns: " << s_white[col] << ", total: " << s_white[col] + is_open_col * (open_row_factor - 1) << endl;

			isolated_pawns += isolated_pawn * (s_white[col] + is_open_col * (open_row_factor - 1)) * isolated_adv;
		}

		// Pion isolé noir sur la colonne
		if (s_black[col] > 0 && (col == 0 || s_black[col - 1] == 0) && (col == 7 || s_black[col + 1] == 0)) {
			bool is_open_col = true;

			for (uint_fast8_t row = 0; row < 7; row++) {
				if (_array[row][col] == b_pawn) {
					break;
				}
				else if (_array[row][col] == w_pawn) {
					is_open_col = false;
					break;
				}
			}

			//cout << "B col: " << (int)col << " | " << is_open_col << ", isolated pawns: " << s_black[col] << ", total: " << s_black[col] + is_open_col * (open_row_factor - 1) << endl;

			isolated_pawns -= isolated_pawn * (s_black[col] + is_open_col * (open_row_factor - 1)) * isolated_adv;
		}
	}

	if (display_factor != 0.0f)
		main_GUI._eval_components += "isolated pawns: " + (isolated_pawns >= 0 ? string("+") : string()) + to_string(static_cast<int>(isolated_pawns * display_factor)) + "\n";

	pawn_structure += isolated_pawns;

	// Pions doublés (ou triplés...)
	constexpr int doubled_pawn = -60;
	constexpr float doubled_adv_factor = 0.45f; // En fonction de l'advancement de la partie
	const float doubled_adv = eval_from_progress(1, _adv, doubled_adv_factor);
	float doubled_pawns = 0.0f;

	for (uint_fast8_t i = 0; i < 8; i++) {
		doubled_pawns += (s_white[i] >= 2) * doubled_pawn * (s_white[i] - 1) * doubled_adv;
		doubled_pawns -= (s_black[i] >= 2) * doubled_pawn * (s_black[i] - 1) * doubled_adv;
	}

	if (display_factor != 0.0f)
		main_GUI._eval_components += "doubled pawns: " + (doubled_pawns >= 0 ? string("+") : string()) + to_string(static_cast<int>(doubled_pawns * display_factor)) + "\n";

	pawn_structure += doubled_pawns;

	// Pions arrierés
	// Pion qui ne peut être défendu par aucun autre pion, et qui est fixé par un pion adverse (la case devant est controlée par un pion adverse)
	constexpr int backward_pawn = -15;
	constexpr float backward_adv_factor = 0.0f; // En fonction de l'advancement de la partie
	constexpr float backward_open_factor = 2.5f; // Si le pion arriéré est sur une colonne ouverte, il est beaucoup plus faible
	constexpr float blocked_factor = 2.0f; // Si le pion est bloqué par une pièce adverse, ou si la case devant est controlée par un pion adverse
	const float backward_adv = eval_from_progress(1, _adv, backward_adv_factor);
	float backward_pawns = 0.0f;

	// Pions blancs
	for (uint_fast8_t col = 0; col < 8; col++) {
		for (uint_fast8_t row = 1; row < 7; row++) {
			if (pawns_white[row][col]) {
				bool is_backward = true;
				
				// Y'a-t-il un potentiel défenseur?
				for (int_fast8_t k = row; k >= 1; k--) {
					if (col > 0 && _array[k][col - 1] == w_pawn) {
						is_backward = false;
						break;
					}

					if (col < 7 && _array[k][col + 1] == w_pawn) {
						is_backward = false;
						break;
					}
				}

				// La case devant est-elle controlée par un pion adverse?
				if (is_backward) {
					bool is_controlled = false;

					if (row < 6 && col > 0 && _array[row + 2][col - 1] == b_pawn) {
						is_controlled = true;
					}
					else if (row < 6 && col < 7 && _array[row + 2][col + 1] == b_pawn) {
						is_controlled = true;
					}

					bool is_blocked = is_black(_array[row + 1][col]);

					// Si la colonne est ouverte, le pion arriéré est beaucoup plus faible
					bool is_open_col = true;

					for (uint_fast8_t k = 7; k > row; k--) {
						if (_array[k][col] == b_pawn) {
							is_open_col = false;
							break;
						}
					}

					backward_pawns += backward_pawn * backward_adv * ((is_controlled || is_blocked) ? blocked_factor : 1.0f) * (is_open_col ? backward_open_factor : 1.0f);
				}
			}
		}
	}

	// Pions noirs
	for (uint_fast8_t col = 0; col < 8; col++) {
		for (uint_fast8_t row = 6; row > 0; row--) {
			if (pawns_black[row][col]) {
				bool is_backward = true;

				// Y'a-t-il un potentiel défenseur?
				for (int_fast8_t k = row; k <= 6; k++) {
					if (col > 0 && _array[k][col - 1] == b_pawn) {
						is_backward = false;
						break;
					}

					if (col < 7 && _array[k][col + 1] == b_pawn) {
						is_backward = false;
						break;
					}
				}

				// La case devant est-elle controlée par un pion adverse?
				if (is_backward) {
					bool is_controlled = false;

					if (row > 1 && col > 0 && _array[row - 2][col - 1] == w_pawn) {
						is_controlled = true;
					}
					else if (row > 1 && col < 7 && _array[row - 2][col + 1] == w_pawn) {
						is_controlled = true;
					}

					bool is_blocked = is_white(_array[row - 1][col]);

					// Si la colonne est ouverte, le pion arriéré est beaucoup plus faible
					bool is_open_col = true;

					for (uint_fast8_t k = 0; k < row; k++) {
						if (_array[k][col] == w_pawn) {
							is_open_col = false;
							break;
						}
					}

					backward_pawns -= backward_pawn * backward_adv * ((is_controlled || is_blocked) ? blocked_factor : 1.0f) * (is_open_col ? backward_open_factor : 1.0f);
				}
			}
		}
	}

	if (display_factor != 0.0f)
		main_GUI._eval_components += "backward pawns: " + (backward_pawns >= 0 ? string("+") : string()) + to_string(static_cast<int>(backward_pawns * display_factor)) + "\n";

	pawn_structure += backward_pawns;

	// Pions passés
	// r5k1/P3Rpp1/7p/8/3p4/8/2P2PPP/6K1 b - - 1 30 : position à tester
	// 8/P5kp/8/q7/8/8/5B1P/5K2 w - - 1 57 : ici, il faut considérer la case controlée (par rayon X)

	// 8/5bB1/8/5PP1/8/4K3/p7/1k6 w - - 1 9 : blancs gagnants! la case est controllée pour l'autre pion passé...

	// Table de valeur des pions passés en fonction de leur avancement sur le plateau
	static constexpr int passed_pawns[8] = { 0, 175, 175, 250, 370, 650, 1250, 0 };

	// Facteur de division par pièce qui contrôle (cavalier, fou, tour, dame, roi)
	//static constexpr float control_division_per_piece[5] = { 1.5f, 1.75f, 1.35f, 1.2f, 1.35f };
	static constexpr float control_division = 1.85f;

	// Facteur de division par pièce qui bloque (cavalier, fou, tour, dame, roi)
	static constexpr float block_division_per_piece[5] = { 2.75f, 2.15f, 2.0f, 1.55f, 2.3f };

	// Bloquage par une pièce alliée
	static constexpr float self_block_division = 1.5f;

	// Bonus pour les pions passés connectés
	constexpr int connected_passed_pawn_bonus = 2.0f;


	// Pion passé - chemin controllé par une pièce adverse
	//static constexpr int controlled_passed_pawn[8] = { 0, 40, 40, 70, 130, 235, 350, 0 };

	// Pion passé bloqué
	//static constexpr int blocked_passed_pawn[8] = { 0, 20, 30, 55, 110, 200, 300, 0 };


	constexpr float passed_adv_factor = 1.0f; // En fonction de l'advancement de la partie
	const float passed_adv = eval_from_progress(1, _adv, passed_adv_factor);
	float passed_pawns_value = 0.0f;

	//print_array(s_white, 8);
	//print_array(s_black, 8);

	//Map white_controls_map = get_white_controls_map();
	//Map black_controls_map = get_black_controls_map();

	// Bonus quand le roi n'est pas dans le carré du pion passé
	constexpr int out_of_square_bonus[8] = { 0, 500, 500, 750, 1000, 1250, 1500, 0 };
	//constexpr int out_of_square_bonus[8] = { 0, 500, 500, 500, 500, 500, 500, 0 };

	// Est-on dans une finale de pions?
	bool pawn_endgame = is_pawn_endgame();

	// Les blancs ont-ils encore des pièces?
	bool has_white_pieces = has_pieces(true);

	// Les noirs ont-ils encore des pièces?
	bool has_black_pieces = has_pieces(false);

	// Pour chaque colonne
	for (uint_fast8_t col = 0; col < 8; col++) {

		// On prend en compte seulement le pion le plus avancé de la colonne (car les autre seraient bloqués derrière)

		// Pions blancs
		if (s_white[col] > 0) {

			// On regarde de la rangée la plus proche de la promotion, jusqu'a la première
			for (uint_fast8_t row = 6; row > 0; row--) {

				// S'il y a un pion potentiellement passé
				if (pawns_white[row][col]) {

					// Pas de pion sur une colonne adjacente ou pareille avec une lattitude supérieure (strictement)
					bool is_passed_pawn = true;
					for (uint_fast8_t k = row + 1; k < 7; k++) {
						if ((col > 0 && _array[k][col - 1] == b_pawn) || _array[k][col] == b_pawn || (col < 7 && _array[k][col + 1] == b_pawn)) {
							is_passed_pawn = false;
							break;
						}
					}

					// Si c'est un pion passé
					if (is_passed_pawn) {

						// Contrôles et bloquages
						float division_factor = 1.0f;

						// Regarde s'il y a un bloqueur
						for (uint_fast8_t k = row + 1; k <= 7; k++) {
							if (is_black(_array[k][col])) {
								division_factor += block_division_per_piece[_array[k][col] - 8] - 1.0f;
							}
							else if (is_white(_array[k][col])) {
								division_factor += self_block_division - 1.0f;
							}
						}

						// On retire le pion pour regarder si la case est controlée par rayon X
						_array[row][col] = none;

						Map white_controls_map = get_white_controls_map();
						Map black_controls_map = get_black_controls_map();

						for (uint_fast8_t k = row + 1; k <= 7; k++) {
							int controls_diff = max(0, black_controls_map._array[k][col] - white_controls_map._array[k][col]);
							division_factor += (control_division - 1.0f) * controls_diff;
						}

						// On remet le pion
						_array[row][col] = w_pawn;


						int passed_value = passed_pawns[row] * (!has_black_pieces ? 2.0f : 1.0f);

						// Est-il connecté avec un autre pion?
						if ((col > 0 && (pawns_white[row][col - 1] || pawns_white[row - 1][col - 1])) || (col < 7 && (pawns_white[row][col + 1] || pawns_white[row - 1][col + 1]))) {
							passed_value *= connected_passed_pawn_bonus;
						}

						//cout << "Passed pawn: " << square_name(row, col) << " (" << passed_value << " ) | " << division_factor << ": " << passed_value / division_factor * passed_adv << endl;

						// Finale de pions -> le roi est-il dans le carré du pion passé?
						bool out_of_square = !has_black_pieces && !in_king_square(Pos(row, col), false);

						//cout << "Passed pawn: " << square_name(row, col) << ", Is pawn endgame: " << pawn_endgame << ", Out of square: " << out_of_square << ", bonus: " << out_of_square * out_of_square_bonus[row] << endl;

						// Ajoute la valeur du pion passé
						passed_pawns_value += (passed_value / division_factor + out_of_square * out_of_square_bonus[row]) * passed_adv;
					}

				}
			}
		}

		// rnb2rk1/p5pp/3qp3/1P1p4/3P1p2/4PP2/1pPBB1PP/R2Q1RK1 w - - 0 16

		// Pions noirs
		if (s_black[col] > 0) {

			// On regarde de la rangée la plus proche de la promotion, jusqu'a la première
			for (uint_fast8_t row = 1; row < 7; row++) {

				// S'il y a un pion potentiellement passé
				if (pawns_black[row][col]) {

					// Pas de pion sur une colonne adjacente ou pareille avec une lattitude supérieure (strictement)
					bool is_passed_pawn = true;
					for (uint_fast8_t k = row - 1; k > 0; k--) {
						if ((col > 0 && _array[k][col - 1] == w_pawn) || _array[k][col] == w_pawn || (col < 7 && _array[k][col + 1] == w_pawn)) {
							is_passed_pawn = false;
							break;
						}
					}

					// Si c'est un pion passé
					if (is_passed_pawn) {

						// Contrôles et bloquages
						float division_factor = 1.0f;

						// Regarde s'il y a un bloqueur
						for (int_fast8_t k = row - 1; k >= 0; k--) {
							if (is_white(_array[k][col])) {
								division_factor += block_division_per_piece[_array[k][col] - 2] - 1.0f;
							}
							else if (is_black(_array[k][col])) {
								division_factor += self_block_division - 1.0f;
							}
						}

						// On retire le pion pour regarder si la case est controlée par rayon X
						_array[row][col] = none;

						Map white_controls_map = get_white_controls_map();
						Map black_controls_map = get_black_controls_map();

						for (int_fast8_t k = row - 1; k >= 0; k--) {
							int controls_diff = max(0, white_controls_map._array[k][col] - black_controls_map._array[k][col]);
							division_factor += (control_division - 1.0f) * controls_diff;
						}

						// On remet le pion
						_array[row][col] = b_pawn;

						int passed_value = passed_pawns[7 - row] * (!has_white_pieces ? 2.0f : 1.0f);

						// Est-il connecté avec un autre pion?
						if ((col > 0 && (pawns_black[row][col - 1] || pawns_black[row + 1][col - 1])) || (col < 7 && (pawns_black[row][col + 1] || pawns_black[row + 1][col + 1]))) {
							passed_value *= connected_passed_pawn_bonus;
						}

						//8/8/4p2p/1R6/pPpP1k2/K6P/8/8 b - - 0 43
						//cout << "Passed pawn: " << square_name(row, col) << " (" << passed_value << " ) | " << division_factor << ": " << passed_value / division_factor * passed_adv << endl;

						// Finale de pions -> le roi est-il dans le carré du pion passé?
						bool out_of_square = !has_white_pieces && !in_king_square(Pos(row, col), true);

						//cout << "Passed pawn: " << square_name(row, col) << ", Is pawn endgame: " << pawn_endgame << ", Out of square: " << out_of_square << ", bonus: " << out_of_square * out_of_square_bonus[7 - row] << endl;

						// 8/8/7P/6p1/pP1p4/P7/3Kpk2/8 w - - 2 8

						// Ajoute la valeur du pion passé
						passed_pawns_value -= (passed_value / division_factor + out_of_square * out_of_square_bonus[7 - row]) * passed_adv;
					}

				}
			}
		}
	}

	// Valeur qui diminue quand on pousse c3??

	if (display_factor != 0.0f)
		main_GUI._eval_components += "passed pawns: " + (passed_pawns_value >= 0 ? string("+") : string()) + to_string(static_cast<int>(passed_pawns_value * display_factor)) + "\n";

	pawn_structure += passed_pawns_value;


	// Pions connectés
	// Un pion est dit connecté, s'il y a un pion de la même couleur sur une colonne adjacente sur la même rangée ou la rangée inférieure
	constexpr int connected_pawns[8] = { 0, 25, 35, 70, 120, 180, 275, 0 };
	constexpr float connected_pawns_factor = 0.3f; // En fonction de l'advancement de la partie
	const float connected_pawns_adv = eval_from_progress(1, _adv, connected_pawns_factor);

	float connected_pawns_value = 0.0f;

	// Pour chaque colonne
	for (uint_fast8_t j = 0; j < 8; j++) {
		for (uint_fast8_t i = 1; i < 7; i++) {
			if (pawns_white[i][j]) {
				if ((j > 0 && (pawns_white[i][j - 1] || pawns_white[i - 1][j - 1])) || (j < 7 && (pawns_white[i][j + 1] || pawns_white[i - 1][j + 1]))) {
					// S'il est contesté par un pion adverse, on retire le bonus
					if (j > 0 && pawns_black[i + 1][j - 1] || j < 7 && pawns_black[i + 1][j + 1]) {
						// TODO
					}
					else {
						connected_pawns_value += connected_pawns[i] * connected_pawns_adv;
					}
				}
			}
			else if (pawns_black[i][j]) {
				if ((j > 0 && (pawns_black[i][j - 1] || pawns_black[i + 1][j - 1])) || (j < 7 && (pawns_black[i][j + 1] || pawns_black[i + 1][j + 1]))) {
					// S'il est contesté par un pion adverse, on retire le bonus
					if (j > 0 && pawns_white[i - 1][j - 1] || j < 7 && pawns_white[i - 1][j + 1]) {
						// TODO
					}
					else {
						connected_pawns_value -= connected_pawns[7 - i] * connected_pawns_adv;
					}
				}
			}
		}
	}

	if (display_factor != 0.0f)
		main_GUI._eval_components += "connected pawns: " + (connected_pawns_value >= 0 ? string("+") : string()) + to_string(static_cast<int>(connected_pawns_value * display_factor)) + "\n";

	pawn_structure += connected_pawns_value;

	// TODO *** les facteurs multiplicatifs par avancement devraient être ajoutés à la fin

	// On doit encore vérifier si le pion adverse est devant ou derrière l'autre pion...

	return pawn_structure;
}

// Fonction qui renvoie le temps que l'IA doit passer sur le prochain coup (en ms), en fonction d'un facteur k, et des temps restant
int time_to_play_move(const int t1, int t2, const float k) {
	return t1 * k;

	// A améliorer :
	// Prendre en compte le temps de l'adversaire
	// Prendre en compte le nombre de coups restants dans la partie (ou une approximation) -> Si on va mater ou si on est quasi foutu -> passer plus de temps
	// Prendre en compte les variations d'évaluation, ou les coups montants
	// Reste à gérer les incréments
	// Nombre de noeuds min avant de jouer?
}

// Fonction qui calcule la résultante des attaques et des défenses et la renvoie
float Board::get_attacks_and_defenses() const
{
	// TODO: à revoir (pour mieux modéliser les vraies menaces)
	// FIXME *** malus pour les pièces non-protegées?

	// Tableau des valeurs d'attaques des pièces (0 = pion, 1 = caval, 2 = fou, 3 = tour, 4 = dame, 5 = roi)
	static constexpr int attacks_array[6][6] = {
	//   P    N    B     R    Q    K
		{50, 130, 135, 150, 200, 100}, // P
		{40,  60,  70, 150, 175, 100}, // N
		{40,  50,  60, 125, 160, 100}, // B
		{35,  40,  50,  50, 135, 100}, // R
		{25,  30,  35,  45,  50,  50}, // Q
		{15,  10,  10,  15,   0,   0}, // K
	};

	// Tableau des valeurs de défenses des pièces (0 = pion, 1 = caval, 2 = fou, 3 = tour, 4 = dame, 5 = roi)
	static constexpr int defenses_array[6][6] = {
	//   P    N    B     R    Q    K
		{100, 45,  45,  35,  30,   0}, // P
		{25,  30,  30,  30,  30,   0}, // N
		{25,  30,  30,  30,  30,   0}, // B
		{20,  20,  20,  35,  25,   0}, // R
		{10,   5,   5,   5,   5,   0}, // Q
		{10,   5,   5,   5,   5,   0}, // K
	};

	// TODO ne pas additionner la défense de toutes les pièces? seulement regarder les pièces non-défendues? (sinon devient pleutre)
	// FIXME : les attaques/défenses par rayon X ne sont pas prises en compte

	// Tableau d'attaques pour les blancs
	int attacks_white[8][8] = { 0 };

	// Tableau d'attaques pour les noirs
	int attacks_black[8][8] = { 0 };


	// Tableau de défenses pour les blancs
	int defenses_white[8][8] = { 0 };

	// Tableau de défenses pour les noirs
	int defenses_black[8][8] = { 0 };


	// TODO : utiliser des constantes pour les calculs redondants


	// TODO changer les if par des &&

	for (int row = 0; row < 8; row++) {
		for (int col = 0; col < 8; col++) {

			// Pièce sur la case
			uint_fast8_t p = _array[row][col];

			// Si la case est vide, on passe
			if (p == none)
				continue;

			switch (p) {

				// Pion blanc
				case w_pawn:
					if (col > 0) {
						// Case haut-gauche du pion blanc
						uint_fast8_t new_row = row + 1;
						uint_fast8_t new_col = col - 1;
						uint_fast8_t p2 = _array[new_row][new_col];

						if (is_black(p2))
							attacks_white[new_row][new_col] += attacks_array[0][p2 - 7];
						else if (is_white(p2))
							defenses_white[new_row][new_col] += defenses_array[0][p2 - 1];
					}
					if (col < 7) {
						// Case haut-droit du pion blanc
						uint_fast8_t new_row = row + 1;
						uint_fast8_t new_col = col + 1;
						uint_fast8_t p2 = _array[new_row][new_col];

						if (is_black(p2))
							attacks_white[new_row][new_col] += attacks_array[0][p2 - 7];
						else if (is_white(p2))
							defenses_white[new_row][new_col] += defenses_array[0][p2 - 1];
					}

					break;

				// Cavalier blanc
				case w_knight:
					for (int k = 0; k < 8; k++) {
						uint_fast8_t new_row = row + knight_directions[k][0];
						uint_fast8_t new_col = col + knight_directions[k][1];

						if (is_in_fast(new_row, 0, 7) && is_in_fast(new_col, 0, 7)) {
							uint_fast8_t p2 = _array[new_row][new_col];

							if (is_black(p2))
								attacks_white[new_row][new_col] += attacks_array[1][p2 - 7];
							else if (is_white(p2))
								defenses_white[new_row][new_col] += defenses_array[1][p2 - 1];
						}
					}

					break;

				// Fou blanc
				case w_bishop:
					for (uint_fast8_t k = 0; k < 4; k++) {
						const uint_fast8_t d_row = diag_directions[k][0];
						const uint_fast8_t d_col = diag_directions[k][1];
						uint_fast8_t new_row = row + d_row;
						uint_fast8_t new_col = col + d_col;

						while (new_row >= 0 && new_row < 8 && new_col >= 0 && new_col < 8) {
							const uint_fast8_t p2 = _array[new_row][new_col];

							if (is_black(p2))
								attacks_white[new_row][new_col] += attacks_array[2][p2 - 7];
							else if (is_white(p2))
								defenses_white[new_row][new_col] += defenses_array[2][p2 - 1];

							// Si y'a une pièce, on arrête
							if (p2 != none)
								break;

							new_row += d_row;
							new_col += d_col;
						}
					}

					break;

				// Tour blanche
				case w_rook:
					for (uint_fast8_t k = 0; k < 4; k++) {
						const uint_fast8_t d_row = rect_directions[k][0];
						const uint_fast8_t d_col = rect_directions[k][1];
						uint_fast8_t new_row = row + d_row;
						uint_fast8_t new_col = col + d_col;

						while (new_row >= 0 && new_row < 8 && new_col >= 0 && new_col < 8) {
							const uint_fast8_t p2 = _array[new_row][new_col];

							if (is_black(p2))
								attacks_white[new_row][new_col] += attacks_array[3][p2 - 7];
							else if (is_white(p2))
								defenses_white[new_row][new_col] += defenses_array[3][p2 - 1];

							// Si y'a une pièce, on arrête
							if (p2 != none)
								break;

							new_row += d_row;
							new_col += d_col;
						}
					}
					break;

				// Dame blanche
				case w_queen:
					for (uint_fast8_t k = 0; k < 8; k++) {
						const uint_fast8_t d_row = all_directions[k][0];
						const uint_fast8_t d_col = all_directions[k][1];
						uint_fast8_t new_row = row + d_row;
						uint_fast8_t new_col = col + d_col;

						while (new_row >= 0 && new_row < 8 && new_col >= 0 && new_col < 8) {
							const uint_fast8_t p2 = _array[new_row][new_col];

							if (is_black(p2))
								attacks_white[new_row][new_col] += attacks_array[4][p2 - 7];
							else if (is_white(p2))
								defenses_white[new_row][new_col] += defenses_array[4][p2 - 1];

							// Si y'a une pièce, on arrête
							if (p2 != none)
								break;

							new_row += d_row;
							new_col += d_col;
						}
					}

					break;

				// Roi blanc
				case w_king:
					for (uint_fast8_t k = 0; k < 8; k++) {
						uint_fast8_t new_row = row + all_directions[k][0];
						uint_fast8_t new_col = col + all_directions[k][1];

						if (is_in_fast(new_row, 0, 7) && is_in_fast(new_col, 0, 7)) {
							uint_fast8_t p2 = _array[new_row][new_col];

							if (is_black(p2))
								attacks_white[new_row][new_col] += attacks_array[5][p2 - 7];
							else if (is_white(p2))
								defenses_white[new_row][new_col] += defenses_array[5][p2 - 1];
						}
					}

					break;

				// Pion noir
				case b_pawn:
					if (col > 0) {
						// Case bas-gauche du pion noir
						uint_fast8_t new_row = row - 1;
						uint_fast8_t new_col = col - 1;
						uint_fast8_t p2 = _array[new_row][new_col];

						if (is_white(p2))
							attacks_black[new_row][new_col] += attacks_array[0][p2 - 1];
						else if (is_black(p2))
							defenses_black[new_row][new_col] += defenses_array[0][p2 - 7];
					}
					if (col < 7) {
						// Case bas-droit du pion noir
						uint_fast8_t new_row = row - 1;
						uint_fast8_t new_col = col + 1;
						uint_fast8_t p2 = _array[new_row][new_col];

						if (is_white(p2))
							attacks_black[new_row][new_col] += attacks_array[0][p2 - 1];
						else if (is_black(p2))
							defenses_black[new_row][new_col] += defenses_array[0][p2 - 7];
					}

					break;
			
				// Cavalier noir
				case b_knight:
					for (int k = 0; k < 8; k++) {
						uint_fast8_t new_row = row + knight_directions[k][0];
						uint_fast8_t new_col = col + knight_directions[k][1];

						if (is_in_fast(new_row, 0, 7) && is_in_fast(new_col, 0, 7)) {
							uint_fast8_t p2 = _array[new_row][new_col];

							if (is_white(p2))
								attacks_black[new_row][new_col] += attacks_array[1][p2 - 1];
							else if (is_black(p2))
								defenses_black[new_row][new_col] += defenses_array[1][p2 - 7];
						}
					}

					break;

				// Fou noir
				case b_bishop:
					for (uint_fast8_t k = 0; k < 4; k++) {
						const uint_fast8_t d_row = diag_directions[k][0];
						const uint_fast8_t d_col = diag_directions[k][1];
						uint_fast8_t new_row = row + d_row;
						uint_fast8_t new_col = col + d_col;

						while (new_row >= 0 && new_row < 8 && new_col >= 0 && new_col < 8) {
							const uint_fast8_t p2 = _array[new_row][new_col];

							if (is_white(p2))
								attacks_black[new_row][new_col] += attacks_array[2][p2 - 1];
							else if (is_black(p2))
								defenses_black[new_row][new_col] += defenses_array[2][p2 - 7];

							// Si y'a une pièce, on arrête
							if (p2 != none)
								break;

							new_row += d_row;
							new_col += d_col;
						}
					}

					break;

				// Tour noire
				case b_rook:
					for (uint_fast8_t k = 0; k < 4; k++) {
						const uint_fast8_t d_row = rect_directions[k][0];
						const uint_fast8_t d_col = rect_directions[k][1];
						uint_fast8_t new_row = row + d_row;
						uint_fast8_t new_col = col + d_col;

						while (new_row >= 0 && new_row < 8 && new_col >= 0 && new_col < 8) {
							const uint_fast8_t p2 = _array[new_row][new_col];

							if (is_white(p2))
								attacks_black[new_row][new_col] += attacks_array[3][p2 - 1];
							else if (is_black(p2))
								defenses_black[new_row][new_col] += defenses_array[3][p2 - 7];

							// Si y'a une pièce, on arrête
							if (p2 != none)
								break;

							new_row += d_row;
							new_col += d_col;
						}
					}

					break;

				// Dame noire
				case b_queen:
					for (uint_fast8_t k = 0; k < 8; k++) {
						const uint_fast8_t d_row = all_directions[k][0];
						const uint_fast8_t d_col = all_directions[k][1];
						uint_fast8_t new_row = row + d_row;
						uint_fast8_t new_col = col + d_col;

						while (new_row >= 0 && new_row < 8 && new_col >= 0 && new_col < 8) {
							const uint_fast8_t p2 = _array[new_row][new_col];

							if (is_white(p2))
								attacks_black[new_row][new_col] += attacks_array[4][p2 - 1];
							else if (is_black(p2))
								defenses_black[new_row][new_col] += defenses_array[4][p2 - 7];

							// Si y'a une pièce, on arrête
							if (p2 != none)
								break;

							new_row += d_row;
							new_col += d_col;
						}
					}

					break;

				// Roi noir
				case b_king:
					for (uint_fast8_t k = 0; k < 8; k++) {
						uint_fast8_t new_row = row + all_directions[k][0];
						uint_fast8_t new_col = col + all_directions[k][1];

						if (is_in_fast(new_row, 0, 7) && is_in_fast(new_col, 0, 7)) {
							uint_fast8_t p2 = _array[new_row][new_col];

							if (is_white(p2))
								attacks_black[new_row][new_col] += attacks_array[5][p2 - 1];
							else if (is_black(p2))
								defenses_black[new_row][new_col] += defenses_array[5][p2 - 7];
						}
					}

					break;
			}
		}
	}


	// Somme toutes les valeurs positives des tableaux d'attaques pour chaque camp
	int white_attacks_eval = 0;
	int black_attacks_eval = 0;

	// Valeur max de défense
	int max_defense = 10;

	// Malus pour les pièces non-défendues (pions, cavaliers, fous, tours, dames, rois)
	//int undefended_malus[6] = { 0, 20, 8, 10, 5, 0 };
	int undefended_malus[6] = { 0, 0, 0, 0, 0, 0 };

	// TESTS
	//rnbqkbnr/ppp2ppp/4p3/3p4/3P4/2N5/PPP1PPPP/R1BQKBNR w KQkq - 0 3
	//4r2k/2q3bp/R4pp1/1R3p2/3P4/1PP2N1n/1P1Q1P1P/7K w - - 1 30

	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			uint_fast8_t p = _array[row][col];

			if (p == none) {
				continue;
			}


			if (is_white(p)) {
				int black_attack = attacks_black[row][col] - defenses_white[row][col] + (defenses_white[row][col] == 0 ? undefended_malus[p - 1] : 0);

				if (black_attack < 0) {
					float division_factor = 1.0f - static_cast<float>(black_attack) / max_defense;
					black_attack = static_cast<int>(black_attack / division_factor);
				}

				black_attacks_eval += black_attack;

				//cout << "W: " << square_name(row, col) << " (" << piece_name(p) << ") : " << attacks_black[row][col] << "/" << defenses_white[row][col] << " - undefended: " << (defenses_white[row][col] == 0 ? undefended_malus[p - 1] : 0) << " - Value: " << black_attack << " | B TOTAL: " << black_attacks_eval << endl;
			}
			else {
				int white_attack = attacks_white[row][col] - defenses_black[row][col] + (defenses_black[row][col] == 0 ? undefended_malus[p - 7] : 0);

				if (white_attack < 0) {
					float division_factor = 1.0f - static_cast<float>(white_attack) / max_defense;
					white_attack = static_cast<int>(white_attack / division_factor);
				}

				white_attacks_eval += white_attack;

				//cout << "B: " << square_name(row, col) << " (" << piece_name(p) << ") : " << attacks_white[row][col] << "/" << defenses_black[row][col] << " - undefended: " << (defenses_black[row][col] == 0 ? undefended_malus[p - 7] : 0) << " - Value: " << white_attack << " | W TOTAL: " << white_attacks_eval << endl;
			}
		}
	}

	// r6r/p1kb2p1/1p2p3/3pP3/2p2P2/2P1R1P1/PP1N2P1/R5K1 b - - 0 20 : les noirs n'attaquent rien, mais ont quand-même du bonus?

	// 4R3/p4pkp/2p2pb1/qp1p4/2nP2PN/PRPB3P/2P2P2/6K1 b - - 2 21

	// En fonction de l'avancement
	constexpr float advancement_factor = 0.5f;

	//cout << "\nWhite attacks: " << white_attacks_eval << " | Black attacks: " << black_attacks_eval << endl;

	return eval_from_progress(white_attacks_eval - black_attacks_eval, _adv, advancement_factor);
}

// Fonction qui calcule l'opposition des rois (en finales de pions)
int Board::get_kings_opposition() {

	// FIXME *** est-ce que ça devrait être compté seulement en finales de pions?

	// Met à jour la position des rois
	update_kings_pos();

	// Les rois sont-ils opposés?
	const int di = abs(_white_king_pos.row - _black_king_pos.row);
	const int dj = abs(_white_king_pos.col - _black_king_pos.col);
	if (!((di == 0 || di == 2) && (dj == 0 || dj == 2)))
		return 0;

	// S'ils sont opposés, le joueur qui a l'opposition, est celui qui n'a pas le trait
	int opposition = -get_color();

	// L'opposition doit commencer en fin de partie
	float adv_begin = 0.9f;

	return opposition * (_adv - adv_begin) / (1.0f - adv_begin);
}

// Fonction qui renvoie le type de pièce sélectionnée
uint_fast8_t Board::selected_piece() const
{
	// Faut-il stocker cela pour éviter de le re-calculer?
	if (main_GUI._selected_pos.row == -1 || main_GUI._selected_pos.col == -1)
		return 0;
	return _array[main_GUI._selected_pos.row][main_GUI._selected_pos.col];
}

// Fonction qui renvoie le type de pièce où la souris vient de cliquer
uint_fast8_t Board::clicked_piece() const
{
	if (main_GUI._clicked_pos.row == -1 || main_GUI._clicked_pos.col == -1)
		return 0;
	return _array[main_GUI._clicked_pos.row][main_GUI._clicked_pos.col];
}

// Fonction qui renvoie si la pièce sélectionnée est au joueur ayant trait ou non
bool Board::selected_piece_has_trait() const
{
	return ((_player && is_in_fast(selected_piece(), 1, 6)) || (!_player && is_in_fast(selected_piece(), 7, 12)));
}

// Fonction qui renvoie si la pièce cliquée est au joueur ayant trait ou non
bool Board::clicked_piece_has_trait() const
{
	return ((_player && is_in_fast(clicked_piece(), 1, 6)) || (!_player && is_in_fast(clicked_piece(), 7, 12)));
}

// Fonction qui remet les compteurs de temps "à zéro" (temps de base)
void Board::reset_timers() {
	// Temps par joueur (en ms)
	main_GUI._time_white = main_GUI._initial_time_white;
	main_GUI._time_black = main_GUI._initial_time_black;
}

// Fonction qui remet le plateau dans sa position initiale
void Board::restart() {
	// Fonction largement optimisable
	from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	// _pgn = "";
	reset_timers();
}

// Fonction qui renvoie la différence matérielle entre les deux camps
int Board::material_difference() const
{
	int mat = 0;
	int w_material[6] = { 0, 0, 0, 0, 0, 0 };
	int b_material[6] = { 0, 0, 0, 0, 0, 0 };

	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			const int p = _array[i][j];
			if (p > 0) {
				if (p < 6)
					w_material[p]++;
				else
					b_material[p % 6]++;
			}

			mat += main_GUI._piece_GUI_values[p % 6] * (1 - (p / 6) * 2);
		}
	}

	for (int i = 0; i < 6; i++) {
		main_GUI._missing_w_material[i] = max(0, main_GUI._base_material[i] - w_material[i]);
		main_GUI._missing_b_material[i] = max(0, main_GUI._base_material[i] - b_material[i]);
	}

	return mat;
}

// Fonction qui réinitialise les composantes de l'évaluation
void Board::reset_eval() {
	_displayed_components = false;
	_evaluated = false; _evaluation = 0;
	_advancement = false; _adv = 0;
}

// Fonction qui compte les tours sur les colonnes ouvertes et semi-ouvertes et renvoie la valeur
int Board::get_sliders_on_open_file() const
{
	// TEST: 5rk1/6p1/p2qp2p/1b1p4/3P4/1P2BP2/3Q2PP/2R3K1 w - - 0 28

	// Bonus
	constexpr int rook_open_bonus = 50;
	constexpr int rook_semi_open_bonus = 35;
	constexpr int queen_open_bonus = 30;
	constexpr int queen_semi_open_bonus = 20;

	int w_open_value = 0;
	int b_open_value = 0;

	int w_semi_open_value = 0;
	int b_semi_open_value = 0;

	// Pions sur les colonnes, tours sur les colonnes et dames sur les colonnes
	uint_fast8_t w_pawns[8] = { 0 };
	uint_fast8_t b_pawns[8] = { 0 };
	uint_fast8_t w_rooks[8] = { 0 };
	uint_fast8_t b_rooks[8] = { 0 };
	uint_fast8_t w_queens[8] = { 0 };
	uint_fast8_t b_queens[8] = { 0 };


	// Calcul du nombre de pions, tours et dames par colonne
	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			const uint_fast8_t p = _array[row][col];

			if (p == w_pawn) {
				w_pawns[col]++;
			}
			else if (p == w_rook) {
				w_rooks[col]++;
			}
			else if (p == w_queen) {
				w_queens[col]++;
			}
			else if (p == b_pawn) {
				b_pawns[col]++;
			}
			else if (p == b_rook) {
				b_rooks[col]++;
			}
			else if (p == b_queen) {
				b_queens[col]++;
			}
		}
	}

	// Colonnes ouvertes
	uint_fast8_t open_files = 0;

	for (uint_fast8_t col = 0; col < 8; col++) {
		bool is_open = !w_pawns[col] && !b_pawns[col];

		// Si la colonne est ouverte
		if (is_open) {
			open_files++;

			w_open_value += w_rooks[col] * rook_open_bonus;
			b_open_value += b_rooks[col] * rook_open_bonus;

			w_open_value += w_queens[col] * queen_open_bonus;
			b_open_value += b_queens[col] * queen_open_bonus;
		}
	}

	// Colonnes semi-ouvertes
	uint_fast8_t w_semi_open_files = 0;
	uint_fast8_t b_semi_open_files = 0;

	for (uint_fast8_t col = 0; col < 8; col++)
	{
		bool w_semi_open = !w_pawns[col] && b_pawns[col];
		bool b_semi_open = w_pawns[col] && !b_pawns[col];

		// Si la colonne est semi-ouverte pour les blancs
		if (w_semi_open) {
			w_semi_open_files++;
			w_semi_open_value += w_rooks[col] * rook_semi_open_bonus;
			w_semi_open_value += w_queens[col] * queen_semi_open_bonus;
		}

		// Si la colonne est semi-ouverte pour les noirs
		if (b_semi_open) {
			b_semi_open_files++;
			b_semi_open_value += b_rooks[col] * rook_semi_open_bonus;
			b_semi_open_value += b_queens[col] * queen_semi_open_bonus;
		}
	}

	//cout << "Open files: " << (int)open_files << " | Semi-open files: " << (int)w_semi_open_files << " / " << (int)b_semi_open_files << endl;
	//cout << "W open value: " << w_open_value << " | B open value: " << b_open_value << endl;
	//cout << "W semi-open value: " << w_semi_open_value << " | B semi-open value: " << b_semi_open_value << endl;

	// L'importance est moindre s'il y a plusieurs colonnes ouvertes
	w_open_value = w_open_value == 0 ? 0 : w_open_value / open_files;
	b_open_value = b_open_value == 0 ? 0 : b_open_value / open_files;

	// L'importance est moindre s'il y a plusieurs colonnes semi-ouvertes
	w_semi_open_value = w_semi_open_value == 0 ? 0 : w_semi_open_value / (w_semi_open_files + open_files * 2);
	b_semi_open_value = b_semi_open_value == 0 ? 0 : b_semi_open_value / (b_semi_open_files + open_files * 2);

	//cout << "W open value: " << w_open_value << " | B open value: " << b_open_value << endl;
	//cout << "W semi-open value: " << w_semi_open_value << " | B semi-open value: " << b_semi_open_value << endl;

	// Valeur totale
	return w_open_value - b_open_value + w_semi_open_value - b_semi_open_value;
}

// Fonction qui calcule la valeur des cases controllées sur l'échiquier
int Board::get_square_controls() const
{
	// TODO ajouter des valeurs pour le contrôle des cases par les pièces?
	// rnbqkbnr/pp3ppp/2p5/3pp3/8/1P4P1/PBPPPPBP/RN1QK1NR b KQkq - 1 4 : faut pas faire e4 ni d4

	// Valeur du contrôle de chaque case (pour les pions)
	static constexpr int square_controls[8][8] = {
		{10,  10,  10,  10,  10,  10,  10,  10},
		{20,  20,  30,  40,  40,  30,  20,  20},
		{10,  25,  45,  65,  65,  45,  25,  10},
		{5,   35,  55,  75,  75,  55,  35,   5},
		{0,   15,  25,  30,  30,  25,  15,   0},
		{5,    5,  10,  20,  20,  10,   5,   5},
		{0,    0,   0,   0,   0,   0,   0,   0},
		{0,    0,   0,   0,   0,   0,   0,   0}
	};

	int total_control = 0;

	// Calcul des cases controllées par les pions de chaque camp
	// TODO Regarder si avoir un double contrôle c'est important
	bool white_controls[8][8] = { {false} };
	bool black_controls[8][8] = { {false} };

	// Ajoute le contrôle des cases par les pions
	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			const int p = _array[i][j];
			(j - 1 >= 0 && 7 - i - 1 >= 0) && (white_controls[7 - i - 1][j - 1] |= (p == 1));
			(j + 1 < 8 && 7 - i - 1 >= 0) && (white_controls[7 - i - 1][j + 1] |= (p == 1));
			(j - 1 >= 0 && i - 1 >= 0) && (black_controls[i - 1][j - 1] |= (p == 7));
			(j + 1 < 8 && i - 1 >= 0) && (black_controls[i - 1][j + 1] |= (p == 7));
		}
	}

	// Somme les contrôles des cases par les pions
	for (uint_fast8_t i = 0; i < 8; i++)
		for (uint_fast8_t j = 0; j < 8; j++)
			total_control += (white_controls[i][j] - black_controls[i][j]) * square_controls[i][j];

	// L'importance de ce paramètre dépend de l'avancement de la partie : l'espace est d'autant plus important que le nombre de pièces est grand
	constexpr float control_adv_factor = 0.5f; // En fonction de l'advancement de la partie

	return eval_from_progress(total_control, _adv, control_adv_factor);
}

// Fonction qui renvoie la valeur UCT
float uct(const float win_chance, const float c, const int nodes_parent, const int nodes_child) {
	// cout << win_chance << ", " << nodes_parent << ", " << nodes_child << " = " << win_chance + c * sqrt(log(nodes_parent) / nodes_child) << endl;
	return win_chance + c * static_cast<float>(sqrt(log(nodes_parent) / nodes_child));
}

// Fonction qui fait un tri rapide des coups (en plaçant les captures en premier)
// TODO : à faire lors de la génération de coups?
bool Board::sort_moves() {

	// Si le tri a déjà été fait
	if (_sorted_moves)
		return false;

	// Captures, promotions.. échecs?
	// TODO : ajouter les échecs? les mats?
	// TODO : faire en fonction de la pièce qui prend?

	// Liste des valeurs de chaque coup
	int *moves_values = new int[_got_moves];

	//cout << "test0.2" << endl;

	// Valeurs assignées

	// Prises
	static const int captures_values[13] = { 0, 100, 300, 300, 500, 900, 10000, 100, 300, 300, 500, 900, 10000 }; // rien,   |pion, cavalier, fou, tour, dame, roi| (blancs, puis noirs)

	// Promotions
	static constexpr int promotion_value = 500;

	// Valeur des pièces en jeu

	// Pour chaque coup
	for (int i = 0; i < _got_moves; i++) {

		// Constantes pour éviter les accès mémoire
		const Move move = _moves[i];
		const int piece = _array[move.start_row][move.start_col];

		// Prise
		const int captured_value = captures_values[_array[move.end_row][move.end_col]];

		// Valeur de la pièce qui prend
		const int capturer_value = captures_values[piece];

		// Valeur du coup
		//int move_value = captured_value == 0 ? 0 : captured_value / capturer_value;

		// Alternative plus précise
		int move_value = 0;
		if (captured_value != 0) {
			move_value = captured_value - capturer_value;
			move_value += (move_value < 0 && is_controlled(move.end_row, move.end_col, _player)) ? move_value : 10000;
		}

		// Promotion
		// Blancs
		move_value += (piece == w_pawn && move.end_row == 7) * promotion_value;

		// Noirs
		move_value += (piece == b_pawn && move.end_row == 0) * promotion_value;

		// Assignation de la valeur
		moves_values[i] = move_value;

		//cout << move_label(move) << " : " << move_value << endl;
	}

	//print_array(moves_values, _got_moves);


	//print_array(moves_values, _got_moves);

	// Plus lent??

	//// Paires (coups, valeurs)
	//vector<pair<Move, int>> pairs;
	//for (int i = 0; i < _got_moves; i++)
	//	pairs.emplace_back(_moves[i], moves_values[i]);

	//// Trie les paires par ordre décroissant de valeur
	//sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

	//// Update the original key and value lists
	//for (int i = 0; i < _got_moves; i++)
	//	_moves[i] = pairs[i].first;

	//// Suppression des tableaux
	//delete[] moves_values;


	// Construction des nouveaux coups

	// Liste des index des coups triés par ordre décroissant de valeur
	const auto moves_indexes = new int[_got_moves];

	for (int i = 0; i < _got_moves; i++) {
		const int max_ind = max_index(moves_values, _got_moves);
		moves_indexes[i] = max_ind;
		moves_values[max_ind] = -INT_MAX;
	}

	//print_array(moves_indexes, _got_moves);

	// Génération de la liste de coups de façon ordonnée
	Move* new_moves = new Move[_got_moves];
	copy(_moves, _moves + _got_moves, new_moves);

	for (int i = 0; i < _got_moves; i++)
		_moves[i] = new_moves[moves_indexes[i]];

	// Suppression des tableaux
	delete[] moves_values;
	delete[] new_moves;
	delete[] moves_indexes;

	_sorted_moves = true;

	return true;
}

// Fonction qui fait cliquer le coup m
bool Board::click_m_move(const Move m, const bool orientation) const
{
	simulate_mouse_release();
	click_move(m.start_row, m.start_col, m.end_row, m.end_col , main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom, orientation);
	SetWindowFocused();

	return true;
}

// Fonction qui met à jour une text box
void update_text_box(TextBox& text_box) {
	// Si on clique
	if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
		const Vector2 mouse_pos = GetMousePosition();
		const Rectangle rect = { text_box.x, text_box.y, text_box.width, text_box.height };

		// Sur la text box
		if (CheckCollisionPointRec(mouse_pos, rect)) {
			// Active, et montre la valeur
			if (!text_box.active) {
				text_box.active = true;
				text_box.text = to_string(text_box.value);
			}
		}

		else {
			// Désactive, et met à jour la valeur
			if (text_box.active) {
				text_box.value = stoi(text_box.text);
				text_box.active = false;
			}
		}
	}

	// Si elle est active
	if (text_box.active) {
		// Prend les input du clavier
		const int key = GetKeyPressed();

		// Regarde seulement les touches importantes (numériques)
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

// Fonction qui dessine une text box
void draw_text_box(const TextBox& text_box) {
	const Rectangle rect = { text_box.x, text_box.y, text_box.width, text_box.height };
	DrawRectangleRec(rect, text_box.main_color);

	//Vector2 text_dimensions = MeasureTextEx(textBox.text_font, textBox.text.c_str(), textBox.text_size, font_spacing * textBox.text_size); // C'est pour dessiner au milieu
	const float text_x = text_box.x + text_box.text_size / 2;
	const float text_y = text_box.y + text_box.text_size / 4;
	DrawTextEx(text_box.text_font, text_box.text.c_str(), { text_x, text_y }, text_box.text_size, main_GUI._font_spacing * text_box.text_size, text_box.text_color);
}

// Fonction qui récupère et renvoie la couleur du joueur au trait (1 pour les blancs, -1 pour les noirs)
int Board::get_color() const
{
	return _player ? 1 : -1;
}

// Fonction qui calcule l'avantage d'espace
int Board::get_space() const
{
	// Multiplication par un poids
	// L'avantage d'espace dépend du nombre de pièces restantes

	int w_pieces = 0; int b_pieces = 0;

	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			if (is_in(_array[i][j], 2, 6))
				w_pieces++;
			else if (is_in(_array[i][j], 8, 12))
				b_pieces++;
		}
	}
		

	// Nombre de colonnes ouvertes
	int open_rows = 0;
	for (uint_fast8_t j = 0; j < 8; j++) {
		bool open = true;
		for (uint_fast8_t i = 0; i < 8; i++) {
			if (_array[i][j] == 1 || _array[i][j] == 7) {
				open = false;
				break;
			}
		}
		if (open)
			open_rows++;
	}

	// Poids
	const int w_weight = max(0, w_pieces - 2 * open_rows);
	const int b_weight = max(0, b_pieces - 2 * open_rows);

	// Avantage d'espace
	int space_area = 0;

	// Valeur de l'avantage d'espace pour chaque case centrale
	int w_space[3][4]{};
	int b_space[3][4]{};

	// Assigne les valeurs pour chaque case
	for (uint_fast8_t j = 2; j < 6; j++) {
		// Inutile de calculer si le poids est nul
		if (w_weight != 0) {
			// Blancs
			for (uint_fast8_t i = 1; i <= 3; i++) {
				// S'il y a un pion allié sur la case, ou qu'elle est contrôlée par un pion adverse, met la valeur 0
				if (_array[i][j] == 1 || (_array[i + 1][j - 1] == 7) || (_array[i + 1][j + 1] == 7))
					w_space[i - 1][j - 2] = 0;
				// Sinon, s'il y a un pion allié à moins de 3 cases devant, met la valeur à 2
				else if (_array[i + 1][j] == 1 || _array[i + 2][j] == 1 || _array[i + 3][j] == 1)
					w_space[i - 1][j - 2] = 2;
				else
					w_space[i - 1][j - 2] = 1;
			}
		}

		if (b_weight != 0)
		{
			// Noirs
			for (uint_fast8_t i = 6; i >= 4; i--) {
				// S'il y a un pion allié sur la case, ou qu'elle est contrôlée par un pion adverse, met la valeur 0
				if (_array[i][j] == 7 || (_array[i - 1][j - 1] == 1) || (_array[i - 1][j + 1] == 1))
					b_space[6 - i][j - 2] = 0;
				// Sinon, s'il y a un pion allié à moins de 3 cases devant, met la valeur à 2
				else if (_array[i - 1][j] == 7 || _array[i - 2][j] == 7 || _array[i - 3][j] == 7)
					b_space[6 - i][j - 2] = 2;
				else
					b_space[6 - i][j - 2] = 1;
			}
		}
	}

	if (w_weight != 0 || b_weight != 0) {
		// Calcule l'avantage d'espace
		for (uint_fast8_t i = 0; i < 3; i++) {
			for (uint_fast8_t j = 0; j < 4; j++) {
				space_area += w_space[i][j] * w_weight - b_space[i][j] * b_weight;
			}
		}
	}

	constexpr float space_adv_factor = 0.5f; // En fonction de l'advancement de la partie

	return space_area * max(0.0f, (1 + (space_adv_factor - 1) * _adv));
}

// Fonction qui calcule et renvoie une évaluation des vis-à-vis
int Board::get_alignments() const
{
	// TODO: parfois des pièces peuvent complètement en déclouer une autre (un fou décloue une diagonale...)
	// 8/2p2kp1/3bp2p/4p3/1pP1P2P/1P1Q1NP1/qr2RPK1/8 w - - 5 34 : ici par exemple, la tour en b2 décloue la dame en a2
	// rnb1kbnr/pp1ppppp/2p5/q7/8/2NP4/PPPBPPPP/R2QKBNR b KQkq - 3 3
	// r2qk2r/pb1pbpp1/1pn4n/2p1P2p/8/2NB1N1P/PP1BQPP1/3RR1K1 b kq - 2 13 : tour d1 en face de la dame

	// Valeurs des pièces clouées (TODO: prendre les autres valeurs par l'évaluation?)
	constexpr int pinned_king = 100;
	constexpr int pinned_queen = 80;
	constexpr int pinned_rook = 40;
	constexpr int pinned_bishop = 20;
	constexpr int pinned_knight = 20;
	constexpr int pinned_pawn = 5;

	constexpr int pieces_values[6] = { pinned_pawn, pinned_knight, pinned_bishop, pinned_rook, pinned_queen, pinned_king };

	// Valeurs pour les pièces alliées
	constexpr int ally_piece_value = 30;
	constexpr int ally_pawn_value = 5;

	// Puissance des clouages, par pièce
	constexpr float bishop_power = 1.0f;
	constexpr float rook_power = 1.0f;
	constexpr float queen_power = 0.3f;

	// Directions possibles pour les clouages

	// Valeur des alignements
	int w_pins = 0;
	int b_pins = 0;

	// Parcourt le plateau
	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {

			// Si la case contient une pièce
			const uint_fast8_t piece = _array[row][col];

			// Si ça n'est pas une sliding piece, on skip
			if (!is_sliding(piece))
				continue;

			// Couleur de la pièce
			const bool pinning_piece_color = piece < b_pawn;

			// Pour chaque direction
			for (uint_fast8_t d = 0; d < 8; d++) {

				// 4 premières directions: diagonales
				// 4 dernières directions: rectilignes

				if (!is_diagonal(piece) && d < 4)
					continue;

				if (!is_rectilinear(piece) && d >= 4)
					continue;

				// Direction
				int_fast8_t d_row = all_directions[d][0];
				int_fast8_t d_col = all_directions[d][1];

				// Position de la pièce
				uint_fast8_t current_row = row + d_row;
				uint_fast8_t current_col = col + d_col;

				// Liste des pièces rencontrées
				uint_fast8_t pieces[7] = { 0 };

				// Avancement dans la direction
				uint_fast8_t i = 0;


				// Tant que la case est dans le plateau
				while (current_row >= 0 && current_row <= 7 && current_col >= 0 && current_col <= 7) {
					// Si la case contient une pièce
					const uint_fast8_t piece2 = _array[current_row][current_col];

					// On continue
					current_row += d_row;
					current_col += d_col;

					// Ajoute la pièce à la liste
					if (piece2 != none) {
						pieces[i] = piece2;
						i++;
					}
				}

				// Calcule la valeur totale du clouage pour cette direction en fonction des pièces rencontrées
				int total_value = 0;

				// Valeur de la pièce qui cloue
				const uint_fast8_t pinning_piece_value = pieces_values[(piece - 1) % 6];

				// Puissance de la pièce qui cloue
				const float pinning_piece_power = is_bishop(piece) ? bishop_power : (is_rook(piece) ? rook_power : queen_power);

				//cout << "color: " << pinning_piece_color << ", square: " << square_name(i, j) << endl;

				// TODO: trouver formule...
				//rnb1kbnr/1p2pppp/p1qp4/2p5/B3P3/P1N2N2/1PPP1PPP/R1BQK2R b KQkq - 1 6

				// Les premières pièces compte plus que les dernières

				// TENTATIVE: pour chaque pièce, on multiplie par la valeur de la précédente, et on divise par une constante d'éloignement
				uint_fast8_t previous_piece = 0;
				bool is_previous_ally = true;
				uint_fast8_t distance = 1;

				for (int j = 0; j < i; j++) {
					uint_fast8_t pinned_piece = pieces[j];
					const bool pinned_piece_color = pinned_piece < b_pawn;
					const bool ally_piece = pinned_piece_color == pinning_piece_color;

					// Si on tombe sur une pièce adverse du même type, on arrête
					if ((pinned_piece - 1) % 6 == (piece - 1) % 6 && !ally_piece) {
						break;
					}

					const float division_factor = pow(distance, 4);
					const int pinned_piece_value = ally_piece ? ((pinned_piece - 1) % 6 == 0 ? ally_pawn_value : ally_piece_value) : pieces_values[(pinned_piece - 1) % 6];

					if (!ally_piece || !is_previous_ally) {
						total_value += pinning_piece_power * pinned_piece_value * previous_piece / division_factor;
					}

					//cout << "pinned: " << (int)pinned_piece << "total: " << total_value << endl;

					previous_piece = pinned_piece_value;
					is_previous_ally = ally_piece;
					distance++;
				}

				// DEBUG:
				if (total_value < 0) {
					cout << "negative value (overflow) in piece alignments" << endl;
				}

				if (pinning_piece_color) {
					w_pins += total_value;
				}
				else {
					b_pins += total_value;
				}
			}
		}
	}

	// En fonction de l'avancement de la partie
	constexpr float alignment_adv_factor = 1.0f;

	return eval_from_progress(w_pins - b_pins, _adv, alignment_adv_factor);
}

// Fonction qui met à jour la position des rois
bool Board::update_kings_pos()
{
	// Regarde si la position des rois est déjà connue
	bool search_white = _array[_white_king_pos.row][_white_king_pos.col] != w_king;
	bool search_black = _array[_black_king_pos.row][_black_king_pos.col] != b_king;

	// Affiche la position des rois (supposée), ainsi que la valeur de la pièce sur cette case
	/*cout << "White king pos : " << _white_king_pos.i << " " << _white_king_pos.j << " " << (int)_array[_white_king_pos.i][_white_king_pos.j] << endl;
	cout << "Black king pos : " << _black_king_pos.i << " " << _black_king_pos.j << " " << (int)_array[_black_king_pos.i][_black_king_pos.j] << endl;
	cout << search_white << " " << search_black << endl;*/

	if (!search_white && !search_black)
		return true;

	// Parcourt le plateau
	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			if (const uint_fast8_t piece = _array[i][j]; search_white && piece == w_king) {
				_white_king_pos = { i, j };
				if (!search_black)
					return true;
				search_white = false;
			}
			else if (search_black && piece == b_king) {
				_black_king_pos = { i, j };
				if (!search_white)
					return true;
				search_black = false;
			}
		}
	}

	return false;
}

// Fonction qui renvoie l'activité des pièces
int Board::get_piece_activity() const
{
	Map white_map = get_white_controls_map();
	Map black_map = get_black_controls_map();

	// 2r2b2/q4p1k/P1P3p1/1p1rBpPp/1P1NbP1P/2Q5/4R3/R4K2 w - - 4 37 : pas mieux aux blancs??

	// Puissance du contrôle d'une case adverse
	// Controllée 1 fois, 2 fois...
	static constexpr float controlled_power[8] = { 1.0f, 1.25f, 1.35f, 1.4f, 1.45f, 1.5f, 1.55f, 1.6f };

	// Puissance du contrôle de chaque case
	static constexpr uint_fast8_t activity_controlled_squares[8][8] = {
	{30, 30, 30, 30, 30, 30, 30, 30},
	{30, 30, 30, 30, 30, 30, 30, 30},
	{30, 30, 30, 30, 30, 30, 30, 30},
	{30, 30, 30, 30, 30, 30, 30, 30},
	{0,  0,  10, 20, 20, 10,  0,  0},
	{0,  0,  0,   5,  5,  0,  0,  0},
	{0,  0,  0,   0,  0,  0,  0,  0},
	{0,  0,  0,   0,  0,  0,  0,  0} };
	
	// Total de l'activité des pièce
	int white_activity = 0;
	int black_activity = 0;

	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			white_activity += activity_controlled_squares[7 - row][col] * controlled_power[min(7, white_map._array[row][col])];
			black_activity += activity_controlled_squares[row][col] * controlled_power[min(7, black_map._array[row][col])];
		}
	}

	// Facteur multiplicatif en fonction de l'avancement
	float advancement_factor = 0.5f;

	return eval_from_progress(white_activity - black_activity, _adv, advancement_factor);
}

// Fonction qui reset le buffer
bool Buffer::reset() const
{
	for (int i = 0; i < _length; i++)
		_heap_boards[i].reset_board();

	return true;
}

// Fonction qui renvoie la map correspondante au nombre de contrôles pour chaque case de l'échiquier pour le joueur blanc
Map Board::get_white_controls_map() const
{
	// Map de contrôles
	Map controls_map;

	// Itère sur toutes les pièces, et ajoute les contrôles de la pièce pour chaque case
	for (uint_fast8_t i = 0; i < 8; i++)
		for (uint_fast8_t j = 0; j < 8; j++) {
			const uint_fast8_t piece = _array[i][j];
			if (is_white(piece)) {
				add_piece_controls(&controls_map, i, j, piece);
			}
		}

	return controls_map;
}

// Fonction qui renvoie la map correspondante au nombre de contrôles pour chaque case de l'échiquier pour le joueur noir
Map Board::get_black_controls_map() const
{
	// FIXME : si y'a une tour derrière une autre, normalement les cases devraient être comptées 2 fois, hors ce n'est pas le cas

	// Map de contrôles
	Map controls_map;

	// Itère sur toutes les pièces, et ajoute les contrôles de la pièce pour chaque case
	for (uint_fast8_t i = 0; i < 8; i++)
		for (uint_fast8_t j = 0; j < 8; j++) {
			const uint_fast8_t piece = _array[i][j];
			if (is_black(piece)) {
				add_piece_controls(&controls_map, i, j, piece);
			}
		}

	return controls_map;
}

// Fonction qui ajoute à une map les contrôles d'une pièce
bool Board::add_piece_controls(Map* map, int i, int j, int piece) const
{
	if (piece == none)
		return false;

	// Pion blanc
	if (piece == w_pawn) {
		j > 0 && (map->_array[i + 1][j - 1]++);
		j < 7 && (map->_array[i + 1][j + 1]++);
		return true;
	}

	// Pion noir
	if (piece == b_pawn) {
		j > 0 && (map->_array[i - 1][j - 1]++);
		j < 7 && (map->_array[i - 1][j + 1]++);
		return true;
	}

	// Cavaliers
	if (is_knight(piece)) {
		for (uint_fast8_t k = 0; k < 8; k++) {
			const uint_fast8_t i2 = i + knight_directions[k][0];
			const uint_fast8_t j2 = j + knight_directions[k][1];
			i2 >= 0 && i2 <= 7 && j2 >= 0 && j2 <= 7 && (map->_array[i2][j2]++);
		}
		return true;
	}

	// Fous
	if (is_bishop(piece)) {
		for (uint_fast8_t k = 0; k < 4; k++) {
			const uint_fast8_t mi = diag_directions[k][0];
			const uint_fast8_t mj = diag_directions[k][1];
			uint_fast8_t i2 = i + mi;
			uint_fast8_t j2 = j + mj;
			while (i2 >= 0 && i2 <= 7 && j2 >= 0 && j2 <= 7) {
				map->_array[i2][j2]++;
				// Si la case est occupée, on arrête (sauf si la pièce est un fou, une dame ou un pion allié)
				//if (_array[i2][j2] != 0 && _array[i2][j2] != piece && _array[i2][j2] != (piece + 2) && _array[i2][j2] != piece - 2)
				if (_array[i2][j2] != 0 && _array[i2][j2] != piece && _array[i2][j2] != (piece + 2)) // On arrête même si c'est un pion allié
					break;
				i2 += mi;
				j2 += mj;
			}
		}
		return true;
	}

	// Tours
	if (is_rook(piece)) {
		for (uint_fast8_t k = 0; k < 4; k++) {
			const uint_fast8_t mi = rect_directions[k][0];
			const uint_fast8_t mj = rect_directions[k][1];
			uint_fast8_t i2 = i + mi;
			uint_fast8_t j2 = j + mj;
			while (i2 >= 0 && i2 <= 7 && j2 >= 0 && j2 <= 7) {
				map->_array[i2][j2]++;
				// Si la case est occupée, on arrête (sauf si la pièce est une tour ou une dame alliée)
				if (_array[i2][j2] != 0 && _array[i2][j2] != piece && _array[i2][j2] != (piece + 1)) 
					break;
				i2 += mi;
				j2 += mj;
			}
		}
		return true;
	}

	// Dames
	if (is_queen(piece)) {
		for (uint_fast8_t k = 0; k < 8; k++) {
			const uint_fast8_t mi = all_directions[k][0];
			const uint_fast8_t mj = all_directions[k][1];
			uint_fast8_t i2 = i + mi;
			uint_fast8_t j2 = j + mj;
			while (i2 >= 0 && i2 <= 7 && j2 >= 0 && j2 <= 7) {
				map->_array[i2][j2]++;
				bool diagonal = mi != 0 && mj != 0;
				// Si la case est occupée, on arrête (sauf si la pièce est une dame alliée, un (fou ou pion) allié et un déplacement diagonal, ou une tour alliée et un déplacement non diagonal)
				//if (_array[i2][j2] != 0 && _array[i2][j2] != piece && ((_array[i2][j2] != (piece - 2) && _array[i2][j2] != (piece - 4)) || !diagonal) && (_array[i2][j2] != (piece - 1) || diagonal))
				if (_array[i2][j2] != 0 && _array[i2][j2] != piece && ((_array[i2][j2] != (piece - 2)) || !diagonal) && (_array[i2][j2] != (piece - 1) || diagonal)) // On arrête même si c'est un pion allié
					break;
				i2 += mi;
				j2 += mj;
			}
		}
		return true;
	}

	// Rois
	if (is_king(piece)) {
		for (uint_fast8_t k = 0; k < 8; k++) {
			uint_fast8_t i2 = i + all_directions[k][0];
			uint_fast8_t j2 = j + all_directions[k][1];
			i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8 && (map->_array[i2][j2]++);
		}
		return true;
	}

	

	return false;
}

// Fonction qui renvoie la mobilité virtuelle d'un roi
int Board::get_king_virtual_mobility(bool color) {
	// FIXME: à faire que en fonction des pions??

	// On remplace le roi par une dame, et on regarde le nombre de coups possibles
	update_kings_pos();
	const int i = color ? _white_king_pos.row : _black_king_pos.row;
	const int j = color ? _white_king_pos.col : _black_king_pos.col;
	
	// On compte le nombre de coups possibles pour la nouvelle dame
	int mobility = 0;

	for (uint_fast8_t k = 0; k < 8; k++) {
		const uint_fast8_t mi = all_directions[k][0];
		const uint_fast8_t mj = all_directions[k][1];
		uint_fast8_t i2 = i + mi;
		uint_fast8_t j2 = j + mj;

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

// Fonction qui renvoie le nombre d'échecs 'safe' dans la position pour les deux joueurs
int Board::get_checks_value(Map white_controls, Map black_controls, bool color)
{
	constexpr int initial_safe_check_value = 250;
	constexpr int initial_unsafe_check_value = 25;
	constexpr float no_escape_multiplier = 2.5f;
	constexpr float inital_division = 1.0f;
	constexpr float king_escape_division_add = 0.25f;
	constexpr float piece_block_division_add = 0.75f;

	// Valeur augmentée si le joueur a le trait? A tester
	constexpr float has_trait_multiplier = 2.0f;

	//3r2k1/pp3r2/2q2pp1/3n3P/7Q/7R/1B5P/4R2K b - - 0 33 : il y a énormément de découvertes!

	int safe_checks_value = 0;
	int unsafe_checks_value = 0;

	// Position du roi adverse
	update_kings_pos();
	const Pos king_pos = color ? _black_king_pos : _white_king_pos;

	// On regarde tous les coups possibles pour le joueur
	Board b(*this);
	b._player = !color;
	if (b.in_check()) // FIXME?
		return 0;

	b._player = color;
	b.get_moves(true);

	for (uint_fast8_t i = 0; i < b._got_moves; i++) {
		
		// Coup
		const Move move = b._moves[i];

		// Case de destination du coup
		const uint_fast8_t i2 = move.end_row;
		const uint_fast8_t j2 = move.end_col;

		// Nombres de contrôles de la case de destination par les blancs et les noirs
		//const uint_fast8_t controls_ally = color ? white_controls._array[i2][j2] : black_controls._array[i2][j2];
		//const uint_fast8_t controls_enemy = color ? black_controls._array[i2][j2] : white_controls._array[i2][j2];

		// Si la destination du coup est non controlée par les blancs, ou qu'elle est seulement controllée par le roi blanc et une pièce noire (au moins)
		// FIXME: pas ouf
		//if (controls_enemy == 0 || (controls_enemy == 1 && controls_ally > 1 && abs(king_pos.i - i2) <= 1 && abs(king_pos.j - j2) <= 1)) {
		if (true) {
			// Joue le coup et regarde s'il fait échec
			Board b_check(b);
			//cout << "color: " << color << ", move: " << b_check.move_label(move) << endl;
			b_check.make_move(move);


			// TODO : à remplacer par 'est-ce que le coup attaque le roi'?
			if (b_check.in_check()) {
				b_check.get_moves(true); // FIMXE: BOF

				// Nombre d'échapatoires pour le roi
				int king_escapes = 0;

				// Nombre de pièces pouvant bloquer l'échec
				int piece_blocks = 0;

				// L'échec est-il safe?
				bool is_safe_check = true;

				for (uint_fast8_t j = 0; j < b_check._got_moves; j++) {
					// S'il existe une capture pour l'adversaire, l'échec n'est pas safe (?? faut-il que ça soit pas pièce faisant échec?) 
					// FIXME: bof
					uint_fast8_t eaten_piece = b_check._array[b_check._moves[j].end_row][b_check._moves[j].end_col];
					if (eaten_piece != none) {
						is_safe_check = false;
						break;
					}

					// Pièce pouvant empêcher l'échec
					uint_fast8_t piece = b_check._array[b_check._moves[j].start_row][b_check._moves[j].start_col];

					// Echapatoire pour le roi
					if (piece == (color ? b_king : w_king)) {
						king_escapes++;
					}
					else {
						piece_blocks++;
					}
				}

				// Valeur de la division
				float division = inital_division + king_escapes * king_escape_division_add + piece_blocks * piece_block_division_add;

				// Valeur de la multiplication
				float multiplier = (king_escapes == 0 && piece_blocks == 0) ? no_escape_multiplier : 1.0f;

				//cout << "is safe check: " << is_safe_check;

				if (is_safe_check) {
					// Ajoute la valeur de l'échec safe
					//cout << ", king_escapes : " << king_escapes << ", piece_blocks : " << piece_blocks << ", division : " << division << ", value : " << initial_safe_check_value / division << endl;
					safe_checks_value += max(multiplier * initial_safe_check_value / division, (float)initial_unsafe_check_value); // Un échec safe est toujours mieux qu'un échec unsafe
				}
				else {
					// Ajoute la valeur de l'échec unsafe
					//cout << "value : " << initial_unsafe_check_value << endl;
					unsafe_checks_value += initial_unsafe_check_value;
				}

			}
		}
	}

	return (safe_checks_value + unsafe_checks_value) * (_player == color ? has_trait_multiplier : 1.0f);
}

// Fonction qui renvoie la vitesse de génération des coups
[[nodiscard]] int Board::moves_generation_benchmark(uint_fast8_t depth, bool main_call)
{
	if (depth == 0)
		return 1;

	if (main_call) {
		for (uint_fast8_t i = 1; i <= depth; i++) {
			clock_t start = clock();
			cout << "Depth " << static_cast<int>(i) << ": ";
			int nodes_main = moves_generation_benchmark(i, false);
			int time = clock() - start;
			time = time == 0 ? 1 : time;
			cout << nodes_main << " nodes in " << time << "ms (" << int_to_round_string(nodes_main / time * 1000) << "N/s)" << endl;
		}

		return 0;
	}

	int nodes = 0;
	get_moves(true);

	for (uint_fast8_t i = 0; i < _got_moves; i++) {
		Board b(*this);
		b.make_index_move(i);
		nodes += b.moves_generation_benchmark(depth - 1, false);
	}

	return nodes;
}

// Fonction qui renvoie la valeur des fous en fianchetto (ou sur la grande diagonale, et à moins de 3 cases du bord)
int Board::get_fianchetto_value() const
{
	int_fast8_t fianchetti = 0;

	// r1bqk2r/ppp2ppp/2nbpn2/8/2pP4/5NP1/PP1NPPBP/R1BQ1RK1 b kq - 1 7 : après b5, ça devrait augmenter!

	// r1bq1rk1/pppnbppp/4p3/3pP3/3P4/6P1/PPP1P1BP/RNBQ1RK1 w - - 1 9 vs r1bq1rk1/pppnbppp/4p3/3pP3/3P4/5BP1/PPP1P2P/RNBQ1RK1 w - - 2 9 vs r1bq1rk1/pppnbppp/4p3/3pP3/3P4/6P1/PPP1P2P/RNBQ1RKB w - - 3 9

	// Valeur d'un fianchetto "normal" : au moins 3 cases accessibles sur la diagonale
	constexpr int base_fianchetto_squares = 1;

	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {

			// Pièce
			const uint_fast8_t piece = _array[row][col];

			// Si ça n'est pas un fou
			if (!is_bishop(piece))
				continue;

			// Si le fou n'est pas sur la grande diagonale
			if (row != col && row + col != 7)
				continue;

			int current_row = row, current_col = col;

			// On considère que le fou n'est pas au centre de l'échiquier...
			if (min(current_row, 7 - current_row) > 2)
				continue;

			// Sur laquelle des deux diagonales est le fou?
			bool diagonal = row == col;

			// Direction de la diagonale
			int d_row = 1;
			int d_col = diagonal ? 1 : -1;

			bool piece_color = piece < 7;

			// On retire la valeur de base du fianchetto
			piece_color ? fianchetti -= base_fianchetto_squares : fianchetti += base_fianchetto_squares;

			// On compte le nombre de cases sur la diagonale, et on s'arrête si on rencontre un pion

			// Vers le haut
			for (int k = current_row + d_row; k < 8; k += d_row) {
				current_row = k;
				current_col += d_col;
				
				// Si on rencontre un pion, on s'arrête
				if (_array[current_row][current_col] % 6 == w_pawn)
					break;

				//cout << current_row << ", " << current_col << ": " << square_name(current_row, current_col) << " ";

				piece_color ? fianchetti++ : fianchetti--;
			}

			// Vers le bas
			current_row = row;
			current_col = col;

			for (int k = current_row - d_row; k >= 0; k -= d_row) {
				current_row = k;
				current_col -= d_col;

				// Si on rencontre un pion, on s'arrête
				if (_array[current_row][current_col] % 6 == w_pawn)
					break;

				//cout << current_row << ", " << current_col << ": " << square_name(current_row, current_col) << " ";

				piece_color ? fianchetti++ : fianchetti--;
			}
		}
	}

	return fianchetti * 25.0f * (1.0f - _adv);
}

// Fonction qui renvoie si la case est controlée par un joueur -> Par le joueur adverse? à vérifier...
bool Board::is_controlled(int square_i, int square_j, bool player) const
{
	// Regarde les potentielles attaques de cavalier
	constexpr int knight_offsets[8][2] = { {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}, {1, -2}, {2, -1}, {2, 1}, {1, 2} };

	for (int k = 0; k < 8; k++) {
		const int ni = square_i + knight_offsets[k][0];

		// S'il y a un cavalier qui attaque, renvoie vrai (en échec)
		if (const int nj = square_j + knight_offsets[k][1]; ni >= 0 && ni < 8 && nj >= 0 && nj < 8 && _array[ni][nj] == (2 + player * 6))
			return true;
	}

	// Regarde les lignes horizontales et verticales

	// Gauche
	for (int j = square_j - 1; j >= 0; j--)
	{
		// Si y'a une pièce
		if (const uint_fast8_t piece = _array[square_i][j]; piece != none)
		{
			// Si la pièce n'est pas au joueur, regarde si c'est une tour, une dame, ou un roi avec une distance de 1
			if (piece < 7 != player)
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 4 || simple_piece == 5 || (simple_piece == 6 && j == square_j - 1))
					return true;

			break;
		}
	}

	// Droite
	for (int j = square_j + 1; j < 8; j++)
	{
		if (const uint_fast8_t piece = _array[square_i][j]; piece != 0)
		{
			if (piece < 7 != player)
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 4 || simple_piece == 5 || (simple_piece == 6 && j == square_j + 1))
					return true;

			break;
		}
	}

	// Haut
	for (int i = square_i - 1; i >= 0; i--)
	{
		if (const uint_fast8_t piece = _array[i][square_j]; piece != 0)
		{
			if (piece < 7 != player)
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 4 || simple_piece == 5 || (simple_piece == 6 && i == square_i - 1))
					return true;

			break;
		}
	}

	// Bas
	for (int i = square_i + 1; i < 8; i++)
	{
		if (const uint_fast8_t piece = _array[i][square_j]; piece != 0)
		{
			if (piece < 7 != player)
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 4 || simple_piece == 5 || (simple_piece == 6 && i == square_i + 1))
					return true;

			break;
		}
	}

	// Regarde les diagonales

	// Diagonale bas-gauche
	for (int i = square_i - 1, j = square_j - 1; i >= 0 && j >= 0; i--, j--)
	{
		if (const uint_fast8_t piece = _array[i][j]; piece != 0)
		{
			if (piece < 7 != player)
			{
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 3 || simple_piece == 5 || (simple_piece == 6 && (abs(square_i - i) == 1)))
					return true;

				// Cas spécial pour les pions
				if (piece == 1 && abs(square_j - j) == 1)
					return true;
			}

			break;
		}
	}

	// Diagonal bas-droite
	for (int i = square_i - 1, j = square_j + 1; i >= 0 && j < 8; i--, j++)
	{
		if (const uint_fast8_t piece = _array[i][j]; piece != 0)
		{
			if ((piece < 7) != player)
			{
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 3 || simple_piece == 5 || (simple_piece == 6 && abs(square_i - i) == 1))
					return true;

				// Special case for pawns
				if (piece == 1 && abs(square_j - j) == 1)
					return true;
			}
			break;
		}
	}

	// Diagonale haut-gauche
	for (int i = square_i + 1, j = square_j - 1; i < 8 && j >= 0; i++, j--)
	{
		if (const uint_fast8_t piece = _array[i][j]; piece != 0)
		{
			if ((piece < 7) != player)
			{
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 3 || simple_piece == 5 || (simple_piece == 6 && abs(square_i - i) == 1))
					return true;

				// Special case for pawns
				if (piece == 7 && abs(square_j - j) == 1)
					return true;
			}
			break;
		}
	}

	// Diagonale haut-droite
	for (int i = square_i + 1, j = square_j + 1; i < 8 && j < 8; i++, j++)
	{
		if (const uint_fast8_t piece = _array[i][j]; piece != 0)
		{
			if ((piece < 7) != player)
			{
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 3 || simple_piece == 5 || (simple_piece == 6 && abs(square_j - j) == 1))
					return true;

				// Special case for pawns
				if (piece == 7 && abs(square_j - j) == 1)
					return true;
			}
			break;
		}
	}

	return false;
}


// Fonction qui calcule et renvoie la valeur des menaces d'avance de pion
int Board::get_pawn_push_threats() const {

	// 1r3r2/1nqb2bk/3p2pp/3Ppp2/p1P5/2BB1N1P/3Q1PP1/R3R1K1 w - - 0 29 : e4 est une grosse menace
	// r4rk1/qp1bbppp/p3pn2/2Pp4/PP1Q1B2/2P2N2/3N1PPP/R3K2R w KQ - 3 15 : ici, ni e5 ni g5 sont vraiment jouables...

	// Pour chacun des pions, on regarde s'il y'a une ou des pièces qui seraient attaquées si le pion avançait
	// Il faut vérifier que rien ne bloque la poussée (ni une pièce, ni un contrôle adverse)
	// TODO améliorer les contrôles... car ça veut dire qu'il n'y a jamais de menaces contre un fou, même si le pion est protégé
	
	Map white_controls = get_white_controls_map();
	Map black_controls = get_black_controls_map();

	int w_threats = 0;
	int b_threats = 0;

	for (uint_fast8_t row = 1; row < 7; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			const uint_fast8_t p = _array[row][col];

			// Pion blanc
			if (p == w_pawn && row < 6) {

				// Le pion peut-il avancer de façon sécurisée? et de combien de cases?
				int safe_push = 0;

				// Si la case devant le pion est vide, et n'est pas controlée par un pion adverse, et que (si elle est controlée par l'adversaire, elle est aussi controlée par un pion allié)
				bool can_push = _array[row + 1][col] == none;
				bool opponent_pawn_control = (col > 0 && _array[row + 2][col - 1] == b_pawn) || (col < 7 && _array[row + 2][col + 1] == b_pawn);
				bool opponent_control = black_controls._array[row + 1][col] > 0;
				bool ally_pawn_control = (col > 0 && _array[row][col - 1] == w_pawn) || (col < 7 && _array[row][col + 1] == w_pawn);

				if (can_push && !opponent_pawn_control && (!opponent_control || ally_pawn_control)) {
					safe_push = 1;

					// Poussée double
					bool can_double_push = row == 1 && _array[row + 2][col] == none;
					bool opponent_pawn_control_double = (col > 0 && _array[row + 3][col - 1] == b_pawn) || (col < 7 && _array[row + 3][col + 1] == b_pawn);
					bool opponent_control_double = black_controls._array[row + 2][col] > 0;
					bool ally_pawn_control_double = (col > 0 && _array[row + 1][col - 1] == w_pawn) || (col < 7 && _array[row + 1][col + 1] == w_pawn);

					if (can_double_push && !opponent_pawn_control_double && (!opponent_control_double || ally_pawn_control_double)) {
						safe_push = 2;
					}
				}

				// Les poussées menacent-elles des pièces?
				if (safe_push > 0) {
					if (col > 0 && is_black(_array[row + 2][col - 1])) {
						w_threats++;
					}
					if (col < 7 && is_black(_array[row + 2][col + 1])) {
						w_threats++;
					}

					if (safe_push > 1) {
						if (col > 0 && is_black(_array[row + 3][col - 1])) {
							w_threats++;
						}
						if (col < 7 && is_black(_array[row + 3][col + 1])) {
							w_threats++;
						}
					}
				}
			}

			// Pion noir
			else if (p == b_pawn && row > 1) {

				// Le pion peut-il avancer de façon sécurisée? et de combien de cases?
				int safe_push = 0;

				// Si la case devant le pion est vide, et n'est pas controlée par un pion adverse, et que (si elle est controlée par l'adversaire, elle est aussi controlée par un pion allié)
				bool can_push = _array[row - 1][col] == none;
				bool opponent_pawn_control = (col > 0 && _array[row - 2][col - 1] == w_pawn) || (col < 7 && _array[row - 2][col + 1] == w_pawn);
				bool opponent_control = white_controls._array[row - 1][col] > 0;
				bool ally_pawn_control = (col > 0 && _array[row][col - 1] == b_pawn) || (col < 7 && _array[row][col + 1] == b_pawn);

				if (can_push && !opponent_pawn_control && (!opponent_control || ally_pawn_control)) {
					safe_push = 1;

					// Poussée double
					bool can_double_push = row == 6 && _array[row - 2][col] == none;
					bool opponent_pawn_control_double = (col > 0 && _array[row - 3][col - 1] == w_pawn) || (col < 7 && _array[row - 3][col + 1] == w_pawn);
					bool opponent_control_double = white_controls._array[row - 2][col] > 0;
					bool ally_pawn_control_double = (col > 0 && _array[row - 1][col - 1] == b_pawn) || (col < 7 && _array[row - 1][col + 1] == b_pawn);

					if (can_double_push && !opponent_pawn_control_double && (!opponent_control_double || ally_pawn_control_double)) {
						safe_push = 2;
					}
				}

				// Les poussées menacent-elles des pièces?
				if (safe_push > 0) {
					if (col > 0 && is_white(_array[row - 2][col - 1])) {
						b_threats++;
					}
					if (col < 7 && is_white(_array[row - 2][col + 1])) {
						b_threats++;
					}

					if (safe_push > 1) {
						if (col > 0 && is_white(_array[row - 3][col - 1])) {
							b_threats++;
						}
						if (col < 7 && is_white(_array[row - 3][col + 1])) {
							b_threats++;
						}
					}
				}
			}
		}
	}

	// En fonction de l'avancement
	constexpr float advancement_factor = 0.25f;

	return eval_from_progress(100 * (w_threats - b_threats), _adv, advancement_factor);
}

// Fonction qui calcule et renvoie la proximité du roi avec les pions
int Board::get_king_proximity()
{
	// TEST: 8/8/8/1k1K3p/6p1/6P1/7P/8 w - - 0 25
	// TEST: 8/8/3k2b1/1p5p/1P1K2p1/1B4P1/7P/8 w - - 6 12
	// 8/8/8/3k3p/5Kp1/6P1/7P/8 w - - 4 4 : quand on fait Rg5, ça devrait le rapprocher du pion non protégé...

	// TODO *** prendre en compte les cases controllées pour trouver le chemin le plus rapide... (pas facile à faire... mais ça peut beaucoup aider)

	// Met à jour la position des rois
	update_kings_pos();

	// Proximité des rois
	float proximity = 0.0f;

	// Bonus de proximité pour les pions adverse non protégés par un autre pion
	// TODO
	constexpr float unprotected_pawn_bonus = 2.5f;

	// Bonus de proximité pour les pions passés
	// TODO
	//constexpr float passed_pawn_bonus = 0.5f;

	// 8/8/8/4K2p/2k3p1/6P1/7P/8 w - - 2 26 : ??
	// 8/8/8/5k2/8/8/2p1p1p1/2R3K1 w - - 0 1 : ??

	// Pourcentage d'avancement pour que ça soit pris en compte
	const float min_advancement = 0.65f;

	if (_adv <= min_advancement)
		return 0;


	//int w_best_bonus = 0;
	//int b_best_bonus = 0;

	// 8/7p/p3K1k1/P4p2/5P1p/2p5/2P3P1/8 b - - 9 13 : pourquoi Rg7 augmente la proximité du roi noir?? (FIXED)

	// 6k1/p3r2p/3R3p/2p2N2/5P2/2P5/P5PP/6K1 b - - 0 31 : après Te2 et Td7...

	constexpr float innaccessibility_multiplier = 0.5f;

	constexpr float self_pawn_multiplier = 1.0f;

	Map white_king_distances = get_king_squares_distance(true);
	Map black_king_distances = get_king_squares_distance(false);

	for (uint_fast8_t row = 1; row < 7; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			const uint_fast8_t p = _array[row][col];

			// Pion blanc
			if (p == w_pawn) {
				//int w_distance = max(abs(row - _white_king_pos.row), abs(col - _white_king_pos.col));
				//int b_distance = max(abs(row - _black_king_pos.row), abs(col - _black_king_pos.col));

				float w_distance = white_king_distances._array[row][col];
				float b_distance = black_king_distances._array[row][col];

				// La case est-elle accessible?
				if (w_distance < 0) {
					w_distance = -w_distance;
				}

				if (b_distance < 0) {
					b_distance = -b_distance / innaccessibility_multiplier;
				}

				// Valeur du pion (augmente avec son avancement, s'il est non protégé, et s'il est passé)
				float w_pawn_value = sqrt(row) * self_pawn_multiplier;
				float b_pawn_value = 1;
				//float b_pawn_value = sqrt(row);

				// Plus gros malus en fonction de la ligne... à revoir.. Faire une vraie proximité plutôt que distance?...
				//8/8/8/4K2p/2k3p1/6P1/7P/8 w - - 2 26

				// Le pion est-il non-protégé par un autre pion?
				bool protected_pawn = (col > 0 && _array[row - 1][col - 1] == w_pawn) || (col < 7 && _array[row - 1][col + 1] == w_pawn);
				if (!protected_pawn) {
					//w_pawn_value *= unprotected_pawn_bonus;
					b_pawn_value *= unprotected_pawn_bonus;
				}

				float w_proximity = w_distance == 0.0f ? 0.0f : w_pawn_value / w_distance; // FIXME *** Faire un - plutôt qu'un / ?
				float b_proximity = b_distance == 0.0f ? 0.0f : b_pawn_value / b_distance;

				//cout << square_name(row, col) << ": " << "w_value: " << w_pawn_value << ", w_distance: " << w_distance << ", w_proximity: " << w_proximity << " / " << "b_value: " << b_pawn_value << ", b_distance: " << b_distance << ", b_proximity: " << b_proximity << endl;

				//8/8/8/6Kp/3k2p1/6P1/7P/8 b - - 5 27
				
				proximity += w_proximity;
				proximity -= b_proximity;

				//8/8/8/7p/3k1Kp1/6P1/7P/8 w - - 4 27

				// 8/3k4/3p1K2/p2P1p2/P2P1P2/8/8/8 b - - 27 14 : roi blanc le plus proche...
			}

			// Pion noir
			else if (p == b_pawn) {
				//int w_distance = max(abs(row - _white_king_pos.row), abs(col - _white_king_pos.col));
				//int b_distance = max(abs(row - _black_king_pos.row), abs(col - _black_king_pos.col));

				float w_distance = white_king_distances._array[row][col];
				float b_distance = black_king_distances._array[row][col];

				// La case est-elle accessible?
				if (w_distance < 0) {
					w_distance = -w_distance / innaccessibility_multiplier;
				}

				if (b_distance < 0) {
					b_distance = -b_distance;
				}

				// Valeur du pion (augmente avec son avancement, s'il est non protégé, et s'il est passé)
				//float w_pawn_value = sqrt(7 - row);
				float b_pawn_value = sqrt(7 - row) * self_pawn_multiplier;
				float w_pawn_value = 1;

				// Le pion est-il non-protégé par un autre pion?
				bool protected_pawn = (col > 0 && _array[row + 1][col - 1] == b_pawn) || (col < 7 && _array[row + 1][col + 1] == b_pawn);
				if (!protected_pawn) {
					w_pawn_value *= unprotected_pawn_bonus;
					//b_pawn_value *= unprotected_pawn_bonus;
				}

				float w_proximity = w_distance == 0.0f ? 0.0f : w_pawn_value / w_distance; // FIXME *** Faire un - plutôt qu'un / ?
				float b_proximity = b_distance == 0.0f ? 0.0f : b_pawn_value / b_distance;

				//cout << square_name(row, col) << ": " << "w_value: " << w_pawn_value << ", w_distance: " << w_distance << ", w_proximity: " << w_proximity << " / " << "b_value: " << b_pawn_value << ", b_distance: " << b_distance << ", b_proximity: " << b_proximity << endl;

				proximity += w_proximity;
				proximity -= b_proximity;
			}

		}
	}

	// Supprime les maps
	white_king_distances.~Map();
	black_king_distances.~Map();
	
	return 100 * proximity * (_adv - min_advancement) / (1.0f - min_advancement);
}


// Fonction qui calcule l'activité/mobilité des tours
int Board::get_rook_activity() const
{
	// Positions bug ou mal évaluées:
	// 2bq1k1r/br3p2/p2p2n1/npp1p2p/4P1pP/2PPN1B1/PPBN1PP1/R3QR1K w - - 0 1 : Rg1 augmente l'activité de la tour
	// r2qr1k1/pp3ppp/2nb1n2/4p3/2P3P1/1PNb3P/PB1PNPB1/R2QK2R w KQ - 0 12 : Th2 augmente son activité...
	// rnbqkb1r/ppp2ppp/4pn2/3p4/3P4/4PN2/PPP2PPP/RNBQKB1R w KQkq - 0 4 : il veut h4

	// r2qk2r/pb1pbpp1/1pn4n/2p1P2p/8/2NB1N1P/PP1BQPP1/2R1R1K1 b kq - 2 13 vs r2qk2r/pb1pbpp1/1pn4n/2p1P2p/8/2NB1N1P/PP1BQPP1/3RR1K1 b kq - 2 13

	// Cas de figure:
	// 1. Tour enfermée par le roi: mobilité < 4 -> malus (encore plus grand si le roi ne peut pas roquer) /= mobilité
	// 2. L'activité dépend surtout de la mobilité verticale (distance au pion le plus proche devant)

	// ça doit dimunuer en fonction du nombre de colonnes ouvertes...

	constexpr int vertical_mobility_bonus = 50;
	constexpr int horizontal_mobility_bonus = 15;

	// Bonus si elle attaque le camp adverse
	constexpr float row_bonus[8] = { 1.0f, 1.0f, 1.2f, 1.5f, 1.75f, 2.0f, 2.5f, 2.5f };

	// Malus pour manque de mobilité
	constexpr int bad_mobility_min = 3;
	constexpr int bad_mobility_malus = 350;

	// Valeur normale d'une activité de tour (4 en vertical et 8 en horizontal)
	constexpr int normal_activity = 8 * vertical_mobility_bonus + 8 * horizontal_mobility_bonus;


	float activity = 0.0f;

	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			const uint_fast8_t p = _array[row][col];

			// Tour blanche
			if (p == w_rook) {

				activity -= normal_activity;
				
				// Mobilité horizontale
				float h_mobility = 0.0f;

				// Vers la droite
				for (uint_fast8_t k = col + 1; k < 8; k++) {
					const uint_fast8_t p2 = _array[row][k];
					if (p2 != w_pawn && p2 != b_pawn && p2 != w_king)
						h_mobility += row_bonus[row];
					else
						break;
				}

				// Vers la gauche
				for (int_fast8_t k = col - 1; k >= 0; k--) {
					const uint_fast8_t p2 = _array[row][k];
					if (p2 != w_pawn && p2 != b_pawn && p2 != w_king)
						h_mobility += row_bonus[row];
					else
						break;
				}

				// Mobilité verticale
				float v_mobility = 0.0f;

				// Vers le haut
				for (uint_fast8_t k = row + 1; k < 8; k++) {
					const uint_fast8_t p2 = _array[k][col];
					if (p2 != w_pawn && p2 != b_pawn)
						v_mobility += row_bonus[k];
					else
						break;
				}

				// Vers le bas
				for (int_fast8_t k = row - 1; k >= 0; k--) {
					const uint_fast8_t p2 = _array[k][col];
					if (p2 != w_pawn && p2 != b_pawn)
						v_mobility += row_bonus[k];
					else
						break;
				}

				// Bonus pour la mobilité verticale
				activity += vertical_mobility_bonus * v_mobility;

				// Bonus pour la mobilité horizontale
				activity += horizontal_mobility_bonus * h_mobility;

				// Malus pour manque de mobilité
				const int total_mobility = h_mobility + v_mobility;
				if (total_mobility < bad_mobility_min)
					activity -= bad_mobility_malus;
					//activity -= bad_mobility_malus * (bad_mobility_min - total_mobility);

				//cout << "White rook (" << (int)i << ", " << (int)j << "): h_mobility = " << h_mobility << ", v_mobility = " << v_mobility << "total_mobility = " << total_mobility << ", total_activity = " << activity << endl;
			}

			//1r2q2k/4p1b1/p1n1Q1p1/1pp2p1p/3p4/3P1N1P/PPP1RPP1/4R1K1 b - - 3 26

			// Tour noire
			else if (p == b_rook) {

				activity += normal_activity;

				// Mobilité horizontale
				float h_mobility = 0.0f;

				// Vers la droite
				for (uint_fast8_t k = col + 1; k < 8; k++) {
					const uint_fast8_t p2 = _array[row][k];
					if (p2 != w_pawn && p2 != b_pawn && p2 != b_king)
						h_mobility += row_bonus[7 - row];
					else
						break;
				}

				// Vers la gauche
				for (int_fast8_t k = col - 1; k >= 0; k--) {
					const uint_fast8_t p2 = _array[row][k];
					if (p2 != w_pawn && p2 != b_pawn && p2 != b_king)
						h_mobility += row_bonus[7 - row];
					else
						break;
				}

				// Mobilité verticale
				float v_mobility = 0.0f;

				// Vers le haut
				for (uint_fast8_t k = row + 1; k < 8; k++) {
					const uint_fast8_t p2 = _array[k][col];
					if (p2 != w_pawn && p2 != b_pawn)
						v_mobility += row_bonus[7 - k];
					else
						break;
				}

				// Vers le bas
				for (int_fast8_t k = row - 1; k >= 0; k--) {
					const uint_fast8_t p2 = _array[k][col];
					if (p2 != w_pawn && p2 != b_pawn)
						v_mobility += row_bonus[7 - k];
					else
						break;
				}

				// Bonus pour la mobilité verticale
				activity -= vertical_mobility_bonus * v_mobility;

				// Bonus pour la mobilité horizontale
				activity -= horizontal_mobility_bonus * h_mobility;

				// Malus pour manque de mobilité
				const int total_mobility = h_mobility + v_mobility;
				if (total_mobility < bad_mobility_min)
					activity += bad_mobility_malus;
					//activity += bad_mobility_malus * (bad_mobility_min - total_mobility);

				//cout << "Black rook (" << (int)i << ", " << (int)j << "): h_mobility = " << h_mobility << ", v_mobility = " << v_mobility << "total_mobility = " << total_mobility << ", total_activity = " << activity << endl;
			}

		}
	}

	// Facteur multiplicatif en fonction de l'avancement de la partie
	float advancement_factor = 0.5f;

	return eval_from_progress(activity, _adv, advancement_factor);
}


// Opérateur d'égalité (compare seulement le placement des pièces, droits de roques, et l'en passant)
bool Board::operator== (const Board& b) const
{
	// Comparaison des pièces
	for (uint_fast8_t i = 0; i < 8; i++)
		for (uint_fast8_t j = 0; j < 8; j++)
			if (_array[i][j] != b._array[i][j])
				return false;

	// Comparaison des droits de roques
	/*if (_castling_rights != b._castling_rights)
		return false;*/

	// Comparaison de l'en passant
	/*if (_en_passant_col != b._en_passant_col)
		return false;*/

	return true;
}

// Fonction qui calcule et renvoie la valeur des pions qui bloquent les fous
int Board::get_bishop_pawns() const {

	// 3rr3/1kpn1p1p/p1p1qbb1/2Pp2p1/QP1Pp3/P1N1P1NP/3B1PP1/1R2R1K1 b - - 0 24 : eval pas symétrique...

	// Ajouter un bonus/malus en fonction de la couleur des pions adverses aussi?
	float ally_bishop_pawn_malus = 1.0f;
	float enemy_bishop_pawn_bonus = 1.0f * (1.0f - _adv); // En fin de partie, on veut pouvoir attaquer les pions adverses
	// r1b1k2r/1p1n1p2/p1pBp1pp/2Pp1q1n/PP1P4/4P3/3N1PPP/R2Q1RK1 w kq - 0 5

	// Pions blancs sur case blanche
	int white_pawns_w = 0;

	// Pions blancs sur case noire
	int white_pawns_b = 0;

	// Pions noirs sur case blanche
	int black_pawns_w = 0;

	// Pions noirs sur case noire
	int black_pawns_b = 0;

	// Nombre de pions blancs bloqués sur les colonnes centrales (C, D, E, F)
	int white_central_pawns_blocked = 0;

	// Nombre de pions noirs bloqués sur les colonnes centrales (C, D, E, F)
	int black_central_pawns_blocked = 0;

	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			const uint_fast8_t p = _array[row][col];

			// Pions blancs
			if (p == w_pawn) {
				if ((row + col) % 2)
					white_pawns_w++;
				else
					white_pawns_b++;

				if (_array[row + 1][col] != none && is_in(col, 2, 5))
					white_central_pawns_blocked++;
			}

			// Pions noirs
			else if (p == b_pawn) {
				if ((row + col) % 2)
					black_pawns_w++;
				else
					black_pawns_b++;

				if (_array[row - 1][col] != none && is_in(col, 2, 5))
					black_central_pawns_blocked++;
			}
		}
	}

	//cout << "white_pawns_w: " << white_pawns_w << endl;
	//cout << "white_pawns_b: " << white_pawns_b << endl;
	//cout << "black_pawns_w: " << black_pawns_w << endl;
	//cout << "black_pawns_b: " << black_pawns_b << endl;
	//cout << "white_pawns_blocked: " << white_central_pawns_blocked << endl;
	//cout << "black_pawns_blocked: " << black_central_pawns_blocked << endl;

	float bishop_pawns_value = 0.0f;

	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			const uint_fast8_t p = _array[i][j];

			// Fou blanc
			if (p == w_bishop) {
				if ((i + j) % 2) // Case blanche
					bishop_pawns_value -= (white_pawns_w - white_pawns_b) * (2 + white_central_pawns_blocked) * ally_bishop_pawn_malus + (black_pawns_w - black_pawns_b) * enemy_bishop_pawn_bonus;
				else // Case noire
					bishop_pawns_value -= (white_pawns_b - white_pawns_w) * (2 + white_central_pawns_blocked) * ally_bishop_pawn_malus + (black_pawns_b - black_pawns_w) * enemy_bishop_pawn_bonus;
			}

			// Fou noir
			else if (p == b_bishop) {
				if ((i + j) % 2) // Case blanche
					bishop_pawns_value += (black_pawns_w - black_pawns_b) * (2 + black_central_pawns_blocked) * ally_bishop_pawn_malus + (white_pawns_w - white_pawns_b) * enemy_bishop_pawn_bonus;
				else // Case noire
					bishop_pawns_value += (black_pawns_b - black_pawns_w) * (2 + black_central_pawns_blocked) * ally_bishop_pawn_malus + (white_pawns_b - white_pawns_w) * enemy_bishop_pawn_bonus;
			}
		}
	}

	//cout << "bishop_pawns_value: " << bishop_pawns_value << endl;

	// Facteur multiplicatif en fonction de l'avancement de la partie
	float advancement_factor = 0.5f;

	return eval_from_progress(bishop_pawns_value, _adv, advancement_factor);
}

// Fonction qui renvoie la valeur des faiblesses long terme du bouclier de pions
int Board::get_pawn_shield() {
	// Prendre en compte:
	// - la présence de pions devant le roi: DONE
	// - colonnes semi-ouvertes devant le roi: TODO
	// - pénalités pour pions doublés devant le roi, ou isolés devant le roi: TODO
	// - colonnes/diagonales ouvertes: TODO

	// si roqué (ne peut plus roquer): regarde les 3 pions devant lui
	// si peut roquer que d'un côté, regarde les pions f, g et h ou b, c et d (selon le côté)
	// si peut roquer des deux côtés, fais la moyenne des pions f, g et h et b, c et d, et des 3 pions devant lui

	// Exemples:
	// r3k2r/1ppq2pp/p1pbbpn1/8/3PP3/2N1BN1P/PP3PP1/R2Q1RK1 w kq - 3 13 : ici h4 pourrit la structure du roi. NE JAMAIS FAIRE (surtout en roques opposés)


	int pawn_shield_value = 0;

	update_kings_pos();

	// Roi blanc

	// pions f, g et h
	int w_kingside_pawns = 25 * ((_array[1][5] == 1) + (_array[1][6] == 1) + (_array[1][7] == 1)) + 15 * ((_array[2][5] == 1) + (_array[2][6] == 1) + (_array[2][7] == 1)) + 5 * ((_array[3][5] == 1) + (_array[3][6] == 1) + (_array[3][7] == 1));

	// pions b, c et d
	int w_queenside_pawns = 25 * ((_array[1][1] == 1) + (_array[1][2] == 1) + (_array[1][3] == 1)) + 15 * ((_array[2][1] == 1) + (_array[2][2] == 1) + (_array[2][3] == 1)) + 5 * ((_array[3][1] == 1) + (_array[3][2] == 1) + (_array[3][3] == 1));

	// pions devant le roi
	int w_front_pawns = 25 * (_array[1][_white_king_pos.col] == 1) + 15 * (_array[2][_white_king_pos.col] == 1) + 5 * (_array[3][_white_king_pos.col] == 1);
	if (_white_king_pos.col > 0)
		w_front_pawns += 25 * (_array[1][_white_king_pos.col - 1] == 1) + 15 * (_array[2][_white_king_pos.col - 1] == 1) + 5 * (_array[3][_white_king_pos.col - 1] == 1);
	if (_white_king_pos.col < 7)
		w_front_pawns += 25 * (_array[1][_white_king_pos.col + 1] == 1) + 15 * (_array[2][_white_king_pos.col + 1] == 1) + 5 * (_array[3][_white_king_pos.col + 1] == 1);

	int w_castles_count = _castling_rights.k_w + _castling_rights.q_w;

	int w_pawn_shield_value = (w_front_pawns + _castling_rights.k_w * w_kingside_pawns + _castling_rights.q_w * w_queenside_pawns) / (1 + w_castles_count);


	// Roi noir

	// pions f, g et h
	int b_kingside_pawns = 25 * ((_array[6][5] == 7) + (_array[6][6] == 7) + (_array[6][7] == 7)) + 15 * ((_array[5][5] == 7) + (_array[5][6] == 7) + (_array[5][7] == 7)) + 5 * ((_array[4][5] == 7) + (_array[4][6] == 7) + (_array[4][7] == 7));

	// pions b, c et d
	int b_queenside_pawns = 25 * ((_array[6][1] == 7) + (_array[6][2] == 7) + (_array[6][3] == 7)) + 15 * ((_array[5][1] == 7) + (_array[5][2] == 7) + (_array[5][3] == 7)) + 5 * ((_array[4][1] == 7) + (_array[4][2] == 7) + (_array[4][3] == 7));

	// pions devant le roi
	int b_front_pawns = 25 * (_array[6][_black_king_pos.col] == 7) + 15 * (_array[5][_black_king_pos.col] == 7) + 5 * (_array[4][_black_king_pos.col] == 7);
	if (_black_king_pos.col > 0)
		b_front_pawns += 25 * (_array[6][_black_king_pos.col - 1] == 7) + 15 * (_array[5][_black_king_pos.col - 1] == 7) + 5 * (_array[4][_black_king_pos.col - 1] == 7);
	if (_black_king_pos.col < 7)
		b_front_pawns += 25 * (_array[6][_black_king_pos.col + 1] == 7) + 15 * (_array[5][_black_king_pos.col + 1] == 7) + 5 * (_array[4][_black_king_pos.col + 1] == 7);

	int b_castles_count = _castling_rights.k_b + _castling_rights.q_b;

	int b_pawn_shield_value = (b_front_pawns + _castling_rights.k_b * b_kingside_pawns + _castling_rights.q_b * b_queenside_pawns) / (1 + b_castles_count);

	// Trous sur les colonnes


	// Calcul de la valeur du bouclier de pions
	pawn_shield_value = w_pawn_shield_value - b_pawn_shield_value;

	// A partir de quelle valeur de l'avancement de la partie, cela n'a plus d'importance (décroit linéairement)
	float pawn_shield_advancement_threshold = 0.7f;

	return eval_from_progress(pawn_shield_value, _adv, pawn_shield_advancement_threshold);
}

// Fonction qui renvoie la caleur des cases faibles
int Board::get_weak_squares(bool color, bool around_king) {
	// Case faible: case qui ne peut plus être protégée par un pion (= pas de pions sur une ligne inférieure sur les colonnes adjacentes), s'il n'y a pas de pion dessus
	// Bonus pour le contrôle de la case faible par un pion adverse
	// Bonus pour l'outpost d'un cavalier, d'un fou, ou d'une tour

	// TODO: il faut le moduler en fonction des pièces qui peuvent aller dessus...
	// Essayer un bonus en fonction de la distance d'une pièce vers la case?

	// r1bq2rk/2n4p/3p4/pNpPnp2/P1P1pN2/2Q5/4BPPP/1R3RK1 w - - 1 25 : ici peut-on considérer e5 comme étant une case faible pour les blancs?
	// rnbqkb1r/ppp2ppp/4pn2/3p4/3P4/4PN2/PPP2PPP/RNBQKB1R w KQkq - 0 4 : quand on fait h4, ça rajoute beaucoup à weak_squares... pourquoi??
	// 2r1brk1/6bp/p2pPpp1/qppN4/8/1P3NP1/P1Q2PP1/4RBK1 b - - 1 25 : grosse case faible en d5!!
	// r1bqkbnr/pppp1ppp/8/4p3/2PnP3/7P/PP1P1PP1/RNBQKBNR w KQkq - 1 4 : d4
	// r2qkb1r/pp1b1ppp/4p3/1PPp4/3QnP2/4P3/P1P3PP/RNB1KB1R w KQkq - 1 12 : e4 gratos...
	// 2r1brk1/6bp/p2pPpp1/qppN4/8/1P3NP1/P1Q2PP1/4RBK1 b - - 1 25 vs 2r1brk1/6bp/p2p1pp1/qppN4/8/1P3NP1/P1Q2PP1/4RBK1 b - - 1 25
	// 2r5/pp6/3pkp2/2p1p1p1/2P3Pp/3PPP1P/PP2K3/2R5 w - - 0 1 : beaucoup de cases faibles, mais aucune utilisable...
	// r1bq1rk1/pp1n1ppp/2pbpn2/3p4/3P4/2NBPN2/PPP2PPP/R1BQ1RK1 w - - 4 9 : pas vraiment de case faible...

	//https://chessmood.com/blog/weak-squares-in-chess

	bool display = false;

	// Valeur des cases faibles (point de vue du joueur ayant les cases faibles)
	constexpr static int weak_square_values[8][8] = {
		{ 0,  0,  0,  0,  0,  0,  0,  0},
		{ 0,  0,  5, 10, 10,  5,  0,  0}, 
		{ 0,  5, 10, 20, 20, 10,  5,  0},
		{ 5, 20, 35, 50, 50, 35, 20,  5},
		{ 5, 20, 40, 60, 60, 40, 20,  5},
		{ 5, 10, 15, 25, 25, 15, 10,  5},
		{ 0,  0,  0,  0,  0,  0,  0,  0},
		{ 0,  0,  0,  0,  0,  0,  0,  0}
	};

	// Valeur des outposts sur les cases faibles (point de vue du joueur ayant les cases faibles)
	constexpr static int outpost_square_values[8][8] = {
		{ 0,  0,   0,  0,  0,  0,  0,  0 },
		{ 0,  0,   0,  0,  0,  0,  0,  0 },
		{ 0,  0,   5, 20, 20,  5,  0,  0 },
		{ 0,  10, 30, 45, 45, 30, 10,  0 },
		{ 0,  15, 45, 50, 50, 45, 15,  0 },
		{ 0,  25, 30, 40, 40, 30, 25,  0 },
		{ 0,  5,  15, 25, 25, 15,  5,  0 },
		{ 0,  0,  0,   0,  0,  0,  0,  0 }
	};

	// Valeur des cases faibles proches du roi
	constexpr static int king_weak_square[3] = { 25, 12, 5 };

	// Valeur d'outpost proche du roi (décroit en fonction de la distance)
	constexpr static int king_outpost_square[3] = { 40, 30, 10 };

	// Valeur des outposts (pion, cavalier, fou, tour, dame, roi)
	constexpr static float outpost_values[6] = { 0.0f, 2.0f, 0.85f, 0.5f, 0.35f, 0.24f };

	// Si c'est simplement une case "sécurisée", mais pas faible à proprement parler
	constexpr static float safe_square_bonus = 0.3f;

	// Bonus s'il y a un pion adverse juste devant
	constexpr static float blocked_pawn_bonus = 1.65f;


	// Valeur des cases faibles
	int weak_squares_value = 0;

	// Contrôles des cases
	Map white_controls = get_white_controls_map();
	Map black_controls = get_black_controls_map();


	if (around_king) {
		update_kings_pos();
	}

	// Position du roi
	Pos king_pos = color ? _white_king_pos : _black_king_pos;

	// Cases faibles des blancs
	if (color) {

		if (display)
			cout << endl << "White weak squares:" << endl;

		// Pour chaque case
		for (uint_fast8_t row = 1; row < 7; row++) {
			for (uint_fast8_t col = 0; col < 8; col++) {

				if (_array[row][col] == w_pawn || _array[row][col] == b_pawn)
					continue;

				// Il n'y a pas de pion sur la case, on admet pour l'instant que c'est une case faible et sécurisée
				bool weak = true;
				bool safe = true;

				// Si ça n'est pas le bord gauche
				if (col > 0) {

					// On regarde la ligne du pion blanc le plus proche pouvant controller la case
					int can_control = -1;
					bool blocking_pawn = false;

					for (int_fast8_t k = row - 1; k > 0; k--) {
						if (_array[k][col - 1] == w_pawn) {
							weak = false;
							if (!blocking_pawn) {
								can_control = k;
								break;
							}
						}

						// Si un pion noir bloque le pion blanc
						if (_array[k][col - 1] == b_pawn) {
							blocking_pawn = true;
						}
					}

					// Un pion noir peut-il empêcher le pion blanc de contrôler la case faible?
					bool no_control = false;

					if (can_control != -1) {
						for (int_fast8_t k = row; k > can_control + 1; k--) {
							if (_array[k][col] == b_pawn || (col > 1 && _array[k][col - 2] == b_pawn)) {
								no_control = true;
								break;
							}
						}
					}

					// Un pion blanc peut contrôler la case faible, et un pion noir ne peut pas l'attaquer, donc ce n'est pas une case faible
					if (can_control != -1 && !no_control) {
						if (weak)
							cout << "shouldn't be weak here.. ?" << endl;
						weak = false;
						safe = false;
					}
				}

				if (!weak && !safe)
					continue;

				// Si ça n'est pas le bord gauche
				if (col < 7) {

					// On regarde la ligne du pion blanc le plus proche pouvant contrôler la case
					int can_control = -1;
					bool blocking_pawn = false;

					for (int_fast8_t k = row - 1; k > 0; k--) {
						if (_array[k][col + 1] == w_pawn) {
							weak = false;
							if (!blocking_pawn) {
								can_control = k;
								break;
							}
						}

						// Si un pion noir bloque le pion blanc
						if (_array[k][col + 1] == b_pawn) {
							blocking_pawn = true;
						}
					}

					// Un pion noir peut-il empêcher le pion blanc de contrôler la case faible?
					bool no_control = false;

					if (can_control != -1) {
						for (int_fast8_t k = row; k > can_control + 1; k--) {
							if (_array[k][col] == b_pawn || (col < 6 && _array[k][col + 2] == b_pawn)) {
								no_control = true;
								break;
							}
						}
					}

					// Un pion blanc peut contrôler la case faible, et un pion noir ne peut pas l'attaquer, donc ce n'est pas une case faible
					if (can_control != -1 && !no_control) {
						if (weak)
							cout << "shouldn't be weak here.. ?" << endl;
						weak = false;
						safe = false;
					}
				}

				if (!weak && !safe)
					continue;

				if (display)
					cout << "W weak square: " << Pos(row, col).square() << ": " << (weak ? "weak " : "safe");

				// C'est une case faible
				int square_value = 0;

				if (around_king) {
					int dist = max(abs(king_pos.row - row), abs(king_pos.col - col));
					if (dist > 0 && dist <= 3)
						square_value = king_weak_square[dist - 1];

					if (display)
						cout << " | d_king: " << dist;
				}
				else {
					square_value = weak_square_values[7 - row][col];
				}

				if (display)
					cout << " | value: " << square_value;

				// Contrôle de la case par un (des) pions adverses
				const int pawn_controls = (col > 0 && _array[row + 1][col - 1] == b_pawn) + (col < 7 && _array[row + 1][col + 1] == b_pawn);

				if (display)
					cout << " | pawn controls: " << pawn_controls;

				// Outposts
				if (pawn_controls > 0 || true) {

					// Valeur de l'outpost adverse
					int outpost_value = 0;

					if (around_king) {
						int dist = max(abs(king_pos.row - row), abs(king_pos.col - col));
						if (dist > 0 && dist <= 3)
							outpost_value = king_outpost_square[dist - 1];
					}
					else {
						outpost_value = outpost_square_values[7 - row][col];
					}

					const uint_fast8_t p = _array[row][col];

					// Valeur en fonction de la pièce
					float outpost_value_multiplier = is_black(p) ? outpost_values[p - 7] : 0.0f;

					square_value += outpost_value * outpost_value_multiplier;

					if (display)
						cout << " | outpost: " << outpost_value << " for piece: " << piece_name(p) << " -> *= " << outpost_value_multiplier;
				}

				// Valeur du contrôle de la case faible par un pion adverse
				square_value *= (1.00f + 0.75f * (pawn_controls > 0));

				square_value *= (weak ? 1.0f : safe_square_bonus);

				// Diminue la valeur de la faiblesse si elle est plus controlée par les blancs
				//int control_diff = max(0, white_controls._array[row][col] - black_controls._array[row][col]);
				int control_diff = white_controls._array[row][col];

				if (display)
					cout << " | control diff: " << control_diff;

				square_value /= (1.0f + control_diff / 1.0f);

				if (display)
					cout << " | with controls: " << square_value;

				// Bonus si ça bloque un pion
				if (_array[row - 1][col] == w_pawn)
					square_value *= blocked_pawn_bonus;

				if (display)
					cout << " | with blocked pawn: " << square_value << endl;

				weak_squares_value += square_value;
			}
		}
	}

	// r1bq1r2/pnp1ppbk/1p1p1npp/3P4/1P1NP3/2NBB3/P1PQ1PPP/R4RK1 b - - 2 12

	// Cases faibles des noirs
	else {

		if (display)
			cout << endl << "Black weak squares:" << endl << endl;

		// Pour chaque case
		for (uint_fast8_t row = 1; row < 7; row++) {
			for (uint_fast8_t col = 0; col < 8; col++) {

				if (_array[row][col] == w_pawn || _array[row][col] == b_pawn)
					continue;

				// Il n'y a pas de pion sur la case, on admet pour l'instant que c'est une case faible et sécurisée
				bool weak = true;
				bool safe = true;

				// Si ça n'est pas le bord gauche
				if (col > 0) {

					// On regarde la ligne du pion noir le plus proche pouvant controller la case
					int can_control = -1;
					bool blocking_pawn = false;

					for (int_fast8_t k = row + 1; k < 7; k++) {
						if (_array[k][col - 1] == b_pawn) {
							weak = false;
							if (!blocking_pawn) {
								can_control = k;
								break;
							}
						}

						// Si un pion blanc bloque le pion noir
						if (_array[k][col - 1] == w_pawn) {
							blocking_pawn = true;
						}
					}

					// Un pion blanc peut-il empêcher le pion noir de contrôler la case faible?
					bool no_control = false;

					if (can_control != -1) {
						for (int_fast8_t k = row; k < can_control - 1; k++) {
							if (_array[k][col] == w_pawn || (col > 1 && _array[k][col - 2] == w_pawn)) {
								no_control = true;
								break;
							}
						}
					}

					// Un pion noir peut contrôler la case faible, et un pion blanc ne peut pas l'attaquer, donc ce n'est pas une case faible
					if (can_control != -1 && !no_control) {
						if (weak)
							cout << "shouldn't be weak here.. ?" << endl;
						weak = false;
						safe = false;
					}
				}

				if (!weak && !safe)
					continue;

				// Si ça n'est pas le bord gauche
				if (col < 7) {

					// On regarde la ligne du pion noir le plus proche pouvant controller la case
					int can_control = -1;
					bool blocking_pawn = false;

					for (int_fast8_t k = row + 1; k < 7; k++) {
						if (_array[k][col + 1] == b_pawn) {
							weak = false;
							if (!blocking_pawn) {
								can_control = k;
								break;
							}
						}

						// Si un pion blanc bloque le pion noir
						if (_array[k][col + 1] == w_pawn) {
							blocking_pawn = true;
						}
					}

					// Un pion blanc peut-il empêcher le pion noir de contrôler la case faible?
					bool no_control = false;

					if (can_control != -1) {
						for (int_fast8_t k = row; k < can_control - 1; k++) {
							if (_array[k][col] == w_pawn || (col < 6 && _array[k][col + 2] == w_pawn)) {
								no_control = true;
								break;
							}
						}
					}

					// Un pion noir peut contrôler la case faible, et un pion blanc ne peut pas l'attaquer, donc ce n'est pas une case faible
					if (can_control != -1 && !no_control) {
						if (weak)
							cout << "shouldn't be weak here.. ?" << endl;
						weak = false;
						safe = false;
					}
				}

				if (!weak && !safe)
					continue;

				if (display)
					cout << "B weak square: " << Pos(row, col).square() << ": " << (weak ? "weak " : "safe");

				// C'est une case faible
				int square_value = 0;

				if (around_king) {
					int dist = max(abs(king_pos.row - row), abs(king_pos.col - col));
					if (dist > 0 && dist <= 3)
						square_value = king_weak_square[dist - 1];

					if (display)
						cout << " | d_king: " << dist;
				}
				else {
					square_value = weak_square_values[row][col];
				}

				if (display)
					cout << " | value: " << square_value;

				// Contrôle de la case par un (des) pions adverses
				const int pawn_controls = (col > 0 && _array[row - 1][col - 1] == w_pawn) + (col < 7 && _array[row - 1][col + 1] == w_pawn);

				if (display)
					cout << " | pawn controls: " << pawn_controls;

				// Outposts
				if (pawn_controls > 0 || true) {

					// Valeur de l'outpost adverse
					int outpost_value = 0;

					if (around_king) {
						int dist = max(abs(king_pos.row - row), abs(king_pos.col - col));
						if (dist > 0 && dist <= 3)
							outpost_value = king_outpost_square[dist - 1];
					}
					else {
						outpost_value = outpost_square_values[row][col];
					}

					const uint_fast8_t p = _array[row][col];

					// Valeur en fonction de la pièce
					float outpost_value_multiplier = is_white(p) ? outpost_values[p - 1] : 0.0f;

					square_value += outpost_value * outpost_value_multiplier;

					if (display)
						cout << " | outpost: " << outpost_value << " for piece: " << piece_name(p) << " -> *= " << outpost_value_multiplier;
				}

				// Valeur du contrôle de la case faible par un pion adverse
				square_value *= (1.00f + 0.75f * (pawn_controls > 0));

				square_value *= (weak ? 1.0f : safe_square_bonus);

				// Diminue la valeur de la faiblesse si elle est plus controlée par les noirs
				//int control_diff = max(0, black_controls._array[row][col] - white_controls._array[row][col]);
				int control_diff = black_controls._array[row][col];

				if (display)
					cout << " | control diff: " << control_diff;

				square_value /= (1.0f + control_diff / 1.0f);

				if (display)
					cout << " | with controls: " << square_value;

				// Bonus si ça bloque un pion
				if (_array[row + 1][col] == b_pawn)
					square_value *= blocked_pawn_bonus;

				if (display)
					cout << " | with blocked pawn: " << square_value << endl;

				weak_squares_value += square_value;
			}
		}
	}

	

	// Nombre de colonnes ouvertes
	int open_files = 0;

	// Pour chaque colonne
	for (uint_fast8_t j = 0; j < 8; j++) {
		bool open = true;

		// Pour chaque case
		for (uint_fast8_t i = 0; i < 8; i++) {
			if (_array[i][j] == w_pawn || _array[i][j] == b_pawn) {
				open = false;
				break;
			}
		}

		open_files += open;
	}

	//cout << "open files: " << open_files << endl;
	//cout << "total weak squares value: " << weak_squares_value << endl;
	//cout << "final value: " << weak_squares_value / (open_files / 2 + 1) << endl;

	// En fonction de l'avancement de la partie
	float advancement_factor = around_king ? 0.0f : 0.85f;

	//2r5/3r4/p1p1pk2/PpRnR2p/3P2p1/4P3/7P/1B5K b - - 5 37 : weak squares autour du roi: 200+??

	if (display)
		cout << "weak squares value: " << weak_squares_value << ", open files: " << open_files << ", advancement factor: " << advancement_factor << ", total: " << eval_from_progress(weak_squares_value / (open_files / 2.0f + 1), _adv, advancement_factor) << endl;
	
	return eval_from_progress(weak_squares_value / (open_files / 2.0f + 1), _adv, advancement_factor);
}

// Fonction qui convertit un coup en sa notation algébrique
string Board::algebric_notation(Move move) const {
	string move_notation = main_GUI._abc8[move.start_col] + to_string(move.start_row + 1) + main_GUI._abc8[move.end_col] + to_string(move.end_row + 1);

	// Promotion
	if (move.end_row == 7 && _array[move.start_row][move.start_col] == 1)
		move_notation += "q";
	else if (move.end_row == 0 && _array[move.start_row][move.start_col] == 7)
		move_notation += "q";

	return move_notation;
}

// Fonction qui convertit une notation algébrique en un coup
Move Board::move_from_algebric_notation(string notation) {
	return Move(notation[1] - '1', notation[0] - 'a', notation[3] - '1', notation[2] - 'a');
}

// Fonction qui renvoie la valeur de la distance à la possibilité de roque
int Board::get_castling_distance() const {
	// TODO: à fusionner avec d'autres fonctions pour que ça évite de donner un bonus constant même quand roquer nous fout dans la merde?

	// Regarde s'il y a des pièces qui bloquent le roque (et si elles peuvent bouger? TODO), ou des pièces adverses qui controllent

	// Malus de distance minimale au roque
	int castling_distance_malus = 30;

	// Blancs

	// Petit roque
	uint_fast8_t w_kingside_castle_distance = 0;

	// Si on peut encore roquer côté roi
	if (_castling_rights.k_w) {
		// Y'a -t-il des pièces qui bloquent le roque? (Si le fou est encore en f1 (à vérifier que c'est le fou...), rajoute du malus si qq chose bloque sa sortie)
		// TODO : à améliorer, car il peut y avoir des cas spéciaux...
		w_kingside_castle_distance += (_array[0][5] != 0) + (_array[0][6] != 0) + (_array[0][5] == w_bishop && _array[1][4] != 0 && _array[1][6] != 0);
		//cout << "w_kingside_castle_distance: " << (int)w_kingside_castle_distance << endl;

		// Y'a-t-il des pièces adverses qui contrôlent les cases?
		w_kingside_castle_distance += is_controlled(0, 5, true) + is_controlled(0, 6, true);
	}
	else {
		w_kingside_castle_distance = 2;
	}

	// Grand roque
	uint_fast8_t w_queenside_castle_distance = 0;

	// Si on peut encore roquer côté dame
	if (_castling_rights.q_w) {
		// Y'a -t-il des pièces qui bloquent le roque?
		w_queenside_castle_distance += (_array[0][1] != 0) + (_array[0][2] != 0) + (_array[0][3] != 0) + (_array[0][2] == w_bishop && _array[1][1] != 0 && _array[1][3] != 0);
		//cout << "w_queenside_castle_distance: " << (int)w_queenside_castle_distance << endl;

		// Y'a-t-il des pièces adverses qui contrôlent les cases?
		w_queenside_castle_distance += is_controlled(0, 2, true) + is_controlled(0, 3, true);
	}
	else {
		w_queenside_castle_distance = 2;
	}
	
	// Distance minimale pour roquer (2 s'il a déjà roqué)
	uint_fast8_t w_castling_distance = (_castling_rights.k_w || _castling_rights.q_w) ? min(w_kingside_castle_distance, w_queenside_castle_distance) : 1;


	// Noirs

	// Petit roque
	uint_fast8_t b_kingside_castle_distance = 0;

	// Si on peut encore roquer côté roi
	if (_castling_rights.k_b) {
		// Y'a -t-il des pièces qui bloquent le roque?
		b_kingside_castle_distance += (_array[7][5] != 0) + (_array[7][6] != 0) + (_array[7][5] == b_bishop && _array[6][4] != 0 && _array[6][6] != 0);
		//cout << "b_kingside_castle_distance: " << (int)b_kingside_castle_distance << endl;

		// Y'a-t-il des pièces adverses qui contrôlent les cases?
		b_kingside_castle_distance += is_controlled(7, 5, false) + is_controlled(7, 6, false);
	}
	else {
		b_kingside_castle_distance = 2;
	}

	// Grand roque
	uint_fast8_t b_queenside_castle_distance = 0;

	// Si on peut encore roquer côté dame
	if (_castling_rights.q_b) {
		// Y'a -t-il des pièces qui bloquent le roque?
		b_queenside_castle_distance += (_array[7][1] != 0) + (_array[7][2] != 0) + (_array[7][3] != 0) + (_array[7][2] == b_bishop && _array[6][1] != 0 && _array[6][3] != 0);
		//cout << "b_queenside_castle_distance: " << (int)b_queenside_castle_distance << endl;

		// Y'a-t-il des pièces adverses qui contrôlent les cases?
		b_queenside_castle_distance += is_controlled(7, 2, false) + is_controlled(7, 3, false);
	}
	else {
		b_queenside_castle_distance = 2;
	}

	// Distance minimale pour roquer (2 s'il a déjà roqué)
	uint_fast8_t b_castling_distance = (_castling_rights.k_b || _castling_rights.q_b) ? min(b_kingside_castle_distance, b_queenside_castle_distance) : 1;

	//cout << "w_castling_distance: " << (int)w_castling_distance << endl;
	//cout << "b_castling_distance: " << (int)b_castling_distance << endl;

	return castling_distance_malus * (b_castling_distance - w_castling_distance) * (1 - _adv);
}

// Fonction qui génère la clé de Zobrist du plateau (fonction pour le debug)
void Board::get_zobrist_key()
{
	// On part du principe que la clé ne sera jamais 0
	/*if (_zobrist_key != 0)
		return;*/

	// FIXME: elle est calculée plusieurs fois?

	Zobrist zobrist = transposition_table._zobrist;
	
	// Génération des clés de Zobrist si ce n'est pas déjà fait
	if (!zobrist._keys_generated)
		zobrist.generate_zobrist_keys();

	// Clé de Zobrist
	uint_fast64_t zobrist_key = zobrist._initial_key;

	// Clé de Zobrist pour les pièces
	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			// Numéro de la pièce
			uint_fast8_t p = _array[i][j];

			// Si la case n'est pas vide
			if (p != 0) {
				// Numéro de la case de la pièce
				uint_fast8_t square = i * 8 + j;

				// Zobrist associé
				zobrist_key ^= zobrist._board_keys[square][p];
			}
		}
	}

	// Clé de Zobrist pour les droits de roques
	uint_fast8_t castling_rights = _castling_rights.k_w + _castling_rights.q_w * 2 + _castling_rights.k_b * 4 + _castling_rights.q_b * 8;
	zobrist_key ^= zobrist._castling_keys[castling_rights];

	// Clé de Zobrist pour l'en passant
	if (_en_passant_col != -1)
		zobrist_key ^= zobrist._en_passant_keys[_en_passant_col];

	// Clé de Zobrist pour le trait
	if (_player == 1)
		zobrist_key ^= zobrist._player_key;

	_zobrist_key = zobrist_key;
}

// Fonction qui renvoie à quel point la partie est gagnable (de 0 à 1), pour une couleur
float Board::get_winnable(bool color) const {
	// TODO: à implémenter
	// Prendre en compte:
	// si la position est fermée
	// déséquilibre matériel
	// ENDGAME: matériel restant
	// si y'a des tours, c'est plus drawish
	// combien de pions il reste

	// EXEMPLES:
	// 2q5/k3bp2/p1p1b1p1/P1PpPp1p/1Pp2P1P/2Q3P1/6K1/3RR3 w - - 0 1 : ça c'est nulle (position fermée)

	return 1.0f;
}

// Fonction qui renvoie l'activité des fous sur les diagonales
int Board::get_bishop_activity() const {
	// Mobilité d'un fou = nombre de cases non-pion sur les diagonales du fou

	// Activité "normale" d'un fou
	constexpr int normal_bishop_activity = 3;

	// Bonus pour le fou blanc
	int w_bishop_activity = 0;

	// Bonus pour le fou noir
	int b_bishop_activity = 0;

	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			uint_fast8_t p = _array[i][j];

			// Fou blanc
			if (p == w_bishop) {

				w_bishop_activity -= normal_bishop_activity;

				// Diagonale haut-gauche
				for (uint_fast8_t k = 1; k < min(i, j) + 1; k++) {
					if (_array[i - k][j - k] != 1 && _array[i - k][j - k] != 7)
						w_bishop_activity++;
					else
						break;
				}

				// Diagonale haut-droite
				for (uint_fast8_t k = 1; k < min(i, 7 - j) + 1; k++) {
					if (_array[i - k][j + k] != 1 && _array[i - k][j + k] != 7)
						w_bishop_activity++;
					else
						break;
				}

				// Diagonale bas-gauche
				for (uint_fast8_t k = 1; k < min(7 - i, j) + 1; k++) {
					if (_array[i + k][j - k] != 1 && _array[i + k][j - k] != 7)
						w_bishop_activity++;
					else
						break;
				}

				// Diagonale bas-droite
				for (uint_fast8_t k = 1; k < min(7 - i, 7 - j) + 1; k++) {
					if (_array[i + k][j + k] != 1 && _array[i + k][j + k] != 7)
						w_bishop_activity++;
					else
						break;
				}
			}

			// Fou noir
			else if (p == b_bishop) {

				b_bishop_activity -= normal_bishop_activity;

				// Diagonale haut-gauche
				for (uint_fast8_t k = 1; k < min(i, j) + 1; k++) {
					if (_array[i - k][j - k] != 1 && _array[i - k][j - k] != 7)
						b_bishop_activity++;
					else
						break;
				}

				// Diagonale haut-droite
				for (uint_fast8_t k = 1; k < min(i, 7 - j) + 1; k++) {
					if (_array[i - k][j + k] != 1 && _array[i - k][j + k] != 7)
						b_bishop_activity++;
					else
						break;
				}

				// Diagonale bas-gauche
				for (uint_fast8_t k = 1; k < min(7 - i, j) + 1; k++) {
					if (_array[i + k][j - k] != 1 && _array[i + k][j - k] != 7)
						b_bishop_activity++;
					else
						break;
				}

				// Diagonale bas-droite
				for (uint_fast8_t k = 1; k < min(7 - i, 7 - j) + 1; k++) {
					if (_array[i + k][j + k] != 1 && _array[i + k][j + k] != 7)
						b_bishop_activity++;
					else
						break;
				}
			}
		}
	}

	// Facteur multiplicatif en fonction de l'avancement
	float bishop_activity_advancement_factor = 0.5f;

	// TODO: rendre ça non linéaire?

	return eval_from_progress(w_bishop_activity - b_bishop_activity, _adv, bishop_activity_advancement_factor);
}

// Fonction qui renvoie si un coup est légal ou non
bool Board::is_legal(Move move) {

	// Obtient les coups si nécessaire
	if (_got_moves == -1)
		get_moves();

	// Cherche l'index du coup
	for (int i = 0; i < _got_moves; i++)
		if (move == _moves[i])
			return true;

	return false;
}

// Fonction qui reset l'historique des positions
void Board::reset_positions_history() {
	_positions_history.clear();
}

// Fonction qui renvoie combien de fois la position actuelle a été répétée
int Board::repetition_count() {

	// FIXME: faut-il faire ça là? ça a peut-être déjà été fait...
	get_zobrist_key();

	return count(_positions_history.begin(), _positions_history.end(), _zobrist_key);
}

// Affiche l'histoirque des positions (les clés de Zobrist)
void Board::display_positions_history() const
{
	cout << "Positions history:" << endl;

	for (auto const& x : _positions_history)
	{
		cout << x << endl;
	}
}

// Fonction qui renvoie l'affichage de l'évaluation
[[nodiscard]] string Board::evaluation_to_string(int eval) const {
	string eval_string = "";

	if (eval > 0)
		eval_string += "+";

	// Est-ce que c'est un mat?
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

// Fonction qui renvoie l'évaluation des pièces enfermées
[[nodiscard]] int Board::get_trapped_pieces() const {
	// Pièce isolée: pièce éloignée des autres pièces alliées

	// TODO: adapter ça pour les endgames aussi? pour que le roi se rapproche des pions? pareil pour les chevaux...
	// TODO: ajouter un rayon variable pour le centre? pour savoir s'il est étendu ou non pour mieux évaluer si une pièce est effectivement isolée

	// TESTS
	//rn2kbnr/pp3ppp/4p3/2ppPb2/2PP4/4BN2/Pq2BPPP/RN1QK2R w KQkq - 0 8
	//8/1p4p1/p4p2/2k1p1p1/P1n1P3/2BK1P1P/6P1/8 b - - 0 42

	// 1r3rk1/p3bppp/2p3b1/1pPqN3/3P2P1/1PB1pP2/P5BP/R3Q1K1 w - - 0 23 : -200??

	// Poids par type de pièce
	const float pawn_weight = 1.0f;
	const float knight_weight = 4.0f; // Gros poids, car pièce de courte portée
	const float bishop_weight = 3.0f;
	const float rook_weight = 3.0f;
	const float queen_weight = 5.0f;
	const float king_weight = 0.0f; // Je sais pas trop s'il faut donner un poids ici, car il risque d'accumuler toutes ses pièces en défense?

	// Pièces blanches

	// Calcul du centre d'inertie
	float w_center_of_mass_i = 0.0f;
	float w_center_of_mass_j = 0.0f;
	float w_total_weight = 0.0f;

	// Pièces noires

	// Calcul du centre d'inertie
	float b_center_of_mass_i = 0.0f;
	float b_center_of_mass_j = 0.0f;
	float b_total_weight = 0.0f;

	// Pour chaque pièce
	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			uint_fast8_t p = _array[row][col];

			if (p == none)
				continue;

			// Si c'est une pièce blanche
			if (p <= w_king) {
				// Poids de la pièce
				float weight = p == w_pawn ? pawn_weight : (p == w_knight ? knight_weight : (p == w_bishop ? bishop_weight : (p == w_rook ? rook_weight : (p == w_queen ? queen_weight : king_weight))));

				//cout << "i: " << i << ", j: " << j << ", weight: " << weight << endl;

				// Centre de masse
				w_center_of_mass_i += row * weight;
				w_center_of_mass_j += col * weight;
				w_total_weight += weight;
			}

			// Si c'est une pièce noire
			else {
				// Poids de la pièce
				float weight = p == b_pawn ? pawn_weight : (p == b_knight ? knight_weight : (p == b_bishop ? bishop_weight : (p == b_rook ? rook_weight : (p == b_queen ? queen_weight : king_weight))));
				
				//cout << "i: " << i << ", j: " << j << ", weight: " << weight << endl;

				// Centre de masse
				b_center_of_mass_i += row * weight;
				b_center_of_mass_j += col * weight;
				b_total_weight += weight;
			}
		}
	}

	// Calcul du centre de masse
	w_center_of_mass_i /= w_total_weight;
	w_center_of_mass_j /= w_total_weight;

	b_center_of_mass_i /= b_total_weight;
	b_center_of_mass_j /= b_total_weight;

	//cout << "w_center_of_mass_i: " << w_center_of_mass_i << endl;
	//cout << "w_center_of_mass_j: " << w_center_of_mass_j << endl;
	//cout << "b_center_of_mass_i: " << b_center_of_mass_i << endl;
	//cout << "b_center_of_mass_j: " << b_center_of_mass_j << endl;


	// Distance des pièces au centre de masse
	// Il faut pénaliser les pièces loin du centre
	float min_distance = 3.0f;

	float w_trapped_pieces = 0.0f;
	float b_trapped_pieces = 0.0f;

	// Pénalité linéaire pour le moment?

	// Malus par type de pièce
	const float pawn_malus = 2.0f;
	const float knight_malus = 7.0f; // Gros poids, car pièce de courte portée
	const float bishop_malus = 7.0f;
	const float rook_malus = 9.0f;
	const float queen_malus = 15.0f;
	const float king_malus = 0.0f;

	const float attacked_factor = 3.0f;
	const float trapped_factor = 15.0f;

	// Avancement à partir duquel l'isolement ne compte plus (on garde néanmoins l'enfermement de pièces)
	const float max_adv = 0.6f;

	// Distance à partir de laquelle on considère la pièce comme équivalent "attaquée"
	const int attacked_distance = 10;

	// Facteur attenuant si la pièce peut en prendre une adverse en retour (TODO)

	const Map w_controls = get_white_controls_map();
	const Map b_controls = get_black_controls_map();

	//w_controls.print();
	//b_controls.print();
	//5rk1/Qbp2pp1/1pq1p2p/3p4/8/P1P1P3/2P2PPP/R3R1K1 w - - 0 21
	//rn2kbnr/pp3ppp/4p3/2ppP3/2PP4/1Q2BN2/P3BPPP/qR4K1 b kq - 0 10

	// Pour chaque pièce
	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			uint_fast8_t p = _array[row][col];

			if (p == none)
				continue;

			// Si c'est une pièce blanche
			if (is_white(p)) {

				// Distance au centre de masse
				float base_distance = sqrt(pow(row - w_center_of_mass_i, 2) + pow(col - w_center_of_mass_j, 2));

				float distance = max(0.0f, base_distance - min_distance);

				// Plus gros malus quand proche du camp advserse
				distance *= row + 1;

				// Pénalité
				const float malus = p == w_pawn ? pawn_malus : (p == w_knight ? knight_malus : (p == w_bishop ? bishop_malus : (p == w_rook ? rook_malus : (p == w_queen ? queen_malus : king_malus))));

				// Pénalité en fonction de la distance
				//float isolated_piece = distance * malus * max(1.0f - _adv / max_adv, 0.0f);
				float isolated_piece = 0.0f;

				//cout << Pos(row, col).square() << ", distance: " << distance << ", trapped_piece: " << trapped_piece;

				// Pénalité augmentée en fonction du peu de cases non-controllées où la pièce peut aller

				int safe_squares = 0;

				// Pion
				if (p == w_pawn) {
				}

				// Cavalier
				if (p == w_knight) {
					for (int k = 0; k < 8; k++) {
						int i2 = row + knight_directions[k][0];
						int j2 = col + knight_directions[k][1];

						if (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8)
							safe_squares += (b_controls._array[i2][j2] < w_controls._array[i2][j2]) && !is_white(_array[i2][j2]);
					}
				}
				
				// Pièces à mouvement rectiligne
				if (p == w_rook || p == w_queen) {
					for (int k = 0; k < 4; k++) {
						int i2 = row + rect_directions[k][0];
						int j2 = col + rect_directions[k][1];

						while (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8) {
							// S'il la case n'est pas controllée et qu'une pièce alliée ne la bloque pas
							safe_squares += (b_controls._array[i2][j2] < w_controls._array[i2][j2]) && !is_white(_array[i2][j2]);

							if (_array[i2][j2] != none) {
								break;
							}

							i2 += rect_directions[k][0];
							j2 += rect_directions[k][1];
						}
					}
				}

				// Pièces à mouvement diagonal
				if (p == w_bishop || p == w_queen) {
					for (int k = 0; k < 4; k++) {
						int i2 = row + diag_directions[k][0];
						int j2 = col + diag_directions[k][1];

						while (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8) {
							// S'il la case n'est pas controllée et qu'une pièce alliée ne la bloque pas
							safe_squares += (b_controls._array[i2][j2] < w_controls._array[i2][j2]) && !is_white(_array[i2][j2]);

							if (_array[i2][j2] != none) {
								break;
							}

							i2 += diag_directions[k][0];
							j2 += diag_directions[k][1];
						}
					}
				}

				// Roi
				if (p == w_king) {
					for (int k = 0; k < 8; k++) {
						int i2 = row + all_directions[k][0];
						int j2 = col + all_directions[k][1];

						if (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8)
							safe_squares += (b_controls._array[i2][j2] < w_controls._array[i2][j2]) && !is_white(_array[i2][j2]);
					}
				}

				// A quel point la pièce est enfermée
				const float trapped_malus = trapped_factor / ((safe_squares + 1) * (safe_squares + 1));

				bool attacked = b_controls._array[row][col] != 0;

				// Si la pièce est isolée, cela correspond presque à si elle était atttaquée...
				int attacked_bonus = min(attacked ? max(0.0f, base_distance * (row - 2)) : distance / (float)attacked_distance * attacked_factor / 2.0f, attacked_factor);

				//if (!is_pawn(p))
				//	cout << piece_name(p) << " on " << square_name(row, col) << ", safe_squares: " << safe_squares << ", trapped malus: " << trapped_malus << ", isolated_piece: " << isolated_piece << ", attacked: " << (b_controls._array[row][col] != 0) << ", distance: " << distance << ", attacked_bonus: " << attacked_bonus << endl;

				// On ne consière pas un pion comme "piégeable"
				if (p != w_pawn)
					isolated_piece += malus * trapped_malus * attacked_bonus;

				//if (!is_pawn(p))
				//	cout << "total: " << isolated_piece << endl;

				// TODO *** la pièce en question peut-elle s'échanger contre une autre pièce? Si oui, et pour une pièce de valeur équivalente (ou supérieure), alors elle n'est pas vraiment enfermée...
				// Ex: 1r3rk1/p3bppp/2p3b1/1pPqN3/3P2P1/1PB1pP2/P5BP/R3Q1K1 w - - 0 23 ici le cheval en e5 peut prendre le fou en g6, donc f6 n'est pas une menace

				w_trapped_pieces += isolated_piece;

				//2rq1rk1/1p1b1pp1/p1n2b1p/P2p3n/1P4P1/2PB1N1P/1Q1N1P1B/R2R2K1 b - g3 0 21 : cavalier enfermé en h5
				//rn2kbnr/pp3ppp/1q2p3/2ppPb2/2PP4/4BN2/PP2BPPP/RN1QK2R b KQkq - 0 7 : tests avec une dame enfermée

				// r1bqkb1Q/ppp5/2n1p1p1/3pP3/8/2P5/PP3PPP/RNB1K2R b KQq - 0 11 : la dame blanche n'est pas enfermée ici...
			}

			// Si c'est une pièce noire
			else {

				// Distance au centre de masse
				float base_distance = sqrt(pow(row - b_center_of_mass_i, 2) + pow(col - b_center_of_mass_j, 2));

				float distance = max(0.0f, base_distance - min_distance);

				// Plus gros malus quand proche du camp adverse
				distance *= 8 - row;

				// Pénalité
				const float malus = p == b_pawn ? pawn_malus : (p == b_knight ? knight_malus : (p == b_bishop ? bishop_malus : (p == b_rook ? rook_malus : (p == b_queen ? queen_malus : king_malus))));

				// Pénalité en fonction de la distance
				//float isolated_piece = distance * malus * max(1.0f - _adv / max_adv, 0.0f);
				float isolated_piece = 0.0f;

				//cout << Pos(row, col).square() << ", distance: " << distance << ", trapped_piece: " << trapped_piece;

				// Pénalité augmentée en fonction du peu de cases non-controllées où la pièce peut aller

				int safe_squares = 0;

				// Pion
				if (p == b_pawn) {
				}

				// Cavalier
				if (p == b_knight) {
					for (int k = 0; k < 8; k++) {
						int i2 = row + knight_directions[k][0];
						int j2 = col + knight_directions[k][1];

						if (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8)
							safe_squares += (w_controls._array[i2][j2] < b_controls._array[i2][j2]) && !is_black(_array[i2][j2]);
					}
				}

				// Pièces à mouvement rectiligne
				if (p == b_rook || p == b_queen) {
					for (int k = 0; k < 4; k++) {
						int i2 = row + rect_directions[k][0];
						int j2 = col + rect_directions[k][1];

						while (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8) {
							// S'il la case n'est pas controllée et qu'une pièce alliée ne la bloque pas
							safe_squares += (w_controls._array[i2][j2] < b_controls._array[i2][j2]) && !is_black(_array[i2][j2]);

							if (_array[i2][j2] != none) {
								break;
							}

							i2 += rect_directions[k][0];
							j2 += rect_directions[k][1];
						}
					}
				}

				// Pièces à mouvement diagonal
				if (p == b_bishop || p == b_queen) {
					for (int k = 0; k < 4; k++) {
						int i2 = row + diag_directions[k][0];
						int j2 = col + diag_directions[k][1];

						while (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8) {
							// S'il la case n'est pas controllée et qu'une pièce alliée ne la bloque pas
							safe_squares += (w_controls._array[i2][j2] < b_controls._array[i2][j2]) && !is_black(_array[i2][j2]);

							if (_array[i2][j2] != none) {
								break;
							}

							i2 += diag_directions[k][0];
							j2 += diag_directions[k][1];
						}
					}
				}

				// Roi
				if (p == b_king) {
					for (int k = 0; k < 8; k++) {
						int i2 = row + all_directions[k][0];
						int j2 = col + all_directions[k][1];

						if (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8)
							safe_squares += (w_controls._array[i2][j2] < b_controls._array[i2][j2]) && !is_black(_array[i2][j2]);
					}
				}

				const float trapped_malus = trapped_factor / ((safe_squares + 1) * (safe_squares + 1));

				bool attacked = w_controls._array[row][col] != 0;

				// Si la pièce est isolée, cela correspond presque à si elle était atttaquée...
				int attacked_bonus = min(attacked ? max(0.0f, base_distance * (5 - row)) : distance / (float)attacked_distance * attacked_factor / 2.0f, attacked_factor);

				//if (!is_pawn(p))
				//	cout << piece_name(p) << " on " << square_name(row, col) << ", safe_squares: " << safe_squares << ", trapped malus: " << trapped_malus << ", isolated_piece: " << isolated_piece << ", attacked: " << (b_controls._array[row][col] != 0) << ", distance: " << distance << ", attacked_bonus: " << attacked_bonus << endl;

				// On ne consière pas un pion comme "piégeable"
				if (p != b_pawn)
					isolated_piece += malus * trapped_malus * attacked_bonus;

				//if (!is_pawn(p))
				//	cout << "total: " << isolated_piece << endl;

				b_trapped_pieces += isolated_piece;

				//2kr1b2/pp2pp2/8/8/3p4/3P2B1/PPP1KPr1/R6R b - - 1 21
			}
		}
	}

	//cout << "w_trapped_pieces: " << w_trapped_pieces << endl;
	//cout << "b_trapped_pieces: " << b_trapped_pieces << endl;

	return (b_trapped_pieces - w_trapped_pieces);
}

// Fonction qui ajuste les valeurs des pièces (malus/bonus), en fonction du type de position
[[nodiscard]] int Board::get_updated_piece_values() const {
	// Malus pour les tours en fonction du nombre de colonnes non-ouvertes
	// Malus pour les fous si la position est fermée (diagonales non-ouvertes)
	// Pareil pour la dame. Bonus dans les cas contraires

	// *** TODO ***
	return 0;
}

// Fonction qui renvoie la nature de la position de manière chiffrée: 0 = ouverte, 1 = fermée
[[nodiscard]] float Board::get_position_nature() const {
	// Exemples à tester:
	// rnbqkbnr/8/p1p1p1p1/PpPpPpPp/1P1P1P1P/8/8/RNBQKBNR w KQkq - 1 13 : complètement fermée
	// r1bqkb1r/pp1n1ppp/2n1p3/2ppP3/3P1P2/2N1BN2/PPP3PP/R2QKB1R b KQkq - 3 7 : structure type française -> plutôt fermée
	// rnbq1rk1/ppp2ppp/3b1n2/3p4/3P4/3B1N2/PPP2PPP/RNBQ1RK1 w - - 6 7 : française d'échange -> plutôt équilibrée?
	// rnb1kb1r/ppp1pppp/3q1n2/8/3P4/2N2N2/PPP2PPP/R1BQKB1R b KQkq - 2 5 : scandi -> plutôt ouverte

	// Ne dépend que de la structure de pions

	// Facteurs rendant la position plus fermée
	// - Nombre de pions
	// - Nombre de pions bloqués

	// Facteurs rendant la position plus ouverte
	// - Nombre de colonnes ouvertes
	// - Nombre de diagonales ouvertes

	// Impact sur les autres paramètres d'évaluation:
	// Position ouverte:
	// Activité des pièces

	// Position fermée:
	// Avantage d'espace
	// Structure de pions
	// Placement des pièces


	// Facteur à partir duquel on considère la position comme complètement fermée 
	constexpr int completely_closed = 1100;

	// Facteur en dessous duquel on considère la position comme complètement ouverte
	constexpr int completely_open = 200;


	// Fermeture par pion
	constexpr int pawn_closed = 30;

	// Fermeture par pion bloqué
	constexpr int blocked_pawn_closed = 50;

	// Nombre de pions
	int pawns = 0;

	// Nombre de pions bloqués
	int blocked_pawns = 0;

	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			uint_fast8_t p = _array[row][col];

			if (p == w_pawn || p == b_pawn) {
				pawns++;

				// Si le pion est bloqué par un autre pion
				if (p == w_pawn && (_array[row + 1][col] == w_pawn || _array[row + 1][col] == b_pawn))
					blocked_pawns++;
				else if (p == b_pawn && (_array[row - 1][col] == w_pawn || _array[row - 1][col] == b_pawn))
					blocked_pawns++;
			}
		}
	}

	int range = (completely_closed - completely_open);

	float total_factor = pawn_closed * pawns + blocked_pawn_closed * blocked_pawns;

	float nature = (total_factor - completely_open) / range;

	return min(max(0.0f, nature), 1.0f);
}

// Fonction qui renvoie la probabilité de nulle de la position
[[nodiscard]] float Board::get_draw_chance() const {
	// *** TODO ***

	// Implémenter winnable(color) -> notamment pour les fins de partie
	return 0;
}

// Fonction qui renvoie la valeur des bonus liés aux colonnes ouvertes et semi-ouvertes sur le roi adverse
[[nodiscard]] int Board::get_open_files_on_opponent_king(bool player) {
	
	// Possibilités de roque
	bool can_kingside_castle = player ? _castling_rights.k_w : _castling_rights.k_b;
	bool can_queenside_castle = player ? _castling_rights.q_w : _castling_rights.q_b;

	int current_file_weakness = get_open_files_on_opponent_king_at_column(player, player ? _black_king_pos.col : _white_king_pos.col);
	int kingside_file_weakness = can_kingside_castle ? get_open_files_on_opponent_king_at_column(player, 6) : 0;
	int queenside_file_weakness = can_kingside_castle ? get_open_files_on_opponent_king_at_column(player, 2) : 0;

	// !player car les bonus de l'un sont les faiblesses de l'autre
	int total_weakness = get_long_term_king_weakness(!player, current_file_weakness, kingside_file_weakness, queenside_file_weakness);

	return total_weakness;
}

// Fonction qui renvoie la valeur des bonus liés aux diagonales ouvertes et semi-ouvertes sur le roi adverse
[[nodiscard]] int Board::get_open_diagonals_on_opponent_king(bool color) {
	// *** TODO: à fix?

	// Bonus pour les diagonales ouvertes et semi-ouvertes
	constexpr int open_diagonal_bonus = 60;
	constexpr int semi_open_diagonal_bonus = 25;

	// Facteur en fonction de la proximité avec la diagonale du roi adverse
	// Si le roi est sur la diagonale, le bonus est maximal
	constexpr float king_diagonal_bonus = 1.0f;

	// Si le roi est sur une diagonale adjacente, le bonus est réduit
	constexpr float king_adjacent_diagonal_bonus = 0.5f;

	// Bonus en plus pour les pièces présentes dessus (fous/dame)
	constexpr int bishop_bonus = 20;
	constexpr int queen_bonus = 15;

	update_kings_pos();

	// Diagonale du roi advserse
	uint_fast8_t king_i = color ? _black_king_pos.row : _white_king_pos.row;
	uint_fast8_t king_j = color ? _black_king_pos.col : _white_king_pos.col;

	// Bonus pour le joueur
	int total_bonus = 0;

	// Pion allié
	const int player_pawn = color ? w_pawn : b_pawn;

	// Pion adverse
	const int opponent_pawn = color ? b_pawn : w_pawn;

	// Pour chaque diagonale adjacente au roi noir
	for (int i = -1; i < 2; i += 2) {
		for (int j = -1; j < 2; j += 2) {

			// Coordonnées de la case
			uint_fast8_t new_i = king_i + i;
			uint_fast8_t new_j = king_j + j;

			// Si la case est en dehors de l'échiquier
			if (new_i < 0 || new_i > 7 || new_j < 0 || new_j > 7)
				continue;

			// Nature de la diagonale
			bool semi_open = true;
			bool open = true;

			// Parcours de la diagonale
			while (new_i >= 0 && new_i < 8 && new_j >= 0 && new_j < 8) {
				uint_fast8_t p = _array[new_i][new_j];

				if (p == player_pawn) {
					semi_open = false;
					open = false;
					break;
				}
				else if (p == opponent_pawn) {
					open = false;
				}

				// Prochaine case
				new_i += i;
				new_j += j;
			}

			//cout << "diagonal: " << (int)i << ", " << (int)j << ", open: " << open << ", semi_open: " << semi_open << endl;

			// Bonus
			int bonus = (open ? open_diagonal_bonus : (semi_open ? semi_open_diagonal_bonus : 0));

			// Bonus pour les pièces présentes sur la diagonale
			if (open || semi_open) {
				new_i = king_i + i;
				new_j = king_j + j;

				while (new_i >= 0 && new_i < 8 && new_j >= 0 && new_j < 8) {
					uint_fast8_t p = _array[new_i][new_j];

					if (p == (color ? w_bishop : b_bishop))
						bonus += bishop_bonus * (1 + open);
					else if (p == (color ? w_queen : b_queen))
						bonus += queen_bonus * (1 + open);

					new_i += i;
					new_j += j;
				}
			}

			// Bonus en fonction de la proximité du roi
			bonus *= (i == 0 && j == 0 ? king_diagonal_bonus : king_adjacent_diagonal_bonus);

			total_bonus += bonus;
		}
	}

	// En fonction de l'avancement de la partie
	constexpr float advancement_factor = 0.0f;

	return eval_from_progress(total_bonus, _adv, advancement_factor);
}

// Fonction qui renvoie le nombre de cases de retrait pour le roi
[[nodiscard]] int Board::get_king_escape_squares(bool color) {

	// Contrôle des cases par les pièces adverses
	Map control_map = color ? get_black_controls_map() : get_white_controls_map();

	// Position du roi
	update_kings_pos();

	Pos king_pos = color ? _white_king_pos : _black_king_pos;

	// Nombre de cases de retrait
	int escape_squares = 0;

	// Pour chaque case autour du roi
	for (int i = -1; i < 2; i++) {
		for (int j = -1; j < 2; j++) {

			// Coordonnées de la case
			uint_fast8_t new_i = king_pos.row + i;
			uint_fast8_t new_j = king_pos.col + j;

			// Si la case est en dehors de l'échiquier
			if (new_i < 0 || new_i > 7 || new_j < 0 || new_j > 7)
				continue;

			// S'il y a une pièce alliée sur la case
			uint_fast8_t p = _array[new_i][new_j];
			if (p != 0 && (color ? p <= w_king : p <= b_king))
				continue;


			// Si la case est contrôlée par une pièce adverse
			if (control_map._array[new_i][new_j] != 0)
				continue;

			escape_squares++;
		}
	}

	return escape_squares;
}

// Fonction qui renvoie une valeur correspondante aux pièces attaquant le roi adverse
[[nodiscard]] int Board::get_king_attackers(bool color) {
	// Pour les sliding pieces: regarde simplement sur la ligne/colonne/diagonale: s'il y a un pion qui bloque: est-ce un pion à proximité du roi? sinon: est-ce que il contrôle des cases du roi?

	// FIXME: faut-il compter seulement le nombre de pièces?
	// Faut-il avoir une valeur différente pour chaque type de pièce?
	// Faut-il compter en fonction de la distance avec le roi?

	// TODO: prendre en compte distance 2??

	//6rk/1p3p1p/2nN1q2/2Q2p2/3p4/PP5P/5PP1/2R3K1 b - - 1 28 : la dame attaque quand on la met en e6??
	//6rk/1p3p1p/2nNq3/2Q2p2/3p4/PP5P/5PP1/2R3K1 w - - 2 29 : bug?
	//r1bqk2r/pppp1ppp/2n5/2b1p3/2BPP1n1/5N2/PPP2P1P/RNBQ1RK1 b kq - 0 6 ??
	//r1br2k1/pp2Rp2/6nB/7Q/3p4/8/5PP1/6K1 b - - 0 5 : ici 300??
	//6R1/5p2/5kp1/2q5/pp4B1/2n1R3/5PKP/8 b - - 5 45 : ici 700??
	//1r6/7p/p1P1p3/4kp2/1P1Rp3/4KPP1/8/8 b - - 0 49 ...
	//rnb4r/ppppbk1p/5n2/6Q1/8/2N1p3/PPP3PP/5RK1 w - - 4 16 : Cd5 ajoute une grosse pièce en attaque!!
	//1rbq1r2/2p2pk1/p2p1nn1/4p1N1/p3P2p/2PPP3/RPB3PP/3QBRK1 b - - 1 2 : h3 diminue l'attaque des noirs...? car ça retire le fou de la case h3?

	// rn1q1rkn/pb2bpp1/1ppp4/5P2/3P4/2N2B2/PPP3PP/R1BQR1K1 b - - 4 14 vs rn1q1rkn/pb2bpp1/1ppp4/5P2/3P4/2N2B1R/PPP3PP/R1BQ2K1 b - - 4 14

	// 2r5/3r4/p1p1pk2/PpRnR3/3P2pp/4P3/7P/1B5K w - - 0 38

	// Valeur d'une pièce attaquant le roi adverse
	constexpr int attacking_value[7] = { 0, 100, 110, 114, 117, 119, 120 };

	// Valeur d'attaque d'une pièce attaquant la couronne lointaine du roi
	constexpr int semi_attack_value = 40;

	// Facteur d'attaque par pièce (pion, cavalier, fou, tour, dame, roi)
	constexpr float piece_attack_factor[6] = { 0.35f, 1.35f, 1.25f, 1.45f, 1.75f, 1.15f };

	// Facteur d'attaque en fonction de la distance au roi


	// Met à jour la position des rois
	update_kings_pos();

	// Position du roi
	Pos king_pos = color ? _black_king_pos : _white_king_pos;
	Pos opponent_king_pos = color ? _white_king_pos : _black_king_pos;

	// Nombre de contrôles sur le roi
	int king_attackers = 0;

	//1k1rr3/1pp1q3/pnn1b3/4p3/3pP1p1/PP1P3p/1BPNN2K/R3QR1B b - - 1 46

	// Regarde chaque pièce alliée sur l'échiquier
	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			uint_fast8_t p = _array[row][col];

			uint_fast8_t attacks = 0;
			uint_fast8_t semi_attacks = 0;

			// Pion
			if (p == (color ? w_pawn : b_pawn)) {

				// Cases contrôlées par le pion
				uint_fast8_t di = abs(row + (color ? 1 : -1) - king_pos.row);
				uint_fast8_t dj1 = abs(col - 1 - king_pos.col);
				uint_fast8_t dj2 = abs(col + 1 - king_pos.col);

				uint_fast8_t p2a = _array[row + (color ? 1 : -1)][col - 1];

				// Si le pion contrôle une case du roi
				if (col > 0 && di <= 2 && dj1 <= 2 && p2a != (color ? w_pawn : b_pawn)) {
					semi_attacks++;
					if (di <= 1 && dj1 <= 1) {
						attacks++;
					}
				}

				uint_fast8_t p2b = _array[row + (color ? 1 : -1)][col + 1];

				if (col < 7 && di <= 2 && dj2 <= 2 && p2b != (color ? w_pawn : b_pawn)) {
					semi_attacks++;
					if (di <= 1 && dj2 <= 1) {
						attacks++;
					}
				}
			}

			// Cavalier
			if (p == (color ? w_knight : b_knight)) {
				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_i = row + knight_directions[m][0];
					int new_j = col + knight_directions[m][1];

					if (!is_in(new_i, 0, 7) || !is_in(new_j, 0, 7))
						continue;

					uint_fast8_t p2 = _array[new_i][new_j];

					// La case ne peut pas être attaquée
					if (p2 == (color ? w_pawn : b_pawn))
						continue;

					uint_fast8_t di = abs(new_i - king_pos.row);
					uint_fast8_t dj = abs(new_j - king_pos.col);

					// Si le cavalier contrôle une case du roi
					if (di <= 2 && dj <= 2) {
						semi_attacks++;
						if (di <= 1 && dj <= 1) {
							attacks++;
						}
					}
				}
			}

			// Pièces à mouvement rectiligne
			if ((p == (color ? w_rook : b_rook)) || (p == (color ? w_queen : b_queen))) {

				for (uint_fast8_t m = 0; m < 4; m++) {

					// La pièce est-elle obstruée par une autre pièce dans cette direction?
					bool blocked = false;

					int mi = rect_directions[m][0];
					int mj = rect_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint_fast8_t p2 = _array[new_i][new_j];

						uint_fast8_t di = abs(new_i - king_pos.row);
						uint_fast8_t dj = abs(new_j - king_pos.col);

						// La case ne peut pas être attaquée
						if (p2 == (color ? w_pawn : b_pawn))
							break;

						// Si la pièce contrôle une case du roi
						if (di <= 2 && dj <= 2) {
							semi_attacks++;
							if (di <= 1 && dj <= 1 && !blocked) {
								attacks++;
							}
						}

						// Si un pion bloque la case
						if (p2 == w_pawn || p2 == b_pawn)
							break;

						// Si une pièce bloque la case
						if (p2 != none) {
							blocked = true;
						}

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// Pièces à mouvement diagonal
			if ((p == (color ? w_bishop : b_bishop)) || (p == (color ? w_queen : b_queen))) {

				for (uint_fast8_t m = 0; m < 4; m++) {

					// La pièce est-elle obstruée par une autre pièce dans cette direction?
					bool blocked = false;

					int mi = diag_directions[m][0];
					int mj = diag_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint_fast8_t p2 = _array[new_i][new_j];

						uint_fast8_t di = abs(new_i - king_pos.row);
						uint_fast8_t dj = abs(new_j - king_pos.col);

						// La case ne peut pas être attaquée
						if (p2 == (color ? w_pawn : b_pawn))
							break;

						// Si la pièce contrôle une case du roi
						if (di <= 2 && dj <= 2) {
							semi_attacks++;
							if (di <= 1 && dj <= 1 && !blocked) {
								attacks++;
							}
						}

						// Si un pion bloque la case
						if (p2 == w_pawn || p2 == b_pawn)
							break;

						// Si une pièce bloque la case
						if (p2 != none) {
							blocked = true;
						}

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// Roi
			if (p == (color ? w_king : b_king)) {
				for (int i = -1; i < 2; i++) {
					for (int j = -1; j < 2; j++) {
						int new_i = i + opponent_king_pos.row;
						int new_j = j + opponent_king_pos.col;

						if (!is_in(new_i, 0, 7) || !is_in(new_j, 0, 7))
							continue;

						uint_fast8_t p2 = _array[new_i][new_j];

						// La case ne peut pas être attaquée
						if (p2 == (color ? w_pawn : b_pawn))
							break;

						uint_fast8_t di = abs(new_i - king_pos.row);
						uint_fast8_t dj = abs(new_j - king_pos.col);

						// Si le roi contrôle une case du roi
						if (di <= 2 && dj <= 2) {
							semi_attacks++;
							if (di <= 1 && dj <= 1) {
								attacks++;
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
					king_attackers += semi_attack_value * piece_attack_factor[(p - 1) % 6] * pow(semi_attacks, 0.3);
					//cout << "color: " << color << ", piece: " << piece_name(p) << "(" << square_name(row, col) << "), semi-attacks : " << (int)semi_attacks << ", value : " << semi_attack_value * piece_attack_factor[(p - 1) % 6] * pow(semi_attacks, 0.3) << endl;
				}
			}
		}
	}

	//cout << "king_attackers: " << king_attackers << endl;

	//1r1qr3/5p1k/3p1Ppb/p2N3p/2pPP2P/2Pn3B/P3QR2/5RK1 w - - 1 22

	return king_attackers;
}

[[nodiscard]] int Board::get_king_defenders(bool color) {
	// Pour les sliding pieces: regarde simplement sur la ligne/colonne/diagonale: s'il y a un pion qui bloque: est-ce un pion à proximité du roi? sinon: est-ce que il contrôle des cases du roi?

	// r1b2b1r/ppN3pp/1k6/2p5/3Q1B2/8/PP3PPP/n1R3K1 w - - 0 20 : ici y'a pas beaucoup de défenseurs pour les noirs

	// 5r2/1pk3p1/2pr3p/p1n2P2/2PN4/2P1pBP1/6KP/1R1R4 b - - 0 29 : y'a r

	// Valeur d'une pièce défendant le roi adverse
	constexpr int defending_value[9] = { 0, 100, 110, 115, 119, 122, 125, 128, 130 };

	// Valeur de défense d'une pièce défendant la couronne lointaine du roi
	constexpr int semi_defense_value = 30;

	// Facteur de défense par pièce (pion, cavalier, fou, tour, dame, roi)
	constexpr float piece_defense_factor[6] = { 0.5f, 1.5f, 1.35f, 1.3f, 1.15f, 1.35f };

	// La dame est un très mauvais défenseur, puisqu'elle est très facilement exposée

	// Met à jour la position des rois
	update_kings_pos();

	// Position du roi
	Pos king_pos = color ? _white_king_pos : _black_king_pos;

	// Nombre de contrôles sur le roi
	int king_defenders = 0;

	// Regarde chaque pièce alliée sur l'échiquier
	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			uint_fast8_t p = _array[row][col];

			uint_fast8_t defenses = 0;
			uint_fast8_t semi_defenses = 0;

			// Pion : TODO à revoir...
			if (p == (color ? w_pawn : b_pawn)) {

				// Cases contrôlées par le pion
				uint_fast8_t di = abs(row + (color ? 1 : -1) - king_pos.row);
				uint_fast8_t dj1 = abs(col - 1 - king_pos.col);
				uint_fast8_t dj2 = abs(col + 1 - king_pos.col);

				// Si le pion contrôle une case du roi
				if (col > 0 && di <= 2 && dj1 <= 2) {
					semi_defenses++;
					if (di <= 1 && dj1 <= 1) {
						defenses++;
					}
				}

				if (col < 7 && di <= 2 && dj2 <= 2) {
					semi_defenses++;
					if (di <= 1 && dj2 <= 1) {
						defenses++;
					}
				}
			}

			// Cavalier
			if (p == (color ? w_knight : b_knight)) {
				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_i = row + knight_directions[m][0];
					int new_j = col + knight_directions[m][1];

					if (!is_in(new_i, 0, 7) || !is_in(new_j, 0, 7))
						continue;

					uint_fast8_t di = abs(new_i - king_pos.row);
					uint_fast8_t dj = abs(new_j - king_pos.col);

					// Si le cavalier contrôle une case du roi
					if (di <= 2 && dj <= 2) {
						semi_defenses++;
						if (di <= 1 && dj <= 1) {
							defenses++;
						}
					}
				}
			}

			// Pièces à mouvement rectiligne
			if ((p == (color ? w_rook : b_rook)) || (p == (color ? w_queen : b_queen))) {

				for (uint_fast8_t m = 0; m < 4; m++) {
					int mi = rect_directions[m][0];
					int mj = rect_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint_fast8_t p2 = _array[new_i][new_j];

						uint_fast8_t di = abs(new_i - king_pos.row);
						uint_fast8_t dj = abs(new_j - king_pos.col);

						// Si la pièce contrôle une case du roi
						if (di <= 2 && dj <= 2) {
							semi_defenses++;
							if (di <= 1 && dj <= 1) {
								defenses++;
							}
						}

						// Si une pièce bloque la case
						if (p2 != none)
							break;

						// Si 

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// Pièces à mouvement diagonal
			if ((p == (color ? w_bishop : b_bishop)) || (p == (color ? w_queen : b_queen))) {

				for (uint_fast8_t m = 0; m < 4; m++) {
					int mi = diag_directions[m][0];
					int mj = diag_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint_fast8_t p2 = _array[new_i][new_j];

						uint_fast8_t di = abs(new_i - king_pos.row);
						uint_fast8_t dj = abs(new_j - king_pos.col);

						// Si la pièce contrôle une case du roi
						if (di <= 2 && dj <= 2) {
							semi_defenses++;
							if (di <= 1 && dj <= 1) {
								defenses++;
							}
						}

						// Si un pion bloque la case
						if (p2 == w_pawn || p2 == b_pawn)
							break;

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// Roi
			if (p == (color ? w_king : b_king)) {
				for (int i = -1; i < 2; i++) {
					for (int j = -1; j < 2; j++) {

						int new_i = i + king_pos.row;
						int new_j = j + king_pos.col;

						if (i == 0 && j == 0)
							continue;

						if (!is_in(new_i, 0, 7) || !is_in(new_j, 0, 7))
							continue;

						uint_fast8_t di = abs(new_i - king_pos.row);
						uint_fast8_t dj = abs(new_j - king_pos.col);

						// Si le roi contrôle une case du roi
						if (di <= 2 && dj <= 2) {
							semi_defenses++;
							if (di <= 1 && dj <= 1) {
								defenses++;
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
				else if (semi_defenses > 0) { // TODO: à améliorer en prenant en compte le nombre de semi-défenses?
					king_defenders += semi_defense_value * piece_defense_factor[(p - 1) % 6];
					//cout << "color: " << color << ", piece: " << piece_name(p) << "(" << square_name(row, col) << "), semi-defenses : " << (int)semi_defenses << ", value : " << semi_defense_value * piece_defense_factor[(p - 1) % 6] << endl;
				}
			}
		}
	}

	//cout << "total defenders: " << king_defenders << endl;

	return king_defenders;
}

// Fonction qui renvoie un bonus correspondant au pawn storm sur le roi adverse
[[nodiscard]] int Board::get_pawn_storm(bool color) {
	// FIXME: faut-il un bonus si le pion a déjà passé le roi adverse? en soit, cela veut dire que le roi adverse est sur une colonne "ouverte" sans vraiment être ouverte?
	// FIXME: est-ce qu'une pièce adverse bloque le pawn storm? ou seulement les pions?

	// 3rr3/1kpn1p1p/p1p1qbb1/2Pp2p1/QP1Pp3/P1N1P1NP/3B1PP1/1R2R1K1 b - - 0 24
	// 1k1r2nr/p7/Pqpb2p1/4p2p/5p2/2NP4/1PP1QPKP/R1B1R3 b - - 0 22 : +333??

	// r2qk2r/pp1n1ppp/3bpn2/2pp4/3P4/2N1PN2/PPP2PPP/R1BQ1RK1 b kq - 3 8 : ici y'a roque au prochain, donc pas vraiment de storm

	// Position du roi
	update_kings_pos();

	Pos opponent_king_pos = color ? _black_king_pos : _white_king_pos;

	// Bonus en fonction de la distance verticale entre les pions et le roi
	int bonus[7] = { 85, 75, 55, 38, 21, 10, 0};

	// Bonus des pions sur une colonne quasi adjacente
	float semi_adjacent_bonus = 0.25f;

	int total_bonus = 0;

	// Regarde sur les trois colonnes adjacentes au roi
	for (int_fast8_t col = opponent_king_pos.col - 2; col <= opponent_king_pos.col + 2; col++) {
		if (col < 0 || col > 7) {
			continue;
		}

		for (uint_fast8_t row = 1; row < 7; row++) {

			// Si y'a un pion allié
			if (_array[row][col] == (color ? w_pawn : b_pawn)) {
				//cout << (int)i << ", " << opponent_king_pos.i << endl;
				//cout << abs(i - opponent_king_pos.i) << endl;

				// S'il n'y a pas de pion adverse qui le bloque
				int dy = (color ? 1 : -1);

				uint_fast8_t p = _array[row + dy][col];

				bool blocked = p == w_pawn || p == b_pawn;
				//cout << "piece : " << (int)p << endl;

				// On vérifie que rien ne débloque le pion (prise possible)
				if (blocked) {
					if (col > 1) {
						uint_fast8_t p1 = _array[row + dy][col - 1];
						if (color && is_black(p1) || !color && is_white(p1)) {
							blocked = false;
						}
					}

					if (col < 7) {
						uint_fast8_t p2 = _array[row + dy][col + 1];
						if (color && is_black(p2) || !color && is_white(p2)) {
							blocked = false;
						}
					}
				}

				//cout << square_name(row, col) << ", blocked: " << blocked << endl;

				// 5r2/5pk1/r1pp1qn1/4p2p/4P3/2P1QpB1/BP2b1PP/R5RK w - - 6 29 : f3 considéré comme semi storm?
				//1rbq1r2/2p2pk1/p2p1nn1/4p1N1/p3P2p/2PPP3/RPB3PP/3QBRK1 b - - 1 7 : ici h3 fait un storm...

				// FIXME??
				if (!blocked) { // En théorie, l'indice ne devrait pas sortir de [|0, 7|] puisque les pions ne peuvent pas se situer sur les lignes extrëmes
					total_bonus += bonus[abs(row - opponent_king_pos.row)] * (abs(col - opponent_king_pos.col) < 2 ? 1.0f : semi_adjacent_bonus);
				}
			}
		}
	}

	// En fonction de l'avancement
	float pawn_storm_advancement_factor = 0.0f;

	return eval_from_progress(total_bonus, _adv, pawn_storm_advancement_factor);
}

// Fonction qui renvoie le nom de la case
string square_name(uint_fast8_t row, uint_fast8_t col) {
	return string(1, 'a' + col) + string(1, '1' + row);
}

// Fonction qui renvoie le nom d'une pièce
string piece_name(uint_fast8_t piece) {
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

// Fonction qui renvoie un bonus d'activité pour les cavaliers
int Board::get_knight_activity() const {

	// Bonus par case contrôlée
	constexpr int control_bonus[8][8] = 
	{	{ 3, 4, 6, 6, 6, 6, 4, 3 },
		{ 4, 6, 8,10,10, 8, 6, 4 },
		{ 5, 7,10,12,12,10, 7, 5 },
		{ 4, 6, 8,10,10, 8, 6, 4 },
		{ 3, 5, 6, 8, 8, 6, 5, 3 },
		{ 2, 3, 4, 5, 5, 4, 3, 2 },
		{ 1, 2, 3, 4, 4, 3, 2, 1 },
		{ 1, 2, 2, 3, 3, 2, 2, 1 } };

	int white_knight_bonus = 0;
	int black_knight_bonus = 0;

	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			if (_array[i][j] == w_knight) {
				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_i = i + knight_directions[m][0];
					int new_j = j + knight_directions[m][1];

					if (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						white_knight_bonus += control_bonus[7 - new_i][new_j];
					}
				}
			}

			if (_array[i][j] == b_knight) {
				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_i = i + knight_directions[m][0];
					int new_j = j + knight_directions[m][1];

					if (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						black_knight_bonus += control_bonus[new_i][new_j];
					}
				}
			}
		}
	}

	// Facteur multiplicatif en fonction de l'avancement
	float knight_activity_advancement_factor = 0.5f;

	return eval_from_progress(white_knight_bonus - black_knight_bonus, _adv, knight_activity_advancement_factor);
}

// Fonction qui renvoie la puissance de protection de la structure de pions du roi
int Board::get_pawn_shield_protection(bool color, float opponent_attacking_potential) {

	// Position du roi
	update_kings_pos();

	// TODO: Cas spécifique quand il n'est pas roqué: on estime ses positions possibles après le roque (colonne c ou g), et on en déduit la puissance de protection de la structure de pions

	Pos king_pos = color ? _white_king_pos : _black_king_pos;
	
	bool can_kingside_castle = color ? _castling_rights.k_w : _castling_rights.k_b;
	bool can_queenside_castle = color ? _castling_rights.q_w : _castling_rights.q_b;

	const int main_bonus = get_pawn_shield_protection_at_column(color, king_pos.col, opponent_attacking_potential, true);
	const int kingside_bonus = can_kingside_castle ? get_pawn_shield_protection_at_column(color, 6, opponent_attacking_potential, true) : 0;
	const int queenside_bonus = can_queenside_castle ? get_pawn_shield_protection_at_column(color, 2, opponent_attacking_potential, true) : 0;

	// 4k1r1/2pp4/1p2pq2/6r1/p2P2Pp/2PB1b1P/PPQ5/R4RK1 b - g3 0 24
	// r3k2r/pppnbppp/2n5/3N1q2/5B2/8/PPPQBP1P/1K1R2R1 b kq - 2 15
	// rnb1kb1r/ppp2ppp/5n2/3qp3/8/5N2/PPPPBPPP/RNBQK2R w KQkq - 0 5

	//cout << "main: " << main_bonus << ", kingside: " << kingside_bonus << ", queenside: " << queenside_bonus << endl;

	// TODO *** prendre en compte les distances aux roques
	// FIXME *** c'est pas forcément idéal, car ça ne prend pas en compte le reste de la position à part la structure pour évaluer si roquer est une bonne idée...

	//uint_fast8_t king_row = color ? 0 : 7;
	//uint_fast8_t above_row = color ? 1 : 6;
	//uint_fast8_t blocking_bishop = color ? w_bishop : b_bishop;

	//// Distance au petit roque
	//uint_fast8_t kingside_castle_distance = 0;

	//if (can_kingside_castle) {
	//	kingside_castle_distance = (_array[king_row][5] != none) + (_array[king_row][6] != none)
	//		+ (_array[king_row][5] == blocking_bishop && _array[above_row][4] != none && _array[above_row][6] != none)
	//		+ is_controlled(king_row, 5, color) + is_controlled(king_row, 6, color);
	//}

	//// Distance au grand roque
	//uint_fast8_t queenside_castle_distance = 0;

	//if (can_queenside_castle) {
	//	queenside_castle_distance = (_array[king_row][3] != none) + (_array[king_row][2] != none) + (_array[king_row][1] != none)
	//		+ (_array[king_row][2] == blocking_bishop && _array[above_row][1] != none && _array[above_row][3] != none)
	//		+ is_controlled(king_row, 2, color) + is_controlled(king_row, 3, color);
	//}
	//
	////cout << "kingside: " << (int)kingside_castle_distance << ", queenside: " << (int)queenside_castle_distance << endl;

	//// TODO *** peut être améliorable si d'autres pièces que le fou de base bloquent le roque

	//// TOTAL: main / C + K / (Dk + c) + Q / (Dq + c)

	//int total_bonus = main_bonus;

	//// Gain potentiel pour les roques
	//const int kingside_bonus_diff = max(0, kingside_bonus - main_bonus);
	//const int queenside_bonus_diff = max(0, queenside_bonus - main_bonus);
	//
	//// FIXME *** un peu aribitraire... 0.9 quand on peut roquer, et se rapproche linéairement de 0 sinon. En partant du principe que la distance ne sera pas au delà de 8...
	//// Fonction largement améliorable
	//double kingside_castling_factor = can_kingside_castle ? 1.0 - (2.0 + kingside_castle_distance) / 20.0 : 0.0;
	//double queenside_castling_factor = can_queenside_castle ? 1.0 - (2.0 + queenside_castle_distance) / 20.0 : 0.0;

	//int total_kingside_bonus = kingside_bonus_diff * kingside_castling_factor;
	//int total_queenside_bonus = queenside_bonus_diff * queenside_castling_factor;

	//int best_castle_bonus = max(total_kingside_bonus, total_queenside_bonus);

	//total_bonus += best_castle_bonus;

	//// 2rqk2r/ppb4p/2n1ppn1/3p4/1P1P4/P3BN1P/4BPP1/R2Q1RK1 b k - 2 16 : ??

	//// FIXME: il peut y avoir un cas particulier qui le rend pourri:
	//// Quand les structures aile roi et dame son cassées, le roi est au centre avec une meilleure structure: il va essayer de perdre ses droits de roque
	////const int total_bonus = (main_bonus + kingside_bonus + queenside_bonus) / (1 + kingside_castle + queenside_castle);
	//// FIXED??

	//cout << "total: " << total_bonus << endl;

	//cout << "main: " << main_bonus << ", kingside: " << kingside_bonus << ", queenside: " << queenside_bonus << endl;

	int total_bonus = -get_long_term_king_weakness(color, -main_bonus, -kingside_bonus, -queenside_bonus);

	//cout << "total: " << total_bonus << endl;

	//rnbqkb1r/pppp2pp/4pn2/5p2/4P3/5N2/PPPPBPPP/RNBQK2R w KQkq - 0 4

	return total_bonus;
}

// Fonction qui renvoie la puissance de protection de la structure de pions du roi, s'il est sur la colonne donnée
int Board::get_pawn_shield_protection_at_column(bool color, int column, float opponent_attacking_potential, bool add_column_bonus) {

	// r1bqr1k1/pp3pp1/2pp1b1p/8/4P3/2N1Q3/PPP2PPP/1K2RB1R b - - 1 15 ??????

	// Niveau de protection auquel on peut considérer que le roi est safe
	//const int king_base_protection = 600 * (1 - _adv) - 200;
	int needed_protection = 800 * opponent_attacking_potential - 300;

	// Bonus de protection selon la colonne
	constexpr int column_protection_bonus[8] = { 65, 75, 50, 0, 0, 50, 75, 65 };

	if (add_column_bonus)
		needed_protection -= column_protection_bonus[column];

	// Position du roi
	update_kings_pos();

	Pos king_pos = color ? _white_king_pos : _black_king_pos;
	king_pos.col = column;

	// Structure des pions sur les colonnes adjactentes
	bool pawns[8][5] = { false };

	// Aucun pion sur la colonne du roi
	bool open_files[5] = { false, false, false, false, false };

	// Pour chaque colonne adjacente au roi
	for (int_fast8_t col = king_pos.col - 2; col <= king_pos.col + 2; col++) {
		if (col < 0 || col > 7)
			continue;

		open_files[col - king_pos.col + 2] = true;

		// Pour chaque pion allié sur la colonne
		for (uint_fast8_t row = 0; row < 8; row++) {
			if (_array[row][col] == (color ? w_pawn : b_pawn)) {
				pawns[row][col - king_pos.col + 2] = true;
				open_files[col - king_pos.col + 2] = false;
			}
		}
	}

	// r1b1qk1r/ppppb1pp/5n2/n3NQ2/3PP3/2P5/P4PPP/RNB1K2R w KQ - 5 11

	// Bonus pour la connexion des pions entre eux
	constexpr int connected_pawns_bonus = 75;

	// Malus pour une colonne ouverte
	constexpr int open_file_malus = 50;

	// Bonus pour chaque pion proche du roi
	constexpr int pawn_bonus = 75;

	// Facteur multiplicatif en fonction de la distance verticale
	constexpr float distance_factors[7] = { 1.0f, 1.0f, 0.55f, 0.25f, 0.10f, 0.05f, 0.02f };

	// Facteur multiplicatif si le roi est devant les pions
	constexpr float king_in_front_factor = 0.5f;

	// Facteur multiplicatif pour les pions à 2 colonnes de distance
	constexpr float semi_adjacent_factor = 0.25f;


	// Bonus pour les pions connectés
	float connected_pawns_bonus_total = 0.0f;

	// Pour chaque pion
	float pawns_bonus_total = 0.0f;

	// b2r2k1/2p3pn/p7/3p2Pp/2P4P/2N1B2q/P3Q3/5RK1 w - - 2 32


	// En général, toute poussée devant le roi est affaiblissante
	//r1bqr1k1/ppp1bppp/2np1n2/4p3/2B1P1P1/3P3P/PPP2P2/RNBQNRK1 b - - 2 8
	// vs
	//r1bqr1k1/ppp1bppp/2np1n2/4p3/2B1P1P1/3P1P1P/PPP5/RNBQNRK1 b - - 1 10
	//rnbq1b1r/ppp2kpp/3p1n2/8/4P3/2N5/PPPP1PPP/R1BQKB1R b KQ - 1 5 : g7 reste une protection

	//r2qr1k1/pp1n1pp1/2pbbp1p/3p4/3P4/P1NBPN1P/1PPQ1PP1/R4RK1 w - - 2 12 : changement quand on joue h4
	// r2qr1k1/pp1n1pp1/2pbbp1p/3p4/3P3P/P1NBPNP1/1PPQ1PK1/R4R2 b - - 2 14

	// r1bq1rk1/ppp2ppp/2n2n2/3p4/1bPP4/1PN3P1/P3NPBP/R1BQ1RK1 b - - 2 10 : ??

	for (uint_fast8_t col = 0; col < 5; col++) {
		for (uint_fast8_t row = 0; row < 8; row++) {

			if (pawns[row][col]) {

				// Direction du pion
				int dir = color ? 1 : -1;
				int distance = abs(row - king_pos.row);

				float distance_factor = distance_factors[distance];

				// Si le roi est devant les pions
				if (king_pos.row * dir > row * dir) {
					distance_factor *= king_in_front_factor;
				}

				// Factor (semi-adjacent or adjacent)
				float adjacent_factor = ((col == 0 || col == 4) ? semi_adjacent_factor : 1.0f);


				// REVIEW *** on considère les pions non soutenus mais attachés à un autre comme connecté quand-même...
				// En fait non?
				if ((col > 0 && (pawns[row][col - 1] || pawns[row - dir][col - 1] || pawns[row + dir][col - 1])) || (col < 4 && (pawns[row][col + 1] || pawns[row - dir][col + 1] || pawns[row + dir][col + 1]))) {
				//if ((col > 0 && (pawns[row][col - 1] || pawns[row - dir][col - 1])) || (col < 2 && (pawns[row][col + 1] || pawns[row - dir][col + 1]))) {
					//cout << "connected pawns on " << square_name(row, col + king_pos.col - 2) << " : " << connected_pawns_bonus * distance_factor << endl;
					connected_pawns_bonus_total += connected_pawns_bonus * distance_factor * adjacent_factor;
				}

				//cout << "pawn bonus on " << square_name(row, col + king_pos.col - 2) << " : " << pawn_bonus * distance_factor << endl;

				// Colonne adjacente ou semi-adjacente
				int bonus = pawn_bonus * distance_factor * adjacent_factor;

				pawns_bonus_total += pawn_bonus * distance_factor;
			}
		}
	}

	// rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w kq - 4 4

	//cout << "connected pawns: " << connected_pawns_bonus_total << endl;
	//cout << "pawns: " << pawns_bonus_total << endl;

	// Colonnes ouvertes
	int open_files_total = 0;

	for (uint_fast8_t col = 0; col < 5; col++) {
		if (open_files[col]) {
			//cout << "open file on " << square_name(king_pos.row, col + king_pos.col - 2) << endl;
			open_files_total += open_file_malus * ((col == 0 || col == 4) ? semi_adjacent_factor : 1.0f);
		}
	}

	//cout << "files: " << open_files_total << endl;

	// Protection par les bords de l'échiquier

	// Bord vertical
	constexpr int v_edge_protection = 20;

	// Bord horizontal
	constexpr int h_edge_protection = 25;

	const int edge_protection = v_edge_protection * (min(king_pos.row, 7 - king_pos.row) == 0) + h_edge_protection * (min(king_pos.col, 7 - king_pos.col) == 0);

	//cout << "edges: " << edge_protection << endl;

	int total_bonus = connected_pawns_bonus_total + pawns_bonus_total - open_files_total + edge_protection;

	//cout << "total: " << total_bonus << endl;

	return total_bonus - needed_protection;
}

// Fonction qui calcule tous les coups à une certaine profondeur, et renvoie le nombre de noeuds total
int Board::count_nodes_at_depth(int depth, bool display) {

	if (depth == 0) {
		return 1;
	}

	// FIXME: ne marche pas?
	//int game_over = is_game_over(1);
	//if (game_over != 0) {
	//	cout << "toto" << endl;
	//}

	get_moves(true);
	int nodes_count = 0;

	for (int m = 0; m < _got_moves; m++) {
		Board b(*this);
		//Board b(*this, false, true);

		if (display) {
			cout << move_label(_moves[m]) << ", ";
		}

		b.make_move(_moves[m]);
		//b.make_move(_moves[m], false, false, true);

		int below_nodes = b.count_nodes_at_depth(depth - 1, false);

		if (display) {
			cout << below_nodes << endl;
		}

		nodes_count += below_nodes;
	}

	return nodes_count;
}

// Fonction qui renvoie si le nombre de noeuds calculés pour une position à une certaine profondeur correspond au nombre attendu
bool Board::validate_nodes_count_at_depth(string fen, int depth, vector<int> expected_nodes, bool display) {

	// Met en place la position
	if (fen != "") {
		if (display) {
			cout << endl << fen << endl;
		}

		from_fen(fen);
	}

	clock_t begin_time;
	bool success = true;
	bool validation_existence = true;

	for (int d = 0; d <= depth; d++) {

		if (display) {
			begin_time = clock();
			cout << "Depth " << d << "...\r";
		}

		//int nodes = count_nodes_at_depth(d);
		int nodes = count_nodes_at_depth(d, false);
		
		if (expected_nodes.size() <= d) {
			cout << "Missing expected nodes in nodes count validation" << endl;
			//return false;
			validation_existence = false;
		}

		int expected_nodes_at_depth = validation_existence ? expected_nodes[d] : 0;

		if (display) {
			int total_time = clock() - begin_time;
			string speed = total_time == 0 ? "N/A" : (int_to_round_string(nodes / total_time * CLOCKS_PER_SEC) + "N / s");

			cout << "Depth " << d << ", expected nodes : " << expected_nodes_at_depth << " / actual nodes : " << nodes << " | " << ((nodes == expected_nodes_at_depth) ? "OK" : "FAIL") << ", time : " << total_time << " ms, speed : " << speed << endl;
		}


		if (nodes != expected_nodes_at_depth) {
			success = false;
			break;
			//return false;
		}
	}

	if (display) {
		cout << "Validation " << (success ? "succeeded" : "failed") << endl;
	}

	return success;
}

// Fonction test: nouvelle mobilité des pièces
[[nodiscard]] int Board::get_piece_mobility(bool display) const {
	// Points à prendre en compte:
	// - Nombre de cases virtuellement atteignables (en fonction des pions seulement)
	// - Cases réellement atteignables (en fonction des pièces qui peuvent gêner)
	// - Cas des pièces bloquées: par des pièces? par des pions? enfermement éternel? : les pions qui bloque la pièce sont-ils aussi bloqués?
	// - Pour les cases atteignables, valeurs differentes en fonction de la position de la case? -> ou alors ça va dans piece activity?

	// Cas tests:
	//2b1r1k1/qp3ppn/1r1p1n1p/pP1Pp3/P3P1P1/1P1QP1NP/5RB1/5RK1 w - - 1 3 : la tour en b6
	//r1bqk1nr/p4ppp/2nbp3/3p4/Pp1P4/1Pp1PN2/2P1BPPP/RNBQK2R w KQkq - 0 10 : tour + cavalier en a1 et b1
	//8/1p4pk/1r1p1n1p/pP1Pp2P/P1R1P3/1P2P3/6K1/8 b - - 2 18 : blancs gagnants? (selon Leela oui, mais pas SF17 à petite profondeur)
	//2Rnk3/3p2p1/3Pp2r/1b2P1Np/7P/5P1K/6P1/8 b - - 1 43 : pas de coups pour les noirs -> complètement perdant
	//3k4/1pp5/p1n2p2/2P1p2p/P3p1pP/1P2P1B1/1KP2PP1/8 w - - 0 30 : fou g3 enfermé à vie
	//r1bq1knr/pp1p1pp1/1bnP3p/1p2P3/8/P1N2N2/1P3PPP/R1BQ1RK1 b - - 2 12 : quasi gagnant au blancs
	//rn3rk1/1b1p1ppp/p1pP4/1pP5/P7/8/3B1PPP/1R1K1B1R w - - 0 18 : a5 gagne juste stratégiquement: les trois pièces noirs sur l'aile dame sont bloquées à vie
	//r2qk2r/1bp1ppnp/p5p1/3pP3/6P1/2N2P2/PPPB3P/R2Q1RK1 w kq - 1 14 : noirs mieux...

	//display = true;

	// TODO: comment gérer les pièces clouées?
	// TODO: plus trop de différences pour un fou entre 7 et 8 coups possibles...

	// Importance de la mobilité virtuelle ("à travers" les pièces, mais pas les pions)
	static constexpr int pawn_virtual_mobility[5] = { 0, 0, 0, 0, 0 };
	static constexpr int knight_virtual_mobility[9] = { -1350, -150, 0, 65, 110, 125, 135, 143, 150 };
	static constexpr int bishop_virtual_mobility[15] = { -1350, -500, 0, 65, 100, 125, 138, 145, 150, 155, 160, 165, 170, 175, 180 };
	static constexpr int rook_virtual_mobility[15] = { -1750, -750, 0, 35, 50, 65, 75, 85, 95, 105, 115, 125, 135, 145, 155 };
	static constexpr int queen_virtual_mobility[29] = { -3000, -1500, 0, 65, 100, 125, 138, 145, 150, 155, 160, 165, 170, 175, 180, 185, 190, 195, 200, 205, 210, 215, 220, 225, 230, 235, 240, 245, 250 };
	static constexpr int king_virtual_mobility[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    static const int* virtual_mobilities[6] = { pawn_virtual_mobility, knight_virtual_mobility, bishop_virtual_mobility, rook_virtual_mobility, queen_virtual_mobility, king_virtual_mobility };

	// Importance de la mobilité réelle (en fonction des coups possibles réels; ne traverse aucune pièce)
	static constexpr int pawn_real_mobility[5] = { 0, 0, 0, 0, 0 };
	static constexpr int knight_real_mobility[9] = { -250, -100, -35, 0, 28, 37, 43, 47, 50 };
	static constexpr int bishop_real_mobility[15] = { -350, -180, -85, -10, 15, 20, 25, 30, 35, 37, 40, 42, 45, 47, 50 };
	static constexpr int rook_real_mobility[15] = { -135, -85, -50, 10, 15, 20, 25, 30, 35, 37, 40, 42, 45, 47, 50 };
	static constexpr int queen_real_mobility[29] = { -300, -135, -70, -15, 10, 14, 18, 21, 24, 26, 28, 29, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 };
	static constexpr int king_real_mobility[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };


	static const int* real_mobilities[6] = { pawn_real_mobility, knight_real_mobility, bishop_real_mobility, rook_real_mobility, queen_real_mobility, king_real_mobility };


	// On ne compte pas les cases controllées par les pions adverses
	bool b_pawn_controls[8][8] = { false };
	bool w_pawn_controls[8][8] = { false };

	for (uint_fast8_t row = 1; row < 7; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			if (_array[row][col] == w_pawn) {
				(col > 0) && (w_pawn_controls[row + 1][col - 1] = true);
				(col < 7) && (w_pawn_controls[row + 1][col + 1] = true);
			}
			else if (_array[row][col] == b_pawn) {
				(col > 0) && (b_pawn_controls[row - 1][col - 1] = true);
				(col < 7) && (b_pawn_controls[row - 1][col + 1] = true);
			}
		}
	}


	// Mobilité des pièces blanches
	int white_mobility = 0;

	// Mobilité des pièces noires
	int black_mobility = 0;

	// Mobilité des pièces
	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			uint_fast8_t piece = _array[row][col];

			if (piece == none) {
				continue;
			}

			int virtual_mobility = 0;
			int real_mobility = 0;
			int blocking_pawn_moves = 0;

			// Pion blanc
			if (piece == w_pawn) {

				// Avance de 1
				uint_fast8_t front_piece = _array[row + 1][col];

				if (front_piece == none) {
					virtual_mobility++;
					real_mobility++;

					// Avance de 2
					if (row == 1) {
						uint_fast8_t front_piece2 = _array[row + 2][col];

						if (!is_pawn(front_piece2)) {
							virtual_mobility++;

							if (front_piece2 == none) {
								real_mobility++;
							}
						}
					}
				}
				else if (!is_pawn(front_piece)) {
					virtual_mobility++;

					// Avance de 2
					if (row == 1) {
						virtual_mobility += !is_pawn(_array[row + 2][col]);
					}
				}

				// Captures

				// Capture en diagonale gauche
				if (col > 0) {
					uint_fast8_t left_piece = _array[row + 1][col - 1];

					if (is_black(left_piece)) {
						virtual_mobility++;
						real_mobility++;
					}
				}

				// Capture en diagonale droite
				if (col < 7) {
					uint_fast8_t right_piece = _array[row + 1][col + 1];

					if (is_black(right_piece)) {
						virtual_mobility++;
						real_mobility++;
					}
				}
			}

			// Cavalier blanc
			if (piece == w_knight) {
				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_i = row + knight_directions[m][0];
					int new_j = col + knight_directions[m][1];

					if (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint_fast8_t p = _array[new_i][new_j];

						if (!b_pawn_controls[new_i][new_j]) {
							if (p != w_pawn && p != w_king) {
								virtual_mobility++;

								if (!is_white(p)) {
									real_mobility++;
								}
							}

							// Pion allié qui bloque la pièce
							else {
								// Le pion peut-il bouger?
								if (pawn_can_move(new_i, new_j, true)) {
									blocking_pawn_moves++;
								}
							}
						}
					}
				}
			}

			// Pièce à mouvement rectiligne blanche
			if (is_rectilinear(piece) && is_white(piece)) {
				for (uint_fast8_t m = 0; m < 4; m++) {
					int mi = rect_directions[m][0];
					int mj = rect_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					bool blocked = false;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint_fast8_t p = _array[new_i][new_j];

						if (!b_pawn_controls[new_i][new_j]) {
							if (p != w_pawn && p != w_king) {
								virtual_mobility++;

								if (!is_white(p) && !blocked) {
									real_mobility++;
								}
							}
							else {
								if (pawn_can_move(new_i, new_j, true)) {
									blocking_pawn_moves++;
								}
							}
						}

						// Un pion bloque la pièce
						if (is_pawn(p) || p == w_king) {
							break;
						}

						if (p != none) {
							blocked = true;
						}

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// Pièce à mouvement diagonal blanche
			if (is_diagonal(piece) && is_white(piece)) {
				for (uint_fast8_t m = 0; m < 4; m++) {
					int mi = diag_directions[m][0];
					int mj = diag_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					bool blocked = false;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint_fast8_t p = _array[new_i][new_j];

						if (!b_pawn_controls[new_i][new_j]) {
							if (p != w_pawn && p != w_king) {
								virtual_mobility++;

								if (!is_white(p) && !blocked) {
									real_mobility++;
								}
							}
							else {
								if (pawn_can_move(new_i, new_j, true)) {
									blocking_pawn_moves++;
								}
							}
						}

						// Un pion bloque la pièce
						if (is_pawn(p) || p == w_king) {
							break;
						}

						if (p != none) {
							blocked = true;
						}

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// Roi blanc
			if (piece == w_king) {
				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_i = row + all_directions[m][0];
					int new_j = col + all_directions[m][1];

					if (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint_fast8_t p = _array[new_i][new_j];

						if (!b_pawn_controls[new_i][new_j]) {
							if (p != w_pawn) {
								virtual_mobility++;

								if (!is_white(p)) {
									real_mobility++;
								}
							}
							else {
								if (pawn_can_move(new_i, new_j, true)) {
									blocking_pawn_moves++;
								}
							}
						}
					}
				}
			}

			// Pion noir
			if (piece == b_pawn) {

				// Avance de 1
				uint_fast8_t front_piece = _array[row - 1][col];

				if (front_piece == none) {
					virtual_mobility++;
					real_mobility++;

					// Avance de 2
					if (row == 6) {
						uint_fast8_t front_piece2 = _array[row - 2][col];

						if (!is_pawn(front_piece2)) {
							virtual_mobility++;

							if (front_piece2 == none) {
								real_mobility++;
							}
						}
					}
				}
				else if (!is_pawn(front_piece)) {
					virtual_mobility++;

					// Avance de 2
					if (row == 6) {
						virtual_mobility += !is_pawn(_array[row - 2][col]);
					}
				}

				// Captures

				// Capture en diagonale gauche
				if (col > 0) {
					uint_fast8_t left_piece = _array[row - 1][col - 1];

					if (is_white(left_piece)) {
						virtual_mobility++;
						real_mobility++;
					}
				}

				// Capture en diagonale droite
				if (col < 7) {
					uint_fast8_t right_piece = _array[row - 1][col + 1];

					if (is_white(right_piece)) {
						virtual_mobility++;
						real_mobility++;
					}
				}
			}

			// Cavalier noir
			if (piece == b_knight) {
				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_i = row + knight_directions[m][0];
					int new_j = col + knight_directions[m][1];

					if (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint_fast8_t p = _array[new_i][new_j];

						if (!w_pawn_controls[new_i][new_j]) {
							if (p != b_pawn && p != b_king) {
								virtual_mobility++;

								if (!is_black(p)) {
									real_mobility++;
								}
							}

							// Pion allié qui bloque la pièce
							else {
								// Le pion peut-il bouger?
								if (pawn_can_move(new_i, new_j, false)) {
									blocking_pawn_moves++;
								}
							}
						}
					}
				}
			}

			// Pièce à mouvement rectiligne noire
			if (is_rectilinear(piece) && is_black(piece)) {
				for (uint_fast8_t m = 0; m < 4; m++) {
					int mi = rect_directions[m][0];
					int mj = rect_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					bool blocked = false;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint_fast8_t p = _array[new_i][new_j];

						if (!w_pawn_controls[new_i][new_j]) {
							if (p != b_pawn && p != b_king) {
								virtual_mobility++;

								if (!is_black(p) && !blocked) {
									real_mobility++;
								}
							}
							else {
								if (pawn_can_move(new_i, new_j, false)) {
									blocking_pawn_moves++;
								}
							}
						}

						// Un pion bloque la pièce
						if (is_pawn(p) || p == b_king) {
							break;
						}

						if (p != none) {
							blocked = true;
						}

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// Pièce à mouvement diagonal noire
			if (is_diagonal(piece) && is_black(piece)) {
				for (uint_fast8_t m = 0; m < 4; m++) {
					int mi = diag_directions[m][0];
					int mj = diag_directions[m][1];

					int new_i = row + mi;
					int new_j = col + mj;

					bool blocked = false;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint_fast8_t p = _array[new_i][new_j];

						if (!w_pawn_controls[new_i][new_j]) {
							if (p != b_pawn && p != b_king) {
								virtual_mobility++;

								if (!is_black(p) && !blocked) {
									real_mobility++;
								}
							}
							else {
								if (pawn_can_move(new_i, new_j, false)) {
									blocking_pawn_moves++;
								}
							}
						}

						// Un pion bloque la pièce
						if (is_pawn(p) || p == b_king) {
							break;
						}

						if (p != none) {
							blocked = true;
						}

						new_i += mi;
						new_j += mj;
					}
				}
			}

			// Roi noir
			if (piece == b_king) {
				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_i = row + all_directions[m][0];
					int new_j = col + all_directions[m][1];

					if (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint_fast8_t p = _array[new_i][new_j];

						if (!w_pawn_controls[new_i][new_j]) {
							if (p != b_pawn && p != b_king) {
								virtual_mobility++;

								if (!is_black(p)) {
									real_mobility++;
								}
							}
							else {
								if (pawn_can_move(new_i, new_j, false)) {
									blocking_pawn_moves++;
								}
							}
						}
					}
				}
			}

			if (display)
				cout << piece_name(piece) << ", " << square_name(row, col) << ", virtual-mob : " << virtual_mobility << ", real-mob : " << real_mobility << ", pawn moves : " << blocking_pawn_moves << " | ";

			// Ajout de la mobilité
			if (is_white(piece)) {
				int virtual_mobility_value = virtual_mobilities[piece - 1][virtual_mobility];

				if (display)
					cout << virtual_mobility_value;

				if (virtual_mobility_value < 0) {
					virtual_mobility_value /= (1 + blocking_pawn_moves) * (1 + blocking_pawn_moves);

					if (display)
						cout << " / " << (1 + blocking_pawn_moves) * (1 + blocking_pawn_moves) << " = " << virtual_mobility_value;
				}

				if (display)
					cout << " + " << real_mobilities[piece - 1][real_mobility];

				int piece_mobility = virtual_mobility_value + real_mobilities[piece - 1][real_mobility];

				if (display)
					cout << " = " << piece_mobility << endl;

				white_mobility += piece_mobility;
			}
			else {
				int virtual_mobility_value = virtual_mobilities[piece - 7][virtual_mobility];

				if (display)
					cout << virtual_mobility_value;

				if (virtual_mobility_value < 0) {
					virtual_mobility_value /= (1 + blocking_pawn_moves) * (1 + blocking_pawn_moves);

					if (display)
						cout << " / " << (1 + blocking_pawn_moves) * (1 + blocking_pawn_moves) << " = " << virtual_mobility_value;
				}

				if (display)
					cout << " + " << real_mobilities[piece - 7][real_mobility];

				int piece_mobility = virtual_mobility_value + real_mobilities[piece - 7][real_mobility];

				if (display)
					cout << " = " << piece_mobility << endl;

				black_mobility += piece_mobility;
			}
		}
	}

	if (display) {
		cout << "White mobility: " << white_mobility << ", black mobility: " << black_mobility << endl;
	}

	// Facteur multiplicatif en fonction de l'avancement
	float piece_mobility_advancement_factor = 1.0f;

	return eval_from_progress(white_mobility - black_mobility, _adv, piece_mobility_advancement_factor);
}

// Fonction qui renvoie si le pion peut bouger
bool Board::pawn_can_move(uint_fast8_t row, uint_fast8_t col, bool color) const {

	// Vérifie que c'est un pion
	if (color) {
		if (_array[row][col] != w_pawn) {
			return false;
		}
	}
	else {
		if (_array[row][col] != b_pawn) {
			return false;
		}
	}

	if (color) {
		if (_array[row + 1][col] == none) {
			return true;
		}
		if (col > 0 && is_black(_array[row + 1][col - 1])) {
			return true;
		}
		if (col < 7 && is_black(_array[row + 1][col + 1])) {
			return true;
		}
	}
	else {
		if (_array[row - 1][col] == none) {
			return true;
		}
		if (col > 0 && is_white(_array[row - 1][col - 1])) {
			return true;
		}
		if (col < 7 && is_white(_array[row - 1][col + 1])) {
			return true;
		}
	}

	return false;
}

// Fonction qui renvoie l'incertiude de la position
float Board::get_uncertainty(int material_eval, int winning_eval) {

	// r2r4/8/1p1q2P1/2b5/3k1pP1/pP2pP2/2Q4P/1K6 w - - 9 11 : exemple d'une position à grosse incertitude
	// r2r4/4q3/1p4P1/2b5/5pPk/pP2pP2/8/1K6 w - - 0 18 : plus du tout d'incertitude !!
	// rnb2bnr/pppp1k1p/5q2/8/5p2/4BQ2/PPP3PP/RN3RK1 w - - 2 11 : grosse incertitude
	// r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4 : assez peu...
	// r1bq1b1r/ppp3pp/2n1k3/3np3/2B5/5Q2/PPPP1PPP/RNB1K2R w KQ - 2 8 : grosse incertitude
	// 8/8/7P/1p2Kp2/3P4/P2k2P1/P7/8 b - - 0 41 : plus aucune incertitude

	// rnb2bnr/pppp1k1p/5q2/8/5p2/4BQ2/PPP3PP/RN3RK1 w - - 2 11 : comment l'incertitude passe de 14% à 92% quand on fait Fxf4???
	// et avg score 0.7 -> 0.42??


	// TODO: faudrait-il prendre en compte la réflexion faite par Grogros?
	// Par exemple: si la position change rapidement d'évaluation?
	// Dépend du nombre de pièces restantes

	// TODO: Faut-il prendre en compte le nombre de captures possibles?

	// FIXME *** améliorable: quand il est en négatif matériel et non matériel. il va pas vouloir regagner de l'activité et rapprocher le non-matériel de 0, car ça baisserait l'incertitude...
	//5rk1/1B4p1/7p/3p4/5n2/4n2P/1R4PK/8 b - - 3 36 : ici les noirs ont de l'activité, donc de l'incertitude en plus... ça va pas

	// rnb3r1/ppp2k1p/1b1p1N2/4P3/1P6/2P3P1/P3PP1P/RN1QK2R w KQ - 2 13 : devrait être quasi 100%

	// 4r1k1/p1p2ppp/1p2bn2/8/1r6/1PN1B3/P3BPPP/R2R2K1 b - - 1 16 : ça passe à 50% seulement après c5??

	float raw_incertitude = 0.0f;

	// TODO *** faut réussir à trouver une bonne formule pour l'incertitude ***
	int non_material_eval = _evaluation - material_eval;

	if (non_material_eval != 0) {
		int abs_non_material_eval = abs(non_material_eval);

		// A quel point l'éval non-matérielle est opposée à l'éval matérielle (0 = pas du tout, 1 = totalement)
		//float opposite_material_factor = 1.0f / (1.0f + abs(_evaluation) / (abs_non_material_eval));

		// RE: produit scalaire normalisé: 1 = dans le même sens, -1 = dans le sens opposé
		int max_eval = max(abs(material_eval), abs_non_material_eval);
		float colinear_evals = static_cast<float>(material_eval) * non_material_eval / (max_eval * max_eval);

		// Valeur d'opposite material factor (0 = pas du tout, 1 = totalement)
		float opposite_material_factor = 0.5f - colinear_evals / 2.0f;

		//// Valeur d'opposite material factor quand l'évaluation matérielle est nulle
		constexpr float thresold = 0.5f;

		// New opposite material factor
		float new_opposite_material_factor = opposite_material_factor - thresold;

		//cout << "old value: " << new_opposite_material_factor << endl;

		// Rapproche la valeur des bornes (-0.5, 0.5)
		//float rapprochement = 3.0f;
		//const float rapprochement = abs(material_eval) / 100.0f;
		const float rapprochement = abs(non_material_eval) / 100.0f;
		new_opposite_material_factor = new_opposite_material_factor >= 0 ? pow(new_opposite_material_factor * 2, 1 / rapprochement) / 2 : -pow(-new_opposite_material_factor * 2, 1 / rapprochement) / 2;

		new_opposite_material_factor += thresold;

		// TODO *** opposé -> opposé ++, pas opposé -> pas opposé --

		// Constante pour laquelle on a une incertitude de base de 0.5 dans le cas le plus bordélique
		constexpr int half_uncertainty_constant = 50;

		// Normalise entre 0 et 1 ce facteur non-matériel à l'aide d'une fonction non-linéaire
		//float norm_non_material_eval = abs_non_material_eval / (static_cast<float>(half_uncertainty_constant) + abs_non_material_eval + abs(material_eval) / 2.0f);
		float norm_non_material_eval = abs_non_material_eval / (static_cast<float>(half_uncertainty_constant) + abs_non_material_eval + abs(_evaluation));
		//float norm_non_material_eval = abs_non_material_eval / (static_cast<float>(half_uncertainty_constant) + abs_non_material_eval);


		// 5rk1/1B4p1/7p/3p4/5n2/4n2P/1R4PK/8 b - - 3 36

		// Combine les deux facteurs
		float new_incertitude = norm_non_material_eval * new_opposite_material_factor;
		//float new_incertitude = norm_non_material_eval * opposite_material_factor;

		// Remet l'incertitude entre 0 et 1
		//raw_incertitude = new_incertitude + thresold;
		raw_incertitude = new_incertitude;

		//cout << "eval: " << _evaluation << ", material eval: " << material_eval << ", non-material eval: " << non_material_eval << ", opposite material factor: " << opposite_material_factor << ", new opposite material factor: " << new_opposite_material_factor << ", norm non-material eval: " << norm_non_material_eval << ", raw incertitude: " << raw_incertitude << endl;
	}

	// Importance de l'incertitude non-matérielle
	//float non_material_factor = 0.75f;


	// Incertitude par type de pièce
	int piece_uncertitudes[6] = { 1, 5, 7, 10, 50, 0 };

	// Incertitude max (environ... si y'a plusieurs dames?)
	//constexpr int max_piece_uncertainty = 204;
	constexpr int max_piece_uncertainty_per_side = 102;

	// Incertitude par pièce
	//int total_piece_uncertitude = 0;
	int white_piece_uncertainty = 0;
	int black_piece_uncertainty = 0;

	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			uint_fast8_t piece = _array[row][col];
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

	// Incertitude en fonction du nombre de pièces
	//float piece_uncertainty = total_piece_uncertitude / static_cast<float>(max_piece_uncertainty);
	float piece_uncertainty = _evaluation > 0 ? black_piece_uncertainty / static_cast<float>(max_piece_uncertainty_per_side) : white_piece_uncertainty / static_cast<float>(max_piece_uncertainty_per_side);

	// Importance de cette incertitude
	int eval = abs(_evaluation);
	//float piece_uncertainty_factor = 0.25f;
	//float piece_uncertainty_factor = 0.5 * raw_incertitude;
	float piece_uncertainty_factor = 0.5f / (1 + static_cast<float>(eval) / winning_eval);


	// Autres facteurs à prendre en compte:
	// - Nombre de pièces restantes (augmente l'incertitude)
	// - Complexité de la position (augmente l'incertitude)
	// - Déséquilibre matériel (augmente l'incertitude)
	// - Symétrie de la position (réduit l'incertitude)

	// Prise en compte de l'avancement de la partie

	float alpha = 1 + abs(_evaluation) / static_cast<float>(winning_eval) / 2.0f;
	float new_raw_incertitude = pow(raw_incertitude, alpha);

	//float value = raw_incertitude * piece_uncertainty;
	//float value = raw_incertitude * non_material_factor + piece_uncertainty * piece_uncertainty_factor;
	float value = new_raw_incertitude * (1 - piece_uncertainty_factor) + piece_uncertainty * piece_uncertainty_factor;
	
	//float alpha = 1 + abs(_evaluation) / static_cast<float>(winning_eval) / 10.0f;
	//float relative_uncertainity = pow(value, alpha);


	_uncertainty = value;

	return value;
}

// Fonction qui stocke le WDL de la position (pour les blancs)
void Board::get_WDL(float uncertainity, int winning_eval, float beta) {
	
	// r2r4/8/1p1q2P1/2b5/3k1pP1/pP2pP2/2Q4P/1K6 w - - 9 11 : ici si on va dans les lignes de nulles, ça devrait être 0, 1000, 0, pas 333, 333, 333

	// Winning eval: evaluation pour laquelle la chance de gain et de nulle est égale, pour une incertitude de 0.

	// beta contrôle la lenteur de convergence.
	// Pour beta = 0.25, f(2 * winning_eval) = 0.84
	// pour beta = 0.5,  f(2 * winning_eval) = 0.707
	// pour beta = 0.75,  f(2 * winning_eval) = 0.62

	// TEST
	//const float up_beta = 0.35f;
	//const float down_beta = 1.0f;

	// rnbqkb1r/ppp1pp1p/5p2/3p4/3P4/8/PPP2PPP/RNBQKBNR b KQkq - 0 4 proche de 100% wr pour confiance max

	// Winning eval = eval pour laquelle, la chance certaine de gagner est égale à la chance certaine de nulle (0.5)

	bool is_eval_positive = _evaluation > 0;
	float eval = abs(_evaluation);

	//cout << "eval: " << eval << ", winning_eval: " << winning_eval << ", beta: " << beta << endl;

	const float beta_up = 2.5f;
	const float beta_down = 1.5f;

	const float base_win_chance_factor = eval / winning_eval;
	const float win_chance_factor = base_win_chance_factor > 1.0f ? pow(base_win_chance_factor, beta_up) : pow(base_win_chance_factor, beta_down);
	const float base_win_chance = 1.0f - 1.0f / (1.0f + win_chance_factor);

	//const float base_win_chance = (eval / (eval + winning_eval));

	// On écrase un peu si on est complètement gagnant
	const float threshold_win_chance = base_win_chance - 0.5f;
	//const float updated_threshold = threshold_win_chance > 0 ? pow(abs(threshold_win_chance) * 2, up_beta) / 2 : -pow(abs(threshold_win_chance) * 2, down_beta) / 2;
	const float updated_threshold = threshold_win_chance;

	float certain_win_chance = updated_threshold + 0.5f;

	//float certain_win_chance = pow((eval / (eval + winning_eval)), beta);
	float certain_draw_chance = 1 - certain_win_chance;

	//cout << "certain win chance: " << certain_win_chance << ", certain draw chance: " << certain_draw_chance << endl;

	float white_win_chance = (is_eval_positive ? certain_win_chance : 0.0f);
	float white_lose_chance = (is_eval_positive ? 0.0f : certain_win_chance);

	// Fonction non-linéaire sur l'incertitude
	//float alpha = 1 + eval / winning_eval / 25.0f;
	float alpha = 1.0f;
	float relative_uncertainity = pow(uncertainity, alpha);

	//cout << "relative uncertainity: " << relative_uncertainity << endl;

	float win_chance = white_win_chance * (1 - relative_uncertainity) + relative_uncertainity / 3.0f;
	float draw_chance = certain_draw_chance * (1 - relative_uncertainity) + relative_uncertainity / 3.0f;
	float lose_chance = white_lose_chance * (1 - relative_uncertainity) + relative_uncertainity / 3.0f;

	//cout << "win chance: " << win_chance << ", draw chance: " << draw_chance << ", lose chance: " << lose_chance << endl;

	_uncertainty = uncertainity;
	_wdl = WDL(win_chance, draw_chance, lose_chance);
}

// Fonction qui renvoie l'espérance de gain (en points) de la position (fondé sur les probas de WDL)
float Board::get_average_score(float draw_score) const {
	// TODO: utiliser ce score plutôt que l'évaluation pour choisir quel coup jouer?

	return _wdl.win_chance + draw_score * _wdl.draw_chance;
}

// Fonction qui renvoie l'espérance de gain d'un WDL
float get_average_score(WDL wdl, float draw_score) {
	return wdl.win_chance + draw_score * wdl.draw_chance;
}

// Fonction qui renvoie le score d'un WDL avec une précision de 0.01
string score_string(float avg_score) {
	//float score = get_average_score(wdl, draw_score);
	stringstream stream;
	stream << fixed << setprecision(3) << avg_score;
	return stream.str();
}

// Fonction qui inverse les couleurs des joueurs (y compris le trait et les roques)
void Board::switch_colors() {
	//rnbqkbnr/pppp1ppp/8/4p3/8/8/PPPPPPPP/RNBQKBNR b KQkq - 1 2
	//rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1

	// Inversion du trait
	_player = !_player;

	// Inversion du roque
	bool b_king_castling = _castling_rights.k_b;
	bool b_queen_castling = _castling_rights.q_b;
	bool w_king_castling = _castling_rights.k_w;
	bool w_queen_castling = _castling_rights.q_w;

	_castling_rights.k_b = w_king_castling;
	_castling_rights.q_b = w_queen_castling;
	_castling_rights.k_w = b_king_castling;
	_castling_rights.q_w = b_queen_castling;

	_got_moves = -1;

	// TODO

	// King pos

	// Zobrist


	Board switched_board = *this;

	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			const uint_fast8_t piece = _array[row][col];

			const uint_fast8_t opposite_piece = is_white(piece) ? piece + 6 : is_black(piece) ? piece - 6 : piece;

			switched_board._array[7 - row][col] = opposite_piece;
		}
	}

	*this = switched_board;


	// Reset la réflexion
	main_GUI._root_exploration_node->reset();
}

// Fonction qui itère sur la map des distances à partir d'une position donnée, et renvoie les nouvelles cases contrôlées
[[nodiscard]] vector<Pos> Board::get_next_king_squares(Map& map, Pos start_pos, int distance, bool color) const {

	// Initialisation de la liste des nouvelles cases contrôlées
	vector<Pos> new_controlled_squares;

	// Regarde les 8 directions possibles
	for (uint_fast8_t m = 0; m < 8; m++) {

		// Nouvelle position
		int new_i = start_pos.row + all_directions[m][0];
		int new_j = start_pos.col + all_directions[m][1];

		// Si la case est dans l'échiquier
		if (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
			
			// Si c'est une nouvelle case explorée
			if (map._array[new_i][new_j] == 0) {
				map._array[new_i][new_j] = distance + 1;
				new_controlled_squares.push_back(Pos(new_i, new_j));
			}

			// Si c'est une case contrôlée par l'adversaire
			else if (map._array[new_i][new_j] == -64) {
				map._array[new_i][new_j] = -distance - 1;
			}
		}
	}

	return new_controlled_squares;
}

// Fonction qui renvoie une map des distances entre le roi et chaque point de l'échiquier (nombre de coups pour y arriver, en fonction des contrôles actuels du plateau)
[[nodiscard]] Map Board::get_king_squares_distance(bool color) {
	// TODO *** chiant, mais sûrement très fort...

	//8/8/1k1p4/p2P1p2/P2P1P2/3K4/8/8 w - - 12 7 : ici, le roi noir ne peut pas taper a4 ni d5 ni d4... (ou alors il doit faire tout le tour...)

	// 8/8/3k2b1/1p5p/1P1K2p1/1B4P1/7P/8 w - - 6 12
	// Roi blanc: distance de 3 à h5 (par e3 f4 g5)
	// Ne peut pas atteindre b5 dans l'état actuel...

	// 8/8/6K1/1p2k2p/1P4p1/6P1/7P/8 b - - 0 19

	// 8/5b2/8/1p1k1BKp/1P4p1/6P1/7P/8 b - - 17 17 : roi blanc plus proche


	// Map des contrôles adverses
	Map control_map = color ? get_black_controls_map() : get_white_controls_map();

	// Initialisation de la map

	// -64 = case innaccessible (pièce alliée, ou contrôlée par l'adversaire)
	// k = case à distance k du roi
	// -k = case contrôlée par l'adversaire (ou pièce alliée) à distance k
	Map distance_map;

	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {

			uint_fast8_t piece = _array[row][col];

			// On considère que le roi ne peut pas passer par:
			// Une case contrôlée par l'adversaire
			// Une pièce alliée
			// Une pièce de l'adversaire (non pion) -> car on considère que si le roi arrive sur cette pièce, elle peut reculer et garder le contrôle de la case (TEST)
			if (control_map._array[row][col] || !(piece == none || (is_pawn(piece) && is_white(piece) != color))) {
				distance_map._array[row][col] = -64;
			}
		}
	}

	// Supprime la map
	control_map.~Map();

	// Position du roi
	update_kings_pos();
	Pos king_pos = color ? _white_king_pos : _black_king_pos;

	// Construit la map des distances itérativement
	vector<Pos> current_controlled_squares = { king_pos };
	int distance = 0;

	// FIXME *** peut-être utiliser autre chose que des vecteurs... c'est très lent

	while (!current_controlled_squares.empty()) {
		vector<Pos> new_controlled_squares;
		for (const Pos& pos : current_controlled_squares) {
			vector<Pos> current_new_controlled_squares = get_next_king_squares(distance_map, pos, distance, color);
			new_controlled_squares.insert(new_controlled_squares.end(), current_new_controlled_squares.begin(), current_new_controlled_squares.end());
		}
		current_controlled_squares = new_controlled_squares;
		distance++;
	}

	// On remet les -64 à 0
	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			if (distance_map._array[row][col] == -64) {
				distance_map._array[row][col] = 0;
			}
		}
	}

	// Case du roi
	distance_map._array[king_pos.row][king_pos.col] = 0;

	return distance_map;
}


// Fonction qui renvoie la faiblesse sur les rangées du roi
int Board::get_king_row_weakness(bool color) {
	update_kings_pos();

	return 0;
}

// Fonction qui renvoie la valeur de centralisation du roi en fin de partie
int Board::get_king_centralization(bool color) {

	update_kings_pos();

	// Position du roi
	Pos king_pos = color ? _white_king_pos : _black_king_pos;

	// Distance du roi au centre
	int row_distance = min(abs(king_pos.row - 3), abs(king_pos.row - 4));
	int col_distance = min(abs(king_pos.col - 3), abs(king_pos.col - 4));

	int distance = row_distance * row_distance + col_distance * col_distance;

	// Valeur d'autant plus importante que le nombre de pions restants
	int pawns_count = 0;

	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			uint_fast8_t piece = _array[row][col];
			if (is_pawn(piece)) {
				pawns_count++;
			}
		}
	}

	float pawns_factor = (2.0f + pawns_count) / 10.0f;

	// Avancement où ça commence à être intéressant
	constexpr float begin_adv = 0.85f;

	// Malus en fonction de la distance
	return (10 - distance) * pawns_factor * max(0.0f, (_adv - begin_adv) / (1 - begin_adv));
}

// Fonction qui renvoie la valeur des pièces non défendues
int Board::get_unprotected_pieces(bool color) const {
	// TODO
	return 0;
}

// Fonction qui renvoie si le roi est dans le carré du pion
bool Board::in_king_square(Pos pos, bool king_color) {

	// FIXME *** attention, cette fonction ne fera pas tout... il y a d'autres cas ou le roi ne pourra pas atteindre le pion (si cases controllées par exemple...)

	// Position du roi
	update_kings_pos();
	Pos king_pos = king_color ? _white_king_pos : _black_king_pos;

	// TODO *** prendre en compte le +1 aux limites si le roi a le trait

	// color : couleur du roi (!= couleur du pion)
	bool pawn_color = !king_color;

	// Distance à la promotion
	int distance = pawn_color ? 7 - pos.row + !_player : pos.row + _player;

	//cout << "distance: " << distance << endl;

	// Case de promotion
	Pos promotion_pos = Pos(pawn_color ? 7 : 0, pos.col);

	//cout << "king pos: " << king_pos.row << " " << king_pos.col << endl;
	//cout << "promotion pos: " << promotion_pos.row << " " << promotion_pos.col << endl;

	// Distance du roi à la promotion
	int row_distance = abs(king_pos.row - promotion_pos.row);
	int col_distance = abs(king_pos.col - promotion_pos.col);

	//cout << "row distance: " << row_distance << ", col distance: " << col_distance << endl;

	// Si le roi est dans le carré
	return row_distance <= distance && col_distance <= distance;
}

// Fonction qui renvoie si on est en finale de pions
bool Board::is_pawn_endgame() const {
	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			uint_fast8_t piece = _array[row][col];
			if (piece != none && !is_pawn(piece) && !is_king(piece)) {
				return false;
			}
		}
	}

	return true;
}

// Fonction qui renvoie si le joueur a encore des pièces (autres que roi et pions)
bool Board::has_pieces(bool color) const {
	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			uint_fast8_t piece = _array[row][col];
			if (piece != none && !is_pawn(piece) && !is_king(piece) && (is_white(piece) == color)) {
				return true;
			}
		}
	}

	return false;
}

// Fonction qui renvoie la réelle valeur d'un paramètre de faiblesse long terme du roi, en fonction des possibilités et proximités des roques
int Board::get_long_term_king_weakness(bool player, int current_weakness, int kingside_weakness, int queenside_weakness) {

	// Mise à jour des positions des rois
	update_kings_pos();

	// Position du roi
	Pos king_pos = player ? _white_king_pos : _black_king_pos;

	// Possibilités de roque
	bool can_kingside_castle = player ? _castling_rights.k_w : _castling_rights.k_b;
	bool can_queenside_castle = player ? _castling_rights.q_w : _castling_rights.q_b;

	uint_fast8_t king_row = player ? 0 : 7;
	uint_fast8_t above_row = player ? 1 : 6;
	uint_fast8_t blocking_bishop = player ? w_bishop : b_bishop;

	// TODO *** fonctions pour les distances aux roques

	// Ajout de distance en cas de controle de l'adversaire
	constexpr int control_distance_add = 3;

	// Distance au petit roque
	uint_fast8_t kingside_castle_distance = 0;

	if (can_kingside_castle) {
		kingside_castle_distance = (_array[king_row][5] != none) + (_array[king_row][6] != none)
			+ (_array[king_row][5] == blocking_bishop && _array[above_row][4] != none && _array[above_row][6] != none)
			+ is_controlled(king_row, 5, player) * control_distance_add + is_controlled(king_row, 6, player) * control_distance_add;
	}

	// r1bqk2r/pppp1ppp/1bn2n2/8/4P3/1N3P2/PPP3PP/RNBQKB1R w KQkq - 3 7 : ici le petit roque est controllé -> distance + beaucoup...

	// Distance au grand roque
	uint_fast8_t queenside_castle_distance = 0;

	if (can_queenside_castle) {
		queenside_castle_distance = (_array[king_row][3] != none) + (_array[king_row][2] != none) + (_array[king_row][1] != none)
			+ (_array[king_row][2] == blocking_bishop && _array[above_row][1] != none && _array[above_row][3] != none)
			+ is_controlled(king_row, 2, player) * control_distance_add + is_controlled(king_row, 3, player) * control_distance_add;
	}

	//cout << "kingside: " << (int)kingside_castle_distance << ", queenside: " << (int)queenside_castle_distance << endl;
	//cout << "castling distance factor, kingside: " << max(0.0, 1.0 - (1.0 + kingside_castle_distance) / 7.0) << ", queenside: " << max(0.0, 1.0 - (1.0 + queenside_castle_distance) / 7.0) << endl;

	// TODO *** peut être améliorable si d'autres pièces que le fou de base bloquent le roque

	// TOTAL: main / C + K / (Dk + c) + Q / (Dq + c)

	int total_weakness = current_weakness;

	// Gain potentiel pour les roques
	const int kingside_castling_bonus = max(0, current_weakness - kingside_weakness);
	const int queenside_castling_bonus = max(0, current_weakness - queenside_weakness);

	//3qkb1r/p1pbnppp/2p5/4N3/Q7/2N5/Pr3PPP/3RR1K1 b k - 1 14 : distance au roque: au moins 3

	// FIXME *** un peu aribitraire... 0.9 quand on peut roquer, et se rapproche linéairement de 0 sinon. En partant du principe que la distance ne sera pas au delà de 8...
	// Fonction largement améliorable
	double kingside_castling_factor = can_kingside_castle ? max(0.0, 1.0 - (1.0 + kingside_castle_distance) / 5.0) : 0.0;
	double queenside_castling_factor = can_queenside_castle ? max(0.0, 1.0 - (1.0 + queenside_castle_distance) / 5.0) : 0.0;

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

// Fonction qui renvoie la valeur des bonus liés aux colonnes ouvertes et semi-ouvertes sur le roi adverse, s'il était sur une certaine colonne
int Board::get_open_files_on_opponent_king_at_column(bool player, int king_col) const {

	// Bonus pour les colonnes ouvertes et semi-ouvertes
	constexpr int open_file_bonus = 30;
	constexpr int semi_open_file_bonus = 25;

	// Facteur en fonction de la proximité avec la colonne du roi adverse
	// Si le roi est sur la colonne, le bonus est maximal
	constexpr float king_file_bonus = 1.0f;

	// Si le roi est sur une colonne adjacente, le bonus est réduit
	constexpr float king_adjacent_file_bonus = 0.4f;

	// Bonus en plus pour les pièces présentes dessus (tours/dame)
	constexpr int rook_open_bonus = 40;
	constexpr int queen_open_bonus = 45;

	constexpr int rook_semi_open_bonus = 25;
	constexpr int queen_semi_open_bonus = 20;

	// Bonus pour le joueur
	int total_bonus = 0;

	// Pion allié
	const int player_pawn = player ? w_pawn : b_pawn;

	// Pion adverse
	const int opponent_pawn = player ? b_pawn : w_pawn;

	// Pour chaque colonne adjacente au roi noir
	for (uint_fast8_t col = king_col - 1; col < king_col + 2; col++) {

		// Si la colonne est en dehors de l'échiquier
		if (col < 0 || col > 7)
			continue;

		// Nature de la colonne
		bool semi_open = true;
		bool open = true;

		for (uint_fast8_t row = 0; row < 8; row++) {
			uint_fast8_t p = _array[row][col];

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

		// Bonus pour les pièces présentes sur la colonne
		if (open || semi_open) {
			for (uint_fast8_t row = 0; row < 8; row++) {
				uint_fast8_t p = _array[row][col];

				if (p == (player ? w_rook : b_rook))
					bonus += open ? rook_open_bonus : rook_semi_open_bonus;
				else if (p == (player ? w_queen : b_queen))
					bonus += open ? queen_open_bonus : queen_semi_open_bonus;
			}

			//cout << "col: " << (int)col << ", open: " << open << ", semi_open: " << semi_open << ", bonus: " << bonus << endl;
		}

		// 4k1r1/2pp4/1p2pq2/6r1/p2P3p/2PB1b1P/PPQ3P1/R4RK1 w - - 2 24
		// 4k2r/2pp3B/1p2pq2/6r1/p2P4/2P2b1P/PPQ2Rp1/4R1K1 b - - 3 27

		// Bonus en fonction de la proximité du roi
		bonus *= (col == king_col ? king_file_bonus : king_adjacent_file_bonus);

		total_bonus += bonus;
	}

	// En fonction de l'avancement de la partie
	constexpr float advancement_factor = 0.0f;

	return eval_from_progress(total_bonus, _adv, advancement_factor);
}

// Fonction qui renvoie la valeur des bonus liés au placement du roi, s'il était sur une certaine colonne
int Board::get_king_placement_weakness_at_column(bool player, Pos king_pos) const {

	// Paramétrage
	constexpr float edge_adv = 0.9f;
	constexpr float mult_endgame = 0.25f;

	// Version additive, adaptée pour l'endgame
	constexpr int edge_defense = 75;

	const int col_dist = min(king_pos.col, 7 - king_pos.col);
	const int row_dist = min(king_pos.row, 7 - king_pos.row);

	const int center_col_dist = min(abs(king_pos.col - 3), abs(king_pos.col - 4));
	const int center_row_dist = min(abs(king_pos.row - 3), abs(king_pos.row - 4));

	const double base_factor = edge_defense * (edge_adv - _adv);

	const double row_malus = 0.5f;
	const double row_factor = row_malus * (player ? (king_pos.row * king_pos.row) : ((7 - king_pos.row) * (7 - king_pos.row)));

	const double placement_weakness = base_factor * ((_adv < edge_adv) ? (max(0, col_dist - 1) + row_factor / 2.0f) : (mult_endgame / (edge_adv - 1.0f) * (center_col_dist * center_col_dist + center_row_dist * center_row_dist)));

	//cout << "king pos: " << (int)king_pos.row << " " << (int)king_pos.col << ", row dist: " << row_dist << ", col dist: " << col_dist << ", row factor: " << row_factor << ", placement weakness: " << placement_weakness << endl;

	return placement_weakness;
}


// Fonction qui renvoie la valeur des bonus liés au placement du roi
int Board::get_king_placement_weakness(bool player) {

	// Position du roi
	update_kings_pos();
	Pos king_pos = player ? _white_king_pos : _black_king_pos;

	// Droits de roque
	bool can_kingside_castle = player ? _castling_rights.k_w : _castling_rights.k_b;
	bool can_queenside_castle = player ? _castling_rights.q_w : _castling_rights.q_b;
	
	int current_weakness = get_king_placement_weakness_at_column(player, king_pos);
	int kingside_weakness = can_kingside_castle ? get_king_placement_weakness_at_column(player, { player ? 0 : 7, 6 }) : 0;
	int queenside_weakness = can_queenside_castle ? get_king_placement_weakness_at_column(player, { player ? 0 : 7, 2 }) : 0;

	return get_long_term_king_weakness(player, current_weakness, kingside_weakness, queenside_weakness);
	//return current_weakness;
}

// Fonction qui renvoie une map de tous les pions bloqués
Map Board::get_blocked_pawns(bool color) const {

	// On considère le roi comme bloquant, quel que soit ses coups possibles

	// Initialisation de la map
	Map blocked_pawns;

	// Pour chaque pion
	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {

			uint_fast8_t piece = _array[row][col];

			// Pion blanc
			if (color && piece == w_pawn) {
				// Si le pion est bloqué
				if (_array[row + 1][col] != none && !(col > 0 && is_black(_array[row + 1][col - 1])) && !(col < 7 && is_black(_array[row + 1][col + 1]))) {
					blocked_pawns._array[row][col] = 1;
				}
			}

			// Roi blanc
			else if (color && piece == w_king) {
					blocked_pawns._array[row][col] = 1;
			}

			// Pion noir
			else if (!color && piece == b_pawn) {
				// Si le pion est bloqué
				if (_array[row - 1][col] != none && !(col > 0 && is_white(_array[row - 1][col - 1])) && !(col < 7 && is_white(_array[row - 1][col + 1]))) {
					blocked_pawns._array[row][col] = 1;
				}
			}

			// Roi noir
			else if (!color && piece == b_king) {
				blocked_pawns._array[row][col] = 1;
			}
		}
	}

	return blocked_pawns;
}


// Fonction qui prend une map de pions/pièces bloquées, la met à jour en fonction des nouvelles pièces bloquées, et renvoie si une ou plusieurs pièces y ont été ajoutées
bool Board::update_blocked_pieces(Map& blocked_pieces, bool color) const {

	// rn1qkbnr/pbp1p1p1/1p1pPpPp/3P1P2/8/8/PPP4P/RNBQKBNR b KQkq - 0 10

	bool new_blocked = false;

	// On regarde chaque pièce
	// Si la pièce a au moins un mouvement possible, elle n'est pas bloquée

	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			uint_fast8_t piece = _array[row][col];

			if (!is_white(piece) == color || piece == none || is_king(piece) || blocked_pieces._array[row][col]) {
				continue;
			}
			
			// Cavalier
			if (is_knight(piece)) {
				bool is_blocked = true;

				for (uint_fast8_t m = 0; m < 8 && is_blocked; m++) {
					int new_row = row + knight_directions[m][0];
					int new_col = col + knight_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !blocked_pieces._array[new_row][new_col]) {
						is_blocked = false;
						break;
					}
				}

				if (is_blocked) {
					blocked_pieces._array[row][col] = 1;
					new_blocked = true;
				}
			}

			// Fou
			else if (is_bishop(piece)) {
				bool is_blocked = true;

				for (uint_fast8_t m = 0; m < 4 && is_blocked; m++) {
					int new_row = row + diag_directions[m][0];
					int new_col = col + diag_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !blocked_pieces._array[new_row][new_col]) {
						is_blocked = false;
						break;
					}
				}

				if (is_blocked) {
					blocked_pieces._array[row][col] = 1;
					new_blocked = true;
				}
			}

			// Tour
			else if (is_rook(piece)) {
				bool is_blocked = true;

				for (uint_fast8_t m = 0; m < 4 && is_blocked; m++) {
					int new_row = row + rect_directions[m][0];
					int new_col = col + rect_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !blocked_pieces._array[new_row][new_col]) {
						is_blocked = false;
						break;
					}
				}

				if (is_blocked) {
					blocked_pieces._array[row][col] = 1;
					new_blocked = true;
				}
			}

			// Dame
			else if (is_queen(piece)) {

				bool is_blocked = true;

				for (uint_fast8_t m = 0; m < 8 && is_blocked; m++) {

					int new_row = row + all_directions[m][0];
					int new_col = col + all_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !blocked_pieces._array[new_row][new_col]) {
						is_blocked = false;
						break;
					}
				}

				if (is_blocked) {
					blocked_pieces._array[row][col] = 1;
					new_blocked = true;
				}
			}
		}
	}

	return new_blocked;
}

// Fonction qui renvoie toutes la map de toutes les pièces bloquées
Map Board::get_all_blocked_pieces(bool color) const {

	// rn1qkbnr/pbp1p1pr/1p1pPpPp/3P1P1P/8/8/PPP5/RNBQKBNR b KQkq - 0 12 : ici sont bloqués: Fou f8, Cavalier g8, Tour h8 et Tour h7. Problème, les tours ne se considèrent pas entre elles comme bloquantes...
	// rn1qkbnr/pbp1p1pn/1p1pPpPp/3P1PpP/6P1/8/PPP5/RNBQKBNR b KQkq - 0 12 : ici sont bloqués: Fou f8, Cavalier g8, Tour h8 et le cavalier en h7

	// Map des pions bloqués
	Map blocked_pieces = get_blocked_pawns(color);

	//cout << "initial " << (color ? "white" : "black") << " blocked pawns:" << endl;
	//blocked_pieces.print();

	// Tant qu'on trouve de nouvelles pièces bloquées
	while (true) {
		bool new_blocked = update_blocked_pieces(blocked_pieces, color);

		if (!new_blocked) {
			//cout << "done" << endl;
			break;
		}

		//cout << "new blocked pieces:" << endl;
		//blocked_pieces.print();
	}

	return blocked_pieces;
}

// Fonction qui renvoie la map des cases controlées par les pions
[[nodiscard]] Map Board::get_pawns_controls(bool color) const {

	// Map des contrôles
	Map controls;

	// Pion allié
	uint_fast8_t player_pawn = color ? w_pawn : b_pawn;

	// Direction des pions
	const int pawn_direction = color ? 1 : -1;

	// Pour chaque pion
	for (uint_fast8_t row = 1; row < 7; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {

			// Pion allié
			if (_array[row][col] == player_pawn) {
				(col > 0) && (controls._array[row + pawn_direction][col - 1] = true);
				(col < 7) && (controls._array[row + pawn_direction][col + 1] = true);
			}
		}
	}
	return controls;
}


// Fonction qui renvoie la mobilité réelle des pièces (court terme)
[[nodiscard]] int Board::get_short_term_piece_mobility(bool display) const {

	// Importance de la mobilité réelle (en fonction des coups possibles réels; ne traverse aucune pièce)
	static constexpr int pawn_real_mobility[5] = { 0, 0, 0, 0, 0 };
	static constexpr int knight_real_mobility[9] = { -250, -100, -35, 0, 28, 37, 43, 47, 50 };
	static constexpr int bishop_real_mobility[15] = { -200, -120, -50, -10, 15, 20, 25, 30, 35, 37, 40, 42, 45, 47, 50 };
	static constexpr int rook_real_mobility[15] = { -135, -85, -50, 10, 15, 20, 25, 30, 35, 37, 40, 42, 45, 47, 50 };
	static constexpr int queen_real_mobility[29] = { -300, -135, -70, -15, 10, 14, 18, 21, 24, 26, 28, 29, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 };
	static constexpr int king_real_mobility[9] = { -100, -40, 0, 5, 10, 12, 13, 14, 15 };

	static const int* real_mobilities[6] = { pawn_real_mobility, knight_real_mobility, bishop_real_mobility, rook_real_mobility, queen_real_mobility, king_real_mobility };

	// Map des contrôles de pions
	Map white_pawns_controls = get_pawns_controls(true);
	Map black_pawns_controls = get_pawns_controls(false);

	// Mobilité des pièces
	int white_mobility = 0;
	int black_mobility = 0;

	// Pour chaque pièce
	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			uint_fast8_t piece = _array[row][col];

			if (piece == none) {
				continue;
			}

			int piece_mobility = 0;

			// Pion blanc
			if (piece == w_pawn) {
				piece_mobility = _array[row + 1][col] == none; // Poussée simple
				piece_mobility += col > 0 && is_black(_array[row + 1][col - 1]); // Prise à gauche
				piece_mobility += col < 7 && is_black(_array[row + 1][col + 1]); // Prise à droite
				piece_mobility += row == 1 && _array[row + 1][col] == none && _array[row + 2][col] == none; // Poussée double
			}

			// Cavalier blanc
			if (piece == w_knight) {
				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_row = row + knight_directions[m][0];
					int new_col = col + knight_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !is_white(_array[new_row][new_col]) && !black_pawns_controls._array[new_row][new_col]) {
						piece_mobility++;
					}
				}
			}

			// Pièce à mouvement rectiligne blanche
			if (is_rectilinear(piece) && is_white(piece)) {
				for (uint_fast8_t m = 0; m < 4; m++) {
					int d_row = rect_directions[m][0];
					int d_col = rect_directions[m][1];

					int new_row = row + d_row;
					int new_col = col + d_col;

					while (is_in(new_row, 0, 7) && is_in(new_col, 0, 7)) {
						uint_fast8_t p = _array[new_row][new_col];
						
						if (!is_white(p) && !black_pawns_controls._array[new_row][new_col]) {
							piece_mobility++;
						}

						if (p != none) {
							break;
						}

						new_row += d_row;
						new_col += d_col;
					}
				}
			}

			// Pièce à mouvement diagonal blanche
			if (is_diagonal(piece) && is_white(piece)) {
				for (uint_fast8_t m = 0; m < 4; m++) {
					int d_row = diag_directions[m][0];
					int d_col = diag_directions[m][1];

					int new_row = row + d_row;
					int new_col = col + d_col;

					while (is_in(new_row, 0, 7) && is_in(new_col, 0, 7)) {
						uint_fast8_t p = _array[new_row][new_col];

						if (!is_white(p) && !black_pawns_controls._array[new_row][new_col]) {
							piece_mobility++;
						}

						if (p != none) {
							break;
						}

						new_row += d_row;
						new_col += d_col;
					}
				}
			}

			// Roi blanc
			if (piece == w_king) {
				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_row = row + all_directions[m][0];
					int new_col = col + all_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !is_white(_array[new_row][new_col]) && !black_pawns_controls._array[new_row][new_col]) {
						piece_mobility++;
					}
				}
			}

			// Pion noir
			if (piece == b_pawn) {
				piece_mobility = _array[row - 1][col] == none; // Poussée simple
				piece_mobility += col > 0 && is_white(_array[row - 1][col - 1]); // Prise à gauche
				piece_mobility += col < 7 && is_white(_array[row - 1][col + 1]); // Prise à droite
				piece_mobility += row == 6 && _array[row - 1][col] == none && _array[row - 2][col] == none; // Poussée double
			}

			// Cavalier noir
			if (piece == b_knight) {
				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_row = row + knight_directions[m][0];
					int new_col = col + knight_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !is_black(_array[new_row][new_col]) && !white_pawns_controls._array[new_row][new_col]) {
						piece_mobility++;
					}
				}
			}

			// Pièce à mouvement rectiligne noire
			if (is_rectilinear(piece) && is_black(piece)) {
				for (uint_fast8_t m = 0; m < 4; m++) {
					int d_row = rect_directions[m][0];
					int d_col = rect_directions[m][1];

					int new_row = row + d_row;
					int new_col = col + d_col;

					while (is_in(new_row, 0, 7) && is_in(new_col, 0, 7)) {
						uint_fast8_t p = _array[new_row][new_col];

						if (!is_black(p) && !white_pawns_controls._array[new_row][new_col]) {
							piece_mobility++;
						}

						if (p != none) {
							break;
						}

						new_row += d_row;
						new_col += d_col;
					}
				}
			}

			// Pièce à mouvement diagonal noire
			if (is_diagonal(piece) && is_black(piece)) {
				for (uint_fast8_t m = 0; m < 4; m++) {
					int d_row = diag_directions[m][0];
					int d_col = diag_directions[m][1];

					int new_row = row + d_row;
					int new_col = col + d_col;

					while (is_in(new_row, 0, 7) && is_in(new_col, 0, 7)) {
						uint_fast8_t p = _array[new_row][new_col];

						if (!is_black(p) && !white_pawns_controls._array[new_row][new_col]) {
							piece_mobility++;
						}

						if (p != none) {
							break;
						}

						new_row += d_row;
						new_col += d_col;
					}
				}
			}

			// Roi noir
			if (piece == b_king) {
				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_row = row + all_directions[m][0];
					int new_col = col + all_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && !is_black(_array[new_row][new_col]) && !white_pawns_controls._array[new_row][new_col]) {
						piece_mobility++;
					}
				}
			}

			if (display) {
				cout << piece_name(piece) << " on " << square_name(row, col) << ", mobility: " << piece_mobility << endl;
			}

			// Ajout de la mobilité de la pièce
			if (is_white(piece)) {
				white_mobility += real_mobilities[piece - 1][piece_mobility];
			}
			else if (is_black(piece)) {
				black_mobility += real_mobilities[piece - 7][piece_mobility];
			}
		}
	}

	if (display) {
		cout << "white mobility: " << white_mobility << ", black mobility: " << black_mobility << endl;
	}

	// En fonction de l'avancement de la partie
	float advancement_factor = 1.0f;

	return eval_from_progress(white_mobility - black_mobility, _adv, advancement_factor);
}

// Fonction qui renvoie la mobilité virtuelle des pièces (long terme)
[[nodiscard]] int Board::get_long_term_piece_mobility(bool display) const {

	// Importance de la mobilité virtuelle (à travers toutes les pièces non bloquées)
	static constexpr int pawn_virtual_mobility[5] = { 0, 0, 0, 0, 0 };
	static constexpr int knight_virtual_mobility[9] = { -1350, -450, 0, 65, 110, 125, 135, 143, 150 };
	static constexpr int bishop_virtual_mobility[15] = { -1350, -500, 0, 65, 100, 125, 138, 145, 150, 155, 160, 165, 170, 175, 180 };
	static constexpr int rook_virtual_mobility[15] = { -1750, -750, 0, 35, 50, 65, 75, 85, 95, 105, 115, 125, 135, 145, 155 };
	static constexpr int queen_virtual_mobility[29] = { -3000, -1500, 0, 65, 100, 125, 138, 145, 150, 155, 160, 165, 170, 175, 180, 185, 190, 195, 200, 205, 210, 215, 220, 225, 230, 235, 240, 245, 250 };
	static constexpr int king_virtual_mobility[9] = { -1000, -500, 0, 10, 20, 29, 37, 44, 50 };

	static const int* virtual_mobilities[6] = { pawn_virtual_mobility, knight_virtual_mobility, bishop_virtual_mobility, rook_virtual_mobility, queen_virtual_mobility, king_virtual_mobility };

	// Map des pièces bloquées
	Map white_blocked_pieces = get_all_blocked_pieces(true);
	Map black_blocked_pieces = get_all_blocked_pieces(false);

	// Map des contrôles de pions
	Map white_pawns_controls = get_pawns_controls(true);
	Map black_pawns_controls = get_pawns_controls(false);

	// Mobilité des pièces
	int white_mobility = 0;
	int black_mobility = 0;

	// Pour chaque pièce
	for (uint_fast8_t row = 0; row < 8; row++) {
		for (uint_fast8_t col = 0; col < 8; col++) {
			uint_fast8_t piece = _array[row][col];

			if (piece == none) {
				continue;
			}

			int piece_mobility = 0;

			// Pion blanc
			if (piece == w_pawn) {
				piece_mobility = white_blocked_pieces._array[row + 1][col] == 0; // Poussée simple
				piece_mobility += col > 0 && is_black(_array[row + 1][col - 1]); // Prise à gauche
				piece_mobility += col < 7 && is_black(_array[row + 1][col + 1]); // Prise à droite
				piece_mobility += row == 1 && white_blocked_pieces._array[row + 1][col] == 0 && white_blocked_pieces._array[row + 2][col] == 0; // Poussée double
			}

			// Cavalier blanc
			if (piece == w_knight) {
				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_row = row + knight_directions[m][0];
					int new_col = col + knight_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && white_blocked_pieces._array[new_row][new_col] == 0 && !black_pawns_controls._array[new_row][new_col]) {
						piece_mobility++;
					}
				}
			}

			// Pièce à mouvement rectiligne blanche
			if (is_rectilinear(piece) && is_white(piece)) {
				for (uint_fast8_t m = 0; m < 4; m++) {
					int d_row = rect_directions[m][0];
					int d_col = rect_directions[m][1];

					int new_row = row + d_row;
					int new_col = col + d_col;

					while (is_in(new_row, 0, 7) && is_in(new_col, 0, 7)) {
						uint_fast8_t p = _array[new_row][new_col];

						if (white_blocked_pieces._array[new_row][new_col] == 1 || (p == b_pawn && black_pawns_controls._array[new_row][new_col])) {
							break;
						}

						if (!black_pawns_controls._array[new_row][new_col]) {
							piece_mobility++;
						}

						new_row += d_row;
						new_col += d_col;
					}
				}
			}

			// Pièce à mouvement diagonal blanche
			if (is_diagonal(piece) && is_white(piece)) {
				for (uint_fast8_t m = 0; m < 4; m++) {
					int d_row = diag_directions[m][0];
					int d_col = diag_directions[m][1];

					int new_row = row + d_row;
					int new_col = col + d_col;

					while (is_in(new_row, 0, 7) && is_in(new_col, 0, 7)) {
						uint_fast8_t p = _array[new_row][new_col];

						if (white_blocked_pieces._array[new_row][new_col] == 1 || (p == b_pawn && black_pawns_controls._array[new_row][new_col])) {
							break;
						}

						if (!black_pawns_controls._array[new_row][new_col]) {
							piece_mobility++;
						}

						new_row += d_row;
						new_col += d_col;
					}
				}
			}

			// Roi blanc
			if (piece == w_king) {
				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_row = row + all_directions[m][0];
					int new_col = col + all_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && white_blocked_pieces._array[new_row][new_col] == 0 && !black_pawns_controls._array[new_row][new_col]) {
						piece_mobility++;
					}
				}
			}

			// Pion noir
			if (piece == b_pawn) {
				piece_mobility = black_blocked_pieces._array[row - 1][col] == 0; // Poussée simple
				piece_mobility += col > 0 && is_white(_array[row - 1][col - 1]); // Prise à gauche
				piece_mobility += col < 7 && is_white(_array[row - 1][col + 1]); // Prise à droite
				piece_mobility += row == 6 && black_blocked_pieces._array[row - 1][col] == 0 && black_blocked_pieces._array[row - 2][col] == 0; // Poussée double
			}

			// Cavalier noir
			if (piece == b_knight) {
				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_row = row + knight_directions[m][0];
					int new_col = col + knight_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && black_blocked_pieces._array[new_row][new_col] == 0 && !white_pawns_controls._array[new_row][new_col]) {
						piece_mobility++;
					}
				}
			}

			// Pièce à mouvement rectiligne noire
			if (is_rectilinear(piece) && is_black(piece)) {
				for (uint_fast8_t m = 0; m < 4; m++) {
					int d_row = rect_directions[m][0];
					int d_col = rect_directions[m][1];

					int new_row = row + d_row;
					int new_col = col + d_col;

					while (is_in(new_row, 0, 7) && is_in(new_col, 0, 7)) {
						uint_fast8_t p = _array[new_row][new_col];

						if (black_blocked_pieces._array[new_row][new_col] == 1 || (p == w_pawn && white_pawns_controls._array[new_row][new_col])) {
							break;
						}

						if (!white_pawns_controls._array[new_row][new_col]) {
							piece_mobility++;
						}

						new_row += d_row;
						new_col += d_col;
					}
				}
			}

			// Pièce à mouvement diagonal noire
			if (is_diagonal(piece) && is_black(piece)) {
				for (uint_fast8_t m = 0; m < 4; m++) {
					int d_row = diag_directions[m][0];
					int d_col = diag_directions[m][1];

					int new_row = row + d_row;
					int new_col = col + d_col;

					while (is_in(new_row, 0, 7) && is_in(new_col, 0, 7)) {
						uint_fast8_t p = _array[new_row][new_col];

						if (black_blocked_pieces._array[new_row][new_col] == 1 || (p == w_pawn && white_pawns_controls._array[new_row][new_col])) {
							break;
						}

						if (!white_pawns_controls._array[new_row][new_col]) {
							piece_mobility++;
						}

						new_row += d_row;
						new_col += d_col;
					}
				}
			}

			// Roi noir
			if (piece == b_king) {
				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_row = row + all_directions[m][0];
					int new_col = col + all_directions[m][1];

					if (is_in(new_row, 0, 7) && is_in(new_col, 0, 7) && black_blocked_pieces._array[new_row][new_col] == 0 && !white_pawns_controls._array[new_row][new_col]) {
						piece_mobility++;
					}
				}
			}

			if (display) {
				cout << piece_name(piece) << " on " << square_name(row, col) << ", mobility: " << piece_mobility << endl;
			}

			// Ajout de la mobilité de la pièce
			if (is_white(piece)) {
				white_mobility += virtual_mobilities[piece - 1][piece_mobility];
			}
			else if (is_black(piece)) {
				black_mobility += virtual_mobilities[piece - 7][piece_mobility];
			}
		}
	}

	if (display) {
		cout << "white mobility: " << white_mobility << ", black mobility: " << black_mobility << endl;
	}

	// En fonction de l'avancement de la partie
	float advancement_factor = 1.0f;

	return eval_from_progress(white_mobility - black_mobility, _adv, advancement_factor);
}