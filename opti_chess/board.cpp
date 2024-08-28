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
bool Board::add_move(const uint_fast8_t i, const uint_fast8_t j, const uint_fast8_t k, const uint_fast8_t l, int* iterator, const uint_fast8_t piece)
{
	// Si on dépasse le nombre de coups que l'on pensait possible dans une position
	if (*iterator >= max_moves)
		return false;

	const Move m(i, j, k, l); // Si on utilise pas les flag, autant éviter les calculs inutiles
	//const Move m(i, j, k, l, _array[k][l] != 0, (piece == 1 && i == 7) || (piece == 7 && i == 1));
	_moves[*iterator] = m;

	// Incrémentation du nombre de coups
	(*iterator)++;

	return true;
}

// Fonction qui ajoute les coups "pions" dans la liste de coups
bool Board::add_pawn_moves(const uint_fast8_t i, const uint_fast8_t j, int* iterator) {

	// Joueur avec les pièces blanches
	if (_player) {
		_array[i + 1][j] == 0 && add_move(i, j, i + 1, j, iterator, 1) && i == 1 && _array[i + 2][j] == 0 && add_move(i, j, i + 2, j, iterator, 1); // Poussées
		j > 0 && (is_in_fast(_array[i + 1][j - 1], 7, 12) || (_en_passant_col == j - 1 && i == 4)) && add_move(i, j, i + 1, j - 1, iterator, 1); // Prise (gauche)
		j < 7 && (is_in_fast(_array[i + 1][j + 1], 7, 12) || (_en_passant_col == j + 1 && i == 4)) && add_move(i, j, i + 1, j + 1, iterator, 1); // Prise (droite)
	}

	// Joueur avec les pièces noires
	else {
		_array[i - 1][j] == 0 && add_move(i, j, i - 1, j, iterator, 7) && i == 6 && _array[i - 2][j] == 0 && add_move(i, j, i - 2, j, iterator, 7); // Poussées
		j > 0 && (is_in_fast(_array[i - 1][j - 1], 1, 6) || (_en_passant_col == j - 1 && i == 3)) && add_move(i, j, i - 1, j - 1, iterator, 7); // Prise (gauche)
		j < 7 && (is_in_fast(_array[i - 1][j + 1], 1, 6) || (_en_passant_col == j + 1 && i == 3)) && add_move(i, j, i - 1, j + 1, iterator, 7); // Prise (droite)
	}

	return true;
}

// Fonction qui ajoute les coups "cavaliers" dans la liste de coups
bool Board::add_knight_moves(const uint_fast8_t i, const uint_fast8_t j, int* iterator, const uint_fast8_t piece) {

	// Tableau des déplacements possibles
	static constexpr int_fast8_t knight_moves[8][2] = { {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1} };

	for (uint_fast8_t m = 0; m < 8; m++) {
		const uint_fast8_t i2 = i + knight_moves[m][0];
		if (!is_in_fast(i2, 0, 7))
			continue;

		const uint_fast8_t j2 = j + knight_moves[m][1];
		if (!is_in_fast(j2, 0, 7))
			continue;

		((_player && !is_in_fast(_array[i2][j2], 1, 6)) || (!_player && !is_in_fast(_array[i2][j2], 7, 12))) && add_move(i, j, i2, j2, iterator, piece);
	}

	return true;
}

// Fonction qui ajoute les coups diagonaux dans la liste de coups
bool Board::add_diag_moves(const uint_fast8_t i, const uint_fast8_t j, int* iterator, const uint_fast8_t piece) {

	const uint_fast8_t ally_min = _player ? 1 : 7;
	const uint_fast8_t ally_max = _player ? 6 : 12;

	// Directions possibles : diagonales
	static constexpr int_fast8_t diag_moves[4][2] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1} };

	for (uint_fast8_t k = 0; k < 4; k++) {

		// Direction
		const uint_fast8_t mi = diag_moves[k][0];
		const uint_fast8_t mj = diag_moves[k][1];

		uint_fast8_t i2 = i + mi;
		uint_fast8_t j2 = j + mj;

		while (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8) {
			const uint_fast8_t p2 = _array[i2][j2];

			// Si y'a une pièce alliée, on arrête
			if (is_in_fast(p2, ally_min, ally_max))
				break;

			// Coup possible
			add_move(i, j, i2, j2, iterator, piece);

			// Si y'a une pièce ennemie, on arrête
			if (p2 != 0)
				break;

			i2 += mi;
			j2 += mj;
		}
	}

	return true;
}

// Fonction qui ajoute les coups horizontaux et verticaux dans la liste de coups
bool Board::add_rect_moves(const uint_fast8_t i, const uint_fast8_t j, int* iterator, const uint_fast8_t piece) {

	const uint_fast8_t ally_min = _player ? 1 : 7;
	const uint_fast8_t ally_max = _player ? 6 : 12;

	// Directions possibles : horizontales et verticales
	static constexpr int_fast8_t rect_moves[4][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };

	for (uint_fast8_t k = 0; k < 4; k++) {

		// Direction
		const uint_fast8_t mi = rect_moves[k][0];
		const uint_fast8_t mj = rect_moves[k][1];

		uint_fast8_t i2 = i + mi;
		uint_fast8_t j2 = j + mj;

		while (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8) {
			const uint_fast8_t p2 = _array[i2][j2];

			// Si y'a une pièce alliée, on arrête
			if (is_in_fast(p2, ally_min, ally_max))
				break;

			// Coup possible
			add_move(i, j, i2, j2, iterator, piece);

			// Si y'a une pièce ennemie, on arrête
			if (p2 != 0)
				break;

			i2 += mi;
			j2 += mj;
		}
	}

	return true;
}

// Fonction qui ajoute les coups "roi" dans la liste de coups
bool Board::add_king_moves(const uint_fast8_t i, const uint_fast8_t j, int* iterator, const uint_fast8_t piece) {
	const uint_fast8_t ally_min = _player ? 1 : 7;
	const uint_fast8_t ally_max = _player ? 6 : 12;

	for (int k = -1; k < 2; k++) {
		for (int l = -1; l < 2; l++) {
			if (k == 0 && l == 0)
				continue;
			const uint_fast8_t i2 = i + k;
			const uint_fast8_t j2 = j + l;
			// Si le coup n'est ni hors du plateau, ni sur une case où une pièce alliée est placée
			is_in_fast(i2, 0, 7) && is_in_fast(j2, 0, 7) && !is_in_fast(_array[i2][j2], ally_min, ally_max) && add_move(i, j, i2, j2, iterator, piece);
		}
	}

	return true;
}

// Calcule la liste des coups possibles. pseudo ici fait référence au droit de roquer en passant par une position illégale.
bool Board::get_moves(const bool forbide_check) 
{
	int iterator = 0;

	for (int index = 0; index < 64; index++) {
		const uint_fast8_t i = index / 8;
		const uint_fast8_t j = index % 8;
		const uint_fast8_t p = _array[i][j];

		// Si on dépasse le nombre de coups que l'on pensait possible dans une position
		if (iterator >= max_moves) {
			cout << "Too many moves in the position : " << iterator << "+" << endl;
			return false;
		}


		// Le joueur doit correspondre à la couleur de la pièce
		if (p == 0 || _player != (p < 7))
			continue;


		switch (p) {

		case 0: // Case vide
			break;

		case 1: // Pion blanc
			add_pawn_moves(i, j, &iterator);
			break;

		case 2: // Cavalier blanc
			add_knight_moves(i, j, &iterator, 2);
			break;

		case 3: // Fou blanc
			add_diag_moves(i, j, &iterator, 3);
			break;

		case 4: // Tour blanche
			add_rect_moves(i, j, &iterator, 4);
			break;

		case 5: // Dame blanche
			add_diag_moves(i, j, &iterator, 5) && add_rect_moves(i, j, &iterator, 5);
			break;

		case 6: // Roi blanc
			add_king_moves(i, j, &iterator, 6);
			// Roques
			// Grand
			// TODO : optimisable
			if (_castling_rights.q_w && _array[i][j - 1] == 0 && _array[i][j - 2] == 0 && _array[i][j - 3] == 0 && (!is_controlled(i, j, true) && !is_controlled(i, j - 1, true) && !is_controlled(i, j - 2, true)))
				add_move(i, j, i, j - 2, &iterator, 6);
			// Petit
			if (_castling_rights.k_w && _array[i][j + 1] == 0 && _array[i][j + 2] == 0 && (!is_controlled(i, j, true) && !is_controlled(i, j + 1, true) && !is_controlled(i, j + 2, true)))
				add_move(i, j, i, j + 2, &iterator, 6);
			break;

		case 7: // Pion noir
			add_pawn_moves(i, j, &iterator);
			break;

		case 8: // Cavalier noir
			add_knight_moves(i, j, &iterator, 8);
			break;

		case 9: // Fou noir
			add_diag_moves(i, j, &iterator, 9);
			break;

		case 10: // Tour noire
			add_rect_moves(i, j, &iterator, 10);
			break;

		case 11: // Dame noire
			add_diag_moves(i, j, &iterator, 11) && add_rect_moves(i, j, &iterator, 11);
			break;

		case 12: // Roi noir
			add_king_moves(i, j, &iterator, 12);
			// Roques
			// Grand
			if (_castling_rights.q_b && _array[i][j - 1] == 0 && _array[i][j - 2] == 0 && _array[i][j - 3] == 0 && (!is_controlled(i, j, false) && !is_controlled(i, j - 1, false) && !is_controlled(i, j - 2, false)))
				add_move(i, j, i, j - 2, &iterator, 12);
			// Petit
			if (_castling_rights.k_b && _array[i][j + 1] == 0 && _array[i][j + 2] == 0 && (!is_controlled(i, j, false) && !is_controlled(i, j + 1, false) && !is_controlled(i, j + 2, false)))
				add_move(i, j, i, j + 2, &iterator, 12);
			break;
		}

	}

	_got_moves = static_cast<int_fast8_t>(iterator);

	// Vérification échecs
	if (forbide_check) {
		Move new_moves[100]; // Est-ce que ça prend de la mémoire?
		int_fast8_t n_moves = 0;
		Board b;

		for (int i = 0; i < _got_moves; i++) {
			b.copy_data(*this);
			b.make_index_move(i, false);
			b._player = !b._player;
			if (!b.in_check()) {
				new_moves[n_moves] = _moves[i];
				n_moves++;
			}
		}

		for (int i = 0; i < n_moves; i++)
			_moves[i] = new_moves[i];

		_got_moves = n_moves;
	}

	return true;
}

// Fonction qui dit s'il y'a échec
bool Board::in_check()
{
	update_kings_pos();

	const int king_i = _player ? _white_king_pos.i : _black_king_pos.i;
	const int king_j = _player ? _white_king_pos.j : _black_king_pos.j;

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
		if (const uint_fast8_t piece = _array[king_i][j]; piece != 0)
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
		cout << " " << move_label_from_index(i) << " |";
	cout << "]" << endl;
}

