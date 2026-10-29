#pragma once
#include "board.hpp"
#include <cstdint>

namespace Zobrist {
    // pieces, side to move, castling rights, en passant square
    extern uint64_t piece_keys[2][6][64];
    extern uint64_t side_key;
    extern uint64_t castle_keys[16];
    extern uint64_t ep_keys[64];

    void init();
    uint64_t compute_hash(const Board& board);
}