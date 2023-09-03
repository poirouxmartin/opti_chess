#include "opti_chess.h"
#include <algorithm>
#include "useful_functions.h"
#include "gui.h"
#include <ranges>
#include <string>
#include <sstream>
#include <thread>
#include <cmath>
#include "windows_tests.h"
#include <utility>
#include <iomanip>

// Tests pour la parallélisation
vector<thread> threads;

// Constructeur par défaut
Board::Board() {
}

// Constructeur de copie
Board::Board(const Board& b) {
	// Copie du plateau
	memcpy(_array, b._array, sizeof(_array));
	_got_moves = b._got_moves;
	_player = b._player;
	memcpy(_moves, b._moves, sizeof(_moves));
	_sorted_moves = b._sorted_moves;
	_quick_sorted_moves = b._quick_sorted_moves;
	_evaluation = b._evaluation;
	_castling_rights = b._castling_rights;
	_half_moves_count = b._half_moves_count;
	_moves_count = b._moves_count;
	_white_king_pos = b._white_king_pos;
	_black_king_pos = b._black_king_pos;
}

// Fonction qui copie les attributs d'un tableau
void Board::copy_data(const Board& b) {
	// Copie du plateau
	memcpy(_array, b._array, sizeof(_array));
	_got_moves = b._got_moves;
	_player = b._player;
	memcpy(_moves, b._moves, sizeof(_moves));
	_sorted_moves = b._sorted_moves;
	_quick_sorted_moves = b._quick_sorted_moves;
	_evaluation = b._evaluation;
	_castling_rights = b._castling_rights;
	_half_moves_count = b._half_moves_count;
	_moves_count = b._moves_count;
	_white_king_pos = b._white_king_pos;
	_black_king_pos = b._black_king_pos;
}

// Fonction qui ajoute un coup dans une liste de coups
bool Board::add_move(const uint_fast8_t i, const uint_fast8_t j, const uint_fast8_t k, const uint_fast8_t l, int* iterator, const uint_fast8_t piece)
{
	// Si on dépasse le nombre de coups que l'on pensait possible dans une position
	if (*iterator >= max_moves)
		return false;

	const Move m(i, j, k, l, false, false); // Si on utilise pas les flag, autant éviter les calculs inutiles
	//const Move m(i, j, k, l, _array[k][l] != 0, (piece == 1 && i == 7) || (piece == 7 && i == 1));
	_moves[*iterator] = m;

	// Incrémentation du nombre de coups
	(*iterator)++;

	return true;
}

// Fonction qui ajoute les coups "pions" dans la liste de coups
bool Board::add_pawn_moves(const uint_fast8_t i, const uint_fast8_t j, int* iterator, const uint_fast8_t piece) {
	// FIXME : ne pas utiliser 'piece', et mettre juste 1 et 7 à la place?

	// Joueur avec les pièces blanches
	if (_player) {
		_array[i + 1][j] == 0 && add_move(i, j, i + 1, j, iterator, piece) && i == 1 && _array[i + 2][j] == 0 && add_move(i, j, i + 2, j, iterator, piece); // Poussées
		j > 0 && (is_in_fast(_array[i + 1][j - 1], 7, 12) || (_en_passant_col == j - 1 && i == 4)) && add_move(i, j, i + 1, j - 1, iterator, piece); // Prise (gauche)
		j < 7 && (is_in_fast(_array[i + 1][j + 1], 7, 12) || (_en_passant_col == j + 1 && i == 4)) && add_move(i, j, i + 1, j + 1, iterator, piece); // Prise (droite)
	}
	// Joueur avec les pièces noires
	else {
		_array[i - 1][j] == 0 && add_move(i, j, i - 1, j, iterator, piece) && i == 6 && _array[i - 2][j] == 0 && add_move(i, j, i - 2, j, iterator, piece); // Poussées
		j > 0 && (is_in_fast(_array[i - 1][j - 1], 1, 6) || (_en_passant_col == j - 1 && i == 3)) && add_move(i, j, i - 1, j - 1, iterator, piece); // Prise (gauche)
		j < 7 && (is_in_fast(_array[i - 1][j + 1], 1, 6) || (_en_passant_col == j + 1 && i == 3)) && add_move(i, j, i - 1, j + 1, iterator, piece); // Prise (droite)
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
bool Board::get_moves(const bool pseudo, const bool forbide_check) {
	// Si la partie est finie

	// Règle des 50 coups
	if (_half_moves_count >= max_half_moves) {
		_got_moves = 0;
		return false;
	}

	if (_got_moves != -1) {
		// Si on souhaite calculer les autres types de coups (légaux plutôt qu'illégaux, ou inversement...)
		if (_pseudo_moves == forbide_check)
			_pseudo_moves = !forbide_check;
		else
			return true;
	}

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
			add_pawn_moves(i, j, &iterator, 1);
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
			if (_castling_rights.q_w && _array[i][j - 1] == 0 && _array[i][j - 2] == 0 && _array[i][j - 3] == 0 && (pseudo || (!attacked(i, j) && !attacked(i, j - 1) && !attacked(i, j - 2))))
				add_move(i, j, i, j - 2, &iterator, 6);
			// Petit
			if (_castling_rights.k_w && _array[i][j + 1] == 0 && _array[i][j + 2] == 0 && (pseudo || (!attacked(i, j) && !attacked(i, j + 1) && !attacked(i, j + 2))))
				add_move(i, j, i, j + 2, &iterator, 6);
			break;

		case 7: // Pion noir
			add_pawn_moves(i, j, &iterator, 7);
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
			if (_castling_rights.q_b && _array[i][j - 1] == 0 && _array[i][j - 2] == 0 && _array[i][j - 3] == 0 && (pseudo || (!attacked(i, j) && !attacked(i, j - 1) && !attacked(i, j - 2))))
				add_move(i, j, i, j - 2, &iterator, 12);
			// Petit
			if (_castling_rights.k_b && _array[i][j + 1] == 0 && _array[i][j + 2] == 0 && (pseudo || (!attacked(i, j) && !attacked(i, j + 1) && !attacked(i, j + 2))))
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

// Fonction qui dit si une case est attaquée
bool Board::attacked(const int i, const int j) const
{
	// Regarde tous les coups adverses dans cette position, puis renvoie si l'un d'entre eux a pour case finale, la case en argument
	Board b;
	b.copy_data(*this);
	b._player = !b._player;
	b._half_moves_count = 0;
	b.get_moves(true);
	for (int m = 0; m < b._got_moves; m++) {
		if (i == b._moves[m].i2 && j == b._moves[m].j2)
			return true;
	}

	return false;
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
	constexpr int knight_offsets[8][2] = { {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}, {1, -2}, {2, -1}, {2, 1}, {1, 2} };
	// TODO : mettre en static? et regrouper avec ceux des autres fonctions?

	for (int k = 0; k < 8; k++) {
		const int ni = king_i + knight_offsets[k][0];

		// S'il y a un cavalier qui attaque, renvoie vrai (en échec)
		if (const int nj = king_j + knight_offsets[k][1]; ni >= 0 && ni < 8 && nj >= 0 && nj < 8 && _array[ni][nj] == (2 + _player * 6))
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
		get_moves(false, !pseudo);

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
void Board::make_move(Move move, const bool pgn, const bool new_board, const bool add_to_list)
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
	(p == 1 || p == 7 || p_last) && ((_half_moves_count = 0));


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



	// TODO : rendre plus efficace (les accès mémoire sont lents)
	_last_move[0] = i;
	_last_move[1] = j;
	_last_move[2] = k;
	_last_move[3] = l;
	_last_move[4] = p;
	_last_move[9] = p_last;

	// Reset le nombre de coups possibles
	_got_moves = -1;

	// Les coups ne sont plus triés
	_sorted_moves = false;
	_quick_sorted_moves = false;

	// Il faut regarder de nouveau les fins de partie
	_game_over_checked = false;

	reset_eval();

	_new_board = true;

	if (new_board) {
		if (_is_active)
			reset_all();
		_tested_moves = 0;
		_current_move = 0;
		_nodes = 0;
		_evaluated = false;
	}

	if (add_to_list) {
		all_positions[_half_moves_count] = simple_position();
		total_positions++;
		total_positions = _half_moves_count;
	}

	return;
}

// Fonction qui joue le coup i
void Board::make_index_move(const int i, const bool pgn, const bool add_to_list) {
	make_move(_moves[i], pgn, false, add_to_list);
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
	static constexpr int adv_queen = 50;
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

	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			if (const uint_fast8_t piece = _array[i][j]) {
				const int value = static_cast<int>(static_cast<float>(eval->_pieces_value_begin[(piece - 1) % 6]) * (1.0f - _adv) + static_cast<float>(eval->_pieces_value_end[(piece - 1) % 6]) * _adv);
				material_count += (piece < 7) ? value : -value;
			}
		}
	}

	return material_count;
}

// Fonction qui calcule et renvoie la valeur de positionnement des pièces sur l'échiquier
// TODO : fusion avec material
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
bool Board::evaluate(Evaluator* eval, const bool display, Network* n)
{
	/*if (_evaluated)
		return false;*/

	_evaluated = true;
	_mate = false;

	if (display)
	{
		eval_components = "";
		_displayed_components = true;
	}
	else
	{
		_displayed_components = false;
	}

	_evaluator = eval;



	// Reset l'évaluation
	_evaluation = 0.0f;

	// Avancement de la partie
	game_advancement();
	if (display)
		eval_components += "game advancement: " + to_string(static_cast<int>(round(100 * _adv))) + "%\n";

	// Matériel
	if (eval->_piece_value != 0.0f) {
		const float material = count_material(eval) * eval->_piece_value / 100; // à changer (le /100)
		if (display)
			eval_components += "material: " + (material >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * material))) + "\n";
		_evaluation += material;
	}	

	
	// TODO : à compter directement avec le matériel
	// Comptage des fous
	uint_fast8_t bishop_w = 0; uint_fast8_t bishop_b = 0;
	for (uint_fast8_t i = 0; i < 8; i++) {
		for (uint_fast8_t j = 0; j < 8; j++) {
			const uint_fast8_t p = _array[i][j];
			(p == 3) && bishop_w++;
			(p == 9) && bishop_b++;
		}
	}

	// Paire de oufs
	if (eval->_bishop_pair != 0.0f) {
		const float bishop_pair = eval->_bishop_pair * ((bishop_w >= 2) - (bishop_b >= 2));
		if (display)
			eval_components += "bishop pair: " + (bishop_pair >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * bishop_pair))) + "\n";
		_evaluation += bishop_pair;
	}

	// Positionnement des pièces
	if (eval->_piece_positioning != 0.0f) {
		const float positioning = pieces_positioning(eval) * eval->_piece_positioning;
		if (display)
			eval_components += "positioning: " + (positioning >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * positioning))) + "\n";
		_evaluation += positioning;
	}

	// Ajout random
	if (eval->_random_add != 0.0f) {
		float random_add = 0.0f;
		random_add += static_cast<float>(GetRandomValue(-50, 50)) * eval->_random_add / 100;
		if (display)
			eval_components += "random add: " + (random_add >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * random_add))) + "\n";
		_evaluation += random_add;
	}

	// Mobilité des pièces
	if (eval->_piece_mobility != 0.0f) {
		const float piece_mobility = static_cast<float>(get_piece_mobility()) * eval->_piece_mobility;
		if (display)
			eval_components += "piece mobility: " + (piece_mobility >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * piece_mobility))) + "\n";
		_evaluation += piece_mobility;
	}

	// Activité des pièces
	if (eval->_piece_activity != 0.0f) {
		const float piece_activity = static_cast<float>(get_piece_activity()) * eval->_piece_activity;
		if (display)
			eval_components += "piece activity: " + (piece_activity >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * piece_activity))) + "\n";
		_evaluation += piece_activity;
	}

	// Sécurité du roi
	if (eval->_king_safety != 0.0f) {
		const float king_safety = static_cast<float>(get_king_safety()) * eval->_king_safety;
		if (display)
			eval_components += "king safety: " + (king_safety >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * king_safety))) + "\n";
		_evaluation += king_safety;
	}

	// Droits de roques
	if (eval->_castling_rights != 0.0f) {
		float castling_rights = 0.0f;
		castling_rights += eval->_castling_rights * static_cast<float>(_castling_rights.k_w + _castling_rights.q_w - _castling_rights.k_b - _castling_rights.q_b) * (1 - _adv);
		if (display)
			eval_components += "castling rights: " + (castling_rights >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * castling_rights))) + "\n";
		_evaluation += castling_rights;
	}

	// Contrôle des cases
	if (eval->_square_controls != 0.0f) {
		const float square_controls = static_cast<float>(get_square_controls()) * eval->_square_controls;
		if (display)
			eval_components += "square controls: " + (square_controls >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * square_controls))) + "\n";
		_evaluation += square_controls;
	}

	// Avantage d'espace
	if (eval->_space_advantage != 0.0f)
	{
		const float space = static_cast<float>(get_space()) * eval->_space_advantage;
		if (display)
			eval_components += "space: " + (space >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * space))) + "\n";
		_evaluation += space;
	}

	// Structure de pions
	if (eval->_pawn_structure != 0.0f) {
		const float pawn_structure = static_cast<float>(get_pawn_structure(display * eval->_pawn_structure)) * eval->_pawn_structure;
		if (display)
			eval_components += "pawn structure: " + (pawn_structure >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * pawn_structure))) + "\n";
		_evaluation += pawn_structure;
	}

	// Attaques et défenses de pièces
	if (eval->_attacks != 0.0f || eval->_defenses != 0.0f) {
		const float pieces_attacks_and_defenses = get_attacks_and_defenses(eval->_attacks, eval->_defenses);
		if (display)
			eval_components += "attacks/defenses: " + (pieces_attacks_and_defenses >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * pieces_attacks_and_defenses))) + "\n";
		_evaluation += pieces_attacks_and_defenses;
	}

	// Opposition des rois
	if (eval->_kings_opposition != 0.0f) {
		const float kings_opposition = static_cast<float>(get_kings_opposition()) * eval->_kings_opposition;
		if (display)
			eval_components += "king opposition: " + (kings_opposition >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * kings_opposition))) + "\n";
		_evaluation += kings_opposition;
	}

	// Tours sur les colonnes ouvertes / semi-ouvertes
	if (eval->_rook_open != 0.0f) {
		const float rook_open = static_cast<float>(get_rooks_on_open_file()) * eval->_rook_open;
		if (display)
			eval_components += "rooks on open/semi files: " + (rook_open >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * rook_open))) + "\n";
		_evaluation += rook_open;
	}

	// Alignement des pièces (fou-tour/dame-roi)
	if (eval->_alignments != 0.0f)
	{
		const float pieces_alignment = static_cast<float>(get_alignments()) * eval->_alignments;
		if (display)
			eval_components += "pieces alignment: " + (pieces_alignment >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * pieces_alignment))) + "\n";
		_evaluation += pieces_alignment;
	}

	// Fous en fianchetto
	if (eval->_fianchetto != 0.0f) {
		const float fianchetto = static_cast<float>(get_fianchetto_value()) * eval->_fianchetto;
		if (display)
			eval_components += "fianchetto bishops: " + (fianchetto >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * fianchetto))) + "\n";
		_evaluation += fianchetto;
	}

	// Trait du joueur
	if (eval->_player_trait != 0.0f) {
		const float player_trait = eval->_player_trait * static_cast<float>(get_color());
		if (display)
			eval_components += "player trait: " + (player_trait >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * player_trait))) + "\n";
		_evaluation += player_trait;
	}

	// Total de l'évaluation
	if (display)
		eval_components += "--- total: " + (_evaluation >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * _evaluation))) + " ---\n";


	// Forteresse
	if (eval->_push != 0.0f) {
		const float push = 1 - static_cast<float>(_half_moves_count) * eval->_push / max_half_moves;
		if (display)
			eval_components += "fortress: " + to_string(static_cast<int>(100 - push * 100)) + "%\n";
		_evaluation *= push;
	}

	// Chances de gain
	const float win_chance = get_winning_chances_from_eval(_evaluation * 100, false, true);
	if (display)
		eval_components += "W/D/L: " + to_string(static_cast<int>(100 * win_chance)) + "/" + to_string(static_cast<int>(100 * 0)) + "/" + to_string(static_cast<int>(100 * (1.0f - win_chance))) + "%\n";

	// Partie non finie
	return false;
}

// Fonction qui évalue la position à l'aide d'heuristiques -> évaluation entière
bool Board::evaluate_int(Evaluator* eval, const bool display, Network* n) {
	const bool evaluated = evaluate(eval, display, n);
	if (n == nullptr || _mate)
		_evaluation *= 100;
	_evaluation = _evaluation + 0.5f - static_cast<float>(_evaluation < 0); // pour l'arrondi
	_static_evaluation = static_cast<int>(_evaluation);

	return evaluated;
}

// Fonction qui joue le coup d'une position, renvoyant la meilleure évaluation à l'aide d'un negamax (similaire à un minimax)
int Board::negamax(const int depth, int alpha, const int beta, const bool max_depth, Evaluator* eval, const bool play, const bool display, const int quiescence_depth, const int null_depth) 
{
	// Nombre de noeuds
	if (max_depth) {
		visited_nodes = 1;
		begin_time = clock();
	}
	else {
		visited_nodes++;
	}

	// Vérifie si la position est terminée
	is_game_over();

	if (_game_over_value == 2)
		return 0;
	if (_game_over_value != 0)
		return -mate_value + _moves_count * mate_ply;

	// Evaluation de la position via quiescence
	if (depth <= 0)
		return quiescence(eval, -INT_MAX, INT_MAX, quiescence_depth, false);


	// Null move pruning
	/*if (null_depth > 0 && depth > 1 && !in_check())
	{
		_player = !_player;
		int null_move_value = -negamax(depth - 1 - null_depth, -beta, -beta + 1, false, eval, false, false, quiescence_depth, 0);
		_player = !_player;

		if (null_move_value >= beta)
			return null_move_value;
	}*/


	int value = -1e9;
	Board b;

	int best_move = 0;

	if (depth > 1) {
		//sort_moves(eval);
		quick_moves_sort();
	}
		
	if (max_depth) {
		display_moves();
	}
		

	for (int i = 0; i < _got_moves; i++) {
		b.copy_data(*this);
		b.make_index_move(i);

		int tmp_value = -b.negamax(depth - 1, -beta, -alpha, false, eval, false, false, quiescence_depth);
		// threads.emplace_back(std::thread([&]() {
		//     tmp_value = -b.negamax(depth - 1, -beta, -alpha, -color, false, eval, a, use_agent);
		// })); // Test de OpenAI

		if (max_depth) {
			if (display)
				cout << "move : " << move_label_from_index(i) << ", value : " << tmp_value << endl;
			if (tmp_value > value)
				best_move = i;
		}

		value = max(value, tmp_value);
		alpha = max(alpha, value);
		// undo move
		// b.undo();

		if (alpha >= beta)
			break;
	}

	// Attendre la fin des threads
	// for (auto &thread : threads) {
	//     thread.join();
	// } // Test de OpenAI

	if (max_depth) {
		if (display) {
			cout << "visited nodes : " << visited_nodes / 1000 << "k" << endl;
			const auto spent_time = static_cast<double>(clock() - begin_time);
			cout << "time spend : " << spent_time << "ms" << endl;
			cout << "speed : " << visited_nodes / spent_time << "kN/s" << endl;
		}
		if (play) {
			play_index_move_sound(best_move);
			if (display)
				if (_tested_moves > 0)
					((main_GUI._click_bind && main_GUI._board.click_i_move(main_GUI._board.best_monte_carlo_move(), get_board_orientation())) || true) && play_monte_carlo_move_keep(best_move, true, true, true, false);
				else
					make_index_move(best_move, true);
		}

		return value;
	}

	return value;
}

// Grogrosfish
bool Board::grogrosfish(const int depth, Evaluator* eval, const bool display = false) {
	negamax(depth, -1e9, 1e9, true, eval, true, display);
	if (display) {
		evaluate(eval);
		cout << main_GUI._current_fen << endl;
		cout << main_GUI._global_pgn << endl;
	}

	return true;
}

