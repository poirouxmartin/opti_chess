#include "opti_chess.h"
#include "useful_functions.h"
#include "gui.h"
#include <string>
#include <sstream>



// Liste de coups globale, pour les calculs, et éviter d'avoir des listes trop grosses pour chaque plateau
uint_fast8_t _global_moves[1000];
int _global_moves_size = 0;


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



// Fonction qui à partir des coordonnées d'un coup renvoie le coup codé sur un entier (à 4 chiffres) (base 10)
int Board::move_to_int(int i, int j, int k, int l) {
    return l + 10 * (k + 10 * (j + 10 * i));
}



// Fonction qui ajoute un coup dans une liste de coups
bool Board::add_move(int i, int j, int k, int l, int *iterator) {
    _moves[*iterator] = i;
    _moves[*iterator + 1] = j;
    _moves[*iterator + 2] = k;
    _moves[*iterator + 3] = l;


    _global_moves[*iterator] = i;
    _global_moves[*iterator + 1] = j;
    _global_moves[*iterator + 2] = k;
    _global_moves[*iterator + 3] = l;


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


// Fonction qui génère la liste des coups sous forme de vecteur
bool Board::get_moves_vector() {
    _moves_vector.resize(_got_moves * 4);
    for (int i = 0; i < _got_moves * 4; i++)
        _moves_vector[i] = _global_moves[i];

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

    // get_moves_vector();

    


    // Vérification échecs
    if (forbide_check) {
        int new_moves[1000]; // Est-ce que ça prend de la mémoire?
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
    b.get_moves(true);
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
    int p_last = _array[k][l];

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

    // Si une tour se fait manger
    if (p_last == 4) {
        if (l == 0)
            _q_castle_w = false;
        if (l == 7)
            _k_castle_w = false;
    }
    if (p_last == 10) {
        if (l == 0)
            _q_castle_b = false;
        if (l == 7)
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


    _last_move[0] = i;
    _last_move[1] = j;
    _last_move[2] = k;
    _last_move[3] = l;
    _last_move[4] = p;
    // if (_last_move[5] == -1) {
    //     _last_move[5] =
    // }
    _last_move[9] = p_last;

    _sorted_moves = -1;

    _activity = false;
    _safety = false;

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
void Board::evaluate(Evaluator eval, bool checkmates, bool display) {

    if (checkmates) {

        int _is_mate = is_mate();

        if (_is_mate == 1) {
            _mate = true;
            _evaluation = - _color * (1000000 - 1000 * _moves_count);
            return;
        }
        if (_is_mate == 0) {
            _evaluation = 0;
            return;
        }
        
    }
    
    _evaluation = 0;

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
                case 8:  _evaluation -= (eval._knight_value_begin * (1 - adv) + eval._knight_value_end * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_knight_begin[i][j]     * (1 - adv) + eval._pos_knight_end[i][j]     * adv); break;
                case 9:  _evaluation -= (eval._bishop_value_begin * (1 - adv) + eval._bishop_value_end * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_bishop_begin[i][j]     * (1 - adv) + eval._pos_bishop_end[i][j]     * adv); bishop_b += 1; break;
                case 10: _evaluation -= (eval._rook_value_begin   * (1 - adv) + eval._rook_value_end   * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_rook_begin[i][j]       * (1 - adv) + eval._pos_rook_end[i][j]       * adv); break;
                case 11: _evaluation -= (eval._queen_value_begin  * (1 - adv) + eval._queen_value_end  * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_queen_begin[i][j]      * (1 - adv) + eval._pos_queen_end[i][j]      * adv); break;
                case 12: _evaluation -= (eval._king_value_begin   * (1 - adv) + eval._king_value_end   * adv) * eval._piece_value + eval._piece_positioning * (eval._pos_king_begin[i][j]       * (1 - adv) + eval._pos_king_end[i][j]       * adv); break;
            }

        }
    }

    if (display) {
        cout << "*** EVALUATION ***" << endl;
        cout << "pieces and their position : " << _evaluation << endl;
    }


    // Paire de oufs
    float bishop_pair = 0;
    if (eval._bishop_pair != 0) {
        bishop_pair = eval._bishop_pair * ((bishop_w >= 2) - (bishop_b >= 2));
        if (display)
            cout << "bishop pair : " << bishop_pair << endl;
        _evaluation += bishop_pair;
    }
        

    // Droits de roques
    float castling_rights = 0;
    if (eval._castling_rights != 0) {
        castling_rights += eval._castling_rights * (_k_castle_w + _q_castle_w - _k_castle_b - _q_castle_b) * (1 - adv);
        if (display)
            cout << "castling rights : " << castling_rights << endl;
        _evaluation += castling_rights;
    }
        

    // Ajout random
    float random_add = 0;
    if (eval._random_add != 0) {
        random_add += GetRandomValue(-50, 50) * eval._random_add / 100;
        if (display)
            cout << "random add : " << random_add << endl;
        _evaluation += random_add;
    }
        

    // Activité des pièces
    float piece_activity = 0;
    if (eval._piece_activity != 0) {
        get_piece_activity();
        piece_activity = _piece_activity * eval._piece_activity;
        if (display)
            cout << "piece activity : " << piece_activity << endl;
        _evaluation += piece_activity;
    }

    // Trait du joueur
    float player_trait = 0;
    if (eval._player_trait != 0) {
        player_trait = eval._player_trait * _color;
        if (display)
            cout << "player trait : " << player_trait << endl;
        _evaluation += player_trait;
    }
    
    // Sécurité du roi
    float king_safety = 0;
    if (eval._king_safety != 0) {
        get_king_safety();
        king_safety = _king_safety * eval._king_safety * (1 - adv);
        if (display)
            cout << "king safety : " << king_safety << endl;
        _evaluation += king_safety;
    }
        

    // Pour éviter les répétitions (ne fonctionne pas)
    //_evaluation *= 1 - (float)(_half_moves_count + _moves_count) / 1000;


    if (display) {
        cout << "*** TOTAL : " << _evaluation << " ***" << endl;
    }

    _evaluated = true;


    return;
}


// Fonction qui évalue la position à l'aide d'heuristiques -> évaluation entière
void Board::evaluate_int(Evaluator eval, bool checkmates) {

    evaluate(eval, checkmates);
    _evaluation = (int)(100 * _evaluation);
    _static_evaluation = _evaluation;

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
        // b.undo();

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
                if (_tested_moves > 0)
                    play_monte_carlo_move_keep(best_move, true);
                else
                    make_index_move(best_move, true);
        }
            
        return best_move;
    }
    
    return value;
    
}



