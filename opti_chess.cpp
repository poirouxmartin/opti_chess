#include "opti_chess.h"
#include "useful_functions.h"
#include "gui.h"
#include <string>
#include <sstream>
#include <thread>
#include "math.h"
#include "windows_tests.h"
#include <utility>


// Tests pour la parallélisation
vector<thread> threads;



// Constructeur par défaut
Board::Board() {
}


// Constructeur de copie
Board::Board(Board &b) {
    // Copie du plateau
    memcpy(_array, b._array, sizeof(_array));

    _got_moves = b._got_moves;
    _player = b._player;
    memcpy(_moves, b._moves, sizeof(_moves));
    _sorted_moves = b._sorted_moves;
    _quick_sorted_moves = b._quick_sorted_moves;
    _evaluation = b._evaluation;
    _color = b._color;
    _castling_rights = b._castling_rights;
    _en_passant = b._en_passant;
    _half_moves_count = b._half_moves_count;
    _moves_count = b._moves_count;
    _pgn = b._pgn;
}

// Fonction qui copie les attributs d'un tableau
void Board::copy_data(Board &b) {
    // Copie du plateau
    memcpy(_array, b._array, sizeof(_array));

    _got_moves = b._got_moves;
    _player = b._player;
    memcpy(_moves, b._moves, sizeof(_moves));
    _sorted_moves = b._sorted_moves;
    _quick_sorted_moves = b._quick_sorted_moves;
    _evaluation = b._evaluation;
    _color = b._color;
    _castling_rights = b._castling_rights;
    _en_passant = b._en_passant;
    _half_moves_count = b._half_moves_count;
    _moves_count = b._moves_count;
    _pgn = b._pgn;
}


// Fonction qui copie les coups d'un plateau
void Board::copy_moves(Board &b) {
    _got_moves = b._got_moves;
    memcpy(_moves, b._moves, sizeof(_moves));
}


// Affichage du plateau
void Board::display() {
    int p;
    string s = "\n--------------------------------\n";

    for (int i = 7; i > -1; i--) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            switch (p)
            {   
                case 0: s += "|   "; break;
                case 1: s += "| P "; break;
                case 2: s += "| N "; break;
                case 3: s += "| B "; break;
                case 4: s += "| R "; break;
                case 5: s += "| Q "; break;
                case 6: s += "| K "; break;
                case 7: s += "| p "; break;
                case 8: s += "| n "; break;
                case 9: s += "| b "; break;
                case 10: s += "| r "; break;
                case 11: s += "| q "; break;
                case 12: s += "| k "; break;
            }

        }
        s += "|\n--------------------------------\n";
    }

    cout << s;
}


// Fonction qui à partir des coordonnées d'un coup renvoie le coup codé sur un entier (à 4 chiffres) (base 10)
int Board::move_to_int(int i, int j, int k, int l) {
    return l + 10 * (k + 10 * (j + 10 * i));
}


// Fonction qui ajoute un coup dans une liste de coups
bool Board::add_move(uint_fast8_t i, uint_fast8_t j, uint_fast8_t k, uint_fast8_t l, int *iterator) {

    // Si on dépasse le nombre de coups que l'on pensait possible dans une position
    if (*iterator + 4 > _max_moves * 4)
        return false;

    _moves[*iterator] = i;
    _moves[*iterator + 1] = j;
    _moves[*iterator + 2] = k;
    _moves[*iterator + 3] = l;

    *iterator += 4;
    return true;
}


// Fonction qui ajoute les coups "pions" dans la liste de coups
bool Board::add_pawn_moves(uint_fast8_t i, uint_fast8_t j, int *iterator) {
    static const string abc = "abcdefgh";

    // Joueur avec les pièces blanches
    if (_player) {
        // Poussée (de 1)
        (_array[i + 1][j] == 0) && add_move(i, j, i + 1, j, iterator);
        // Poussée (de 2)
        (i == 1 && _array[i + 1][j] == 0 && _array[i + 2][j] == 0) && add_move(i, j, i + 2, j, iterator);
        // Prise (gauche)
        (j > 0 && (is_in_fast(_array[i + 1][j - 1], 7, 12) || (_en_passant[0] == abc[j - 1] && _en_passant[1] - 48 - 1 == i + 1))) && add_move(i, j, i + 1, j - 1, iterator);
        // Prise (droite)
        (j < 7 && (is_in_fast(_array[i + 1][j + 1], 7, 12) || (_en_passant[0] == abc[j + 1] && _en_passant[1] - 48 - 1 == i + 1))) && add_move(i, j, i + 1, j + 1, iterator);
    }
    // Joueur avec les pièces noires
    else {
        // Poussée (de 1)
        (_array[i - 1][j] == 0) && add_move(i, j, i - 1, j, iterator);
        // Poussée (de 2)
        (i == 6 && _array[i - 1][j] == 0 && _array[i - 2][j] == 0) && add_move(i, j, i - 2, j, iterator);
        // Prise (gauche)
        (j > 0 && (is_in_fast(_array[i - 1][j - 1], 1, 6) || (_en_passant[0] == abc[j - 1] && _en_passant[1] - 48 - 1 == i - 1))) && add_move(i, j, i - 1, j - 1, iterator);
        // Prise (droite)
        (j < 7 && (is_in_fast(_array[i - 1][j + 1], 1, 6) || (_en_passant[0] == abc[j + 1] && _en_passant[1] - 48 - 1 == i - 1))) && add_move(i, j, i - 1, j + 1, iterator);
    }

    return true;
}


// Fonction qui ajoute les coups "cavaliers" dans la liste de coups
bool Board::add_knight_moves(uint_fast8_t i, uint_fast8_t j, int *iterator) {
    uint_fast8_t i2, j2;
    // On va utiliser un tableau pour stocker les déplacements possibles du cavalier
    static const int_fast8_t knight_moves[8][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1}};
    // On parcourt ce tableau
    for (int m = 0; m < 8; m++) {
        i2 = i + knight_moves[m][0];
        j2 = j + knight_moves[m][1];
        if (_player)
            (is_in_fast(i2, 0, 7) && is_in_fast(j2, 0, 7) && !is_in_fast(_array[i2][j2], 1, 6)) && add_move(i, j, i2, j2, iterator);
        else
            (is_in_fast(i2, 0, 7) && is_in_fast(j2, 0, 7) && !is_in_fast(_array[i2][j2], 7, 12)) && add_move(i, j, i2, j2, iterator);
    }

    return true;
}


// Fonction qui ajoute les coups diagonaux dans la liste de coups
bool Board::add_diag_moves(uint_fast8_t i, uint_fast8_t j, int *iterator) {
    uint_fast8_t ally_min = _player ? 1 : 7;
    uint_fast8_t ally_max = _player ? 6 : 12;

    uint_fast8_t i2; uint_fast8_t j2; uint_fast8_t p2;
        
    // Diagonale 1
    for (int k = 1; k < 8; k++) {
        i2 = i + k; j2 = j + k;
        // Si le coup n'est pas sur le plateau
        if (!is_in_fast(i2, 0, 7) || !is_in_fast(j2, 0, 7))
            k = 7;
        else {
            p2 = _array[i2][j2];
            // Si la case est occupée par une pièce alliée
            if (is_in_fast(p2, ally_min, ally_max))
                k = 7;
            // Sinon
            else {
                add_move(i, j, i2, j2, iterator);
                // Si la case est occupée par une pièce adverse
                if (p2 != 0) {
                    k = 7;
                }
            }
        }
    }

    // Diagonale 2
    for (int k = 1; k < 8; k++) {
        i2 = i - k; j2 = j + k;
        // Si le coup n'est pas sur le plateau
        if (!is_in_fast(i2, 0, 7) || !is_in_fast(j2, 0, 7))
            k = 7;
        else {
            p2 = _array[i2][j2];
            // Si la case est occupée par une pièce alliée
            if (is_in_fast(p2, ally_min, ally_max))
                k = 7;
            // Sinon
            else {
                add_move(i, j, i2, j2, iterator);
                // Si la case est occupée par une pièce adverse
                if (p2 != 0) {
                    k = 7;
                }
            }
        }
    }

    // Diagonale 3
    for (int k = 1; k < 8; k++) {
        i2 = i + k; j2 = j - k;
        // Si le coup n'est pas sur le plateau
        if (!is_in_fast(i2, 0, 7) || !is_in_fast(j2, 0, 7))
            k = 7;
        else {
            p2 = _array[i2][j2];
            // Si la case est occupée par une pièce alliée
            if (is_in_fast(p2, ally_min, ally_max))
                k = 7;
            // Sinon
            else {
                add_move(i, j, i2, j2, iterator);
                // Si la case est occupée par une pièce adverse
                if (p2 != 0) {
                    k = 7;
                }
            }
        }
    }

    // Diagonale 4
    for (int k = 1; k < 8; k++) {
        i2 = i - k; j2 = j - k;
        // Si le coup n'est pas sur le plateau
        if (!is_in_fast(i2, 0, 7) || !is_in_fast(j2, 0, 7))
            k = 7;
        else {
            p2 = _array[i2][j2];
            // Si la case est occupée par une pièce alliée
            if (is_in_fast(p2, ally_min, ally_max))
                k = 7;
            // Sinon
            else {
                add_move(i, j, i2, j2, iterator);
                // Si la case est occupée par une pièce adverse
                if (p2 != 0) {
                    k = 7;
                }
            }
        }
    }

    return true;

}


// Fonction qui ajoute les coups horizontaux et verticaux dans la liste de coups
bool Board::add_rect_moves(uint_fast8_t i, uint_fast8_t j, int *iterator) {
    uint_fast8_t ally_min = _player ? 1 : 7;
    uint_fast8_t ally_max = _player ? 6 : 12;

    uint_fast8_t i2; uint_fast8_t j2;
    uint_fast8_t p2;

    // Horizontale 1
    for (int k = 1; k < 8; k++) {
        j2 = j - k;
        // Si le coup n'est pas sur le plateau
        if (!is_in_fast(i, 0, 7) || !is_in_fast(j2, 0, 7))
            k = 7;
        else {
            p2 = _array[i][j2];
            // Si la case est occupée par une pièce alliée
            if (is_in_fast(p2, ally_min, ally_max))
                k = 7;
            // Sinon
            else {
                add_move(i, j, i, j2, iterator);
                // Si la case est occupée par une pièce adverse
                if (p2 != 0) {
                    k = 7;
                }
            }
        }
    }

    // Horizontale 2
    for (int k = 1; k < 8; k++) {
        j2 = j + k;
        // Si le coup n'est pas sur le plateau
        if (!is_in_fast(i, 0, 7) || !is_in_fast(j2, 0, 7))
            k = 7;
        else {
            p2 = _array[i][j2];
            // Si la case est occupée par une pièce alliée
            if (is_in_fast(p2, ally_min, ally_max))
                k = 7;
            // Sinon
            else {
                add_move(i, j, i, j2, iterator);
                // Si la case est occupée par une pièce adverse
                if (p2 != 0) {
                    k = 7;
                }
            }
        }
    }

    // Verticale 1
    for (int k = 1; k < 8; k++) {
        i2 = i - k;
        // Si le coup n'est pas sur le plateau
        if (!is_in_fast(i2, 0, 7) || !is_in_fast(j, 0, 7))
            k = 7;
        else {
            p2 = _array[i2][j];
            // Si la case est occupée par une pièce alliée
            if (is_in_fast(p2, ally_min, ally_max))
                k = 7;
            // Sinon
            else {
                add_move(i, j, i2, j, iterator);
                // Si la case est occupée par une pièce adverse
                if (p2 != 0) {
                    k = 7;
                }
            }
        }
    }

    // Verticale 2
    for (int k = 1; k < 8; k++) {
        i2 = i + k;
        // Si le coup n'est pas sur le plateau
        if (!is_in_fast(i2, 0, 7) || !is_in_fast(j, 0, 7))
            k = 7;
        else {
            p2 = _array[i2][j];
            // Si la case est occupée par une pièce alliée
            if (is_in_fast(p2, ally_min, ally_max))
                k = 7;
            // Sinon
            else {
                add_move(i, j, i2, j, iterator);
                // Si la case est occupée par une pièce adverse
                if (p2 != 0) {
                    k = 7;
                }
            }
        }
    }

    return true;

}


// Fonction qui ajoute les coups "roi" dans la liste de coups
bool Board::add_king_moves(uint_fast8_t i, uint_fast8_t j, int *iterator) {
    uint_fast8_t ally_min = _player ? 1 : 7;
    uint_fast8_t ally_max = _player ? 6 : 12;

    uint_fast8_t i2; uint_fast8_t j2;
    
    for (int k = -1; k < 2; k++) {
        for (int l = -1; l < 2; l++) {
            i2 = i + k; j2 = j + l;
            // Si le coup n'est ni hors du plateau, ni sur une case où une pièce alliée est placée
            ((k != 0 || l != 0) && is_in_fast(i2, 0, 7) && is_in_fast(j2, 0, 7) && !is_in_fast(_array[i2][j2], ally_min, ally_max)) && add_move(i, j, i2, j2, iterator);
        }
    }

    return true;

}


// Calcule la liste des coups possibles. pseudo ici fait référence au droit de roquer en passant par une position illégale.
bool Board::get_moves(bool pseudo, bool forbide_check) {

    // Si la partie est finie

    // Règle des 50 coups
    if (_half_moves_count >= 100) {
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


    int p;
    int iterator = 0;

    // Si un des rois est décédé, plus de coups possibles
    bool king_w = false;
    bool king_b = false;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            if (p == 6) {
                king_w = true;
                if (king_b)
                    goto kings;
            }
            if (p == 12) {
                king_b = true;
                if (king_w)
                    goto kings;
            }
        }
    }

    kings:

    if (!king_w || !king_b) {
        _got_moves = 0;
        return true;
    }
        
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];

            // Si on dépasse le nombre de coups que l'on pensait possible dans une position
            if (iterator >= _max_moves * 4) {
                cout << "Too many moves in the position : " << iterator / 4 + 1 << "+" << endl;
                goto end_loops;
            }
                
            
            switch (p)
            {   
                case 0: // Case vide
                    break;

                case 1: // Pion blanc
                    _player && add_pawn_moves(i, j, &iterator);
                    break;

                case 2: // Cavalier blanc
                    _player && add_knight_moves(i, j, &iterator);
                    break;

                case 3: // Fou blanc   
                    _player && add_diag_moves(i, j, &iterator);
                    break;

                case 4: // Tour blanche
                    _player && add_rect_moves(i, j, &iterator);
                    break;

                case 5: // Dame blanche
                    _player && add_diag_moves(i, j, &iterator) && add_rect_moves(i, j, &iterator);
                    break;

                case 6: // Roi blanc
                    _player && add_king_moves(i, j, &iterator);
                    // Roques
                    // Grand
                    if (_player && _castling_rights.q_w && _array[i][j - 1] == 0 && _array[i][j - 2] == 0 && _array[i][j - 3] == 0 && (pseudo || (!attacked(i, j) && !attacked(i, j - 1) && !attacked(i, j - 2))))
                        add_move(i, j, i, j - 2, &iterator);
                    // Petit
                    if (_player && _castling_rights.k_w && _array[i][j + 1] == 0 && _array[i][j + 2] == 0 && (pseudo || (!attacked(i, j) && !attacked(i, j + 1) && !attacked(i, j + 2))))
                        add_move(i, j, i, j + 2, &iterator);
                    break;

                case 7: // Pion noir
                    !_player && add_pawn_moves(i, j, &iterator);
                    break;

                case 8: // Cavalier noir
                    !_player && add_knight_moves(i, j, &iterator);
                    break;

                case 9: // Fou noir
                    !_player && add_diag_moves(i, j, &iterator);
                    break;

                case 10: // Tour noire
                    !_player && add_rect_moves(i, j, &iterator);
                    break;

                case 11: // Dame noire
                    !_player && add_diag_moves(i, j, &iterator) && add_rect_moves(i, j, &iterator);        
                    break;

                case 12: // Roi noir
                    !_player && add_king_moves(i, j, &iterator);
                    // Roques
                    // Grand
                    if (!_player && _castling_rights.q_b && _array[i][j - 1] == 0 && _array[i][j - 2] == 0 && _array[i][j - 3] == 0 && (pseudo || (!attacked(i, j) && !attacked(i, j - 1) && !attacked(i, j - 2))))
                        add_move(i, j, i, j - 2, &iterator);
                    // Petit
                    if (!_player && _castling_rights.k_b && _array[i][j + 1] == 0 && _array[i][j + 2] == 0 && (pseudo || (!attacked(i, j) && !attacked(i, j + 1) && !attacked(i, j + 2))))
                        add_move(i, j, i, j + 2, &iterator);
                    break;

            }

        }
    }

    end_loops:

    //_moves[iterator] = -1;
    _got_moves = iterator / 4;    


    // Vérification échecs
    if (forbide_check) {
        uint_fast8_t new_moves[400]; // Est-ce que ça prend de la mémoire?
        int n_moves = 0;
        Board b;


        for (int i = 0; i < _got_moves; i++) {
            b.copy_data(*this);
            b.make_index_move(i, false);
            b._color = -b._color;
            b._player = !b._player;
            if (!b.in_check()) {
                for (int k = 4 * i; k < 4 * i + 4; k++) {
                    new_moves[n_moves] = _moves[k];
                    n_moves++;
                }
            }
        }

        for (int i = 0; i < n_moves; i++) {
            _moves[i] = new_moves[i];
        }

        _got_moves = n_moves / 4;

    }

    return true;
}


// Fonction qui dit si une case est attaquée
bool Board::attacked(int i, int j) {

    // Pour accelérer les tests
    if (i < 0 || i > 7 || j < 0 || j > 7)
        return false;


    // Regarde tous les coups adverses dans cette position, puis renvoie si l'un d'entre eux a pour case finale, la case en argument
    Board b;
    b.copy_data(*this);
    b._player = !b._player;
    b._half_moves_count = 0;
    b.get_moves(true);
    for (int m = 0; m < b._got_moves; m++) {
        if (i == b._moves[4 * m + 2] && j == b._moves[4 * m + 3])
            return true;
    }

    return false;

}