// Fonction qui revient à la position précédente (ne marchera pas avec les roques pour le moment)
bool Board::undo(const uint_fast8_t i1, const uint_fast8_t j1, const uint_fast8_t p1, const uint_fast8_t i2, const uint_fast8_t j2, const uint_fast8_t p2, const int half_moves) {
	_array[i1][j1] = p1;
	_array[i2][j2] = p2;

	// Incrémentation des demi-coups
	_half_moves_count = half_moves;

	_player = !_player;
	_got_moves = -1;

	// Decrémentation des coups
	!_player && _moves_count--;

	return true;
}

// Une surcharge
bool Board::undo() {
	// Il faut rétablir les droits de roque, les en passant........

	print_array(_last_move, 10);
	const int_fast8_t i1 = _last_move[0];
	const int_fast8_t j1 = _last_move[1];
	const int_fast8_t k1 = _last_move[2];
	const int_fast8_t l1 = _last_move[3];
	const int_fast8_t p1 = _last_move[4];

	const int_fast8_t i2 = _last_move[5];
	const int_fast8_t j2 = _last_move[6];
	const int_fast8_t k2 = _last_move[7];
	const int_fast8_t l2 = _last_move[8];
	const int_fast8_t p2 = _last_move[9];

	if (i1 == -1 || j1 == -1 || k1 == -1 || l1 == -1 || p1 == -1)
		return true;

	// Replace les piècse leur place initiale
	_array[k1][l1] = 0;
	_array[i1][j1] = p1;

	if (i2 != -1 && j2 != -1 && k2 != -1 && l2 != -1 && p2 != -1) {
		_array[k2][l2] = 0;
		_array[i2][j2] = p2;
	}

	_player = !_player;
	_got_moves = -1;

	// Incrémentation des coups
	!_player && (_moves_count -= 1);

	_last_move[0] = -1;
	_last_move[1] = -1;
	_last_move[2] = -1;
	_last_move[3] = -1;
	_last_move[4] = -1;
	_last_move[5] = -1;
	_last_move[6] = -1;
	_last_move[7] = -1;
	_last_move[8] = -1;
	_last_move[9] = -1;

	_sorted_moves = false;
	_quick_sorted_moves = false;

	reset_eval();

	_new_board = true;

	if constexpr (constexpr bool new_board = true) {
		if (_is_active)
			reset_all();
		_tested_moves = 0;
		_current_move = 0;
		_nodes = 0;
		_evaluated = false;
	}

	_mate = false;
	_mate_checked = false;
	_game_over_checked = false;

	// Decrémentation des coups
	!_player && _moves_count--;
	// TODO : remettre le compteur des half_move à ce qu'il était auparavent...

	// Reste à changer le PGN, gérer les roques, en passant, demi-coups...

	return true;
}

// Fonction qui arrange les coups de façon "logique", pour optimiser les algorithmes de calcul
void Board::sort_moves(Evaluator* eval) {
	// Modifier pour seulement garder le (ou les deux) meilleur(s) coups?

	Board b;

	(_got_moves == -1 && get_moves());

	auto* values = new float[_got_moves];

	// Création de la liste des valeurs des évaluations des positions après chaque coup
	for (int i = 0; i < _got_moves; i++) {
		// Mise à jour du plateau
		b.copy_data(*this);
		b.make_index_move(i);

		// Evaluation
		b.evaluate(eval);
		const float value = get_color() * b._evaluation;

		// Place l'évaluation en i dans les valeurs
		values[i] = value;
	}

	// Construction des nouveaux coups

	// Liste des index des coups triés par ordre décroissant de valeur
	const auto moves_indexes = new int[_got_moves];

	for (int i = 0; i < _got_moves; i++) {
		const int max_ind = max_index(values, _got_moves);
		moves_indexes[i] = max_ind;
		values[max_ind] = -FLT_MAX;
	}

	// Génération de la list de coups de façon ordonnée
	auto* new_moves = new Move[_got_moves];
	copy(_moves, _moves + _got_moves, new_moves);

	for (int i = 0; i < _got_moves; i++) {
		_moves[i] = new_moves[moves_indexes[i]];
	}

	// Suppression des tableaux
	delete[] values;
	delete[] new_moves;
	delete[] moves_indexes;

	_sorted_moves = true;
	_quick_sorted_moves = false;
}

// Fonction qui récupère le plateau d'un FEN
// TODO : à refaire
void Board::from_fen(string fen)
{
	string pgn;
	reset_all();

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

	_new_board = true;
	_quiescence_nodes = 0;

	reset_eval();

	_last_move[0] = -1;
	_last_move[1] = -1;
	_last_move[2] = -1;
	_last_move[3] = -1;

	_is_game_over = false;

	// Oriente le plateau dans pour le joueur qui joue
	board_orientation = _player;

	// Met à jour le FEN de la position dans la GUI
	main_GUI._initial_fen = fen;
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
		en_passant = abc8[_en_passant_col] + static_cast<string>(_player ? "6" : "3");

	s += " " + en_passant + " " + to_string(_half_moves_count) + " " + to_string(_moves_count);

	return s;
}

// Fonction qui renvoie le gagnant si la partie est finie (-1/1, et 2 pour nulle), et 0 sinon
// Génère également les coups légaux, s'il y en a
int Board::is_game_over() {

	// Ne pas recalculer si déjà fait
	if (_game_over_checked)
		return _game_over_value;
		
	// Pour ne pas le recalculer
	_game_over_checked = true;

	// Règle des 50 coups
	if (_half_moves_count >= max_half_moves) {
		_game_over_value = 2;
		return 2;
	}

	// Règle des 3 répétitions
	// TODO

	// Calcule les coups légaux
	if (_got_moves == -1)
		get_moves(false, true);

	// S'il n'y a pas de coups légaux, c'est soit mat, soit pat
	if (_got_moves == 0) {
		
		// Mat
		if (in_check()) {
			_game_over_value = _player ? -1 : 1;
			return _game_over_value;
		}

		// Pat
		_game_over_value = 2;
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
			else if (p != 6 && p != 12 && p != 0) {
				_game_over_value = 0;
				return 0;
			}

			// Si on a au moins 1 fou, et un cheval/fou ou plus -> plus de nulle par manque de matériel
			if ((count_w_bishop > 0) && (count_w_knight > 0 || count_w_bishop > 1)) {
				_game_over_value = 0;
				return 0;
			}
		}
	}

	// Possibilités de nulles par manque de matériel
	if (count_w_knight + count_w_bishop < 2 && count_b_knight + count_b_bishop < 2) {
		_game_over_value = 2;
		return 2;
	}

	// On ne peut pas mater avec seulement 2 cavaliers
	// TODO: est-ce que la partie est déclarée nulle?
	/*if (count_w_knight == 2 || count_b_knight == 2) {
		_game_over_value = 2;
		return 2;
	}*/

	_game_over_value = 0;
	return 0;
}

// Fonction qui renvoie le label d'un coup
// En passant manquant... échecs aussi, puis roques, promotions, mats/pats
string Board::move_label(Move move)
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
		get_moves(false, true);

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
	case 2: case 8: s += "N"; if (spec_line) s += abc8[j]; if (spec_col) s += static_cast<char>(i + 1 + 48); break;
	case 3: case 9: s += "B"; if (spec_line) s += abc8[j]; if (spec_col) s += static_cast<char>(i + 1 + 48); break;
	case 4: case 10: s += "R"; if (spec_line) s += abc8[j]; if (spec_col) s += static_cast<char>(i + 1 + 48); break;
	case 5: case 11: s += "Q"; if (spec_line) s += abc8[j]; if (spec_col) s += static_cast<char>(i + 1 + 48); break;
	case 6: case 12:
		if (l - j == 2) {
			s += "O-O"; return s;
		}
		if (j - l == 2) {
			s += "O-O-O"; return s;
		}
		s += "K"; break;
	}

	if (p2 || ((p1 == 1 || p1 == 7) && j != l)) {
		if (p1 == 1 || p1 == 7)
			s += abc8[j];
		s += "x";
		s += abc8[l];
		s += static_cast<char>(k + 1 + 48);
	}

	else {
		s += abc8[l];
		s += static_cast<char>(k + 1 + 48);
	}

	// Promotion (seulement en dame pour le moment)
	if ((p1 == 1 && k == 7) || (p1 == 7 && k == 0))
		s += "=Q";

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
		get_moves(false, true);
	return move_label(_moves[i]);
}

// Fonction qui affiche un texte dans une zone donnée
void Board::draw_text_rect(const string& s, const float pos_x, const float pos_y, const float width, const float height, const float size) {
	// Division du texte
	const int sub_div = (1.5f * width) / size;

	if (width <= 0 || height <= 0 || sub_div <= 0)
		return;

	const Rectangle rect_text = { pos_x, pos_y, width, height };
	DrawRectangleRec(rect_text, background_text_color);

	const size_t string_size = s.length();
	int i = 0;
	while (sub_div * i <= string_size) {
		const char* c = s.substr(i * sub_div, sub_div).c_str();
		DrawTextEx(text_font, c, { pos_x, pos_y + i * size }, size, font_spacing * size, text_color);
		i++;
	}
}

// Fonction qui charge les textures
void load_resources() {
	cout << GetWorkingDirectory() << endl;

	// Pièces
	piece_images[0] = LoadImage("resources/images/w_pawn.png");
	piece_images[1] = LoadImage("resources/images/w_knight.png");
	piece_images[2] = LoadImage("resources/images/w_bishop.png");
	piece_images[3] = LoadImage("resources/images/w_rook.png");
	piece_images[4] = LoadImage("resources/images/w_queen.png");
	piece_images[5] = LoadImage("resources/images/w_king.png");
	piece_images[6] = LoadImage("resources/images/b_pawn.png");
	piece_images[7] = LoadImage("resources/images/b_knight.png");
	piece_images[8] = LoadImage("resources/images/b_bishop.png");
	piece_images[9] = LoadImage("resources/images/b_rook.png");
	piece_images[10] = LoadImage("resources/images/b_queen.png");
	piece_images[11] = LoadImage("resources/images/b_king.png");

	// Mini-Pièces
	mini_piece_images[0] = LoadImage("resources/images/mini_pieces/w_pawn.png");
	mini_piece_images[1] = LoadImage("resources/images/mini_pieces/w_knight.png");
	mini_piece_images[2] = LoadImage("resources/images/mini_pieces/w_bishop.png");
	mini_piece_images[3] = LoadImage("resources/images/mini_pieces/w_rook.png");
	mini_piece_images[4] = LoadImage("resources/images/mini_pieces/w_queen.png");
	mini_piece_images[5] = LoadImage("resources/images/mini_pieces/w_king.png");
	mini_piece_images[6] = LoadImage("resources/images/mini_pieces/b_pawn.png");
	mini_piece_images[7] = LoadImage("resources/images/mini_pieces/b_knight.png");
	mini_piece_images[8] = LoadImage("resources/images/mini_pieces/b_bishop.png");
	mini_piece_images[9] = LoadImage("resources/images/mini_pieces/b_rook.png");
	mini_piece_images[10] = LoadImage("resources/images/mini_pieces/b_queen.png");
	mini_piece_images[11] = LoadImage("resources/images/mini_pieces/b_king.png");

	// Chargement du son
	move_1_sound = LoadSound("resources/sounds/move_1.mp3");
	move_2_sound = LoadSound("resources/sounds/move_2.mp3");
	castle_1_sound = LoadSound("resources/sounds/castle_1.mp3");
	castle_2_sound = LoadSound("resources/sounds/castle_2.mp3");
	check_1_sound = LoadSound("resources/sounds/check_1.mp3");
	check_2_sound = LoadSound("resources/sounds/check_2.mp3");
	capture_1_sound = LoadSound("resources/sounds/capture_1.mp3");
	capture_2_sound = LoadSound("resources/sounds/capture_2.mp3");
	checkmate_sound = LoadSound("resources/sounds/checkmate.mp3");
	stealmate_sound = LoadSound("resources/sounds/stealmate.mp3");
	game_begin_sound = LoadSound("resources/sounds/game_begin.mp3");
	game_end_sound = LoadSound("resources/sounds/game_end.mp3");
	promotion_sound = LoadSound("resources/sounds/promotion.mp3");

	// Police de l'écriture
	text_font = LoadFontEx("resources/fonts/SF TransRobotics.ttf", 64, 0, 250);
	// text_font = GetFontDefault();

	// Icône
	icon = LoadImage("resources/images/grogros_zero.png"); // TODO essayer de charger le .ico, pour que l'icone s'affiche tout le temps (pas seulement lors du build)
	SetWindowIcon(icon);
	UnloadImage(icon);

	// Grogros
	grogros_image = LoadImage("resources/images/grogros_zero.png");

	// Curseur
	cursor_image = LoadImage("resources/images/cursor_new.png");

	loaded_resources = true;
}

// Fonction qui met à la bonne taille les images et les textes de la GUI
void resize_GUI() {
	const int min_screen = min(main_GUI._screen_height, main_GUI._screen_width);
	board_size = board_scale * min_screen;
	board_padding_y = (main_GUI._screen_height - board_size) / 4.0f;
	board_padding_x = (main_GUI._screen_height - board_size) / 8.0f;

	tile_size = board_size / 8.0f;
	piece_size = tile_size * piece_scale;
	arrow_thickness = tile_size * arrow_scale;

	// Génération des textures

	// Pièces
	for (int i = 0; i < 12; i++) {
		Image piece_image = ImageCopy(piece_images[i]);
		ImageResize(&piece_image, piece_size, piece_size);
		piece_textures[i] = LoadTextureFromImage(piece_image);
	}
	text_size = board_size / 16.0f;

	// Grogros
	grogros_size = board_size / 16.0f;
	Image grogros_copy = ImageCopy(grogros_image);
	ImageResize(&grogros_copy, grogros_size, grogros_size);
	grogros_texture = LoadTextureFromImage(grogros_copy);

	// Curseur
	Image cursor_copy = ImageCopy(cursor_image);
	ImageResize(&cursor_copy, cursor_size, cursor_size);
	cursor_texture = LoadTextureFromImage(cursor_copy);

	// Mini-pièces (pour le compte des pièces prises durant la partie)
	mini_piece_size = text_size / 3;
	for (int i = 0; i < 12; i++) {
		Image mini_piece_image = ImageCopy(mini_piece_images[i]);
		ImageResize(&mini_piece_image, mini_piece_size, mini_piece_size);
		mini_piece_textures[i] = LoadTextureFromImage(mini_piece_image);
	}
}

// Fonction qui actualise les nouvelles dimensions de la fenêtre
void get_window_size() {
	main_GUI._screen_width = GetScreenWidth();
	main_GUI._screen_height = GetScreenHeight();
}

