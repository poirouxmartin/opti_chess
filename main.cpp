#include "opti_chess.h"
#include "time_tests.h"
#include "useful_functions.h"


/* Projets

En passant
Echecs?
Promotions
Tour du joueur
Roque
-> Fen, Fen -> (Rajouter des tests de validité de FEN)
Implémentation des compteurs de coups et demi-coups


Optimisations à implémenter

Regarder quel compilateur est le plus rapide/opti
Boucles pour les coups de cavalier à modifier
De même pour les coups de roi
Algo Negamax -> copies des tableaux -> undo moves?
Sort moves
Transposition tables




*/




// Test function
void test() {

    Board t;
    //t.get_moves();
    //t.evaluate();
    t.negamax(6, -1e9, 1e9, 1);

}




// Main
int main() {

    Board t;
    t.display();

    // Calcul du temps de la fonction
    test_function(&test, 1);

    t.to_fen();

    cout << "FEN : " << t._fen << endl;

    t.from_fen("rnbqkbnr/pppp1ppp/8/4p2Q/4P3/8/PPPP1PPP/RNB1KBNR b KQkq - 1 2");
    t.display();

    t.to_fen();
    cout << "FEN : " << t._fen << endl;

    t.grogrosfish(6);
    t.display();


    // Joue contre l'IA
    /*int depth = -1;
    int move[4];
    while (depth != 0) {
        cout << "your move : ";
        cin >> move[0] >> move[1] >> move[2] >> move[3];
        t.make_move(move[0], move[1], move[2], move[3]);
        t.display();
        cout << "depth : ";
        cin >> depth;
        if (depth == 0)
            break;
        t.grogrosfish(depth);
        t.display();
    }*/


    return 0;
}