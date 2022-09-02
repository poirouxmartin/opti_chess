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
    static const string abc = "abcdefgh";

    // Joueur avec les pièces blanches
    if (_player) {
        // Poussée (de 1)
        (_array[i + 1][j] == 0) && add_move(i, j, i + 1, j, iterator);
        // Poussée (de 2)
        (i == 1 && _array[i + 1][j] == 0 && _array[i + 2][j] == 0) && add_move(i, j, i + 2, j, iterator);
        // Prise (gauche)
        (j > 0 && (is_in(_array[i + 1][j - 1], 7, 12) || (_en_passant[0] == abc[j - 1] && (int)_en_passant[1] - 48 - 1 == i + 1))) && add_move(i, j, i + 1, j - 1, iterator);
        // Prise (droite)
        (j < 7 && (is_in(_array[i + 1][j + 1], 7, 12) || (_en_passant[0] == abc[j + 1] && _en_passant[1] - 48 - 1 == i + 1))) && add_move(i, j, i + 1, j + 1, iterator);
    }
    // Joueur avec les pièces noires
    else {
        // Poussée (de 1)
        (_array[i - 1][j] == 0) && add_move(i, j, i - 1, j, iterator);
        // Poussée (de 2)
        (i == 6 && _array[i - 1][j] == 0 && _array[i - 2][j] == 0) && add_move(i, j, i - 2, j, iterator);
        // Prise (gauche)
        (j > 0 && (is_in(_array[i - 1][j - 1], 1, 6) || (_en_passant[0] == abc[j - 1] && _en_passant[1] - 48 - 1 == i - 1))) && add_move(i, j, i - 1, j - 1, iterator);
        // Prise (droite)
        (j < 7 && (is_in(_array[i - 1][j + 1], 1, 6) || (_en_passant[0] == abc[j + 1] && _en_passant[1] - 48 - 1 == i - 1))) && add_move(i, j, i - 1, j + 1, iterator);
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



// Calcule la liste des coups possibles. pseudo ici fait référence au droit de roquer en passant par une position illégale.
bool Board::get_moves(bool pseudo, bool forbide_check) {

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
            }
            if (p == 12) {
                king_b = true;
            }
        }
    }

    if (!king_w || !king_b) {
        _got_moves = 0;
        return true;
    }
        




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
                    if (_player && _q_castle_w && _array[i][j - 1] == 0 && _array[i][j - 2] == 0 && _array[i][j - 3] == 0 && (pseudo || (!attacked(i, j) && !attacked(i, j - 1) && !attacked(i, j - 2))))
                        add_move(i, j, i, j - 2, &iterator);
                    // Petit
                    if (_player && _k_castle_w && _array[i][j + 1] == 0 && _array[i][j + 2] == 0 && (pseudo || (!attacked(i, j) && !attacked(i, j + 1) && !attacked(i, j + 2))))
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
                    if (!_player && _q_castle_b && _array[i][j - 1] == 0 && _array[i][j - 2] == 0 && _array[i][j - 3] == 0 && (pseudo || (!attacked(i, j) && !attacked(i, j - 1) && !attacked(i, j - 2))))
                        add_move(i, j, i, j - 2, &iterator);
                    // Petit
                    if (!_player && _k_castle_b && _array[i][j + 1] == 0 && _array[i][j + 2] == 0 && (pseudo || (!attacked(i, j) && !attacked(i, j + 1) && !attacked(i, j + 2))))
                        add_move(i, j, i, j + 2, &iterator);
                    break;

            }

        }
    }

    _moves[iterator] = -1;
    _got_moves = iterator / 4;


    // Vérification échecs
    if (forbide_check) {
        int new_moves[1000];
        int n_moves = 0;
        Board b;


        for (int i = 0; i < _got_moves; i++) {
            b.copy_data(*this);
            b.make_index_move(i, false);
            b._color = - b._color;
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
    (b._got_moves == -1 && b.get_moves(true));
    //b.display_moves();
    for (int m = 0; m < b._got_moves; m++) {
        //cout << "move : " << move_label(b._moves[4 * m], b._moves[4 * m + 1], b._moves[4 * m + 2], b._moves[4 * m + 3]) << endl;
        if (i == b._moves[4 * m + 2] && j == b._moves[4 * m + 3]) {
            //cout << "toto" << endl;
            return true;
        }
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
void Board::make_move(int i, int j, int k, int l, bool pgn, bool new_board) {
    int p = _array[i][j];
    if (pgn) {
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


    _new_board = true;

    if (new_board) {
        delete_all();
        _tested_moves = 0;
        _current_move = 0;
        _nodes = 0;
        _evaluated = false;
    }

}


// Fonction qui joue le coup i
void Board::make_index_move(int i, bool pgn) {
    // if (i < 0 | i >= _got_moves)
    //     cout << "move index out of range" << endl;
    // else {
    //     int k = 4 * i;
    //     make_move(_moves[k], _moves[k + 1], _moves[k + 2], _moves[k + 3]);
    // }

    int k = 4 * i;
    make_move(_moves[k], _moves[k + 1], _moves[k + 2], _moves[k + 3], pgn);
}



// Fonction qui renvoie l'avancement de la partie (0 = début de partie, 1 = fin de partie)
float Board::game_advancement() {
    // Définition personnelle de l'avancement d'une partie : (p_tot - p) / p_tot, où p_tot = le total matériel (du joueur adverse? ou les deux?) en début de partie, et p = le total matériel (du joueur adverse? ou les deux?) actuellement
    float adv_pawn = 0.1;
    float adv_knight = 1.0;
    float adv_bishop = 1.0;
    float adv_rook = 1.0;
    float adv_queen = 3.0;

    float p_tot = 2 * (8 * adv_pawn + 2 * adv_knight + 2 * adv_bishop + 2 * adv_rook + 1 * adv_queen);
    float p = 0;

    int piece;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            piece = _array[i][j];
           
            switch (piece)
            {   
                case 0: break;
                case 1: case 7: p += adv_pawn; break;
                case 2: case 8: p += adv_knight; break;
                case 3: case 9: p += adv_bishop; break;
                case 4: case 10: p += adv_rook; break;
                case 5: case 11: p += adv_queen; break;
            }

        }
    }


    return (p_tot - p) / p_tot;

}



// Fonction qui évalue la position à l'aide d'heuristiques
void Board::evaluate(Evaluator eval, bool check_all) {

    if (check_all) {
        get_moves(false, true);
        if (_got_moves == 0) {
            if (in_check())
                _evaluation = - _color * 1000000;
            else
                _evaluation = 0;
            return;
        }
    }
    

    _evaluation = 0;

    // int g = game_over();
    // if (g == -1) {
    //     _evaluation = -10e7;
    //     return;
    // }
    // if (g == 1) {
    //     _evaluation = 10e7;
    //     return;
    // }
    // if (g == 2) {
    //     _evaluation = 0;
    //     return;
    // }

    // à tester: changer les boucles par des for (i : array) pour optimiser
    int p;
    int bishop_w = 0; int bishop_b = 0;

    // Avancement de la partie
    float adv = game_advancement();
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
           
            switch (p)
            {   
                case 0: break;
                case 1:  _evaluation += (eval._pawn_value_begin   * (1 - adv) + eval._pawn_value_end   * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_pawn_begin[7 - i][j]   * (1 - adv) + eval._pos_pawn_end[7 - i][j]   * adv); break;
                case 2:  _evaluation += (eval._knight_value_begin * (1 - adv) + eval._knight_value_end * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_knight_begin[7 - i][j] * (1 - adv) + eval._pos_knight_end[7 - i][j] * adv); break;
                case 3:  _evaluation += (eval._bishop_value_begin * (1 - adv) + eval._bishop_value_end * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_bishop_begin[7 - i][j] * (1 - adv) + eval._pos_bishop_end[7 - i][j] * adv); bishop_w += 1; break;
                case 4:  _evaluation += (eval._rook_value_begin   * (1 - adv) + eval._rook_value_end   * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_rook_begin[7 - i][j]   * (1 - adv) + eval._pos_rook_end[7 - i][j]   * adv); break;
                case 5:  _evaluation += (eval._queen_value_begin  * (1 - adv) + eval._queen_value_end  * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_queen_begin[7 - i][j]  * (1 - adv) + eval._pos_queen_end[7 - i][j]  * adv); break;
                case 6:  _evaluation += (eval._king_value_begin   * (1 - adv) + eval._king_value_end   * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_king_begin[7 - i][j]   * (1 - adv) + eval._pos_king_end[7 - i][j]   * adv); break;
                case 7:  _evaluation -= (eval._pawn_value_begin   * (1 - adv) + eval._pawn_value_end   * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_pawn_begin[i][j]       * (1 - adv) + eval._pos_pawn_end[i][j]       * adv); break;
                case 8:  _evaluation -= (eval._knight_value_begin * (1 - adv) + eval._knight_value_end * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_bishop_begin[i][j]     * (1 - adv) + eval._pos_queen_end[i][j]      * adv); break;
                case 9:  _evaluation -= (eval._bishop_value_begin * (1 - adv) + eval._bishop_value_end * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_bishop_begin[i][j]     * (1 - adv) + eval._pos_bishop_end[i][j]     * adv); bishop_b += 1; break;
                case 10: _evaluation -= (eval._rook_value_begin   * (1 - adv) + eval._rook_value_end   * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_rook_begin[i][j]       * (1 - adv) + eval._pos_rook_end[i][j]       * adv); break;
                case 11: _evaluation -= (eval._queen_value_begin  * (1 - adv) + eval._queen_value_end  * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_queen_begin[i][j]      * (1 - adv) + eval._pos_queen_end[i][j]      * adv); break;
                case 12: _evaluation -= (eval._king_value_begin   * (1 - adv) + eval._king_value_end   * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_king_begin[i][j]       * (1 - adv) + eval._pos_king_end[i][j]       * adv); break;
            }

        }
    }


    // Paire de oufs
    if (eval._bishop_pair != 0)
        _evaluation += eval._bishop_pair * ((bishop_w >= 2) - (bishop_b >= 2));

    // Droits de roques
    if (eval._castling_rights != 0)
        _evaluation += eval._castling_rights * (_k_castle_w + _q_castle_w - _k_castle_b - _q_castle_b) * (1 - adv);

    if (eval._random_add != 0)
        _evaluation += GetRandomValue(-50, 50) * eval._random_add / 100;

    if (eval._piece_activity != 0) {
        if (_got_moves == -1)
            get_moves();
        _evaluation += _color * _got_moves * eval._piece_activity;
    }
    
        

    // Pour éviter les répétitions (ne fonctionne pas)
    //_evaluation *= 1 - (float)(_half_moves_count + _moves_count) / 1000;

    _evaluated = true;

    return;
}