// Fonction qui dit s'il y'a échec
bool Board::in_check() {
    int king = 9 - 3 * _color;
    int pos_i = -1;
    int pos_j = -1;
    
    // Trouve la case correspondante au roi
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (_array[i][j] == king) {
                pos_i = i;
                pos_j = j;
                goto end_loops;
            }
        }
    }
    end_loops:

    return attacked(pos_i, pos_j);
}


// Fonction qui donne la position du roi du joueur
pair<int, int> Board::get_king_pos() {
    pair<int, int> pos = {-1, -1};

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (_array[i][j] == 6 * (2 - _player)) {
                pos = {i, j};
                goto end_loops;
            }
        }
    }

    end_loops:
    
    return pos;

}


// Fonction qui affiche la liste des coups donnée en argument
void Board::display_moves(bool pseudo) {
    
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
void Board::make_move(uint_fast8_t i, uint_fast8_t j, uint_fast8_t k, uint_fast8_t l, bool pgn, bool new_board, bool add_to_list) {
    uint_fast8_t p = _array[i][j];
    uint_fast8_t p_last = _array[k][l];

    if (pgn) {
        if (_moves_count != 0 || _half_moves_count != 0)
            _pgn += " ";
        if (_player) {
            stringstream ss;
            ss << _moves_count;
            string s = "";
            ss >> s;
            _pgn += s;
            _pgn += ". ";
        }
        _pgn += move_label(i, j, k, l);
    }


    // Incrémentation des demi-coups
    _half_moves_count++;
    (p == 1 || p == 7 || _array[k][l]) && (_half_moves_count = 0);


    // Coups donnant la possibilité d'un en passant
    _en_passant = "-";
    if (p == 1 && k == i + 2 && (_array[k][l - 1] == 7||_array[k][l + 1] == 7)) {
        string abc = "abcdefgh";
        _en_passant = abc[j];
        _en_passant += char(i + 1 + 1 + 48);
    }
    if (p == 7 && k == i - 2 && (_array[k][l - 1] == 1||_array[k][l + 1] == 1)) {
        string abc = "abcdefgh";
        _en_passant = abc[j];
        _en_passant += char(i + 1 - 1 + 48);
    }

    // En passant
    if (p == 1 && j != l && _array[k][l] == 0)
        _array[k - 1][l] = 0;
    if (p == 7 && j != l && _array[k][l] == 0)
        _array[k + 1][l] = 0;

    
    // Si c'est le roi qui bouge, retire la permission de roque
    if (p == 6) {
        _castling_rights.q_w = false;
        _castling_rights.k_w = false;
    }
    if (p == 12) {
        _castling_rights.q_b = false;
        _castling_rights.k_b = false;
    }

    // Si c'est une tour, peut retirer la permission de roque
    if (p == 4) {
        if (j == 0)
            _castling_rights.q_w = false;
        if (j == 7)
            _castling_rights.k_w = false;
    }
    if (p == 10) {
        if (j == 0)
            _castling_rights.q_b = false;
        if (j == 7)
            _castling_rights.k_b = false;
    }

    // Si une tour se fait manger
    if (p_last == 4) {
        if (l == 0)
            _castling_rights.q_w = false;
        if (l == 7)
            _castling_rights.k_w = false;
    }
    if (p_last == 10) {
        if (l == 0)
            _castling_rights.q_b = false;
        if (l == 7)
            _castling_rights.k_b = false;
    }


    // Roque
    // Blanc
    if (p == 6) {
        // Petit
        if (l == j + 2) {
            _array[0][7] = 0;
            _array[0][5] = 4;
        }
        // Grand
        if (l == j - 2) {
            _array[0][0] = 0;
            _array[0][3] = 4;
        }
    }
    // Noir
    if (p == 12) {
        // Petit
        if (l == j + 2) {
            _array[7][7] = 0;
            _array[7][5] = 10;
        }
        // Grand
        if (l == j - 2) {
            _array[7][0] = 0;
            _array[7][3] = 10;
        }
    }

    // PGN pour roque à modifier (voir move_label)
    // Permissions de roque à modifier

    _array[k][l] = p;

    // Promotion (en dame seulement pour le moment)
    if (p == 1 && k == 7)
        _array[k][l] = 5;
    if (p == 7 && k == 0)
        _array[k][l] = 11;

    
    _array[i][j] = 0;
    _player = !_player;
    _got_moves = -1;
    _color = - _color;

    // Implémentation du FEN possible?
    _fen = "";

    // Implémentation des coups
    _player && (_moves_count += 1);

    // Actualise les coups possibles
    //get_moves();


    _last_move[0] = i;
    _last_move[1] = j;
    _last_move[2] = k;
    _last_move[3] = l;
    _last_move[4] = p;
    // if (_last_move[5] == -1) {
    //     _last_move[5] =
    // }
    _last_move[9] = p_last;

    _sorted_moves = false;
    _quick_sorted_moves = false;

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

    _mate = false;
    _mate_checked = false;
    _game_over_checked = false;
    

    if (add_to_list) {
        _all_positions[_half_moves_count] = simple_position();
        _total_positions++;
        _total_positions = _half_moves_count;
    }        

    return;
}


// Fonction qui joue le coup i
void Board::make_index_move(int i, bool pgn, bool add_to_list) {
    int k = 4 * i;
    make_move(_moves[k], _moves[k + 1], _moves[k + 2], _moves[k + 3], pgn, false, add_to_list);
}


// Fonction qui renvoie l'avancement de la partie (0 = début de partie, 1 = fin de partie)
void Board::game_advancement() {
    if (_advancement)
        return;

    _adv = 0;

    // Définition personnelle de l'avancement d'une partie : (p_tot - p) / p_tot, où p_tot = le total matériel (du joueur adverse? ou les deux?) en début de partie, et p = le total matériel (du joueur adverse? ou les deux?) actuellement
    static const int adv_pawn = 1;
    static const int adv_knight = 10;
    static const int adv_bishop = 10;
    static const int adv_rook = 10;
    static const int adv_queen = 30;
    static const int adv_castle = 5;

    static const int p_tot = 2 * (8 * adv_pawn + 2 * adv_knight + 2 * adv_bishop + 2 * adv_rook + 1 * adv_queen + 2 * adv_castle);
    int p = 0;

    uint_fast8_t piece;
    static const int values[6] = {0, adv_pawn, adv_knight, adv_bishop, adv_rook, adv_queen};

    // Pièces
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            piece = _array[i][j];
            p += values[piece % 6];
        }
    }

    // Roques
    p += (_castling_rights.k_w + _castling_rights.q_w + _castling_rights.k_b + _castling_rights.q_b) * adv_castle;

    _adv = (float)(p_tot - p) / p_tot;

    return;

}


// Fonction qui compte le matériel sur l'échiquier
void Board::count_material(Evaluator *eval) {
    if (_material)
        return;

    _material_count = 0;

    uint_fast8_t piece;
    int value;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            piece = _array[i][j];
            if (piece) {
                value = (float)eval->_pieces_value_begin[(piece - 1) % 6] * (1.0f - _adv) + (float)eval->_pieces_value_end[(piece - 1) % 6] * _adv;
                _material_count += (piece < 7) ? value : -value;
            }
        }
    }

    _material = true;
}


// Fonction qui calcule les valeurs de positionnement des pièces sur l'échiquier
// TODO : fusion avec material
void Board::pieces_positionning(Evaluator *eval) {
    if (_positioning)
        return;

    _pos = 0;

    uint_fast8_t piece;
    int value;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            piece = _array[i][j];
            if (piece) {
                value = (float)eval->_pieces_pos_begin[(piece - 1) % 6][(piece < 7) ? 7 - i : i][j] * (1.0f - _adv) + (float)eval->_pieces_pos_end[(piece - 1) % 6][(piece < 7) ? 7 - i : i][j] * _adv;
                _pos += (piece < 7) ? value : -value;
            }
        }
    }

    _positioning = true;
}


// Fonction qui évalue la position à l'aide d'heuristiques
bool Board::evaluate(Evaluator *eval, bool checkmates, bool display, Network *n) {

    _evaluated = true;
    _mate = false;

    eval_components = "";
    _evaluator = eval;

    if (checkmates) {

        int _is_mate = is_mate();

        if (_is_mate == 1) {
            _mate = true;
            _evaluation = - _color * (1000000 - 1000 * _moves_count);
            _is_game_over = true;
            if (display)
                eval_components += "Checkmate\n";
            return true;
        }
        else if (_is_mate == 0) {
            _evaluation = 0;
            _is_game_over = true;
            if (display)
                eval_components += "Stealmate\n";
            return true;
        }
        
    }

    // Répétitions

    // Règle des 50 coups
    if (_half_moves_count >= 100) {
        _evaluation = 0;
        _is_game_over = true;
        if (display)
            eval_components += "Draw by 50 moves rule\n";
        return true;
    }


    // Répétition de coups
    // if (is_in(simple_position(), _all_positions, _total_positions - 1)) {
    //     _evaluation = 0;
    //     _is_game_over = true;
    //     if (display)
    //             cout << "Draw by repetition" << endl;
    //     return true;
    // }


    // Matériel insuffisant
    int count_w_knight = 0;
    int count_w_bishop = 0;
    int count_b_knight = 0;
    int count_b_bishop = 0;
    uint_fast8_t p;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
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
                goto no_draw;

            // Si on a au moins 1 fou, et un cheval/fou ou plus -> plus de nulle par manque de matériel
            if ((count_w_bishop > 0) && (count_w_knight > 0 || count_w_bishop > 1))
                goto no_draw;
        }
    }

    // Possibilités de nulles par manque de matériel
    if (count_w_knight + count_w_bishop < 2 && count_b_knight + count_b_bishop < 2) {
        _evaluation = 0;
        _is_game_over = true;
        if (display)
            eval_components += "Draw by insufficient material";
        return true;
    }

    // On ne peut pas mater avec seulement 2 cavaliers
    if (count_w_knight == 2) {
        _evaluation = 0;
        return true;
    }


    // Sinon
    no_draw:



    // Réseau de neurones
    if (n != nullptr) {
        to_fen();
        n->input_from_fen(_fen);
        n->calculate_output();
        _evaluation = n->_output;
        return true;
    }



    // Reset l'évaluation
    _evaluation = 0;

    // Avancement de la partie
    game_advancement();
    if (display)
        eval_components += "game advancement : " + to_string((int)round(100 * _adv)) + "%\n";
        

    // Matériel
    float material = 0;
    if (eval->_piece_value != 0) {
        count_material(eval);
        material = _material_count * eval->_piece_value / 100; // à changer (le /100)
        if (display)
            eval_components += "material : " + to_string((int)round(100 * material)) + "\n";
        _evaluation += material;
    }


    // Positionnement des pièces
    float positioning = 0;
    if (eval->_piece_positioning != 0) {
        pieces_positionning(eval);
        positioning = _pos * eval->_piece_positioning;
        if (display)
            eval_components += "positionning : " + to_string((int)round(100 * positioning)) + "\n";
        _evaluation += positioning;
    }



    // Comptage des fous
    // à tester: changer les boucles par des for (i : array) pour optimiser
    int bishop_w = 0; int bishop_b = 0;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            (p == 3) && bishop_w++;
            (p == 9) && bishop_b++;
        }
    }

    // Paire de oufs
    float bishop_pair = 0;
    if (eval->_bishop_pair != 0) {
        bishop_pair = eval->_bishop_pair * ((bishop_w >= 2) - (bishop_b >= 2));
        if (display)
            eval_components += "bishop pair : " + to_string((int)round(100 * bishop_pair)) + "\n";
        _evaluation += bishop_pair;
    }
        

    // Ajout random
    float random_add = 0;
    if (eval->_random_add != 0) {
        random_add += GetRandomValue(-50, 50) * eval->_random_add / 100;
        if (display)
            eval_components += "random add : " + to_string((int)round(100 * random_add)) + "\n";
        _evaluation += random_add;
    }
        

    // Activité des pièces
    float piece_activity = 0;
    if (eval->_piece_activity != 0) {
        get_piece_activity();
        piece_activity = _piece_activity * eval->_piece_activity;
        if (display)
            eval_components += "piece activity : " + to_string((int)round(100 * piece_activity)) + "\n";
        _evaluation += piece_activity;
    }

    // Trait du joueur
    float player_trait = 0;
    if (eval->_player_trait != 0) {
        player_trait = eval->_player_trait * _color;
        if (display)
            eval_components += "player trait : " + to_string((int)round(100 * player_trait)) + "\n";
        _evaluation += player_trait;
    }


    // Droits de roques
    float castling_rights = 0;
    if (eval->_castling_rights != 0) {
        castling_rights += eval->_castling_rights * (_castling_rights.k_w + _castling_rights.q_w - _castling_rights.k_b - _castling_rights.q_b) * (1 - _adv);
        if (display)
            eval_components += "castling rights : " + to_string((int)round(100 * castling_rights)) + "\n";
        _evaluation += castling_rights;
    }
    
    // Sécurité du roi
    float king_safety = 0;
    if (eval->_king_safety != 0) {
        get_king_safety();
        king_safety = _king_safety * eval->_king_safety;
        if (display)
            eval_components += "king safety : " + to_string((int)round(100 * king_safety)) + "\n";
        _evaluation += king_safety;
    }

    // Structure de pions
    float pawn_structure = 0;
    if (eval->_pawn_structure != 0) {
        get_pawn_structure();
        pawn_structure = _pawn_structure * eval->_pawn_structure;
        if (display)
            eval_components += "pawn structure : " + to_string((int)round(100 * pawn_structure)) + "\n";
        _evaluation += pawn_structure;
    }

    // Attaques et défenses de pièces
    float pieces_attacks = 0; float pieces_defenses = 0;
    if (eval->_attacks != 0 || eval->_defenses != 0) {
        get_attacks_and_defenses();
        pieces_attacks = _attacks_eval * eval->_attacks;
        pieces_defenses = _defenses_eval * eval->_defenses;
        if (display) {
            if (eval->_attacks)
                eval_components += "attacks : " + to_string((int)round(100 * pieces_attacks)) + "\n";
            if (eval->_defenses)
                eval_components += "defenses : " + to_string((int)round(100 * pieces_defenses)) + "\n";
        }
        _evaluation += pieces_attacks;
        _evaluation += pieces_defenses;
    }

    // Opposition des rois
    float kings_opposition = 0;
    if (eval->_kings_opposition != 0) {
        get_kings_opposition();
        kings_opposition = _kings_opposition * eval->_kings_opposition;
        if (display)
            eval_components += "opposition : " + to_string((int)round(100 * kings_opposition)) + "\n";
        _evaluation += kings_opposition;
    }


    // Tours sur les colonnes ouvertes / semi-ouvertes
    float rook_open = 0; float rook_semi = 0;
    if (eval->_rook_open != 0 || eval->_rook_semi != 0) {
        get_rook_on_open_file();
        rook_open = _rook_open * eval->_rook_open;
        rook_semi = _rook_semi * eval->_rook_semi;
        if (display) {
            eval_components += "rooks on open files : " + to_string((int)round(100 * rook_open)) + "\n";
            eval_components += "rooks on semi-open files : " + to_string((int)round(100 * rook_semi)) + "\n";
        }
        _evaluation += rook_open;
        _evaluation += rook_semi;
    }


    // Contrôle des cases
    float square_controls = 0;
    if (eval->_square_controls != 0) {
        get_square_controls();
        square_controls = _control * eval->_square_controls;
        if (display)
            eval_components += "square controls : " + to_string((int)round(100 * square_controls)) + "\n";
        _evaluation += square_controls;
    }


    // Forteresse
    if (eval->_push != 0) {
        float push = 1 - _half_moves_count * eval->_push / 100;
        if (display)
            eval_components += "forteress : " + to_string((int)(100 - push * 100)) + "%\n";
        _evaluation *= push;
    }


    // Chances de gain
    get_winning_chances();
    if (display)
        eval_components += "W/D/L : " + to_string((int)(100 * _white_winning_chance)) + "/" + to_string((int)(100 * _drawing_chance)) + "/" + to_string((int)(100 * _black_winning_chance)) + "%\n";


    // Partie non finie
    return false;
}


// Fonction qui évalue la position à l'aide d'heuristiques -> évaluation entière
bool Board::evaluate_int(Evaluator *eval, bool checkmates, bool display, Network *n) {

    bool is_game_over = evaluate(eval, checkmates, display, n);
    if (n == nullptr || _mate)
        _evaluation *= 100;
    _evaluation = _evaluation + 0.5f - (_evaluation < 0); // pour l'arrondi
    _static_evaluation = (int)_evaluation;

    return is_game_over;

}


// Fonction qui joue le coup d'une position, renvoyant la meilleure évaluation à l'aide d'un negamax (similaire à un minimax)
float Board::negamax(int depth, float alpha, float beta, int color, bool max_depth, Evaluator *eval, bool play = false, bool display = false) {

    // Nombre de noeuds
    if (max_depth) {
        visited_nodes = 1;
        begin_time = clock();
    }
    else {
        visited_nodes++;
    }

    if (depth == 0) {
        //evaluate(eval);
        return quiescence(eval, -10000000, 10000000);
        //return color * _evaluation;
    }

    // à mettre avant depth == 0?
    int g = game_over();
    if (g == 2)
        return 0.0f;
    if (g == -1 || g == 1)
        return -1e7f * (float)(depth + 1);
    if (g == -10 || g == 10)
        return -1e8f * (float)(depth + 1);
        

    float value = -1e9;
    Board b;

    int best_move = 0;
    float tmp_value;

    if (_got_moves == -1) {
        if (max_depth)
            get_moves(false, true);
        else
            get_moves();
    }

    if (depth > 1)
        sort_moves(eval);
    if (max_depth)
        display_moves();

    for (int i = 0; i < _got_moves; i++) {

        b.copy_data(*this);
        b.make_index_move(i);
        
        tmp_value = -b.negamax(depth - 1, -beta, -alpha, -color, false, eval);
        // threads.emplace_back(std::thread([&]() {
        //     tmp_value = -b.negamax(depth - 1, -beta, -alpha, -color, false, eval, a, use_agent);
        // })); // Test de OpenAI

        if (max_depth) {
            if (display)
                cout << "move : " << move_label(_moves[4 * i], _moves[4 * i + 1], _moves[4 * i + 2], _moves[4 * i + 3]) << ", value : " << tmp_value << endl;
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
            cout << "visited nodes : " << (float)(visited_nodes / 1000) << "k" << endl;
            double spent_time = (double)(clock() - begin_time);
            cout << "time spend : " << spent_time << "ms"  << endl;
            cout << "speed : " << visited_nodes / spent_time << "kN/s" << endl;
        }
        if (play) {
            play_index_move_sound(best_move);
            if (display)
                if (_tested_moves > 0)
                    ((_GUI._click_bind && _GUI._board.click_i_move(_GUI._board.best_monte_carlo_move(), get_board_orientation())) || true) && play_monte_carlo_move_keep(best_move, true, true, true, false);
                else
                    make_index_move(best_move, true);
        }
            
        return value;
    }
    
    return value;
    
}


