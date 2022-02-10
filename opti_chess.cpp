#include "opti_chess.h"
#include "useful_functions.h"
#include <string>



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
    _evaluation = b._evaluation;
    _color = b._color;
    _k_castle_w = b._k_castle_w;
    _q_castle_w = b._q_castle_w;
    _k_castle_b = b._k_castle_b;
    _q_castle_b = b._q_castle_b;
    _en_passant = b._en_passant;
    _half_moves_count = b._half_moves_count;
    _moves_count = b._moves_count;
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
    _evaluation = b._evaluation;
    _color = b._color;
    _k_castle_w = b._k_castle_w;
    _q_castle_w = b._q_castle_w;
    _k_castle_b = b._k_castle_b;
    _q_castle_b = b._q_castle_b;
    _en_passant = b._en_passant;
    _half_moves_count = b._half_moves_count;
    _moves_count = b._moves_count;
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



// Fonction qui ajoute un coup dans une liste de coups
void Board::add_move(int i, int j, int k, int l, int *iterator) {
    _moves[*iterator] = i;
    _moves[*iterator + 1] = j;
    _moves[*iterator + 2] = k;
    _moves[*iterator + 3] = l;
    *iterator += 4;
}



// Fonction qui ajoute les coups "pions" dans la liste de coups
void Board::add_pawn_moves(int i, int j, int *iterator) {
    // Joueur avec les pièces noires
    if (_player) {
        // Poussée (de 1)
        if (_array[i + 1][j] == 0) {
            add_move(i, j, i + 1, j, iterator);
        }
        // Poussée (de 2)
        if (i == 1 & _array[i + 2][j] == 0) {
            add_move(i, j, i + 2, j, iterator);
        }
        // Prise (gauche)
        if (j > 0 & is_in(_array[i + 1][j - 1], 7, 12)) {
            add_move(i, j, i + 1, j - 1, iterator);
        }
        // Prise (droite)
        if (j < 7 & is_in(_array[i + 1][j + 1], 7, 12)) {
            add_move(i, j, i + 1, j + 1, iterator);
        }
    }
    else {
        // Poussée (de 1)
        if (_array[i - 1][j] == 0) {
            add_move(i, j, i - 1, j, iterator);
        }
        // Poussée (de 2)
        if (i == 6 & _array[i - 2][j] == 0) {
            add_move(i, j, i - 2, j, iterator);
        }
        // Prise (gauche)
        if (j > 0 & is_in(_array[i - 1][j - 1], 1, 6)) {
            add_move(i, j, i - 1, j - 1, iterator);
        }
        // Prise (droite)
        if (j < 7 & is_in(_array[i - 1][j + 1], 1, 6)) {
            add_move(i, j, i - 1, j - 1, iterator);
        }
    }
}


// Fonction qui ajoute les coups "cavaliers" dans la liste de coups
void Board::add_knight_moves(int i, int j, int *iterator) {
    // les boucles for sont à modifier, car très lentes
    int i2; int j2;
    for (int k : {-2, -1, 1, 2}) {
        for (int l : {-2, -1, 1, 2}) {
            i2 = i + k; j2 = j + l;
            if (_player) {
                // Si le coup n'est ni hors du plateau, ni sur une case où une pièce alliée est placée
                if (abs(k) + abs(l) == 3 & is_in(i2, 0, 7) & is_in (j2, 0, 7) & !is_in(_array[i2][j2], 1, 6))
                    add_move(i, j, i2, j2, iterator);
            }
            else {
                // Si le coup n'est ni hors du plateau, ni sur une case où une pièce alliée est placée
                if (abs(k) + abs(l) == 3 & is_in(i2, 0, 7) & is_in (j2, 0, 7) & !is_in(_array[i2][j2], 7, 12))
                    add_move(i, j, i2, j2, iterator);
            }
            
        }
    }
}