// Fonction qui évalue la position à l'aide d'heuristiques -> évaluation entière
void Board::evaluate_int(Evaluator eval, bool check_all) {

    evaluate(eval, check_all);
    _evaluation = (int)(100 * _evaluation);

    return;

}


// Fonction qui joue le coup d'une position, renvoyant la meilleure évaluation à l'aide d'un negamax (similaire à un minimax)
float Board::negamax(int depth, float alpha, float beta, int color, bool max_depth, Evaluator eval, Agent a, bool use_agent, bool play = false, bool display = false) {

    // Nombre de noeuds
    if (max_depth) {
        visited_nodes = 1;
        begin_time = clock();
    }
    else {
        visited_nodes++;
    }

    if (depth == 0) {
        if (use_agent)
            evaluate(a);
        else
            evaluate(eval);
        return color * _evaluation;
    }

    // à mettre avant depth == 0?
    int g = game_over();
    if (g == 2)
        return 0;
    if (g == -1 || g == 1)
        return -1e7 * (depth + 1);
    if (g == -10 || g == 10)
        return -1e8 * (depth + 1);
        

    float value = -1e9;
    Board b;

    int best_move = 0;
    float tmp_value;

    (_got_moves == -1 && get_moves());

    // b.copy_data(*this);
    // int i1, j1, p1, i2, j2, p2, h;

    // Sort moves à faire
    if (depth > 1)
        sort_moves(eval);
    int i;


    // Parallélisation?

    //parallel_for (0, _got_moves, [&](int j)) { ... }

    // vector<int> moves_vector = {};
    // for (int i = 0; i < _got_moves; i++) {
    //     moves_vector.push_back(_move_order[i]);
    // }


    // for_each(execution::par_unseq, moves_vector.begin(), moves_vector.end(),
    // [](auto&& i) {


    // });


    for (int j = 0; j < _got_moves; j++) {

        
        // Pour le triage des coups
        if (depth > 1)
            i = _move_order[j];
        else
            i = j;


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
        
        tmp_value = -b.negamax(depth - 1, -beta, -alpha, -color, false, eval, a, use_agent);

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
            if (display)
                make_index_move(best_move, true);
        }
            
        return best_move;
    }
    
    return value;
    
}