// Fonction qui joue un coup
void Board::make_move(Move move, const bool pgn, const bool new_board, const bool add_to_history)
{
	// TODO : à voir si ça rend plus rapide ou non
	const uint_fast8_t i = move.i1;
	const uint_fast8_t j = move.j1;
	const uint_fast8_t k = move.i2;
	const uint_fast8_t l = move.j2;
	const uint_fast8_t p = _array[i][j];
	const uint_fast8_t p_last = _array[k][l];


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
	if (p == 1 || p == 7 || p_last) {
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
	(p == 1 && k == i + 2 && (_array[k][l - 1] == 7 || _array[k][l + 1] == 7)) && ((_en_passant_col = j));
	(p == 7 && k == i - 2 && (_array[k][l - 1] == 1 || _array[k][l + 1] == 1)) && ((_en_passant_col = j));

	// En passant
	(p == 1 && j != l && p_last == 0) && ((_array[k - 1][l] = 0));
	(p == 7 && j != l && p_last == 0) && ((_array[k + 1][l] = 0));


	// Roi blanc
	if (p == 6) {
		_castling_rights.q_w = false;
		_castling_rights.k_w = false;
		_white_king_pos = { k, l }; // Met à jour la position du roi

		(l == j + 2) && ((_array[0][7] = 0), (_array[0][5] = 4)); // Petit roque
		(l == j - 2) && ((_array[0][0] = 0), (_array[0][3] = 4)); // Grand roque
	}

	// Roi noir
	else if (p == 12) {
		_castling_rights.q_b = false;
		_castling_rights.k_b = false;
		_black_king_pos = { k, l };

		(l == j + 2) && ((_array[7][7] = 0), (_array[7][5] = 10)); // Petit roque
		(l == j - 2) && ((_array[7][0] = 0), (_array[7][3] = 10)); // Grand roque
	}

	// Tour blanche
	(p == 4) && ((j == 0) && (_castling_rights.q_w = false) || (j == 7) && (_castling_rights.k_w = false));

	// Tour noire
	(p == 10) && ((j == 0) && (_castling_rights.q_b = false) || (j == 7) && (_castling_rights.k_b = false));

	// Tour blanche mangée
	(p_last == 4) && ((l == 0) && (_castling_rights.q_w = false) || (l == 7) && (_castling_rights.k_w = false));

	// Tour noire mangée
	(p_last == 10) && ((l == 0) && (_castling_rights.q_b = false) || (l == 7) && (_castling_rights.k_b = false));


	// Actualise la case d'arrivée
	_array[k][l] = p;

	// Promotion (en dame seulement pour le moment)
	(p == 1 && k == 7) && (_array[k][l] = 5);
	(p == 7 && k == 0) && (_array[k][l] = 11);

	// Vide la case de départ
	_array[i][j] = 0;

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
	static constexpr int adv_pawn = 1;
	static constexpr int adv_knight = 10;
	static constexpr int adv_bishop = 10;
	static constexpr int adv_rook = 10;
	static constexpr int adv_queen = 100;
	static constexpr int adv_castle = 25;

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

	_adv = static_cast<float>(p_tot - p) / p_tot;

	return;
}

// Fonction qui compte le matériel sur l'échiquier et renvoie sa valeur
int Board::count_material(const Evaluator* eval) const
{
	int material_count = 0;

	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			if (const uint_fast8_t piece = _array[i][j]) {
				const int value = static_cast<int>(static_cast<float>(eval->_pieces_value_begin[(piece - 1) % 6]) * (1.0f - _adv) + static_cast<float>(eval->_pieces_value_end[(piece - 1) % 6]) * _adv);
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

	if (check_game_over) {
		is_game_over();

		// Nulle
		if (_game_over_value == 2) {
			_evaluation = 0;
			_evaluated = true;

			if (display)
				main_GUI._eval_components = "DRAW\n";

			return true;
		}

		// Mat
		if (_game_over_value != 0) {
			_evaluation = (-mate_value + _moves_count * mate_ply) * get_color();
			_evaluated = true;

			if (display)
				main_GUI._eval_components = "CHECKMATE\n";

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


	// *** MATERIEL ***

	if (display)
		main_GUI._eval_components += "\nMATERIAL:\n";

	int total_material = 0;

	// Matériel
	if (eval->_piece_value != 0.0f) {
		const int material = count_material(eval) * eval->_piece_value;
		if (display)
			main_GUI._eval_components += "material: " + (material >= 0 ? string("+") : string()) + to_string(material) + "\n";
		total_material += material;
	}	

	// Paire de oufs
	if (eval->_bishop_pair != 0.0f) {
		const int bishop_pair = count_bishop_pairs() * eval->_bishop_pair;
		if (display)
			main_GUI._eval_components += "bishop pair: " + (bishop_pair >= 0 ? string("+") : string()) + to_string(bishop_pair) + "\n";
		total_material += bishop_pair;
	}

	if (display)
		main_GUI._eval_components += "TOTAL: " + (total_material >= 0 ? string("+") : string()) + to_string(total_material) + "\n";

	_evaluation += total_material;

	// *** POSITIONNEMENT ***

	if (display)
		main_GUI._eval_components += "\nPOSITIONING:\n";

	int total_positioning = 0;

	// Positionnement des pièces
	if (eval->_piece_positioning != 0.0f) {
		const int positioning = pieces_positioning(eval) * eval->_piece_positioning;
		if (display)
			main_GUI._eval_components += "piece positioning: " + (positioning >= 0 ? string("+") : string()) + to_string(positioning) + "\n";
		total_positioning += positioning;
	}

	// Tours sur les colonnes ouvertes / semi-ouvertes
	if (eval->_rook_open != 0.0f) {
		const int rook_open = get_rooks_on_open_file() * eval->_rook_open;
		if (display)
			main_GUI._eval_components += "rooks on open/semi files: " + (rook_open >= 0 ? string("+") : string()) + to_string(rook_open) + "\n";
		total_positioning += rook_open;
	}

	// Fous en fianchetto
	if (eval->_fianchetto != 0.0f) {
		const int fianchetto = get_fianchetto_value() * eval->_fianchetto;
		if (display)
			main_GUI._eval_components += "fianchetto bishops: " + (fianchetto >= 0 ? string("+") : string()) + to_string(fianchetto) + "\n";
		total_positioning += fianchetto;
	}

	// Alignement des pièces (fou-tour/dame-roi)
	if (eval->_alignments != 0.0f)
	{
		const int pieces_alignment = get_alignments() * eval->_alignments;
		if (display)
			main_GUI._eval_components += "(pieces alignment: " + (pieces_alignment >= 0 ? string("+") : string()) + to_string(pieces_alignment) + ")\n";
		total_positioning += pieces_alignment;
	}

	// Pièces isolées
	if (eval->_isolated_pieces != 0.0f) {
		const int isolated_pieces = get_isolated_pieces() * eval->_isolated_pieces;
		if (display)
			main_GUI._eval_components += "isolated_pieces: " + (isolated_pieces >= 0 ? string("+") : string()) + to_string(isolated_pieces) + "\n";
		total_positioning += isolated_pieces;
	}

	// Menace de poussée de pion sur une pièce adverse
	if (eval->_pawn_push_threats != 0.0f) {
		const int pawn_push_threat = get_pawn_push_threats() * eval->_pawn_push_threats;
		if (display)
			main_GUI._eval_components += "pawn push threats: " + (pawn_push_threat >= 0 ? string("+") : string()) + to_string(pawn_push_threat) + "\n";
		total_positioning += pawn_push_threat;
	}

	if (display)
		main_GUI._eval_components += "TOTAL: " + (total_positioning >= 0 ? string("+") : string()) + to_string(total_positioning) + "\n";

	_evaluation += total_positioning;


	// *** ACTIVITE ***

	if (display)
		main_GUI._eval_components += "\nACTIVITY:\n";

	int total_activity = 0;

	// Mobilité des pièces
	if (eval->_piece_mobility != 0.0f) {
		const int piece_mobility = get_piece_mobility() * eval->_piece_mobility;
		if (display)
			main_GUI._eval_components += "piece mobility: " + (piece_mobility >= 0 ? string("+") : string()) + to_string(piece_mobility) + "\n";
		total_activity += piece_mobility;
	}

	// Activité des pièces
	if (eval->_piece_activity != 0.0f) {
		const int piece_activity = get_piece_activity() * eval->_piece_activity;
		if (display)
			main_GUI._eval_components += "piece activity: " + (piece_activity >= 0 ? string("+") : string()) + to_string(piece_activity) + "\n";
		total_activity += piece_activity;
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
		const int player_trait = eval->_player_trait * get_color();
		if (display)
			main_GUI._eval_components += "player trait: " + (player_trait >= 0 ? string("+") : string()) + to_string(player_trait) + "\n";
		total_activity += player_trait;
	}

	if (display)
		main_GUI._eval_components += "TOTAL: " + (total_activity >= 0 ? string("+") : string()) + to_string(total_activity) + "\n";

	_evaluation += total_activity;


	// *** STRUCTURE DE PIONS ***

	if (display)
		main_GUI._eval_components += "\nPAWN STRUCTURE:\n";

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
		const int space = get_space() * eval->_space_advantage;
		if (display)
			main_GUI._eval_components += "space: " + (space >= 0 ? string("+") : string()) + to_string(space) + "\n";
		total_pawn_structure += space;
	}

	// Structure de pions
	if (eval->_pawn_structure != 0.0f) {
		const int pawn_structure = get_pawn_structure(display * eval->_pawn_structure) * eval->_pawn_structure;
		if (display)
			main_GUI._eval_components += "pawn structure: " + (pawn_structure >= 0 ? string("+") : string()) + to_string(pawn_structure) + "\n";
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
		const int weak_squares = get_weak_squares() * eval->_weak_squares;
		if (display)
			main_GUI._eval_components += "weak squares: " + (weak_squares >= 0 ? string("+") : string()) + to_string(weak_squares) + "\n";
		total_pawn_structure += weak_squares;
	}

	// Evaluation avec toutes ses composantes
	if (display)
		main_GUI._eval_components += "TOTAL: " + (total_pawn_structure >= 0 ? string("+") : string()) + to_string(total_pawn_structure) + "\n";

	_evaluation += total_pawn_structure;


	// *** ROI ***

	if (display)
		main_GUI._eval_components += "\nKING:\n";

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
	if (eval->_castling_distance != 0.0f) {
		const int castling_distance = get_castling_distance() * eval->_castling_distance;
		if (display)
			main_GUI._eval_components += "castling distance: " + (castling_distance >= 0 ? string("+") : string()) + to_string(castling_distance) + "\n";
		total_king += castling_distance;
	}

	// Marrées de pions
	if (eval->_pawn_storm != 0.0f) {
		const int pawn_storm = get_pawn_storm() * eval->_pawn_storm;
		if (display)
			main_GUI._eval_components += "pawn storm: " + (pawn_storm >= 0 ? string("+") : string()) + to_string(pawn_storm) + "\n";
		total_king += pawn_storm;
	}

	// Boucliers de pions
	if (eval->_pawn_shield != 0.0f) {
		const int pawn_shield = get_pawn_shield() * eval->_pawn_shield;
		if (display)
			main_GUI._eval_components += "pawn shield: " + (pawn_shield >= 0 ? string("+") : string()) + to_string(pawn_shield) + "\n";
		total_king += pawn_shield;
	}

	if (display)
		main_GUI._eval_components += "TOTAL: " + (total_king >= 0 ? string("+") : string()) + to_string(total_king) + "\n";

	_evaluation += total_king;


	// *** FINALES ***

	if (display)
		main_GUI._eval_components += "\nENDGAME:\n";

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

	if (display)
		main_GUI._eval_components += "TOTAL: " + (total_endgame >= 0 ? string("+") : string()) + to_string(total_endgame) + "\n";

	_evaluation += total_endgame;

	// *** NATURE DE LA POSITION ***

	if (display)
		main_GUI._eval_components += "\nPOSITION NATURE:\n";

	int total_nature = 0;

	// Nature de la position (TODO)
	if (display)
		main_GUI._eval_components += "(position nature: 0)\n";

	// Forteresse
	if (eval->_push != 0.0f) {
		const float push = 1 - static_cast<float>(_half_moves_count) * eval->_push / max_half_moves;
		const int fortress = 100.0f - push * 100.0f;
		const int fortress_value = _evaluation * (push - 1);
		if (display)
			main_GUI._eval_components += "fortress: " + to_string(fortress) + "% (" + (fortress_value >= 0 ? string("+") : string()) + to_string(fortress_value) + ")\n";
		total_nature += fortress_value;
	}

	if (display)
		main_GUI._eval_components += "TOTAL: " + (total_nature >= 0 ? string("+") : string()) + to_string(total_nature) + "\n";

	_evaluation += total_nature;


	// *** TOTAL ***
	if (display) {
		main_GUI._eval_components += "\nTOTAL COMPONENTS:\n";
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
	const float win_chance = get_winning_chances_from_eval(_evaluation, true);
	if (display)
		main_GUI._eval_components += "W/D/L: " + to_string(static_cast<int>(100 * win_chance)) + "/" + to_string(static_cast<int>(100 * 0)) + "/" + to_string(static_cast<int>(100 * (1.0f - win_chance))) + "%\n";


	// L'évaluation a été effectuée
	_evaluated = true;

	// Met à jour l'évaluation statique
	_static_evaluation = _evaluation;

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
	const uint_fast8_t i = move.i1;
	const uint_fast8_t j = move.j1;
	const uint_fast8_t k = move.i2;
	const uint_fast8_t l = move.j2;

	const uint_fast8_t p1 = _array[i][j]; // Pièce qui bouge
	const uint_fast8_t p2 = _array[k][l];

	// Pour savoir si une autre pièce similaire peut aller sur la même case
	bool spec_col = false;
	bool spec_line = false;
	if (_got_moves == -1)
		get_moves(true);

	uint_fast8_t i1; uint_fast8_t j1; uint_fast8_t k1; uint_fast8_t l1; uint_fast8_t p11;
	for (int m = 0; m < _got_moves; m++) {
		i1 = _moves[m].i1;
		j1 = _moves[m].j1;
		k1 = _moves[m].i2;
		l1 = _moves[m].j2;
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

// Fonction qui renvoie le label d'un coup en fonction de son index
string Board::move_label_from_index(const int i) {
	// Pour pas qu'il re écrase les moves
	if (_got_moves == -1)
		get_moves(true);
	return move_label(_moves[i]);
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
	const uint_fast8_t i = move.i1;
	const uint_fast8_t j = move.j1;
	const uint_fast8_t k = move.i2;
	const uint_fast8_t l = move.j2;

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

// Fonction qui calcule et renvoie la mobilité des pièces
int Board::get_piece_mobility(const bool legal) const
{
	// TODO : fonction LARGEMENT optimisable

	Board b;
	b.copy_data(*this);
	int piece_mobility = 0;

	// Pour chaque pièce (sauf le roi)
	//static constexpr int mobility_values_pawn[3] = { -100, 0, 100 };
	static constexpr int mobility_values_pawn[3] = { -50, 0, 25 };
	static constexpr int mobility_values_knight[9] = { -500, -200, 0, 100, 200, 300, 400, 450, 500 };
	static constexpr int mobility_values_bishop[15] = { -600, -300, -50, 100, 210, 280, 330, 475, 415, 450, 480, 505, 525, 540, 550 };
	static constexpr int mobility_values_rook[15] = { -300, -100, 100, 150, 190, 235, 275, 300, 325, 345, 365, 385, 390, 400, 405 };
	static constexpr int mobility_values_queen[29] = { -700, -400, -300, 150, 190, 235, 275, 300, 325, 345, 365, 385, 390, 400, 410, 420, 430, 440, 450, 460, 470, 480, 490, 495, 500, 505, 510 };

	// TODO : rajouter le roi, pour l'endgame? faire des valeurs différentes selon le moment de la partie?

	


	// Fait un tableau de toutes les pièces : position, valeur
	int piece_move_count[8][8] = {0};


	// On ne compte pas les cases controllées par les pions adverses

	// Mobility area pour les blancs
	bool mobility_area_white[8][8] = { false };

	for (uint_fast8_t i = 1; i < 7; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			if (_array[i][j] == 7) {
				(j > 0) && (mobility_area_white[i - 1][j - 1] = true);
				(j < 7) && (mobility_area_white[i - 1][j + 1] = true);
			}
		}
	}

	// Mobility area pour les noirs
	bool mobility_area_black[8][8] = { false };

	for (uint_fast8_t i = 1; i < 7; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			if (_array[i][j] == 1) {
				(j > 0) && (mobility_area_black[i + 1][j - 1] = true);
				(j < 7) && (mobility_area_black[i + 1][j + 1] = true);
			}
		}
	}


	// Activité des pièces du joueur
	// TODO : ça doit être très lent : on re-calcule tous les coups à chaque fois... (et on les garde même pas en mémoire pour après, car c'est sur un plateau virtuel)
	// En plus ça calcule aussi les coups de l'autre
	b.get_moves(legal);

	// Pour chaque coup, incrémente dans le tableau le nombre de coup à la position correspondante
	if (_player) {
		for (int i = 0; i < b._got_moves; i++)
			!mobility_area_white[b._moves[i].i2][b._moves[i].j2] && piece_move_count[b._moves[i].i1][b._moves[i].j1]++;
	}
	else {
		for (int i = 0; i < b._got_moves; i++)
			!mobility_area_black[b._moves[i].i2][b._moves[i].j2] && piece_move_count[b._moves[i].i1][b._moves[i].j1]++;
	}
	


	// Activité des pièces de l'autre joueur
	b._player = !b._player; b._got_moves = -1;
	b.get_moves(legal);

	if (_player) {
		for (int i = 0; i < b._got_moves; i++)
			!mobility_area_black[b._moves[i].i2][b._moves[i].j2] && piece_move_count[b._moves[i].i1][b._moves[i].j1]++;
	}
	else {
		for (int i = 0; i < b._got_moves; i++)
			!mobility_area_white[b._moves[i].i2][b._moves[i].j2] && piece_move_count[b._moves[i].i1][b._moves[i].j1]++;
	}
		

	// Pour chaque pièce : ajoute la valeur correspondante à l'activité
	int index = 0;
	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			if (const uint_fast8_t piece = _array[i][j]; piece > 0) {
				if (piece == 1)
					piece_mobility += mobility_values_pawn[min(2, piece_move_count[i][j])];
				else if (piece == 2)
					piece_mobility += mobility_values_knight[min(8, piece_move_count[i][j])];
				else if (piece == 3)
					piece_mobility += mobility_values_bishop[min(14, piece_move_count[i][j])];
				else if (piece == 4)
					piece_mobility += mobility_values_rook[min(14, piece_move_count[i][j])];
				else if (piece == 5)
					piece_mobility += mobility_values_queen[min(28, piece_move_count[i][j])];
				else if (piece == 7)
					piece_mobility -= mobility_values_pawn[min(2, piece_move_count[i][j])];
				else if (piece == 8)
					piece_mobility -= mobility_values_knight[min(8, piece_move_count[i][j])];
				else if (piece == 9)
					piece_mobility -= mobility_values_bishop[min(14, piece_move_count[i][j])];
				else if (piece == 10)
					piece_mobility -= mobility_values_rook[min(14, piece_move_count[i][j])];
				else if (piece == 11)
					piece_mobility -= mobility_values_queen[min(28, piece_move_count[i][j])];

				//cout << "(" << (int)i << ", " << (int)j << ") p: " << (int)piece << " -> m: " << piece_move_count[i][j] << " (total: " << piece_mobility << ")" << endl;
				//r2qkb1r/ppp1pppp/2n5/5b2/2PP4/5N2/PP2BPPP/R1BQ1RK1 w kq - 1 10

				index++;
			}
		}
	}

	return piece_mobility * (1 - _adv);
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

	// 8/6PK/5k2/8/8/8/8/8 b - - 0 8


	// Met à jour la position des rois
	update_kings_pos();


	// Faiblesses des rois
	int w_king_weakness = 0;
	int b_king_weakness = 0;

	// Protection des rois
	int w_king_protection = 0;
	int b_king_protection = 0;

	// Puissances d'attaque
	int w_attacking_power = 0;
	int b_attacking_power = 0;

	// Puissance de défense (on met de base une valeur, car selon Kasparov : il faut du surnombre pour attaquer - constante)
	//int w_defending_power = 75;
	//int b_defending_power = 75;
	int w_defending_power = 0;
	int b_defending_power = 0;

	// Facteurs multiplicatifs
	constexpr float piece_attack_factor = 1.25f;
	constexpr float piece_defense_factor = 1.0f;

	constexpr float piece_overload_multiplicator = 2.0f; // TODO: à utiliser
	constexpr float piece_defense_multiplicator = 0.5f;

	constexpr float pawn_protection_factor = 1.5f;


	// -------------------------------------
	// *** CALCUL DES PUISSANCES ***
	// * ATTAQUES - DEFENSES - PROTECTIONS *
	// -------------------------------------

	// TODO: fonction pour ça
	// Calcul des protections et puissances d'attaques
	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			if (const uint_fast8_t p = _array[i][j]; p > 0) {
				if (p == 1)
					w_king_protection += static_cast<int>(pawn_protection_factor * static_cast<float>(get_piece_defense_power(i, j)));
				else if (p == 7)
					b_king_protection += static_cast<int>(pawn_protection_factor * static_cast<float>(get_piece_defense_power(i, j)));
			}
		}
	}

	// Attaquants
	w_attacking_power = get_king_attackers(true) * piece_attack_factor;
	b_attacking_power = get_king_attackers(false) * piece_attack_factor;

	// Défenseurs
	w_defending_power = get_king_defenders(true) * piece_defense_factor;
	b_defending_power = get_king_defenders(false) * piece_defense_factor;

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "----------\n";
		main_GUI._eval_components += "Attacking power: " + to_string(w_attacking_power) + " / " + to_string(b_attacking_power) + "\n";
		main_GUI._eval_components += "Defending power: " + to_string(w_defending_power) + " / " + to_string(b_defending_power) + "\n";
		main_GUI._eval_components += "King protection: " + to_string(w_king_protection) + " / " + to_string(b_king_protection) + "\n";
	}


	// -------------------
	// *** MATING NETS ***
	// -------------------

	// Récupère les maps de contrôle des cases
	Map white_controls_map = get_white_controls_map();
	Map black_controls_map = get_black_controls_map();

	
	// ----------------------
	// *** ESCAPE SQUARES ***
	// ----------------------

	// FIXME: faut-il l'utiliser?
	
	// Récupère les cases d'échappatoires des rois
	//const uint_fast8_t w_escape_squares = get_king_escape_squares(true);
	//const uint_fast8_t b_escape_squares = get_king_escape_squares(false);

	//if (display_factor != 0.0f) {
	//	main_GUI._eval_components += "Escape squares: " + to_string(w_escape_squares) + " / " + to_string(b_escape_squares) + "\n";
	//}


	// -----------------
	// *** OVERLOADS ***
	// -----------------

	// Est-ce utile?

	// Résultante des contrôles
	Map controls_map = white_controls_map - black_controls_map;

	// Danger des surcharges (cases controlées en supériorité par l'allié), proche du roi adverse
	constexpr uint_fast8_t overloard_distance_dangers[8] = { 100, 75, 15, 5, 2, 0, 0, 0 };


	// Overload sur le roi blanc
	int w_king_overloaded = 0;

	// Cases controllées proches du roi
	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			if (controls_map._array[i][j] < 0)
			{
				const uint_fast8_t distance = max(abs(i - _white_king_pos.i), abs(j - _white_king_pos.j));
				w_king_overloaded -= overloard_distance_dangers[distance] * controls_map._array[i][j]; // - car valeur négative
			}
		}
	}
	
	// Overload sur le roi noir
	int b_king_overloaded = 0;

	// Attaques sur le plateau
	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			if (controls_map._array[i][j] > 0)
			{
				const uint_fast8_t distance = max(abs(i - _black_king_pos.i), abs(j - _black_king_pos.j));
				b_king_overloaded += overloard_distance_dangers[distance] * controls_map._array[i][j];
			}
		}
	}

	const float overload_factor = 1.5f;
	//const float overload_factor = 0.0f;

	w_king_overloaded *= overload_factor;
	b_king_overloaded *= overload_factor;

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "Overloaded: " + to_string(w_king_overloaded) + " / " + to_string(b_king_overloaded) + "\n";
	}


	// -------------------
	// *** PROTECTIONS ***
	// -------------------

	// Protection par le bord de l'échiquier
	constexpr int edge_protection = 100;
	//r2r2k1/1b3p2/p3pn2/2b1q2p/4P1p1/2N4P/PPBBQPP1/R2R2K1 w - - 0 19
	w_king_protection += edge_protection * ((min(_white_king_pos.i, 7 - _white_king_pos.i) == 0) + (min(_white_king_pos.j, 7 - _white_king_pos.j) == 0));
	b_king_protection += edge_protection * ((min(_black_king_pos.i, 7 - _black_king_pos.i) == 0) + (min(_black_king_pos.j, 7 - _black_king_pos.j) == 0));

	// Droits de roque
	//constexpr int castling_rights_protection = 200;
	constexpr int castling_rights_protection = 0;
	constexpr int single_castling_protection = 2;

	const int w_castling_protection = (_castling_rights.k_w || _castling_rights.q_w) * single_castling_protection + (_castling_rights.k_w && _castling_rights.q_w);
	w_king_protection += w_castling_protection * castling_rights_protection;

	const int b_castling_protection = (_castling_rights.k_b || _castling_rights.q_b) * single_castling_protection + (_castling_rights.k_b && _castling_rights.q_b);
	b_king_protection += b_castling_protection * castling_rights_protection;

	// Niveau de protection auquel on peut considérer que le roi est safe
	const int king_base_protection = 600 * (1 - _adv);
	w_king_protection -= king_base_protection;
	b_king_protection -= king_base_protection;

	// Facteur multiplicatif pour ce paramètre
	const float king_protection_factor = 0.5f;

	w_king_protection *= king_protection_factor;
	b_king_protection *= king_protection_factor;

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "Updated king protection: " + to_string(w_king_protection) + " / " + to_string(b_king_protection) + "\n";
	}


	// -----------------------
	// *** POSITION DU ROI ***
	// -----------------------

	// Proximité avec le bord
	// Avancement à partir duquel il est plus dangereux d'être sur un bord
	constexpr float edge_adv = 0.75f;
	constexpr float mult_endgame = 1.0f;

	// Version additive, adaptée pour l'endgame
	constexpr int edge_defense = 150;
	constexpr int endgame_safe_zone = 25; // Si le "i * j" du roi en endgame est supérieur, alors il n'est pas en danger : s'il est en c4 (2, 3 -> (2 + 1) * (3 + 1) = 12 < 16 -> danger)
	
	int w_placement_weakness = max_int(edge_defense, edge_defense * (edge_adv - _adv) * ((_adv < edge_adv) ? (min(_white_king_pos.i, 7 - _white_king_pos.i) + min(_white_king_pos.j, 7 - _white_king_pos.j)) : (endgame_safe_zone - ((min(_white_king_pos.i, 7 - _white_king_pos.i) + 1) * (min(_white_king_pos.j, 7 - _white_king_pos.j) + 1))) * mult_endgame / (edge_adv - 1))) - edge_defense;
	int b_placement_weakness = max_int(edge_defense, edge_defense * (edge_adv - _adv) * ((_adv < edge_adv) ? (min(_black_king_pos.i, 7 - _black_king_pos.i) + min(_black_king_pos.j, 7 - _black_king_pos.j)) : (endgame_safe_zone - ((min(_black_king_pos.i, 7 - _black_king_pos.i) + 1) * (min(_black_king_pos.j, 7 - _black_king_pos.j) + 1))) * mult_endgame / (edge_adv - 1))) - edge_defense;

	const float placement_factor = 0.5f;

	w_placement_weakness *= placement_factor;
	b_placement_weakness *= placement_factor;


	if (display_factor != 0.0f) {
		main_GUI._eval_components += "King placement weakness: " + to_string(w_placement_weakness) + " / " + to_string(b_placement_weakness) + "\n";
	}


	// ---------------------------------
	// *** MOBILITE VIRTUELLE DU ROI ***
	// ---------------------------------
	
	// FIXME: est-ce vraiment utile? ça casse peut-être tout en fait
	//constexpr int virtual_mobility_danger = 125;
	//w_king_weakness += virtual_mobility_danger * get_king_virtual_mobility(true) * (1 - _adv);
	//b_king_weakness += virtual_mobility_danger * get_king_virtual_mobility(false) * (1 - _adv);

	// ------------------
	// *** PAWN STORM ***
	// ------------------

	int w_pawn_storm = get_pawn_storm(true);
	int b_pawn_storm = get_pawn_storm(false);

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "Pawn storms: " + to_string(w_pawn_storm) + " / " + to_string(b_pawn_storm) + "\n";
	}

	// ------------------
	// *** OPEN LINES ***
	// ------------------

	constexpr int open_lines_danger = 3;

	int w_open_lines = get_open_files_on_opponent_king(true) * open_lines_danger * (1 - _adv);
	int b_open_lines = get_open_files_on_opponent_king(false) * open_lines_danger * (1 - _adv);

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "Open lines: " + to_string(w_open_lines) + " / " + to_string(b_open_lines) + "\n";
	}


	// ------------------
	// *** OPEN DIAGONALS ***
	// ------------------

	constexpr int open_diagonals_danger = 1;

	int w_open_diagonals = get_open_diagonals_on_opponent_king(true) * open_diagonals_danger * (1 - _adv);
	int b_open_diagonals = get_open_diagonals_on_opponent_king(false) * open_diagonals_danger * (1 - _adv);

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "Open diagonals: " + to_string(w_open_diagonals) + " / " + to_string(b_open_diagonals) + "\n";
	}


	// -------------------
	// *** CHECKS ***
	// -------------------

	const int w_checks = get_checks_value(white_controls_map, black_controls_map, true);
	const int b_checks = get_checks_value(white_controls_map, black_controls_map, false);

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "Checks: " + to_string(w_checks) + " / " + to_string(b_checks) + "\n";
	}

	// ---------------------------
	// *** POTENTIEL D'ATTAQUE ***
	// ---------------------------

	// Potentiel d'attaque de chaque pièce (pion, caval, fou, tour, dame)
	static constexpr int attack_potentials[6] = { 1, 25, 28, 50, 100, 0 };
	constexpr int reference_mating_potential = 40; // Matériel minimum pour mater (en général)
	constexpr int reference_potential = 314 - reference_mating_potential; // Si y'a toutes les pièces de base sur l'échiquier

	int w_total_potential = 0;
	int b_total_potential = 0;

	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			if (const uint_fast8_t p = _array[i][j]; p > 0)
				if (p < 7)
					w_total_potential += attack_potentials[p - 1];
				else
					b_total_potential += attack_potentials[(p - 1) % 6];
		}
	}

	w_total_potential = max(0, w_total_potential - reference_mating_potential);
	b_total_potential = max(0, b_total_potential - reference_mating_potential);

	float potential_factor = 1.0f;

	w_total_potential *= potential_factor;
	b_total_potential *= potential_factor;


	// -------------------------------------
	// *** CALCUL DE LA FAIBLESSE DU ROI ***
	// -------------------------------------


	// Facteur multiplicatif en cas de faiblesse négative (pour compenser le court/long terme)
	const float negative_long_term_factor = 1.0f;
	const float negative_short_term_factor = 0.3f;


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
	// Colonnes/Diagonales ouvertes
	// Pawn storm (TODO)
	// Structure de pions autour du roi
	int b_long_term_weakness = w_pawn_storm + w_open_lines + w_open_diagonals - b_king_protection;
	if (b_long_term_weakness < 0) {
		b_long_term_weakness *= negative_long_term_factor;
	}

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "B LONG TERM WEAKNESS: Storm: " + to_string(w_pawn_storm) + " + Lines: " + to_string(w_open_lines) + " + Diags: " + to_string(w_open_diagonals) + " - Protec: " + to_string(b_king_protection) + "= " + to_string(b_long_term_weakness) + "\n";
	}

	// Attaque court terme:
	// Attaque des pièces adverses / Défense des pièces alliées
	// Overload
	// Potentiel de mat
	// Placement du roi
	int b_short_term_weakness = w_checks + w_attacking_overload + b_placement_weakness + b_king_overloaded;
	if (b_short_term_weakness < 0) {
		b_short_term_weakness *= negative_short_term_factor;
	}

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "B SHORT TERM WEAKNESS: Checks: " + to_string(w_checks) + " + Attack: " + to_string(w_attacking_overload) + " + Placement: " + to_string(b_placement_weakness) + " + Overload: " + to_string(b_king_overloaded) + "= " + to_string(b_short_term_weakness) + "\n";
	}

	b_king_weakness = b_long_term_weakness + b_short_term_weakness;


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
	// Colonnes/Diagonales ouvertes
	// Pawn storm (TODO)
	// Structure de pions autour du roi
	int w_long_term_weakness = b_pawn_storm + b_open_lines + b_open_diagonals - w_king_protection;
	if (w_long_term_weakness < 0) {
		w_long_term_weakness *= negative_long_term_factor;
	}

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "W LONG TERM WEAKNESS: Storm: " + to_string(b_pawn_storm) + " + Lines: " + to_string(b_open_lines) + " + Diags: " + to_string(b_open_diagonals) + " - Protec: " + to_string(w_king_protection) + "= " + to_string(w_long_term_weakness) + "\n";
	}

	// Attaque court terme:
	// Attaque des pièces adverses / Défense des pièces alliées
	// Overload
	// Potentiel de mat
	// Placement du roi
	int w_short_term_weakness = b_checks + b_attacking_overload + w_placement_weakness + w_king_overloaded;
	if (w_short_term_weakness < 0) {
		w_short_term_weakness *= negative_short_term_factor;
	}

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "W SHORT TERM WEAKNESS: Checks: " + to_string(b_checks) + " + Attack: " + to_string(b_attacking_overload) + " + Placement: " + to_string(w_placement_weakness) + " + Overload: " + to_string(w_king_overloaded) + "= " + to_string(w_short_term_weakness) + "\n";
	}

	w_king_weakness = w_long_term_weakness + w_short_term_weakness;

	// Potentiel d'attaque
	const float w_attacking_potential = (float)w_total_potential / reference_potential;
	const float b_attacking_potential = (float)b_total_potential / reference_potential;

	if (display_factor != 0.0f) {
		main_GUI._eval_components += "Attacking potential: " + to_string(w_attacking_potential) + " / " + to_string(b_attacking_potential) + "\n";
	}

	// Mise à jour de la king safety en fonction des potentiels d'attaque
	w_king_weakness *= b_attacking_potential;
	b_king_weakness *= w_attacking_potential;

	// Ajout de la protection du roi... la faiblesse du roi ne peut pas être négative (potentiellement à revoir, mais parfois la surprotection donne des valeurs délirantes)
	float overprotection_factor = 0.2f;

	if (w_king_weakness < 0) {
		w_king_weakness *= overprotection_factor;
	}

	if (b_king_weakness < 0) {
		b_king_weakness *= overprotection_factor;
	}


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
		if (_moves[k].i2 == i && _moves[k].j2 == j)
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
			s_white[j] += (_array[i][j] == 1);
			s_black[j] += (_array[i][j] == 7);
			pawns_white[i][j] = (_array[i][j] == 1);
			pawns_black[i][j] = (_array[i][j] == 7);
		}
	}

	// Pions isolés
	constexpr int isolated_pawn = -30;
	constexpr float open_row_factor = 3.0f; // Si le pion isolé est sur une colonne ouverte, il est beaucoup plus faible
	constexpr float isolated_adv_factor = 0.75f; // En fonction de l'advancement de la partie
	const float isolated_adv = 1 * (1 + (isolated_adv_factor - 1) * _adv);
	int isolated_pawns = 0;

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

			isolated_pawns += isolated_pawn * (s_white[col] + is_open_col * (open_row_factor - 1)) / (1 + (col == 0 || col == 7)) * isolated_adv;
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

			isolated_pawns -= isolated_pawn * (s_black[col] + is_open_col * (open_row_factor - 1)) / (1 + (col == 0 || col == 7)) * isolated_adv;
		}
	}

	if (display_factor != 0.0f)
		main_GUI._eval_components += "isolated pawns: " + (isolated_pawns >= 0 ? string("+") : string()) + to_string(static_cast<int>(isolated_pawns * display_factor)) + " | ";

	pawn_structure += isolated_pawns;

	// Pions doublés (ou triplés...)
	constexpr int doubled_pawn = -40;
	constexpr float doubled_adv_factor = 0.5f; // En fonction de l'advancement de la partie
	const float doubled_adv = 1 * (1 + (doubled_adv_factor - 1) * _adv);
	int doubled_pawns = 0;

	for (uint_fast8_t i = 0; i < 8; i++) {
		doubled_pawns += (s_white[i] >= 2) * doubled_pawn * (s_white[i] - 1) * doubled_adv;
		doubled_pawns -= (s_black[i] >= 2) * doubled_pawn * (s_black[i] - 1) * doubled_adv;
	}

	if (display_factor != 0.0f)
		main_GUI._eval_components += "doubled pawns: " + (doubled_pawns >= 0 ? string("+") : string()) + to_string(static_cast<int>(doubled_pawns * display_factor)) + " | ";

	pawn_structure += doubled_pawns;

	// Pions passés
	// r5k1/P3Rpp1/7p/8/3p4/8/2P2PPP/6K1 b - - 1 30 : position à tester
	// 8/P5kp/8/q7/8/8/5B1P/5K2 w - - 1 57 : ici, il faut considérer la case controlée (par rayon X)

	// Table de valeur des pions passés en fonction de leur avancement sur le plateau
	static const int passed_pawns[8] = { 0, 50, 50, 120, 235, 400, 750, 0 };

	// Pion passé - chemin controllé par une pièce adverse
	static const int controlled_passed_pawn[8] = { 0, 40, 40, 70, 130, 220, 325, 0 };

	// Pion passé bloqué
	static const int blocked_passed_pawn[8] = { 0, 20, 30, 45, 90, 140, 250, 0 };


	constexpr float passed_adv_factor = 2.0f; // En fonction de l'advancement de la partie
	const float passed_adv = 1 * (1 + (passed_adv_factor - 1) * _adv);
	int passed_pawns_value = 0;

	// Pour chaque colonne
	for (uint_fast8_t col = 0; col < 8; col++) {

		// On prend en compte seulement le pion le plus avancé de la colonne (car les autre seraient bloqués derrière)

		// Pions blancs
		if (s_white[col] >= 1) {
			// On regarde de la rangée la plus proche de la promotion, jusqu'a la première
			for (uint_fast8_t row = 6; row > 0; row--) {
				// S'il y a un pion potentiellement passé
				if (pawns_white[row][col]) {
					// Pas de pion sur une colonne adjacente ou pareille avec une lattitude supérieure (strictement)
					bool is_passed_pawn = true;
					for (uint_fast8_t k = row + 1; k < 7; k++)
						if ((col > 0 && _array[k][col - 1] == 7) || _array[k][col] == 7 || (col < 7 && _array[k][col + 1] == 7)) {
							is_passed_pawn = false;
							break;
						}

					// Si c'est un pion passé
					if (is_passed_pawn) {

						// Regarde s'il y a un bloqueur
						bool blocked = false;
						for (uint_fast8_t k = row + 1; k <= 7; k++) {
							if (_array[k][col] != 0) {
								blocked = true;
								break;
							}
						}

						// S'il n'est pas bloqué, regarde s'il est controllé par une pièce adverse
						bool controlled = false;

						if (!blocked) {

							// On retire le pion pour regarder si la case est controlée par rayon X
							_array[row][col] = 0;

							for (uint_fast8_t k = row + 1; k <= 7; k++) {
								if (is_controlled(k, col, true) && !is_controlled(k, col, false)) {
									controlled = true;
									break;
								}
							}

							// TODO: peut-être à revoir...

							// Si c'est controlé par une pièce alliée, annule le bonus
							//if (controlled) {
							//	for (uint_fast8_t k = row + 1; k <= 7; k++) {
							//		if (is_controlled(k, col, false)) {
							//			controlled = false;
							//			break;
							//		}
							//	}
							//}

							// On remet le pion
							_array[row][col] = 1;
						}

						// Ajoute la valeur du pion passé
						passed_pawns_value += (blocked ? blocked_passed_pawn[row] : (controlled ? controlled_passed_pawn[row] : passed_pawns[row])) * passed_adv;
					}

				}
			}
		}

		// Pions noirs
		if (s_black[col] >= 1) {
			for (uint_fast8_t row = 1; row < 7; row++) {
				// Pas de pion sur une colonne adjacente ou pareille avec une lattitude inférieure (strictement)
				if (pawns_black[row][col]) {
					bool is_passed_pawn = true;
					for (uint_fast8_t k = row - 1; k > 0; k--)
						if ((col > 0 && _array[k][col - 1] == 1) || _array[k][col] == 1 || (col < 7 && _array[k][col + 1] == 1))
						{
							is_passed_pawn = false;
							break;
						}

					// Si c'est un pion passé
					if (is_passed_pawn) {

						// Regarde s'il y a un bloqueur
						bool blocked = false;
						for (int_fast8_t k = row - 1; k >= 0; k--) {
							if (_array[k][col] != 0) {
								blocked = true;
								break;
							}
						}
							

						// S'il n'est pas bloqué, regarde s'il est controllé par une pièce adverse
						bool controlled = false;

						if (!blocked) {

							// On retire le pion pour regarder si la case est controlée par rayon X
							_array[row][col] = 0;

							for (int_fast8_t k = row - 1; k >= 0; k--) {
								if (is_controlled(k, col, false) && !is_controlled(k, col, true)) {
									controlled = true;
									break;
								}
							}
							
							// Si c'est controlé par une pièce alliée, annule le bonus
							//if (controlled) {
							//	for (int_fast8_t k = row - 1; k >= 0; k--) {
							//		if (is_controlled(k, col, true)) {
							//			controlled = false;
							//			break;
							//		}
							//	}
							//}

							// On remet le pion
							_array[row][col] = 7;
						}

						// Ajoute la valeur du pion passé
						passed_pawns_value -= (blocked ? blocked_passed_pawn[7 - row] : (controlled ? controlled_passed_pawn[7 - row] : passed_pawns[7 - row])) * passed_adv;
					}
				}
			}
		}
	}

	if (display_factor != 0.0f)
		main_GUI._eval_components += "passed pawns: " + (passed_pawns_value >= 0 ? string("+") : string()) + to_string(static_cast<int>(passed_pawns_value * display_factor)) + " || ";

	pawn_structure += passed_pawns_value;


	// Pions connectés
	// Un pion est dit connecté, s'il y a un pion de la même couleur sur une colonne adjacente sur la même rangée ou la rangée inférieure
	constexpr int connected_pawns[8] = { 0, 25, 40, 60, 90, 135, 225, 0 };
	constexpr float connected_pawns_factor = 0.7f; // En fonction de l'advancement de la partie
	const float connected_pawns_adv = 1 * (1 + (connected_pawns_factor - 1) * _adv);

	int connected_pawns_value = 0;

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
		main_GUI._eval_components += "connected pawns: " + (connected_pawns_value >= 0 ? string("+") : string()) + to_string(static_cast<int>(connected_pawns_value * display_factor)) + " || ";

	pawn_structure += connected_pawns_value;


	// TODO : pions arriérés


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

	// Tableau des valeurs d'attaques des pièces (0 = pion, 1 = caval, 2 = fou, 3 = tour, 4 = dame, 5 = roi)
	static constexpr int attacks_array[6][6] = {
	//   P    N    B     R    Q    K
		{15,  75,  75, 100, 200,  70}, // P
		{25,  25,  30,  70, 100,  80}, // N
		{25,  30,  25,  50,  80,  40}, // B
		{25,  15,  15,  25,  60,  40}, // R
		{15,  10,  10,  20,  35,  60}, // Q
		{10,  10,  10,  15,  20,   0}, // K
	};

	// Tableau des valeurs de défenses des pièces (0 = pion, 1 = caval, 2 = fou, 3 = tour, 4 = dame, 5 = roi)
	static constexpr int defenses_array[6][6] = {
	//   P    N    B     R    Q    K
		{40,  25,  15,  10,   5,   0}, // P
		{20,  10,  10,   5,   5,   0}, // N
		{20,  10,  10,   5,   5,   0}, // B
		{15,   5,   5,  50,  10,   0}, // R
		{15,   5,   5,  10,  20,   0}, // Q
		{10,   5,   5,   5,   5,   0}, // K
	};

	// TODO ne pas additionner la défense de toutes les pièces? seulement regarder les pièces non-défendues? (sinon devient pleutre)
	// FIXME : les attaques/défenses par rayon X ne sont pas prises en compte
	// TODO : utiliser les offsets des cavalier plutôt que ces boucles dégueu


	// Diagonales
	static constexpr int_fast8_t dx[] = { -1, -1, 1, 1 };
	static constexpr int_fast8_t dy[] = { -1, 1, -1, 1 };

	// Mouvements rectilignes
	static constexpr int_fast8_t vx[] = { -1, 1, 0, 0 }; // vertical
	static constexpr int_fast8_t hy[] = { 0, 0, -1, 1 }; // horizontal



	// Tableau d'attaques pour les blancs
	int attacks_white[8][8] = { 0 };

	// Tableau d'attaques pour les noirs
	int attacks_black[8][8] = { 0 };


	// TODO : utiliser des constantes pour les calculs redondants


	// TODO changer les if par des &&

	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			uint_fast8_t p = _array[i][j];
			switch (p) {

			// Pion blanc
			case 1:
				
				if (j > 0) {
					uint_fast8_t i2 = i + 1;
					uint_fast8_t j2 = j - 1;
					uint_fast8_t p2 = _array[i2][j2]; // Case haut-gauche du pion blanc
					if (p2 >= 7)
						attacks_white[i2][j2] += attacks_array[0][p2 - 7];
					else
						p2 && (attacks_black[i2][j2] -= defenses_array[0][p2 - 1]);
				}
				if (j < 7) {
					uint_fast8_t i2 = i + 1;
					uint_fast8_t j2 = j + 1;
					uint_fast8_t p2 = _array[i2][j2]; // Case haut-droit du pion blanc
					if (p2 >= 7)
						attacks_white[i2][j2] += attacks_array[0][p2 - 7];
					else
						p2 && (attacks_black[i2][j2] -= defenses_array[0][p2 - 1]);
				}
				break;

			// Cavalier blanc
			case 2:
				for (int k = -2; k <= 2; k++) {
					for (int l = -2; l <= 2; l++) {
						if (k * l == 0) continue;
						if (abs(k) + abs(l) != 3) continue;
						uint_fast8_t i2 = i + k; uint_fast8_t j2 = j + l;
						if (is_in_fast(i2, 0, 7) && is_in_fast(j2, 0, 7)) {
							uint_fast8_t p2 = _array[i2][j2];
							if (p2 >= 7)
								attacks_white[i2][j2] += attacks_array[1][p2 - 7];
							else
								p2 && (attacks_black[i2][j2] -= defenses_array[1][p2 - 1]);
						}
					}
				}
				break;

			// Fou blanc
			case 3:
				// Pour chaque diagonale
				for (int idx = 0; idx < 4; ++idx) {
					uint_fast8_t i2 = i;
					uint_fast8_t j2 = j;
					int lim = min(dx[idx] == 1 ? 7 - i : i, dy[idx] == 1 ? 7 - j : j);

					while (lim > 0) {
						i2 += dx[idx];
						j2 += dy[idx];
						uint_fast8_t p2 = _array[i2][j2];
						if (p2 != 0) {
							if (p2 >= 7)
								attacks_white[i2][j2] += attacks_array[2][p2 - 7];
							else
								p2 && (attacks_black[i2][j2] -= defenses_array[2][p2 - 1]);
							break;
						}
						lim--;
					}
				}
				break;

			// Tour blanche
			case 4:
				// Pour chaque mouvement rectiligne
				for (int idx = 0; idx < 4; ++idx) {
					uint_fast8_t i2 = i;
					uint_fast8_t j2 = j;
					int lim = vx[idx] == -1 ? i : (vx[idx] == 1 ? 7 - i : (hy[idx] == -1 ? j : 7 - j));

					while (lim > 0) {
						i2 += vx[idx];
						j2 += hy[idx];
						uint_fast8_t p2 = _array[i2][j2];
						if (p2 != 0) {
							if (p2 >= 7)
								attacks_white[i2][j2] += attacks_array[3][p2 - 7];
							else
								p2 && (attacks_black[i2][j2] -= defenses_array[3][p2 - 1]);
							break;
						}
						lim--;
					}
				}
				break;

			// Dame blanche
			case 5:
				// Pour chaque diagonale
				for (int idx = 0; idx < 4; ++idx) {
					uint_fast8_t i2 = i;
					uint_fast8_t j2 = j;
					int lim = min(dx[idx] == 1 ? 7 - i : i, dy[idx] == 1 ? 7 - j : j);

					while (lim > 0) {
						i2 += dx[idx];
						j2 += dy[idx];
						uint_fast8_t p2 = _array[i2][j2];
						if (p2 != 0) {
							if (p2 >= 7)
								attacks_white[i2][j2] += attacks_array[4][p2 - 7];
							else
								p2 && (attacks_black[i2][j2] -= defenses_array[4][p2 - 1]);
							break;
						}
						lim--;
					}
				}

				// Pour chaque mouvement rectiligne
				for (int idx = 0; idx < 4; ++idx) {
					uint_fast8_t i2 = i;
					uint_fast8_t j2 = j;
					int lim = vx[idx] == -1 ? i : (vx[idx] == 1 ? 7 - i : (hy[idx] == -1 ? j : 7 - j));

					while (lim > 0) {
						i2 += vx[idx];
						j2 += hy[idx];
						uint_fast8_t p2 = _array[i2][j2];
						if (p2 != 0) {
							if (p2 >= 7)
								attacks_white[i2][j2] += attacks_array[4][p2 - 7];
							else
								p2 && (attacks_black[i2][j2] -= defenses_array[4][p2 - 1]);
							break;
						}
						lim--;
					}
				}
				break;

			// Roi blanc
			case 6:
				for (int k = -1; k < 2; k++) {
					for (int l = -1; l < 2; l++) {
						if (k || l) {
							uint_fast8_t i2 = i + k; uint_fast8_t j2 = j + l;
							if (is_in_fast(i2, 0, 7) && is_in_fast(j2, 0, 7)) {
								uint_fast8_t p2 = _array[i2][j2];
								if (p2 >= 7)
									attacks_white[i2][j2] += attacks_array[5][p2 - 7];
								else
									p2 && (attacks_black[i2][j2] -= defenses_array[5][p2 - 1]);
							}
						}
					}
				}
				break;

			// Pion noir
			case 7:
				if (j > 0) {
					uint_fast8_t i2 = i - 1;
					uint_fast8_t j2 = j - 1;
					uint_fast8_t p2 = _array[i2][j2]; // Case bas-gauche du pion noir
					if (p2 >= 1 && p2 <= 6)
						attacks_black[i2][j2] += attacks_array[0][p2 - 1];
					else
						p2 && (attacks_white[i2][j2] -= defenses_array[0][p2 - 7]);
				}
				if (j < 7) {
					uint_fast8_t i2 = i - 1;
					uint_fast8_t j2 = j + 1;
					uint_fast8_t p2 = _array[i2][j2]; // Case bas-droit du pion noir
					if (p2 >= 1 && p2 <= 6)
						attacks_black[i2][j2] += attacks_array[0][p2 - 1];
					else
						p2 && (attacks_white[i2][j2] -= defenses_array[0][p2 - 7]);
				}
				break;
			
			// Cavalier noir
			case 8:
				for (int k = -2; k <= 2; k++) {
					for (int l = -2; l <= 2; l++) {
						if (k * l == 0) continue;
						if (abs(k) + abs(l) != 3) continue;
						uint_fast8_t i2 = i + k; uint_fast8_t j2 = j + l;
						if (is_in_fast(i2, 0, 7) && is_in_fast(j2, 0, 7)) {
							uint_fast8_t p2 = _array[i2][j2];
							if (p2 >= 1 && p2 <= 6)
								attacks_black[i2][j2] += attacks_array[1][p2 - 1];
							else
								p2 && (attacks_white[i2][j2] -= defenses_array[1][p2 - 7]);
						}
					}
				}
				break;

			// Fou noir
			case 9:
				// Pour chaque diagonale
				for (int idx = 0; idx < 4; ++idx) {
					uint_fast8_t i2 = i;
					uint_fast8_t j2 = j;
					int lim = min(dx[idx] == 1 ? 7 - i : i, dy[idx] == 1 ? 7 - j : j);

					while (lim > 0) {
						i2 += dx[idx];
						j2 += dy[idx];
						uint_fast8_t p2 = _array[i2][j2];
						if (p2 != 0) {
							if (p2 >= 1 && p2 <= 6)
								attacks_black[i2][j2] += attacks_array[2][p2 - 1];
							else
								p2 && (attacks_white[i2][j2] -= defenses_array[2][p2 - 7]);
							break;
						}
						lim--;
					}
				}
				break;

			// Tour noire
			case 10:
				// Pour chaque mouvement rectiligne
				for (int idx = 0; idx < 4; ++idx) {
					uint_fast8_t i2 = i;
					uint_fast8_t j2 = j;
					int lim = vx[idx] == -1 ? i : (vx[idx] == 1 ? 7 - i : (hy[idx] == -1 ? j : 7 - j));

					while (lim > 0) {
						i2 += vx[idx];
						j2 += hy[idx];
						uint_fast8_t p2 = _array[i2][j2];
						if (p2 != 0) {
							if (p2 >= 1 && p2 <= 6)
								attacks_black[i2][j2] += attacks_array[3][p2 - 1];
							else
								p2 && (attacks_white[i2][j2] -= defenses_array[3][p2 - 7]);
							break;
						}
						lim--;
					}
				}
				break;

			// Dame noire
			case 11:
				// Pour chaque diagonale
				for (int idx = 0; idx < 4; ++idx) {
					uint_fast8_t i2 = i;
					uint_fast8_t j2 = j;
					int lim = min(dx[idx] == 1 ? 7 - i : i, dy[idx] == 1 ? 7 - j : j);

					while (lim > 0) {
						i2 += dx[idx];
						j2 += dy[idx];
						uint_fast8_t p2 = _array[i2][j2];
						if (p2 != 0) {
							if (p2 >= 1 && p2 <= 6)
								attacks_black[i2][j2] += attacks_array[4][p2 - 1];
							else
								p2 && (attacks_white[i2][j2] -= defenses_array[4][p2 - 7]);
							break;
						}
						lim--;
					}
				}

				// Pour chaque mouvement rectiligne
				for (int idx = 0; idx < 4; ++idx) {
					uint_fast8_t i2 = i;
					uint_fast8_t j2 = j;
					int lim = vx[idx] == -1 ? i : (vx[idx] == 1 ? 7 - i : (hy[idx] == -1 ? j : 7 - j));

					while (lim > 0) {
						i2 += vx[idx];
						j2 += hy[idx];
						uint_fast8_t p2 = _array[i2][j2];
						if (p2 != 0) {
							if (p2 >= 1 && p2 <= 6)
								attacks_black[i2][j2] += attacks_array[4][p2 - 1];
							else
								p2 && (attacks_white[i2][j2] -= defenses_array[4][p2 - 7]);
							break;
						}
						lim--;
					}
				}
				break;

			// Roi noir
			case 12:
				for (int k = -1; k < 2; k++) {
					for (int l = -1; l < 2; l++) {
						if (k || l) {
							uint_fast8_t i2 = i + k; uint_fast8_t j2 = j + l;
							if (is_in_fast(i2, 0, 7) && is_in_fast(j2, 0, 7)) {
								uint_fast8_t p2 = _array[i2][j2];
								if (p2 >= 1 && p2 <= 6)
									attacks_black[i2][j2] += attacks_array[5][p2 - 1];
								else
									p2 && (attacks_white[i2][j2] -= defenses_array[5][p2 - 7]);
							}
						}
					}
				}
				break;
			}
		}
	}


	// Somme toutes les valeurs positives des tableaux d'attaques pour chaque camp
	int white_attacks_eval = 0;
	int black_attacks_eval = 0;

	// Facteur de défense
	float defense_factor = 0.2f;

	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			white_attacks_eval += attacks_white[i][j] * (attacks_white[i][j] > 0 ? 1 : defense_factor);
			black_attacks_eval += attacks_black[i][j] * (attacks_black[i][j] > 0 ? 1 : defense_factor);
		}
	}

	return white_attacks_eval - black_attacks_eval;
}

