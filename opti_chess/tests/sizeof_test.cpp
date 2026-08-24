#include "../board.h"
#include "../exploration.h"
#include <iostream>
int main() {
    std::cout << "sizeof(Board) = " << sizeof(Board) << std::endl;
    std::cout << "sizeof(Node) = " << sizeof(Node) << std::endl;
    std::cout << "sizeof(Move) = " << sizeof(Move) << std::endl;
    std::cout << "sizeof(SquareMap) = " << sizeof(SquareMap) << std::endl;
    std::cout << "sizeof(RepetitionHistory) = " << sizeof(RepetitionHistory) << std::endl;
    std::cout << "sizeof(Evaluation) = " << sizeof(Evaluation) << std::endl;
    return 0;
}