// Mieux que negamax? tend à supprimer plus de coups (ne marche pas du tout)
float Board::negascout(int depth, float alpha, float beta, int color, bool max_depth, Evaluator eval) {
    // Nombre de noeuds
    if (max_depth) {
        visited_nodes = 1;
        begin_time = clock();
    }
    else {
        visited_nodes++;
    }

    if (depth == 0) {
        evaluate(eval);
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
    get_moves();

    // Sort moves à faire
    sort_moves(eval);

    _a = alpha;
    _b = beta;

    for (int i = 0; i < _got_moves; i++) {
        // Pour le triage des coups
        j = _move_order[i];
        

        b.copy_data(*this);
        b.make_index_move(j);
        tmp_value = -b.negascout(depth - 1, -_b, -_a, -color, false, eval);

        if (max_depth) {
            cout << "move : " << move_label(_moves[4 * j], _moves[4 * j + 1], _moves[4 * j + 2], _moves[4 * j + 3]) << ", value : " << tmp_value << endl;
            // Pas sûr pour le choix du meilleur coup
            if (tmp_value > _a)
                best_move = j;
        }

        if ((tmp_value > _a) && (tmp_value < beta) && (i > 0) && (depth > 1)) 
            _a = -b.negascout(depth - 1, -beta, -tmp_value, -color, false, eval);
            
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
float Board::pvs(int depth, float alpha, float beta, int color, bool max_depth, Evaluator eval) {
    // Nombre de noeuds
    if (max_depth) {
        visited_nodes = 1;
        begin_time = clock();
    }
    else {
        visited_nodes++;
    }

    if (depth == 0) {
        evaluate(eval);
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
    get_moves();

    // Sort moves à faire
    sort_moves(eval);


    for (int i = 0; i < _got_moves; i++) {
        // Pour le triage des coups
        j = _move_order[i];
        

        b.copy_data(*this);
        b.make_index_move(j);


        if (i > 0) {
            tmp_value = -b.pvs(depth - 1, -alpha - 1, -alpha, -color, false, eval);

            if ((alpha < tmp_value) && (tmp_value < beta)) 
                tmp_value = -b.pvs(depth - 1, -beta, -tmp_value, -color, false, eval);
        }

        else {
            tmp_value = -b.pvs(depth - 1, -beta, -alpha, -color, false, eval);
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
// void Board::grogrosfish(int depth, Evaluator eval) {
//     int best_move = 0;
//     float best_value = -1e9;
//     float value;
//     Board b;

//     if (_got_moves == -1)
//         get_moves();

//     // display_moves();
//     // cout << "n moves : " << _got_moves << endl;

//     for (int i = 0; i < _got_moves; i++) {
//         b.copy_data(*this);
//         b.make_index_move(i);
//         value = -b.negamax(depth - 1, -1e9, 1e9, -_color, false, eval);
//         cout << "move : " << move_label(_moves[4 * i], _moves[4 * i + 1], _moves[4 * i + 2], _moves[4 * i + 3]) << ", value : " << value << endl;
//         if (value > best_value) {
//             best_move = i;
//             best_value = value;
//         }
//     }

//     cout << "best move : " << move_label(_moves[4 * best_move], _moves[4 * best_move + 1], _moves[4 * best_move + 2], _moves[4 * best_move + 3]) << ", value : " << best_value << endl;

//     make_index_move(best_move);
    
// }



// Version un peu mieux optimisée de Grogrosfish
bool Board::grogrosfish2(int depth, Evaluator eval, bool display = false) {
    Agent a;
    negamax(depth, -1e9, 1e9, _color, true, eval, a, false, true, display);
    if (display) {
        evaluate(eval);
        to_fen();
        cout << _fen << endl;
        cout << _pgn << endl;
    }
    
    return true;
}

// Version un peu mieux optimisée de Grogrosfish
bool Board::grogrosfish2(int depth, Agent a, bool display = false) {
    Evaluator eval;
    negamax(depth, -1e9, 1e9, _color, true, eval, a, true, true, display);
    if (display) {
        evaluate(a);
        to_fen();
        cout << _fen << endl;
        cout << _pgn << endl;
    }
    
    return true;
}



// Version un peu mieux optimisée de Grogrosfish
void Board::grogrosfish3(int depth, Evaluator eval) {
    negascout(depth, -1e9, 1e9, _color, true, eval);
    to_fen();
    cout << _fen << endl;
    cout << _pgn << endl;
}

// Test de Grogrofish
void Board::grogrosfish4(int depth, Evaluator eval) {
    pvs(depth, -1e9, 1e9, _color, true, eval);
    to_fen();
    cout << _fen << endl;
    cout << _pgn << endl;
}


// Test de Grogrofish avec combinaison d'agents
// void Board::grogrosfish_multiagents(int depth, int n_agents, Evaluator eval_begin, Evaluator eval_end) {
//     if (_got_moves == -1)
//         get_moves();

//     float eval_parameters[4];
//     int votes[250];

//     // Met la liste des votes à 0 pour tous les coups
//     for (int i = 0; i < _got_moves; i++) {
//         votes[i] = 0;
//     }
//     int move;

//     // Pour chaque agent
//     for (int i = 0; i < n_agents; i++) {
//         // Calcul des paramètres de l'agent
//         for (int j = 0; j < 4; j++) {
//             eval_parameters[j] = (float)(n_agents - i - 1) / (n_agents - 1) * begin_eval_parameters[j] + (float)i / (n_agents - 1) * end_eval_parameters[j];
//         }

//         // Ajoute à la liste de votes son coup
//         move = negamax(depth, -1e9, 1e9, _color, true, eval_parameters);
//         votes[move] += 1;

//     }

//     // Choix du coup en fonction du nombre de votes
//     int max_vote = 0;
//     int best_move = 0;

//     cout << "Votes : ";

//     for (int i = 0; i < _got_moves; i++) {
//         cout << votes[i] << "   ";
//         if (votes[i] > max_vote) {
//             max_vote = votes[i];
//             best_move = i;
//         }
//     }

//     cout << "->    Voted move : " << move_label_from_index(best_move) << " (" << 100 * votes[best_move] / n_agents << "%)" << endl;

//     play_index_move_sound(best_move);
//     make_index_move(best_move);
//     to_fen();
//     cout << _fen << endl;
//     cout << _pgn << endl;
// }



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
void Board::sort_moves(Evaluator eval) {
    // Modifier pour seulement garder le (ou les deux) meilleur(s) coups?

    Board b;

    (_got_moves == -1 && get_moves());

    float* values = new float[_got_moves];
    float value;

    // Création de la liste des valeurs des évaluations des positions après chaque coup
    // ...
    for (int i = 0; i < _got_moves; i++) {
        // Mise à jour du plateau
        b.copy_data(*this);
        b.make_index_move(i);

        // Evaluation
        b.evaluate(eval);
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

    _got_moves = -1;

    _new_board = true;

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

    // Règle des 50 coups
    if (_half_moves_count >= 50)
        return 2;

    // Si un des rois est décédé
    bool king_w = false;
    bool king_b = false;

    // Position du roi s'il existe (pour les mats et pats)
    pair<int, int> king_pos;
    pair<int, int> opponent_king_pos;

    int p;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            if (p == 6) {
                king_w = true;
                /*if (_player)
                    king_pos = {i, j};
                else
                    opponent_king_pos = {i, j};
                if (king_b) {
                    i = 8; j = 8; 
                }*/
            }
            if (p == 12) {
                king_b = true;
                /*if (!_player)
                    king_pos = {i, j};
                else
                    opponent_king_pos = {i, j};
                if (king_w) {
                    i = 8; j = 8; 
                }*/
            }
        }
    }

    if (!king_w)
        return -1;
    if (!king_b)
        return 1;

    /*
    _player = !_player;
    bool att = attacked(opponent_king_pos.first, opponent_king_pos.second);
    _player = !_player;
    // if (att)
    //     return -10 * _color; 

    // Manque de matériel

    // Pat? pas très utile... (et Mat...)
    if (_got_moves == -1)
        get_moves();

    bool attacked_king = true;
    Board b;
    pair<int, int> new_king_pos;
    for (int i = 0; i < _got_moves; i++) {
        b.copy_data(*this);
        b.make_index_move(i);
        b._player = !b._player;
        new_king_pos = b.get_king_pos();
        if (!b.attacked(new_king_pos.first, new_king_pos.second)) {
            attacked_king = false;
            break;
        }
    }

    if (attacked_king) {
        // Echec et mat
        if (attacked(king_pos.first, king_pos.second)) {
            //return - _color;
        }
        // Pat
        else {
            return 2;
        }
    }
    */
    

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


// Fonction qui renvoie le label d'un coup en fonction de son index
string Board::move_label_from_index(int i) {
    // Pour pas qu'il re écrase les moves
    if (_got_moves == -1)
        get_moves();
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
void Board::from_pgn() {

}






// Fonction qui affiche un texte dans une zone donnée
void Board::draw_text_rect(string s, float pos_x, float pos_y, float width, float height, int size) {

    Rectangle rect_text = {pos_x, pos_y, width, height};
    DrawRectangleRec(rect_text, background_text_color);
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
        // board_image = LoadImage("../resources/board.png");
        float min_screen = min(screen_height, screen_width);
        board_size = board_scale * min_screen;
        // ImageResize(&board_image, board_size, board_size);
        // board_texture = LoadTextureFromImage(board_image);
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
        arrow_thickness = tile_size * arrow_scale;

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


        // Icône
        icon = LoadImage("../resources/grogros_zero.png");

        SetWindowIcon(icon);


        loaded_textures = true;
    }


    // Position de la souris
    mouse_pos = GetMousePosition();
    

    // Si on clique avec la souris
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        // Retire les highlight
        for (int i = 0; i < 8; i++)
            for (int j = 0; j < 8; j++)
                highlighted_array[i][j] = 0;

        // Si on clique
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
                get_moves(false, true);
                for (int i = 0; i < _got_moves; i++) {
                    if (_moves[4 * i] == selected_pos.first && _moves[4 * i + 1] == selected_pos.second && _moves[4 * i + 2] == clicked_pos.first && _moves[4 * i + 3] == clicked_pos.second) {
                        // Le joue
                        play_move_sound(selected_pos.first, selected_pos.second, clicked_pos.first, clicked_pos.second);
                        make_move(selected_pos.first, selected_pos.second, clicked_pos.first, clicked_pos.second, true, true);
                        cout << _pgn << endl;
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
            if (is_in(drop_pos.first, 0, 7) && is_in(drop_pos.second, 0, 7)) {
                // Déselection de la pièce si on reclique dessus
                if (drop_pos.first == selected_pos.first && drop_pos.second == selected_pos.second) {
                }
                else {
                    // Si le coup est légal
                    get_moves(false, true);
                    for (int i = 0; i < _got_moves; i++) {
                        if (_moves[4 * i] == clicked_pos.first && _moves[4 * i + 1] == clicked_pos.second && _moves[4 * i + 2] == drop_pos.first && _moves[4 * i + 3] == drop_pos.second) {
                            play_move_sound(clicked_pos.first, clicked_pos.second, drop_pos.first, drop_pos.second);
                            make_move(clicked_pos.first, clicked_pos.second, drop_pos.first, drop_pos.second, true, true);
                            cout << _pgn << endl;
                            selected_pos = {-1, -1};
                            break;
                        }
                    }
                }
                
            }
        }

        clicked = false;
    }

    // Si on clique avec la souris
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
        int x_mouse = get_pos_from_gui(mouse_pos.x, mouse_pos.y).first;
        int y_mouse = get_pos_from_gui(mouse_pos.x, mouse_pos.y).second;
        highlighted_array[x_mouse][y_mouse] = 1 - highlighted_array[x_mouse][y_mouse];
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

    // Cases surglignées
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            if (highlighted_array[i][j])
                DrawRectangle(board_padding_x + tile_size * orientation_index(j), board_padding_y + tile_size * orientation_index(7 - i), tile_size, tile_size, highlight_color);


    // Sélection de cases et de pièces
    if (selected_pos.first != -1) {
        // Affiche la case séléctionnée
        DrawRectangle(board_padding_x + orientation_index(selected_pos.second) * tile_size, board_padding_y + orientation_index(7 - selected_pos.first) * tile_size, tile_size, tile_size, select_color);
        get_moves(false, true);
        // Affiche les coups possibles pour la pièce séléctionnée
        for (int i = 0; i < _got_moves; i++) {
            if (_moves[4 * i] == selected_pos.first && _moves[4 * i + 1] == selected_pos.second) {
                DrawRectangle(board_padding_x + orientation_index(_moves[4 * i + 3]) * tile_size, board_padding_y + orientation_index(7 - _moves[4 * i + 2]) * tile_size, tile_size, tile_size, select_color);
            }
        }
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

    // Coups auquel l'IA réflechit...
    draw_monte_carlo_arrows();


    // Texte
    DrawText("Grogrosfish engine", board_padding_x, 10, 32, text_color);

    // Joueurs de la partie
    DrawText(_player_2, board_padding_x, board_padding_y - 32 + (board_size + 40) * !board_orientation, 24, text_color);
    DrawText(_player_1, board_padding_x, board_padding_y - 32 + (board_size + 40) * board_orientation, 24, text_color);


    // Temps des joueurs
    DrawText(to_string(_time_player_2 / 1000).c_str(), board_padding_x + board_size - 72, board_padding_y - 32 + (board_size + 40) * !board_orientation, 24, text_color);
    DrawText(to_string(_time_player_1 / 1000).c_str(), board_padding_x + board_size - 72, board_padding_y - 32 + (board_size + 40) * board_orientation, 24, text_color);


    // FEN
    if (_fen == "")
        to_fen();
    const char *fen = _fen.c_str();
    DrawText(fen, board_padding_x, screen_height - 30, 20, text_color);


    // PGN
    draw_text_rect(_pgn, board_padding_x + board_size + 20, board_padding_y, screen_width - (board_padding_x + board_size + 20) - 100, board_size, 20);

    // Evaluation

    // Peut faire tout planter
    DrawText(to_string(_evaluation).c_str(), screen_width - 200, screen_height - 30, 20, text_color);

}



// Fonction qui joue le son d'un coup
void Board::play_move_sound(int i, int j, int k, int l) {
    // Pièces
    int p1 = _array[i][j];
    int p2 = _array[k][l];

    // Echecs
    Board b(*this);
    b.make_move(i, j, k, l);

    if (b.in_check()) {
        if (_player)
            PlaySound(check_1_sound);
        else
            PlaySound(check_2_sound);
    }

    // Si pas d'échecs
    else {

        // Prises
        if (p2 != 0) {
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


// Evaluation par un agent
void Board::evaluate(Agent a) {

    _evaluation = 0;
    int p;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            if (p)
                _evaluation += a._input_layer[64 * _array[i][j] + 8 * i + j];
        }
    }

    return;

}

 // Renvoie le meilleur coup selon l'agent, en utilisant l'algorithme de Monte-Carlo avec n noeuds, d'une profondeur depth
void Board::monte_carlo(Agent a, int n, int depth_calc, int depth, bool display = false) {

    if (game_over() != 0)
        return;

    get_moves();

    // if (_got_moves == 0) {
    //     cout << "the game is over." << endl;
    //     return;
    // }

    // au départ, envoie les noeuds équitablement dans chaque branche

    int i = 0;
    int j;
    int d;
    Board b;
    int move;
    int evaluations[250][2];
    int eval;


    // Met toutes les évaluations des coups à 0
    for (j = 0; j < _got_moves; j++) {
        evaluations[j][0] = 0;
        evaluations[j][1] = 0;
    }


    Evaluator evaluator;

    while (i < n || i < _got_moves) {

        // Joue un coup sur le plateau d'origine
        // A faire : mettre plus de noeuds sur les meilleurs coups
        move = i%_got_moves;
        b.copy_data(*this);
        b.make_index_move(move);

        // Avec la profondeur donnée, joue de façon aléatoire
        for (d = 0; d < depth; d++) {
            if (b._got_moves == -1)
                b.get_moves();
            if (b._got_moves)
                b.make_index_move(GetRandomValue(0, b._got_moves - 1));
            else
                break;
        }

        // Evalue le plateau après ces coups aléatoires
        b.evaluate(a);

        //b.evaluate(evaluator);

        // Il faut raisonner un peu quand le roi décède, sinon, cela fausse un peu tout...
        // if (eval > 10000)
        //     eval = 10000;
        // if (eval <= -10000)
        //     eval = -10000;

        // Stocke l'évaluation sur l'index du coup associé
        evaluations[move][0] += b._evaluation;
        evaluations[move][1] += 1;
        i += 1;
        
    }

    int e;

    int best_move = 0;
    int best_value = _color * 10e9;


    for (j = 0; j < _got_moves; j++) {
        e = evaluations[j][0] / evaluations[j][1];
        if (display)
            cout << e << " | ";
        if (e * _color > best_value * _color) {
            best_value = e;
            best_move = j;
        }
        
    }

    make_index_move(best_move, true);
    //make_index_move(best_move, display);
    
    if (display) {
        cout << endl;
        cout << _pgn << endl;
    }


    _evaluated = true;

    return;

}



// Test iterative depth

// Problème --> voir endgame.. une dame de plus, mais Monte Carlo donne toujours une eval bof bof
// Complètement buggé...
void Board::monte_carlo_2(Agent a, Evaluator e, int nodes, bool use_agent, bool display, int depth) {
    // static Board children_list[100000];
    // static int index_children = 0;
    static int max_depth;

    if (depth == 0 & _new_board) {
        max_depth = 0;
        // Stockage des plateaux --> à implémenter... gros stockage global, et utilisation des index pour partager
        
    }

    if (depth > max_depth) {
        max_depth = depth;
        if (display) {
            cout << "Monte-Carlo - depth : " << max_depth << '\r';
            to_fen();
            cout << _fen << endl;
        }
    }

    // Obtention des coups jouables
    get_moves(false, true);

    if (_got_moves == 0)
        return;

    if (_new_board) {
        _eval_children = new int[_got_moves];
        _nodes_children = new int[_got_moves];
        for (int i = 0; i < _got_moves; i++) {
            _nodes_children[i] = 0;
            _eval_children[i] = 0;
        }

        // Liste des plateaux fils
        _children = new Board[_got_moves];
        // _index_children = index_children; // Test de la nouvelle méthode d'allocation mémoire
        // index_children += _got_moves;

        _tested_moves = 0;
        _current_move = 0;

        _new_board = false;
    }


    // Tant qu'il reste des noeuds à calculer...
    while (nodes > 0) {

        // Choix du coup à jouer pour l'exploration

        // Si tous les coups de la position ne sont pas encore testés
        if (_tested_moves < _got_moves) {

            // Joue un nouveau coup
            Board b_child(*this); // Vérifier la copie...
            _children[_current_move] = b_child;
            _children[_current_move].make_index_move(_current_move);
            // children_list[_index_children + _current_move] = b_child;
            // children_list[_index_children + _current_move].make_index_move(_current_move);

            // Evalue une première fois la position, puis stocke dans la liste d'évaluation des coups
            if (use_agent)
                _children[_current_move].evaluate(a);
            else
                _children[_current_move].evaluate_int(e, true);
            // children_list[_index_children + _current_move].evaluate_int(e);
            _eval_children[_current_move] = _children[_current_move]._evaluation;
            // _eval_children[_current_move] = children_list[_index_children + _current_move]._evaluation;
            _nodes_children[_current_move]++;

            // Actualise la valeur d'évaluation du plateau

            // Première évaluation
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
            _current_move = pick_random_good_move(_eval_children, _got_moves, _color, false);

            // Va une profondeur plus loin... appel récursif sur Monte-Carlo
            _children[_current_move].monte_carlo_2(a, e, 1, use_agent, display, depth + 1);
            // children_list[_index_children + _current_move].monte_carlo_2(a, e, 1, depth + 1);

            // Actualise l'évaluation
            _eval_children[_current_move] = _children[_current_move]._evaluation;
            // _eval_children[_current_move] = children_list[_index_children + _current_move]._evaluation;
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

    if (depth == 0 && display) {
        
        to_fen();
        cout << "____________________________________________________________" << endl;
        cout << "Position : " << _fen << endl;
        cout << "Monte Carlo..." << endl;
        cout << "depth : " << max_depth << endl;
        cout << "Best evaluation : " << _evaluation << endl;

        int min_val; 
        int *power = new int[_got_moves];

        for (int i = 0; i < _got_moves; i++) {
            power[i] = (_player - 0.5) * 2 * _eval_children[i];
        }

        softmax(power, _got_moves);

        cout << "[|";
        for (int i = 0; i < _got_moves; i++)
            cout << " " << move_label_from_index(i) << " (n:" << _nodes_children[i] << ", e: " << _eval_children[i] << ", p:" << power[i] << ") |";
        cout << "]" << endl;

        // make_index_move(max_index(_nodes_children, _got_moves), depth == 0);
        // cout << _pgn << endl;

        cout << "____________________________________________________________" << endl;
        
    }

    return;
    
}


// Fonction qui joue le coup après analyse par l'algo de Monte Carlo
void Board::play_monte_carlo_move(bool display) {
    
    int move = max_index(_nodes_children, _tested_moves);
    if (display)
        play_index_move_sound(move);

    make_index_move(move, true);

    if (display) {
        cout << _pgn << endl;
        to_fen();
        cout << _fen << endl;
    }

    // Deletes the children... should it be kept in order to lessen the incoming thinking?
    delete_all();

}



int match(Agent &agent_a, Agent &agent_b) {

    Board b;
    while (b.game_over() == 0) {
        // if (b._player)
        //     b.monte_carlo(agent_a, 1000, 0, 2, false);
        // else
        //     b.monte_carlo(agent_b, 1000, 0, 2, false);

        // if (b._player)
        //     b.grogrosfish2(2, agent_a);
        // else
        //     b.grogrosfish2(2, agent_b);

        if (b._player)
            b.monte_carlo(agent_a, 0, 0, 0, false);
        else
            b.monte_carlo(agent_b, 0, 0, 0, false);
    }

    //cout << b._pgn << endl;

    agent_a._games += 1;
    agent_b._games += 1;

    int g = b.game_over();
    float result;
    if (g == 2)
        result = 0.5;
    else    
        result = (g + 1) / 2;

    calc_elo(agent_a, agent_b, result);


    return g;

}



// Fonction qui fait un tournoi d'agents, et retourne la liste des scores
int* tournament(Agent *agents, const int n_agents) {
    // Regarder pourquoi les derniers rounds sont plus lents...

    cout << "Tournament !" << endl;

    int result;

    // Points par victoire
    int victory = 2;

    // Points par nulle
    int draw = 1;


    for (int i = 0; i < n_agents; i++) {
        agents[i]._victories = 0;
        agents[i]._draws = 0;
        agents[i]._losses = 0;
        agents[i]._score = 0;
    }
        

    for (int i = 0; i < n_agents; i++) {

        cout << "Round " << i + 1 << "/" << n_agents << '\r';

        for (int j = 0; j < n_agents & j != i; j++) {
            // Match aller
            result = match(agents[i], agents[j]);
            if (result == 1) {
                agents[i]._victories++;
                agents[j]._losses++;
            }
            if (result == -1) {
                agents[j]._victories++;
                agents[i]._losses++;
            }
            if (result == 2) {
                agents[i]._draws++;
                agents[j]._draws++;
            }

            // Match retour
            result = match(agents[j], agents[i]);
            if (result == 1) {
                agents[j]._victories++;
                agents[i]._losses++;
            }
            if (result == -1) {
                agents[i]._victories++;
                agents[j]._losses++;
            }
            if (result == 2) {
                agents[j]._draws++;
                agents[i]._draws++;
            }
                
        }
    }

    // Résultats du tournoi
    int *scores;

    // cout << "1 : " << agents[0]._score << endl;
    // cout << "2 : " << agents[1]._score << endl;


    for (int i = 0; i < n_agents; i++) {
        agents[i]._score = victory * agents[i]._victories + draw * agents[i]._draws;
        cout << "Agent " << i << ", Generation : " << agents[i]._generation << ", Score : " << agents[i]._score << "/" << victory * 2 * (n_agents - 1) << ", Ratio (V/D/L): " << agents[i]._victories << "/" << agents[i]._draws << "/" << agents[i]._losses << ", Elo : " << agents[i]._elo << endl;
        //scores[i] = agents[i]._score;
        //cout << "toto";
    }

    return scores;

}






// Fonction pour supprimer les allocation mémoire du tableau, et de tous ses enfants
void Board::delete_all(bool self) {
    if (self)
        cout << "removing tree from memory..." << endl;
    for (int i = 0; i < _tested_moves; i++)
        _children[i].delete_all(false);
    if (!self) {
        delete []_children;
        delete []_nodes_children;
        // delete []_eval_children; // bugge...
    }
    else {
        _tested_moves = 0;
        _current_move = 0;
        _evaluated = false;
        cout << "done cleaning" << endl;
    }
    
    return;
}


// Fonction pour dessiner une flèche
void draw_arrow(float x1, float y1, float x2, float y2, float thickness, Color c) {
    DrawLineEx({x1, y1}, {x2, y2}, thickness, c);
}

// A partir de coordonnées sur le plateau
void draw_arrow_from_coord(int i1, int j1, int i2, int j2, float thickness, Color c, bool use_value, int value) {
    // cout << thickness << endl;
    if (thickness == -1.0)
        thickness = arrow_thickness;
    float x1 = board_padding_x + tile_size * orientation_index(j1) + tile_size /2;
    float y1 = board_padding_y + tile_size * orientation_index(7 - i1) + tile_size /2;
    float x2 = board_padding_x + tile_size * orientation_index(j2) + tile_size /2;
    float y2 = board_padding_y + tile_size * orientation_index(7 - i2) + tile_size /2;
    DrawLineEx({x1, y1}, {x2, y2}, thickness, c);
    DrawCircle(x1, y1, thickness, c);
    DrawCircle(x2, y2, thickness * 2, c);

    if (use_value) {
        char v[4];
        sprintf(v, "%d", value);
        int size = thickness * 2;
        int width = MeasureText(v, size);
        DrawText(v, x2 - width / 2, y2 - size / 2, size, BLACK);
        
    }

}


// Fonction qui dessine les flèches en fonction des valeurs dans l'algo de Monte-Carlo d'un plateau
void Board::draw_monte_carlo_arrows() {
    get_moves(false, true);

    int sum_nodes = 0;
    for (int i = 0; i < _tested_moves; i++)
        sum_nodes += _nodes_children[i];
    int test = 0;

    for (int i = 0; i < _tested_moves; i++) {
        // Si une pièce est sélectionnée
        if (selected_pos.first != -1 && selected_pos.second != -1) {
            if (selected_pos.first == _moves[4 * i] && selected_pos.second == _moves[4 * i + 1]) {
                draw_arrow_from_coord(_moves[4 * i], _moves[4 * i + 1], _moves[4 * i + 2], _moves[4 * i + 3], -1.0, move_color(_nodes_children[i], sum_nodes), true, _eval_children[i]);
            }
        }
        else {
            // cout << move_label(_moves[4 * i], _moves[4 * i + 1], _moves[4 * i + 2], _moves[4 * i + 3]) << ", ";
            // cout << (float)_nodes_children[i] << ", " << (float)_nodes_children[i] / (float)sum_nodes << endl;
            float n = _nodes_children[i];
            // cout << n << endl;
            // cout << (n / (float)sum_nodes > arrow_rate) << endl;

            // Bug ici... ça n'affiche pas toujours tous les noups (parfois si on ne cout pas, _nodes_children[i] -> 0??)
            // Sinon
            if (n / (float)sum_nodes > arrow_rate)
                draw_arrow_from_coord(_moves[4 * i], _moves[4 * i + 1], _moves[4 * i + 2], _moves[4 * i + 3], -1.0, move_color(_nodes_children[i], sum_nodes), true, _eval_children[i]);
        }
    }
}


// Couleur de la flèche en fonction du coup (de son nombre de noeuds)
Color move_color(int nodes, int total_nodes) {
    float Tmin = 0;
    float Tmax = total_nodes;
    float Tfmax = (Tmin + Tmax * 2) / 3;
    float avg = (Tmin + Tfmax) / 2;
    float range = Tfmax - Tmin;
    float T = nodes;

    unsigned char blue = min_float(255, max_float(0, 255 * (T - avg) / range * 2) * 2);
    unsigned char green = min_float(255, min_float(255 * (T - Tmin) / range * 2, 255 * (Tfmax - T) / range * 2) * 2);
    unsigned char red = min_float(255, max_float(0, 255 * (avg - T) / range * 2) * 2);

    return {red, green, blue, 255};
}