// Fonction qui calcule l'opposition des rois (en finales de pions)
int Board::get_kings_opposition() {
	// Regarde si on est dans une finale de pions
	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			const uint_fast8_t p = _array[i][j];
			if (p != 0 && p != 1 && p != 6 && p != 7 && p != 12)
				return 0;
		}
	}

	// Met à jour la position des rois
	update_kings_pos();

	// Les rois sont-ils opposés?
	const int di = abs(_white_king_pos.i - _black_king_pos.i);
	const int dj = abs(_white_king_pos.j - _black_king_pos.j);
	if (!((di == 0 || di == 2) && (dj == 0 || dj == 2)))
		return 0;

	// S'ils sont opposés, le joueur qui a l'opposition, est celui qui n'a pas le trait
	return -get_color();
}

// Fonction qui renvoie le type de pièce sélectionnée
uint_fast8_t Board::selected_piece() const
{
	// Faut-il stocker cela pour éviter de le re-calculer?
	if (main_GUI._selected_pos.i == -1 || main_GUI._selected_pos.j == -1)
		return 0;
	return _array[main_GUI._selected_pos.i][main_GUI._selected_pos.j];
}

// Fonction qui renvoie le type de pièce où la souris vient de cliquer
uint_fast8_t Board::clicked_piece() const
{
	if (main_GUI._clicked_pos.i == -1 || main_GUI._clicked_pos.j == -1)
		return 0;
	return _array[main_GUI._clicked_pos.i][main_GUI._clicked_pos.j];
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
int Board::get_rooks_on_open_file() const
{
	// TODO : j'ai fait nimporque quoi ici... à revoir

	int rook_open = 0;

	// Pions sur les colonnes, tours sur les colonnes
	uint_fast8_t w_pawns[8] = { 0 };
	uint_fast8_t b_pawns[8] = { 0 };
	uint_fast8_t w_rooks[8] = { 0 };
	uint_fast8_t b_rooks[8] = { 0 };

	// TODO Calculer le nombre de colonnes ouvertes et semi-ouvertes -> diviser le résultat par ce nombre...

	// Calcul du nombre de pions et tours par colonne
	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			const uint_fast8_t p = _array[i][j];
			(p == 1) ? w_pawns[j]++ : ((p == 4) ? w_rooks[j]++ : ((p == 7) ? b_pawns[j]++ : (p == 10) && b_rooks[j]++));
		}
	}

	// Nombre de colonnes ouvertes
	uint_fast8_t open_files = 0;

	// Tour sur les colonnes ouvertes
	constexpr int open_value = 50;

	for (uint_fast8_t i = 0; i < 8; i++) {
		(w_rooks[i] && !w_pawns[i] && !b_pawns[i]) && (rook_open += w_rooks[i] * open_value);
		(b_rooks[i] && !b_pawns[i] && !w_pawns[i]) && (rook_open -= b_rooks[i] * open_value);
		!w_pawns[i] && !b_pawns[i] && open_files++;
	}

	// Tour sur les colonnes semi-ouvertes
	constexpr int semi_open_value = 25;

	for (uint_fast8_t i = 0; i < 8; i++)
	{
		(w_rooks[i] && !w_pawns[i] && b_pawns[i]) && (rook_open += w_rooks[i] * semi_open_value);
		(b_rooks[i] && !b_pawns[i] && w_pawns[i]) && (rook_open -= b_rooks[i] * semi_open_value);
	}

	// L'importance est moindre s'il y a plusieurs colonnes ouvertes
	return rook_open / (open_files == 0 ? 0.5f : open_files);
}