// Version un peu mieux optimisée de Grogrosfish
bool Board::grogrosfish(int depth, Evaluator *eval, bool display = false) {
    negamax(depth, -1e9, 1e9, _color, true, eval, true, display);
    if (display) {
        evaluate(eval);
        to_fen();
        cout << _fen << endl;
        cout << _pgn << endl;
    }
    
    return true;
}


// Fonction qui revient à la position précédente (ne marchera pas avec les roques pour le moment)
bool Board::undo(uint_fast8_t i1, uint_fast8_t j1, uint_fast8_t p1, uint_fast8_t i2, uint_fast8_t j2, uint_fast8_t p2, int half_moves) {
    _array[i1][j1] = p1;
    _array[i2][j2] = p2;

    // Incrémentation des demi-coups
    _half_moves_count = half_moves;

    _player = !_player;
    _got_moves = -1;
    _color = - _color;

    // Implémentation du FEN possible?
    _fen = "";

    // Decrémentation des coups
    !_player && _moves_count--;

    return true;
}


// Une surcharge
bool Board::undo() {
    // Il faut rétablir les droits de roque, les en passant........

    print_array(_last_move, 10);
    int_fast8_t i1 = _last_move[0];
    int_fast8_t j1 = _last_move[1];
    int_fast8_t k1 = _last_move[2];
    int_fast8_t l1 = _last_move[3];
    int_fast8_t p1 = _last_move[4];

    int_fast8_t i2 = _last_move[5];
    int_fast8_t j2 = _last_move[6];
    int_fast8_t k2 = _last_move[7];
    int_fast8_t l2 = _last_move[8];
    int_fast8_t p2 = _last_move[9];

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
    _color = - _color;
    _fen = "";

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
    
    bool new_board = true;
    if (new_board) {
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
void Board::sort_moves(Evaluator *eval) {
    // Modifier pour seulement garder le (ou les deux) meilleur(s) coups?

    Board b;

    (_got_moves == -1 && get_moves());

    float* values = new float[_got_moves];
    float value;

    // Création de la liste des valeurs des évaluations des positions après chaque coup
    for (int i = 0; i < _got_moves; i++) {
        // Mise à jour du plateau
        b.copy_data(*this);
        b.make_index_move(i);

        // Evaluation
        b.evaluate(eval);
        value = b._evaluation * _color;

        // Place l'évaluation en i dans les valeurs
        values[i] = value;
    }


    // Construction des nouveaux coups

    // Liste des index des coups triés par ordre décroissant de valeur
    int* moves_indexes = new int[_got_moves];
    int max_ind;

    for (int i = 0; i < _got_moves; i++) {
        max_ind = max_index(values, _got_moves);
        moves_indexes[i] = max_index(values, _got_moves);
        values[max_ind] = -FLT_MAX;
    }

    // Génération de la list de coups de façon ordonnée
    uint_fast8_t* new_moves = new uint_fast8_t[_got_moves * 4];
    copy(_moves, _moves + _got_moves * 4, new_moves);
    int j;

    for (int i = 0; i < _got_moves; i++) {
        j = moves_indexes[i];
        _moves[4 * i] = new_moves[4 * j];
        _moves[4 * i + 1] = new_moves[4 * j + 1];
        _moves[4 * i + 2] = new_moves[4 * j + 2];
        _moves[4 * i + 3] = new_moves[4 * j + 3];
    }

    // Suppression des tableaux
    delete[] values;
    delete[] new_moves;

    _sorted_moves = true;
    _quick_sorted_moves = false;
}


// Fonction qui récupère le plateau d'un FEN
void Board::from_fen(string fen, bool fen_in_pgn, bool keep_headings) {
    bool named;
    bool timed;
    if (keep_headings) {
        named = _named_pgn;
        timed = _timed_pgn;
    }
    
    reset_all();


    // Mise à jour du FEN
    _fen = fen;

    // PGN

    // keep_headings ne sert à rien pour le moment (ni fen_in_pgn)
    size_t headings;
    while(true) {
        headings = _pgn.find_last_of("]");

        if (headings == -1)
            goto no_headings;

        if (_pgn[headings + 1] != '\n')
            _pgn = _pgn.substr(0, headings);
            
        else
            break;
    }

    headings = _pgn.find_last_of("]");
    _pgn = _pgn.substr(0, headings + 3);

    no_headings:

    if (fen_in_pgn) {

        // Retire l'ancien FEN du PGN s'il y'en avait déjà un
        size_t fen_begin = _pgn.find("[FEN \"");
        if (fen_begin != -1) {
            size_t fen_end = _pgn.find_first_of("]", fen_begin);
            _pgn.replace(fen_begin + 6, fen_end - fen_begin - 7, fen);
        }
        else {
            if (headings != -1)
                _pgn.append("\n"); // Append ou += est plus rapide?

            _pgn.append("[FEN \"" + fen + "\"]\n\n");
        }
        
    }
    

    // Iterateur qui permet de parcourir la chaine de caractères
    int iterator = 0;

    // Position à itérer dans le plateau
    int i = 7;
    int j = 0;

    int digit;
    char c;
    string s;

    // Positionnement des pièces
    while (i >= 0) {
        c = fen[iterator];
        switch (c) {
            case '/' : case ' '  : i -= 1; j = 0; break;
            case 'P' : _array[i][j] = 1; j += 1; break;
            case 'N' : _array[i][j] = 2; j += 1; break;
            case 'B' : _array[i][j] = 3; j += 1; break;
            case 'R' : _array[i][j] = 4; j += 1; break;
            case 'Q' : _array[i][j] = 5; j += 1; break;
            case 'K' : _array[i][j] = 6; j += 1; break;
            case 'p' : _array[i][j] = 7; j += 1; break;
            case 'n' : _array[i][j] = 8; j += 1; break;
            case 'b' : _array[i][j] = 9; j += 1; break;
            case 'r' : _array[i][j] = 10; j += 1; break;
            case 'q' : _array[i][j] = 11; j += 1; break;
            case 'k' : _array[i][j] = 12; j += 1; break;
            default :
                if (isdigit(c)) {
                    digit = ((int)c) - ((int)'0');
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
        _color = 1;
    }
    else {
        _player = false;
        _color = -1;
    }

    iterator += 2;

    bool next = true;

    // Roques
    _castling_rights.k_w = false; _castling_rights.q_w = false; _castling_rights.k_b = false; _castling_rights.q_b = false;

    while (next) {
        c = fen[iterator];
        
        switch (c) {
            case '-' : iterator += 1; next = false; break;
            case 'K' : _castling_rights.k_w = true; break;
            case 'Q' : _castling_rights.q_w = true; break;
            case 'k' : _castling_rights.k_b = true; break;
            case 'q' : _castling_rights.q_b = true; iterator += 1; next = false; break;
            default : next = false; break;
        }

        iterator += 1;
    }


    c = fen[iterator];

    // En passant
    if (c == '-')
        _en_passant = "-";
    else {
        _en_passant = fen[iterator];
        _en_passant += fen[iterator + 1];
        iterator += 1;
    }

    iterator += 2;
    s = "";
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

    reset_eval();

    _last_move[0] = -1;
    _last_move[1] = -1;
    _last_move[2] = -1;
    _last_move[3] = -1;

    
    if (keep_headings) {
        _named_pgn = named;
        _timed_pgn = timed; 
    }
    else {
        _named_pgn = false;
        _timed_pgn = false;
    }

    _is_game_over = false;

    // Oriente le plateau dans pour le joueur qui joue
    board_orientation = _player;

}


// Fonction qui renvoie le FEN du tableau
void Board::to_fen() {
    string s = "";
    int p;
    int it = 0;

    const char *piece_letters = "PNBRQKpnbrqk";

    for (int i = 7; i >= 0; i--) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            if (p == 0)
                it += 1;
            else {

                if (it > 0) {
                    s += char(it + 48);
                    it = 0;
                }

                s += piece_letters[p - 1];

            }

        }

        if (it > 0) {
            s += char(it + 48);
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

    s += " " + _en_passant + " " + to_string(_half_moves_count) + " " + to_string(_moves_count);


    _fen = s;

    return;
    
}


// Fonction qui renvoie le gagnant si la partie est finie (-1/1, et 2 pour nulle), et 0 sinon
int Board::game_over() {

    if (_game_over_checked)
        return _game_over_value;

    _game_over_checked = true;

    // Règle des 50 coups
    if (_half_moves_count >= 100) {
        _game_over_value = 2;
        return 2;
    }

    // Si un des rois est décédé
    bool king_w = false;
    bool king_b = false;

    int p;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            if (p == 6) {
                king_w = true;
                if (king_b)
                    goto kings;
            }
            if (p == 12) {
                king_b = true;
                if (king_w)
                    goto kings;
            }
        }
    }

    kings:

    if (!king_w) {
        _game_over_value = -1;
        return -1;
    }
    if (!king_b) {
        _game_over_value = 1;
        return 1;
    }

    _game_over_value = 0;
    return 0;

}


// Fonction qui renvoie le label d'un coup
// En passant manquant... échecs aussi, puis roques, promotions, mats/pats
string Board::move_label(uint_fast8_t i, uint_fast8_t j, uint_fast8_t k, uint_fast8_t l) {
    uint_fast8_t p1 = _array[i][j]; // Pièce qui bouge
    uint_fast8_t p2 = _array[k][l];
    static const string abc = "abcdefgh";

    // Pour savoir si une autre pièce similaire peut aller sur la même case
    bool spec_col = false;
    bool spec_line = false;
    if (_got_moves == -1)
        get_moves(false, true);

    uint_fast8_t i1; uint_fast8_t j1; uint_fast8_t k1; uint_fast8_t l1; uint_fast8_t p11;
    for (int m = 0; m < _got_moves; m++) {
        i1 = _moves[4 * m];
        j1 = _moves[4 * m + 1];
        k1 = _moves[4 * m + 2];
        l1 = _moves[4 * m + 3];
        p11 = _array[i1][j1];
        // Si c'est une pièce différente que celle à bouger, mais du même type, et peut aller sur la même case
        if ((i1 != i || j1 != j) && p11 == p1 && k1 == k && l1 == l) {
            if (i1 != i)
                spec_col = true;
            if (j1 != j)
                spec_line = true;
        }
    }



    string s = "";

    switch (p1)
    {   
        case 2: case 8: s += "N"; if (spec_line) s += abc[j]; if (spec_col) s += char(i + 1 + 48); break;
        case 3: case 9: s += "B"; if (spec_line) s += abc[j]; if (spec_col) s += char(i + 1 + 48); break;
        case 4: case 10: s += "R"; if (spec_line) s += abc[j]; if (spec_col) s += char(i + 1 + 48); break;
        case 5: case 11: s += "Q"; if (spec_line) s += abc[j]; if (spec_col) s += char(i + 1 + 48); break;
        case 6: case 12: 
        if (l - j == 2) {
            s += "O-O"; return s;
        }
        if (j - l == 2) {
            s += "O-O-O"; return s;
        }
        s += "K"; break;
    }



    if (p2 || ((p1 == 1 || p1 == 7 ) && j != l)) {
        if (p1 == 1 || p1 == 7)
            s += abc[j];
        s += "x";
        s += abc[l];
        s += char(k + 1 + 48);
    }

    else {
        s += abc[l];
        s += char(k + 1 + 48);
    }

    // Promotion (seulement en dame pour le moment)
    if ((p1 == 1 && k == 7) || (p1 == 7 && k == 0))
        s += "=Q";

    
    
    // Mats, pats, échecs...
    Board b(*this);
    b.make_move(i, j, k, l);
    int m = b.is_mate();

    // mat
    if (m == 1) {
        s += "#";
        if (_player)
            s += " 1-0";
        else
            s += " 0-1";
        return s;
    }

    // pat
    else if (m == 0) {
        s += "@ 1/2-1/2";
        return s;
    }
        
    // échec
    else if (b.in_check())
        s += "+";

    // sinon... plus de coups = draw? règles des 50 coups, nul par manque de matériel... 
    // Ne fonctionne pas -> A FIX
    if (b._got_moves == 0 || b._is_game_over)
        s += "@ 1/2-1/2";

    return s;
}


// Fonction qui renvoie le label d'un coup en fonction de son index
string Board::move_label_from_index(int i) {
    // Pour pas qu'il re écrase les moves
    if (_got_moves == -1)
        get_moves(false, true);
    return move_label(_moves[4 * i], _moves[4 * i + 1], _moves[4 * i + 2], _moves[4 * i + 3]);
}


// Fonction qui fait un coup à partir de son label (pour permettre d'importer une partie à partir d'un PGN)
void Board::make_label_move(string s) {
    // char c = s[0];
    // int iterator = 0;
    // int i; int j; int k; int l;

    // if (isupper(c)) {
    // }
    // else {
    //     j = c - 97;
    //     iterator += 1;
    // }

    // make_move(i, j, k, l);

}


// Fonction qui renvoie un plateau à partir d'un PGN
void Board::from_pgn(string pgn) {
    _pgn = pgn;
}


// Fonction qui affiche un texte dans une zone donnée
void Board::draw_text_rect(string s, float pos_x, float pos_y, float width, float height, int size) {

    // Division du texte
    int sub_div = (1.5f * width) / size;

    if (width <= 0 || height <= 0 || sub_div <= 0)
        return;

    Rectangle rect_text = {pos_x, pos_y, width, height};
    DrawRectangleRec(rect_text, background_text_color);
    
    size_t string_size = s.length();
    const char *c;
    int i = 0;
    while (sub_div * i <= string_size) {
        c = s.substr(i * sub_div, sub_div).c_str();
        DrawTextEx(text_font, c, {pos_x, pos_y + i * size}, size, font_spacing * size, text_color);
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
    text_font = LoadFontEx("resources/fonts/SF TransRobotics.ttf", 32, 0, 250);
    // text_font = GetFontDefault();

    // Icône
    icon = LoadImage("resources/images/grogros_zero.png"); // TODO essayer de charger le .ico, pour que l'icone s'affiche tout le temps (pas seulement lors du build)
    SetWindowIcon(icon);
    UnloadImage(icon);

    // Grogros
    grogros_image = LoadImage("resources/images/grogros_zero.png");

    // Curseur
    cursor_image = LoadImage("resources/images/cursor.png");

    loaded_resources = true;
}


// Fonction qui met à la bonne taille les images et les textes de la GUI
void resize_gui() {
    int min_screen = min(_GUI._screen_height, _GUI._screen_width);
    board_size = board_scale * min_screen;
    board_padding_y = (_GUI._screen_height - board_size) / 4.0f;
    board_padding_x = (_GUI._screen_height - board_size) / 8.0f;

    tile_size = board_size / 8.0f;
    piece_size = tile_size * piece_scale;
    arrow_thickness = tile_size * arrow_scale;

    // Génération des textures

    // Pièces
    for (int i = 0; i < 12; i++) {
        ImageResize(&piece_images[i], piece_size, piece_size);
        piece_textures[i] = LoadTextureFromImage(piece_images[i]);
    }
    text_size = board_size / 16.0f;

    // Grogros
    grogros_size = board_size / 16.0f;
    ImageResize(&grogros_image, grogros_size, grogros_size);
    grogros_texture = LoadTextureFromImage(grogros_image);

    // Curseur
    ImageResize(&cursor_image, cursor_size, cursor_size);
    cursor_texture = LoadTextureFromImage(cursor_image);

    // Mini-pièces (pour le compte des pièces prises durant la partie)
    mini_piece_size = text_size / 3;
    for (int i = 0; i < 12; i++) {
        ImageResize(&mini_piece_images[i], mini_piece_size, mini_piece_size);
        mini_piece_textures[i] = LoadTextureFromImage(mini_piece_images[i]);
    }
}


// Fonction qui actualise les nouvelles dimensions de la fenêtre
void get_window_size() {
    _GUI._screen_width = GetScreenWidth();
    _GUI._screen_height = GetScreenHeight();
}


// Fonction qui dessine le plateau
bool Board::draw() {

    // Chargement des textures, si pas déjà fait
    if (!loaded_resources) {
        load_resources();
        resize_gui();
        PlaySound(game_begin_sound);
    }


    // Position de la souris
    mouse_pos = GetMousePosition();
    

    // Si on clique avec la souris
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        // Retire toute les cases surlignées
        remove_hilighted_tiles();

        // Retire toutes les flèches
        arrows_array = {};

        // Si on était pas déjà en train de cliquer (début de clic)
        if (!clicked) {

            // Stocke la case cliquée sur le plateau
            clicked_pos = get_pos_from_gui(mouse_pos.x, mouse_pos.y);
            clicked = true;

            // S'il y'a les flèches de réflexion de GrogrosZero, et qu'aucune pièce n'est sélectionnée
            if (drawing_arrows && !selected_piece()) {
                // On regarde dans le sens inverse pour jouer la flèche la plus récente (donc celle visible en cas de superposition)
                for (auto it = grogros_arrows.rbegin(); it != grogros_arrows.rend(); ++it) {
                    vector<int> arrow = *it;
                    if (arrow[2] == clicked_pos.first && arrow[3] == clicked_pos.second) {
                        // Retrouve le coup correspondant
                        play_move_sound(arrow[0], arrow[1], arrow[2], arrow[3]);
                        ((_GUI._click_bind && _GUI._board.click_i_move(_GUI._board.best_monte_carlo_move(), get_board_orientation())) || true) && play_monte_carlo_move_keep(arrow[4], true, true, true, true);
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
                    pre_move[0] = selected_pos.first;
                    pre_move[1] = selected_pos.second;
                    pre_move[2] = clicked_pos.first;
                    pre_move[3] = clicked_pos.second;
                    unselect();
                }
                
                // Si le coup est légal, le joue
                get_moves(false, true);
                for (int i = 0; i < _got_moves; i++) {
                    if (_moves[4 * i] == selected_pos.first && _moves[4 * i + 1] == selected_pos.second && _moves[4 * i + 2] == clicked_pos.first && _moves[4 * i + 3] == clicked_pos.second) {
                        play_move_sound(selected_pos.first, selected_pos.second, clicked_pos.first, clicked_pos.second);
                        ((_GUI._click_bind && _GUI._board.click_i_move(_GUI._board.best_monte_carlo_move(), get_board_orientation())) || true) && play_monte_carlo_move_keep(i, true, true, true, true);
                        break;
                    }
                }

                // Déselectionne
                unselect();

                // Changement de sélection de pièce
                if ((_player && is_in_fast(_array[clicked_pos.first][clicked_pos.second], 1, 6)) || (!_player && is_in_fast(_array[clicked_pos.first][clicked_pos.second], 7, 12)))
                    selected_pos = get_pos_from_gui(mouse_pos.x, mouse_pos.y);
                
            }
        }
        
    }
    else {
        // Si on clique
        if (clicked && clicked_pos.first != -1 && _array[clicked_pos.first][clicked_pos.second] != 0) {
            pair<uint_fast8_t, uint_fast8_t> drop_pos = get_pos_from_gui(mouse_pos.x, mouse_pos.y);
            if (is_in_fast(drop_pos.first, 0, 7) && is_in_fast(drop_pos.second, 0, 7)) {
                // Déselection de la pièce si on reclique dessus
                if (drop_pos.first == selected_pos.first && drop_pos.second == selected_pos.second) {
                }
                else {
                    int selected_piece = _array[selected_pos.first][selected_pos.second];
                    if (selected_piece > 0 && (selected_piece < 7 && !_player) || (selected_piece >= 7 && _player)) {
                        // Si c'est pas ton tour, pre-move
                        pre_move[0] = selected_pos.first;
                        pre_move[1] = selected_pos.second;
                        pre_move[2] = drop_pos.first;
                        pre_move[3] = drop_pos.second;
                        selected_pos = {-1, -1};
                    }


                    else {
                        // Si le coup est légal
                        get_moves(false, true);
                        for (int i = 0; i < _got_moves; i++) {
                            if (_moves[4 * i] == clicked_pos.first && _moves[4 * i + 1] == clicked_pos.second && _moves[4 * i + 2] == drop_pos.first && _moves[4 * i + 3] == drop_pos.second) {
                                play_move_sound(clicked_pos.first, clicked_pos.second, drop_pos.first, drop_pos.second);
                                // make_move(clicked_pos.first, clicked_pos.second, drop_pos.first, drop_pos.second, true, true);
                                ((_GUI._click_bind && _GUI._board.click_i_move(_GUI._board.best_monte_carlo_move(), get_board_orientation())) || true) && play_monte_carlo_move_keep(i, true, true, true, true);
                                selected_pos = {-1, -1};
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
    if (pre_move[0] != -1 && pre_move[1] != -1 && pre_move[2] != -1 && pre_move[3] != -1) {
        if ((!_player && is_in_fast(_array[pre_move[0]][pre_move[1]], 7, 12)) || (_player && is_in_fast(_array[pre_move[0]][pre_move[1]], 1, 6))) {
            if (_got_moves == -1)
                get_moves(false, true);
            for (int i = 0; i < _got_moves; i++) {
                if (_moves[4 * i] == pre_move[0] && _moves[4 * i + 1] == pre_move[1] && _moves[4 * i + 2] == pre_move[2] && _moves[4 * i + 3] == pre_move[3]) {
                    play_move_sound(pre_move[0], pre_move[1], pre_move[2], pre_move[3]);
                    ((_GUI._click_bind && _GUI._board.click_i_move(_GUI._board.best_monte_carlo_move(), get_board_orientation())) || true) && play_monte_carlo_move_keep(i, true, true, true, true);
                    break;
                }
            }
            pre_move[0] = -1;
            pre_move[1] = -1;
            pre_move[2] = -1;
            pre_move[3] = -1;
        }
        
        
    }


    // Si on fait un clic droit
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        int x_mouse = get_pos_from_gui(mouse_pos.x, mouse_pos.y).first;
        int y_mouse = get_pos_from_gui(mouse_pos.x, mouse_pos.y).second;
        right_clicked_pos = {x_mouse, y_mouse};

        // Retire les pre-moves
        pre_move[0] = -1;
        pre_move[1] = -1;
        pre_move[2] = -1;
        pre_move[3] = -1;
    }


    // Si on fait un clic droit (en le relachant)
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
        int x_mouse = get_pos_from_gui(mouse_pos.x, mouse_pos.y).first;
        int y_mouse = get_pos_from_gui(mouse_pos.x, mouse_pos.y).second;

        if (x_mouse != -1) {
            // Surlignage d'une case
            if (pair<int, int>{x_mouse, y_mouse} == right_clicked_pos)
                highlighted_array[x_mouse][y_mouse] = 1 - highlighted_array[x_mouse][y_mouse];
            
            // Flèche
            else {

                vector<int> arrow = {right_clicked_pos.first, right_clicked_pos.second, x_mouse, y_mouse};
                vector<vector<int>>::iterator found_arrow = find(arrows_array.begin(), arrows_array.end(), arrow);

                // Si la flèche existe, la supprime
                if (found_arrow != arrows_array.end())
                    arrows_array.erase(found_arrow);

                // Sinon, la rajoute
                else
                    arrows_array.push_back(arrow);
            }
        }

        
    }


    // Dessins

    // Couleur de fond
    ClearBackground(background_color);

    // Nombre de FPS
    DrawTextEx(text_font, ("FPS : " + to_string(GetFPS())).c_str(), {_GUI._screen_width - 3 * text_size, text_size / 3}, text_size / 3, font_spacing, text_color);


    // Plateau
    DrawRectangle(board_padding_x, board_padding_y, tile_size * 8, tile_size * 8, board_color_light);

    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            ((i + j) % 2 == 1) && DrawRectangle(board_padding_x + tile_size * j, board_padding_y + tile_size * i, tile_size, tile_size, board_color_dark);


    // Coordonnées sur le plateau
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) {
            if (j == 0 + 7 * board_orientation) // Chiffres
                DrawTextEx(text_font, to_string(i + 1).c_str(), {board_padding_x + text_size / 8, board_padding_y + tile_size * orientation_index(7 - i) + text_size / 8}, text_size / 2, font_spacing, ((i + j) % 2 == 1) ? board_color_light : board_color_dark);
            if (i == 0 + 7 * board_orientation) // Lettres
                DrawTextEx(text_font, abc8.substr(j, 1).c_str(), {board_padding_x + tile_size * (orientation_index(j) + 1) - text_size / 2, board_padding_y + tile_size * 8 - text_size / 2}, text_size / 2, font_spacing, ((i + j) % 2 == 1) ? board_color_light : board_color_dark);
        }    


    // Surligne du dernier coup joué
    if (_last_move[0] != -1) {
        DrawRectangle(board_padding_x + orientation_index(_last_move[1]) * tile_size, board_padding_y + orientation_index(7 - _last_move[0]) * tile_size, tile_size, tile_size, last_move_color);
        DrawRectangle(board_padding_x + orientation_index(_last_move[3]) * tile_size, board_padding_y + orientation_index(7 - _last_move[2]) * tile_size, tile_size, tile_size, last_move_color);
    }

    // Cases surglignées
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            if (highlighted_array[i][j])
                DrawRectangle(board_padding_x + tile_size * orientation_index(j), board_padding_y + tile_size * orientation_index(7 - i), tile_size, tile_size, highlight_color);

    // Pre-move
    if (pre_move[0] != -1 && pre_move[1] != -1 && pre_move[2] != -1 && pre_move[3] != -1) {
        DrawRectangle(board_padding_x + orientation_index(pre_move[1]) * tile_size, board_padding_y + orientation_index(7 - pre_move[0]) * tile_size, tile_size, tile_size, pre_move_color);
        DrawRectangle(board_padding_x + orientation_index(pre_move[3]) * tile_size, board_padding_y + orientation_index(7 - pre_move[2]) * tile_size, tile_size, tile_size, pre_move_color);
    }

    // Sélection de cases et de pièces
    if (selected_pos.first != -1) {
        // Affiche la case séléctionnée
        DrawRectangle(board_padding_x + orientation_index(selected_pos.second) * tile_size, board_padding_y + orientation_index(7 - selected_pos.first) * tile_size, tile_size, tile_size, select_color);
        // Affiche les coups possibles pour la pièce séléctionnée
        for (int i = 0; i < _got_moves; i++) {
            if (_moves[4 * i] == selected_pos.first && _moves[4 * i + 1] == selected_pos.second) {
                DrawRectangle(board_padding_x + orientation_index(_moves[4 * i + 3]) * tile_size, board_padding_y + orientation_index(7 - _moves[4 * i + 2]) * tile_size, tile_size, tile_size, select_color);
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
                        DrawTexture(piece_textures[p - 1], mouse_pos.x - piece_size / 2, mouse_pos.y - piece_size / 2, WHITE);
                    else
                        DrawTexture(piece_textures[p - 1], board_padding_x + tile_size * orientation_index(j) + (tile_size - piece_size) / 2, board_padding_y + tile_size * orientation_index(7 - i) + (tile_size - piece_size) / 2, WHITE);
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
                        DrawTexture(piece_textures[p - 1], mouse_pos.x - piece_size / 2.0f, mouse_pos.y - piece_size / 2.0f, WHITE);
                    else
                        DrawTexture(piece_textures[p - 1], board_padding_x + tile_size * (float)orientation_index(j) + (tile_size - piece_size) / 2.0f, board_padding_y + tile_size * (float)orientation_index(7 - i) + (tile_size - piece_size) / 2.0f, WHITE);
                }
            }
        }
    }

    // Flèches déssinées
    for (vector<int> arrow : arrows_array)
        draw_simple_arrow_from_coord(arrow[0], arrow[1], arrow[2], arrow[3], -1, arrow_color);



    // Titre
    DrawTextEx(text_font, "Grogros Chess", {board_padding_x + grogros_size / 2 + text_size / 2.8f, text_size / 4.0f}, text_size / 1.4f, font_spacing * text_size / 1.4f, text_color);

    // Grogros
    DrawTexture(grogros_texture, board_padding_x, text_size / 4.0f - text_size / 5.6f, WHITE);

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
    DrawTextEx(text_font, _GUI._black_player.c_str(), { (float)(x_mini_piece - t_size * 2), (float)(y_mini_piece_black - t_size) }, t_size, font_spacing * t_size, text_color);
    DrawTextEx(text_font, black_material.c_str(), { (float)(x_mini_piece - t_size * 2), (float)(y_mini_piece_black + t_size / 6) }, t_size, font_spacing * t_size, text_color_info);

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
    DrawTextEx(text_font, _GUI._white_player.c_str(), { (float)(x_mini_piece - t_size * 2), (float)(y_mini_piece_white - t_size) }, t_size, font_spacing * t_size, text_color);
    DrawTextEx(text_font, white_material.c_str(), { (float)(x_mini_piece - t_size * 2), (float)(y_mini_piece_white + t_size / 6) }, t_size, font_spacing * t_size, text_color_info);

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
    update_time();
    float x_pad = board_padding_x + board_size - text_size * 2;
    Color time_colors[4] = {(_GUI._time && !_player) ? BLACK : VDARKGRAY, (_GUI._time && !_player) ? WHITE : LIGHTGRAY, (_GUI._time && _player) ? WHITE : LIGHTGRAY, (_GUI._time && _player) ? BLACK : VDARKGRAY};
    DrawRectangle(x_pad, board_padding_y - text_size / 2 * board_orientation + board_size * !board_orientation, board_padding_x + board_size - x_pad, text_size / 2, time_colors[0]);
    DrawTextEx(text_font, clock_to_string(_GUI._time_black, false).c_str(), {x_pad + text_size / 6, board_padding_y - text_size / 2 * board_orientation + board_size * !board_orientation + text_size / 12}, text_size / 3, font_spacing * text_size / 3, time_colors[1]);
    DrawRectangle(x_pad, board_padding_y - text_size / 2 * !board_orientation + board_size * board_orientation, board_padding_x + board_size - x_pad, text_size / 2, time_colors[2]);
    DrawTextEx(text_font, clock_to_string(_GUI._time_white, false).c_str(), {x_pad + text_size / 6, board_padding_y - text_size / 2 * !board_orientation + board_size * board_orientation + text_size / 12}, text_size / 3, font_spacing * text_size / 3, time_colors[3]);

    // FEN
    if (_fen == "")
        to_fen();
    const char *fen = _fen.c_str();
    DrawTextEx(text_font, fen, {text_size / 2, board_padding_y + board_size + text_size * 3 / 2}, text_size / 3, font_spacing * text_size / 3, text_color_blue);


    // PGN
    slider_text(_pgn, text_size / 2, board_padding_y + board_size + text_size * 2, _GUI._screen_width - text_size, _GUI._screen_height - (board_padding_y + board_size + text_size * 2) - text_size / 3, text_size / 3, &pgn_slider, text_color);


    // Analyse de Monte-Carlo
    string monte_carlo_text = "Monte-Carlo research parameters : beta : " + to_string(_beta) + " | k_add : " + to_string(_k_add) + (!_GUI._grogros_analysis ? "\nrun GrogrosZero-Auto (CTRL-G)" : "\nstop GrogrosZero-Auto (CTRL-H)");
    if (_tested_moves && drawing_arrows && _monte_called) {
        // int best_eval = (_player) ? max_value(_eval_children, _tested_moves) : min_value(_eval_children, _tested_moves);
        int best_move = max_index(_nodes_children, _tested_moves);
        int best_eval = _eval_children[best_move];
        string eval;
        int mate = is_eval_mate(best_eval);
        if (mate != 0) {
            if (mate * _color > 0)
                eval = "+";
            else
                eval = "-";
            eval += "M";
            eval += to_string(abs(mate));
        }
            
        else
            eval = to_string(best_eval);

        global_eval = best_eval;
        global_eval_text = eval;

        // get_winning_chances();
        // eval += "\nW/D/L : " + to_string((int)(100 * _white_winning_chance)) + "/" + to_string((int)(100 * _drawing_chance)) + "/" + to_string((int)(100 * _black_winning_chance)) + "\%\n";


        // Pour l'évaluation statique
        evaluate_int(_evaluator, true, true);
        int max_depth = grogros_main_depth();
        int n_nodes = total_nodes();
        monte_carlo_text += "\n\n--- static eval : "  + to_string(_static_evaluation) + " ---\n" + eval_components + "\n--- dynamic eval : "  + eval + "---" + "\nnodes : " + int_to_round_string(n_nodes) + "/" + int_to_round_string(_monte_buffer._length) + " | time : " + clock_to_string(_time_monte_carlo) + " | speed : " + int_to_round_string(total_nodes() / (_time_monte_carlo + 1) * 1000) + "N/s" + " | depth : " + to_string(max_depth);


        // Affichage des paramètres d'analyse de Monte-Carlo
        slider_text(monte_carlo_text.c_str(), board_padding_x + board_size + text_size / 2, text_size, _GUI._screen_width - text_size - board_padding_x - board_size, board_size * 9 / 16,  text_size / 3, &monte_carlo_slider, text_color);

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
                    if (mate * _color > 0)
                        eval = "+";
                    else
                        eval = "-";
                    eval += "M";
                    eval += to_string(abs(mate));
                }
                else
                    eval = to_string(_eval_children[i]);
                
                string variant_i = _monte_buffer._heap_boards[_index_children[i]].get_monte_carlo_variant(true); // Peut être plus rapide
                // Ici aussi y'a qq chose qui ralentit, mais quoi?...
                monte_carlo_variants += "eval : " + eval + " | " + move_label_from_index(i) + variant_i + " (" + to_string(100.0 * _nodes_children[i] / n_nodes).substr(0, 5) + "% - " + int_to_round_string(_nodes_children[i]) + ")";
            }
            _monte_called = false;
        }

        // Affichage des variantes
        slider_text(monte_carlo_variants.c_str(), board_padding_x + board_size + text_size / 2, board_padding_y + board_size * 9 / 16 , _GUI._screen_width - text_size - board_padding_x - board_size, board_size / 2,  text_size / 3, &variants_slider);

        // Affichage de la barre d'évaluation
        draw_eval_bar(global_eval, global_eval_text, board_padding_x / 6, board_padding_y, 2 * board_padding_x / 3, board_size);
    }

    // Affichage des contrôles et autres informations
    else {
        
        // Touches
        static string keys_information = "CTRL-G : Start GrogrosZero analysis\nCTRL-H : Stop GrogrosZero analysis\n\n";

        // Binding chess.com
        static string binding_information;
        binding_information = "Binding chess.com:\n- Auto-click: " + (_GUI._click_bind ? (string)"enabled" : (string)"disabled") + "\n- Binding mode: " + (_GUI._binding_full ? (string)"analysis" : _GUI._binding_solo ? (string)"play" : "none");

        // Texte total
        static string controls_information;
        controls_information = "Controls :\n\n" + keys_information + binding_information;

        // TODO : ajout d'une valeur de slider
        slider_text(controls_information.c_str(), board_padding_x + board_size + text_size / 2, board_padding_y, _GUI._screen_width - text_size - board_padding_x - board_size, board_size, text_size / 3, 0, text_color_info);

    }

    // Affichage du curseur
    DrawTexture(cursor_texture, mouse_pos.x - cursor_size / 2, mouse_pos.y - cursor_size / 2, WHITE);

    return true;
}


// Fonction qui joue le son d'un coup
void Board::play_move_sound(int i, int j, int k, int l) {
    // Pièces
    int p1 = _array[i][j];
    int p2 = _array[k][l];

    // Echecs
    Board b(*this);
    b.make_move(i, j, k, l);

    int mate = b.is_mate();

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
void Board::play_index_move_sound(int i) {
    play_move_sound(_moves[4 * i], _moves[4 * i + 1], _moves[4 * i + 2], _moves[4 * i + 3]);
}


// Fonction qui obtient la case correspondante à la position sur la GUI
pair<int, int> get_pos_from_gui(float x, float y) {
    if (!is_in(x, board_padding_x, board_padding_x + board_size) || !is_in(y, board_padding_y, board_padding_y + board_size))
        return {-1, -1};
    else
        return {orientation_index(8 - (y - board_padding_y) / tile_size), orientation_index((x - board_padding_x) / tile_size)};
}


// Fonction qui permet de changer l'orientation du plateau
void switch_orientation() {
    board_orientation = !board_orientation;
}


// Fonction aidant à l'affichage du plateau (renvoie i si board_orientation, et 7 - i sinon)
int orientation_index(int i) {
    if (board_orientation)
        return i;
    return 7 - i;
}


// A partir de coordonnées sur le plateau
void draw_arrow_from_coord(int i1, int j1, int i2, int j2, int index, int color, float thickness, Color c, bool use_value, int value, int mate, bool outline) {
    if (thickness == -1.0)
        thickness = arrow_thickness;
    float x1 = board_padding_x + tile_size * orientation_index(j1) + tile_size /2;
    float y1 = board_padding_y + tile_size * orientation_index(7 - i1) + tile_size /2;
    float x2 = board_padding_x + tile_size * orientation_index(j2) + tile_size /2;
    float y2 = board_padding_y + tile_size * orientation_index(7 - i2) + tile_size /2;

    // Transparence nulle
    c.a = 255;

    // Outline pour le coup choisi
    if (outline) {
        if (abs(j2 - j1) != abs(i2 - i1))
            DrawLineBezier(x1, y1, x2, y2, thickness * 1.4f, BLACK);
        else
            DrawLineEx(x1, y1, x2, y2, thickness * 1.4f, BLACK);
        DrawCircle(x1, y1, thickness * 1.2f, BLACK);
        DrawCircle(x2, y2, thickness * 2.0f * 1.1f, BLACK);
    }
    
    // "Flèche"
    if (abs(j2 - j1) != abs(i2 - i1))
        DrawLineBezier(x1, y1, x2, y2, thickness, c);
    else
        DrawLineEx(x1, y1, x2, y2, thickness, c);
    DrawCircle(x1, y1, thickness, c);
    DrawCircle(x2, y2, thickness * 2.0f, c);

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
            sprintf(v, "%d", value);
        }
            
        float size = thickness * 1.5f;
        float max_size = thickness * 3.25f;
        float width = MeasureTextEx(text_font, v, size, font_spacing * size).x;
        if (width > max_size) {
            size = size * max_size / width;
            width = MeasureTextEx(text_font, v, size, font_spacing * size).x;
        }
        float height = MeasureTextEx(text_font, v, size, font_spacing * size).y;

        Color t_c = ColorAlpha(BLACK, (float)c.a / 255.0f);
        DrawTextEx(text_font, v, {x2 - width / 2.0f, y2 - height / 2.0f}, size, font_spacing * size, BLACK);
        
    }

    // Ajoute la flèche au vecteur
    grogros_arrows.push_back({i1, j1, i2, j2, index});

    return;

}


// Fonction qui dessine les flèches en fonction des valeurs dans l'algo de Monte-Carlo d'un plateau
void Board::draw_monte_carlo_arrows() {
    // get_moves(false, true);

    grogros_arrows = {};

    int best_move = best_monte_carlo_move();

    int sum_nodes = 0;
    for (int i = 0; i < _tested_moves; i++)
        sum_nodes += _nodes_children[i];
    int mate;

    // Ordonnancement des coups pour le dessin des flèches
    vector<int> ordered_moves = sort_by_nodes(true);
    int i;

    // Une pièce est-elle sélectionnée?
    bool is_selected = selected_pos.first != -1 && selected_pos.second != -1;

    for (int j = 0; j < _tested_moves; j++) {
        i = ordered_moves[j];
        mate = is_eval_mate(_eval_children[i]);
        // Si une pièce est sélectionnée
        if (is_selected) {
            // Dessine pour la pièce sélectionnée
            if (selected_pos.first == _moves[4 * i] && selected_pos.second == _moves[4 * i + 1])
                draw_arrow_from_coord(_moves[4 * i], _moves[4 * i + 1], _moves[4 * i + 2], _moves[4 * i + 3], i, _color, -1.0, move_color(_nodes_children[i], sum_nodes), true, _eval_children[i], mate, i == best_move);
        }
        else {
            float n = _nodes_children[i];
            if (n / (float)sum_nodes > arrow_rate)
                draw_arrow_from_coord(_moves[4 * i], _moves[4 * i + 1], _moves[4 * i + 2], _moves[4 * i + 3], i, _color, -1.0, move_color(_nodes_children[i], sum_nodes), true, _eval_children[i], mate, i == best_move);
        }
    }
}


// Fonction qui calcule l'activité des pièces
void Board::get_piece_activity(bool legal) {

    if (_activity)
        return;

    Board b;
    b.copy_data(*this);
    _piece_activity = 0;

    static const int activity_values[21] = {-200, 100, 142, 183, 223, 261, 298, 332, 364, 393, 419, 442, 461, 478, 492, 504, 513, 520, 525, 528, 529};

    // Fait un tableau de toutes les pièces : position, valeur
    int piece_move_count[64] = {0};
    int index = 0;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (_array[i][j] != 0) {
                piece_move_count[index] = 0;
                index++;
            }
        }
    }

    // Activité des pièces du joueur
    // TODO : ça doit être très lent : on re-calcule tous les coups à chaque fois... (et on les garde même pas en mémoire pour après, car c'est sur un plateau virtuel)
    // En plus ça calcule aussi les coups de l'autre
    b.get_moves(false, legal);

    // Pour chaque coup, incrémente dans le tableau le nombre de coup à la position correspondante
    for (int i = 0; i < b._got_moves; i++) {
        int pos = b._moves[4 * i] * 8 + b._moves[4 * i + 1];
        piece_move_count[pos]++;
    }

    // Activité des pièces de l'autre joueur
    b._player = !b._player; b._got_moves = -1;
    b.get_moves(false, legal);

    for (int i = 0; i < b._got_moves; i++) {
        int pos = b._moves[4 * i] * 8 + b._moves[4 * i + 1];
        piece_move_count[pos]++;
    }

    // Pour chaque pièce : ajoute la valeur correspondante à l'activité
    index = 0;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (_array[i][j] != 0) {
                int pos = i * 8 + j;
                _piece_activity += (_array[i][j] < 7 ? 1 : -1) * activity_values[min(20, piece_move_count[pos])];
                index++;
            }
        }
    }

    _activity = true;
}



