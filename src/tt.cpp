#include "tt.hpp"
#include "search.hpp" // For MATE_SCORE

namespace TT {
    std::vector<TTEntry> table;
    uint64_t size_mask = 0;

    // Scores >= this threshold are considered mate scores and need ply adjustment
    // Must be less than MATE_SCORE to create a safe band
    constexpr int MATE_SCORE_THRESHOLD = 29000;

    void resize(int mb) {
        // calculate how many entries fit in 'mb' megabytes
        uint64_t bytes = mb * 1024 * 1024;
        uint64_t entry_count = bytes / sizeof(TTEntry);
        
        // size must be a power of 2 for fast bitwise
        uint64_t power_of_2 = 1;
        while ((power_of_2 * 2) <= entry_count) {
            power_of_2 *= 2;
        }
        
        table.resize(power_of_2);
        size_mask = power_of_2 - 1;
        clear();
    }

    void clear() {
        for (auto& entry : table) {
            entry.key = 0;
            entry.depth = 0;
            entry.flag = HASH_NONE;
        }
    }

    int score_to_tt(int score, int ply) {
        if (score > MATE_SCORE_THRESHOLD) {
            return score + ply;
        }
        if (score < -MATE_SCORE_THRESHOLD) {
            return score - ply;
        }
        return score;
    }

    int score_from_tt(int score, int ply) {
        if (score > MATE_SCORE_THRESHOLD) {
            return score - ply;
        }
        if (score < -MATE_SCORE_THRESHOLD) {
            return score + ply;
        }
        return score;
    }

    // extract upper 16 bits of zobrist key for verification
    static inline uint16_t verify_key(uint64_t key) {
        return static_cast<uint16_t>(key >> 48);
    }

    void store(uint64_t key, int depth, int ply, int score, int flag, Move best_move) {
        TTEntry& entry = table[key & size_mask];
        uint16_t vkey = verify_key(key);

        // Prefer higher depth entries
        // Overwrite if the positions differ, regardless of depth
        // This is because if position A is cached at high depth, this bucket is essentially reserved for A
        // But if we aren't probing A much and moved to a different variation, other positions that hash to A get ignored
        // I could prefer EXACT over ALPHA or BETA, but a very small percent of tt entry hits are EXACT so it doesn't matter
        if (entry.key == vkey && depth < entry.depth) {
            return;
        }

        entry.key = vkey;
        entry.depth = depth;
        entry.flag = flag;
        entry.score = score_to_tt(score, ply);

        if (best_move.data != 0) {
            entry.move = best_move; 
        }
    }

    bool probe(uint64_t key, int depth, int ply, int alpha, int beta, int& tt_score, Move& tt_move) {
        TTEntry& entry = table[key & size_mask];
        uint16_t vkey = verify_key(key);

        // hash collision check
        if (entry.key != vkey) {
            return false;
        }

        tt_move = entry.move;

        if (entry.depth < depth) {
            return false;
        }

        tt_score = score_from_tt(entry.score, ply);

        if (entry.flag == HASH_EXACT ||
            (entry.flag == HASH_ALPHA && tt_score <= alpha) ||
            (entry.flag == HASH_BETA && tt_score >= beta)) {
            return true;
        }

        return false;
    }
}