// Fonction qui calcule la valeur des cases controllées sur l'échiquier
int Board::get_square_controls() const
{
	// TODO ajouter des valeurs pour le contrôle des cases par les pièces?

	// Valeur du contrôle de chaque case (pour les pions)
	static constexpr int square_controls[8][8] = {
		{10,  10,  10,  10,  10,  10,  10,  10},
		{20,  20,  20,  20,  20,  20,  20,  20},
		{10,  20,  30,  40,  40,  30,  20,  10},
		{5,   10,  40,  50,  50,  40,  10,   5},
		{0,    5,  20,  40,  40,  20,   5,   0},
		{5,   -5,   5,  10,  10,   5,  -5,   5},
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
	constexpr float control_adv_factor = -0.5f; // En fonction de l'advancement de la partie

	return total_control * max(0.0f, (1 + (control_adv_factor - 1) * _adv));
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
		const int piece = _array[move.i1][move.j1];

		// Prise
		const int captured_value = captures_values[_array[move.i2][move.j2]];

		// Valeur de la pièce qui prend
		const int capturer_value = captures_values[piece];

		// Valeur du coup
		//int move_value = captured_value == 0 ? 0 : captured_value / capturer_value;

		// Alternative plus précise
		int move_value = 0;
		if (captured_value != 0) {
			move_value = captured_value - capturer_value;
			move_value += (move_value < 0 && is_controlled(move.i2, move.j2, _player)) ? move_value : 10000;
		}

		// Promotion
		// Blancs
		move_value += (piece == 1 && move.i2 == 7) * promotion_value;

		// Noirs
		move_value += (piece == 7 && move.i2 == 0) * promotion_value;

		// Assignation de la valeur
		moves_values[i] = move_value;
	}


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

// Fonction qui fait un quiescence search
// TODO: améliorer avec un delta pruning
// TODO: utiliser le buffer de plateaux pour remplir la réflexion
int Board::quiescence(Evaluator* eval, int alpha, const int beta, int depth, bool explore_checks, bool main_player, int delta)
{
	// Compte le nombre de noeuds visités
	//_quiescence_nodes = 1;
	
	// Si la partie est terminée
	is_game_over();
	if (_game_over_value == 2) // Nulle
		return 0;
	if (_game_over_value != 0) // Mat
		return -mate_value + _moves_count * mate_ply;

	// Evalue la position initiale
	evaluate(eval);
	const int stand_pat = _evaluation * get_color();

	// Si on est en échec (pour ne pas terminer les variantes sur un échec)
	bool check_extension = in_check();

	if (depth == 0)
		return stand_pat;
		

	// Beta cut-off
	if (stand_pat >= beta) {
		//cout << "Beta cut-off1: " << stand_pat << " >= " << beta << endl;
		return beta;
	}
	else {
		//cout << "No beta cut-off1: " << stand_pat << " < " << beta << endl;
	}

	// Mise à jour de alpha si l'éval statique est plus grande
	// Pas de stand_pat si on est en échec
	if (alpha < stand_pat && !check_extension)
		alpha = stand_pat;

	// Trie rapidement les coups
	sort_moves();

	for (int i = 0; i < _got_moves; i++) {
		// TODO : ajouter promotions et échecs
		// TODO : utiliser des flags

		// Coup
		Move move = _moves[i];

		// Si c'est une capture
		if (_array[move.i2][move.j2] != 0 || check_extension) {
			//cout << "Capture, depth: " << depth << " | move: " << move_label(move) << " | check_extension: " << check_extension << endl;

			Board b;
			b.copy_data(*this);
			b.make_move(move);

			const int score = -b.quiescence(eval, -beta, -alpha, depth - 1, explore_checks, true, delta);
			//_quiescence_nodes += b._quiescence_nodes;

			if (score >= beta) {
				//cout << "Beta cut-off2: " << score << " >= " << beta << endl;
				return beta;
			}

			if (score > alpha)
				alpha = score;

			// Delta pruning (TODO : à tester)
			//if (alpha >= beta - delta)
			//	return alpha;
		}

		// Mats
		// TODO : utiliser les flags 'échec' pour savoir s'il faut regarder ce coup
		else if (explore_checks)
		{
			Board b;
			b.copy_data(*this); // FIXME: Est-ce qu'on doit regarder les répétitions ici?
			b.make_move(move);

			if (!main_player || b.in_check())
			{
				//cout << "Check, depth: " << depth << " | move: " << move_label(move) << " | check_extension: " << check_extension << endl;

				const int score = -b.quiescence(eval, -beta, -alpha, depth - 1, explore_checks, !main_player, delta);
				//_quiescence_nodes += b._quiescence_nodes;

				if (score >= beta) {
					//cout << "Beta cut-off3: " << score << " >= " << beta << endl;
					return beta;
				}

				if (score > alpha)
					alpha = score;

				// Delta pruning (TODO : à tester)
				//if (alpha >= beta - delta)
				//	return alpha;
			}
		}
	}

	return alpha;
}

// Fonction qui fait cliquer le coup m
bool Board::click_m_move(const Move m, const bool orientation) const
{
	simulate_mouse_release();
	click_move(m.i1, m.j1, m.i2, m.j2 , main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom, orientation);
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

	for (uint_fast8_t i = 0; i < 8; i++)
		for (uint_fast8_t j = 0; j < 8; j++)
			if (is_in(_array[i][j], 2, 6))
				w_pieces++;
			else if (is_in(_array[i][j], 8, 12))
				b_pieces++;

	// Nombre de colonnes ouvertes
	int open_rows = 0;
	for (uint_fast8_t j = 0; j < 8; j++)
	{
		bool open = true;
		for (uint_fast8_t i = 0; i < 8; i++)
		{
			if (_array[i][j] == 1 || _array[i][j] == 7)
			{
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
	int w_space[3][4];
	int b_space[3][4];

	// Assigne les valeurs pour chaque case
	for (uint_fast8_t j = 2; j < 6; j++)
	{
		// Inutile de calculer si le poids est nul
		if (w_weight != 0)
		{
			// Blancs
			for (uint_fast8_t i = 1; i <= 3; i++)
			{
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
			for (uint_fast8_t i = 6; i >= 4; i--)
			{
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

	if (w_weight != 0 || b_weight != 0)
	{
		// Calcule l'avantage d'espace
		for (uint_fast8_t i = 0; i < 3; i++)
		{
			for (uint_fast8_t j = 0; j < 4; j++)
			{
				space_area += w_space[i][j] * w_weight - b_space[i][j] * b_weight;
			}
		}
	}

	constexpr float space_adv_factor = -1.0f; // En fonction de l'advancement de la partie

	return space_area * max(0.0f, (1 + (space_adv_factor - 1) * _adv));
}

// Fonction qui calcule et renvoie une évaluation des vis-à-vis
// TODO : à finir
int Board::get_alignments() const
{
	// Il faut prendre en compte le nombre de pions/pieces au milieu du vis-à-vis

	// Puissance de base des vis-à-vis
	constexpr int rook_queen = 30;
	constexpr int bishop_queen = 25;
	constexpr int rook_king = 50;
	constexpr int bishop_king = 35;
	constexpr int queen_king_rect = 60;
	constexpr int queen_king_diag = 50;

	// Protection contre les vis-à-vis
	constexpr int ally_pawn_rect = 15;
	constexpr int enemy_pawn_rect = 10;
	constexpr int ally_pawn_diag = 5;
	constexpr int enemy_pawn_diag = 10;

	// Valeur totale des vis-à-vis
	int alignments = 0;

	// Regarde toutes les pièces du plateau
	for (uint_fast8_t i = 0; i < 8; i++)
	{
		for (uint_fast8_t j = 0; j < 8; j++)
		{
			const uint_fast8_t piece = _array[i][j];

			// Dame blanche
			if (piece == 5)
			{
				// Cherche les tours noires sur la même ligne ou colonne
				constexpr int directions[4][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };  // Bas, haut, gauche, droite

				for (int direction = 0; direction < 4; direction++) {
					int count = 0;
					const int step_i = directions[direction][0];
					const int step_j = directions[direction][1];

					int row = (step_i == 0) ? i : (step_i == 1) ? 0 : 7;
					int col = (step_j == 0) ? j : (step_j == 1) ? 0 : 7;

					// Parcourir les cases dans la direction sélectionnée
					while (row != i || col != j) {
						const int piece_b = _array[row][col];

						if (piece_b == 10)
							count += rook_queen;
						else if (count > 0 && piece_b == 1)
							count -= enemy_pawn_rect;
						else if (count > 0 && piece_b == 7)
							count -= ally_pawn_rect;

						row += step_i;
						col += step_j;
					}

					alignments -= max(count, 0);
				}

				// Cherche les fous noirs sur la même diagonale
				constexpr int directions_diag[4][2] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1} };  // Bas gauche, bas droite, haut gauche, haut droite

				for (int direction = 0; direction < 4; direction++)
				{
					int count = 0;
					const int step_i = directions_diag[direction][0];
					const int step_j = directions_diag[direction][1];

					// Commence sur la même diagonale que la dame, au bout de l'échiquier
					int row = i - step_i;
					int col = j - step_j;
					if (row < 0 || row > 7 || col < 0 || col > 7)
						continue;
					while (row != 0 && row != 7 && col != 0 && col != 7) {
						row -= step_i, col -= step_j;
					}

					// Parcourir les cases dans la direction sélectionnée
					while (row != i || col != j)
					{
						const int piece_b = _array[row][col];

						if (piece_b == 9)
							count += bishop_queen;
						else if (count > 0 && piece_b == 1)
							count -= enemy_pawn_diag;
						else if (count > 0 && piece_b == 7)
							count -= ally_pawn_diag;

						row += step_i;
						col += step_j;
					}

					alignments -= max(count, 0);
				}
			}
		}
	}

	return alignments;
}

// Fonction qui met à jour la position des rois
bool Board::update_kings_pos()
{
	// Regarde si la position des rois est déjà connue
	bool search_white = _array[_white_king_pos.i][_white_king_pos.j] != 6;
	bool search_black = _array[_black_king_pos.i][_black_king_pos.j] != 12;

	// Affiche la position des rois (supposée), ainsi que la valeur de la pièce sur cette case
	/*cout << "White king pos : " << _white_king_pos.i << " " << _white_king_pos.j << " " << (int)_array[_white_king_pos.i][_white_king_pos.j] << endl;
	cout << "Black king pos : " << _black_king_pos.i << " " << _black_king_pos.j << " " << (int)_array[_black_king_pos.i][_black_king_pos.j] << endl;
	cout << search_white << " " << search_black << endl;*/

	if (!search_white && !search_black)
		return true;

	// Parcourt le plateau
	for (uint_fast8_t i = 0; i < 8; i++)
	{
		for (uint_fast8_t j = 0; j < 8; j++)
		{
			if (const uint_fast8_t piece = _array[i][j]; search_white && piece == 6)
			{
				_white_king_pos = { i, j };
				if (!search_black)
					return true;
				search_white = false;
			}
			else if (search_black && piece == 12)
			{
				_black_king_pos = { i, j };
				if (!search_white)
					return true;
				search_black = false;
			}
		}
	}

	return false;
}

// Fonction qui renvoie la puissance de défense d'une pièce pour le roi allié
int Board::get_piece_defense_power(const int i, const int j) const
{
	const uint_fast8_t piece = _array[i][j];

	// Renvoie 0 si la case est vide
	if (piece == 0)
		return 0;

	//return 0;

	// Tableaux des puissances d'attaque

	// Pion
	static constexpr int pawn_defensing_power_map[8][8] = {
	{0, 150, 25, 0, 0, 0, 0, 0},
	{250, 175, 35, 0, 0, 0, 0, 0},
	{150, 100, 50, 0, 0, 0, 0, 0},
	{75, 15, 10, 15, 0, 0, 0, 0},
	{35, 0, 0, 0, 5, 0, 0, 0},
	{15, 0, 0, 0, 0, 0, 0, 0},
	{5, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0}
	};

	// Cavalier
	static constexpr int knight_defensing_power_map[8][8] = {
	{0, 45, 25, 10, 0, 0, 0, 0},
	{95, 35, 10, 0, 0, 0, 0, 0},
	{60, 50, 35, 5, 0, 0, 0, 0},
	{35, 25, 10, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0}
	};

	// Fou
	static constexpr int bishop_defensing_power_map[8][8] = {
	{   0,   35,   25,    0,    0,    0,    0,    0 },
	{  65,   35,   15,    0,    0,    0,    0,    0 },
	{  55,   35,   15,    5,    0,    0,    0,    0 },
	{  25,   20,   15,    5,    5,    0,    0,    0 },
	{   0,    0,    0,    5,    10,    5,    0,    0 },
	{   0,    0,    0,    0,    5,    10,    5,    0 },
	{   0,    0,    0,    0,    0,    5,    10,    5 },
	{   0,    0,    0,    0,    0,    0,    5,    10 }
	};

	// Tour
	static constexpr int rook_defensing_power_map[8][8] = {
	{0, 15, 5, 5, 5, 5, 5, 5},
	{30, 25, 10, 10, 10, 10, 10, 10},
	{25, 10, 0, 0, 0, 0, 0, 0},
	{10, 5, 0, 0, 0, 0, 0, 0},
	{10, 5, 0, 0, 0, 0, 0, 0},
	{10, 5, 0, 0, 0, 0, 0, 0},
	{10, 5, 0, 0, 0, 0, 0, 0},
	{10, 5, 0, 0, 0, 0, 0, 0}
	};

	// Dame
	static constexpr int queen_defensing_power_map[8][8] = {
	{0, 95, 45, 15, 0, 0, 0, 0},
	{150, 120, 60, 15, 0, 0, 0, 0},
	{185, 150, 90, 30, 0, 0, 0, 0},
	{60, 95, 75, 15, 0, 0, 0, 0},
	{30, 15, 5, 0, 0, 0, 0, 0},
	{15, 0, 0, 0, 0, 0, 0, 0},
	{5, 0, 0, 0, 0, 0, 0, 0},
	{5, 0, 0, 0, 0, 0, 0, 0}
	};

	// Renvoie la valeur correspondante à la puissance d'attaque de la pièce

	// Pièces blanches
	if (piece == 1)
		return (i < _white_king_pos.i) ? 0 : pawn_defensing_power_map[i - _white_king_pos.i][abs(j - _white_king_pos.j)];
	if (piece == 2)
		return (i < _white_king_pos.i) ? 0 : knight_defensing_power_map[i - _white_king_pos.i][abs(j - _white_king_pos.j)];
	if (piece == 3)
		return (i < _white_king_pos.i) ? 0 : bishop_defensing_power_map[i - _white_king_pos.i][abs(j - _white_king_pos.j)];
	if (piece == 4)
		return (i < _white_king_pos.i) ? 0 : rook_defensing_power_map[i - _white_king_pos.i][abs(j - _white_king_pos.j)];
	if (piece == 5)
		return (i < _white_king_pos.i) ? 0 : queen_defensing_power_map[i - _white_king_pos.i][abs(j - _white_king_pos.j)];

	// Pièces noires
	if (piece == 7)
		return (i > _black_king_pos.i) ? 0 : pawn_defensing_power_map[_black_king_pos.i - i][abs(j - _black_king_pos.j)];
	if (piece == 8)
		return (i > _black_king_pos.i) ? 0 : knight_defensing_power_map[_black_king_pos.i - i][abs(j - _black_king_pos.j)];
	if (piece == 9)
		return (i > _black_king_pos.i) ? 0 : bishop_defensing_power_map[_black_king_pos.i - i][abs(j - _black_king_pos.j)];
	if (piece == 10)
		return (i > _black_king_pos.i) ? 0 : rook_defensing_power_map[_black_king_pos.i - i][abs(j - _black_king_pos.j)];
	if (piece == 11)
		return (i > _black_king_pos.i) ? 0 : queen_defensing_power_map[_black_king_pos.i - i][abs(j - _black_king_pos.j)];

	return 0;
}

// Fonction qui renvoie l'activité des pièces
int Board::get_piece_activity() const
{
	// Cela correspond on nombre de cases controllées par les pièces dans le camp adverse

	int white_controlled_squares[8][8] = { 0 };
	int black_controlled_squares[8][8] = { 0 };

	Board b(*this);

	// Contrôle des cases adverses

	// Blancs
	b._player = true;
	b.get_moves(false);
	for (uint_fast8_t i = 0; i < b._got_moves; i++)
	{
		const uint_fast8_t p = _array[b._moves[i].i1][b._moves[i].j1];
		if (p != 1 && p != 5)
			white_controlled_squares[b._moves[i].i2][b._moves[i].j2]++;
	}

	// Noirs
	b._player = false;
	b.get_moves(false);
	for (uint_fast8_t i = 0; i < b._got_moves; i++)
	{
		const uint_fast8_t p = _array[b._moves[i].i1][b._moves[i].j1];
		if (p != 7 && p != 11)
			black_controlled_squares[b._moves[i].i2][b._moves[i].j2]++;
	}

	// Puissance du contrôle d'une case adverse
	// Controllée 1 fois, 2 fois...
	static constexpr uint_fast8_t controlled_power[4] = { 10, 15, 18, 20 };

	// Puissance du contrôle de chaque case
	static constexpr uint_fast8_t activity_controlled_squares[8][8] = {
	{10, 10, 10, 10, 10, 10, 10, 10},
	{10, 10, 10, 10, 10, 10, 10, 10},
	{10, 10, 15, 20, 20, 15, 10, 10},
	{10, 10, 20, 25, 25, 20, 10, 10},
	{0,  0,  15, 20, 20, 15,  0,  0},
	{0,  0,  5,   5,  5,  5,  0,  0},
	{0,  0,  0,   0,  0,  0,  0,  0},
	{0,  0,  0,   0,  0,  0,  0,  0} };


	int activity = 0;

	for (uint_fast8_t j = 0; j < 8; j++)
	{
		for (uint_fast8_t i = 0; i < 8; i++)
		{
			activity += controlled_power[min(3, white_controlled_squares[i][j] - 1)] * activity_controlled_squares[7 - i][j];
			activity -= controlled_power[min(3, black_controlled_squares[i][j] - 1)] * activity_controlled_squares[i][j];
		}
	}

	// Facteur multiplicatif en fonction de l'avancement
	float advancement_factor = 0.6f;

	return activity * (1 - _adv * (advancement_factor - 1));
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
			piece > 0 && piece < 7 && add_piece_controls(&controls_map, i, j, piece);
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
			piece >= 7 && add_piece_controls(&controls_map, i, j, piece);
		}

	return controls_map;
}

// Fonction qui ajoute à une map les contrôles d'une pièce
bool Board::add_piece_controls(Map* m, int i, int j, int piece) const
{
	if (piece == 0)
		return false;

	// Pion blanc
	if (piece == 1) {
		j > 0 && (m->_array[i + 1][j - 1]++);
		j < 7 && (m->_array[i + 1][j + 1]++);
		return true;
	}

	// Pion noir
	if (piece == 7) {
		j > 0 && (m->_array[i - 1][j - 1]++);
		j < 7 && (m->_array[i - 1][j + 1]++);
		return true;
	}

	// Cavaliers
	if (piece == 2 || piece == 8) {
		static constexpr int_fast8_t knight_moves[8][2] = { {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1} };
		for (uint_fast8_t k = 0; k < 8; k++) {
			const uint_fast8_t i2 = i + knight_moves[k][0];
			const uint_fast8_t j2 = j + knight_moves[k][1];
			i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8 && (m->_array[i2][j2]++);
		}
		return true;
	}

	// Fous
	if (piece == 3 || piece == 9) {
		static constexpr int_fast8_t bishop_moves[4][2] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1} };
		for (uint_fast8_t k = 0; k < 4; k++) {
			const uint_fast8_t mi = bishop_moves[k][0];
			const uint_fast8_t mj = bishop_moves[k][1];
			uint_fast8_t i2 = i + mi;
			uint_fast8_t j2 = j + mj;
			while (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8) {
				m->_array[i2][j2]++;
				// Si la case est occupée, on arrête (sauf si la pièce est un fou, une dame ou un pion allié)
				if (_array[i2][j2] != 0 && _array[i2][j2] != piece && _array[i2][j2] != (piece + 2) && _array[i2][j2] != piece - 2)
					break;
				i2 += mi;
				j2 += mj;
			}
		}
		return true;
	}

	// Tours
	if (piece == 4 || piece == 10) {
		static constexpr int_fast8_t rook_moves[4][2] = { {-1, 0}, {0, -1}, {0, 1}, {1, 0} };
		for (uint_fast8_t k = 0; k < 4; k++) {
			const uint_fast8_t mi = rook_moves[k][0];
			const uint_fast8_t mj = rook_moves[k][1];
			uint_fast8_t i2 = i + mi;
			uint_fast8_t j2 = j + mj;
			while (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8) {
				m->_array[i2][j2]++;
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
	if (piece == 5 || piece == 11) {
		static constexpr int_fast8_t queen_moves[8][2] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1}, {-1, 0}, {0, -1}, {0, 1}, {1, 0} };
		for (uint_fast8_t k = 0; k < 8; k++) {
			const uint_fast8_t mi = queen_moves[k][0];
			const uint_fast8_t mj = queen_moves[k][1];
			uint_fast8_t i2 = i + mi;
			uint_fast8_t j2 = j + mj;
			while (i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8) {
				m->_array[i2][j2]++;
				bool diagonal = mi != 0 && mj != 0;
				// Si la case est occupée, on arrête (sauf si la pièce est une dame alliée, un (fou ou pion) allié et un déplacement diagonal, ou une tour alliée et un déplacement non diagonal)
				if (_array[i2][j2] != 0 && _array[i2][j2] != piece && ((_array[i2][j2] != (piece - 2) && _array[i2][j2] != (piece - 4)) || !diagonal) && (_array[i2][j2] != (piece - 1) || diagonal))
					break;
				i2 += mi;
				j2 += mj;
			}
		}
		return true;
	}

	// Rois
	if (piece == 6 || piece == 12) {
		static constexpr int_fast8_t king_moves[8][2] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1}, {-1, 0}, {0, -1}, {0, 1}, {1, 0} };
		for (uint_fast8_t k = 0; k < 8; k++) {
			uint_fast8_t i2 = i + king_moves[k][0];
			uint_fast8_t j2 = j + king_moves[k][1];
			i2 >= 0 && i2 < 8 && j2 >= 0 && j2 < 8 && (m->_array[i2][j2]++);
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
	const int i = color ? _white_king_pos.i : _black_king_pos.i;
	const int j = color ? _white_king_pos.j : _black_king_pos.j;
	
	// On compte le nombre de coups possibles pour la nouvelle dame
	int mobility = 0;
	static constexpr int_fast8_t queen_moves[8][2] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1}, {-1, 0}, {0, -1}, {0, 1}, {1, 0} };

	for (uint_fast8_t k = 0; k < 8; k++) {
		const uint_fast8_t mi = queen_moves[k][0];
		const uint_fast8_t mj = queen_moves[k][1];
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
	constexpr int initial_unsafe_check_value = 20;
	constexpr float initial_division = 0.5f;
	constexpr float king_escape_division = 0.5f;
	constexpr float piece_block_division = 2.0f;

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
		const uint_fast8_t i2 = move.i2;
		const uint_fast8_t j2 = move.j2;

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
					uint_fast8_t eaten_piece = b_check._array[b_check._moves[j].i2][b_check._moves[j].j2];
					if (eaten_piece != none) {
						is_safe_check = false;
						break;
					}

					// Pièce pouvant empêcher l'échec
					uint_fast8_t piece = b_check._array[b_check._moves[j].i1][b_check._moves[j].j1];

					// Echapatoire pour le roi
					if (piece == (color ? b_king : w_king)) {
						king_escapes++;
					}
					else {
						piece_blocks++;
					}
				}

				// Valeur de la division
				float division = initial_division + king_escapes * king_escape_division + piece_blocks * piece_block_division;

				//cout << "is safe check: " << is_safe_check;

				if (is_safe_check) {
					// Ajoute la valeur de l'échec safe
					//cout << ", king_escapes : " << king_escapes << ", piece_blocks : " << piece_blocks << ", division : " << division << ", value : " << initial_safe_check_value / division << endl;
					safe_checks_value += initial_safe_check_value / division;
				}
				else {
					// Ajoute la valeur de l'échec unsafe
					//cout << "value : " << initial_unsafe_check_value << endl;
					unsafe_checks_value += initial_unsafe_check_value;
				}

			}
		}
	}

	return safe_checks_value + unsafe_checks_value;
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

	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {

			// Pièce
			const uint_fast8_t piece = _array[i][j];

			// Si ça n'est pas un fou
			if (piece % 6 != 3)
				continue;

			// Si le fou n'est pas sur la grande diagonale
			if (i != j && i + j != 7)
				continue;

			int i1 = i, j1 = j;

			if (min(i1, 7 - i1) > 2)
				continue;

			for (uint_fast8_t k = min(i1, 7 - i1); k < 4; k++) {
				if (_array[i1][j1] % 6 == 1)
					break;
				if (k == 3) {
					piece < 7 ? fianchetti++ : fianchetti--;
					break;
				}
				(i1 < 4) ? i1++ : i1--;
				(j1 < 4) ? j1++ : j1--;
			}
		}
	}

	return fianchetti * 100.0f * (1.0f - _adv);
}