// Couleur de la flèche en fonction du coup (de son nombre de noeuds)
Color move_color(int nodes, int total_nodes) {
    float x = (float)nodes / (float)total_nodes;

    unsigned char red = 255.0 * ((x <= 0.2) + (x > 0.2 && x < 0.4) * (0.4 - x) / 0.2 + (x > 0.8) * (x - 0.8) / 0.2);
    unsigned char green = 255.0 * ((x < 0.2) * x / 0.2 + (x >= 0.2 && x <= 0.6) + (x > 0.6 && x < 0.8) * (0.8 - x) / 0.2);
    unsigned char blue = 255.0 * ((x > 0.4 && x < 0.6) * (x - 0.4) / 0.2 + (x >= 0.6));

    unsigned char alpha = 100 + 155 * nodes / total_nodes;

    return {red, green, blue, alpha};
}


// Fonction qui renvoie le meilleur coup selon l'analyse faite par l'algo de Monte-Carlo
int Board::best_monte_carlo_move() {
    return max_index(_nodes_children, _tested_moves, _eval_children, _color);
}


// Fonction qui joue le coup après analyse par l'algo de Monte Carlo, et qui garde en mémoire les infos du nouveau plateau
bool Board::play_monte_carlo_move_keep(int m, bool keep, bool keep_display, bool display, bool add_to_list) {
    if (_got_moves == -1)
        get_moves(false, true);

    // Il faut obtenir le vrai coup (correspondant aux plateaux fils de l'algo de Monte-Carlo)
    // Pour le moment c'est pas beau, il faudra changer ça à l'avenir
    // TODO
    Move wanted_move;
    wanted_move.x1 = _moves[4 * m];
    wanted_move.y1 = _moves[4 * m + 1];
    wanted_move.x2 = _moves[4 * m + 2];
    wanted_move.y2 = _moves[4 * m + 3];

    Move child_move;
    int move = m;
    for (int i = 0; i < _tested_moves; i++) {
        int last_child_move[4];
        copy(_monte_buffer._heap_boards[_index_children[i]]._last_move, _monte_buffer._heap_boards[_index_children[i]]._last_move + 4, last_child_move);
        child_move.x1 = last_child_move[0];
        child_move.y1 = last_child_move[1];
        child_move.x2 = last_child_move[2];
        child_move.y2 = last_child_move[3];
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
            b.to_fen();
            if (display) {
                b.display_pgn();
                b.to_fen();
                cout << "***** FEN : " << b._fen << " *****" << endl;
            }
            if (_is_active) {
                _monte_buffer._heap_boards[_index_children[move]]._pgn = b._pgn;
                _monte_buffer._heap_boards[_index_children[move]]._timed_pgn = _timed_pgn;
                _monte_buffer._heap_boards[_index_children[move]]._named_pgn = _named_pgn;
            }
                
        }

        // Deletes all the children from the other boards
        for (int i = 0; i < _tested_moves; i++)
            if (i != move) {
                if (_is_active)
                    _monte_buffer._heap_boards[_index_children[i]].reset_all();
            }

        if (_is_active) {
            Board *b = &_monte_buffer._heap_boards[_index_children[move]];
            reset_board(true);
            *this = *b;
            update_time();
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
int Board::max_monte_carlo_depth() {
    int max_depth = 0;
    int depth;
    for (int i = 0; i < _tested_moves; i++) {
        depth = _monte_buffer._heap_boards[_index_children[i]].max_monte_carlo_depth() + 1;
        if (depth > max_depth)
            max_depth = depth;
    }

    return max_depth;
}


// Valeurs de base pour Grogros
float _beta = 0.05f;
float _k_add = 25.0f;


// Constructeur par défaut
Buffer::Buffer() {

    // Crée un gros buffer, de 4GB
    unsigned long int _size_buffer = 4000000000;
    _length = _size_buffer / sizeof(Board);
    _length = 0;

    _heap_boards = new Board[_length];

}


// Constructeur utilisant la taille max (en bits) du buffer
Buffer::Buffer(unsigned long int size) {

    _length = size / sizeof(Board);
    _heap_boards = new Board[_length];

}


// Initialize l'allocation de n plateaux
void Buffer::init(int length) {

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
        cout << "approximate buffer size : " << long_int_to_round_string(_monte_buffer._length * sizeof(Board)) << "b" << endl;
        
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
}


// Buffer pour l'algo de Monte-Carlo
Buffer _monte_buffer;


// Algo de grogros_zero
void Board::grogros_zero(Evaluator *eval, int nodes, bool checkmates, float beta, float k_add, bool display, int depth, Network *net) {
    static int max_depth;
    static int n_positions = 0;
    
    _monte_called = true;

    _is_active = true;

    clock_t begin_monte_time = clock();

    if (_new_board && depth == 0) {
        max_depth = 0;
        if (!_evaluated)
            evaluate_int(eval, true, false, net); 
    }

    if (depth == 0) {
        int n = total_nodes();
        if (_monte_buffer._length - n < nodes) {
            if (display)
                cout << "buffer is full" << endl;
            nodes = _monte_buffer._length - n;
        }
    }
        

    if (depth > max_depth) {
        max_depth = depth;
        if (display) {
            cout << "GrogrosZero - depth : " << max_depth << '\r';
            to_fen();
            cout << _fen << endl;
        }
    }

    // Si la partie est finie, évite le calcul des coups... bizarre aussi : ne plus rentrer dans cette ligne?
    if (_is_game_over) {
        _nodes++; // un peu bizarre mais bon... revoir les cas où y'a des mats
        _time_monte_carlo += clock() - begin_monte_time;
        return;
    }


    // Obtention des coups jouables
    (false || _got_moves == -1) && _new_board && get_moves(false, true); // A faire à chaque fois? (sinon, mettre à false) -> à mettre seulement si new_board??
    (!_quick_sorted_moves) && quick_moves_sort();

    // TODO
    
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

        _eval_children = new int[_got_moves];
        _nodes_children = new int[_got_moves];
        _index_children = new int[_got_moves]; // à changer? cela prend du temps?

        for (int i = 0; i < _got_moves; i++) {
            _nodes_children[i] = 0;
            _eval_children[i] = 0;
        }

        n_positions += _got_moves;
        _tested_moves = 0;
        _current_move = 0;
        _new_board = false;
    }

    if (_got_moves == 0) {
        _nodes++; // un peu bizarre mais bon... revoir les cas où y'a des mats
        _time_monte_carlo += clock() - begin_monte_time;
        return;
    }


    // Tant qu'il reste des noeuds à calculer...
    while (nodes > 0) {

        // Choix du coup à jouer pour l'exploration

        // Si tous les coups de la position ne sont pas encore testés
        if (_tested_moves < _got_moves) {

            // Prend une nouvelle place dans le buffer
            int index = _monte_buffer.get_first_free_index();
            if (index == -1) {
                return;
            }
            _index_children[_current_move] = index;
            _monte_buffer._heap_boards[_index_children[_current_move]]._is_active = true;

            // Joue un nouveau coup
            _monte_buffer._heap_boards[_index_children[_current_move]].copy_data(*this);
            _monte_buffer._heap_boards[_index_children[_current_move]].make_index_move(_current_move);
            
            // Evalue une première fois la position, puis stocke dans la liste d'évaluation des coups
            //_monte_buffer._heap_boards[_index_children[_current_move]].evaluate_int(eval, checkmates, false, net);
            
            _monte_buffer._heap_boards[_index_children[_current_move]]._evaluation = _monte_buffer._heap_boards[_index_children[_current_move]].quiescence(eval, -2147483647, 2147483647) * -_color;
            _monte_buffer._heap_boards[_index_children[_current_move]]._got_moves = -1; // FIXME euuuuh pourquoi ça bug sinon?

            _eval_children[_current_move] = _monte_buffer._heap_boards[_index_children[_current_move]]._evaluation;
            _nodes_children[_current_move]++;

            // Actualise la valeur d'évaluation du plateau

            // Première évaluation // FIXME? ça ne devrait jamais aller ici...
            if (!_evaluated) {
                _evaluation = _eval_children[_current_move];
                _evaluated = true;
            }
            
            if (_player) {
                if (_eval_children[_current_move] > _evaluation)
                    _evaluation = _eval_children[_current_move];
            }
            else {
                if (_eval_children[_current_move] < _evaluation)
                    _evaluation = _eval_children[_current_move];
            }

            // Incrémentation des coups
            _current_move++; 
            _tested_moves++;

        }

        // Lorsque tous les coups de la position ont déjà été testés (et évalués)
        else {
            // Choisit aléatoirement un "bon" coup
            _current_move = pick_random_good_move(_eval_children, _got_moves, _color, false, _nodes, _nodes_children, beta, k_add);
            //_current_move = select_uct();

            // Va une profondeur plus loin... appel récursif sur Monte-Carlo
           _monte_buffer._heap_boards[_index_children[_current_move]].grogros_zero(eval, 1, checkmates, beta, k_add, display, depth + 1, net);

            // Actualise l'évaluation
            _eval_children[_current_move] = _monte_buffer._heap_boards[_index_children[_current_move]]._evaluation;
            _nodes_children[_current_move]++;

            if (_player)
                _evaluation = max_value(_eval_children, _got_moves);
            else
                _evaluation = min_value(_eval_children, _got_moves);

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
void Board::reset_board(bool display) {

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
    _fen = "";
    _pgn = "";
    
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

    /*if (display)
        cout << "board reset done" << endl;*/
    
    return;
}


// Fonction qui réinitialise tous les plateaux fils dans le buffer
void Board::reset_all(bool self, bool display) {
    for (int i = 0; i < _tested_moves; i++)
        _monte_buffer._heap_boards[_index_children[i]].reset_all(false);

    reset_board();

    /*if (self && display)
        cout << "boards reset done" << endl;*/

    return;
}


// Fonction qui renvoie le nombre de noeuds calculés par GrogrosZero ou Monte-Carlo
int Board::total_nodes() {

    int nodes = 0;

    for (int i = 0; i < _tested_moves; i++)
        nodes += _nodes_children[i];

    return nodes;
}


// Fonction qui calcule la sécurité des rois
void Board::get_king_safety(int piece_attack, int piece_defense, int pawn_attack, int pawn_defense, int edge_defense) {
    // TODO est cassé


    if (_safety)
        return;

    // Position des rois
    int w_king_i;
    int w_king_j;
    int b_king_i;
    int b_king_j;

    bool w_king = false;
    bool b_king = false;

    int p;

    // Recherche la position des rois
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];

            if (!w_king && p == 6) {
                w_king_i = i;
                w_king_j = j;
                w_king = true;
            }

            if (!b_king && p == 12) {
                b_king_i = i;
                b_king_j = j;
                b_king = true;
            }

            if (w_king && b_king)
                goto kings;
        }

    kings:

    // TODO : piece_attack/defense depending on the piece

    // Valeurs de protection et attaque envers un roi en fonction de la position relative entre la pièce et le roi

    // Protection d'un pion (le roi se situe en 2, 1)
    // TODO vérifier la symétrie horizontale dans le cas où on a les noirs
    static const int pawn_protection_map[3][3] = {
        {75, 125, 75},
        {150, 250, 150},
        {100,  0, 100}
    };

    // TODO : faire une fonction qui en fonction de la pièce et le roi, renvoie la valeur de relation entre les deux



    float proximity_pawn_defense = 2;

    // Faiblesses des rois
    float w_king_weakness = 0.0;
    float b_king_weakness = 0.0;

    float w_king_protection = 0.0;
    float b_king_protection = 0.0;

    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            if (p > 0) {
                if (p < 6) {
                    if (p == 1) {
                        abs(i - w_king_i - 1) <= 1 && abs(j - w_king_j) <= 1 && (w_king_protection += pawn_protection_map[2 - (i - w_king_i)][j - w_king_j + 1]);
                        b_king_weakness += pawn_attack * proximity(i, j, b_king_i, b_king_j, 3);
                    }   
                    else {
                        w_king_protection += piece_defense * proximity(i, j, w_king_i, w_king_j, 4);
                        b_king_weakness += piece_attack * proximity(i, j, b_king_i, b_king_j, 7);
                    }
                    
                } 
                else if (p > 6 && p < 12) {
                    if (p == 7) {
                        w_king_weakness += pawn_attack * proximity(i, j, w_king_i, w_king_j, 3);
                        abs(b_king_i - i - 1) <= 1 && abs(j - b_king_j) <= 1 && (b_king_protection += pawn_protection_map[2 - (b_king_i - i)][j - b_king_j + 1]);
                    }   
                    else {
                        w_king_weakness += piece_attack * proximity(i, j, w_king_i, w_king_j, 7);
                        b_king_protection += piece_defense * proximity(i, j, b_king_i, b_king_j, 4);
                    }
                }
            }
        }

    // Il faut compter les cases vides (non-pion) autour de lui

    // Droits de roque
    w_king_protection += (_castling_rights.k_w + _castling_rights.q_w) * 100;
    b_king_protection += (_castling_rights.k_b + _castling_rights.q_b) * 100;

    // Niveau de protection auquel on peut considérer que le roi est safe
    float king_base_protection = 200;
    // king_base_protection = 0;
    w_king_protection -= king_base_protection;
    b_king_protection -= king_base_protection;
    

    // Proximité avec le bord
    // Avancement à partir duquel il est plus dangereux d'être sur un bord
    float edge_adv = 0.75; float mult_endgame = 2;
    
    
    // Calcul de safety du roi
    // Facteur additif pour les multiplications (pour rendre ça plus linéaire)
    int mult_add = 0;

    // Version multiplicative
    // w_king_weakness += edge_defense / (mult_add + 1) / (mult_add + 1) * (min(abs(w_king_i - (-1)), abs((w_king_i) - 8)) + mult_add) * (min(abs(w_king_j - (-1)), abs((w_king_j) - 8)) + mult_add) * (edge_adv - _adv) * (_adv < edge_adv ? 1 / edge_adv : mult_endgame / (1 - edge_adv));
    // b_king_weakness += edge_defense / (mult_add + 1) / (mult_add + 1) * (min(abs(b_king_i - (-1)), abs((b_king_i) - 8)) + mult_add) * (min(abs(b_king_j - (-1)), abs((b_king_j) - 8)) + mult_add) * (edge_adv - _adv) * (_adv < edge_adv ? 1 / edge_adv : mult_endgame / (1 - edge_adv));

    // Version additive
    // w_king_weakness += max_int(150, edge_defense * (min(w_king_i, 7 - w_king_i) + min(w_king_j, 7 - w_king_j)) * (edge_adv - _adv) * (_adv < edge_adv ? 1 / edge_adv : - mult_endgame / (1 - edge_adv))) - 150;
    // b_king_weakness += max_int(150, edge_defense * (min(b_king_i, 7 - b_king_i) + min(b_king_j, 7 - b_king_j)) * (edge_adv - _adv) * (_adv < edge_adv ? 1 / edge_adv : - mult_endgame / (1 - edge_adv))) - 150;

    // Version additive, adaptée pour l'endgame
    const int endgame_safe_zone = 16; // Si le "i * j" du roi en endgame est supérieur, alors il n'est pas en danger : s'il est en c4 (2, 3 -> (2 + 1) * (3 + 1) = 12 < 16 -> danger)
    w_king_weakness += max_int(150, edge_defense * (edge_adv - _adv) * ((_adv < edge_adv) ? min(w_king_i, 7 - w_king_i) + min(w_king_j, 7 - w_king_j) : endgame_safe_zone - ((min(w_king_i, 7 - w_king_i) + 1) * (min(w_king_j, 7 - w_king_j) + 1))) * mult_endgame / (edge_adv - 1)) - 150;
    b_king_weakness += max_int(150, edge_defense * (edge_adv - _adv) * ((_adv < edge_adv) ? min(b_king_i, 7 - b_king_i) + min(b_king_j, 7 - b_king_j) : endgame_safe_zone - ((min(b_king_i, 7 - b_king_i) + 1) * (min(b_king_j, 7 - b_king_j) + 1))) * mult_endgame / (edge_adv - 1)) - 150;

    
    // Ajout de la protection du roi... la faiblesse du roi ne peut pas être négative (potentiellement à revoir, mais parfois la surprotection donne des valeurs délirantes)
    float w_king_over_protection = max_float(0.0, w_king_protection - w_king_weakness);
    float b_king_over_protection = max_float(0.0, b_king_protection - b_king_weakness);
    w_king_weakness = max_float(0.0, w_king_weakness - w_king_protection);
    b_king_weakness = max_float(0.0, b_king_weakness - b_king_protection);


    // Force de la surprotection du roi
    float overprotection = 0.10f;

    // Potentiel d'attaque de chaque pièce (pion, caval, fou, tour, dame)
    static const int attack_potentials[6] = {1, 25, 28, 30, 100, 0};
    const int reference_potential = 258; // Si y'a toutes les pièces de base sur l'échiquier
    int w_total_potential = 0;
    int b_total_potential = 0;

    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            if (p > 0)
                if (p < 7)
                    w_total_potential += attack_potentials[p - 1];
                else
                    b_total_potential += attack_potentials[(p - 1) % 6];
        }

    _king_safety = b_king_weakness * w_total_potential / reference_potential - w_king_weakness * b_total_potential / reference_potential;

    _king_safety += overprotection * (w_king_over_protection - b_king_over_protection);

    _safety = true;

}


