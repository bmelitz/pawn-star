#pragma once
#include "board.hpp"
#include <cstdint>

// Perft = count every leaf of the legal move tree to a fixed depth.
// Matching known counts is the standard proof that move generation and
// make/unmake are correct (castling, en passant, promotions, pins, checks).
namespace Perft {
    uint64_t perft(Board& board, int depth);

    // prints the node count under each root move, then the total.
    // used for debugging: diff against another engine's divide to find the bad move.
    uint64_t divide(Board& board, int depth);

    // runs the built-in suite of positions with known counts; true if all pass
    bool run_suite();
}