// Fonction qui renvoie si la case est controlée par un joueur
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
		if (const uint_fast8_t piece = _array[square_i][j]; piece != 0)
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

	// Pour chacun des pions, on regarde s'il y'a une ou des pièces qui seraient attaquées si le pion avançait
	// Il faut vérifier que rien ne bloque la poussée (ni une pièce, ni un contrôle adverse)
	// TODO améliorer les contrôles... car ça veut dire qu'il n'y a jamais de menaces contre un fou, même si le pion est protégé
	
	int threats = 0;
	bool player = _player;

	for (uint_fast8_t i = 1; i < 7; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			const uint_fast8_t p = _array[i][j];

			// Pion blanc
			if (p == 1 && i < 6) {
				if (j > 0) {
					threats += _array[i + 1][j] == 0 && _array[i + 2][j - 1] > 7; // Poussée simple
					threats += i == 1 && _array[i + 1][j] == 0 && _array[i + 2][j] == 0 && _array[i + 3][j - 1] > 7; // Poussée double
				}
				if (j < 7) {
					threats += _array[i + 1][j] == 0 && _array[i + 2][j + 1] > 7;
					threats += i == 1 && _array[i + 1][j] == 0 && _array[i + 2][j] == 0 && _array[i + 3][j + 1] > 7;
				}
			}

			// Pion noir
			else if (p == 7 && i > 1) {
				if (j > 0) {
					threats -= _array[i - 1][j] == 0 && is_in_fast(_array[i - 2][j - 1], 2, 6);
					threats -= i == 6 && _array[i - 1][j] == 0 && _array[i - 2][j] == 0 && is_in_fast(_array[i - 3][j - 1], 2, 6);
				}
				if (j < 7) {
					threats -= _array[i - 1][j] == 0 && is_in_fast(_array[i - 2][j + 1], 2, 6);
					threats -= i == 6 && _array[i - 1][j] == 0 && _array[i - 2][j] == 0 && is_in_fast(_array[i - 3][j + 1], 2, 6);
				}
					
			}

		}
	}

	return threats;
}