// Version un peu mieux optimisée de Grogrosfish
bool Board::grogrosfish(int depth, Evaluator eval, bool display = false) {
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

// Version un peu mieux optimisée de Grogrosfish (utilisant un agent)
bool Board::grogrosfish(int depth, Agent a, bool display = false) {
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


// Fonction qui revient à la position précédente (ne marchera pas avec les roques pour le moment)
bool Board::undo(int i1, int j1, int p1, int i2, int j2, int p2, int half_moves) {
    _array[i1][j1] = p1;
    _array[i2][j2] = p2;

    // Incrémentation des demi-coups
    _half_moves_count = half_moves;

    _player = !_player;
    _got_moves = -1;
    _color = - _color;

    // Implémentation du FEN possible?
    _fen = "";

    // Incrémentation des coups
    !_player && (_moves_count -= 1);

    return true;
}


// Une surcharge
bool Board::undo() {
    // Il faut rétablir les droits de roque, les en passant........

    print_array(_last_move, 10);
    int i1 = _last_move[0];
    int j1 = _last_move[1];
    int k1 = _last_move[2];
    int l1 = _last_move[3];
    int p1 = _last_move[4];

    int i2 = _last_move[5];
    int j2 = _last_move[6];
    int k2 = _last_move[7];
    int l2 = _last_move[8];
    int p2 = _last_move[9];

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

    _sorted_moves = -1;

    _activity = false;
    _safety = false;

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


    // Reste à changer le PGN, gérer les roques, en passant, demi-coups...

    return true;
}



// Fonction qui arrange les coups de façon "logique", pour optimiser les algorithmes de calcul
void Board::sort_moves(Evaluator eval) {
    // Modifier pour seulement garder le (ou les deux) meilleur(s) coups?

    Board b;

    (_got_moves == -1 && get_moves());

    float* values = new float[_got_moves]; // A delete?
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

    delete[] values;
    _sorted_moves = true;
    
}


// Fonction qui récupère le plateau d'un FEN
void Board::from_fen(string fen) {
    // Mise à jour du FEN
    _fen = fen;

    // PGN
    _pgn = "[FEN \"" + fen + "\"]\n";

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

    _activity = false;
    _safety = false;

    _last_move[0] = -1;
    _last_move[1] = -1;
    _last_move[2] = -1;
    _last_move[3] = -1;

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

    int p;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            if (p == 6)
                king_w = true;
            if (p == 12)
                king_b = true;
        }
    }

    if (!king_w)
        return -1;
    if (!king_b)
        return 1;

    
    // int m = is_mate();
    // if (m == 1)
    //     return 1;
    // if (m == 0)
    //     return 2;


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
void Board::from_pgn(string pgn) {

    _pgn = pgn;
}






// Fonction qui affiche un texte dans une zone donnée
void Board::draw_text_rect(string s, float pos_x, float pos_y, float width, float height, int size) {

    // Division du texte
    int sub_div = (1.5 * width) / size;

    if (width <= 0 || height <= 0 || sub_div <= 0)
        return;

    Rectangle rect_text = {pos_x, pos_y, width, height};
    DrawRectangleRec(rect_text, background_text_color);
    
    int string_size = s.length();
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

        // Pièces
        piece_images[0] = LoadImage("../resources/images/w_pawn.png");
        piece_images[1] = LoadImage("../resources/images/w_knight.png");
        piece_images[2] = LoadImage("../resources/images/w_bishop.png");
        piece_images[3] = LoadImage("../resources/images/w_rook.png");
        piece_images[4] = LoadImage("../resources/images/w_queen.png");
        piece_images[5] = LoadImage("../resources/images/w_king.png");
        piece_images[6] = LoadImage("../resources/images/b_pawn.png");
        piece_images[7] = LoadImage("../resources/images/b_knight.png");
        piece_images[8] = LoadImage("../resources/images/b_bishop.png");
        piece_images[9] = LoadImage("../resources/images/b_rook.png");
        piece_images[10] = LoadImage("../resources/images/b_queen.png");
        piece_images[11] = LoadImage("../resources/images/b_king.png");

        // Chargement du son
        move_1_sound = LoadSound("../resources/sounds/move_1.mp3");
        move_2_sound = LoadSound("../resources/sounds/move_2.mp3");
        castle_1_sound = LoadSound("../resources/sounds/castle_1.mp3");
        castle_2_sound = LoadSound("../resources/sounds/castle_2.mp3");
        check_1_sound = LoadSound("../resources/sounds/check_1.mp3");
        check_2_sound = LoadSound("../resources/sounds/check_2.mp3");
        capture_1_sound = LoadSound("../resources/sounds/capture_1.mp3");
        capture_2_sound = LoadSound("../resources/sounds/capture_2.mp3");
        checkmate_sound = LoadSound("../resources/sounds/checkmate.mp3");
        stealmate_sound = LoadSound("../resources/sounds/stealmate.mp3");
        game_begin_sound = LoadSound("../resources/sounds/game_begin.mp3");
        game_end_sound = LoadSound("../resources/sounds/game_end.mp3");

        // Police de l'écriture
        text_font = LoadFont("../resources/fonts/RobotReaversItalic-4aa4.ttf");
        // text_font = GetFontDefault();

        // Icône
        icon = LoadImage("../resources/images/grogros_zero.png");
        SetWindowIcon(icon);

        loaded_resources = true;
}


// Fonction qui met à la bonne taille les images et les textes de la GUI
void resize_gui() {
        cout << screen_width << ", " << screen_height << endl;
        float min_screen = min(screen_height, screen_width);
        board_size = board_scale * min_screen;
        board_padding_y = (screen_height - board_size) / 2;
        board_padding_x = (screen_height - board_size) / 4;

        tile_size = board_size / 8;
        piece_size = tile_size * piece_scale;
        arrow_thickness = tile_size * arrow_scale;

        // Génération des textures
        for (int i = 0; i < 12; i++) {
            ImageResize(&piece_images[i], piece_size, piece_size);
            piece_textures[i] = LoadTextureFromImage(piece_images[i]);
        }
        text_size = board_size / 16;
}


// Fonction qui actualise les nouvelles dimensions de la fenêtre
void get_window_size() {
    screen_width = GetScreenWidth();
    screen_height = GetScreenHeight();
}

// Fonction qui dessine le plateau
void Board::draw() {

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
                        // make_move(selected_pos.first, selected_pos.second, clicked_pos.first, clicked_pos.second, true, true);
                        play_monte_carlo_move_keep(i, true);
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
                            // make_move(clicked_pos.first, clicked_pos.second, drop_pos.first, drop_pos.second, true, true);
                            play_monte_carlo_move_keep(i, true);
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
                        DrawTexture(piece_textures[p - 1], mouse_pos.x - piece_size / 2, mouse_pos.y - piece_size / 2, WHITE);
                    else
                        DrawTexture(piece_textures[p - 1], board_padding_x + tile_size * orientation_index(j) + (tile_size - piece_size) / 2, board_padding_y + tile_size * orientation_index(7 - i) + (tile_size - piece_size) / 2, WHITE);
                }
            }
        }
    }


    // Texte
    DrawTextEx(text_font, "GrogrosZero", {board_padding_x, text_size / 4}, text_size, font_spacing * text_size, text_color);

    // Joueurs de la partie
    DrawTextEx(text_font, _player_2.c_str(), {board_padding_x, board_padding_y - text_size / 2 * board_orientation + board_size * !board_orientation}, text_size / 2, font_spacing * text_size / 2, text_color);
    DrawTextEx(text_font, _player_1.c_str(), {board_padding_x, board_padding_y - text_size / 2 * !board_orientation + board_size * board_orientation}, text_size / 2, font_spacing * text_size / 2, text_color);


    // Temps des joueurs
    DrawTextEx(text_font, to_string(_time_player_2 / 1000).c_str(), {board_padding_x + board_size - text_size, board_padding_y - text_size / 2 * board_orientation + board_size * !board_orientation}, text_size / 2, font_spacing * text_size / 2, text_color);
    DrawTextEx(text_font, to_string(_time_player_1 / 1000).c_str(), {board_padding_x + board_size - text_size, board_padding_y - text_size / 2 * !board_orientation + board_size * board_orientation}, text_size / 2, font_spacing * text_size / 2, text_color);

    // FEN
    if (_fen == "")
        to_fen();
    const char *fen = _fen.c_str();
    DrawTextEx(text_font, fen, {board_padding_x, screen_height - text_size}, text_size / 2, font_spacing * text_size / 2, text_color);


    // PGN
    draw_text_rect(_pgn, board_padding_x + board_size + text_size / 2, board_padding_y, screen_width - (board_padding_x + board_size + text_size / 2) - text_size / 2, board_size / 2 - text_size / 2, text_size / 2);

    // Analyse de Monte-Carlo
    string monte_carlo_text = "Monte-Carlo analysis\n\nresearch parameters :\nbeta : " + to_string(_beta) + "\nk_add : " + to_string(_k_add);
    if (_tested_moves) {
        // int best_eval = (_player) ? max_value(_eval_children, _tested_moves) : min_value(_eval_children, _tested_moves);
        int best_move = max_index(_nodes_children, _tested_moves);
        int best_eval = _eval_children[best_move];
        string eval;
        if (best_eval > 100000)
            eval = "M" + to_string((100000000 - best_eval) / 100000 - _moves_count + 1); // (Immonde) à changer...
        else if (best_eval < -100000)
            eval = "M" + to_string((100000000 + best_eval) / 100000 - _moves_count);
        else
            eval = to_string(best_eval);
        
        monte_carlo_text += "\n\nnodes : " + int_to_round_string(total_nodes()) + "/" + int_to_round_string(_monte_buffer._length) + "\ndepth : " + to_string(max_monte_carlo_depth()) + "\neval : "  + eval + "\nmove : "  + move_label_from_index(best_move) + " (" + to_string(100 * _nodes_children[best_move] / total_nodes()) + "%)" + "\nstatic eval : "  + to_string(_static_evaluation);
    }
    DrawTextEx(text_font, monte_carlo_text.c_str(), {board_padding_x + board_size + text_size / 2, board_padding_y + board_size / 2 + text_size / 4}, text_size / 2, font_spacing * text_size / 2, text_color);

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