// Fonction qui renvoie s'il y a échec et mat, pat, ou rien (1 pour mat, 0 pour pat, -1 sinon)
int Board::is_mate() {

    if (_mate_checked)
        return _mate_value;

    _mate_checked = true;

    // Pour accélérer en ne re calculant pas forcément les coups (marche avec coups légaux OU illégaux)
    int half_moves = _half_moves_count;
    _half_moves_count = 0;

    int moves = _got_moves;

    if (_got_moves == -1)
        get_moves();

    Board b;

    for (int i = 0; i < _got_moves; i++) {
        b.copy_data(*this);
        b.make_index_move(i);
        b._player = _player;
        b._color = _color;
        if (!b.in_check()) {
            _half_moves_count = half_moves;
            _got_moves = moves;
            _mate_value = -1;
            return -1; 
        }
            
    }

    if (in_check()) {
        _half_moves_count = half_moves;
        _got_moves = moves;
        _mate_value = 1;
        return 1;  
    }
              
    _got_moves = moves;
    _half_moves_count = half_moves;
    _mate_value = 0;
    return 0;
    
}


// Fonction qui dit si une pièce est capturable par l'ennemi (pour les affichages GUI)
bool Board::is_capturable(int i, int j) {
    _got_moves == -1 && get_moves(false, true);

    for (int k = 0; k < _got_moves; k++)
        if (_moves[4 * k + 2] == i && _moves[4 * k + 3] == j)
            return true;

    return false;
}


