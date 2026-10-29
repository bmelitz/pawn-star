#include "board.hpp"
#include "movegen.hpp"
#include "eval.hpp"
#include "search.hpp"
#include "zobrist.hpp"
#include "tt.hpp"
#include "bitboard_helpers.hpp"
#include <iostream>

int main() {
    MoveGen::init_tables();
    Zobrist::init();
    TT::resize(16);
    
    Board b;
    b.set_fen("8/8/2N5/8/1k2p3/1p3p2/3K1P2/8 b - - 0 1");
    
    int score = Search::get_best_move(b, 15);
    
    std::cout << "\nBest move: ";
    BitboardHelpers::print_move(Search::best_move);
    std::cout << "\nScore: " << score << " cp\n";
    
    return 0;
}
