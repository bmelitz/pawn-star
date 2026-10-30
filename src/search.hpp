#pragma once
#include "board.hpp"
#include "movegen.hpp"

namespace Search {
    constexpr int INFINITY_SCORE = 32000;
    constexpr int MATE_SCORE = 30000;
    constexpr int MAX_PLY = 64;

    extern Move best_move;
    extern int nodes_searched;

    
    std::vector<Move> extract_pv(Board& board, int max_length);

    int get_best_move(Board& board, int depth);
    int quiescence_search(Board& board, int alpha, int beta);
}