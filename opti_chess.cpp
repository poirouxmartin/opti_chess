#include "opti_chess.h"
#include "useful_functions.h"
#include "gui.h"
#include <string>
#include <sstream>



// Constructeur par défaut
Board::Board() {
}


// Constructeur de copie
Board::Board(Board &b) {
    // Copie du plateau
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            _array[i][j] = b._array[i][j];
        }
    }

    _got_moves = b._got_moves;
    _player = b._player;
    for (int i = 0; i < _got_moves * 4 + 1; i++) {
        _moves[i] = b._moves[i];
        
    }
    for (int i = 0; i < _got_moves; i++) {
        _move_order[i] = b._move_order[i];
    }
    _sorted_moves = b._sorted_moves;
    _evaluation = b._evaluation;
    _color = b._color;
    _k_castle_w = b._k_castle_w;
    _q_castle_w = b._q_castle_w;
    _k_castle_b = b._k_castle_b;
    _q_castle_b = b._q_castle_b;
    _en_passant = b._en_passant;
    _half_moves_count = b._half_moves_count;
    _moves_count = b._moves_count;
    _pgn = b._pgn;
}

// Fonction qui copie les attributs d'un tableau
void Board::copy_data(Board &b) {
    // Copie du plateau
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            _array[i][j] = b._array[i][j];
        }
    }

    _got_moves = b._got_moves;
    _player = b._player;
    for (int i = 0; i < _got_moves * 4 + 1; i++) {
        _moves[i] = b._moves[i];
        
    }
    for (int i = 0; i < _got_moves; i++) {
        _move_order[i] = b._move_order[i];
    }
    _sorted_moves = b._sorted_moves;
    _evaluation = b._evaluation;
    _color = b._color;
    _k_castle_w = b._k_castle_w;
    _q_castle_w = b._q_castle_w;
    _k_castle_b = b._k_castle_b;
    _q_castle_b = b._q_castle_b;
    _en_passant = b._en_passant;
    _half_moves_count = b._half_moves_count;
    _moves_count = b._moves_count;
    _pgn = b._pgn;
}


// Fonction qui copie les coups d'un plateau
void Board::copy_moves(Board &b) {
    _got_moves = b._got_moves;
    for (int i = 0; i < _got_moves * 4 + 1; i++) {
        _moves[i] = b._moves[i];
    } 
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
        //s += char(i + 48);
        s += "|\n--------------------------------\n";
    }

    cout << s;
}



// Fonction qui ajoute un coup dans une liste de coups
bool Board::add_move(int i, int j, int k, int l, int *iterator) {
    _moves[*iterator] = i;
    _moves[*iterator + 1] = j;
    _moves[*iterator + 2] = k;
    _moves[*iterator + 3] = l;
    *iterator += 4;
    return true;
}



// Fonction qui ajoute les coups "pions" dans la liste de coups
bool Board::add_pawn_moves(int i, int j, int *iterator) {
    string abc = "abcdefgh";

    // Joueur avec les pièces blanches
    if (_player) {
        // Poussée (de 1)
        (_array[i + 1][j] == 0) && add_move(i, j, i + 1, j, iterator);
        // Poussée (de 2)
        (i == 1 && _array[i + 1][j] == 0 && _array[i + 2][j] == 0) && add_move(i, j, i + 2, j, iterator);
        // Prise (gauche)
        (j > 0 && (is_in(_array[i + 1][j - 1], 7, 12) || _en_passant[0] == abc[j - 1])) && add_move(i, j, i + 1, j - 1, iterator);
        // Prise (droite)
        (j < 7 && (is_in(_array[i + 1][j + 1], 7, 12) || _en_passant[0] == abc[j + 1])) && add_move(i, j, i + 1, j + 1, iterator);
    }
    // Joueur avec les pièces noires
    else {
        // Poussée (de 1)
        (_array[i - 1][j] == 0) && add_move(i, j, i - 1, j, iterator);
        // Poussée (de 2)
        (i == 6 && _array[i - 1][j] == 0 && _array[i - 2][j] == 0) && add_move(i, j, i - 2, j, iterator);
        // Prise (gauche)
        (j > 0 && (is_in(_array[i - 1][j - 1], 1, 6) || _en_passant[0] == abc[j - 1])) && add_move(i, j, i - 1, j - 1, iterator);
        // Prise (droite)
        (j < 7 && (is_in(_array[i - 1][j + 1], 1, 6) || _en_passant[0] == abc[j + 1])) && add_move(i, j, i - 1, j + 1, iterator);
    }

    return true;
}


// Fonction qui ajoute les coups "cavaliers" dans la liste de coups
bool Board::add_knight_moves(int i, int j, int *iterator) {
    // les boucles for sont à modifier, car très lentes
    int i2; int j2;
    for (int k = -2; k <= 2; k++) {
        for (int l = -2; l <= 2; l++) {
            i2 = i + k; j2 = j + l;
            if (_player) {
                // Si le coup n'est ni hors du plateau, ni sur une case où une pièce alliée est placée
                (k * l != 0 && abs(k) + abs(l) == 3 && is_in(i2, 0, 7) && is_in (j2, 0, 7) && !is_in(_array[i2][j2], 1, 6)) && add_move(i, j, i2, j2, iterator);
            }
            else {
                // Si le coup n'est ni hors du plateau, ni sur une case où une pièce alliée est placée
                (k * l != 0 && abs(k) + abs(l) == 3 && is_in(i2, 0, 7) && is_in (j2, 0, 7) && !is_in(_array[i2][j2], 7, 12)) && add_move(i, j, i2, j2, iterator);
            }
            
        }
    }

    return true;

}