// Fonction qui dessine le plateau
bool Board::draw() {
	// Chargement des textures, si pas déjà fait
	if (!loaded_resources) {
		load_resources();
		resize_GUI();
		PlaySound(game_begin_sound);
	}

	// Position de la souris
	mouse_pos = GetMousePosition();

	// Si on clique avec la souris
	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
		// Retire toute les cases surlignées
		remove_highlighted_tiles();

		// Retire toutes les flèches
		arrows_array = {};

		// Si on était pas déjà en train de cliquer (début de clic)
		if (!clicked) {
			// Stocke la case cliquée sur le plateau
			clicked_pos = get_pos_from_GUI(mouse_pos.x, mouse_pos.y);
			clicked = true;

			// S'il y'a les flèches de réflexion de GrogrosZero, et qu'aucune pièce n'est sélectionnée
			if (drawing_arrows && !selected_piece()) {
				// On regarde dans le sens inverse pour jouer la flèche la plus récente (donc celle visible en cas de superposition)
				for (auto arrow : ranges::reverse_view(grogros_arrows))
				{
					if (arrow[2] == clicked_pos.first && arrow[3] == clicked_pos.second) {
						// Retrouve le coup correspondant
						play_move_sound(Move(arrow[0], arrow[1], arrow[2], arrow[3]));
						((main_GUI._click_bind && main_GUI._board.click_i_move(arrow[4], get_board_orientation())) || true) && play_monte_carlo_move_keep(arrow[4], true, true, true, true);
						goto piece_selection;
					}
				}
			}

		piece_selection:

			// Si aucune pièce n'est sélectionnée et que l'on clique sur une pièce, la sélectionne
			if (!selected_piece() && clicked_piece()) {
				if (false || clicked_piece_has_trait())
					selected_pos = clicked_pos;
			}

			// Si le coup est l'un des mouvements possible de la pièce (diagonale pour le fou...)
			// Quand cette pièce est sélectionnée, il faut afficher ces coups
			// Il faut de même déplacer la pièce virtuellement quand on fait les pre-move

			// Si une pièce est déjà sélectionnée
			else if (selected_piece()) {
				// Si c'est pas ton tour, pre-move, et déselectionne la pièce
				if (selected_piece() > 0 && (selected_piece() < 7 && !_player) || (selected_piece() >= 7 && _player)) {
					/*pre_move[0] = selected_pos.first;
					pre_move[1] = selected_pos.second;
					pre_move[2] = clicked_pos.first;
					pre_move[3] = clicked_pos.second;*/
					unselect();
				}

				// Si le coup est légal, le joue
				get_moves(false, true);
				for (int i = 0; i < _got_moves; i++) {
					if (_moves[i].i1 == selected_pos.first && _moves[i].j1 == selected_pos.second && _moves[i].i2 == clicked_pos.first && _moves[i].j2 == clicked_pos.second) {
						play_move_sound(Move(selected_pos.first, selected_pos.second, clicked_pos.first, clicked_pos.second));
						((main_GUI._click_bind && main_GUI._board.click_i_move(i, get_board_orientation())) || true) && play_monte_carlo_move_keep(i, true, true, true, true);
						break;
					}
				}

				// Déselectionne
				unselect();

				// Changement de sélection de pièce
				if ((_player && is_in_fast(_array[clicked_pos.first][clicked_pos.second], 1, 6)) || (!_player && is_in_fast(_array[clicked_pos.first][clicked_pos.second], 7, 12)))
					selected_pos = get_pos_from_GUI(mouse_pos.x, mouse_pos.y);
			}
		}
	}
	else {
		// Si on clique
		if (clicked && clicked_pos.first != -1 && _array[clicked_pos.first][clicked_pos.second] != 0) {
			pair<uint_fast8_t, uint_fast8_t> drop_pos = get_pos_from_GUI(mouse_pos.x, mouse_pos.y);
			if (is_in_fast(drop_pos.first, 0, 7) && is_in_fast(drop_pos.second, 0, 7)) {
				// Déselection de la pièce si on reclique dessus
				if (drop_pos.first == selected_pos.first && drop_pos.second == selected_pos.second) {
				}
				else {
					if (int selected_piece = _array[selected_pos.first][selected_pos.second]; selected_piece > 0 && (selected_piece < 7 && !_player) || (selected_piece >= 7 && _player)) {
						// Si c'est pas ton tour, pre-move
						/*pre_move[0] = selected_pos.first;
						pre_move[1] = selected_pos.second;
						pre_move[2] = drop_pos.first;
						pre_move[3] = drop_pos.second;*/
						selected_pos = { -1, -1 };
					}

					else {
						// Si le coup est légal
						get_moves(false, true);
						for (int i = 0; i < _got_moves; i++) {
							if (_moves[i].i1 == selected_pos.first && _moves[i].j1 == selected_pos.second && _moves[i].i2 == drop_pos.first && _moves[i].j2 == drop_pos.second) {
								play_move_sound(Move(clicked_pos.first, clicked_pos.second, drop_pos.first, drop_pos.second));
								((main_GUI._click_bind && main_GUI._board.click_i_move(i, get_board_orientation())) || true) && play_monte_carlo_move_keep(i, true, true, true, true);
								selected_pos = { -1, -1 };
								break;
							}
						}
					}
				}
			}
		}

		clicked = false;
	}

	// Pre-moves
	/*if (pre_move[0] != -1 && pre_move[1] != -1 && pre_move[2] != -1 && pre_move[3] != -1) {
		if ((!_player && is_in_fast(_array[pre_move[0]][pre_move[1]], 7, 12)) || (_player && is_in_fast(_array[pre_move[0]][pre_move[1]], 1, 6))) {
			if (_got_moves == -1)
				get_moves(false, true);
			for (int i = 0; i < _got_moves; i++) {
				if (_moves[i].i1 == pre_move[0] && _moves[i].j1 == pre_move[1] && _moves[i].i2 == pre_move[2] && _moves[i].j2 == pre_move[3]) {
					play_move_sound(Move(pre_move[0], pre_move[1], pre_move[2], pre_move[3]));
					((main_GUI._click_bind && main_GUI._board.click_i_move(i, get_board_orientation())) || true) && play_monte_carlo_move_keep(i, true, true, true, true);
					break;
				}
			}
			pre_move[0] = -1;
			pre_move[1] = -1;
			pre_move[2] = -1;
			pre_move[3] = -1;
		}
	}*/

	// Si on fait un clic droit
	if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
		int x_mouse = get_pos_from_GUI(mouse_pos.x, mouse_pos.y).first;
		int y_mouse = get_pos_from_GUI(mouse_pos.x, mouse_pos.y).second;
		right_clicked_pos = { x_mouse, y_mouse };

		// Retire les pre-moves
		/*pre_move[0] = -1;
		pre_move[1] = -1;
		pre_move[2] = -1;
		pre_move[3] = -1;*/
	}

	// Si on fait un clic droit (en le relachant)
	if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
		int x_mouse = get_pos_from_GUI(mouse_pos.x, mouse_pos.y).first;
		int y_mouse = get_pos_from_GUI(mouse_pos.x, mouse_pos.y).second;

		if (x_mouse != -1) {
			// Surlignage d'une case
			if (pair<int, int>{x_mouse, y_mouse} == right_clicked_pos)
				highlighted_array[x_mouse][y_mouse] = 1 - highlighted_array[x_mouse][y_mouse];

			// Flèche
			else {
				if (right_clicked_pos.first != -1 && right_clicked_pos.second != -1 && x_mouse != -1 && y_mouse != -1) {
					vector<int> arrow = { right_clicked_pos.first, right_clicked_pos.second, x_mouse, y_mouse };

					// Si la flèche existe, la supprime
					if (auto found_arrow = find(arrows_array.begin(), arrows_array.end(), arrow); found_arrow != arrows_array.end())
						arrows_array.erase(found_arrow);

					// Sinon, la rajoute
					else
						arrows_array.push_back(arrow);
				}
				
			}
		}
	}

	// Dessins

	// Couleur de fond
	ClearBackground(background_color);

	// Nombre de FPS
	DrawTextEx(text_font, ("FPS : " + to_string(GetFPS())).c_str(), { main_GUI._screen_width - 3 * text_size, text_size / 3 }, text_size / 3, font_spacing, text_color);

	// Plateau
	draw_rectangle(board_padding_x, board_padding_y, tile_size * 8, tile_size * 8, board_color_light);

	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++)
			((i + j) % 2 == 1) && draw_rectangle(board_padding_x + tile_size * j, board_padding_y + tile_size * i, tile_size, tile_size, board_color_dark);

	// Coordonnées sur le plateau
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++) {
			if (j == 0 + 7 * board_orientation) // Chiffres
				DrawTextEx(text_font, to_string(i + 1).c_str(), { board_padding_x + text_size / 8, board_padding_y + tile_size * orientation_index(7 - i) + text_size / 8 }, text_size / 2, font_spacing, ((i + j) % 2 == 1) ? board_color_light : board_color_dark);
			if (i == 0 + 7 * board_orientation) // Lettres
				DrawTextEx(text_font, abc8.substr(j, 1).c_str(), { board_padding_x + tile_size * (orientation_index(j) + 1) - text_size / 2, board_padding_y + tile_size * 8 - text_size / 2 }, text_size / 2, font_spacing, ((i + j) % 2 == 1) ? board_color_light : board_color_dark);
		}

	// Surligne du dernier coup joué
	if (_last_move[0] != -1) {
		draw_rectangle(board_padding_x + orientation_index(_last_move[1]) * tile_size, board_padding_y + orientation_index(7 - _last_move[0]) * tile_size, tile_size, tile_size, last_move_color);
		draw_rectangle(board_padding_x + orientation_index(_last_move[3]) * tile_size, board_padding_y + orientation_index(7 - _last_move[2]) * tile_size, tile_size, tile_size, last_move_color);
	}

	// Cases surglignées
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++)
			if (highlighted_array[i][j])
				draw_rectangle(board_padding_x + tile_size * orientation_index(j), board_padding_y + tile_size * orientation_index(7 - i), tile_size, tile_size, highlight_color);

	// Pre-move
	/*if (pre_move[0] != -1 && pre_move[1] != -1 && pre_move[2] != -1 && pre_move[3] != -1) {
		draw_rectangle(board_padding_x + orientation_index(pre_move[1]) * tile_size, board_padding_y + orientation_index(7 - pre_move[0]) * tile_size, tile_size, tile_size, pre_move_color);
		draw_rectangle(board_padding_x + orientation_index(pre_move[3]) * tile_size, board_padding_y + orientation_index(7 - pre_move[2]) * tile_size, tile_size, tile_size, pre_move_color);
	}*/

	// Sélection de cases et de pièces
	if (selected_pos.first != -1) {
		// Affiche la case séléctionnée
		draw_rectangle(board_padding_x + orientation_index(selected_pos.second) * tile_size, board_padding_y + orientation_index(7 - selected_pos.first) * tile_size, tile_size, tile_size, select_color);
		// Affiche les coups possibles pour la pièce séléctionnée
		for (int i = 0; i < _got_moves; i++) {
			if (_moves[i].i1 == selected_pos.first && _moves[i].j1 == selected_pos.second) {
				draw_rectangle(board_padding_x + orientation_index(_moves[i].j2) * tile_size, board_padding_y + orientation_index(7 - _moves[i].i2) * tile_size, tile_size, tile_size, select_color);
			}
		}
	}

	// Pièces capturables
	int p;
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			p = _array[i][j];
			if (p > 0) {
				if (is_capturable(i, j)) {
					if (clicked && i == clicked_pos.first && j == clicked_pos.second)
						draw_texture(piece_textures[p - 1], mouse_pos.x - piece_size / 2, mouse_pos.y - piece_size / 2, WHITE);
					else
						draw_texture(piece_textures[p - 1], board_padding_x + tile_size * orientation_index(j) + (tile_size - piece_size) / 2, board_padding_y + tile_size * orientation_index(7 - i) + (tile_size - piece_size) / 2, WHITE);
				}
			}
		}
	}

	// Coups auquel l'IA réflechit...
	if (drawing_arrows)
		draw_monte_carlo_arrows();

	// Pièces non-capturables
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			p = _array[i][j];
			if (p > 0) {
				if (!is_capturable(i, j)) {
					if (clicked && i == clicked_pos.first && j == clicked_pos.second)
						draw_texture(piece_textures[p - 1], mouse_pos.x - piece_size / 2.0f, mouse_pos.y - piece_size / 2.0f, WHITE);
					else
						draw_texture(piece_textures[p - 1], board_padding_x + tile_size * static_cast<float>(orientation_index(j)) + (tile_size - piece_size) / 2.0f, board_padding_y + tile_size * static_cast<float>(orientation_index(7 - i)) + (tile_size - piece_size) / 2.0f, WHITE);
				}
			}
		}
	}

	// Flèches déssinées
	for (vector<int> arrow : arrows_array)
		draw_simple_arrow_from_coord(arrow[0], arrow[1], arrow[2], arrow[3], -1, arrow_color);

	// Titre
	DrawTextEx(text_font, "Grogros Chess", { board_padding_x + grogros_size / 2 + text_size / 2.8f, text_size / 4.0f }, text_size / 1.4f, font_spacing * text_size / 1.4f, text_color);

	// Grogros
	draw_texture(grogros_texture, board_padding_x, text_size / 4.0f - text_size / 5.6f, WHITE);

	// Joueurs de la partie
	int material = material_difference();
	string black_material = (material < 0) ? ("+" + to_string(-material)) : "";
	string white_material = (material > 0) ? ("+" + to_string(material)) : "";

	int t_size = text_size / 3.0f;

	int x_mini_piece = board_padding_x + t_size * 4;
	int y_mini_piece_black = board_padding_y - t_size + (board_size + 2 * t_size) * !board_orientation;
	int y_mini_piece_white = board_padding_y - t_size + (board_size + 2 * t_size) * board_orientation;

	// Noirs
	DrawCircle(x_mini_piece - t_size * 3, y_mini_piece_black, t_size * 0.6f, board_color_dark);
	DrawTextEx(text_font, main_GUI._black_player.c_str(), { static_cast<float>(x_mini_piece - t_size * 2), static_cast<float>(y_mini_piece_black - t_size) }, t_size, font_spacing * t_size, text_color);
	DrawTextEx(text_font, black_material.c_str(), { static_cast<float>(x_mini_piece - t_size * 2), static_cast<float>(y_mini_piece_black + t_size / 6) }, t_size, font_spacing * t_size, text_color_info);

	bool next = false;
	for (int i = 1; i < 6; i++) {
		for (int j = 0; j < missing_w_material[i]; j++) {
			DrawTexture(mini_piece_textures[i - 1], x_mini_piece, y_mini_piece_black, WHITE);
			x_mini_piece += mini_piece_size / 2;
			next = true;
		}
		if (next)
			x_mini_piece += mini_piece_size;
		next = false;
	}

	x_mini_piece = board_padding_x + t_size * 4;

	// Blancs
	DrawCircle(x_mini_piece - t_size * 3, y_mini_piece_white, t_size * 0.6f, board_color_light);
	DrawTextEx(text_font, main_GUI._white_player.c_str(), { static_cast<float>(x_mini_piece - t_size * 2), static_cast<float>(y_mini_piece_white - t_size) }, t_size, font_spacing * t_size, text_color);
	DrawTextEx(text_font, white_material.c_str(), { static_cast<float>(x_mini_piece - t_size * 2), static_cast<float>(y_mini_piece_white + t_size / 6) }, t_size, font_spacing * t_size, text_color_info);

	for (int i = 1; i < 6; i++) {
		for (int j = 0; j < missing_b_material[i]; j++) {
			DrawTexture(mini_piece_textures[i - 1 + 6], x_mini_piece, y_mini_piece_white, WHITE);
			x_mini_piece += mini_piece_size / 2;
			next = true;
		}
		if (next)
			x_mini_piece += mini_piece_size;
		next = false;
	}

	// Temps des joueurs
	// Update du temps
	main_GUI.update_time();
	float x_pad = board_padding_x + board_size - text_size * 2;
	Color time_colors[4] = { (main_GUI._time && !_player) ? BLACK : VDARKGRAY, (main_GUI._time && !_player) ? WHITE : LIGHTGRAY, (main_GUI._time && _player) ? WHITE : LIGHTGRAY, (main_GUI._time && _player) ? BLACK : VDARKGRAY };

	// Temps des blancs
	if (!main_GUI._white_time_text_box.active) {
		main_GUI._white_time_text_box.value = main_GUI._time_white;
		main_GUI._white_time_text_box.text = clock_to_string(main_GUI._white_time_text_box.value, false);
	}
	update_text_box(main_GUI._white_time_text_box);
	if (!main_GUI._white_time_text_box.active) {
		main_GUI._time_white = main_GUI._white_time_text_box.value;
		main_GUI._white_time_text_box.text = clock_to_string(main_GUI._white_time_text_box.value, false);
	}

	// Position du texte
	main_GUI._white_time_text_box.set_rect(x_pad, board_padding_y - text_size / 2 * !board_orientation + board_size * board_orientation, board_padding_x + board_size - x_pad, text_size / 2);
	main_GUI._white_time_text_box.text_size = text_size / 3;
	main_GUI._white_time_text_box.text_color = time_colors[3];
	main_GUI._white_time_text_box.text_font = text_font;
	main_GUI._white_time_text_box.main_color = time_colors[2];
	draw_text_box(main_GUI._white_time_text_box);

	// Temps des noirs
	if (!main_GUI._black_time_text_box.active) {
		main_GUI._black_time_text_box.value = main_GUI._time_black;
		main_GUI._black_time_text_box.text = clock_to_string(main_GUI._black_time_text_box.value, false);
	}
	update_text_box(main_GUI._black_time_text_box);
	if (!main_GUI._black_time_text_box.active) {
		main_GUI._time_black = main_GUI._black_time_text_box.value;
		main_GUI._black_time_text_box.text = clock_to_string(main_GUI._black_time_text_box.value, false);
	}

	// Position du texte
	main_GUI._black_time_text_box.set_rect(x_pad, board_padding_y - text_size / 2 * board_orientation + board_size * !board_orientation, board_padding_x + board_size - x_pad, text_size / 2);
	main_GUI._black_time_text_box.text_size = text_size / 3;
	main_GUI._black_time_text_box.text_color = time_colors[1];
	main_GUI._black_time_text_box.text_font = text_font;
	main_GUI._black_time_text_box.main_color = time_colors[0];
	draw_text_box(main_GUI._black_time_text_box);

	// FEN
	main_GUI._current_fen = to_fen();
	const char* fen = main_GUI._current_fen.c_str();
	DrawTextEx(text_font, fen, { text_size / 2, board_padding_y + board_size + text_size * 3 / 2 }, text_size / 3, font_spacing * text_size / 3, text_color_blue);

	// PGN
	main_GUI.update_global_pgn();
	slider_text(main_GUI._global_pgn, text_size / 2, board_padding_y + board_size + text_size * 2, main_GUI._screen_width - text_size, main_GUI._screen_height - (board_padding_y + board_size + text_size * 2) - text_size / 3, text_size / 3, &pgn_slider, text_color);

	// Analyse de Monte-Carlo
	string monte_carlo_text = static_cast<string>(main_GUI._grogros_analysis ? "stop GrogrosZero-Auto (CTRL-H)" : "run GrogrosZero-Auto (CTRL-G)") + "\n\nMonte-Carlo research parameters:\nbeta: " + to_string(main_GUI._beta) + " | k_add: " + to_string(main_GUI._k_add) + "\nquiescence depth: " + to_string(main_GUI._quiescence_depth) + " | explore checks: " + (main_GUI._explore_checks ? "true" : "false");
	if (_tested_moves && drawing_arrows && (_monte_called || true)) {
		// int best_eval = (_player) ? max_value(_eval_children, _tested_moves) : min_value(_eval_children, _tested_moves);
		int best_move = max_index(_nodes_children, _tested_moves);
		int best_eval = _eval_children[best_move];
		string eval;
		int mate = is_eval_mate(best_eval);
		if (mate != 0) {
			if (mate * get_color() > 0)
				eval = "";
			else
				eval = "-";
			eval += "M";
			eval += to_string(abs(mate));
		}

		else
			eval = to_string(best_eval);

		global_eval = best_eval;

		stringstream stream;
		stream << fixed << setprecision(2) << best_eval / 100.0f;
		global_eval_text = mate ? (best_eval > 0 ? "+" + eval : "-" + eval) : (best_eval > 0) ? "+" + stream.str() : stream.str();

		float win_chance = get_winning_chances_from_eval(best_eval, mate != 0, _player);
		if (!_player)
			win_chance = 1 - win_chance;
		string win_chances = "\nW/D/L: " + to_string(static_cast<int>(100 * win_chance)) + "/0/" + to_string(static_cast<int>(100 * (1 - win_chance))) + "\%\n";

		// Pour l'évaluation statique
		if (!_displayed_components)
			evaluate_int(_evaluator, true);
		int max_depth = grogros_main_depth();
		int n_nodes = total_nodes();
		monte_carlo_text += "\n\n--- static eval: " + ((_static_evaluation > 0) ? static_cast<string>("+") : static_cast<string>("")) + to_string(_static_evaluation) + " ---\n" + eval_components + "\n--- dynamic eval: " + ((best_eval > 0) ? static_cast<string>("+") : static_cast<string>("")) + eval + " ---" + win_chances + "\nnodes: " + int_to_round_string(n_nodes) + "/" + int_to_round_string(monte_buffer._length) + " | time: " + clock_to_string(_time_monte_carlo) + " | speed: " + int_to_round_string(1000 * total_nodes() / (_time_monte_carlo + 1)) + "N/s" + " | depth: " + to_string(max_depth) + "\nquiescence: " + int_to_round_string(_quiescence_nodes) + "N" + " | speed: " + int_to_round_string(1000 * _quiescence_nodes / (_time_monte_carlo + 1)) + "N/s";

		// Affichage des paramètres d'analyse de Monte-Carlo
		slider_text(monte_carlo_text, board_padding_x + board_size + text_size / 2, text_size, main_GUI._screen_width - text_size - board_padding_x - board_size, board_size * 9 / 16, text_size / 3, &monte_carlo_slider, text_color);

		// Lignes d'analyse de Monte-Carlo
		static string monte_carlo_variants;

		// Calcul des variantes
		if (_monte_called) {
			bool next_variant = false;
			monte_carlo_variants = "";
			vector<int> v(sort_by_nodes());
			for (int i : v) {
				if (next_variant)
					monte_carlo_variants += "\n\n";
				next_variant = true;
				mate = is_eval_mate(_eval_children[i]);
				string eval;
				if (mate != 0) {
					if (mate > 0)
						eval = "+";
					else
						eval = "-";
					eval += "M";
					eval += to_string(abs(mate));
				}
				else {
					eval = _eval_children[i] > 0 ? "+" + to_string(_eval_children[i]) : to_string(_eval_children[i]);
				}

				string variant_i = monte_buffer._heap_boards[_index_children[i]].get_monte_carlo_variant(true); // Peut être plus rapide
				// Ici aussi y'a qq chose qui ralentit, mais quoi?...
				monte_carlo_variants += "eval: " + eval + " | " + to_string(_moves_count) + (_player ? ". " : "... ") + move_label_from_index(i) + variant_i + " | (" + int_to_round_string(_nodes_children[i]) + "N - " + to_string(100.0 * _nodes_children[i] / n_nodes).substr(0, 5) + "%)";
			}
			_monte_called = false;
		}

		// Affichage des variantes
		slider_text(monte_carlo_variants, board_padding_x + board_size + text_size / 2, board_padding_y + board_size * 9 / 16, main_GUI._screen_width - text_size - board_padding_x - board_size, board_size / 2, text_size / 3, &variants_slider);

		// Affichage de la barre d'évaluation
		draw_eval_bar(global_eval, global_eval_text, board_padding_x / 6, board_padding_y, 2 * board_padding_x / 3, board_size);
	}

	// Affichage des contrôles et autres informations
	else {
		// Touches
		static string keys_information = "CTRL-G: Start GrogrosZero analysis\nCTRL-H: Stop GrogrosZero analysis\n\n";

		// Binding chess.com
		static string binding_information;
		binding_information = "Binding chess.com:\n- Auto-click: " + (main_GUI._click_bind ? static_cast<string>("enabled") : static_cast<string>("disabled")) + "\n- Binding mode: " + (main_GUI._binding_full ? static_cast<string>("analysis") : main_GUI._binding_solo ? static_cast<string>("play") : "none");

		// Texte total
		static string controls_information;
		controls_information = "Controls:\n\n" + keys_information + binding_information;

		// TODO : ajout d'une valeur de slider
		slider_text(controls_information, board_padding_x + board_size + text_size / 2, board_padding_y, main_GUI._screen_width - text_size - board_padding_x - board_size, board_size, text_size / 3, 0, text_color_info);
	}

	// Affichage du curseur
	draw_texture(cursor_texture, mouse_pos.x - cursor_size / 2, mouse_pos.y - cursor_size / 2, WHITE);

	return true;
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

	const int mate = b.is_mate();

	if (mate == 0)
		return PlaySound(stealmate_sound);
	if (mate == 1)
		return PlaySound(checkmate_sound);

	if (b.in_check()) {
		if (_player)
			PlaySound(check_1_sound);
		else
			PlaySound(check_2_sound);
	}

	// Si pas d'échecs
	else {
		// Promotions
		if ((p1 == 1 || p1 == 7) && (k == 0 || k == 7))
			return PlaySound(promotion_sound);

		// Prises (ou en passant)
		if (p2 != 0 || ((p1 == 1 || p1 == 7) && j != l)) {
			if (_player)
				return PlaySound(capture_1_sound);
			else
				return PlaySound(capture_2_sound);
		}

		// Roques
		if (p1 == 6 && abs(j - l) == 2)
			return PlaySound(castle_1_sound);
		if (p1 == 12 && abs(j - l) == 2)
			return PlaySound(castle_2_sound);

		// Coup "normal"
		if (_player)
			return PlaySound(move_1_sound);
		if (!_player)
			return PlaySound(move_2_sound);
	}

	// Mats à rajouter

	return;
}

// Fonction qui joue le son d'un coup à partir de son index
void Board::play_index_move_sound(const int i) const
{
	play_move_sound(_moves[i]);
}