// Fonction qui renvoie si le joueur est en train de jouer (pour que l'IA arrête de réflechir à ce moment sinon ça lagge)
bool is_playing() {
    Vector2 new_mouse_pos = GetMousePosition();
    return (selected_pos.first != -1 || new_mouse_pos.x != mouse_pos.x || new_mouse_pos.y != mouse_pos.y);
}


// Fonction qui change le mode d'affichage des flèches (oui/non)
void switch_arrow_drawing() {
    drawing_arrows = !drawing_arrows;
}


// Fonction qui affiche le PGN
void Board::display_pgn() {
    cout << "\n***** PGN *****\n" << _pgn << "\n***** PGN *****" << endl;
}

// Fonction qui ajoute les noms des gens au PGN
void Board::add_names_to_pgn() {
    if (_named_pgn) {
        // Change le nom du joueur aux pièces blanches
        size_t p_white = _pgn.find("[White ") + 8;
        size_t p_white_2 = _pgn.find("\"]");
        _pgn = _pgn.substr(0, p_white) + _GUI._white_player + _pgn.substr(p_white_2);

        // Change le nom du joueur aux pièces noires
        size_t p_black = _pgn.find("[Black ") + 8;
        size_t p_black_2 = _pgn.find("\"]", p_black);
        _pgn = _pgn.substr(0, p_black) + _GUI._black_player + _pgn.substr(p_black_2);
    }

    else {
        size_t p = _pgn.find_last_of("\"]\n");
        if (p == -1)
            _pgn = "[White \"" + _GUI._white_player + "\"]\n" + "[Black \"" + _GUI._black_player + "\"]\n\n" + _pgn;
        else
            _pgn = "[White \"" +_GUI._white_player + "\"]\n" + "[Black \"" + _GUI._black_player + "\"]\n" + _pgn;
        _named_pgn = true;
    }
}


// Fonction qui ajoute le time control au PGN
void Board::add_time_to_pgn() {
    if (_timed_pgn) {
        // cout << "déjà fait !" << endl;
        true;
    }
    
    else {
        size_t p = _pgn.find_last_of("\"]\n");
        if (p == -1)
            _pgn = "[TimeControl \"" + to_string((int)(max(_GUI._time_white, _GUI._time_black) / 1000)) + " + " + to_string((int)(max(_GUI._time_increment_white, _GUI._time_increment_black) / 1000)) +"\"]\n\n" + _pgn;
        else
            _pgn = _pgn.substr(0, p) + "[TimeControl \"" + to_string((int)(max(_GUI._time_white, _GUI._time_black) / 1000)) + " + " + to_string((int)(max(_GUI._time_increment_white, _GUI._time_increment_black) / 1000)) +"\"]\n" + _pgn.substr(p);
        

        _timed_pgn = true;
    }
}


// Fonction qui renvoie en chaîne de caractères la meilleure variante selon monte carlo
string Board::get_monte_carlo_variant(bool evaluate_final_pos) {
    string s = "";

    if ((_got_moves == -1 && !_is_game_over) || _got_moves == 0)
        return s;
    if (_is_game_over)
        return s;

    int move = best_monte_carlo_move();
    s += " " + move_label_from_index(move);

    if (_tested_moves == _got_moves)
        return s + _monte_buffer._heap_boards[_index_children[move]].get_monte_carlo_variant(evaluate_final_pos);

    if (evaluate_final_pos)
        s += " | " + to_string(_monte_buffer._heap_boards[_index_children[move]]._static_evaluation);

    return s;

}


// Fonction qui trie les index des coups par nombre de noeuds décroissant
vector<int> Board::sort_by_nodes(bool ascending) {
    // Tri assez moche, et lent (tri par insertion)
    vector<int> sorted_indexes;
    vector<int> sorted_nodes;

    for (int i = 0; i < _tested_moves; i++) {
        for (int j = 0; j <= sorted_indexes.size(); j++) {
            if (j == sorted_indexes.size()) {
                sorted_indexes.push_back(i);
                sorted_nodes.push_back(_monte_buffer._heap_boards[_index_children[i]]._nodes);
                break;
            }
            if ((!ascending && _monte_buffer._heap_boards[_index_children[i]]._nodes > sorted_nodes[j]) || (ascending && _monte_buffer._heap_boards[_index_children[i]]._nodes < sorted_nodes[j])) {
                sorted_indexes.insert(sorted_indexes.begin() + j, i);
                sorted_nodes.insert(sorted_nodes.begin() + j, _monte_buffer._heap_boards[_index_children[i]]._nodes);
                break;
            }
        }
    }

    return sorted_indexes;
}


// Fonction qui renvoie selon l'évaluation si c'est un mat ou non
int Board::is_eval_mate(int e) {
    if (e > 100000)
        return (100000000 - e) / 100000 - _moves_count + _player; // (Immonde) à changer...
    if (e < -100000)
        return - ((100000000 + e) / 100000 - _moves_count);
    else
        return 0;
}


// Fonction qui affiche un texte dans une zone donnée avec un slider
void slider_text(string s, float pos_x, float pos_y, float width, float height, int size, float *slider_value, Color t_color, float slider_width, float slider_height) {

    Rectangle rect_text = {pos_x, pos_y, width, height};
    DrawRectangleRec(rect_text, background_text_color);

    string new_string = "";
    float new_width = width - slider_width;


    // Pour chaque bout de texte
    std::stringstream ss(s);
    std::string line;

    while (getline(ss, line, '\n')) {

        // Taille horizontale du texte
        float horizontal_text_size = MeasureTextEx(text_font, line.c_str(), size, font_spacing * size).x;

        // Split le texte en parties égales
        if (horizontal_text_size > new_width) {
            int split_length = line.length() * new_width / horizontal_text_size;
            for (int i = split_length - 1; i < line.length() - 1; i += split_length)
                line.insert(i, "\n");
        }

        new_string += line + "\n";

    }


    // Taille verticale totale du texte
    float vertical_text_size = MeasureTextEx(text_font, new_string.c_str(), size, font_spacing * size).y;

    // Si le texte prend plus de place verticalement que l'espace alloué
    if (vertical_text_size > height) {

        int n_lines;

        // Nombre de lignes total
        int total_lines = 1;
        for (int i = 0; i < new_string.length() - 1; i++) {
            if (new_string.substr(i, 1) == "\n")
                total_lines++;                
        }

        n_lines = total_lines * height / MeasureTextEx(text_font, new_string.c_str(), size, font_spacing * size).y;

        int starting_line = (total_lines - n_lines) * *slider_value;

        string final_text = "";
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
        Rectangle slider_background_rect = {pos_x + width - slider_width, pos_y, slider_width, height};
        DrawRectangleRec(slider_background_rect, slider_backgrond_color);

        // Slider
        Rectangle slider_rect = {pos_x + width - slider_width, pos_y + *slider_value * (height - slider_height), slider_width, slider_height};
        DrawRectangleRec(slider_rect, slider_color);



        // Slide

        // Avec la molette
        if (is_cursor_in_rect({pos_x, pos_y, width, height})) {
            *slider_value -= GetMouseWheelMove() * GetFrameTime() * 100 / (total_lines - n_lines);
            if (*slider_value < 0.0f)
                *slider_value = 0.0f;
            if (*slider_value > 1.0f)
                *slider_value = 1.0f;
        }
    }
        


    // Texte total
    const char *c = new_string.c_str();

    DrawTextEx(text_font, c, {pos_x, pos_y}, size, font_spacing * size, t_color);
    
    

}


// Fonction pour obtenir l'orientation du plateau
bool get_board_orientation() {
    return board_orientation;
}


// Fonction qui renvoie si le curseur de la souris se trouve dans le rectangle
bool is_cursor_in_rect(Rectangle rec) {
    mouse_pos = GetMousePosition();
    return (is_in(mouse_pos.x, rec.x, rec.x + rec.width) && is_in(mouse_pos.y, rec.y, rec.y + rec.height));
}


// Fonction qui dessine un rectangle à partir de coordonnées flottantes
bool DrawRectangle(float posX, float posY, float width, float height, Color color) {
    DrawRectangle(float_to_int(posX), float_to_int(posY), float_to_int(width + posX) - float_to_int(posX), float_to_int(height + posY) - float_to_int(posY), color);
    return true;
}


// Fonction qui dessine un rectangle à partir de coordonnées flottantes, en fonction des coordonnées de début et de fin
bool DrawRectangleFromPos(float posX1, float posY1, float posX2, float posY2, Color color) {
    DrawRectangle(float_to_int(posX1), float_to_int(posY1), float_to_int(posX2) - float_to_int(posX1), float_to_int(posY2) - float_to_int(posY1), color);
    return true;
}

// Fonction qui dessine un cercle à partir de coordonnées flottantes
void DrawCircle(float posX, float posY, float radius, Color color) {
    DrawCircle(float_to_int(posX), float_to_int(posY), radius, color);
}


// Fonction qui dessine une ligne à partir de coordonnées flottantes
void DrawLineEx(float x1, float y1, float x2, float y2, float thick, Color color) {
    DrawLineEx({(float)float_to_int(x1), (float)float_to_int(y1)}, {(float)float_to_int(x2), (float)float_to_int(y2)}, thick, color);
}


// Fonction qui dessine une ligne de Bézier à partir de coordonnées flottantes
void DrawLineBezier(float x1, float y1, float x2, float y2, float thick, Color color) {
    DrawLineBezier({(float)float_to_int(x1), (float)float_to_int(y1)}, {(float)float_to_int(x2), (float)float_to_int(y2)}, thick, color);
}


// Fonction qui dessine une texture à partir de coordonnées flottantes
void DrawTexture(Texture texture, float posX, float posY, Color color) {
    DrawTexture(texture, float_to_int(posX), float_to_int(posY), color);
}