// Fonction qui calcule et renvoie la proximité du roi avec les pions
int Board::get_king_proximity()
{
	// Met à jour la position des rois
	update_kings_pos();

	// Proximité des rois
	int proximity = 0;

	// Pourcentage d'avancement pour que ça soit pris en compte
	const float min_advancement = 0.60f;

	if (_adv <= min_advancement)
		return 0;

	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			const uint_fast8_t p = _array[i][j];

			// Pion blanc
			if (p == 1) {
				proximity -= max(abs(i - _white_king_pos.i), abs(j - _white_king_pos.j)) * i;
				proximity += max(abs(i - _black_king_pos.i), abs(j - _black_king_pos.j)) * i;
			}

			// Pion noir
			else if (p == 7) {
				proximity -= max(abs(i - _white_king_pos.i), abs(j - _white_king_pos.j)) * (7 - i);
				proximity += max(abs(i - _black_king_pos.i), abs(j - _black_king_pos.j)) * (7 - i);
			}

		}
	}
	
	return 10 * proximity * (_adv - min_advancement) / (1.0f - min_advancement);
}


// Fonction qui calcule l'activité/mobilité des tours
int Board::get_rook_activity() const
{
	// Cas de figure:
	// 1. Tour enfermée par le roi: mobilité < 4 -> malus (encore plus grand si le roi ne peut pas roquer) /= mobilité
	// 2. L'activité dépend surtout de la mobilité verticale (distance au pion le plus proche devant)

	// ça doit dimunuer en fonction du nombre de colonnes ouvertes...

	constexpr int trapped_rook_malus = 500;
	constexpr int vertical_mobility_bonus = 50;

	int activity = 0;

	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			const uint_fast8_t p = _array[i][j];

			// Tour blanche
			if (p == 4) {
				
				// Mobilité horizontale
				int h_mobility = 0;

				// Vers la droite
				for (uint_fast8_t k = j + 1; k < 8; k++) {
					if (_array[i][k] == 0)
						h_mobility++;
					else
						break;
				}

				// Vers la gauche
				for (int_fast8_t k = j - 1; k >= 0; k--) {
					if (_array[i][k] == 0)
						h_mobility++;
					else
						break;
				}

				// Mobilité verticale
				int v_mobility = 0;

				// Vers le haut
				for (uint_fast8_t k = i + 1; k < 8; k++) {
					const uint_fast8_t p2 = _array[k][j];
					if (p2 != 1 && p2 != 7)
						v_mobility++;
					else
						break;
				}

				// Vers le bas
				for (int_fast8_t k = i - 1; k >= 0; k--) {
					const uint_fast8_t p2 = _array[k][j];
					if (p2 != 1 && p2 != 7)
						v_mobility++;
					else
						break;
				}

				// Malus pour tour enfermée par le roi (si le tour est plus proche du bord que le roi)
				if ((h_mobility + v_mobility) < 6 && _white_king_pos.i == i && abs(_white_king_pos.j - j) < 4 && min(_white_king_pos.j, 7 - _white_king_pos.j) > min((int)j, 7 - j))
					activity -= trapped_rook_malus;

				// Bonus pour la mobilité verticale
				activity += vertical_mobility_bonus * v_mobility;
			}


			// Tour noire
			else if (p == 10) {

				// Mobilité horizontale
				int h_mobility = 0;

				// Vers la droite
				for (uint_fast8_t k = j + 1; k < 8; k++) {
					if (_array[i][k] == 0)
						h_mobility++;
					else
						break;
				}

				// Vers la gauche
				for (int_fast8_t k = j - 1; k >= 0; k--) {
					if (_array[i][k] == 0)
						h_mobility++;
					else
						break;
				}

				// Mobilité verticale
				int v_mobility = 0;

				// Vers le haut
				for (uint_fast8_t k = i + 1; k < 8; k++) {
					const uint_fast8_t p2 = _array[k][j];
					if (p2 != 1 && p2 != 7)
						v_mobility++;
					else
						break;
				}

				// Vers le bas
				for (int_fast8_t k = i - 1; k >= 0; k--) {
					const uint_fast8_t p2 = _array[k][j];
					if (p2 != 1 && p2 != 7)
						v_mobility++;
					else
						break;
				}

				// Malus pour tour enfermée par le roi
				if ((h_mobility + v_mobility) < 6 && _black_king_pos.i == i && abs(_black_king_pos.j - j) < 4 && min(_black_king_pos.j, 7 - _black_king_pos.j) > min((int)j, 7 - j))
					activity += trapped_rook_malus;

				// Bonus pour la mobilité verticale
				activity -= vertical_mobility_bonus * v_mobility;
			}

		}
	}

	// Facteur multiplicatif en fonction de l'avancement de la partie
	float advancement_factor = 0.35f;

	return activity * (1 - _adv * (advancement_factor - 1));
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


	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			const uint_fast8_t p = _array[i][j];

			// Pions blancs
			if (p == w_pawn) {
				if ((i + j) % 2)
					white_pawns_w++;
				else
					white_pawns_b++;

				if (_array[i + 1][j] != 0 && is_in(j, 2, 5))
					white_central_pawns_blocked++;
			}

			// Pions noirs
			else if (p == b_pawn) {
				if ((i + j) % 2)
					black_pawns_w++;
				else
					black_pawns_b++;

				if (_array[i - 1][j] != 0 && is_in(j, 2, 5))
					black_central_pawns_blocked++;
			}
		}
	}

	//cout << "white_pawns_w: " << white_pawns_w << endl;
	//cout << "white_pawns_b: " << white_pawns_b << endl;
	//cout << "black_pawns_w: " << black_pawns_w << endl;
	//cout << "black_pawns_b: " << black_pawns_b << endl;
	//cout << "white_pawns_blocked: " << white_pawns_blocked << endl;
	//cout << "black_pawns_blocked: " << black_pawns_blocked << endl;

	int bishop_pawns_value = 0;

	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			const uint_fast8_t p = _array[i][j];

			// Fou blanc
			if (p == w_bishop) {
				if ((i + j) % 2)
					bishop_pawns_value -= white_pawns_w * (1 + white_central_pawns_blocked);
				else
					bishop_pawns_value -= white_pawns_b * (1 + white_central_pawns_blocked);
			}

			// Fou noir
			else if (p == b_bishop) {
				if ((i + j) % 2)
					bishop_pawns_value += black_pawns_w * (1 + black_central_pawns_blocked);
				else
					bishop_pawns_value += black_pawns_b * (1 + black_central_pawns_blocked);
			}
		}
	}

	//cout << "bishop_pawns_value: " << bishop_pawns_value << endl;

	// Facteur multiplicatif en fonction de l'avancement de la partie
	float advancement_factor = 0.5f;

	return bishop_pawns_value * (1 + _adv * (advancement_factor - 1));
}