// Fonction qui obtient la case correspondante à la position sur la GUI
pair<int, int> get_pos_from_GUI(const float x, const float y) {
	if (!is_in(x, board_padding_x, board_padding_x + board_size) || !is_in(y, board_padding_y, board_padding_y + board_size))
		return { -1, -1 };
	else
		return { orientation_index(8 - (y - board_padding_y) / tile_size), orientation_index((x - board_padding_x) / tile_size) };
}

// Fonction qui permet de changer l'orientation du plateau
void switch_orientation() {
	board_orientation = !board_orientation;
}

// Fonction aidant à l'affichage du plateau (renvoie i si board_orientation, et 7 - i sinon)
int orientation_index(const int i) {
	if (board_orientation)
		return i;
	return 7 - i;
}

// A partir de coordonnées sur le plateau
void draw_arrow_from_coord(int i1, int j1, int i2, int j2, int index, const int color, float thickness, Color c, const bool use_value, int value, const int mate, const bool outline) {
	if (thickness == -1.0f)
		thickness = arrow_thickness;

	const float x1 = board_padding_x + tile_size * orientation_index(j1) + tile_size / 2;
	const float y1 = board_padding_y + tile_size * orientation_index(7 - i1) + tile_size / 2;
	const float x2 = board_padding_x + tile_size * orientation_index(j2) + tile_size / 2;
	const float y2 = board_padding_y + tile_size * orientation_index(7 - i2) + tile_size / 2;

	// Transparence nulle
	c.a = 255;

	// Outline pour le coup choisi
	if (outline) {
		if (abs(j2 - j1) != abs(i2 - i1))
			draw_line_bezier(x1, y1, x2, y2, thickness * 1.4f, BLACK);
		else
			draw_line_ex(x1, y1, x2, y2, thickness * 1.4f, BLACK);
		draw_circle(x1, y1, thickness * 1.2f, BLACK);
		draw_circle(x2, y2, thickness * 2.0f * 1.1f, BLACK);
	}

	// "Flèche"
	if (abs(j2 - j1) != abs(i2 - i1))
		draw_line_bezier(x1, y1, x2, y2, thickness, c);
	else
		draw_line_ex(x1, y1, x2, y2, thickness, c);
	draw_circle(x1, y1, thickness, c);
	draw_circle(x2, y2, thickness * 2.0f, c);

	if (use_value) {
		char v[4];
		string eval;
		if (mate != 0) {
			if (mate * color > 0)
				eval = "+";
			else
				eval = "-";
			eval += "M";
			eval += to_string(abs(mate));
#pragma warning(suppress : 4996)
			sprintf(v, eval.c_str());
		}
		else {
#pragma warning(suppress : 4996)
			if (main_GUI._display_win_chances)
				value = float_to_int(100.0f * get_winning_chances_from_eval(value, false, main_GUI._board._player));
			sprintf_s(v, "%d", value);
		}

		float size = thickness * 1.5f;
		const float max_size = thickness * 3.25f;
		float width = MeasureTextEx(text_font, v, size, font_spacing * size).x;
		if (width > max_size) {
			size = size * max_size / width;
			width = MeasureTextEx(text_font, v, size, font_spacing * size).x;
		}
		const float height = MeasureTextEx(text_font, v, size, font_spacing * size).y;

		Color t_c = ColorAlpha(BLACK, static_cast<float>(c.a) / 255.0f);
		DrawTextEx(text_font, v, { x2 - width / 2.0f, y2 - height / 2.0f }, size, font_spacing * size, BLACK);
	}

	// Ajoute la flèche au vecteur
	grogros_arrows.push_back({ i1, j1, i2, j2, index });

	return;
}

// Fonction qui dessine les flèches en fonction des valeurs dans l'algo de Monte-Carlo d'un plateau
void Board::draw_monte_carlo_arrows() const
{
	// get_moves(false, true);

	grogros_arrows = {};

	const int best_move = best_monte_carlo_move();

	int sum_nodes = 0;
	for (int i = 0; i < _tested_moves; i++)
		sum_nodes += _nodes_children[i];

	// Une pièce est-elle sélectionnée?
	const bool is_selected = selected_pos.first != -1 && selected_pos.second != -1;

	// Crée un vecteur avec les coups visibles
	vector<int> moves_vector;
	for (int i = 0; i < _tested_moves; i++) {
		if (is_selected) {
			// Dessine pour la pièce sélectionnée
			if (selected_pos.first == _moves[i].i1 && selected_pos.second == _moves[i].j1)
				moves_vector.push_back(i);
		}

		else {
			if (_nodes_children[i] / static_cast<float>(sum_nodes) > arrow_rate)
				moves_vector.push_back(i);
		}
	}

	// BUG : invalid comparator ? (in debug mode)
	sort(moves_vector.begin(), moves_vector.end(), compare_move_arrows);

	for (const int i : moves_vector) {
		const int mate = is_eval_mate(_eval_children[i]);
		draw_arrow_from_coord(_moves[i].i1, _moves[i].j1, _moves[i].i2, _moves[i].j2, i, get_color(), -1.0, move_color(_nodes_children[i], sum_nodes), true, _eval_children[i], mate, i == best_move);
	}
}

// Fonction qui calcule et renvoie la mobilité des pièces
int Board::get_piece_mobility(const bool legal) const
{

	// TODO : fonction LARGEMENT optimisable

	Board b;
	b.copy_data(*this);
	int piece_mobility = 0;

	// Pour chaque pièce (sauf le roi)
	static constexpr int mobility_values_pawn[3] = { -200, 0, 100 };
	static constexpr int mobility_values_knight[9] = { -500, -200, 0, 100, 200, 300, 400, 450, 500 };
	static constexpr int mobility_values_bishop[15] = { -600, -300, -50, 100, 210, 280, 330, 475, 415, 450, 480, 505, 525, 540, 550 };
	static constexpr int mobility_values_rook[15] = { -750, -100, 100, 150, 190, 235, 275, 300, 325, 345, 365, 385, 390, 400, 405 };
	static constexpr int mobility_values_queen[29] = { -400, -100, 100, 150, 190, 235, 275, 300, 325, 345, 365, 385, 390, 400, 410, 420, 430, 440, 450, 460, 470, 480, 490, 495, 500, 505, 510 };

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
	b.get_moves(false, legal);

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
	b.get_moves(false, legal);

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

	return piece_mobility;
}

// Couleur de la flèche en fonction du coup (de son nombre de noeuds)
Color move_color(const int nodes, const int total_nodes) {
	const float x = static_cast<float>(nodes) / static_cast<float>(total_nodes);

	const auto red = static_cast<unsigned char>(255.0f * ((x <= 0.2f) + (x > 0.2f && x < 0.4f) * (0.4f - x) / 0.2f + (x > 0.8f) * (x - 0.8f) / 0.2f));
	const auto green = static_cast<unsigned char>(255.0f * ((x < 0.2f) * x / 0.2f + (x >= 0.2f && x <= 0.6f) + (x > 0.6f && x < 0.8f) * (0.8f - x) / 0.2f));
	const auto blue = static_cast<unsigned char>(255.0f * ((x > 0.4f && x < 0.6f) * (x - 0.4f) / 0.2f + (x >= 0.6f)));

	//unsigned char alpha = 100 + 155 * nodes / total_nodes;

	return { red, green, blue, 255 };
}

// Fonction qui renvoie le meilleur coup selon l'analyse faite par l'algo de Monte-Carlo
int Board::best_monte_carlo_move() const
{
	return max_index(_nodes_children, _tested_moves, _eval_children, get_color());
}

// Fonction qui joue le coup après analyse par l'algo de Monte Carlo, et qui garde en mémoire les infos du nouveau plateau
bool Board::play_monte_carlo_move_keep(const int m, const bool keep, const bool keep_display, const bool display, const bool add_to_list)
{
	// Obtient les coups si nécessaire
	if (_got_moves == -1)
		get_moves(false, true);

	// Il faut obtenir le vrai coup (correspondant aux plateaux fils de l'algo de Monte-Carlo)
	// Pour le moment c'est pas beau, il faudra changer ça à l'avenir
	// TODO
	const Move wanted_move = _moves[m];

	// TODO c'est quoi ça déjà??
	Move child_move;
	int move = m;
	for (int i = 0; i < _tested_moves; i++) {
		uint_fast8_t last_child_move[4];
		copy(monte_buffer._heap_boards[_index_children[i]]._last_move, monte_buffer._heap_boards[_index_children[i]]._last_move + 4, last_child_move);
		child_move.i1 = last_child_move[0];
		child_move.j1 = last_child_move[1];
		child_move.i2 = last_child_move[2];
		child_move.j2 = last_child_move[3];
		if (child_move == wanted_move) {
			move = i;
			break;
		}
	}

	// Si le coup a été calculé par l'algo de Monte-Carlo
	if (move < _tested_moves) {
		if (keep_display) {
			play_index_move_sound(m);
			Board b(*this);
			b.make_index_move(m, true, add_to_list);
		}

		// Deletes all the children from the other boards
		for (int i = 0; i < _tested_moves; i++)
			if (i != move) {
				if (_is_active)
					monte_buffer._heap_boards[_index_children[i]].reset_all();
			}

		if (_is_active) {
			const Board* b = &monte_buffer._heap_boards[_index_children[move]];
			reset_board();
			*this = *b;
			main_GUI.update_time();
		}

		if (!keep)
			reset_all();
	}

	// Sinon, joue simplement le coup
	else {
		if (_got_moves == -1)
			get_moves(false, true);

		if (m < _got_moves) {
			if (_is_active)
				reset_all();

			make_index_move(m, true, add_to_list);
		}
		else {
			cout << "illegal move" << endl;
			return false;
		}
	}

	return true;
}

// Pas très opti pour l'affichage, mais bon... Fonction qui cherche la profondeur la plus grande dans la recherche de Monté-Carlo
int Board::max_monte_carlo_depth() const
{
	int max_depth = 0;
	for (int i = 0; i < _tested_moves; i++) {
		const int depth = monte_buffer._heap_boards[_index_children[i]].max_monte_carlo_depth() + 1;
		if (depth > max_depth)
			max_depth = depth;
	}

	return max_depth;
}

// Constructeur par défaut
Buffer::Buffer() {
	// Crée un gros buffer, de 4GB
	constexpr unsigned long int _size_buffer = 4000000000;
	_length = _size_buffer / sizeof(Board);
	_length = 0;

	_heap_boards = new Board[_length];
}

// Constructeur utilisant la taille max (en bits) du buffer
Buffer::Buffer(const unsigned long int size) {
	_length = size / sizeof(Board);
	_heap_boards = new Board[_length];
}

// Initialize l'allocation de n plateaux
void Buffer::init(const int length) {
	if (_init)
		cout << "already initialized" << endl;
	else {
		cout << "initializing buffer..." << endl;
		_length = length;
		_heap_boards = new Board[_length];
		_init = true;
		cout << "buffer initialized :" << endl;
		cout << "board size : " << int_to_round_string(sizeof(Board)) << "b" << endl;
		cout << "length : " << int_to_round_string(_length) << endl;
		cout << "approximate buffer size : " << long_int_to_round_string(monte_buffer._length * sizeof(Board)) << "b" << endl;
	}
}

// Fonction qui donne l'index du premier plateau de libre dans le buffer
int Buffer::get_first_free_index() {
	for (int i = 0; i < _length; i++) {
		_iterator++;
		if (_iterator >= _length)
			_iterator -= _length;
		if (!_heap_boards[_iterator]._is_active)
			return _iterator;
	}

	return -1;
}

// Fonction qui désalloue toute la mémoire
void Buffer::remove() {
	delete[] _heap_boards;
	_init = false;
}

// Buffer pour l'algo de Monte-Carlo
Buffer monte_buffer;

// Algo de grogros_zero
void Board::grogros_zero(Evaluator* eval, int nodes, const float beta, const float k_add, const int quiescence_depth, const bool explore_checks, const bool display, const int depth, Network* net)
{
	// Pour la GUI
	_monte_called = true;

	// Pour le buffer
	_is_active = true;

	// Temps au début de l'appel de GrogrosZero
	const clock_t begin_monte_time = clock();

	// Si c'est le premier appel, sur le plateau principal
	if (_new_board && depth == 0) {
		evaluate_int(eval, false, net);
	}

	// Si c'est le plateau principal
	if (depth == 0) {
		const int n = total_nodes();
		if (monte_buffer._length - n < nodes) {
			if (display)
				cout << "buffer is full" << endl;
			nodes = monte_buffer._length - n;
		}
	}

	// Vérifie si la partie est finie
	if (_new_board)
		is_game_over();

	// Si la partie est finie, évite le calcul des coups... bizarre aussi : ne plus rentrer dans cette ligne?
	if (_game_over_value) {
		_nodes++; // un peu bizarre mais bon... revoir les cas où y'a des mats
		_time_monte_carlo += clock() - begin_monte_time;
		return;
	}

	// Trie les coups si ça n'est pas déjà fait
	!_quick_sorted_moves && quick_moves_sort();

	// Reset les tableaux pour les plateaux fils
	if (_new_board) {
		if (_eval_children != nullptr) {
			delete[] _eval_children;
			_eval_children = nullptr;
		}

		if (_nodes_children != nullptr) {
			delete[] _nodes_children;
			_nodes_children = nullptr;
		}

		if (_index_children != nullptr) {
			delete[] _index_children;
			_index_children = nullptr;
		}

		_eval_children = new int[_got_moves]();
		_nodes_children = new int[_got_moves]();
		_index_children = new int[_got_moves](); // à changer? cela prend du temps?


		_tested_moves = 0;
		_current_move = 0;
		_new_board = false;
	}


	// *** BOUCLE PRINCIPALE ***
	while (nodes > 0) {

		// *** TOUS LES COUPS NE SONT PAS ENCORE EXPLORÉS ***
		if (_tested_moves < _got_moves) {

			// Prend une nouvelle place dans le buffer
			const int index = monte_buffer.get_first_free_index();
			if (index == -1) {
				cout << "buffer is full" << endl;
				return;
			}

			// Stocke l'index du plateau dans le buffer pour ce coup
			_index_children[_current_move] = index;

			// Rend actif le plateau fils
			monte_buffer._heap_boards[index]._is_active = true;

			// Joue un nouveau coup
			monte_buffer._heap_boards[index].copy_data(*this);
			monte_buffer._heap_boards[index].make_index_move(_current_move);

			// Evalue une première fois la position, puis stocke dans la liste d'évaluation des coups
			monte_buffer._heap_boards[index]._evaluation = monte_buffer._heap_boards[index].quiescence(eval, -INT32_MAX, INT32_MAX, quiescence_depth, explore_checks) * -get_color();
			_quiescence_nodes += monte_buffer._heap_boards[index]._quiescence_nodes;

			_eval_children[_current_move] = monte_buffer._heap_boards[index]._evaluation;
			_nodes_children[_current_move] = 1;

			// Actualise la valeur d'évaluation du plateau
			_evaluation = (_player && _eval_children[_current_move] > _evaluation) ? _eval_children[_current_move] : (!_player && _eval_children[_current_move] < _evaluation) ? _eval_children[_current_move] : _evaluation;

			// Incrémentation des coups
			_current_move++;
			_tested_moves++;
		}

		// *** TOUS LES COUPS SONT DÉJÀ EXPLORÉS ***
		else {
			// Choisit aléatoirement un "bon" coup
			_current_move = pick_random_good_move(_eval_children, _got_moves, get_color(), false, _nodes, _nodes_children, beta, k_add);
			//_current_move = select_uct();

			// Va une profondeur plus loin... appel récursif sur Monte-Carlo
			_quiescence_nodes -= monte_buffer._heap_boards[_index_children[_current_move]]._quiescence_nodes;
			monte_buffer._heap_boards[_index_children[_current_move]].grogros_zero(eval, 1, beta, k_add, quiescence_depth, explore_checks, display, depth + 1, net);
			_quiescence_nodes += monte_buffer._heap_boards[_index_children[_current_move]]._quiescence_nodes;

			// Actualise l'évaluation
			_eval_children[_current_move] = monte_buffer._heap_boards[_index_children[_current_move]]._evaluation;
			_nodes_children[_current_move] = monte_buffer._heap_boards[_index_children[_current_move]]._nodes + 1;

			// Actualise la valeur d'évaluation du plateau
			_evaluation = _player ? max_value(_eval_children, _got_moves) : min_value(_eval_children, _got_moves);
		}

		// Décrémentation du nombre de noeuds restants
		nodes--;
		_nodes++;
	}

	_time_monte_carlo += clock() - begin_monte_time;

	return;
}

// Fonction qui réinitialise le plateau dans son état de base (pour le buffer)
// FIXME? plus rapide d'instancier un nouveau plateau? et plus safe niveau mémoire?
void Board::reset_board(const bool display) {
	_got_moves = -1;
	_is_active = false;
	_current_move = 0;
	_evaluated = false;
	_mate_checked = false;
	_game_over_checked = false;
	_monte_called = true;
	_is_game_over = false;
	_time_monte_carlo = 0;
	_static_evaluation = 0;
	_evaluation = 0;
	_quick_sorted_moves = false;
	_sorted_moves = false;
	_nodes = 0;
	_quiescence_nodes = 0;

	if (!_new_board) {
		_tested_moves = 0;
		if (_eval_children != nullptr) {
			delete[] _eval_children;
			_eval_children = nullptr;
		}

		if (_nodes_children != nullptr) {
			delete[] _nodes_children;
			_nodes_children = nullptr;
		}

		if (_index_children != nullptr) {
			delete[] _index_children;
			_index_children = nullptr;
		}
		_new_board = true;
	}

	if (display)
		cout << "board reset done" << endl;

	return;
}

// Fonction qui réinitialise tous les plateaux fils dans le buffer
void Board::reset_all(bool self, bool display) {
	for (int i = 0; i < _tested_moves; i++)
		monte_buffer._heap_boards[_index_children[i]].reset_all(false);

	reset_board();
}

// Fonction qui renvoie le nombre de noeuds calculés par GrogrosZero ou Monte-Carlo
int Board::total_nodes() const
{
	int nodes = 0;

	for (int i = 0; i < _tested_moves; i++)
		nodes += _nodes_children[i];

	return nodes;
}

