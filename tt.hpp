#pragma once
#include "board.hpp"
#include <cstdint>
#include <vector>

enum TTFlag {HASH_EXACT, HASH_ALPHA, HASH_BETA, HASH_NONE};

struct TTEntry {
    uint16_t key;     // upper 16 bits of zobrist key for verification - 2 bytes
    Move move;        // best move - 2 bytes
    int16_t score;    // eval score - 2 bytes
    uint8_t depth;    // depth searched - 1 byte
    uint8_t flag;     // flag - 1 byte
    // total: 8 bytes (no padding needed)
};

namespace TT {
    extern std::vector<TTEntry> table;
    extern uint64_t size_mask;

    void resize(int mb);
    void clear();
    
    void store(uint64_t key, int depth, int ply, int score, int flag, Move best_move);
    
    bool probe(uint64_t key, int depth, int ply, int alpha, int beta, int& tt_score, Move& tt_move);
}