// Fonction qui joue un match entre deux IA utilisant GrogrosZero, et une évaluation par réseau de neurones et renvoie le résultat de la partie (1/-1/0)
int match(Evaluator *e_white, Evaluator *e_black, Network *n_white, Network *n_black, int nodes, bool display, int max_moves) {

    if (display)
        cout << "Match (" << max_moves << " moves max)" << endl;

    Board b;

    // Jeu
    while ((b.is_mate() == -1 && b.game_over() == 0)) {
        if (b._player)
            b.grogros_zero(e_white, nodes, true, _beta, _k_add, false, 0, n_white);
        else
            b.grogros_zero(e_black, nodes, true, _beta, _k_add, false, 0, n_black);
        b.play_monte_carlo_move_keep(b.best_monte_carlo_move(), false, true, false);

        // Limite de coups
        if (max_moves && b._player && b._moves_count > max_moves)
            break;
    }

    if (display)
        cout << b._pgn << endl;

    int g = b.is_mate();
    if (g == -1)
        g = b.game_over();
    else
        return -g * b._color;
    if (g == 2)
        return 0;
    return g;
    

    return g;

}


// Fonction qui organise un tournoi entre les IA utilisant évaluateurs et réseaux de neurones des listes et renvoie la liste des scores
int* tournament(Evaluator **evaluators, Network **networks, int n_players, int nodes, int victory, int draw, bool display_full, int max_moves) {

    cout << "***** Tournament !! " << n_players << " players *****" << endl;

    // Liste des scores
    int *scores = new int[n_players];
    for (int i = 0; i < n_players; i++)
        scores[i] = 0;


    int result;
    for (int i = 0; i < n_players; i++) {

        if (display_full)
            cout << "\n***** Round : " << i + 1 << "/" << n_players << " *****" << endl;

        for (int j = 0; j < n_players; j++) {
            if (i != j) {
                if (display_full)
                    cout << "\nPlayer " << i << " vs Player " << j << endl;
                result = match(evaluators[i], evaluators[j], networks[i], networks[j], nodes, display_full, max_moves);
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

    // Lit le livre d'ouvertures actuel
    string book = LoadFileText("resources/data/opening_book.txt");
    cout << "Book : " << book << endl;

    // Se place à l'endroit concerné dans le livre ----> mettre des FEN dans le livre et chercher?
    to_fen();
    size_t pos = book.find(_fen); // Que faire si y'en a plusieurs? Fabriquer un tableau avec les positions puis diviser le livre en plus de parties? puis insérer au milieu...
    string book_part_1 = "";
    string book_part_2 = "";

    string add_to_book = "()";


    // Regarde si tous les coups ont été testés. Sinon, teste un des coups restants -> avec nodes noeuds


    string new_book = book_part_1 + add_to_book + book_part_2;

    SaveFileText("resources/data/opening_book.txt", (char*)new_book.c_str());
}


// Fonction qui renvoie si deux positions (en format FEN) sont les mêmes
bool equal_fen(string fen_a, string fen_b) {

    size_t k;

    k = fen_a.find(" ");
    k = fen_a.find(" ", k + 1);
    k = fen_a.find(" ", k + 1);
    k = fen_a.find(" ", k + 1);
    string simple_fen_a = fen_a.substr(0, k);
    
    k = fen_b.find(" ");
    k = fen_b.find(" ", k + 1);
    k = fen_b.find(" ", k + 1);
    k = fen_b.find(" ", k + 1);
    string simple_fen_b = fen_b.substr(0, k);

    return (simple_fen_a == simple_fen_b);
}


// Fonction qui renvoie si deux positions (en format FEN) sont les mêmes (pour les répétitions)
bool equal_positions(Board a, Board b) {
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            if (a._array[i][j] != b._array[i][j])
                return false;

    return (a._player == b._player && a._castling_rights == b._castling_rights && a._en_passant == b._en_passant);
}


// Fonction qui renvoie une représentation simple et rapide de la position
string Board::simple_position() {
    
    string s = "";
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            s += _array[i][j];

    s += _player + _castling_rights.k_b + _castling_rights.k_w + _castling_rights.q_b + _castling_rights.q_w;
    s += _en_passant;

    return s;
}


int _total_positions = 0;
string _all_positions[102];


// Fonction qui calcule la structure de pions
void Board::get_pawn_structure() {
    // Améliorations : 
    // Nombre d'ilots de pions
    // Doit dépendre de l'avancement de la partie
    // Pions faibles
    // Contrôle des cases
    // Pions passés


    if (_structure)
        return;


    _pawn_structure = 0;

    // Liste des pions par colonne
    int s_white[8];
    int s_black[8];

    // Placement des pions (6 lignes suffiraient théoriquement... car on ne peut pas avoir de pions sur la première ou la dernière rangée...)
    int pawns_white[8][8];
    int pawns_black[8][8];

    for (int i = 0; i < 8; i++) {
        s_white[i] = 0;
        s_black[i] = 0;
    }

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            pawns_white[i][j] = 0;
            pawns_black[i][j] = 0;
        }
    }


    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            s_white[j] += (_array[i][j] == 1);
            s_black[j] += (_array[i][j] == 7);
            pawns_white[i][j] = (_array[i][j] == 1);
            pawns_black[i][j] = (_array[i][j] == 7);
        }
    }

    // Pions isolés
    int isolated_pawn = -50;
    float isolated_adv_factor = 0.3f; // En fonction de l'advancement de la partie
    float isolated_adv = 1 * (1 + (isolated_adv_factor - 1) * _adv);

    for (int i = 0; i < 8; i++) {
        if (s_white[i] > 0 && (i == 0 || s_white[i - 1] == 0) && (i == 7 || s_white[i + 1] == 0))
            _pawn_structure += isolated_pawn * s_white[i] / (1 + (i == 0 || i == 7)) * isolated_adv;
        if (s_black[i] > 0 && (i == 0 || s_black[i - 1] == 0) && (i == 7 || s_black[i + 1] == 0))
            _pawn_structure -= isolated_pawn * s_black[i] / (1 + (i == 0 || i == 7)) * isolated_adv;
    }

    // Pions doublés (ou triplés...)
    int doubled_pawn = -25;
    float doubled_adv_factor = 0.6f; // En fonction de l'advancement de la partie
    float doubled_adv = 1 * (1 + (doubled_adv_factor - 1) * _adv);
    for (int i = 0; i < 8; i++) {
        _pawn_structure += (s_white[i] >= 2) * doubled_pawn * (s_white[i] - 1) * doubled_adv;
        _pawn_structure -= (s_black[i] >= 2) * doubled_pawn * (s_black[i] - 1) * doubled_adv;
    }

    // Pions passés
    // Table de valeur des pions passés en fonction de leur avancement sur le plateau
    static const int passed_pawns[8] = {0, 50, 65, 90, 125, 200, 250, 0}; // TODO à vérif
    float passed_adv_factor = 2; // En fonction de l'advancement de la partie
    float passed_adv = 1 * (1 + (passed_adv_factor - 1) * _adv);
    
    // Pour chaque colonne
    for (int i = 0; i < 8; i++) {

        // On prend en compte seulement le pion le plus avancé de la colonne (car les autre seraient bloqués derrière)
        if (s_white[i] >= 1) {
            // TODO : 8/1p6/p1p5/2P3K1/1P1k3P/P7/8/8 w - - 0 11 : pion non passé??

            // On regarde de la rangée la plus proche de la promotion, jusqu'a la première
            for (int j = 6; j > 0; j--) {
                // S'il y a un pion potentiellement passé
                if (pawns_white[j][i]) {
                    // Pas de pion sur une colonne adjacente ou pareille avec une lattitude supérieure (strictement)
                    for (int k = j + 1; k < 7; k++)
                        if ((i > 0 && _array[k][i - 1] == 7) || _array[k][i] == 7 || (i < 7 && _array[k][i + 1] == 7))
                            goto next_white_pawn;
                    _pawn_structure += passed_pawns[j] * passed_adv;
                    // cout << "toto" << endl;
                }

                next_white_pawn:
                true;
            }
        }

        if (s_black[i] >= 1) {
            for (int j = 1; j < 7; j++) {
                // Pas de pion sur une colonne adjacente ou pareille avec une lattitude inférieure (strictement)
                if (pawns_black[j][i]) {
                    for (int k = j - 1; k > 0; k--)
                        if ((i > 0 && _array[k][i - 1] == 1) || _array[k][i] == 1 || (i < 7 && _array[k][i + 1] == 1))
                            goto next_black_pawn;
                    _pawn_structure -= passed_pawns[7 - j] * passed_adv;
                }

                next_black_pawn:
                true;
            }
        }
        
    }
    // On doit encore vérifier si le pion adverse est devant ou derrière l'autre pion...

    return;
}


// Fonction qui renvoie le temps que l'IA doit passer sur le prochain coup (en ms), en fonction d'un facteur k, et des temps restant
int time_to_play_move(int t1, int t2, float k) {
    return t1 * k;

    // A améliorer :
    // Prendre en compte le temps de l'adversaire
    // Prendre en compte le nombre de coups restants dans la partie (ou une approximation) -> Si on va mater ou si on est quasi foutu -> passer plus de temps
    // Prendre en compte les variations d'évaluation, ou les coups montants
    // Reste à gérer les incréments
    // Nombre de noeuds min avant de jouer?
}


// Fonction qui met à jour le temps des joueurs
void Board::update_time() {
    // Faut-il quand même mettre à jour le temps quand il est désactivé?
    if (!_GUI._time)
        return;

    if (_GUI._last_player)
        _GUI._time_white -= clock() - _GUI._last_move_clock;
    else
        _GUI._time_black -= clock() - _GUI._last_move_clock;
    _GUI._last_move_clock = clock();


    // Gestion du temps
    if (_player != _GUI._last_player) { // TODO fix dans le cas où GrogrosZero joue (car il ne joue pas sur le plateau principal)
        if (_player) {
            _GUI._time_black -= clock() - _GUI._last_move_clock - _GUI._time_increment_black;
            _pgn += " {[%clk " + clock_to_string(_GUI._time_black, true) + "]}";
        }
        else {
            _GUI._time_white -= clock() - _GUI._last_move_clock - _GUI._time_increment_white;
            _pgn += " {[%clk " + clock_to_string(_GUI._time_white, true) + "]}";
        }

        _GUI._last_move_clock = clock();
    }

    _GUI._last_player = _player;
}


// Fonction qui lance le temps
void Board::start_time() {
    add_time_to_pgn();
    _GUI._time = true;
    _GUI._last_move_clock = clock();
}


// Fonction qui stoppe le temps
void Board::stop_time() {
    add_time_to_pgn();
    update_time();
    _GUI._time = false;
}


// Fonction qui calcule la résultante des attaques et des défenses
void Board::get_attacks_and_defenses() {
    if (_attacks && _defenses)
        return;

    // TODO faire en sorte que l'on puisse calculer les attaques seules ou les défenses seules
    _attacks_eval = 0;
    _defenses_eval = 0;

    // Tableau des valeurs d'attaques des pièces (0 = pion, 1 = caval, 2 = fou, 3 = tour, 4 = dame, 5 = roi)
    static const int attacks_array[6][6] = {
    //   P    N    B     R    Q    K
        {0,   25,  25,  30,  50,  70}, // P
        {5,   0,   20,  30, 100,  80}, // N
        {5,   10,  0,   20,  60,  40}, // B
        {5,   5,   5,   0,   60,  40}, // R
        {5,   5,   5,   10,  0,   60}, // Q
        {10,  20,  20,  25,  0,    0}, // K
    }; // TODO à définir autre part, puis à améliorer

    // Tableau des valeurs de défenses des pièces (0 = pion, 1 = caval, 2 = fou, 3 = tour, 4 = dame, 5 = roi)
    static const int defenses_array[6][6] = {
    //   P    N    B     R    Q    K
        {15,   5,  10,   5,    5,  0}, // P
        {5,   10,  10,  15,   20,  0}, // N
        {5,   10,  10,   5,   15,  0}, // B
        {10,  10,  10,  50,   25,  0}, // R
        {2,    5,   5,  10,   20,  0}, // Q
        {15,   5,   5,   5,   10,  0}, // K
    }; // TODO à définir autre part, puis à améliorer

    // TODO ne pas additionner la défense de toutes les pièces? seulement regarder les pièces non-défendues? (sinon devient pleutre)

    // TODO Contrôle des cases importantes

    // Tant pis pour le en passant...

    uint_fast8_t p; uint_fast8_t p2;
    uint_fast8_t i2; uint_fast8_t j2;

    // Diagonales
    const int_fast8_t dx[] = {-1, -1, 1, 1};
    const int_fast8_t dy[] = {-1, 1, -1, 1}; // à définir en dehors de la fonction pour gagner du temps, et pour le réutiliser autre part

    // Mouvements rectilignes
    const int_fast8_t vx[] = {-1, 1, 0, 0}; // vertical
    const int_fast8_t hy[] = {0, 0, -1, 1}; // horizontal


    // TODO Switch à changer, car c'est lent
    // TODO changer les if par des &&


    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            switch(p) {
                // Pion blanc
                case 1:
                    if (j > 0) {
                        p2 = _array[i + 1][j - 1]; // Case haut-gauche du pion blanc
                        if (p2 >= 7)
                            _attacks_eval += attacks_array[0][p2 - 7];
                        else
                            p2 && (_defenses_eval += defenses_array[0][p2 - 1]);
                    }
                    if (j < 7) {
                        p2 = _array[i + 1][j + 1]; // Case haut-droit du pion blanc
                        if (p2 >= 7)
                            _attacks_eval += attacks_array[0][p2 - 7];
                        else
                            p2 && (_defenses_eval += defenses_array[0][p2 - 1]);
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
                                    _attacks_eval += attacks_array[1][p2 - 7];
                                else
                                    p2 && (_defenses_eval += defenses_array[1][p2 - 1]);
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
                                    _attacks_eval += attacks_array[2][p2 - 7];
                                else
                                    p2 && (_defenses_eval += defenses_array[2][p2 - 1]);
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
                                    _attacks_eval += attacks_array[3][p2 - 7];
                                else
                                    p2 && (_defenses_eval += defenses_array[3][p2 - 1]);
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
                                    _attacks_eval += attacks_array[4][p2 - 7];
                                else
                                    p2 && (_defenses_eval += defenses_array[4][p2 - 1]);
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
                                    _attacks_eval += attacks_array[4][p2 - 7];
                                else
                                    p2 && (_defenses_eval += defenses_array[4][p2 - 1]);
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
                                        _attacks_eval += attacks_array[5][p2 - 7];
                                    else
                                        p2 && (_defenses_eval += defenses_array[5][p2 - 1]);
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
                            _attacks_eval -= attacks_array[0][p2 - 1];
                        else
                            p2 && (_defenses_eval -= defenses_array[0][p2 - 7]);
                    }
                    if (j < 7) {
                        p2 = _array[i - 1][j + 1]; // Case bas-droit du pion noir
                        if (p2 >= 1 && p2 <= 6)
                            _attacks_eval -= attacks_array[0][p2 - 1];
                        else
                            p2 && (_defenses_eval -= defenses_array[0][p2 - 7]);
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
                                    _attacks_eval -= attacks_array[1][p2 - 1];
                                else
                                    p2 && (_defenses_eval -= defenses_array[1][p2 - 7]);
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
                                    _attacks_eval -= attacks_array[2][p2 - 1];
                                else
                                    p2 && (_defenses_eval -= defenses_array[2][p2 - 7]);
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
                                    _attacks_eval -= attacks_array[3][p2 - 1];
                                else
                                    p2 && (_defenses_eval -= defenses_array[3][p2 - 7]);
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
                                    _attacks_eval -= attacks_array[4][p2 - 1];
                                else
                                    p2 && (_defenses_eval -= defenses_array[4][p2 - 7]);
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
                                    _attacks_eval -= attacks_array[4][p2 - 1];
                                else
                                    p2 && (_defenses_eval -= defenses_array[4][p2 - 7]);
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
                                        _attacks_eval -= attacks_array[5][p2 - 1];
                                    else
                                        p2 && (_defenses_eval -= defenses_array[5][p2 - 7]);
                                }    
                            }
                        }
                    }
                    break;
            }

        }
    }



    _attacks = true;
    _defenses = true;
    return;
}


// Fonction qui calcule l'opposition des rois (en finales de pions)
void Board::get_kings_opposition() {
    _kings_opposition = 0;

    // Regarde si on est dans une finale de pions
    int w_king_i; int w_king_j; int b_king_i; int b_king_j;
    bool w_king = false; bool b_king = false;
    int p;
    bool pawns_only = true;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            if (pawns_only && p != 0 && p != 1 && p != 6 && p != 7 && p != 12)
                pawns_only = false;
            if (!w_king && p == 6) {
                w_king_i = i;
                w_king_j = j;
                w_king = true;
            }
            if (!b_king && p == 12) {
                b_king_i = i;
                b_king_j = j;
                b_king = true;
            }
            if (w_king && b_king)
                if (pawns_only)
                    goto end_loop;
                else
                    return;
        }
    }


    end_loop:

    // Les rois sont-ils opposés?
    int di = abs(w_king_i - b_king_i);
    int dj = abs(w_king_j - b_king_j);
    if (!((di == 0 || di == 2) && (dj == 0 || dj == 2)))
        return;

    // S'ils sont opposés, le joueur qui a l'opposition, est celui qui n'a pas le trait
    _kings_opposition = -_color;
    _opposition = true;
    return;
}


// Fonction qui affiche la barre d'evaluation
void draw_eval_bar(float eval, string text_eval, float x, float y, float width, float height, float max_eval, Color white, Color black, float max_height) {
    bool is_mate = text_eval.find("M") != -1;
    float max_bar = is_mate ? 1 : max_height;
    float switch_color = min(max_bar * height, max((1 - max_bar) * height, height / 2 - eval / max_eval * height / 2));
    bool orientation = get_board_orientation();
    if (orientation) {
        DrawRectangle(x, y, width, height, black);
        DrawRectangle(x, y + switch_color, width, height - switch_color, white);
    }
    else {
        DrawRectangle(x, y, width, height, black);
        DrawRectangle(x, y, width, height - switch_color, white);
    }

    float y_margin = (1 - max_height) / 4;
    bool text_pos = (orientation ^ (eval < 0));
    float t_size = width / 2;
    Vector2 text_dimensions = MeasureTextEx(text_font, text_eval.c_str(), t_size, font_spacing);
    if (text_dimensions.x > width)
        t_size = t_size * width / text_dimensions.x;
    text_dimensions = MeasureTextEx(text_font, text_eval.c_str(), t_size, font_spacing);
    DrawTextEx(text_font, text_eval.c_str(), {x + (width - text_dimensions.x) / 2.0f, y + (y_margin + text_pos * (1.0f - y_margin * 2.0f)) * height - text_dimensions.y * text_pos}, t_size, font_spacing, (eval < 0) ? white : black);
}