// Fonction qui calcule et renvoie la valeur correspondante à la sécurité des rois
int Board::get_king_safety() {
	// TODO : à rajouter pour préciser l'évaluation
	// - escape squares (à compter?)
	// https://www.chessprogramming.org/King_Safety
	// - pawn storm : -> rajout de faiblesse pour le roi adverse



	// *** CALCUL ***
	// ap = puissance d'attaque (pièces proches du roi adverse)
	// dp = puissance de défense (pièces proches du roi allié)
	// pp = puissance de protection (pions proches du roi allié + au bord de l'échiquier + droits de roques)
	// w = faiblesse du roi (positionnement sur l'échiquier -> centre : roi plus faible + virtual mobility)
	// cp = puissance de contrôle (cases contrôlées autour du roi adverse)

	// kd = danger du roi


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
	// r1b1k2r/p1p2ppp/2p5/8/5P1q/3B1R1P/PBP3P1/Q5K1 w kq - 3 17 : le roi noir est le plus faible
	// 1r4k1/p2n1pp1/2p1b2p/3p3P/4pQ2/2q1P3/P1P1BPP1/2KR3R w - - 1 23 : c'est mat pour les noirs
	// rnbr2k1/ppq2p2/2pb1npQ/6N1/7R/3B2P1/PPP2P1P/2KR4 b - - 2 17 : mat pour les blancs
	// 3rk2r/ppp2ppq/2p1b3/2P5/4P1P1/2P3P1/PPQ1B3/RNB2RK1 w k - 1 7 : quasi égal
	// 2k2r2/ppp3pp/1bp1b3/8/4Pp1q/1N1B1Pn1/PP3RPP/R2QB1K1 w - - 8 6 : roi blanc pas très safe
	// 8/p7/r3pk2/8/1P2Kp2/P1R2P2/5P2/8 b - - 3 39 : roi blanc pas en danger
	// 2rk3q/1pp5/p4n2/1P1p1bp1/2PQ1b2/N2p4/P2P2PP/R1B1R2K w - - 0 23 : roi blanc foutu
	// r1b1k2r/pppp2pp/2n5/4Pp2/8/BB3N2/P1PQ2PP/5K2 b kq - 0 15 : le roi est pas bien en fait
	// r1bq1b1r/pp4pp/2p1k3/3np3/1nBP4/2N2Q2/PPP2PPP/R1B2RK1 b - - 0 10 : +2.5 / +5 pour king safety

	// 8/6PK/5k2/8/8/8/8/8 b - - 0 8


	constexpr bool display = false;

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
	int w_defending_power = 75;
	int b_defending_power = 75;

	// Facteurs multiplicatifs
	constexpr float piece_attack_factor = 1.0f;
	constexpr float piece_defense_factor = 1.25f;
	constexpr float pawn_protection_factor = 1.0f;


	// -------------------------------------
	// *** CALCUL DES PUISSANCES ***
	// * ATTAQUES - DEFENSES - PROTECTIONS *
	// -------------------------------------

	// Calcul des protections et puissances d'attaques
	for (uint_fast8_t i = 0; i < 8; i++)
		for (uint_fast8_t j = 0; j < 8; j++) {
			if (const uint_fast8_t p = _array[i][j]; p > 0) {
				if (p < 6) {
					w_attacking_power += static_cast<int>(piece_attack_factor * static_cast<float>(get_piece_attack_power(i, j)));
					if (p == 1)
						w_king_protection += static_cast<int>(pawn_protection_factor * static_cast<float>(get_piece_defense_power(i, j)));
					else
						w_defending_power += static_cast<int>(piece_defense_factor * static_cast<float>(get_piece_defense_power(i, j)));
				}
				else if (p > 6 && p < 12) {
					b_attacking_power += static_cast<int>(piece_attack_factor * static_cast<float>(get_piece_attack_power(i, j)));
					if (p == 7)
						b_king_protection += static_cast<int>(pawn_protection_factor * static_cast<float>(get_piece_defense_power(i, j)));
					else
						b_defending_power += static_cast<int>(piece_defense_factor * static_cast<float>(get_piece_defense_power(i, j)));
				}
			}
		}


	if (display) {
		cout << "base_w_attacking_power = " << w_attacking_power << "		-	base_b_attacking_power = " << b_attacking_power << endl;
		cout << "base_w_defending_power = " << w_defending_power << "		-	 base_b_defending_power = " << b_defending_power << endl;
		cout << "base_w_king_protection = " << w_king_protection << "		-	 base_b_king_protection = " << b_king_protection << endl;
	}


	// -------------------
	// *** MATING NETS ***
	// -------------------

	// Récupère les maps de contrôle des cases
	Map white_controls_map = get_white_controls_map();
	Map black_controls_map = get_black_controls_map();

	if (display) {
		//white_controls_map.print();
		//black_controls_map.print();
	}

	// Danger par cases controllées ou occupée en fonction de la distance (sera rajouté à la puissance d'attaque)
	constexpr uint_fast8_t controls_distance_dangers[8] = { 50, 25, 5, 1, 0, 0, 0, 0 };
	constexpr uint_fast8_t edge_danger = 25;

	// Roi blanc
	int white_king_mating_net = 0;

	// Bords : ajoute +3 s'il touche 1 bord, +5 s'il est dans un coin (nombre de cases perdues)
	const bool white_vertical_edge = (_white_king_pos.i == 0 || _white_king_pos.i == 7);
	const bool white_horizontal_edge = (_white_king_pos.j == 0 || _white_king_pos.j == 7);
	white_king_mating_net += (white_vertical_edge && white_horizontal_edge) ? 5 * edge_danger : (white_vertical_edge || white_horizontal_edge) ? 3 * edge_danger : 0;

	// Cases controllées ou occupées proches du roi blanc
	for (uint_fast8_t i = 0; i < 8; i++)
		for (uint_fast8_t j = 0; j < 8; j++)
			white_king_mating_net += (black_controls_map._array[i][j] > 0 || is_in(_array[i][j], 1, 6)) * controls_distance_dangers[max(abs(i - _white_king_pos.i), abs(j - _white_king_pos.j))];


	// Roi noir
	int black_king_mating_net = 0;

	// Bord : ajoute +3 s'il touche 1 bord, +5 s'il est dans un coin (nombre de cases perdues)
	const bool black_vertical_edge = (_black_king_pos.i == 0 || _black_king_pos.i == 7);
	const bool black_horizontal_edge = (_black_king_pos.j == 0 || _black_king_pos.j == 7);
	black_king_mating_net += (black_vertical_edge && black_horizontal_edge) ? 5 * edge_danger : (black_vertical_edge || black_horizontal_edge) ? 3 * edge_danger : 0;

	// Cases controllées ou occupées proches du roi noir
	for (uint_fast8_t i = 0; i < 8; i++)
		for (uint_fast8_t j = 0; j < 8; j++)
			black_king_mating_net += (white_controls_map._array[i][j] > 0 || is_in(_array[i][j], 7, 12)) * controls_distance_dangers[max(abs(i - _black_king_pos.i), abs(j - _black_king_pos.j))];


	if (display)
		cout << "white_king_mating_net = " << white_king_mating_net << "		-	 black_king_mating_net = " << black_king_mating_net << endl;

	// Ajout des mating nets aux puissances d'attaque
	//w_attacking_power += black_king_mating_net;
	//b_attacking_power += white_king_mating_net;
	w_attacking_power *= 1 + static_cast<float>(black_king_mating_net) / 100;
	b_attacking_power *= 1 + static_cast<float>(white_king_mating_net) / 100;


	// -----------------
	// *** OVERLOADS ***
	// -----------------

	// Résultante des contrôles
	Map controls_map = white_controls_map - black_controls_map;

	// Danger des surcharges (cases controlées en supériorité par l'allié), proche du roi adverse
	constexpr uint_fast8_t overloard_distance_dangers[8] = { 100, 75, 15, 5, 2, 0, 0, 0 };


	// Overload sur le roi blanc
	int white_king_overloaded = 0;

	// Cases controllées proches du roi
	for (uint_fast8_t i = 0; i < 8; i++)
		for (uint_fast8_t j = 0; j < 8; j++)
			if (controls_map._array[i][j] < 0)
			{
				const uint_fast8_t distance = max(abs(i - _white_king_pos.i), abs(j - _white_king_pos.j));
				white_king_overloaded -= overloard_distance_dangers[distance] * controls_map._array[i][j]; // - car valeur négative
			}


	// Overload sur le roi blanc
	int black_king_overloaded = 0;

	// Attaques sur le plateau
	for (uint_fast8_t i = 0; i < 8; i++)
		for (uint_fast8_t j = 0; j < 8; j++)
			if (controls_map._array[i][j] > 0)
			{
				const uint_fast8_t distance = max(abs(i - _black_king_pos.i), abs(j - _black_king_pos.j));
				black_king_overloaded += overloard_distance_dangers[distance] * controls_map._array[i][j];
			}


	if (display)
		cout << "white_king_overloaded = " << white_king_overloaded << "		-	 black_king_overloaded = " << black_king_overloaded << endl;

	// Les surcharges peuvent provoquer des faiblesses sur le roi adverse
	w_king_weakness += white_king_overloaded;
	b_king_weakness += black_king_overloaded;


	// -------------------
	// *** PROTECTIONS ***
	// -------------------

	// Protection des bords de l'échiquier
	constexpr int edge_protection = 0;
	w_king_protection += (_white_king_pos.i % 7 == 0 + _white_king_pos.j % 7 == 0) * edge_protection;
	b_king_protection += (_black_king_pos.i % 7 == 0 + _black_king_pos.j % 7 == 0) * edge_protection;

	// Il faut compter les cases vides (non-pion) autour de lui
	// Droits de roque
	constexpr int castling_rights_protection = 200;
	w_king_protection += (_castling_rights.k_w + _castling_rights.q_w) * castling_rights_protection;
	b_king_protection += (_castling_rights.k_b + _castling_rights.q_b) * castling_rights_protection;

	// Niveau de protection auquel on peut considérer que le roi est safe
	const int king_base_protection = 400 * (1 - _adv);
	w_king_protection -= king_base_protection;
	b_king_protection -= king_base_protection;


	// -----------------------
	// *** POSITION DU ROI ***
	// -----------------------

	// Proximité avec le bord
	// Avancement à partir duquel il est plus dangereux d'être sur un bord
	constexpr float edge_adv = 0.75f;
	constexpr float mult_endgame = 1.0f;

	// Version additive, adaptée pour l'endgame
	constexpr int edge_defense = 50;
	constexpr int endgame_safe_zone = 25; // Si le "i * j" du roi en endgame est supérieur, alors il n'est pas en danger : s'il est en c4 (2, 3 -> (2 + 1) * (3 + 1) = 12 < 16 -> danger)
	w_king_weakness += max_int(edge_defense, edge_defense * (edge_adv - _adv) * ((_adv < edge_adv) ? (min(_white_king_pos.i, 7 - _white_king_pos.i) + min(_white_king_pos.j, 7 - _white_king_pos.j)) : (endgame_safe_zone - ((min(_white_king_pos.i, 7 - _white_king_pos.i) + 1) * (min(_white_king_pos.j, 7 - _white_king_pos.j) + 1))) * mult_endgame / (edge_adv - 1))) - edge_defense;
	b_king_weakness += max_int(edge_defense, edge_defense * (edge_adv - _adv) * ((_adv < edge_adv) ? (min(_black_king_pos.i, 7 - _black_king_pos.i) + min(_black_king_pos.j, 7 - _black_king_pos.j)) : (endgame_safe_zone - ((min(_black_king_pos.i, 7 - _black_king_pos.i) + 1) * (min(_black_king_pos.j, 7 - _black_king_pos.j) + 1))) * mult_endgame / (edge_adv - 1))) - edge_defense;
	
	if (display) {
		cout << "White king placement weakness : " << max_int(edge_defense, edge_defense * (edge_adv - _adv) * ((_adv < edge_adv) ? (min(_white_king_pos.i, 7 - _white_king_pos.i) + min(_white_king_pos.j, 7 - _white_king_pos.j)) : (endgame_safe_zone - ((min(_white_king_pos.i, 7 - _white_king_pos.i) + 1) * (min(_white_king_pos.j, 7 - _white_king_pos.j) + 1))) * mult_endgame / (edge_adv - 1))) - edge_defense;
		cout << "	-	 Black king placement weakness : " << max_int(edge_defense, edge_defense * (edge_adv - _adv) * ((_adv < edge_adv) ? (min(_black_king_pos.i, 7 - _black_king_pos.i) + min(_black_king_pos.j, 7 - _black_king_pos.j)) : (endgame_safe_zone - ((min(_black_king_pos.i, 7 - _black_king_pos.i) + 1) * (min(_black_king_pos.j, 7 - _black_king_pos.j) + 1))) * mult_endgame / (edge_adv - 1))) - edge_defense << endl;
	}


	// Mobilité virtuelle du roi
	constexpr int virtual_mobility_danger = 75;
	w_king_weakness += virtual_mobility_danger * get_king_virtual_mobility(true) * (1 - _adv);
	b_king_weakness += virtual_mobility_danger * get_king_virtual_mobility(false) * (1 - _adv);


	// Affiche faiblesse, protection, attaque et defense des deux côtés
	if (display)
	{
		cout << "White king weakness : " << w_king_weakness << "		-	 Black king weakness : " << b_king_weakness << endl;
		cout << "White king protection : " << w_king_protection << "		-	 Black king protection : " << b_king_protection << endl;
		cout << "White attacking power : " << w_attacking_power << "		-	 Black attacking power : " << b_attacking_power << endl;
		cout << "White defending power : " << w_defending_power << "		-	 Black defending power : " << b_defending_power << endl;
	}

	// Ajout de la protection du roi... la faiblesse du roi ne peut pas être négative (potentiellement à revoir, mais parfois la surprotection donne des valeurs délirantes)
	w_king_weakness = max_int(0, w_king_weakness - w_king_protection);
	b_king_weakness = max_int(0, b_king_weakness - b_king_protection);

	// Calcul de la resultante de la puissance d'attaque
	w_attacking_power = max_int(0, w_attacking_power - b_defending_power);
	b_attacking_power = max_int(0, b_attacking_power - w_defending_power);


	// -------------------
	// *** SAFE CHECKS ***
	// -------------------

	// TODO : à fix + ajouter en fonction du nombre d'escape squares du roi adverse
	constexpr int safe_check_weakness = 350;
	constexpr float safe_check_attack = 2.0f;
	constexpr int safe_check_add = 1000;

	pair<uint_fast8_t, uint_fast8_t> safe_checks = get_safe_checks(white_controls_map, black_controls_map);

	if (display)
		cout << "White safe checks : " << static_cast<int>(safe_checks.first) << "			-	 Black safe checks : " << static_cast<int>(safe_checks.second) << endl;

	w_king_weakness += safe_checks.second * safe_check_weakness;
	b_king_weakness += safe_checks.first * safe_check_weakness;

	w_attacking_power += safe_checks.first * safe_check_attack;
	b_attacking_power += safe_checks.second * safe_check_attack;


	// ---------------------------
	// *** POTENTIEL D'ATTAQUE ***
	// ---------------------------

	// Potentiel d'attaque de chaque pièce (pion, caval, fou, tour, dame)
	static constexpr int attack_potentials[6] = { 1, 25, 28, 30, 100, 0 };
	constexpr int reference_mating_potential = 50; // Matériel minimum pour mater
	constexpr int reference_potential = 274 - reference_mating_potential; // Si y'a toutes les pièces de base sur l'échiquier

	int w_total_potential = 0;
	int b_total_potential = 0;

	for (uint_fast8_t i = 0; i < 8; i++)
		for (uint_fast8_t j = 0; j < 8; j++) {
			if (const uint_fast8_t p = _array[i][j]; p > 0)
				if (p < 7)
					w_total_potential += attack_potentials[p - 1];
				else
					b_total_potential += attack_potentials[(p - 1) % 6];
		}

	w_total_potential = max(0, w_total_potential - reference_mating_potential);
	b_total_potential = max(0, b_total_potential - reference_mating_potential);

	// Le surnombre peut aussi provoquer une faiblesse sur le roi adverse -> TODO : king_weakness à modifier si attacking power est grand? -> au dessus d'une constante?

	// Constante qui determine la puissance d'attaque
	constexpr int reference_attacking_power = 75;

	// Affiche la faiblesse resultante
	if (display)
	{
		cout << "______________________________" << endl;
		cout << "White king new weakness : " << w_king_weakness << "		-	 Black king new weakness : " << b_king_weakness << endl;
	}

	// Affiche les puissances d'attaque
	if (display)
	{
		cout << "White attacking power : " << static_cast<float>(w_attacking_power) / reference_attacking_power + static_cast<float>(w_total_potential) / reference_potential;
		cout << "		-	 Black attacking power : " << static_cast<float>(b_attacking_power) / reference_attacking_power + static_cast<float>(b_total_potential) / reference_potential << endl;
	}

	// Calcul de la resultante finale
	w_king_weakness = w_king_weakness * (static_cast<float>(b_attacking_power) / reference_attacking_power + static_cast<float>(b_total_potential) / reference_potential);
	b_king_weakness = b_king_weakness * (static_cast<float>(w_attacking_power) / reference_attacking_power + static_cast<float>(w_total_potential) / reference_potential);

	// Même pour un roi 'non faible', un surnombre peut être dangereux
	constexpr int attack_initiation_factor = 1;
	w_king_weakness += max(0, (b_attacking_power - max(0, w_king_protection))) * attack_initiation_factor;
	b_king_weakness += max(0, (w_attacking_power - max(0, b_king_protection))) * attack_initiation_factor;

	if (display)
	{
		cout << "______________________________" << endl;
		cout << "White king weakness : " << w_king_weakness << "			-	 Black king weakness : " << b_king_weakness << endl;
	}


	if (display) {
		cout << "White safe checks : " << static_cast<int>(safe_checks.first) << "			-	 Black safe checks : " << static_cast<int>(safe_checks.second) << endl;
	}

	// En fonction de l'avancement
	w_king_weakness += safe_checks.second * safe_check_add * max(0.0f, (1 - _adv * 1.25f));
	b_king_weakness += safe_checks.first * safe_check_add * max(0.0f, (1 - _adv * 1.25f));


	const int king_safety = b_king_weakness - w_king_weakness;

	if (display) {
		cout << "Final white king weakness : " << w_king_weakness << "		-	 Final black king weakness : " << b_king_weakness << endl;
		cout << "Eval white king weakness : -" << w_king_weakness * 0.00035f << "		-	 Eval black king weakness : +" << b_king_weakness * 0.00035f << endl;
	}

	return king_safety;
}

// Fonction qui renvoie s'il y a échec et mat, pat, ou rien (1 pour mat, 0 pour pat, -1 sinon)
// TODO : à rendre plus rapide
int Board::is_mate() {
	if (_mate_checked)
		return _mate_value;

	_mate_checked = true;

	// Pour accélérer en ne re calculant pas forcément les coups (marche avec coups légaux OU illégaux)
	const int half_moves = _half_moves_count;
	_half_moves_count = 0;

	const int moves = _got_moves;

	if (_got_moves == -1)
		get_moves();

	Board b;

	for (int i = 0; i < _got_moves; i++) {
		b.copy_data(*this);
		b.make_index_move(i);
		b._player = _player;
		if (!b.in_check()) {
			_half_moves_count = half_moves;
			_got_moves = moves;
			_mate_value = -1;
			return -1;
		}
	}

	// Mat
	if (in_check()) {
		_half_moves_count = half_moves;
		_got_moves = moves;
		_mate_value = 1;
		return 1;
	}

	// Pat
	_got_moves = moves;
	_half_moves_count = half_moves;
	_mate_value = 0;
	return 0;
}

// Fonction qui dit si une pièce est capturable par l'ennemi (pour les affichages GUI)
bool Board::is_capturable(const int i, const int j) {
	_got_moves == -1 && get_moves(false, true);

	for (int k = 0; k < _got_moves; k++)
		if (_moves[k].i2 == i && _moves[k].j2 == j)
			return true;

	return false;
}

// Fonction qui renvoie si le joueur est en train de jouer (pour que l'IA arrête de réflechir à ce moment sinon ça lagge)
bool is_playing() {
	const auto [x, y] = GetMousePosition();
	return (selected_pos.first != -1 || x != mouse_pos.x || y != mouse_pos.y);
}

// Fonction qui change le mode d'affichage des flèches (oui/non)
void switch_arrow_drawing() {
	drawing_arrows = !drawing_arrows;
}

// Fonction qui affiche le PGN
void Board::display_pgn() const
{
	cout << "\n***** PGN *****\n" << main_GUI._pgn << "\n***** PGN *****" << endl;
}

// Fonction qui renvoie en chaîne de caractères la meilleure variante selon monte carlo
string Board::get_monte_carlo_variant(const bool evaluate_final_pos) 
{
	if (_got_moves == 0 || _is_game_over)
		return "";

	if (_tested_moves > 0) {
		const int move = best_monte_carlo_move();
		string s = " " + (_player ? to_string(_moves_count) + ". " : "") + move_label_from_index(move);

		if (_tested_moves == _got_moves)
			return s + monte_buffer._heap_boards[_index_children[move]].get_monte_carlo_variant(evaluate_final_pos);

		if (evaluate_final_pos) {
			int eval = monte_buffer._heap_boards[_index_children[move]]._evaluation;
			const int mate = is_eval_mate(eval);
			const string eval_text = (mate != 0) ? ((mate > 0 ? "M" : "-M") + to_string(abs(mate))) : to_string(static_cast<int>(eval));
			return s + " (" + (eval > 0 ? static_cast<string>("+") : static_cast<string>("")) + eval_text + ")";

		}
			
		return s;
	}
	
	if (evaluate_final_pos) {
		const int mate = is_eval_mate(_evaluation);
		const string eval_text = (mate != 0) ? ((mate > 0 ? "M" : "-M") + to_string(abs(mate))) : to_string(static_cast<int>(_evaluation));
		return " (" + (_evaluation > 0 ? static_cast<string>("+") : static_cast<string>("")) + eval_text + ")";
	}
	
	return "";
}