int match(Agent &agent_a, Agent &agent_b) {

    Board b;
    while (b.game_over() == 0) {
        cout << "check function match" << endl;
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
    int *scores = new int[n_agents];

    // cout << "1 : " << agents[0]._score << endl;
    // cout << "2 : " << agents[1]._score << endl;


    for (int i = 0; i < n_agents; i++) {
        agents[i]._score = victory * agents[i]._victories + draw * agents[i]._draws;
        cout << "Agent " << i << ", Generation : " << agents[i]._generation << ", Score : " << agents[i]._score << "/" << victory * 2 * (n_agents - 1) << ", Ratio (V/D/L): " << agents[i]._victories << "/" << agents[i]._draws << "/" << agents[i]._losses << ", Elo : " << agents[i]._elo << endl;
        scores[i] = agents[i]._score;
        //cout << "toto";
    }

    return scores;

}



// Fonction pour dessiner une flèche
void draw_arrow(float x1, float y1, float x2, float y2, float thickness, Color c) {
    DrawLineEx({x1, y1}, {x2, y2}, thickness, c);
}

// A partir de coordonnées sur le plateau
void draw_arrow_from_coord(int i1, int j1, int i2, int j2, float thickness, Color c, bool use_value, int value, int mate) {
    // cout << thickness << endl;
    if (thickness == -1.0)
        thickness = arrow_thickness;
    float x1 = board_padding_x + tile_size * orientation_index(j1) + tile_size /2;
    float y1 = board_padding_y + tile_size * orientation_index(7 - i1) + tile_size /2;
    float x2 = board_padding_x + tile_size * orientation_index(j2) + tile_size /2;
    float y2 = board_padding_y + tile_size * orientation_index(7 - i2) + tile_size /2;
    DrawLineEx({x1, y1}, {x2, y2}, thickness, c);
    // DrawCircle(x1, y1, thickness, c);
    DrawCircle(x2, y2, thickness * 2, c);

    if (use_value) {
        char v[4];
        if (mate != -1)
            sprintf(v, "M%d", mate);
        else
            sprintf(v, "%d", value);
        int size = thickness * 1.85;
        int max_size = thickness * 4;
        int width = MeasureTextEx(text_font, v, size, font_spacing * size).x;
        if (width > max_size) {
            size = size * max_size / width;
            width = MeasureTextEx(text_font, v, size, font_spacing * size).x;
        }
        Color t_c = ColorAlpha(BLACK, (float)c.a / 255.0);
        DrawTextEx(text_font, v,  {x2 - width / 2, y2 - size / 2}, size, font_spacing * size, BLACK);
        
    }

}


// Fonction qui dessine les flèches en fonction des valeurs dans l'algo de Monte-Carlo d'un plateau
void Board::draw_monte_carlo_arrows() {
    get_moves(false, true);

    int sum_nodes = 0;
    for (int i = 0; i < _tested_moves; i++)
        sum_nodes += _nodes_children[i];
    int mate;

    for (int i = 0; i < _tested_moves; i++) {
        if (_eval_children[i] > 100000)
            mate = (100000000 - _eval_children[i]) / 100000 - _moves_count + 1; // (Immonde) à changer...
        else if (_eval_children[i] < -100000)
            mate = (100000000 + _eval_children[i]) / 100000 - _moves_count;
        else
            mate = -1;
        // Si une pièce est sélectionnée
        if (selected_pos.first != -1 && selected_pos.second != -1) {
            if (selected_pos.first == _moves[4 * i] && selected_pos.second == _moves[4 * i + 1]) {
                draw_arrow_from_coord(_moves[4 * i], _moves[4 * i + 1], _moves[4 * i + 2], _moves[4 * i + 3], -1.0, move_color(_nodes_children[i], sum_nodes), true, _eval_children[i], mate);
            }
        }
        else {
            float n = _nodes_children[i];
            if (n / (float)sum_nodes > arrow_rate)
                draw_arrow_from_coord(_moves[4 * i], _moves[4 * i + 1], _moves[4 * i + 2], _moves[4 * i + 3], -1.0, move_color(_nodes_children[i], sum_nodes), true, _eval_children[i], mate);
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

    // Activité des pièces du joueur
    b.get_moves(false, legal);
    _piece_activity += b._got_moves;

    // Activité des pièces de l'autre joueur
    b._player = !b._player; b._got_moves = -1;
    b.get_moves(false, legal);
    _piece_activity -= b._got_moves;

    _piece_activity *= _color;
    _activity = true;

    return;
}










// Couleur de la flèche en fonction du coup (de son nombre de noeuds)
Color move_color(int nodes, int total_nodes) {

    float x = (float) nodes / total_nodes;

    unsigned char red = 255 * ((x <= 0.2) + (x > 0.2 && x < 0.4) * (0.4 - x) / 0.2 + (x > 0.8) * (x - 0.8) / 0.2);
    unsigned char green = 255 * ((x < 0.2) * (x - 0.2) / 0.2 + (x >= 0.2 && x <= 0.6) + (x > 0.6 && x < 0.8) * (0.6 - x) / 0.2);
    unsigned char blue = 255 * ((x > 0.4 && x < 0.6) * (x - 0.4) / 0.2 + (x >= 0.6));

    unsigned char alpha = 100 + 155 * nodes / total_nodes;
    // cout << x << ", " << (int)red << ", " << (int)green << ", " << (int)blue << endl;

    return {red, green, blue, alpha};
}


// Fonction qui renvoie le meilleur coup selon l'analyse faite par l'algo de Monte-Carlo
int Board::best_monte_carlo_move() {
    return max_index(_nodes_children, _tested_moves);
}



// Fonction qui joue le coup après analyse par l'algo de Monte Carlo, et qui garde en mémoire les infos du nouveau plateau
void Board::play_monte_carlo_move_keep(int move, bool display) {

    // Si le coup a été calculé par l'algo de Monte-Carlo
    if (move < _tested_moves) {

        if (display) {
            play_index_move_sound(move);
            Board b(*this);
            b.make_index_move(move, true);
            cout << b._pgn << endl;
            b.to_fen();
            cout << b._fen << endl;
            if (_is_active) {
                _monte_buffer._heap_boards[_index_children[move]]._pgn = b._pgn;
                _monte_buffer._heap_boards[_index_children[move]]._player_1 = _player_1;
                _monte_buffer._heap_boards[_index_children[move]]._player_2 = _player_2;
                _monte_buffer._heap_boards[_index_children[move]]._time_player_1 = _time_player_1;
                _monte_buffer._heap_boards[_index_children[move]]._time_player_2 = _time_player_2;
                _monte_buffer._heap_boards[_index_children[move]]._time = _time;
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
        }
    

    }


    // Sinon, joue simplement le coup
    else {
        if (_got_moves == -1)
            get_moves(false, true);

        if (move < _got_moves) {
            if (_is_active)
                reset_all();
            
            make_index_move(move, true);
        }
        else 
            cout << "illegal move" << endl;
    }


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


double _beta = 0.035;
int _k_add = 50;





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
        cout << "buffer initialized ! length : " << int_to_round_string(_length) << endl;
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
void Board::grogros_zero(Agent a, Evaluator e, int nodes, bool use_agent, bool checkmates, double beta, int k_add, bool display, int depth) {
    static int max_depth;
    static int n_positions = 0;

    _is_active = true;

    if (_new_board && depth == 0) {
        max_depth = 0;    
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

    // Obtention des coups jouables
    get_moves(false, true);

    if (_got_moves == 0) {
        return;
    }

    if (_new_board) {
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
            if (use_agent)
               _monte_buffer._heap_boards[_index_children[_current_move]].evaluate(a);
            else
                _monte_buffer._heap_boards[_index_children[_current_move]].evaluate_int(e, checkmates);
            _eval_children[_current_move] = _monte_buffer._heap_boards[_index_children[_current_move]]._evaluation;
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
            _current_move = pick_random_good_move(_eval_children, _got_moves, _color, false, beta, k_add);

            // Va une profondeur plus loin... appel récursif sur Monte-Carlo
           _monte_buffer._heap_boards[_index_children[_current_move]].grogros_zero(a, e, 1, use_agent, checkmates, beta, k_add, display, depth + 1);

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

    return;
    
}


// Fonction qui réinitialise le plateau dans son état de base (pour le buffer)
void Board::reset_board(bool display) {

    _is_active = false;
    _current_move = 0;
    _evaluated = false;
    
    if (!_new_board) {
        _tested_moves = 0;
        delete []_index_children; // Vérifier que tous les plateaux sont supprimés
        delete []_nodes_children;
        delete []_eval_children;
        _new_board = true;
    }      
    
    return;
}



// Fonction qui réinitialise tous les plateaux fils dans le buffer
void Board::reset_all(bool self, bool display) {
    for (int i = 0; i < _tested_moves; i++)
        _monte_buffer._heap_boards[_index_children[i]].reset_all(false);

    reset_board();
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

    if (_safety)
        return;

    int w_king_i;
    int w_king_j;
    int b_king_i;
    int b_king_j;

    bool w_king = false;
    bool b_king = false;

    int p;

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

    // if (!b_king || !w_king)
    //     cout << "a king is missing in the position" << endl;

    kings:

    _king_safety = 0;

    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            if (p > 0) {
                if (p < 6) {
                    if (p == 1) {
                        _king_safety += pawn_defense * proximity(i, j, w_king_i, w_king_j);
                        _king_safety += pawn_attack * proximity(i, j, b_king_i, b_king_j);
                    }   
                    else {
                        _king_safety += piece_defense * proximity(i, j, w_king_i, w_king_j);
                        _king_safety += piece_attack * proximity(i, j, b_king_i, b_king_j);
                    }
                    
                } 
                else if (p > 6 && p < 12) {
                    if (p == 7) {
                        _king_safety -= pawn_attack * proximity(i, j, w_king_i, w_king_j);
                        _king_safety -= pawn_defense * proximity(i, j, b_king_i, b_king_j);
                    }   
                    else {
                        _king_safety -= piece_attack * proximity(i, j, w_king_i, w_king_j);
                        _king_safety -= piece_defense * proximity(i, j, b_king_i, b_king_j);
                    }
                }
            }
        }

    _king_safety += edge_defense * ((w_king_i == 0 || w_king_i == 7) + (w_king_j == 0 || w_king_j == 7) - (b_king_i == 0 || b_king_i == 7) - (b_king_j == 0 || b_king_j == 7));

    _safety = true;

}



// Fonction qui renvoie s'il y a échec et mat (ou pat) (-1, 1 ou 0)
int Board::is_mate() {

    // Pour accélérer en ne re calculant pas forcément les coups (marche avec coups légaux OU illégaux)
    if (_got_moves == -1)
        get_moves();

    Board b;

    for (int i = 0; i < _got_moves; i++) {
        b.copy_data(*this);
        b.make_index_move(i);
        b._player = _player;
        b._color = _color;
        if (!b.in_check()) {
            _got_moves = -1;
            return -1; 
        }
            
    }

    if (in_check()) {
        _got_moves = -1;
        return 1;  
    }
              
    _got_moves = -1;
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
    return selected_pos.first != -1;
}


// Fonction qui change le mode d'affichage des flèches (oui/non)
void switch_arrow_drawing() {
    drawing_arrows = !drawing_arrows;
}