// Fonction qui renvoie la valeur d'une marrée de pions
int Board::get_pawn_storm() const {
	// TODO : implementer
	return 0;
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
	int w_front_pawns = 25 * (_array[1][_white_king_pos.j] == 1) + 15 * (_array[2][_white_king_pos.j] == 1) + 5 * (_array[3][_white_king_pos.j] == 1);
	if (_white_king_pos.j > 0)
		w_front_pawns += 25 * (_array[1][_white_king_pos.j - 1] == 1) + 15 * (_array[2][_white_king_pos.j - 1] == 1) + 5 * (_array[3][_white_king_pos.j - 1] == 1);
	if (_white_king_pos.j < 7)
		w_front_pawns += 25 * (_array[1][_white_king_pos.j + 1] == 1) + 15 * (_array[2][_white_king_pos.j + 1] == 1) + 5 * (_array[3][_white_king_pos.j + 1] == 1);

	int w_castles_count = _castling_rights.k_w + _castling_rights.q_w;

	int w_pawn_shield_value = (w_front_pawns + _castling_rights.k_w * w_kingside_pawns + _castling_rights.q_w * w_queenside_pawns) / (1 + w_castles_count);


	// Roi noir

	// pions f, g et h
	int b_kingside_pawns = 25 * ((_array[6][5] == 7) + (_array[6][6] == 7) + (_array[6][7] == 7)) + 15 * ((_array[5][5] == 7) + (_array[5][6] == 7) + (_array[5][7] == 7)) + 5 * ((_array[4][5] == 7) + (_array[4][6] == 7) + (_array[4][7] == 7));

	// pions b, c et d
	int b_queenside_pawns = 25 * ((_array[6][1] == 7) + (_array[6][2] == 7) + (_array[6][3] == 7)) + 15 * ((_array[5][1] == 7) + (_array[5][2] == 7) + (_array[5][3] == 7)) + 5 * ((_array[4][1] == 7) + (_array[4][2] == 7) + (_array[4][3] == 7));

	// pions devant le roi
	int b_front_pawns = 25 * (_array[6][_black_king_pos.j] == 7) + 15 * (_array[5][_black_king_pos.j] == 7) + 5 * (_array[4][_black_king_pos.j] == 7);
	if (_black_king_pos.j > 0)
		b_front_pawns += 25 * (_array[6][_black_king_pos.j - 1] == 7) + 15 * (_array[5][_black_king_pos.j - 1] == 7) + 5 * (_array[4][_black_king_pos.j - 1] == 7);
	if (_black_king_pos.j < 7)
		b_front_pawns += 25 * (_array[6][_black_king_pos.j + 1] == 7) + 15 * (_array[5][_black_king_pos.j + 1] == 7) + 5 * (_array[4][_black_king_pos.j + 1] == 7);

	int b_castles_count = _castling_rights.k_b + _castling_rights.q_b;

	int b_pawn_shield_value = (b_front_pawns + _castling_rights.k_b * b_kingside_pawns + _castling_rights.q_b * b_queenside_pawns) / (1 + b_castles_count);

	// Trous sur les colonnes


	// Calcul de la valeur du bouclier de pions
	pawn_shield_value = w_pawn_shield_value - b_pawn_shield_value;

	// A partir de quelle valeur de l'avancement de la partie, cela n'a plus d'importance (décroit linéairement)
	float pawn_shield_advancement_threshold = 0.7f;

	return pawn_shield_value * (max(0.0f, 1.0f - _adv / pawn_shield_advancement_threshold));
}

// Fonction qui renvoie la caleur des cases faibles
int Board::get_weak_squares() const {
	// Case faible: case qui ne peut plus être protégée par un pion (= pas de pions sur une ligne inférieure sur les colonnes adjacentes), s'il n'y a pas de pion dessus
	// Bonus pour le contrôle de la case faible par un pion adverse
	// Bonus pour l'outpost d'un cavalier, d'un fou, ou d'une tour

	// TODO: il faut le moduler en fonction des pièces qui peuvent aller dessus...
	// Essayer un bonus en fonction de la distance d'une pièce vers la case?

	// Valeur des cases faibles
	const static int weak_square_values[8][8] = {
		{ 0,  0,  0,  0,  0,  0,  0,  0},
		{ 0,  0,  0,  0,  0,  0,  0,  0}, 
		{ 0,  5, 10, 20, 20, 10,  5,  0},
		{ 5, 20, 35, 50, 50, 35, 20,  5},
		{ 5, 20, 40, 60, 60, 40, 20,  5},
		{ 5, 10, 15, 40, 40, 15, 10,  5},
		{ 0,  0,  0,  0,  0,  0,  0,  0},
		{ 0,  0,  0,  0,  0,  0,  0,  0}
	};

	// Valeur des outposts sur les cases faibles
	const static int outpost_square_values[8][8] = {
		{ 0,  0,  0,  0,  0,  0,  0,  0},
		{ 0,  5, 15, 25, 25, 15,  5,  0},
		{ 0, 25, 30, 40, 40, 30, 25,  0},
		{ 0, 15, 35, 50, 50, 35, 15,  0},
		{ 0, 10, 20, 35, 35, 20, 10,  0},
		{ 0,  0,  5, 10, 10,  5,  0,  0},
		{ 0,  0,  0,  0,  0,  0,  0,  0},
		{ 0,  0,  0,  0,  0,  0,  0,  0}
	};

	// Outpost pour un cavalier
	const static float knight_outpost_value = 1.5f;

	// Outpost pour un fou
	const static float bishop_outpost_value = 0.85f;

	// Outpost pour une tour
	const static float rook_outpost_value = 0.5f;


	// Valeur des cases faibles
	int weak_squares_value = 0;

	// Cases faibles des blancs

	// Pour chaque case
	for (uint_fast8_t i = 2; i < 7; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			bool weak = _array[i][j] != 1 && _array[i][j] != 7;
			
			if (weak && j > 0) {
				for (uint_fast8_t k = i - 1; k > 0; k--) {
					if (_array[k][j - 1] == 1) {
						weak = false;
						break;
					}
				}
			}

			if (weak && j < 7) {
				for (uint_fast8_t k = i - 1; k > 0; k--) {
					if (_array[k][j + 1] == 1) {
						weak = false;
						break;
					}
				}
			}

			// Si c'est une case faible
			if (weak) {
				int square_value = weak_square_values[7 - i][j];
				
				//cout << "***\nweak square of white: " << Pos(i, j).square() << ": " << square_value << endl;

				// Contrôle de la case par un (des) pions adverses
				int pawn_controls = (j > 0 && _array[i + 1][j - 1] == 7) + (j < 7 && _array[i + 1][j + 1] == 7);

				//cout << "pawn controls: " << pawn_controls << endl;

				// Outposts
				if (pawn_controls > 0) {

					// Valeur de l'outpost adverse
					int outpost_value = outpost_square_values[i][j];
					int p = _array[i][j];

					// Valeur en fonction de la pièce
					square_value += outpost_value * (p == 8 ? knight_outpost_value : (p == 9 ? bishop_outpost_value : (p == 10 ? rook_outpost_value : 0)));

					//cout << "outpost value: " << outpost_value << " for piece: " << p << " -> " << (p == 8 ? knight_outpost_value : (p == 9 ? bishop_outpost_value : (p == 10 ? rook_outpost_value : 0))) << endl;
					//cout << "square value: " << square_value << endl;
				}

				// Valeur du contrôle de la case faible par un pion adverse
				square_value *= (0.25 + (pawn_controls > 0));

				//cout << "with controls, square value: " << square_value << endl;

				weak_squares_value -= square_value;
			}
				
		}
	}

	// Cases faibles des noirs

	// Pour chaque case
	for (uint_fast8_t i = 5; i > 1; i--) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			bool weak = _array[i][j] != 1 && _array[i][j] != 7;

			if (weak && j > 0) {
				for (uint_fast8_t k = i + 1; k < 7; k++) {
					if (_array[k][j - 1] == 7) {
						weak = false;
						break;
					}
				}
			}

			if (weak && j < 7) {
				for (uint_fast8_t k = i + 1; k < 7; k++) {
					if (_array[k][j + 1] == 7) {
						weak = false;
						break;
					}
				}
			}

			// Si c'est une case faible
			if (weak) {
				int square_value = weak_square_values[i][j];

				//cout << "***\nweak square of black: " << Pos(i, j).square() << ": " << square_value << endl;

				// Contrôle de la case par un (des) pions adverses
				int pawn_controls = (j > 0 && _array[i - 1][j - 1] == 1) + (j < 7 && _array[i - 1][j + 1] == 1);

				//cout << "pawn controls: " << pawn_controls << endl;

				// Outposts
				if (pawn_controls > 0) {

					// Valeur de l'outpost adverse
					int outpost_value = outpost_square_values[7 - i][j];
					int p = _array[i][j];

					// Valeur en fonction de la pièce
					square_value += outpost_value * (p == 2 ? knight_outpost_value : (p == 3 ? bishop_outpost_value : (p == 4 ? rook_outpost_value : 0)));

					//cout << "outpost value: " << outpost_value << " for piece: " << p << " -> " << (p == 2 ? knight_outpost_value : (p == 3 ? bishop_outpost_value : (p == 4 ? rook_outpost_value : 0))) << endl;
					//cout << "square value: " << square_value << endl;
				}

				// Valeur du contrôle de la case faible par un pion adverse
				square_value *= (0.25 + (pawn_controls > 0));

				//cout << "with controls, square value: " << square_value << endl;

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

	return weak_squares_value / (open_files / 2 + 1);
}