// Fonction qui trie les index des coups par nombre de noeuds décroissant
vector<int> Board::sort_by_nodes(const bool ascending) const
{
	// Tri assez moche, et lent (tri par insertion)
	vector<int> sorted_indexes;
	vector<int> sorted_nodes;

	for (int i = 0; i < _tested_moves; i++) {
		for (int j = 0; j <= sorted_indexes.size(); j++) {
			if (j == sorted_indexes.size()) {
				sorted_indexes.push_back(i);
				sorted_nodes.push_back(monte_buffer._heap_boards[_index_children[i]]._nodes);
				break;
			}
			if ((!ascending && monte_buffer._heap_boards[_index_children[i]]._nodes > sorted_nodes[j]) || (ascending && monte_buffer._heap_boards[_index_children[i]]._nodes < sorted_nodes[j])) {
				sorted_indexes.insert(sorted_indexes.begin() + j, i);
				sorted_nodes.insert(sorted_nodes.begin() + j, monte_buffer._heap_boards[_index_children[i]]._nodes);
				break;
			}
		}
	}

	return sorted_indexes;
}

// Fonction qui renvoie selon l'évaluation si c'est un mat ou non (0 si non, sinon le nombre de coups pour le mat, positif pour les blancs, négatif pour les noirs)
int Board::is_eval_mate(const int e) const
{
	int abs_eval = abs(e);

	if (10 * abs_eval > mate_value) {
		int mate_moves = (mate_value - abs_eval - _moves_count * mate_ply) * (e > 0 ? 1 : -1) / mate_ply + (_player && e > 0);
		return mate_moves != 0 ? mate_moves : 1;
	}
	else
		return 0;
}

// Fonction qui affiche un texte dans une zone donnée avec un slider
void slider_text(const string& s, float pos_x, float pos_y, float width, float height, float size, float* slider_value, Color t_color, float slider_width, float slider_height) {
	Rectangle rect_text = { pos_x, pos_y, width, height };
	DrawRectangleRec(rect_text, background_text_color);

	string new_string;
	float new_width = width - slider_width;

	// Pour chaque bout de texte
	std::stringstream ss(s);
	std::string line;

	while (getline(ss, line, '\n')) {
		// Taille horizontale du texte

		// Split le texte en parties égales
		if (float horizontal_text_size = MeasureTextEx(text_font, line.c_str(), size, font_spacing * size).x; horizontal_text_size > new_width) {
			int split_length = line.length() * new_width / horizontal_text_size;
			for (int i = split_length - 1; i < line.length() - 1; i += split_length)
				line.insert(i, "\n");
		}

		new_string += line + "\n";
	}

	// Taille verticale totale du texte

	// Si le texte prend plus de place verticalement que l'espace alloué
	if (float vertical_text_size = MeasureTextEx(text_font, new_string.c_str(), size, font_spacing * size).y; vertical_text_size > height) {
		int n_lines;

		// Nombre de lignes total
		int total_lines = 1;
		for (int i = 0; i < new_string.length() - 1; i++) {
			if (new_string.substr(i, 1) == "\n")
				total_lines++;
		}

		n_lines = total_lines * height / MeasureTextEx(text_font, new_string.c_str(), size, font_spacing * size).y;

		int starting_line = (total_lines - n_lines) * *slider_value;

		string final_text;
		int current_line = 0;

		for (int i = 0; i < new_string.length(); i++) {
			if (new_string.substr(i, 1) == "\n") {
				current_line++;
				if (current_line >= n_lines + starting_line)
					break;
			}

			if (current_line >= starting_line) {
				final_text += new_string[i];
			}
		}

		new_string = final_text;
		slider_height = height / sqrtf(total_lines - n_lines + 1);

		// Background
		Rectangle slider_background_rect = { pos_x + width - slider_width, pos_y, slider_width, height };
		DrawRectangleRec(slider_background_rect, slider_background_color);

		// Slider
		Rectangle slider_rect = { pos_x + width - slider_width, pos_y + *slider_value * (height - slider_height), slider_width, slider_height };
		DrawRectangleRec(slider_rect, slider_color);

		// Slide

		// Avec la molette
		if (is_cursor_in_rect({ pos_x, pos_y, width, height })) {
			*slider_value -= GetMouseWheelMove() * GetFrameTime() * 100 / (total_lines - n_lines);
			if (*slider_value < 0.0f)
				*slider_value = 0.0f;
			if (*slider_value > 1.0f)
				*slider_value = 1.0f;
		}
	}

	// Texte total
	const char* c = new_string.c_str();

	DrawTextEx(text_font, c, { pos_x, pos_y }, size, font_spacing * size, t_color);
}

// Fonction pour obtenir l'orientation du plateau
bool get_board_orientation() {
	return board_orientation;
}

// Fonction qui renvoie si le curseur de la souris se trouve dans le rectangle
bool is_cursor_in_rect(const Rectangle rec) {
	mouse_pos = GetMousePosition();
	return (is_in(mouse_pos.x, rec.x, rec.x + rec.width) && is_in(mouse_pos.y, rec.y, rec.y + rec.height));
}

// Fonction qui dessine un rectangle à partir de coordonnées flottantes
bool draw_rectangle(const float pos_x, const float pos_y, const float width, const float height, const Color color) {
	DrawRectangle(float_to_int(pos_x), float_to_int(pos_y), float_to_int(width + pos_x) - float_to_int(pos_x), float_to_int(height + pos_y) - float_to_int(pos_y), color);
	return true;
}

// Fonction qui dessine un rectangle à partir de coordonnées flottantes, en fonction des coordonnées de début et de fin
bool draw_rectangle_from_pos(const float pos_x1, const float pos_y1, const float pos_x2, const float pos_y2, const Color color) {
	DrawRectangle(float_to_int(pos_x1), float_to_int(pos_y1), float_to_int(pos_x2) - float_to_int(pos_x1), float_to_int(pos_y2) - float_to_int(pos_y1), color);
	return true;
}

// Fonction qui dessine un cercle à partir de coordonnées flottantes
void draw_circle(const float pos_x, const float pos_y, const float radius, const Color color) {
	DrawCircle(float_to_int(pos_x), float_to_int(pos_y), radius, color);
}

// Fonction qui dessine une ligne à partir de coordonnées flottantes
void draw_line_ex(const float x1, const float y1, const float x2, const float y2, const float thick, const Color color) {
	DrawLineEx({ static_cast<float>(float_to_int(x1)), static_cast<float>(float_to_int(y1)) }, { static_cast<float>(float_to_int(x2)), static_cast<float>(float_to_int(y2)) }, thick, color);
}

// Fonction qui dessine une ligne de Bézier à partir de coordonnées flottantes
void draw_line_bezier(const float x1, const float y1, const float x2, const float y2, const float thick, const Color color) {
	DrawLineBezier({ static_cast<float>(float_to_int(x1)), static_cast<float>(float_to_int(y1)) }, { static_cast<float>(float_to_int(x2)), static_cast<float>(float_to_int(y2)) }, thick, color);
}

// Fonction qui dessine une texture à partir de coordonnées flottantes
void draw_texture(const Texture& texture, const float pos_x, const float pos_y, const Color color) {
	DrawTexture(texture, float_to_int(pos_x), float_to_int(pos_y), color);
}

// Fonction qui joue un match entre deux IA utilisant GrogrosZero, et une évaluation par réseau de neurones et renvoie le résultat de la partie (1/-1/0)
int match(Evaluator* e_white, Evaluator* e_black, Network* n_white, Network* n_black, const int nodes, const bool display, const int max_moves) {
	if (display)
		cout << "Match (" << max_moves << " moves max)" << endl;

	Board b;

	// Jeu
	while ((b.is_mate() == -1 && b.is_game_over() == 0)) {
		if (b._player)
			b.grogros_zero(e_white, nodes, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, true, false, 0, n_white);
		else
			b.grogros_zero(e_black, nodes, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, true, false, 0, n_black);
		b.play_monte_carlo_move_keep(b.best_monte_carlo_move(), false, true, false);

		// Limite de coups
		if (max_moves && b._player && b._moves_count > max_moves)
			break;
	}

	int g = b.is_mate();
	if (g == -1)
		g = b.is_game_over();
	else
		return -g * b.get_color();
	if (g == 2)
		return 0;
	return g;

	return g;
}

// Fonction qui organise un tournoi entre les IA utilisant évaluateurs et réseaux de neurones des listes et renvoie la liste des scores
int* tournament(Evaluator** evaluators, Network** networks, const int n_players, const int nodes, const int victory, const int draw, const bool display_full, const int max_moves) {
	cout << "***** Tournament !! " << n_players << " players *****" << endl;

	// Liste des scores
	const auto scores = new int[n_players];
	for (int i = 0; i < n_players; i++)
		scores[i] = 0;

	for (int i = 0; i < n_players; i++) {
		if (display_full)
			cout << "\n***** Round : " << i + 1 << "/" << n_players << " *****" << endl;

		for (int j = 0; j < n_players; j++) {
			if (i != j) {
				if (display_full)
					cout << "\nPlayer " << i << " vs Player " << j << endl;
				const int result = match(evaluators[i], evaluators[j], networks[i], networks[j], nodes, display_full, max_moves);
				if (result == 1) {
					if (display_full)
						cout << "1-0" << endl;
					scores[i] += victory;
				}
				else if (result == -1) {
					if (display_full)
						cout << "0-1" << endl;
					scores[j] += victory;
				}
				else {
					if (display_full)
						cout << "1/2-1/2" << endl;
					scores[i] += draw;
					scores[j] += draw;
				}
			}
		}
	}

	// Afficher les scores
	cout << "Scores : " << endl;
	print_array(scores, n_players);

	return scores;
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

int total_positions = 0;
string all_positions[102];

// Fonction qui calcule la structure de pions et renvoie sa valeur
int Board::get_pawn_structure(float display_factor) const
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
	constexpr float isolated_adv_factor = 0.3f; // En fonction de l'advancement de la partie
	const float isolated_adv = 1 * (1 + (isolated_adv_factor - 1) * _adv);
	int isolated_pawns = 0;

	for (uint_fast8_t i = 0; i < 8; i++) {
		if (s_white[i] > 0 && (i == 0 || s_white[i - 1] == 0) && (i == 7 || s_white[i + 1] == 0))
			isolated_pawns += isolated_pawn * s_white[i] / (1 + (i == 0 || i == 7)) * isolated_adv;
		if (s_black[i] > 0 && (i == 0 || s_black[i - 1] == 0) && (i == 7 || s_black[i + 1] == 0))
			isolated_pawns -= isolated_pawn * s_black[i] / (1 + (i == 0 || i == 7)) * isolated_adv;
	}

	if (display_factor != 0.0f)
		eval_components += "isolated pawns: " + (isolated_pawns >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * isolated_pawns * display_factor))) + " | ";

	pawn_structure += isolated_pawns;

	// Pions doublés (ou triplés...)
	constexpr int doubled_pawn = -25;
	constexpr float doubled_adv_factor = 0.5f; // En fonction de l'advancement de la partie
	const float doubled_adv = 1 * (1 + (doubled_adv_factor - 1) * _adv);
	int doubled_pawns = 0;

	for (uint_fast8_t i = 0; i < 8; i++) {
		doubled_pawns += (s_white[i] >= 2) * doubled_pawn * (s_white[i] - 1) * doubled_adv;
		doubled_pawns -= (s_black[i] >= 2) * doubled_pawn * (s_black[i] - 1) * doubled_adv;
	}

	if (display_factor != 0.0f)
		eval_components += "doubled pawns: " + (doubled_pawns >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * doubled_pawns * display_factor))) + " | ";

	pawn_structure += doubled_pawns;

	// Pions passés
	// r5k1/P3Rpp1/7p/8/3p4/8/2P2PPP/6K1 b - - 1 30 : position à tester
	// Table de valeur des pions passés en fonction de leur avancement sur le plateau
	static const int passed_pawns[8] = { 0, 50, 50, 100, 165, 250, 300, 0 }; // TODO à vérif

	// Pion passé bloqué
	static const int blocked_passed_pawn[8] = { 0, 10, 10, 20, 30, 40, 50, 60 };


	constexpr float passed_adv_factor = 2.0f; // En fonction de l'advancement de la partie
	const float passed_adv = 1 * (1 + (passed_adv_factor - 1) * _adv);
	int passed_pawns_value = 0;

	// Pour chaque colonne
	for (uint_fast8_t i = 0; i < 8; i++) {
		// On prend en compte seulement le pion le plus avancé de la colonne (car les autre seraient bloqués derrière)
		if (s_white[i] >= 1) {
			// On regarde de la rangée la plus proche de la promotion, jusqu'a la première
			for (uint_fast8_t j = 6; j > 0; j--) {
				// S'il y a un pion potentiellement passé
				if (pawns_white[j][i]) {
					// Pas de pion sur une colonne adjacente ou pareille avec une lattitude supérieure (strictement)
					bool is_passed_pawn = true;
					for (uint_fast8_t k = j + 1; k < 7; k++)
						if ((i > 0 && _array[k][i - 1] == 7) || _array[k][i] == 7 || (i < 7 && _array[k][i + 1] == 7)) {
							is_passed_pawn = false;
							break;
						}

					if (is_passed_pawn) {
						// Regarde s'il y a un bloqueur // TODO : pareil si la case est controllée par une pièce adverse
						bool blocked = false;
						for (uint_fast8_t k = j + 1; k <= 7; k++)
							if (_array[k][i] != 0) {
								blocked = true;
								break;
							}

						passed_pawns_value += (blocked ? blocked_passed_pawn[j] : passed_pawns[j]) * passed_adv;
					}
						
				}
			}
		}

		if (s_black[i] >= 1) {
			for (uint_fast8_t j = 1; j < 7; j++) {
				// Pas de pion sur une colonne adjacente ou pareille avec une lattitude inférieure (strictement)
				if (pawns_black[j][i]) {
					bool is_passed_pawn = true;
					for (uint_fast8_t k = j - 1; k > 0; k--)
						if ((i > 0 && _array[k][i - 1] == 1) || _array[k][i] == 1 || (i < 7 && _array[k][i + 1] == 1))
						{
							is_passed_pawn = false;
							break;
						}

					if (is_passed_pawn) {
						// Regarde s'il y a un bloqueur
						bool blocked = false;
						for (uint_fast8_t k = j - 1; k > 0; k--)
							if (_array[k][i] != 0) {
								blocked = true;
								break;
							}

						passed_pawns_value -= (blocked ? blocked_passed_pawn[7 - j] : passed_pawns[7 - j]) * passed_adv;
					}
				}
			}
		}
	}

	if (display_factor != 0.0f)
		eval_components += "passed pawns: " + (passed_pawns_value >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * passed_pawns_value * display_factor))) + " || ";

	pawn_structure += passed_pawns_value;


	// Pions connectés
	// Un pion est dit connecté, s'il y a un pion de la même couleur sur une colonne adjacente sur la même rangée ou la rangée inférieure
	constexpr int connected_pawns[8] = { 0, 35, 50, 70, 95, 115, 150, 0 };
	constexpr float connected_pawns_factor = 1.0f; // En fonction de l'advancement de la partie
	const float connected_pawns_adv = 1 * (1 + (connected_pawns_factor - 1) * _adv);

	int connected_pawns_value = 0;

	// Pour chaque colonne
	for (uint_fast8_t j = 0; j < 8; j++) {
		for (uint_fast8_t i = 1; i < 7; i++) {
			if (pawns_white[i][j]) {
				if ((j > 0 && (pawns_white[i][j - 1] || pawns_white[i - 1][j - 1])) || (j < 7 && (pawns_white[i][j + 1] || pawns_white[i - 1][j + 1])))
					connected_pawns_value += connected_pawns[i] * connected_pawns_adv;
			}
			else if (pawns_black[i][j]) {
				if ((j > 0 && (pawns_black[i][j - 1] || pawns_black[i + 1][j - 1])) || (j < 7 && (pawns_black[i][j + 1] || pawns_black[i + 1][j + 1])))
					connected_pawns_value -= connected_pawns[7 - i] * connected_pawns_adv;
			}
		}
	}

	if (display_factor != 0.0f)
		eval_components += "connected pawns: " + (connected_pawns_value >= 0 ? string("+") : string()) + to_string(static_cast<int>(round(100 * connected_pawns_value * display_factor))) + " || ";

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

// Fonction qui met à jour le temps des joueurs
void GUI::update_time() {
	// Faut-il quand même mettre à jour le temps quand il est désactivé?
	if (!_time)
		return;

	if (_last_player)
		_time_white -= clock() - _last_move_clock;
	else
		_time_black -= clock() - _last_move_clock;

	_last_move_clock = clock();

	// Gestion du temps
	if (_board._player != _last_player) {
		if (_board._player) {
			_time_black -= clock() - _last_move_clock - _time_increment_black;
			_pgn += " {[%clk " + clock_to_string(_time_black, true) + "]}";
		}
		else {
			_time_white -= clock() - _last_move_clock - _time_increment_white;
			_pgn += " {[%clk " + clock_to_string(_time_white, true) + "]}";
		}

		_last_move_clock = clock();
	}

	_last_player = _board._player;
}

// Fonction qui lance le temps
void GUI::start_time() {
	update_time_control();
	_time = true;
	_last_move_clock = clock();
}

// Fonction qui stoppe le temps
void GUI::stop_time() {
	update_time_control();
	update_time();
	_time = false;
}