// Fonction qui ajoute les coups diagonaux dans la liste de coups
void Board::add_diag_moves(int i, int j, int *iterator) {
    int ally_min; int ally_max;
    if (_player) {
        ally_min = 1; ally_max = 6;
    }
        
    else {
        ally_min = 7; ally_max = 12;
    }

    int i2; int j2; int p2;
        
    // Diagonale 1
    for (int k = 1; k < 7; k++) {
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
    for (int k = 1; k < 7; k++) {
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
    for (int k = 1; k < 7; k++) {
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
    for (int k = 1; k < 7; k++) {
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
}



// Fonction qui ajoute les coups horizontaux et verticaux dans la liste de coups
void Board::add_rect_moves(int i, int j, int *iterator) {
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
    for (int k = 1; k < 7; k++) {
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
    for (int k = 1; k < 7; k++) {
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
    for (int k = 1; k < 7; k++) {
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
    for (int k = 1; k < 7; k++) {
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
}



// Fonction qui ajoute les coups "roi" dans la liste de coups
void Board::add_king_moves(int i, int j, int *iterator) {
    int ally_min; int ally_max;
    if (_player) {
        ally_min = 1; ally_max = 6;
    }
        
    else {
        ally_min = 7; ally_max = 12;
    }

    int i2; int j2;
    
    for (int k : {-1, 0, 1}) {
        for (int l : {-1, 0, 1}) {
            i2 = i + k; j2 = j + l;
            // Si le coup n'est ni hors du plateau, ni sur une case où une pièce alliée est placée
            if ((k | l != 0) & is_in(i2, 0, 7) & is_in (j2, 0, 7) & !is_in(_array[i2][j2], ally_min, ally_max)) {
                add_move(i, j, i2, j2, iterator);
            }
        }
    }
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
                    if (_player)
                        add_pawn_moves(i, j, &iterator);
                    break;

                case 2: // Cavalier blanc
                    if (_player)
                        add_knight_moves(i, j, &iterator);
                    break;

                case 3: // Fou blanc   
                    if (_player)           
                        add_diag_moves(i, j, &iterator);
                    break;

                case 4: // Tour blanche
                    if (_player)
                        add_rect_moves(i, j, &iterator);
                    break;

                case 5: // Dame blanche
                    if (_player) {
                        add_diag_moves(i, j, &iterator);
                        add_rect_moves(i, j, &iterator);
                    }
                    break;

                case 6: // Roi blanc
                    if (_player)
                        add_king_moves(i, j, &iterator);
                    break;

                case 7: // Pion noir
                    if (!_player)
                        add_pawn_moves(i, j, &iterator);
                    break;

                case 8: // Cavalier noir
                    if (!_player)
                        add_knight_moves(i, j, &iterator);
                    break;

                case 9: // Fou noir
                    if (!_player)           
                        add_diag_moves(i, j, &iterator);
                    break;

                case 10: // Tour noire
                    if (!_player)           
                        add_rect_moves(i, j, &iterator);
                    break;

                case 11: // Dame noire
                    if (!_player) {
                        add_diag_moves(i, j, &iterator);
                        add_rect_moves(i, j, &iterator);
                    }           
                    break;

                case 12: // Roi noir
                    if (!_player)
                        add_king_moves(i, j, &iterator);
                    break;

            }

        }
    }

    _moves[iterator] = -1;
    _got_moves = iterator / 4;

    return _moves;
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
    _array[k][l] = _array[i][j];
    _array[i][j] = 0;
    _player = !_player;
    _got_moves = -1;
    _color = - _color;
    _fen = "";

    // Actualise les coups possibles
    //get_moves();
}


// Fonction qui joue le coup i
void Board::make_index_move(int i) {
    if (i < 0 | i >= _got_moves)
        cout << "move index out of range" << endl;
    else {
        int k = 4 * i;
        make_move(_moves[k], _moves[k + 1], _moves[k + 2], _moves[k + 3]);
    }
}


// Fonction qui évalue la position à l'aide d'heuristiques
void Board::evaluate() {
    _evaluation = 0;

    // à tester: changer les boucles par des for (i : array) pour optimiser
    int p;
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            p = _array[i][j];
            switch (p)
            {   
                case 0: break;
                case 1: _evaluation += 1; break;
                case 2: _evaluation += 3; break;
                case 3: _evaluation += 3; break;
                case 4: _evaluation += 5; break;
                case 5: _evaluation += 9; break;
                case 6: _evaluation += 1000; break;
                case 7: _evaluation -= 1; break;
                case 8: _evaluation -= 3; break;
                case 9: _evaluation -= 3; break;
                case 10: _evaluation -= 5; break;
                case 11: _evaluation -= 9; break;
                case 12: _evaluation -= 1000; break;
            }

        }
    }


        
}




// Fonction qui joue le coup d'une position, renvoyant la meilleure évaluation à l'aide d'un negamax (similaire à un minimax)
float Board::negamax(int depth, float alpha, float beta, int color) {
    if (depth == 0) {
        evaluate();
        // ??
        return color * _evaluation;
    }

    float value = -1e9;
    Board b;

    if (_got_moves == -1)
        get_moves();

    // Sort moves à faire

    for (int i = 0; i < _got_moves; i++) {
        // Copie du plateau
        // Opti?? plutôt copier une fois au début, et undo les moves?
        b.copy_data(*this);
        b.make_index_move(i);
        value = max(value, -b.negamax(depth - 1, -beta, -alpha, -color));
        alpha = max(alpha, value);
        // undo move
        if (alpha >= beta)
            break;
    }

    return value;
}



// Fonction qui utilise minimax pour déterminer quel est le "meilleur" coup et le joue
void Board::grogrosfish(int depth) {

    int best_move;
    float best_value = -1e9;
    float value;
    Board b;

    if (_got_moves == -1)
        get_moves();

    for (int i = 0; i < _got_moves; i++) {
        b.copy_data(*this);
        b.make_index_move(i);
        value = -b.negamax(depth - 1, -1e9, 1e9, -_color);
        if (value > best_value) {
            best_move = i;
            best_value = value;
        }
    }

    make_index_move(best_move);
    
}



// Fonction qui revient à la position précédente
void Board::undo() {
    //
}


// Fonction qui arrange les coups de façon "logique", pour optimiser les algorithmes de calcul
void Board::sort_moves() {

    Board b;

    if (_got_moves == -1)
        get_moves();


    
}


// Fonction qui récupère le plateau d'un FEN
void Board::from_fen(string fen) {

    // Iterateur qui permet de parcourir la chaine de caractères
    int iterator = 0;

    // Position à itérer dans le plateau
    int i = 7;
    int j = 0;

    int digit;
    char c;

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
    c = fen[iterator];
    _half_moves_count = ((int)c) - ((int)'0');

    iterator += 2;
    c = fen[iterator];
    _moves_count = ((int)c) - ((int)'0');

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
                    s += to_string(it);
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
            s += to_string(it);
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
