#include "zobrist.hpp"
#include "bitboard_helpers.hpp"
#include <random>

namespace Zobrist {
    uint64_t piece_keys[2][6][64];
    uint64_t side_key;
    uint64_t castle_keys[16];
    uint64_t ep_keys[64];

    // 64-bit pseudo-random number generator
    uint64_t random_uint64() {
        static std::mt19937_64 rng(12345);
        return rng();
    }

    void init() {
        for (int c = 0; c < 2; c++) {
            for (int p = 0; p < 6; p++) {
                for (int sq = 0; sq < 64; sq++) {
                    piece_keys[c][p][sq] = random_uint64();
                }
            }
        }
        side_key = random_uint64();
        for (int i = 0; i < 16; i++) {
            castle_keys[i] = random_uint64();
        }
        for (int i = 0; i < 64; i++) {
            ep_keys[i] = random_uint64();
        }
    }

    // only used for initial set_fen
    uint64_t compute_hash(const Board& board) {
        uint64_t hash = 0;
        
        for (int c = 0; c < 2; c++) {
            for (int p = 0; p < 6; p++) {
                bb bitboard = board.get_pieces(to_color(c), to_piece(p));
                while (bitboard) {
                    int sq = BitboardHelpers::pop_lsb(bitboard);
                    hash ^= piece_keys[c][p][sq];
                }
            }
        }
        
        if (board.get_side_to_move() == Color::BLACK) {
            hash ^= side_key;
        }
        hash ^= castle_keys[board.get_castling_rights()]; 
        if (board.get_en_passant_square() != Square::NONE) {
            hash ^= ep_keys[to_idx(board.get_en_passant_square())];
        }
        
        return hash;
    }
}