// Fonction qui retire les surlignages de toutes les cases
void remove_hilighted_tiles() {
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            highlighted_array[i][j] = 0;
}


// Fonction qui selectionne une case
void select_tile(int a, int b) {
    selected_pos = {a, b};
}


// Fonction qui surligne une case (ou la de-surligne)
void highlight_tile(int a, int b) {
    highlighted_array[a][b] = 1 - highlighted_array[a][b];
}


// Fonction qui renvoie le type de pièce sélectionnée
uint_fast8_t Board::selected_piece() {
    // Faut-il stocker cela pour éviter de le re-calculer?
    if (selected_pos.first == -1 || selected_pos.second == -1)
        return 0;
    return _array[selected_pos.first][selected_pos.second];
}


// Fonction qui renvoie le type de pièce où la souris vient de cliquer
uint_fast8_t Board::clicked_piece() {
    if (clicked_pos.first == -1 || clicked_pos.second == -1)
        return 0;
    return _array[clicked_pos.first][clicked_pos.second];
}


// Fonction qui renvoie si la pièce sélectionnée est au joueur ayant trait ou non
bool Board::selected_piece_has_trait() {
    return ((_player && is_in_fast(selected_piece(), 1, 6)) || (!_player && is_in_fast(selected_piece(), 7, 12)));
}


// Fonction qui renvoie si la pièce cliquée est au joueur ayant trait ou non
bool Board::clicked_piece_has_trait() {
    return ((_player && is_in_fast(clicked_piece(), 1, 6)) || (!_player && is_in_fast(clicked_piece(), 7, 12)));
}


// Fonction qui déselectionne
void unselect() {
    selected_pos = {-1, -1};
}


// Fonction qui remet les compteurs de temps "à zéro" (temps de base)
void Board::reset_timers() {
    // Temps par joueur (en ms)
    _GUI._time_white = base_time_white;
    _GUI._time_black = base_time_black;

    // Incrément (en ms)
    _GUI._time_increment_white = base_time_increment_white;
    _GUI._time_increment_black = base_time_increment_black;
}


// Fonction qui remet le plateau dans sa position initiale
void Board::restart() {
    // Fonction largement optimisable
    from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    // _pgn = "";
    reset_timers();
}


// Fonction qui renvoie la différence matérielle entre les deux camps
int Board::material_difference() {
    int mat = 0;
    int p;
    int w_material[6] = {0, 0, 0, 0, 0, 0};
    int b_material[6] = {0, 0, 0, 0, 0, 0};

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            if (p > 0) {
                if (p < 6)
                    w_material[p]++;
                else
                    b_material[p % 6]++;
            }

            mat += piece_gui_values[p % 6] * (1 - (p / 6) * 2);
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
void draw_simple_arrow_from_coord(int i1, int j1, int i2, int j2, float thickness, Color c) {
    // cout << thickness << endl;
    if (thickness == -1.0)
        thickness = arrow_thickness;
    float x1 = board_padding_x + tile_size * orientation_index(j1) + tile_size /2;
    float y1 = board_padding_y + tile_size * orientation_index(7 - i1) + tile_size /2;
    float x2 = board_padding_x + tile_size * orientation_index(j2) + tile_size /2;
    float y2 = board_padding_y + tile_size * orientation_index(7 - i2) + tile_size /2;
    
    c.a = 255;

    // "Flèche"
    if (abs(j2 - j1) != abs(i2 - i1) && abs(j2 - j1) + abs(i2 - i1) == 3)
        DrawLineBezier(x1, y1, x2, y2, thickness, c);
    else
        DrawLineEx(x1, y1, x2, y2, thickness, c);

    //c.a = 255;
    
    DrawCircle(x1, y1, thickness, c);
    DrawCircle(x2, y2, thickness * 2.0f, c);

}


// Fonction qui réinitialise les composantes de l'évaluation
void Board::reset_eval() {
    _mate_checked = false;
    _game_over_checked = false;
    _evaluated = false; _evaluation = 0;
    _activity = false; _piece_activity = 0;
    _safety = false; _king_safety = 0;
    _structure = false; _pawn_structure = 0;
    _attacks = false; _attacks_eval = 0;
    _defenses = false; _defenses_eval = 0;
    _opposition = false; _kings_opposition = 0;
    _material = false; _material_count = 0;
    _advancement = false; _adv = 0;
    _positioning = false; _pos = 0;
    _rook_open_file = false; _rook_open = 0;
    _rook_semi_open_file = false; _rook_semi = 0;
    _square_controls = false; _control = 0;
    _winning_chances = false; _white_winning_chance = 0; _drawing_chance = 0; _black_winning_chance = 0;
}


// Fonction qui compte les tours sur les colonnes ouvertes et semi-ouvertes
void Board::get_rook_on_open_file() {
    if (_rook_open_file && _rook_semi_open_file)
        return;

    // Pions sur les colonnes, tours sur les colonnes
    int w_pawns[8];
    int b_pawns[8];
    int w_rooks[8];
    int b_rooks[8];

    for (int i = 0; i < 8; i++) {
        w_pawns[i] = 0;
        b_pawns[i] = 0;
        w_rooks[i] = 0;
        b_rooks[i] = 0;
    }

    int p;

    // TODO Calculer le nombre de colonnes ouvertes et semi-ouvertes -> diviser le résultat par ce nombre...

    // Calcul du nombre de pions et tours par colonne
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            (p == 1) ? w_pawns[j]++ : ((p == 4) ? w_rooks[j]++ : ((p == 7) ? b_pawns[j]++ : (p == 10) && b_rooks[j]++));
        }
    }

    // Nombre de colonnes ouvertes
    int open_files = 0;

    // Tour sur les colonnes ouvertes
    if (!_rook_open_file) {
        _rook_open = 0;
        int open_value = 50;

        for (int i = 0; i < 8; i++) {
            (w_rooks[i] && !w_pawns[i] && !b_pawns[i]) ? _rook_open += w_rooks[i] * open_value : (b_rooks[i] && !b_pawns[i] && !w_pawns[i]) && (_rook_open -= b_rooks[i] * open_value);
            !w_pawns[i] && !b_pawns[i] && open_files++;
        }       
    }

    // Tour sur les colonnes semi-ouvertes
    if (!_rook_semi_open_file) {
        _rook_semi = 0;
        int semi_open_value = 25;

        for (int i = 0; i < 8; i++)
            (w_rooks[i] && !w_pawns[i] && b_pawns[i]) ? _rook_semi += w_rooks[i] * semi_open_value : (b_rooks[i] && !b_pawns[i] && w_pawns[i]) && (_rook_semi -= b_rooks[i] * semi_open_value);
    }

    // L'importance est moindre s'il y a plusieurs colonnes ouvertes
    if (_rook_open)
        _rook_open /= open_files; 

    return;
}


// Fonction qui renvoie la profondeur de calcul de la variante principale
int Board::grogros_main_depth() {

    if ((_got_moves == -1 && !_is_game_over) || _got_moves == 0)
        return 0;

    if (_is_game_over)
        return 0;
    
    if (_tested_moves == _got_moves) {
        int move = best_monte_carlo_move();
        return 1 + _monte_buffer._heap_boards[_index_children[move]].grogros_main_depth();
    }
        
    return 1;

}


// Fonction qui calcule la valeur des cases controllées sur l'échiquier
void Board::get_square_controls() {
    // TODO ajouter des valeurs pour le contrôle des cases par les pièces?

    if (_square_controls)
        return;

    _control = 0;

    // Valeur du contrôle de chaque case (pour les pions)
    static const int square_controls[8][8] = {
        {20,  20,  20,  20,  20,  20,  20,  20},
        {50,  50,  50,  50,  50,  50,  50,  50},
        {10,  20,  30,  40,  40,  30,  20,  10},
        {5,   10,  20,  30,  30,  20,  10,   5},
        {0,    0,   0,  20,  20,   0,   0,   0},
        {5,   -5, -10,   0,   0, -10,  -5,   5},
        {0,    0,   0,   0,   0,   0,   0,   0},
        {0,    0,   0,   0,   0,   0,   0,   0}
    };

    int p;
    int total_control = 0;

    // Calcul des cases controllées par les pions de chaque camp
    // TODO Regarder si avoir un double contrôle c'est important
    bool white_controls[8][8] = {false};
    bool black_controls[8][8] = {false};

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            if (p == 1) {
                white_controls[7 - i - 1][j - 1] = true;
                white_controls[7 - i - 1][j + 1] = true;
            }
            if (p == 7) {
                black_controls[i - 1][j - 1] = true;
                black_controls[i - 1][j + 1] = true;
            }    
        }
    }


    // Proposition de ChatGPT : 
    /*const uint_fast8_t numRows = 8;
    const uint_fast8_t numCols = 8;

    for (int i = 1; i < numRows - 1; i++) {
        for (int j = 1; j < numCols - 1; j++) {
            p = _array[i][j];

            white_controls[i - 1][j - 1] |= (p == 1) ? true : false;
            white_controls[i - 1][j + 1] |= (p == 1) ? true : false;
            black_controls[i + 1][j - 1] |= (p == 7) ? true : false;
            black_controls[i + 1][j + 1] |= (p == 7) ? true : false;
        }
    }*/





    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            total_control += (white_controls[i][j] - black_controls[i][j]) * square_controls[i][j];

    // L'importance de ce paramètre dépend de l'avancement de la partie : l'espace est d'autant plus important que le nombre de pièces est grand
    _control = total_control * (1.0f - _adv);
    _square_controls = true;

    return;
}


// Fonction qui calcule les chances de gain/nulle/perte
void Board::get_winning_chances() {
    if (_winning_chances)
        return;

    // float a = 1;
    // float b = 1;
    // float c = 1;
    // float d = 1;
    // float e = 1;
    // float f = 1;

    // TODO y'a des trucs vraiment bizarres dans _evaluation... parfois des *100.. parfois c'est des eniters, parfois des float... bizarre
    // cout << _evaluation << endl;
    _white_winning_chance = 0.5f * (1 + (2 / (1 + (float)exp(-0.75 * _evaluation)) - 1));
    // _drawing_chance = 1 / (1 + exp(-c * _evaluation + d));
    _drawing_chance = 0;
    _black_winning_chance = 1.0f - _white_winning_chance;

    _winning_chances = true;
    return;
}


// Fonction qui renvoie la valeur UCT
float uct(float win_chance, float c, int nodes_parent, int nodes_child) {
    // cout << win_chance << ", " << nodes_parent << ", " << nodes_child << " = " << win_chance + c * sqrt(log(nodes_parent) / nodes_child) << endl;
    return win_chance + c * (float)sqrt(log(nodes_parent) / nodes_child);
}

// Fonction qui sélectionne et renvoie le coup avec le meilleur UCT
int Board::select_uct(float c) {
    
    float max_uct = 0;
    int uct_move = 0;
    float uct_value;
    float win_chance;

    // Pour chaque noeud fils
    for (int i = 0; i < _got_moves; i++) {
        win_chance = get_winning_chances_from_eval(_monte_buffer._heap_boards[_index_children[i]]._evaluation, _monte_buffer._heap_boards[_index_children[i]]._mate, _player);
        uct_value = uct(win_chance, c, _nodes, _nodes_children[i]);
        if (uct_value > max_uct) {
            max_uct = uct_value;
            uct_move = i;
        }
    }

    return uct_move;
}


// Fonction qui fait un tri rapide des coups (en plaçant les captures en premier)
// TODO : à faire lors de la génération de coups?
bool Board::quick_moves_sort() {
    // Captures, promotions.. échecs?
    // TODO : ajouter les échecs? les mats?
    // TODO : faire en fonction de la pièce qui prend?

    // Liste des valeurs de chaque coup
    int *moves_values = new int[_got_moves];

    // Valeurs assignées

    // Prises
    static const int captures_values[13] = {0, 100, 300, 300, 500, 900, 10000, 100, 300, 300, 500, 900, 10000}; // rien,   |pion, cavalier, fou, tour, dame, roi| (blancs, puis noirs)

    // Promotions
    static const int promotion_value = 500;

    // Valeur des pièces en jeu
    int captured_value;
    int capturer_value;

    // Pour chaque coup
    for (int i = 0; i < _got_moves; i++) {
        // Assigne une valeur au coup (valeur de la prise ou de la promotion)

        // Prise
        captured_value = captures_values[_array[_moves[4 * i + 2]][_moves[4 * i + 3]]];
        capturer_value = captures_values[_array[_moves[4 * i]][_moves[4 * i + 1]]];

        moves_values[i] = captured_value == 0 ? 0 : captured_value / capturer_value;

        // Promotion
        // Blancs
        moves_values[i] += (_array[_moves[4 * i]][_moves[4 * i + 1]] == 1 && _moves[4 * i + 2] == 7) * promotion_value;

        // Noirs
        moves_values[i] += (_array[_moves[4 * i]][_moves[4 * i + 1]] == 7 && _moves[4 * i + 2] == 0) * promotion_value;
    }


    // Construction des nouveaux coups

    // Liste des index des coups triés par ordre décroissant de valeur
    int *moves_indexes = new int[_got_moves];
    int max_ind;

    for (int i = 0; i < _got_moves; i++) {
        max_ind = max_index(moves_values, _got_moves);
        moves_indexes[i] = max_index(moves_values, _got_moves);
        moves_values[max_ind] = -inf_int;
    }

    // Génération de la list de coups de façon ordonnée
    uint_fast8_t *new_moves = new uint_fast8_t[_got_moves * 4];
    copy(_moves, _moves + _got_moves * 4, new_moves);
    int j;

    for (int i = 0; i < _got_moves; i++) {
        j = moves_indexes[i];
        _moves[4 * i] = new_moves[4 * j];
        _moves[4 * i + 1] = new_moves[4 * j + 1];
        _moves[4 * i + 2] = new_moves[4 * j + 2];
        _moves[4 * i + 3] = new_moves[4 * j + 3];
    }

    // Suppression des tableaux
    delete[] moves_values;
    delete[] new_moves;

    _quick_sorted_moves = true;
    _sorted_moves = false;

    return true;
}


// Fonction qui fait un quiescence search
// TODO : mettre une profondeur max?
int Board::quiescence(Evaluator *eval, int alpha, int beta, int depth) { 

    // Evalue la position initiale
    evaluate_int(eval, true);
    int stand_pat = _evaluation * _color;
    
    if (depth <= 0)
        return stand_pat;

    // Beta cut-off
    if (stand_pat >= beta)
        return beta;

    // Mise à jour de alpha si l'éval statique est plus grande
    if (alpha < stand_pat)
        alpha = stand_pat;

    
    (_got_moves == -1) && get_moves(false, true);
    quick_moves_sort();


    // Evaluate all capturing moves and promotions
    for (int i = 0; i < _got_moves; i++) {
        // TODO : ajouter promotions et échecs
        // TODO : utiliser des flags

        // Si c'est une capture
        if (_array[_moves[4 * i + 2]][_moves[4 * i + 3]] != 0) {
            Board b;
            b.copy_data(*this);
            b.make_move(_moves[4 * i], _moves[4 * i + 1], _moves[4 * i + 2], _moves[4 * i + 3]);

            int score = -b.quiescence(eval, -beta, -alpha, depth - 1);

            if (score >= beta)
                return beta;

            if (score > alpha)
                alpha = score;
        }

        
    }

    return alpha;
}


// Constructeur GUI
GUI::GUI() {
}

// GUI
GUI _GUI;



// Fonction qui renvoie le i-ème coup
int* Board::get_i_move(int i) {
    if (i < 0 || i >= _got_moves) {
        cout << "i-th move impossible to find";
        return nullptr;
    }

    int* coord = new int[4];
    coord[0] = _moves[4 * i];
    coord[1] = _moves[4 * i + 1];
    coord[2] = _moves[4 * i + 2];
    coord[3] = _moves[4 * i + 3];

    return coord;
}


// Fonction qui fait cliquer le i-ème coup
bool Board::click_i_move(int i, bool orientation) {
    int* coord = new int[4];
    coord = get_i_move(i);

    if (coord == nullptr)
        return false;

    click_move(coord[0], coord[1], coord[2], coord[3], _GUI._binding_left, _GUI._binding_top, _GUI._binding_right, _GUI._binding_bottom, orientation);
    return true;
}


// Fonction qui met en place le binding avec chess.com pour une nouvelle partie
bool GUI::new_bind_game() {
    int orientation = bind_board_orientation(_GUI._binding_left, _GUI._binding_top, _GUI._binding_right, _GUI._binding_bottom);

    if (orientation == -1)
        return false;

    _board.restart();

    if (get_board_orientation() != orientation)
        switch_orientation();
    
    if (orientation) {
        _white_player = "GrogrosZero";
        _black_player = "chess.com player";
    }
    else {
        _white_player = "chess.com player";
        _black_player = "GrogrosZero";
    }

    _board.add_names_to_pgn();

    _binding_solo = true;
    _binding_full = false;
    _click_bind = true;
    if (!_monte_buffer._init)
        _monte_buffer.init();
    _board.start_time();
    _grogros_analysis = false;

    return true;
}

// Fonction qui met en place le binding avec chess.com pour une nouvelle analyse de partie
bool GUI::new_bind_analysis() {
    int orientation = bind_board_orientation(_GUI._binding_left, _GUI._binding_top, _GUI._binding_right, _GUI._binding_bottom);

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

    _board.add_names_to_pgn();

    _binding_solo = false;
    _binding_full = true;
    _click_bind = false;
    if (!_monte_buffer._init)
        _monte_buffer.init();
    _grogros_analysis = true;

    return true;
}