// Fonction qui convertit un coup en sa notation algébrique
string Board::algebric_notation(Move move) const {
	string move_notation = main_GUI._abc8[move.j1] + to_string(move.i1 + 1) + main_GUI._abc8[move.j2] + to_string(move.i2 + 1);

	// Promotion
	if (move.i2 == 7 && _array[move.i1][move.j1] == 1)
		move_notation += "q";
	else if (move.i2 == 0 && _array[move.i1][move.j1] == 7)
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
		w_kingside_castle_distance += (_array[0][5] != 0) + (_array[0][6] != 0) + (_array[0][5] != 0 && _array[1][4] != 0 && _array[1][6] != 0);
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
		w_queenside_castle_distance += (_array[0][1] != 0) + (_array[0][2] != 0) + (_array[0][3] != 0) + (_array[0][3] != 0 && _array[1][2] != 0 && _array[1][4] != 0);
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
		b_kingside_castle_distance += (_array[7][5] != 0) + (_array[7][6] != 0) + (_array[7][5] != 0 && _array[6][4] != 0 && _array[6][6] != 0);
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
		b_queenside_castle_distance += (_array[7][1] != 0) + (_array[7][2] != 0) + (_array[7][3] != 0) + (_array[7][3] != 0 && _array[6][2] != 0 && _array[6][4] != 0);
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

// Fonction qui renvoie à quel point la partie est gagnable (de 0 à 1)
float Board::get_winnable() const {
	// TODO: à implémenter
	// Prendre en compte:
	// si la position est fermée
	// déséquilibre matériel
	// ENDGAME: matériel restant
	// si y'a des tours, c'est plus drawish
	// combien de pions il reste

	// EXEMPLES:
	// 2q5/k3bp2/p1p1b1p1/P1PpPp1p/1Pp2P1P/2Q3P1/6K1/3RR3 w - - 0 1 : ça c'est nulle (position fermée)

	return 0.0f;
}

// Fonction qui renvoie l'activité des fous sur les diagonales
int Board::get_bishop_activity() const {
	// Mobilité d'un fou = nombre de cases non-pion sur les diagonales du fou

	// Bonus pour le fou blanc
	int w_bishop_activity = 0;

	// Bonus pour le fou noir
	int b_bishop_activity = 0;

	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			uint_fast8_t p = _array[i][j];

			// Fou blanc
			if (p == 3) {
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
			else if (p == 9) {
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
	float bishop_activity_advancement_factor = 0.35f;

	return (w_bishop_activity - b_bishop_activity) * (1 - _adv * bishop_activity_advancement_factor);
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

// Fonction qui renvoie l'évaluation des pièces isolées
[[nodiscard]] int Board::get_isolated_pieces() const {
	// Pièce isolée: pièce éloignée des autres pièces alliées

	// TODO: adapter ça pour les endgames aussi? pour que le roi se rapproche des pions? pareil pour les chevaux...
	// TODO: ajouter un rayon variable pour le centre? pour savoir s'il est étendu ou non pour mieux évaluer si une pièce est effectivement isolée

	// Poids par type de pièce
	const float pawn_weight = 1.0f;
	const float knight_weight = 5.0f; // Gros poids, car pièce de courte portée
	const float bishop_weight = 3.0f;
	const float rook_weight = 3.0f;
	const float queen_weight = 10.0f;
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
	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			uint_fast8_t p = _array[i][j];

			if (p == none)
				continue;

			// Si c'est une pièce blanche
			if (p <= w_king) {
				// Poids de la pièce
				float weight = p == w_pawn ? pawn_weight : (p == w_knight ? knight_weight : (p == w_bishop ? bishop_weight : (p == w_rook ? rook_weight : (p == w_queen ? queen_weight : king_weight))));

				//cout << "i: " << i << ", j: " << j << ", weight: " << weight << endl;

				// Centre de masse
				w_center_of_mass_i += i * weight;
				w_center_of_mass_j += j * weight;
				w_total_weight += weight;
			}

			// Si c'est une pièce noire
			else {
				// Poids de la pièce
				float weight = p == b_pawn ? pawn_weight : (p == b_knight ? knight_weight : (p == b_bishop ? bishop_weight : (p == b_rook ? rook_weight : (p == b_queen ? queen_weight : king_weight))));
				
				//cout << "i: " << i << ", j: " << j << ", weight: " << weight << endl;

				// Centre de masse
				b_center_of_mass_i += i * weight;
				b_center_of_mass_j += j * weight;
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
	float min_distance = 1.5f;

	float w_isolated_pieces = 0.0f;
	float b_isolated_pieces = 0.0f;

	// Pénalité linéaire pour le moment?

	// Malus par type de pièce
	const float pawn_malus = 2.0f;
	const float knight_malus = 5.0f; // Gros poids, car pièce de courte portée
	const float bishop_malus = 5.0f;
	const float rook_malus = 7.0f;
	const float queen_malus = 12.0f;
	const float king_malus = 15.0f;

	// FIXME: rqb2Q2/3p3p/pBp1p1p1/P3Pp2/1P3P1k/2N5/2P1B1PP/6K1 w - - 5 30 : king malus marche pas?

	// Pour chaque pièce
	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			uint_fast8_t p = _array[i][j];

			if (p == none)
				continue;

			// Si c'est une pièce blanche
			if (p <= w_king) {
				// Distance au centre de masse
				float distance = sqrt(pow(i - w_center_of_mass_i, 2) + pow(j - w_center_of_mass_j, 2));

				// Plus gros malus quand proche du camp advserse
				distance *= (2 + max(0, (int)i - 3));

				// Pénalité
				float malus = p == w_pawn ? pawn_malus : (p == w_knight ? knight_malus : (p == w_bishop ? bishop_malus : (p == w_rook ? rook_malus : (p == w_queen ? queen_malus : king_malus))));

				// Pénalité en fonction de la distance
				float isolated_piece = max(0.0f, distance - min_distance) * malus;

				//cout << "i: " << i << ", j: " << j << ", distance: " << distance << ", isolated_piece: " << isolated_piece << endl;

				w_isolated_pieces += isolated_piece;
			}

			// Si c'est une pièce noire
			else {
				// Distance au centre de masse
				float distance = sqrt(pow(i - b_center_of_mass_i, 2) + pow(j - b_center_of_mass_j, 2));

				// Plus gros malus quand proche du camp adverse
				distance *= (2 + max(0, 4 - (int)i));

				// Pénalité
				float malus = p == b_pawn ? pawn_malus : (p == b_knight ? knight_malus : (p == b_bishop ? bishop_malus : (p == b_rook ? rook_malus : (p == b_queen ? queen_malus : king_malus))));

				// Pénalité en fonction de la distance
				float isolated_piece = max(0.0f, distance - min_distance) * malus;

				//cout << "i: " << i << ", j: " << j << ", distance: " << distance << ", isolated_piece: " << isolated_piece << endl;

				b_isolated_pieces += isolated_piece;
			}
		}
	}

	//cout << "w_isolated_pieces: " << w_isolated_pieces << endl;
	//cout << "b_isolated_pieces: " << b_isolated_pieces << endl;

	// Avancement à partir duquel cela ne compte plus
	// FIXME: en faut-il vraiment un? un cheval seul en finale ne sert à rien...
	const float max_adv = 0.6f;

	return (b_isolated_pieces - w_isolated_pieces) * max(1.0f - _adv / max_adv, 0.0f);
}

// Fonction qui ajuste les valeurs des pièces (malus/bonus), en fonction du type de position
[[nodiscard]] int Board::get_updated_piece_values() const {
	// Malus pour les tours en fonction du nombre de colonnes non-ouvertes
	// Malus pour les fous si la position est fermée (diagonales non-ouvertes)
	// Pareil pour la dame. Bonus dans les cas contraires

	// *** TODO ***
	return 0;
}

// Fonction qui renvoie la nature de la position de manière chiffrée: 0 = fermée, 1 = ouverte
[[nodiscard]] float Board::get_position_nature() const {
	// *** TODO ***

	// Mettre à jour les autres paramètres d'évaluation en fonction de cette valeur:
	// Plus la position est ouverte, plus les atouts dynamiques sont forts (activité des pièces, sécurité du roi, etc.) (pas sûr...)
	// Plus la position est fermée, plus les atouts statiques sont forts (structure de pions, espace, etc.)

	// En position fermée, la position a plus de chances d'aboutir à une nulle. -> réduire l'évaluation?
	// -> Comment prendre en compte le matériel? Le diminuer? C'est pourtant un paramètre statique...

	return 0;
}

// Fonction qui renvoie la probabilité de nulle de la position
[[nodiscard]] float Board::get_draw_chance() const {
	// *** TODO ***

	// Implémenter winnable(color) -> notamment pour les fins de partie
	return 0;
}

// Fonction qui renvoie la valeur des bonus liés aux colonnes ouvertes et semi-ouvertes sur le roi adverse
[[nodiscard]] int Board::get_open_files_on_opponent_king(bool player) {
	
	// Bonus pour les colonnes ouvertes et semi-ouvertes
	constexpr int open_file_bonus = 50;
	constexpr int semi_open_file_bonus = 25;

	// Facteur en fonction de la proximité avec la colonne du roi adverse
	// Si le roi est sur la colonne, le bonus est maximal
	constexpr float king_file_bonus = 1.0f;

	// Si le roi est sur une colonne adjacente, le bonus est réduit
	constexpr float king_adjacent_file_bonus = 0.5f;

	// Bonus en plus pour les pièces présentes dessus (tours/dame)

	update_kings_pos();

	// Colonne du roi advserse
	uint_fast8_t king_row = player ? _black_king_pos.j : _white_king_pos.j;

	// Bonus pour le joueur
	int total_bonus = 0;

	// Pion allié
	const int player_pawn = player ? w_pawn : b_pawn;

	// Pion adverse
	const int opponent_pawn = player ? b_pawn : w_pawn;

	// Pour chaque colonne adjacente au roi noir
	for (uint_fast8_t j = king_row - 1; j < king_row + 2; j++) {

		// Si la colonne est en dehors de l'échiquier
		if (j < 0 || j > 7)
			continue;
		
		// Nature de la colonne
		bool semi_open = true;
		bool open = true;

		for (uint_fast8_t i = 0; i < 8; i++) {
			uint_fast8_t p = _array[i][j];

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

		// Bonus en fonction de la proximité du roi
		bonus *= (j == king_row ? king_file_bonus : king_adjacent_file_bonus);

		total_bonus += bonus;
	}

	return total_bonus;
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

	update_kings_pos();

	// Diagonale du roi advserse
	uint_fast8_t king_i = color ? _black_king_pos.i : _white_king_pos.i;
	uint_fast8_t king_j = color ? _black_king_pos.j : _white_king_pos.j;

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

			// Bonus en fonction de la proximité du roi
			bonus *= (i == 0 && j == 0 ? king_diagonal_bonus : king_adjacent_diagonal_bonus);

			total_bonus += bonus;
		}
	}

	return total_bonus;
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
			uint_fast8_t new_i = king_pos.i + i;
			uint_fast8_t new_j = king_pos.j + j;

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


	// Valeur d'une pièce attaquant le roi adverse
	constexpr int attacking_value[7] = { 0, 100, 120, 130, 138, 145, 150 };

	// Valeur d'attaque d'une pièce attaquant la couronne lointaine du roi
	constexpr int semi_attack_value = 75;

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
	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			uint_fast8_t p = _array[i][j];

			uint_fast8_t attacks = 0;
			uint_fast8_t semi_attacks = 0;

			// Pion
			if (p == (color ? w_pawn : b_pawn)) {

				// Cases contrôlées par le pion
				uint_fast8_t di = abs(i + (color ? 1 : -1) - king_pos.i);
				uint_fast8_t dj1 = abs(j - 1 - king_pos.j);
				uint_fast8_t dj2 = abs(j + 1 - king_pos.j);

				// Si le pion contrôle une case du roi
				if (j > 0 && di <= 2 && dj1 <= 2) {
					semi_attacks++;
					if (di <= 1 && dj1 <= 1) {
						attacks++;
					}
				}

				if (j < 7 && di <= 2 && dj2 <= 2) {
					semi_attacks++;
					if (di <= 1 && dj2 <= 1) {
						attacks++;
					}
				}
			}

			// Cavalier
			if (p == (color ? w_knight : b_knight)) {
				const int knight_moves[8][2] = {{1, 2}, {1, -2}, {-1, 2}, {-1, -2}, {2, 1}, {2, -1}, {-2, 1}, {-2, -1}}; // TODO: à stocker en variable globale

				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_i = i + knight_moves[m][0];
					int new_j = j + knight_moves[m][1];

					if (!is_in(new_i, 0, 7) || !is_in(new_j, 0, 7))
						continue;

					uint_fast8_t di = abs(new_i - king_pos.i);
					uint_fast8_t dj = abs(new_j - king_pos.j);

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
				const int rect_moves[4][2] = { {0, -1}, {-1, 0}, {0, 1}, {1, 0} };

				for (uint_fast8_t m = 0; m < 4; m++) {
					int mi = rect_moves[m][0];
					int mj = rect_moves[m][1];

					int new_i = i + mi;
					int new_j = j + mj;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint_fast8_t p2 = _array[new_i][new_j];

						uint_fast8_t di = abs(new_i - king_pos.i);
						uint_fast8_t dj = abs(new_j - king_pos.j);

						// La case ne peut pas être attaquée
						if (p2 == (color ? w_pawn : b_pawn))
							break;

						// Si la pièce contrôle une case du roi
						if (di <= 2 && dj <= 2) {
							semi_attacks++;
							if (di <= 1 && dj <= 1) {
								attacks++;
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

			// Pièces à mouvement diagonal
			if ((p == (color ? w_bishop : b_bishop)) || (p == (color ? w_queen : b_queen))) {
				const int diag_moves[4][2] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1} };

				for (uint_fast8_t m = 0; m < 4; m++) {
					int mi = diag_moves[m][0];
					int mj = diag_moves[m][1];

					int new_i = i + mi;
					int new_j = j + mj;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint_fast8_t p2 = _array[new_i][new_j];

						uint_fast8_t di = abs(new_i - king_pos.i);
						uint_fast8_t dj = abs(new_j - king_pos.j);

						// La case ne peut pas être attaquée
						if (p2 == (color ? w_pawn : b_pawn))
							break;

						// Si la pièce contrôle une case du roi
						if (di <= 2 && dj <= 2) {
							semi_attacks++;
							if (di <= 1 && dj <= 1) {
								attacks++;
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
						int new_i = i + opponent_king_pos.i;
						int new_j = j + opponent_king_pos.j;

						if (!is_in(new_i, 0, 7) || !is_in(new_j, 0, 7))
							continue;

						uint_fast8_t di = abs(new_i - king_pos.i);
						uint_fast8_t dj = abs(new_j - king_pos.j);

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
					king_attackers += attacking_value[attacks];
				}
				else if (semi_attacks > 0) { // TODO: à améliorer en prenant en compte le nombre de semi-attaques?
					king_attackers += semi_attack_value;
				}
			}
		}
	}

	return king_attackers;
}

[[nodiscard]] int Board::get_king_defenders(bool color) {
	// Pour les sliding pieces: regarde simplement sur la ligne/colonne/diagonale: s'il y a un pion qui bloque: est-ce un pion à proximité du roi? sinon: est-ce que il contrôle des cases du roi?

	// Valeur d'une pièce défendant le roi adverse
	constexpr int defending_value[7] = { 0, 100, 120, 130, 138, 145, 150 };

	// Valeur de défense d'une pièce défendant la couronne lointaine du roi
	constexpr int semi_defense_value = 50;

	// Met à jour la position des rois
	update_kings_pos();

	// Position du roi
	Pos king_pos = color ? _white_king_pos : _black_king_pos;

	// Nombre de contrôles sur le roi
	int king_defenders = 0;

	// Regarde chaque pièce alliée sur l'échiquier
	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			uint_fast8_t p = _array[i][j];

			uint_fast8_t defenses = 0;
			uint_fast8_t semi_defenses = 0;

			// Pion : TODO à revoir...
			if (p == (color ? w_pawn : b_pawn)) {

				// Cases contrôlées par le pion
				uint_fast8_t di = abs(i + (color ? 1 : -1) - king_pos.i);
				uint_fast8_t dj1 = abs(j - 1 - king_pos.j);
				uint_fast8_t dj2 = abs(j + 1 - king_pos.j);

				// Si le pion contrôle une case du roi
				if (j > 0 && di <= 2 && dj1 <= 2) {
					semi_defenses++;
					if (di <= 1 && dj1 <= 1) {
						defenses++;
					}
				}

				if (j < 7 && di <= 2 && dj2 <= 2) {
					semi_defenses++;
					if (di <= 1 && dj2 <= 1) {
						defenses++;
					}
				}
			}

			// Cavalier
			if (p == (color ? w_knight : b_knight)) {
				const int knight_moves[8][2] = { {1, 2}, {1, -2}, {-1, 2}, {-1, -2}, {2, 1}, {2, -1}, {-2, 1}, {-2, -1} }; // TODO: à stocker en variable globale

				for (uint_fast8_t m = 0; m < 8; m++) {
					int new_i = i + knight_moves[m][0];
					int new_j = j + knight_moves[m][1];

					if (!is_in(new_i, 0, 7) || !is_in(new_j, 0, 7))
						continue;

					uint_fast8_t di = abs(new_i - king_pos.i);
					uint_fast8_t dj = abs(new_j - king_pos.j);

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
				const int rect_moves[4][2] = { {0, -1}, {-1, 0}, {0, 1}, {1, 0} };

				for (uint_fast8_t m = 0; m < 4; m++) {
					int mi = rect_moves[m][0];
					int mj = rect_moves[m][1];

					int new_i = i + mi;
					int new_j = j + mj;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint_fast8_t p2 = _array[new_i][new_j];

						uint_fast8_t di = abs(new_i - king_pos.i);
						uint_fast8_t dj = abs(new_j - king_pos.j);

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
				const int diag_moves[4][2] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1} };

				for (uint_fast8_t m = 0; m < 4; m++) {
					int mi = diag_moves[m][0];
					int mj = diag_moves[m][1];

					int new_i = i + mi;
					int new_j = j + mj;

					while (is_in(new_i, 0, 7) && is_in(new_j, 0, 7)) {
						uint_fast8_t p2 = _array[new_i][new_j];

						uint_fast8_t di = abs(new_i - king_pos.i);
						uint_fast8_t dj = abs(new_j - king_pos.j);

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

			if (defenses > 6) {
				cout << "BUG: too many defenses from a single piece... check get_king_defenders()" << endl;
			}
			else {
				if (defenses > 0) {
					king_defenders += defending_value[defenses];
				}
				else if (semi_defenses > 0) { // TODO: à améliorer en prenant en compte le nombre de semi-défenses?
					king_defenders += semi_defense_value;
				}
			}
		}
	}

	return king_defenders;
}

// Fonction qui renvoie un bonus correspondant au pawn storm sur le roi adverse
[[nodiscard]] int Board::get_pawn_storm(bool color) {
	// FIXME: faut-il un bonus si le pion a déjà passé le roi adverse? en soit, cela veut dire que le roi adverse est sur une colonne "ouverte" sans vraiment être ouverte

	// Position du roi
	update_kings_pos();

	Pos opponent_king_pos = color ? _black_king_pos : _white_king_pos;

	// Bonus en fonction de la distance verticale entre les pions et le roi
	int bonus[7] = { 70, 70, 50, 35, 20, 10, 0};

	int total_bonus = 0;

	// Regarde sur les trois colonnes adjacentes au roi
	for (uint_fast8_t j = opponent_king_pos.j - 1; j <= opponent_king_pos.j + 1; j++) {
		if (j < 0 || j > 7) {
			continue;
		}

		for (uint_fast8_t i = 0; i < 8; i++) {
			if (_array[i][j] == (color ? w_pawn : b_pawn)) {
				//cout << (int)i << ", " << opponent_king_pos.i << endl;
				//cout << abs(i - opponent_king_pos.i) << endl;

				// S'il n'y a pas de pièce adverse qui le bloque
				uint_fast8_t p = _array[i + (color ? 1 : -1)][j];
				//cout << "piece : " << (int)p << endl;

				// FIXME??
				if ((color && p <= w_king) || (!color && (p == none || p >= b_pawn))) { // En théorie, l'indice ne devrait pas sortir de [|0, 7|] puisque les pions ne peuvent pas se situer sur les lignes extrëmes
					total_bonus += bonus[abs(i - opponent_king_pos.i)];
				}
			}
		}
	}

	return total_bonus;
}