// Fonction qui ajoute les coups diagonaux dans la liste de coups
bool Board::add_diag_moves(int i, int j, int *iterator) {
    int ally_min; int ally_max;
    if (_player) {
        ally_min = 1; ally_max = 6;
    }
    else {
        ally_min = 7; ally_max = 12;
    }

    int i2; int j2; int p2;
        
    // Diagonale 1
    for (int k = 1; k < 8; k++) {
        i2 = i + k; j2 = j + k;
        // Si le coup n'est pas sur le plateau
        if (!is_in(i2, 0, 7) || !is_in(j2, 0, 7))
            k = 7;
        else {
            p2 = _array[i2][j2];
            // Si la case est occupée par une pièce alliée
            if (is_in(p2, ally_min, ally_max))
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
        if (!is_in(i2, 0, 7) || !is_in(j2, 0, 7))
            k = 7;
        else {
            p2 = _array[i2][j2];
            // Si la case est occupée par une pièce alliée
            if (is_in(p2, ally_min, ally_max))
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
        if (!is_in(i2, 0, 7) || !is_in(j2, 0, 7))
            k = 7;
        else {
            p2 = _array[i2][j2];
            // Si la case est occupée par une pièce alliée
            if (is_in(p2, ally_min, ally_max))
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
        if (!is_in(i2, 0, 7) || !is_in(j2, 0, 7))
            k = 7;
        else {
            p2 = _array[i2][j2];
            // Si la case est occupée par une pièce alliée
            if (is_in(p2, ally_min, ally_max))
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
bool Board::add_rect_moves(int i, int j, int *iterator) {
    int ally_min; int ally_max;
    if (_player) {
        ally_min = 1; ally_max = 6;
    }
        
    else {
        ally_min = 7; ally_max = 12;
    }

    int i2; int j2;
    int p2;

    // Horizontale 1
    for (int k = 1; k < 8; k++) {
        j2 = j - k;
        // Si le coup n'est pas sur le plateau
        if (!is_in(i, 0, 7) || !is_in(j2, 0, 7))
            k = 7;
        else {
            p2 = _array[i][j2];
            // Si la case est occupée par une pièce alliée
            if (is_in(p2, ally_min, ally_max))
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
        if (!is_in(i, 0, 7) || !is_in(j2, 0, 7))
            k = 7;
        else {
            p2 = _array[i][j2];
            // Si la case est occupée par une pièce alliée
            if (is_in(p2, ally_min, ally_max))
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
        if (!is_in(i2, 0, 7) || !is_in(j, 0, 7))
            k = 7;
        else {
            p2 = _array[i2][j];
            // Si la case est occupée par une pièce alliée
            if (is_in(p2, ally_min, ally_max))
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
        if (!is_in(i2, 0, 7) || !is_in(j, 0, 7))
            k = 7;
        else {
            p2 = _array[i2][j];
            // Si la case est occupée par une pièce alliée
            if (is_in(p2, ally_min, ally_max))
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
bool Board::add_king_moves(int i, int j, int *iterator) {
    int ally_min; int ally_max;
    if (_player) {
        ally_min = 1; ally_max = 6;
    }
        
    else {
        ally_min = 7; ally_max = 12;
    }

    int i2; int j2;
    
    for (int k = -1; k < 2; k++) {
        for (int l = -1; l < 2; l++) {
            i2 = i + k; j2 = j + l;
            // Si le coup n'est ni hors du plateau, ni sur une case où une pièce alliée est placée
            ((k != 0 || l != 0) && is_in(i2, 0, 7) && is_in (j2, 0, 7) && !is_in(_array[i2][j2], ally_min, ally_max)) && add_move(i, j, i2, j2, iterator);
        }
    }

    return true;

}



// Renvoie la liste des coups possibles
int* Board::get_moves() {

    int p;
    int iterator = 0;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            
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
                    if (_q_castle_w && _array[i][j - 1] == 0 && _array[i][j - 2] == 0 && _array[i][j - 3] == 0 && !attacked(i, j) && !attacked(i, j - 1) && !attacked(i, j - 2))
                        add_move(i, j, i, j - 2, &iterator);
                    // Petit
                    if (_k_castle_w && _array[i][j + 1] == 0 && _array[i][j + 2] == 0 && !attacked(i, j) && !attacked(i, j + 1) && !attacked(i, j + 2))
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
                    if (_q_castle_b && _array[i][j - 1] == 0 && _array[i][j - 2] == 0 && _array[i][j - 3] == 0 && !attacked(i, j) && !attacked(i, j - 1) && !attacked(i, j - 2))
                        add_move(i, j, i, j - 2, &iterator);
                    // Petit
                    if (_k_castle_b && _array[i][j + 1] == 0 && _array[i][j + 2] == 0 && !attacked(i, j) && !attacked(i, j + 1) && !attacked(i, j + 2))
                        add_move(i, j, i, j + 2, &iterator);
                    break;

            }

        }
    }

    _moves[iterator] = -1;
    _got_moves = iterator / 4;

    return _moves;
}



// Fonction qui dit si une case est attaqué
bool Board::attacked(int i, int j) {

    // Pour accelérer les tests
    if (i < 0 || i > 7 || j < 0 || j > 7)
        return false;


    // Attaque par un pion
    if ((j > 0 && _array[i + _color][j - 1] == 1 + 6 * _player) || (j < 7 && _array[i + _color][j + 1] == 1 + 6 * _player))
        return true;

    // Attaque par un cavalier
    for (int k = -2; k <= 2; k++) {
        for (int l = -2; l <= 2; l++) {
            if (_array[i + k][j + l] == 2 + 6 * _player && abs(k) + abs(l) == 3 && is_in(i + k, 0, 7) && is_in(j + l, 0, 7))
                return true;
        }
    }

    // Attaque par un mouvement rectiligne
    int k;

    // Verticale
    k = -1;
    // Bas
    while (i + k >= 0) {
        if (_array[i + k][j] == 4 + 6 * _player || _array[i + k][j] == 5 + 6 * _player)
            return true;
        if (_array[i + k][j] != 0)
            break;
        k--;
    }
    // Haut
    k = 1;
    while (i + k <= 7) {
        if (_array[i + k][j] == 4 + 6 * _player || _array[i + k][j] == 5 + 6 * _player)
            return true;
        if (_array[i + k][j] != 0)
            break;
        k++;
    }
    // Horizontale
    k = -1;
    // Gauche
    while (j + k >= 0) {
        if (_array[i][j + k] == 4 + 6 * _player || _array[i][j + k] == 5 + 6 * _player)
            return true;
        if (_array[i][j + k] != 0)
            break;
        k--;
    }
    // Droite
    k = 1;
    while (j + k <= 7) {
        if (_array[i][j + k] == 4 + 6 * _player || _array[i][j + k] == 5 + 6 * _player)
            return true;
        if (_array[i][j + k] != 0)
            break;
        k++;
    }



    // Attaque par diagonale

    // Diagonale 1
    // Bas gauche
    k = -1;
    while (i + k >= 0 && j + k >= 0) {
        if (_array[i + k][j + k] == 3 + 6 * _player || _array[i + k][j + k] == 5 + 6 * _player)
            return true;
        if (_array[i + k][j + k] != 0)
            break;
        k--;
    }
    // Haut droite
    k = +1;
    while (i + k <= 7 && j + k <= 7) {
        if (_array[i + k][j + k] == 3 + 6 * _player || _array[i + k][j + k] == 5 + 6 * _player)
            return true;
        if (_array[i + k][j + k] != 0)
            break;
        k++;
    }
    // Diagonale 2
    // Bas droite
    k = -1;
    while (i + k >= 0 && j - k <= 7) {
        if (_array[i + k][j - k] == 3 + 6 * _player || _array[i + k][j - k] == 5 + 6 * _player)
            return true;
        if (_array[i + k][j - k] != 0)
            break;
        k--;
    }
    // Haut gauche
    k = 1;
    while (i + k <= 7 && j - k >= 0) {
        if (_array[i + k][j - k] == 3 + 6 * _player || _array[i + k][j - k] == 5 + 6 * _player)
            return true;
        if (_array[i + k][j - k] != 0)
            break;
        k++;
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

    cout << "pos king : " << pos_i << ", " << pos_j << endl;

    return attacked(pos_i, pos_j);
}




// Fonction qui affiche la liste des coups donnée en argument
void Board::display_moves() {
    
    int i = 0;
    while (i < 1000) {
        if (_moves[i] == -1)
            break;
        cout << _moves[i] << "," << _moves[i + 1] << " -> " << _moves[i + 2] << "," << _moves[i + 3] << endl;
        i += 4;
    }

    
}


// Fonction qui joue un coup
void Board::make_move(int i, int j, int k, int l) {

    int p = _array[i][j];

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


    // Implémentation des demi-coups
    _half_moves_count += 1;
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
        _q_castle_w = false;
        _k_castle_w = false;
    }
    if (p == 12) {
        _q_castle_b = false;
        _k_castle_b = false;
    }

    // Si c'est une tour, peut retirer la permission de roque
    if (p == 4) {
        if (j == 0)
            _q_castle_w = false;
        if (j == 7)
            _k_castle_w = false;
    }
    if (p == 10) {
        if (j == 0)
            _q_castle_b = false;
        if (j == 7)
            _k_castle_b = false;
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

    _sorted_moves = -1;


    _last_move[0] = i;
    _last_move[1] = j;
    _last_move[2] = k;
    _last_move[3] = l;


}


// Fonction qui joue le coup i
void Board::make_index_move(int i) {
    // if (i < 0 | i >= _got_moves)
    //     cout << "move index out of range" << endl;
    // else {
    //     int k = 4 * i;
    //     make_move(_moves[k], _moves[k + 1], _moves[k + 2], _moves[k + 3]);
    // }

    int k = 4 * i;
    make_move(_moves[k], _moves[k + 1], _moves[k + 2], _moves[k + 3]);
}



// Paramètres d'évaluation par défaut
static float default_eval_parameters[3] = {1, 0.1, 0.025};


// Fonction qui évalue la position à l'aide d'heuristiques
void Board::evaluate(float eval_parameters[3] = default_eval_parameters) {

    // Coefficiants des heuristiques
    float piece_value = eval_parameters[0];
    float piece_activity = eval_parameters[1];
    float piece_positioning = eval_parameters[2];

    int pos_pawn[8][8]      {{0,   0,   0,   0,   0,   0,   0,   0},
                            {78,  83,  86,  73, 102,  82,  85,  90},
                            {7,  29,  21,  44,  40,  31,  44,   7},
                            {-17,  16,  -2,  15,  14,   0,  15, -13},
                            {-26,   3,  10,   9,   6,   1,   0, -23},
                            {-22,   9,   5, -11, -10,  -2,   3, -19},
                            {-31,   8,  -7, -37, -36, -14,   3, -31},
                            {0,   0,   0,   0,   0,   0,   0,   0}};

    int pos_knight[8][8]    {{-66, -53, -75, -75, -10, -55, -58, -70},
                            {-3,  -6, 100, -36,   4,  62,  -4, -14},
                            {10,  67,   1,  74,  73,  27,  62,  -2},
                            {24,  24,  45,  37,  33,  41,  25,  17},
                            {-1,   5,  31,  21,  22,  35,   2,   0},
                            {-18,  10,  13,  22,  18,  15,  11, -14},
                            {-23, -15,   2,   0,   2,   0, -23, -20},
                            {-74, -23, -26, -24, -19, -35, -22, -69}};

    int pos_bishop[8][8]    {{-59, -78, -82, -76, -23,-107, -37, -50},
                            {-11,  20,  35, -42, -39,  31,   2, -22},
                            {-9,  39, -32,  41,  52, -10,  28, -14},
                            {25,  17,  20,  34,  26,  25,  15,  10},
                            {13,  10,  17,  23,  17,  16,   0,   7},
                            {14,  25,  24,  15,   8,  25,  20,  15},
                            {19,  20,  11,   6,   7,   6,  20,  16},
                            {-7,   2, -15, -12, -14, -15, -10, -10}};

    int pos_rook[8][8]      {{35,  29,  33,   4,  37,  33,  56,  50},
                            {55,  29,  56,  67,  55,  62,  34,  60},
                            {19,  35,  28,  33,  45,  27,  25,  15},
                            {0,   5,  16,  13,  18,  -4,  -9,  -6},
                            {-28, -35, -16, -21, -13, -29, -46, -30},
                            {-42, -28, -42, -25, -25, -35, -26, -46},
                            {-53, -38, -31, -26, -29, -43, -44, -53},
                            {-30, -24, -18,   5,  -2, -18, -31, -32}};

    int pos_queen[8][8]     {{6,   1,  -8,-104,  69,  24,  88,  26},
                            {14,  32,  60, -10,  20,  76,  57,  24},
                            {-2,  43,  32,  60,  72,  63,  43,   2},
                            {1, -16,  22,  17,  25,  20, -13,  -6},
                            {-14, -15,  -2,  -5,  -1, -10, -20, -22},
                            {-30,  -6, -13, -11, -16, -11, -16, -27},
                            {-36, -18,   0, -19, -15, -15, -21, -38},
                            {-39, -30, -31, -13, -31, -36, -34, -42}};

    int pos_king[8][8]      {{4,  54,  47, -99, -99,  60,  83, -62},
                            {-32,  10,  55,  56,  56,  55,  10,   3},
                            {-62,  12, -57,  44, -67,  28,  37, -31},
                            {-55,  50,  11,  -4, -19,  13,   0, -49},
                            {-55, -43, -52, -28, -51, -47,  -8, -50},
                            {-47, -42, -43, -79, -64, -32, -29, -32},
                            {-4,   3, -14, -50, -57, -18,  13,   4},
                            {17,  30,  -3, -14,   6,  -1,  40,  18}};

    _evaluation = 0;


    // partie théoriquement nulle
    // if (game_over() == 2)
    //     return;

    // à tester: changer les boucles par des for (i : array) pour optimiser
    int p;
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
           
            switch (p)
            {   
                case 0: break;
                case 1: _evaluation += 1 * piece_value + piece_positioning * pos_pawn[7 - i][j]; break;
                case 2: _evaluation += 3.2 * piece_value + piece_positioning * pos_knight[7 - i][j]; break;
                case 3: _evaluation += 3.3 * piece_value + piece_positioning * pos_bishop[7 - i][j]; break;
                case 4: _evaluation += 4.8 * piece_value + piece_positioning * pos_rook[7 - i][j]; break;
                case 5: _evaluation += 8.8 * piece_value + piece_positioning * pos_queen[7 - i][j]; break;
                case 6: _evaluation += 100000 * piece_value + piece_positioning * pos_king[7 - i][j]; break;
                case 7: _evaluation -= 1 * piece_value + piece_positioning * pos_pawn[i][j]; break;
                case 8: _evaluation -= 3.2 * piece_value + piece_positioning * pos_knight[i][j]; break;
                case 9: _evaluation -= 3.3 * piece_value + piece_positioning * pos_bishop[i][j]; break;
                case 10: _evaluation -= 4.8 * piece_value + piece_positioning * pos_rook[i][j]; break;
                case 11: _evaluation -= 8.8 * piece_value + piece_positioning * pos_queen[i][j]; break;
                case 12: _evaluation -= 100000 * piece_value + piece_positioning * pos_king[i][j]; break;
            }

        }
    }


    // // Activité des pièces
    // if (_got_moves == -1)
    //     get_moves();
    // _evaluation += _color * _got_moves * piece_activity;
        

    // Pour éviter les répétitions (ne fonctionne pas)
    _evaluation *= 1 - (float)(_half_moves_count + _moves_count) / 1000;


}



// Fonction qui joue le coup d'une position, renvoyant la meilleure évaluation à l'aide d'un negamax (similaire à un minimax)
float Board::negamax(int depth, float alpha, float beta, int color, bool max_depth, float eval_parameters[3] = default_eval_parameters, bool play = true, bool display = true) {

    // Nombre de noeuds
    if (max_depth) {
        visited_nodes = 1;
        begin_time = clock();
    }
    else {
        visited_nodes++;
    }

    if (depth == 0) {
        evaluate(eval_parameters);
        //evaluate();
        // ??
        return color * _evaluation;
    }

    // à mettre avant depth == 0?
    int g = game_over();
    if (g == 2)
        return 0;
    if (g == -1 || g == 1)
        return -1000 * (depth + 1);
        

    float value = -1e9;
    Board b;

    int best_move = 0;
    float tmp_value;

    if (_got_moves == -1)
        get_moves();

    // b.copy_data(*this);
    // int i1, j1, p1, i2, j2, p2, h;

    // Sort moves à faire
    sort_moves();
    int i;

    for (int j = 0; j < _got_moves; j++) {
        // Pour le triage des coups
        i = _move_order[j];
        // Copie du plateau
        // Opti?? plutôt copier une fois au début, et undo les moves?
        // i1 = _moves[4 * i];
        // j1 = _moves[4 * i + 1];
        // p1 = _array[i1][j1];
        // i2 = _moves[4 * i + 2];
        // j2 = _moves[4 * i + 3];
        // p2 = _array[i2][j2];
        // h = _half_moves_count;

        b.copy_data(*this);

        b.make_index_move(i);
        
        tmp_value = -b.negamax(depth - 1, -beta, -alpha, -color, false, eval_parameters);

        if (max_depth) {
            if (display)
                cout << "move : " << move_label(_moves[4 * i], _moves[4 * i + 1], _moves[4 * i + 2], _moves[4 * i + 3]) << ", value : " << tmp_value << endl;
            if (tmp_value > value)
                best_move = i;
        }
            
        value = max(value, tmp_value);
        alpha = max(alpha, value);
        // undo move
        // b.undo(i1, j1, p1, i2, j2, p2, h);

        if (alpha >= beta)
            break;

    }

    if (max_depth) {
        if (display) {
            cout << "visited nodes : " << (float)(visited_nodes / 1000) << "k" << endl;
            double spent_time = (double)(clock() - begin_time);
            cout << "time spend : " << spent_time << "ms"  << endl;
            cout << "speed : " << visited_nodes / spent_time << "kN/s" << endl;
        }
        if (play) {
            play_index_move_sound(best_move);
            make_index_move(best_move);
        }
            
        return best_move;
    }
    
    return value;
    
}



// Mieux que negamax? tend à supprimer plus de coups (ne marche pas du tout)
float Board::negascout(int depth, float alpha, float beta, int color, bool max_depth) {
    // Nombre de noeuds
    if (max_depth) {
        visited_nodes = 1;
        begin_time = clock();
    }
    else {
        visited_nodes++;
    }

    if (depth == 0) {
        evaluate();
        return color * _evaluation;
    }

    // à mettre avant depth == 0?
    int g = game_over();
    if (g == 2)
        return 0;
    if (g == -1 || g == 1)
        return -1000 * (depth + 1);
        
    // Définition des variables
    int best_move = 0; float tmp_value; Board b; float _a; float _b; int j;
    
    // Génération des coups
    if (_got_moves == -1)
        get_moves();

    // Sort moves à faire
    sort_moves();

    _a = alpha;
    _b = beta;

    for (int i = 0; i < _got_moves; i++) {
        // Pour le triage des coups
        j = _move_order[i];
        

        b.copy_data(*this);
        b.make_index_move(j);
        tmp_value = -b.negascout(depth - 1, -_b, -_a, -color, false);

        if (max_depth) {
            cout << "move : " << move_label(_moves[4 * j], _moves[4 * j + 1], _moves[4 * j + 2], _moves[4 * j + 3]) << ", value : " << tmp_value << endl;
            // Pas sûr pour le choix du meilleur coup
            if (tmp_value > _a)
                best_move = j;
        }

        if ((tmp_value > _a) && (tmp_value < beta) && (i > 0) && (depth > 1)) 
            _a = -b.negascout(depth - 1, -beta, -tmp_value, -color, false);
            
        _a = max(_a, tmp_value);

        if (_a >= beta) {
            if (max_depth)
                make_index_move(best_move);
            return _a;
        }

        _b = _a + 1;

    }

    if (max_depth) {
        cout << "visited nodes : " << (float)(visited_nodes / 1000) << "k" << endl;
        double spent_time = (double)(clock() - begin_time);
        cout << "time spend : " << spent_time << "ms"  << endl;
        cout << "speed : " << visited_nodes / spent_time << "kN/s" << endl;
        make_index_move(best_move);
    }
    
    return _a;
}






// Algorithme PVS
float Board::pvs(int depth, float alpha, float beta, int color, bool max_depth) {
    // Nombre de noeuds
    if (max_depth) {
        visited_nodes = 1;
        begin_time = clock();
    }
    else {
        visited_nodes++;
    }

    if (depth == 0) {
        evaluate();
        return color * _evaluation;
    }

    // à mettre avant depth == 0?
    int g = game_over();
    if (g == 2)
        return 0;
    if (g == -1 || g == 1)
        return -1000 * (depth + 1);
        
    // Définition des variables
    int best_move = 0; float tmp_value; Board b; int j;
    
    // Génération des coups
    if (_got_moves == -1)
        get_moves();

    // Sort moves à faire
    sort_moves();


    for (int i = 0; i < _got_moves; i++) {
        // Pour le triage des coups
        j = _move_order[i];
        

        b.copy_data(*this);
        b.make_index_move(j);


        if (i > 0) {
            tmp_value = -b.pvs(depth - 1, -alpha - 1, -alpha, -color, false);

            if ((alpha < tmp_value) && (tmp_value < beta)) 
                tmp_value = -b.pvs(depth - 1, -beta, -tmp_value, -color, false);
        }

        else {
            tmp_value = -b.pvs(depth - 1, -beta, -alpha, -color, false);
        }


        if (max_depth) {
            cout << "move : " << move_label(_moves[4 * j], _moves[4 * j + 1], _moves[4 * j + 2], _moves[4 * j + 3]) << ", value : " << tmp_value << endl;
            if (tmp_value > alpha)
                best_move = j;
        }
            
        alpha = max(alpha, tmp_value);

        if (alpha >= beta)
            break;

    }

    if (max_depth) {
        cout << "visited nodes : " << (float)(visited_nodes / 1000) << "k" << endl;
        double spent_time = (double)(clock() - begin_time);
        cout << "time spend : " << spent_time << "ms"  << endl;
        cout << "speed : " << visited_nodes / spent_time << "kN/s" << endl;
        make_index_move(best_move);
    }
    
    return alpha;
}


// Fonction qui utilise minimax pour déterminer quel est le "meilleur" coup et le joue
void Board::grogrosfish(int depth) {

    int best_move = 0;
    float best_value = -1e9;
    float value;
    Board b;

    if (_got_moves == -1)
        get_moves();

    // display_moves();
    // cout << "n moves : " << _got_moves << endl;

    for (int i = 0; i < _got_moves; i++) {
        b.copy_data(*this);
        b.make_index_move(i);
        value = -b.negamax(depth - 1, -1e9, 1e9, -_color, false);
        cout << "move : " << move_label(_moves[4 * i], _moves[4 * i + 1], _moves[4 * i + 2], _moves[4 * i + 3]) << ", value : " << value << endl;
        if (value > best_value) {
            best_move = i;
            best_value = value;
        }
    }

    cout << "best move : " << move_label(_moves[4 * best_move], _moves[4 * best_move + 1], _moves[4 * best_move + 2], _moves[4 * best_move + 3]) << ", value : " << best_value << endl;

    make_index_move(best_move);
    
}



// Version un peu mieux optimisée de Grogrosfish
bool Board::grogrosfish2(int depth, float eval_parameters[3] = default_eval_parameters) {
    negamax(depth, -1e9, 1e9, _color, true, eval_parameters);
    to_fen();
    cout << _fen << endl;
    cout << _pgn << endl;
    return true;
}

// Version un peu mieux optimisée de Grogrosfish
void Board::grogrosfish3(int depth) {
    negascout(depth, -1e9, 1e9, _color, true);
    to_fen();
    cout << _fen << endl;
    cout << _pgn << endl;
    PlaySound(move_1_sound);
}

// Test de Grogrofish
void Board::grogrosfish4(int depth) {
    pvs(depth, -1e9, 1e9, _color, true);
    to_fen();
    cout << _fen << endl;
    cout << _pgn << endl;
    PlaySound(move_1_sound);
}


// Test de Grogrofish avec combinaison d'agents
void Board::grogrosfish_multiagents(int depth, int n_agents, float begin_eval_parameters[3], float end_eval_parameters[3]) {
    if (_got_moves == -1)
        get_moves();

    float eval_parameters[3];
    int votes[250];

    // Met la liste des votes à 0 pour chaque coup
    for (int i = 0; i < _got_moves; i++) {
        votes[i] = 0;
    }
    int move;

    // Pour chaque agent
    for (int i = 0; i < n_agents; i++) {
        // Calcul des paramètres de l'agent
        for (int j = 0; j < 3; j++) {
            eval_parameters[j] = (float)(n_agents - i - 1) / (n_agents - 1) * begin_eval_parameters[j] + (float)i / (n_agents - 1) * end_eval_parameters[j];
        }

        // Ajoute à la liste de votes son coup
        move = negamax(depth, -1e9, 1e9, _color, true, eval_parameters, false, false);
        votes[move] += 1;

    }

    // Choix du coup en fonction du nombre de votes
    int max_vote = 0;
    int best_move = 0;

    cout << "Votes : ";

    for (int i = 0; i < _got_moves; i++) {
        cout << votes[i] << "   ";
        if (votes[i] > max_vote) {
            max_vote = votes[i];
            best_move = i;
        }
    }

    cout << "->    Voted move : " << best_move << " (" << 100 * votes[best_move] / n_agents << "%)" << endl;

    make_index_move(best_move);
    to_fen();
    cout << _fen << endl;
    cout << _pgn << endl;
    PlaySound(move_1_sound);
}



// Fonction qui revient à la position précédente (ne marchera pas avec les roques pour le moment)
void Board::undo(int i1, int j1, int p1, int i2, int j2, int p2, int half_moves) {
    _array[i1][j1] = p1;
    _array[i2][j2] = p2;
    // // Implémentation des demi-coups
    _half_moves_count = half_moves;

    _player = !_player;
    _got_moves = -1;
    _color = - _color;

    // Implémentation du FEN possible?
    _fen = "";

    // Implémentation des coups
    !_player && (_moves_count -= 1);
}



// Fonction qui arrange les coups de façon "logique", pour optimiser les algorithmes de calcul
void Board::sort_moves() {
    // Modifier pour seulement garder le (ou les deux) meilleur(s) coups?

    Board b;

    if (_got_moves == -1)
        get_moves();

    float* values = new float[_got_moves];
    float value;

    // Création de la liste des valeurs des évaluations des positions après chaque coup
    // ...
    for (int i = 0; i < _got_moves; i++) {
        // Mise à jour du plateau
        b.copy_data(*this);
        b.make_index_move(i);

        // Evaluation
        b.evaluate();
        value = b._evaluation * _color;

        // Place l'évaluation en i dans les valeurs
        values[i] = value;

        _move_order[i] = i;

        // Insertion de la nouvelle valeur dans le tableau
        for (int j = 0; j < i; j++) {
            if (value > values[j]) {
                for (int k = i; k > j; k--) {
                    values[k] = values[k - 1];
                    _move_order[k] = _move_order[k - 1];
                }
                values[j] = value;
                _move_order[j] = i;
                break;
            }
                
        }

    }

    _sorted_moves = true;
    
}


// Fonction qui récupère le plateau d'un FEN
void Board::from_fen(string fen) {
    // Mise à jour du FEN
    _fen = fen;

    // PGN
    _pgn = "[FEN \"" + fen + "\"]";

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
                    break;
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
    _k_castle_w = false; _q_castle_w = false; _k_castle_b = false; _q_castle_b = false;

    while (next) {
        c = fen[iterator];
        
        switch (c) {
            case '-' : iterator += 1; next = false; break;
            case 'K' : _k_castle_w = true; break;
            case 'Q' : _q_castle_w = true; break;
            case 'k' : _k_castle_b = true; break;
            case 'q' : _q_castle_b = true; iterator += 1; next = false; break;
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

}




// Fonction qui renvoie le FEN du tableau
void Board::to_fen() {
    string s = "";
    int p;
    int it = 0;

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

                switch (p)
                {   
                    case 1: s += "P"; break;
                    case 2: s += "N"; break;
                    case 3: s += "B"; break;
                    case 4: s += "R"; break;
                    case 5: s += "Q"; break;
                    case 6: s += "K"; break;
                    case 7: s += "p"; break;
                    case 8: s += "n"; break;
                    case 9: s += "b"; break;
                    case 10: s += "r"; break;
                    case 11: s += "q"; break;
                    case 12: s += "k"; break;
                }

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

    if (_k_castle_w)
        s += "K";
    if (_q_castle_w)
        s += "Q";
    if (_k_castle_b)
        s += "k";
    if (_q_castle_b)
        s += "q";
    if (!(_k_castle_w | _q_castle_w | _k_castle_b | _q_castle_b))
        s += "-";

    s += " " + _en_passant + " " + to_string(_half_moves_count) + " " + to_string(_moves_count);


    _fen = s;
    
}



// Fonction qui renvoie le gagnant si la partie est finie (-1/1, et 2 pour nulle), et 0 sinon
int Board::game_over() {

    // Si un des rois est décédé
    bool king_w = false;
    bool king_b = false;
    int p;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            if (p == 6) {
                king_w = true;
                if (king_b) {
                    i = 8; j = 8; 
                }
            }
            if (p == 12) {
                king_b = true;
                if (king_w) {
                    i = 8; j = 8; 
                }
            }
        }
    }

    if (!king_w)
        return -1;
    if (!king_b)
        return 1;

    // Manque de matériel

    // Règle des 50 coups
    if (_half_moves_count >= 50)
        return 2;

    // Pat? pas très utile...

    // Répétition de coups? chiant à faire...



    return 0;

}


// Fonction qui renvoie le label d'un coup
// En passant manquant... échecs aussi, puis roques et promotions
string Board::move_label(int i, int j, int k, int l) {
    int p1 = _array[i][j];
    int p2 = _array[k][l];
    string abc = "abcdefgh";

    string s = "";

    switch (p1)
        {   
            case 2: case 8: s += "N"; s += abc[j]; s += char(i + 1 + 48); break;
            case 3: case 9: s += "B"; s += abc[j]; s += char(i + 1 + 48); break;
            case 4: case 10: s += "R"; s += abc[j]; s += char(i + 1 + 48); break;
            case 5: case 11: s += "Q"; s += abc[j]; s += char(i + 1 + 48); break;
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

    // Promotion (en dame seulement pour le moment)
    if ((p1 == 1 && k == 7) || (p1 == 7 && k == 0))
        s += "=Q";
    

    return s;
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
void Board::from_pgn() {

}






// Fonction qui affiche un texte dans une zone donnée
void Board::draw_text_rect(string s, float pos_x, float pos_y, float width, float height, int size) {

    Rectangle rect_text = {pos_x, pos_y, width, height};
    DrawRectangleRec(rect_text, {35, 35, 35, 255});
    // Division du texte
    int sub_div = (1.5 * width) / size;
    int string_size = s.length();
    const char *c;
    int i = 0;
    while (sub_div * i <= string_size) {
        c = s.substr(i * sub_div, sub_div).c_str();
        DrawText(c, pos_x, pos_y + i * size, size, text_color);
        i++;
    }

}




// Fonction qui dessine le plateau
void Board::draw() {

    // Chargement des textures, si pas déjà fait
    if (!loaded_textures) {

        // Plateau
        board_image = LoadImage("../resources/board.png");
        float min_screen = min(screen_height, screen_width);
        board_size = board_scale * min_screen;
        ImageResize(&board_image, board_size, board_size);
        board_texture = LoadTextureFromImage(board_image);
        board_padding_y = (screen_height - board_size) / 2;
        board_padding_x = board_padding_y;

        // Pièces
        piece_images[0] = LoadImage("../resources/w_pawn.png");
        piece_images[1] = LoadImage("../resources/w_knight.png");
        piece_images[2] = LoadImage("../resources/w_bishop.png");
        piece_images[3] = LoadImage("../resources/w_rook.png");
        piece_images[4] = LoadImage("../resources/w_queen.png");
        piece_images[5] = LoadImage("../resources/w_king.png");
        piece_images[6] = LoadImage("../resources/b_pawn.png");
        piece_images[7] = LoadImage("../resources/b_knight.png");
        piece_images[8] = LoadImage("../resources/b_bishop.png");
        piece_images[9] = LoadImage("../resources/b_rook.png");
        piece_images[10] = LoadImage("../resources/b_queen.png");
        piece_images[11] = LoadImage("../resources/b_king.png");

        tile_size = board_size / 8;
        piece_size = tile_size * piece_scale;

        // Génération des textures
        for (int i = 0; i < 12; i++) {
            ImageResize(&piece_images[i], piece_size, piece_size);
            piece_textures[i] = LoadTextureFromImage(piece_images[i]);
        }


        // Chargement du son
        move_1_sound = LoadSound("../resources/move_1.mp3");
        move_2_sound = LoadSound("../resources/move_2.mp3");
        castle_1_sound = LoadSound("../resources/castle_1.mp3");
        castle_2_sound = LoadSound("../resources/castle_2.mp3");
        check_1_sound = LoadSound("../resources/check_1.mp3");
        check_2_sound = LoadSound("../resources/check_2.mp3");
        capture_1_sound = LoadSound("../resources/capture_1.mp3");
        capture_2_sound = LoadSound("../resources/capture_2.mp3");
        checkmate_sound = LoadSound("../resources/checkmate.mp3");
        stealmate_sound = LoadSound("../resources/stealmate.mp3");
        game_begin_sound = LoadSound("../resources/game_begin.mp3");
        game_end_sound = LoadSound("../resources/game_end.mp3");
        // UnloadSound(fxWav);


        PlaySound(game_begin_sound);


        loaded_textures = true;
    }


    // Position de la souris
    mouse_pos = GetMousePosition();
    

    // Si on clique avec la souris
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        highlighted_pos = {-1, -1};
        if (!clicked) {
            clicked_pos = get_pos_from_gui(mouse_pos.x, mouse_pos.y);
            clicked = true;

            // Sélection de pièces
            // Si aucune pièce n'est sélectionnée, la sélectionne
            if (selected_pos.first == -1 && ((_player && is_in(_array[clicked_pos.first][clicked_pos.second], 1, 6)) || (!_player && is_in(_array[clicked_pos.first][clicked_pos.second], 7, 12))))
                selected_pos = get_pos_from_gui(mouse_pos.x, mouse_pos.y);
            // Si une pièce est déjà sélectionnée
            else if (selected_pos.first != -1) {

                // Si le coup est légal, le joue
                bool legal_move = false;
                get_moves();
                for (int i = 0; i < _got_moves; i++) {
                    if (_moves[4 * i] == selected_pos.first && _moves[4 * i + 1] == selected_pos.second && _moves[4 * i + 2] == clicked_pos.first && _moves[4 * i + 3] == clicked_pos.second) {
                        // Le joue
                        play_move_sound(selected_pos.first, selected_pos.second, clicked_pos.first, clicked_pos.second);
                        make_move(selected_pos.first, selected_pos.second, clicked_pos.first, clicked_pos.second);
                        legal_move = true;
                        break;
                    }
                }

                // Déselectionne
                selected_pos = {-1, -1};

                // Changement de sélection de pièce
                if ((_player && is_in(_array[clicked_pos.first][clicked_pos.second], 1, 6)) || (!_player && is_in(_array[clicked_pos.first][clicked_pos.second], 7, 12)))
                    selected_pos = get_pos_from_gui(mouse_pos.x, mouse_pos.y);
            }
        }
        
    }
    else {
        if (clicked && clicked_pos.first != -1 && _array[clicked_pos.first][clicked_pos.second] != 0) {
            pair<int, int> drop_pos = get_pos_from_gui(mouse_pos.x, mouse_pos.y);
            if (is_in(drop_pos.first, 0, 7) && is_in(drop_pos.second, 0, 7) && (clicked_pos.first != drop_pos.first || clicked_pos.second != drop_pos.second)) {
                // Si le coup est légal
                get_moves();
                for (int i = 0; i < _got_moves; i++) {
                    if (_moves[4 * i] == clicked_pos.first && _moves[4 * i + 1] == clicked_pos.second && _moves[4 * i + 2] == drop_pos.first && _moves[4 * i + 3] == drop_pos.second) {
                        play_move_sound(clicked_pos.first, clicked_pos.second, drop_pos.first, drop_pos.second);
                        make_move(clicked_pos.first, clicked_pos.second, drop_pos.first, drop_pos.second);
                        // Déselectionne
                        selected_pos = {-1, -1};
                        break;
                    }
                }
                
            }
        }

        clicked = false;
    }

    // Si on clique avec la souris
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        highlighted_pos = get_pos_from_gui(mouse_pos.x, mouse_pos.y);
    }


    // Dessins


    // Couleur de fond
    ClearBackground(background_color);


    // Plateau
    //DrawTexture(board_texture, board_padding_x, board_padding_y, WHITE);


    // Plateau
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if ((i + j) % 2 == 1)
                DrawRectangle(board_padding_x + tile_size * j, board_padding_y + tile_size * i, tile_size, tile_size, board_color_dark);
            else
                DrawRectangle(board_padding_x + tile_size * j, board_padding_y + tile_size * i, tile_size, tile_size, board_color_light);
        }
    }


    // Surligne du dernier coup joué
    if (_last_move[0] != -1) {
        DrawRectangle(board_padding_x + orientation_index(_last_move[1]) * tile_size, board_padding_y + orientation_index(7 - _last_move[0]) * tile_size, tile_size, tile_size, last_move_color);
        DrawRectangle(board_padding_x + orientation_index(_last_move[3]) * tile_size, board_padding_y + orientation_index(7 - _last_move[2]) * tile_size, tile_size, tile_size, last_move_color);
    }

    // Sélection de cases et de pièces
    if (selected_pos.first != -1) {
        // Affiche la case séléctionnée
        DrawRectangle(board_padding_x + orientation_index(selected_pos.second) * tile_size, board_padding_y + orientation_index(7 - selected_pos.first) * tile_size, tile_size, tile_size, select_color);
        if (_got_moves == -1)
            get_moves();
        // Affiche les coups possibles pour la pièce séléctionnée
        for (int i = 0; i < _got_moves; i++) {
            if (_moves[4 * i] == selected_pos.first && _moves[4 * i + 1] == selected_pos.second) {
                DrawRectangle(board_padding_x + orientation_index(_moves[4 * i + 3]) * tile_size, board_padding_y + orientation_index(7 - _moves[4 * i + 2]) * tile_size, tile_size, tile_size, select_color);
            }
        }
    }

    // Affichage de la case surlignée
    if (highlighted_pos.first != -1) {
        DrawRectangle(board_padding_x + orientation_index(highlighted_pos.second) * tile_size, board_padding_y + orientation_index(7 - highlighted_pos.first) * tile_size, tile_size, tile_size, highlight_color);
    }


    // Pièces
    int p;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            if (p > 0) {
                if (clicked && i == clicked_pos.first && j == clicked_pos.second)
                    DrawTexture(piece_textures[p - 1], mouse_pos.x - piece_size / 2, mouse_pos.y - piece_size / 2, WHITE);
                else
                    DrawTexture(piece_textures[p - 1], board_padding_x + tile_size * orientation_index(j) + (tile_size - piece_size) / 2, board_padding_y + tile_size * orientation_index(7 - i) + (tile_size - piece_size) / 2, WHITE);
            }

        }
    }


    // Texte
    DrawText("Grogrosfish engine", board_padding_x, 10, 32, text_color);

    // FEN
    if (_fen == "")
        to_fen();
    const char *fen = _fen.c_str();
    DrawText(fen, board_padding_x, screen_height - 30, 20, text_color);


    // PGN
    draw_text_rect(_pgn, board_padding_x + board_size + 20, board_padding_y, screen_width - (board_padding_x + board_size + 20) - 20, board_size, 20);

}



// Fonction qui joue le son d'un coup
void Board::play_move_sound(int i, int j, int k, int l) {
    // Pièces
    int p1 = _array[i][j];
    int p2 = _array[k][l];


    

    // Prises
    if (p2 != 0) {
        if (_player)
            PlaySound(capture_1_sound);
        else
            PlaySound(capture_2_sound);
    }
    
    // Roques
    if (p1 == 6 && abs(j - l) == 2)
        PlaySound(castle_1_sound);
    if (p1 == 12 && abs(j - l) == 2)
        PlaySound(castle_2_sound);


    // Echecs
    Board b(*this);
    b.make_move(i, j, k, l);

    if (b.in_check()) {
        if (_player)
            PlaySound(check_1_sound);
        else
            PlaySound(check_2_sound);
    }

    // Coup "normal"
    else if (p2 == 0) {
        if (_player && (p1 != 6 || abs(j - l) != 2))
            return PlaySound(move_1_sound);
        if (!_player && (p1 != 12 || abs(j - l) != 2))
            return PlaySound(move_2_sound);
    }


    // Mats à rajouter



}


// Fonction qui joue le son d'un coup à partir de son index
void Board::play_index_move_sound(int i) {
    play_move_sound(_moves[4 * i], _moves[4 * i + 1], _moves[4 * i + 2], _moves[4 * i + 3]);
}




// Fonction qui obtient la case correspondante à la position sur la GUI
pair<int, int> get_pos_from_gui(int x, int y) {
    pair<int, int> coord;


    if (!is_in(x, board_padding_x, board_padding_x + board_size) || !is_in(y, board_padding_y, board_padding_y + board_size))
        return {-1, -1};
    else
        return {orientation_index(8 - (y - board_padding_y) / tile_size), orientation_index((x - board_padding_x) / tile_size)};

    return coord;
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