// Fonction qui calcule la résultante des attaques et des défenses et la renvoie
float Board::get_attacks_and_defenses(float attack_scale, float defense_scale) const
{
	// TODO faire en sorte que l'on puisse calculer les attaques seules ou les défenses seules
	int attacks_eval = 0;
	int defenses_eval = 0;

	// Tableau des valeurs d'attaques des pièces (0 = pion, 1 = caval, 2 = fou, 3 = tour, 4 = dame, 5 = roi)
	static constexpr int attacks_array[6][6] = {
		//   P    N    B     R    Q    K
		{0,   25,  25,  30,  50,  70}, // P
		{5,   0,   20,  30, 100,  80}, // N
		{5,   10,  0,   20,  60,  40}, // B
		{5,   5,   5,   0,   60,  40}, // R
		{5,   5,   5,   10,  0,   60}, // Q
		{10,  20,  20,  25,  0,    0}, // K
	};

	// Tableau des valeurs de défenses des pièces (0 = pion, 1 = caval, 2 = fou, 3 = tour, 4 = dame, 5 = roi)
	static constexpr int defenses_array[6][6] = {
		//   P    N    B     R    Q    K
		{15,   5,  10,   5,    5,  0}, // P
		{5,   10,  10,  15,   20,  0}, // N
		{5,   10,  10,   5,   15,  0}, // B
		{10,  10,  10,  50,   25,  0}, // R
		{2,    5,   5,  10,   20,  0}, // Q
		{15,   5,   5,   5,   10,  0}, // K
	};

	// TODO ne pas additionner la défense de toutes les pièces? seulement regarder les pièces non-défendues? (sinon devient pleutre)

	// Tant pis pour le en passant...

	uint_fast8_t p; uint_fast8_t p2;
	uint_fast8_t i2; uint_fast8_t j2;

	// Diagonales
	constexpr static int_fast8_t dx[] = { -1, -1, 1, 1 };
	constexpr static int_fast8_t dy[] = { -1, 1, -1, 1 };

	// Mouvements rectilignes
	constexpr static int_fast8_t vx[] = { -1, 1, 0, 0 }; // vertical
	constexpr static int_fast8_t hy[] = { 0, 0, -1, 1 }; // horizontal

	// TODO Switch à changer, car c'est lent
	// TODO changer les if par des &&

	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			p = _array[i][j];
			switch (p) {
				// Pion blanc
			case 1:
				if (j > 0) {
					p2 = _array[i + 1][j - 1]; // Case haut-gauche du pion blanc
					if (p2 >= 7)
						attacks_eval += attacks_array[0][p2 - 7];
					else
						p2 && (defenses_eval += defenses_array[0][p2 - 1]);
				}
				if (j < 7) {
					p2 = _array[i + 1][j + 1]; // Case haut-droit du pion blanc
					if (p2 >= 7)
						attacks_eval += attacks_array[0][p2 - 7];
					else
						p2 && (defenses_eval += defenses_array[0][p2 - 1]);
				}
				break;
				// Cavalier blanc
			case 2:
				for (int k = -2; k <= 2; k++) {
					for (int l = -2; l <= 2; l++) {
						i2 = i + k; j2 = j + l;
						if (k * l != 0 && abs(k) + abs(l) == 3 && is_in_fast(i2, 0, 7) && is_in_fast(j2, 0, 7)) {
							p2 = _array[i2][j2];
							if (p2 >= 7)
								attacks_eval += attacks_array[1][p2 - 7];
							else
								p2 && (defenses_eval += defenses_array[1][p2 - 1]);
						}
					}
				}
				break;
				// Fou blanc
			case 3:
				// Pour chaque diagonale
				for (int idx = 0; idx < 4; ++idx) {
					i2 = i;
					j2 = j;
					int lim = min(dx[idx] == 1 ? 7 - i : i, dy[idx] == 1 ? 7 - j : j);

					while (lim > 0) {
						i2 += dx[idx];
						j2 += dy[idx];
						p2 = _array[i2][j2];
						if (p2 != 0) {
							if (p2 >= 7)
								attacks_eval += attacks_array[2][p2 - 7];
							else
								p2 && (defenses_eval += defenses_array[2][p2 - 1]);
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
					i2 = i;
					j2 = j;
					int lim = vx[idx] == -1 ? i : (vx[idx] == 1 ? 7 - i : (hy[idx] == -1 ? j : 7 - j));

					while (lim > 0) {
						i2 += vx[idx];
						j2 += hy[idx];
						p2 = _array[i2][j2];
						if (p2 != 0) {
							if (p2 >= 7)
								attacks_eval += attacks_array[3][p2 - 7];
							else
								p2 && (defenses_eval += defenses_array[3][p2 - 1]);
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
					i2 = i;
					j2 = j;
					int lim = min(dx[idx] == 1 ? 7 - i : i, dy[idx] == 1 ? 7 - j : j);

					while (lim > 0) {
						i2 += dx[idx];
						j2 += dy[idx];
						p2 = _array[i2][j2];
						if (p2 != 0) {
							if (p2 >= 7)
								attacks_eval += attacks_array[4][p2 - 7];
							else
								p2 && (defenses_eval += defenses_array[4][p2 - 1]);
							break;
						}
						lim--;
					}
				}

				// Pour chaque mouvement rectiligne
				for (int idx = 0; idx < 4; ++idx) {
					i2 = i;
					j2 = j;
					int lim = vx[idx] == -1 ? i : (vx[idx] == 1 ? 7 - i : (hy[idx] == -1 ? j : 7 - j));

					while (lim > 0) {
						i2 += vx[idx];
						j2 += hy[idx];
						p2 = _array[i2][j2];
						if (p2 != 0) {
							if (p2 >= 7)
								attacks_eval += attacks_array[4][p2 - 7];
							else
								p2 && (defenses_eval += defenses_array[4][p2 - 1]);
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
							i2 = i + k; j2 = j + l;
							if (is_in_fast(i2, 0, 7) && is_in_fast(j2, 0, 7)) {
								p2 = _array[i2][j2];
								if (p2 >= 7)
									attacks_eval += attacks_array[5][p2 - 7];
								else
									p2 && (defenses_eval += defenses_array[5][p2 - 1]);
							}
						}
					}
				}
				break;

				// Pion noir
			case 7:
				if (j > 0) {
					p2 = _array[i - 1][j - 1]; // Case bas-gauche du pion noir
					if (p2 >= 1 && p2 <= 6)
						attacks_eval -= attacks_array[0][p2 - 1];
					else
						p2 && (defenses_eval -= defenses_array[0][p2 - 7]);
				}
				if (j < 7) {
					p2 = _array[i - 1][j + 1]; // Case bas-droit du pion noir
					if (p2 >= 1 && p2 <= 6)
						attacks_eval -= attacks_array[0][p2 - 1];
					else
						p2 && (defenses_eval -= defenses_array[0][p2 - 7]);
				}
				break;
				// Cavalier noir
			case 8:
				for (int k = -2; k <= 2; k++) {
					for (int l = -2; l <= 2; l++) {
						i2 = i + k; j2 = j + l;
						if (k * l != 0 && abs(k) + abs(l) == 3 && is_in_fast(i2, 0, 7) && is_in_fast(j2, 0, 7)) {
							p2 = _array[i2][j2];
							if (p2 >= 1 && p2 <= 6)
								attacks_eval -= attacks_array[1][p2 - 1];
							else
								p2 && (defenses_eval -= defenses_array[1][p2 - 7]);
						}
					}
				}
				break;
				// Fou noir
			case 9:
				// Pour chaque diagonale
				for (int idx = 0; idx < 4; ++idx) {
					i2 = i;
					j2 = j;
					int lim = min(dx[idx] == 1 ? 7 - i : i, dy[idx] == 1 ? 7 - j : j);

					while (lim > 0) {
						i2 += dx[idx];
						j2 += dy[idx];
						p2 = _array[i2][j2];
						if (p2 != 0) {
							if (p2 >= 1 && p2 <= 6)
								attacks_eval -= attacks_array[2][p2 - 1];
							else
								p2 && (defenses_eval -= defenses_array[2][p2 - 7]);
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
					i2 = i;
					j2 = j;
					int lim = vx[idx] == -1 ? i : (vx[idx] == 1 ? 7 - i : (hy[idx] == -1 ? j : 7 - j));

					while (lim > 0) {
						i2 += vx[idx];
						j2 += hy[idx];
						p2 = _array[i2][j2];
						if (p2 != 0) {
							if (p2 >= 1 && p2 <= 6)
								attacks_eval -= attacks_array[3][p2 - 1];
							else
								p2 && (defenses_eval -= defenses_array[3][p2 - 7]);
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
					i2 = i;
					j2 = j;
					int lim = min(dx[idx] == 1 ? 7 - i : i, dy[idx] == 1 ? 7 - j : j);

					while (lim > 0) {
						i2 += dx[idx];
						j2 += dy[idx];
						p2 = _array[i2][j2];
						if (p2 != 0) {
							if (p2 >= 1 && p2 <= 6)
								attacks_eval -= attacks_array[4][p2 - 1];
							else
								p2 && (defenses_eval -= defenses_array[4][p2 - 7]);
							break;
						}
						lim--;
					}
				}

				// Pour chaque mouvement rectiligne
				for (int idx = 0; idx < 4; ++idx) {
					i2 = i;
					j2 = j;
					int lim = vx[idx] == -1 ? i : (vx[idx] == 1 ? 7 - i : (hy[idx] == -1 ? j : 7 - j));

					while (lim > 0) {
						i2 += vx[idx];
						j2 += hy[idx];
						p2 = _array[i2][j2];
						if (p2 != 0) {
							if (p2 >= 1 && p2 <= 6)
								attacks_eval -= attacks_array[4][p2 - 1];
							else
								p2 && (defenses_eval -= defenses_array[4][p2 - 7]);
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
							i2 = i + k; j2 = j + l;
							if (is_in_fast(i2, 0, 7) && is_in_fast(j2, 0, 7)) {
								p2 = _array[i2][j2];
								if (p2 >= 1 && p2 <= 6)
									attacks_eval -= attacks_array[5][p2 - 1];
								else
									p2 && (defenses_eval -= defenses_array[5][p2 - 7]);
							}
						}
					}
				}
				break;
			}
		}
	}

	return attacks_eval * attack_scale + defenses_eval * defense_scale;
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

// Fonction qui affiche la barre d'evaluation
void draw_eval_bar(const float eval, const string& text_eval, const float x, const float y, const float width, const float height, const float max_eval, const Color white, const Color black, const float max_height) {
	const bool is_mate = text_eval.find('M') != -1;
	const float max_bar = is_mate ? 1 : max_height;
	const float switch_color = min(max_bar * height, max((1 - max_bar) * height, height / 2 - eval / max_eval * height / 2));
	const float static_eval_switch = min(max_bar * height, max((1 - max_bar) * height, height / 2 - main_GUI._board._static_evaluation / max_eval * height / 2));
	const bool orientation = get_board_orientation();
	if (orientation) {
		draw_rectangle(x, y, width, height, black);
		draw_rectangle(x, y + switch_color, width, height - switch_color, white);
		draw_rectangle(x, y + static_eval_switch - 1.0f, width, 2.0f, RED);
	}
	else {
		draw_rectangle(x, y, width, height, black);
		draw_rectangle(x, y, width, height - switch_color, white);
		draw_rectangle(x, y + height - static_eval_switch - 1.0f, width, 2.0f, RED);
	}

	const float y_margin = (1 - max_height) / 4;
	const bool text_pos = (orientation ^ (eval < 0));
	float t_size = width / 2;
	Vector2 text_dimensions = MeasureTextEx(text_font, text_eval.c_str(), t_size, font_spacing);
	if (text_dimensions.x > width)
		t_size = t_size * width / text_dimensions.x;
	text_dimensions = MeasureTextEx(text_font, text_eval.c_str(), t_size, font_spacing);
	DrawTextEx(text_font, text_eval.c_str(), { x + (width - text_dimensions.x) / 2.0f, y + (y_margin + text_pos * (1.0f - y_margin * 2.0f)) * height - text_dimensions.y * text_pos }, t_size, font_spacing, (eval < 0) ? white : black);
}

// Fonction qui retire les surlignages de toutes les cases
void remove_highlighted_tiles() {
	for (int i = 0; i < 8; i++)
		for (int j = 0; j < 8; j++)
			highlighted_array[i][j] = 0;
}

// Fonction qui selectionne une case
void select_tile(int a, int b) {
	selected_pos = { a, b };
}

// Fonction qui surligne une case (ou la de-surligne)
void highlight_tile(const int a, const int b) {
	highlighted_array[a][b] = 1 - highlighted_array[a][b];
}

// Fonction qui renvoie le type de pièce sélectionnée
uint_fast8_t Board::selected_piece() const
{
	// Faut-il stocker cela pour éviter de le re-calculer?
	if (selected_pos.first == -1 || selected_pos.second == -1)
		return 0;
	return _array[selected_pos.first][selected_pos.second];
}

// Fonction qui renvoie le type de pièce où la souris vient de cliquer
uint_fast8_t Board::clicked_piece() const
{
	if (clicked_pos.first == -1 || clicked_pos.second == -1)
		return 0;
	return _array[clicked_pos.first][clicked_pos.second];
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

// Fonction qui déselectionne
void unselect() {
	selected_pos = { -1, -1 };
}

// Fonction qui remet les compteurs de temps "à zéro" (temps de base)
void Board::reset_timers() {
	// Temps par joueur (en ms)
	main_GUI._time_white = base_time_white;
	main_GUI._time_black = base_time_black;

	// Incrément (en ms)
	main_GUI._time_increment_white = base_time_increment_white;
	main_GUI._time_increment_black = base_time_increment_black;
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

			mat += piece_GUI_values[p % 6] * (1 - (p / 6) * 2);
		}
	}

	for (int i = 0; i < 6; i++) {
		missing_w_material[i] = max(0, base_material[i] - w_material[i]);
		missing_b_material[i] = max(0, base_material[i] - b_material[i]);
	}

	return mat;
}

// Fonction qui joue le son de fin de partie
void play_end_sound() {
	PlaySound(game_end_sound);
}

// A partir de coordonnées sur le plateau
void draw_simple_arrow_from_coord(const int i1, const int j1, const int i2, const int j2, float thickness, Color c) {
	// cout << thickness << endl;
	if (thickness == -1.0f)
		thickness = arrow_thickness;
	const float x1 = board_padding_x + tile_size * orientation_index(j1) + tile_size / 2;
	const float y1 = board_padding_y + tile_size * orientation_index(7 - i1) + tile_size / 2;
	const float x2 = board_padding_x + tile_size * orientation_index(j2) + tile_size / 2;
	const float y2 = board_padding_y + tile_size * orientation_index(7 - i2) + tile_size / 2;

	c.a = 255;

	// "Flèche"
	if (abs(j2 - j1) != abs(i2 - i1) && abs(j2 - j1) + abs(i2 - i1) == 3)
		draw_line_bezier(x1, y1, x2, y2, thickness, c);
	else
		draw_line_ex(x1, y1, x2, y2, thickness, c);

	//c.a = 255;

	draw_circle(x1, y1, thickness, c);
	draw_circle(x2, y2, thickness * 2.0f, c);
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

// Fonction qui renvoie la profondeur de calcul de la variante principale
int Board::grogros_main_depth() const
{
	if ((_got_moves == -1 && !_is_game_over) || _got_moves == 0)
		return 0;

	if (_is_game_over)
		return 0;

	if (_tested_moves == _got_moves) {
		const int move = best_monte_carlo_move();
		return 1 + monte_buffer._heap_boards[_index_children[move]].grogros_main_depth();
	}

	return 1;
}

// Fonction qui calcule la valeur des cases controllées sur l'échiquier
int Board::get_square_controls() const
{
	// TODO ajouter des valeurs pour le contrôle des cases par les pièces?

	// Valeur du contrôle de chaque case (pour les pions)
	static constexpr int square_controls[8][8] = {
		{20,  20,  20,  20,  20,  20,  20,  20},
		{50,  50,  50,  50,  50,  50,  50,  50},
		{10,  20,  40,  60,  60,  50,  20,  10},
		{5,   10,  20,  50,  50,  20,  10,   5},
		{0,    5,  10,  40,  40,  10,   5,   0},
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
	constexpr float control_adv_factor = -1.0f; // En fonction de l'advancement de la partie

	return total_control * max(0.0f, (1 + (control_adv_factor - 1) * _adv));
}

// Fonction qui renvoie la valeur UCT
float uct(const float win_chance, const float c, const int nodes_parent, const int nodes_child) {
	// cout << win_chance << ", " << nodes_parent << ", " << nodes_child << " = " << win_chance + c * sqrt(log(nodes_parent) / nodes_child) << endl;
	return win_chance + c * static_cast<float>(sqrt(log(nodes_parent) / nodes_child));
}

// Fonction qui sélectionne et renvoie le coup avec le meilleur UCT
int Board::select_uct(const float c) const
{
	float max_uct = 0;
	int uct_move = 0;

	// Pour chaque noeud fils
	for (int i = 0; i < _got_moves; i++) {
		const float win_chance = get_winning_chances_from_eval(monte_buffer._heap_boards[_index_children[i]]._evaluation,
			monte_buffer._heap_boards[_index_children[i]]._mate, _player);
		if (const float uct_value = uct(win_chance, c, _nodes, _nodes_children[i]); uct_value > max_uct) {
			max_uct = uct_value;
			uct_move = i;
		}
	}

	return uct_move;
}

// Fonction qui fait un tri rapide des coups (en plaçant les captures en premier)
// TODO : à faire lors de la génération de coups?
bool Board::quick_moves_sort() {

	// Si le tri a déjà été fait
	if (_quick_sorted_moves)
		return false;

	// Captures, promotions.. échecs?
	// TODO : ajouter les échecs? les mats?
	// TODO : faire en fonction de la pièce qui prend?

	// Liste des valeurs de chaque coup
	const auto moves_values = new int[_got_moves];

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
			move_value += (move_value < 0 && is_controlled(move.i2, move.j2)) ? move_value : 10000;
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
	auto* new_moves = new Move[_got_moves];
	copy(_moves, _moves + _got_moves, new_moves);

	for (int i = 0; i < _got_moves; i++)
		_moves[i] = new_moves[moves_indexes[i]];

	// Suppression des tableaux
	delete[] moves_values;
	delete[] new_moves;
	delete[] moves_indexes;

	_quick_sorted_moves = true;
	_sorted_moves = false;

	return true;
}

// Fonction qui fait un quiescence search
// TODO améliorer avec un delta pruning
int Board::quiescence(Evaluator* eval, int alpha, const int beta, const int depth, bool explore_checks, bool main_player)
{
	// Compte le nombre de noeuds visités
	_quiescence_nodes = 1;
	
	// Si la partie est terminée
	is_game_over();
	if (_game_over_value == 2)
		return 0;
	if (_game_over_value != 0)
		return -mate_value + _moves_count * mate_ply;

	// Evalue la position initiale
	evaluate_int(eval);
	const int stand_pat = static_cast<int>(_evaluation) * get_color();

	if (depth == 0)
		return stand_pat;
		

	// Beta cut-off
	if (stand_pat >= beta)
		return beta;

	// Mise à jour de alpha si l'éval statique est plus grande
	if (alpha < stand_pat)
		alpha = stand_pat;

	// Trie rapidement les coups
	quick_moves_sort();

	for (int i = 0; i < _got_moves; i++) {
		// TODO : ajouter promotions et échecs
		// TODO : utiliser des flags

		// Si c'est une capture
		if (_array[_moves[i].i2][_moves[i].j2] != 0) {
			Board b;
			b.copy_data(*this);
			b.make_index_move(i);

			const int score = -b.quiescence(eval, -beta, -alpha, depth - 1, explore_checks, true);
			_quiescence_nodes += b._quiescence_nodes;

			if (score >= beta)
				return beta;

			if (score > alpha)
				alpha = score;
		}

		// Mats
		// TODO : utiliser les flags 'échec' pour savoir s'il faut regarder ce coup
		else if (explore_checks)
		{
			Board b;
			b.copy_data(*this);
			b.make_index_move(i);
			if (!main_player || b.in_check())
			{
				//if (deep_mates_check)
				//{
				//	const int mate = b.is_mate();
				//	if (mate == 1)
				//		return 100 * (1000000 - 1000 * b._moves_count);
				//	if (mate == 0)
				//		return 0;
				//}

				if (explore_checks)
				{
					const int score = -b.quiescence(eval, -beta, -alpha, depth - 1, explore_checks, !main_player);
					_quiescence_nodes += b._quiescence_nodes;

					if (score >= beta)
						return beta;

					if (score > alpha)
						alpha = score;
				}
			}
		}
	}

	return alpha;
}

// Constructeur GUI
GUI::GUI() {
}

// GUI
GUI main_GUI;

// Fonction qui renvoie le i-ème coup
int* Board::get_i_move(const int i) const
{
	if (i < 0 || i >= _got_moves) {
		cout << "i-th move impossible to find";
		return nullptr;
	}

	const auto coord = new int[4];
	coord[0] = _moves[i].i1;
	coord[1] = _moves[i].j1;
	coord[2] = _moves[i].i2;
	coord[3] = _moves[i].j2;

	return coord;
}

// Fonction qui fait cliquer le i-ème coup
bool Board::click_i_move(const int i, const bool orientation) const
{
	const int* coord = get_i_move(i);

	if (coord == nullptr)
		return false;

	click_move(coord[0], coord[1], coord[2], coord[3], main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom, orientation);
	return true;
}

// Fonction qui met en place le binding avec chess.com pour une nouvelle partie
bool GUI::new_bind_game() {
	const int orientation = bind_board_orientation(main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom);

	if (orientation == -1)
		return false;

	_board.restart();
	main_GUI.reset_pgn();

	if (get_board_orientation() != orientation)
		switch_orientation();

	if (orientation) {
		// Joueur blanc
		_white_player = "GrogrosZero";
		_white_title = "BOT";
		_white_elo = _grogros_zero_elo;
		_white_url = "https://images.chesscomfiles.com/uploads/v1/user/284728633.4af59e2f.50x50o.0c8cdf830b69.png";
		_white_country = "57";

		// Joueur noir
		_black_player = "chess.com player";
		_black_title = "";
		_black_elo = "";
		_black_url = "";
		_black_country = "";
	}
	else {
		// Joueur blanc
		_white_player = "chess.com player";
		_white_title = "";
		_white_elo = "";
		_white_url = "";
		_white_country = "";

		// Joueur noir
		_black_player = "GrogrosZero";
		_black_title = "BOT";
		_black_elo = _grogros_zero_elo;
		_black_url = "https://images.chesscomfiles.com/uploads/v1/user/284728633.4af59e2f.50x50o.0c8cdf830b69.png";
		_black_country = "57";
	}

	_binding_solo = true;
	_binding_full = false;
	_click_bind = true;
	if (!monte_buffer._init)
		monte_buffer.init();
	main_GUI.start_time();
	_grogros_analysis = false;

	return true;
}

// Fonction qui met en place le binding avec chess.com pour une nouvelle analyse de partie
bool GUI::new_bind_analysis() {
	const int orientation = bind_board_orientation(main_GUI._binding_left, main_GUI._binding_top, main_GUI._binding_right, main_GUI._binding_bottom);

	if (orientation == -1)
		return false;

	_board.restart();

	if (get_board_orientation() != orientation)
		switch_orientation();

	if (orientation) {
		_white_player = "chess.com player 1";
		_black_player = "chess.com player 2";
	}
	else {
		_white_player = "chess.com player 2";
		_black_player = "chess.com player 1";
	}

	_binding_solo = false;
	_binding_full = true;
	_click_bind = false;
	if (!monte_buffer._init)
		monte_buffer.init();
	_grogros_analysis = true;

	return true;
}

// Fonction qui compare deux coups pour savoir lequel afficher en premier
bool compare_move_arrows(const int m1, const int m2) {
	// Si deux flèches finissent en un même point, affiche en dernier (au dessus), le "meilleur" coup
	if (main_GUI._board._moves[m1].i2 == main_GUI._board._moves[m2].i2 && main_GUI._board._moves[m1].j2 == main_GUI._board._moves[m2].j2)
		return main_GUI._board._nodes_children[m1] > main_GUI._board._nodes_children[m2];

	// Si les deux flèches partent d'un même point (et vont dans la même direction - TODO), alors affiche par dessus la flèche la plus courte
	if (main_GUI._board._moves[m1].i1 == main_GUI._board._moves[m2].i1 && main_GUI._board._moves[m1].j1 == main_GUI._board._moves[m2].j1) {
		const int d1 = (main_GUI._board._moves[m1].i1 - main_GUI._board._moves[m1].i2) * (main_GUI._board._moves[m1].i1 - main_GUI._board._moves[m1].i2) + (main_GUI._board._moves[m1].j1 - main_GUI._board._moves[m1].j2) * (main_GUI._board._moves[m1].j1 - main_GUI._board._moves[m1].j2);
		const int d2 = (main_GUI._board._moves[m2].i1 - main_GUI._board._moves[m2].i2) * (main_GUI._board._moves[m2].i1 - main_GUI._board._moves[m2].i2) + (main_GUI._board._moves[m2].j1 - main_GUI._board._moves[m2].j2) * (main_GUI._board._moves[m2].j1 - main_GUI._board._moves[m2].j2);
		return d1 > d2;
	}

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
	DrawTextEx(text_box.text_font, text_box.text.c_str(), { text_x, text_y }, text_box.text_size, font_spacing * text_box.text_size, text_box.text_color);
}

// Fonction qui récupère et renvoie la couleur du joueur au trait (1 pour les blancs, -1 pour les noirs)
int Board::get_color() const
{
	return _player ? 1 : -1;
}

// Fonction qui génère la clé du plateau
uint_fast64_t generate_board_key(const Board& b)
{
	uint_fast64_t key = 0;

	/*for (int i = 0; i < 64; i++)
		if (b._board[i] != 0)
			key ^= _zobrist_keys[i][b._board[i] + 1];
	if (b._player)
		key ^= _zobrist_keys[64][0];*/

	return key;
}

// Fonction qui génère et renvoie la clé de Zobrist de la position
uint_fast64_t Board::get_zobrist_key() const
{
	uint_fast64_t key = 0;

	/*for (int i = 0; i < 64; i++)
		if (_board[i] != 0)
			key ^= _zobrist_keys[i][_board[i] + 1];
	if (_player)
		key ^= _zobrist_keys[64][0];*/

	return key;
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

// Fonction qui renvoie la puissance d'attaque d'une pièce sur le roi adverse
int Board::get_piece_attack_power(const int i, const int j) const
{
	// TODO : faire des if en fonction de la position i, j pour éviter des calculs
	const uint_fast8_t piece = _array[i][j];

	// Renvoie 0 si la case est vide
	if (piece == 0)
		return 0;

	// Tableaux des puissances d'attaque

	// Pion
	static constexpr int pawn_attacking_power_map[8][8] = {
	{0, 15, 5, 0, 0, 0, 0, 0},
	{25, 30, 5, 0, 0, 0, 0, 0},
	{20, 15, 10, 0, 0, 0, 0, 0},
	{10, 5, 0, 0, 0, 0, 0, 0},
	{5, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0}
	};

	// Cavalier
	static constexpr int knight_attacking_power_map[8][8] = {
	{0, 15, 15, 10, 0, 0, 0, 0},
	{25, 20, 15, 10, 0, 0, 0, 0},
	{30, 45, 10, 5, 0, 0, 0, 0},
	{60, 95, 15, 10, 0, 0, 0, 0},
	{20, 15, 10, 5, 0, 0, 0, 0},
	{10, 15, 10, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0}
	};

	// Fou
	static constexpr int bishop_attacking_power_map[8][8] = {
	{0,  70, 50, 20, 0, 0, 0, 0},
	{70, 60, 40, 20, 0, 0, 0, 0},
	{75, 80, 45, 10, 0, 0, 0, 0},
	{30, 25, 20, 15, 0, 0, 0, 0},
	{15, 10, 25, 20, 15, 0, 0, 0},
	{0, 0, 0, 25, 20, 15, 0, 0},
	{0, 0, 0, 0, 25, 20, 15, 0},
	{0, 0, 0, 0, 0, 25, 20, 15}
	};

	// Tour
	static constexpr int rook_attacking_power_map[8][8] = {
	{0, 100, 75, 75, 75, 75, 75, 75},
	{100, 75, 50, 50, 50, 50, 50, 50},
	{75, 50, 35, 25, 20, 10, 0, 0},
	{75, 50, 25, 10, 0, 0, 0, 0},
	{75, 50, 20, 0, 0, 0, 0, 0},
	{75, 50, 10, 0, 0, 0, 0, 0},
	{75, 50, 0, 0, 0, 0, 0, 0},
	{75, 50, 0, 0, 0, 0, 0, 0}
	};

	// Dame
	static constexpr int queen_attacking_power_map[8][8] = {
	{ 0, 200, 152, 104, 44, 32, 20, 12},
	{200, 176, 144, 96, 32, 24, 12, 4},
	{144, 136, 120, 88, 20, 12, 4, 0},
	{136, 104, 96, 80, 12, 8, 4, 0},
	{100, 80, 60, 12, 4, 0, 0, 0},
	{ 44, 24, 12, 4, 0, 0, 0, 0},
	{ 24, 12, 4, 0, 0, 0, 0, 0},
	{ 20, 4, 0, 0, 0, 0, 0, 0} };

	// Roi
	static constexpr int king_attacking_power_map[8][8] = {
	{0, 0, 75, 0, 0, 0, 0, 0},
	{0, 0, 35, 0, 0, 0, 0, 0},
	{75, 35, 25, 0, 0, 0, 0, 0},
	{20, 15, 10, 5, 0, 0, 0, 0},
	{5, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0}
	};

	// Renvoie la valeur correspondante à la puissance d'attaque de la pièce

	// Pièces blanches
	if (piece == 1)
		return (i > _black_king_pos.i) ? 0 : pawn_attacking_power_map[_black_king_pos.i - i][abs(j - _black_king_pos.j)];
	if (piece == 2)
		return knight_attacking_power_map[abs(i - _black_king_pos.i)][abs(j - _black_king_pos.j)];
	if (piece == 3)
		return bishop_attacking_power_map[abs(i - _black_king_pos.i)][abs(j - _black_king_pos.j)];
	if (piece == 4)
		return rook_attacking_power_map[abs(i - _black_king_pos.i)][abs(j - _black_king_pos.j)];
	if (piece == 5)
		return queen_attacking_power_map[abs(i - _black_king_pos.i)][abs(j - _black_king_pos.j)];
	if (piece == 6)
		return king_attacking_power_map[abs(i - _black_king_pos.i)][abs(j - _black_king_pos.j)];

	// Pièces noires
	if (piece == 7)
		return (i < _white_king_pos.i) ? 0 : pawn_attacking_power_map[i - _white_king_pos.i][abs(j - _white_king_pos.j)];
	if (piece == 8)
		return knight_attacking_power_map[abs(i - _white_king_pos.i)][abs(j - _white_king_pos.j)];
	if (piece == 9)
		return bishop_attacking_power_map[abs(i - _white_king_pos.i)][abs(j - _white_king_pos.j)];
	if (piece == 10)
		return rook_attacking_power_map[abs(i - _white_king_pos.i)][abs(j - _white_king_pos.j)];
	if (piece == 11)
		return queen_attacking_power_map[abs(i - _white_king_pos.i)][abs(j - _white_king_pos.j)];
	if (piece == 12)
		return king_attacking_power_map[abs(i - _white_king_pos.i)][abs(j - _white_king_pos.j)];

	return 0;
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
	b.get_moves(false, false);
	for (uint_fast8_t i = 0; i < b._got_moves; i++)
	{
		const uint_fast8_t p = _array[b._moves[i].i1][b._moves[i].j1];
		if (p != 1 && p != 5)
			white_controlled_squares[b._moves[i].i2][b._moves[i].j2]++;
	}

	// Noirs
	b._player = false;
	b._got_moves = -1;
	b.get_moves(false, false);
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
	{10, 10, 15, 15, 15, 15, 10, 10},
	{10, 10, 15, 20, 20, 15, 10, 10},
	{0,  0,  5,  10, 10,  5,  0,  0},
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

	return activity * (1 - _adv);
}

// Fonction qui reset le buffer
bool Buffer::reset() const
{
	for (int i = 0; i < _length; i++)
		_heap_boards[i].reset_board();

	return true;
}

// Fonction qui construit le PGN global
bool GUI::update_global_pgn()
{
	_global_pgn = "";

	// Headers

	// Joueurs
	if (!_white_player.empty())
		_global_pgn += "[White \"" + _white_player + "\"]\n";
	if (!_black_player.empty())
		_global_pgn += "[Black \"" + _black_player + "\"]\n";

	// Titres des joueurs
	if (!_white_title.empty())
		_global_pgn += "[WhiteTitle \"" + _white_title + "\"]\n";
	if (!_black_title.empty())
		_global_pgn += "[BlackTitle \"" + _black_title + "\"]\n";

	// Elo des joueurs
	if (!_white_elo.empty())
		_global_pgn += "[WhiteElo \"" + _white_elo + "\"]\n";
	if (!_black_elo.empty())
		_global_pgn += "[BlackElo \"" + _black_elo + "\"]\n";

	// URL des joueurs
	if (!_white_url.empty())
		_global_pgn += "[WhiteUrl \"" + _white_url + "\"]\n";
	if (!_black_url.empty())
		_global_pgn += "[BlackUrl \"" + _black_url + "\"]\n";

	// Pays des joueurs
	if (!_white_country.empty())
		_global_pgn += "[WhiteCountry \"" + _white_country + "\"]\n";
	if (!_black_country.empty())
		_global_pgn += "[BlackCountry \"" + _black_country + "\"]\n";

	// Cadence
	if (!_time_control.empty())
		_global_pgn += "[TimeControl \"" + _time_control + "\"]\n";

	// FEN importé
	if (!_initial_fen.empty())
		_global_pgn += "[FEN \"" + _initial_fen + "\"]\n";

	// Date
	if (!_date.empty())
		_global_pgn += "[Date \"" + _date + "\"]\n";

	// Ajout du PGN de la partie
	_global_pgn += _pgn;

	return true;
}

// Fonction qui met à jour la cadence du PGN
bool GUI::update_time_control()
{
	_time_control = to_string(static_cast<int>(max(_time_white, _time_black) / 1000)) + " + " + to_string(static_cast<int>(max(_time_increment_white, _time_increment_black) / 1000));
	return true;
}

// Fonction qui réinitialise le PGN
bool GUI::reset_pgn()
{
	update_date();
	_pgn = "";
	_initial_fen = "";

	return  true;
}

// Fonction qui met à jour la date du PGN
bool GUI::update_date() {
	const time_t current_time = time(nullptr);
	tm local_time;
	localtime_s(&local_time, &current_time);

	const int year = local_time.tm_year + 1900;
	const int month = local_time.tm_mon + 1;
	const int day = local_time.tm_mday;

	_date = std::to_string(year) + "." + std::to_string(month) + "." + std::to_string(day);

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
	// On remplace le roi par une dame, et on regarde le nombre de coups possibles
	update_kings_pos();
	Board b(*this);
	const int i = color ? _white_king_pos.i : _black_king_pos.i;
	const int j = color ? _white_king_pos.j : _black_king_pos.j;
	color ? b._array[i][j] = 5 : b._array[i][j] = 11;
	
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
pair<uint_fast8_t, uint_fast8_t> Board::get_safe_checks(Map white_controls, Map black_controls) const
{

	// TODO : à revoir -> prendre en compte les clouages pour les échecs safe (la pièce clouée ne contrôle pas vraiment la case...)

	// Un échec safe est :
	// - un échec fait sur une case non controlée par l'adversaire
	// - un échec fait sur une case controlée par le roi adverse, mais aussi controlée par une pièce alliée



	// Map des cases ou un échec safe est possible pour les blancs
	Map white_safe_checks;

	// Nombre d'échecs safe pour les blancs
	uint_fast8_t white_safe_checks_nb = 0;

	// Regarde tous les coups possibles pour les blancs (sans regarder les échecs)
	Board b_white(*this);
	b_white._player = false;

	// On ne le calcule seulement si le roi noir n'est pas en échec
	if (!b_white.in_check()) {
		b_white._player = true;
		b_white._got_moves = -1;
		b_white.get_moves(false, false);

		for (int i = 0; i < b_white._got_moves; i++) {
			// Si la destination du coup est non controlée par les noirs, ou qu'elle est seulement controllée par le roi noir et une pièce blanche (au moins)

			// Case de destination du coup
			const uint_fast8_t i2 = b_white._moves[i].i2;
			const uint_fast8_t j2 = b_white._moves[i].j2;

			/*if (white_safe_checks._array[i2][j2] > 0)
				continue;*/

			// Nombres de contrôles de la case de destination par les blancs et les noirs
			const uint_fast8_t controls_white = white_controls._array[i2][j2];
			const uint_fast8_t controls_black = black_controls._array[i2][j2];

			if (controls_black == 0 || (controls_black == 1 && controls_white > 1 && abs(b_white._black_king_pos.i - i2) <= 1 && abs(b_white._black_king_pos.j - j2) <= 1)) {
				// Joue le coup et regarde s'il fait échec
				Board b_white_check(b_white);
				b_white_check.make_index_move(i);

				// TODO : à remplacer par 'est-ce que le coup attaque le roi'?
				if (b_white_check.in_check()) {
					white_safe_checks._array[i2][j2] = 1;
					white_safe_checks_nb++;
				}
			}
		}
	}

	


	// Map des cases ou un échec safe est possible pour les noirs
	Map black_safe_checks;

	// Nombre d'échecs safe pour les noirs
	uint_fast8_t black_safe_checks_nb = 0;

	// Regarde tous les coups possibles pour les noirs (sans regarder les échecs)
	Board b_black(*this);
	b_black._player = true;

	// On ne le calcule seulement si le roi blanc n'est pas en échec
	if (!b_black.in_check()) {
		b_black._player = false;
		b_black._got_moves = -1;
		b_black.get_moves(false, false);

		for (int i = 0; i < b_black._got_moves; i++) {
			// Si la destination du coup est non controlée par les blancs, ou qu'elle est seulement controllée par le roi blanc et une pièce noire (au moins)

			// Case de destination du coup
			const uint_fast8_t i2 = b_black._moves[i].i2;
			const uint_fast8_t j2 = b_black._moves[i].j2;

			/*if (black_safe_checks._array[i2][j2] > 0)
				continue;*/

			// Nombres de contrôles de la case de destination par les blancs et les noirs
			const uint_fast8_t controls_white = white_controls._array[i2][j2];
			const uint_fast8_t controls_black = black_controls._array[i2][j2];

			if (controls_white == 0 || (controls_white == 1 && controls_black > 1 && abs(b_black._white_king_pos.i - i2) <= 1 && abs(b_black._white_king_pos.j - j2) <= 1)) {
				// Joue le coup et regarde s'il fait échec
				Board b_black_check(b_black);
				b_black_check.make_index_move(i);
				if (b_black_check.in_check()) {
					black_safe_checks._array[i2][j2] = 1;
					black_safe_checks_nb++;
				}
			}
		}
	}



	return { white_safe_checks_nb, black_safe_checks_nb };
}

// Fonction qui lance les threads de GrogrosZero
bool GUI::thread_grogros_zero(Evaluator *eval, int nodes)
{
	// Initialisation du buffer pour GrogrosZero, si besoin
	if (!monte_buffer._init)
		monte_buffer.init();

	// Lance grogros sur 1 noeud
	_board.grogros_zero(eval, 100, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr);

	//_thread_grogros_zero = thread(&Board::grogros_zero, &_board, eval, nodes, true, _beta, _k_add, _quiescence_depth, true, true, false, 0, nullptr);
	////_thread_grogros_zero = thread(&Board::grogros_zero, &monte_buffer._heap_boards[main_GUI._board._index_children[0]], &monte_evaluator, nodes, true, main_GUI._beta, main_GUI._k_add, main_GUI._quiescence_depth, true, true, false, 0, nullptr);
	//_thread_grogros_zero.detach();

	//thread test = thread(&Board::grogros_zero, &monte_buffer._heap_boards[_board._index_children[0]], eval, nodes, true, _beta, _k_add, _quiescence_depth, true, true, false, 0, nullptr);
	//test.detach();

	//thread test2 = thread(&Board::grogros_zero, &monte_buffer._heap_boards[_board._index_children[1]], eval, nodes, true, _beta, _k_add, _quiescence_depth, true, true, false, 0, nullptr);
	////_threads_grogros_zero.push_back()
	//test2.detach();


	_threads_grogros_zero.clear();

	//_threads_grogros_zero.emplace_back(&Board::grogros_zero, &monte_buffer._heap_boards[_board._index_children[0]], eval, nodes, true, _beta, _k_add, _quiescence_depth, true, true, false, 0, nullptr);
	//_threads_grogros_zero.emplace_back(&Board::grogros_zero, &monte_buffer._heap_boards[_board._index_children[1]], eval, nodes, true, _beta, _k_add, _quiescence_depth, true, true, false, 0, nullptr);
	//_threads_grogros_zero.emplace_back(&Board::grogros_zero, &monte_buffer._heap_boards[_board._index_children[2]], eval, nodes, true, _beta, _k_add, _quiescence_depth, true, true, false, 0, nullptr);

	// Lance un thread pour chaque coup possible
	/*for (int i = 0; i < _board._got_moves; i++) {
		_threads_grogros_zero.emplace_back(&Board::grogros_zero, &monte_buffer._heap_boards[_board._index_children[i]], eval, nodes, true, _beta, _k_add, _quiescence_depth, true, true, false, 0, nullptr);
	}*/

	/*for (int i = 0; i < _board._got_moves; i++) {
		Board test(_board);
		_threads_grogros_zero.emplace_back(&Board::grogros_zero, test, eval, nodes, true, _beta, _k_add, _quiescence_depth, true, true, false, 0, nullptr);
	}*/

	for (auto& thread : _threads_grogros_zero) {
		thread.join();
	}

	// Relance grogros sur 1 noeud (pour actualiser les valeurs)
	_board.grogros_zero(eval, 100, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr);

	return true;
}

// Fonction qui lance grogros sur un thread
bool GUI::grogros_zero_threaded(Evaluator* eval, int nodes) {
	// Initialisation du buffer pour GrogrosZero, si besoin
	if (!monte_buffer._init)
		monte_buffer.init();

	// Lance grogros sur un thread
	_thread_grogros_zero = thread(&Board::grogros_zero, &_board, eval, nodes, _beta, _k_add, _quiescence_depth, true, false, 0, nullptr);

	_thread_grogros_zero.detach();
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
	get_moves(false, true);

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

// Fonction qui renvoie si la case est controlée par le joueur adverse
bool Board::is_controlled(int square_i, int square_j) const
{

	// Regarde les potentielles attaques de cavalier
	constexpr int knight_offsets[8][2] = { {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}, {1, -2}, {2, -1}, {2, 1}, {1, 2} };

	for (int k = 0; k < 8; k++) {
		const int ni = square_i + knight_offsets[k][0];

		// S'il y a un cavalier qui attaque, renvoie vrai (en échec)
		if (const int nj = square_j + knight_offsets[k][1]; ni >= 0 && ni < 8 && nj >= 0 && nj < 8 && _array[ni][nj] == (2 + _player * 6))
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
			if (piece < 7 != _player)
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
			if (piece < 7 != _player)
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
			if (piece < 7 != _player)
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
			if (piece < 7 != _player)
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
			if (piece < 7 != _player)
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
			if ((piece < 7) != _player)
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
			if ((piece < 7) != _player)
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
			if ((piece < 7) != _player)
			{
				if (const int simple_piece = (piece - 1) % 6 + 1; simple_piece == 3 || simple_piece == 5 || (simple_piece == 6 && abs(square_j - i) == 1))
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
