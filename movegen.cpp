#include "movegen.hpp"
#include "bitboard_helpers.hpp"
#include <vector>
#include <cstring>
#include <iostream>

namespace MoveGen {
    // lookup tables
    bb knight_attacks[64];
    bb king_attacks[64];
    bb pawn_attacks[2][64]; // [color][square]

    // Total table sizes with variable shifts.
    // Each square's subtable is exactly (1 << mask_bits) entries, where mask_bits
    // is the popcount of that square's occupancy mask (blocker squares on the ray,
    // edges excluded).  Summing over all 64 squares:
    //   Rooks:   102400 entries  (vs 262144 with a uniform 12-bit shift)
    //   Bishops:   5248 entries  (vs  32768 with a uniform  9-bit shift)
    constexpr int ROOK_TABLE_SIZE   = 102400;
    constexpr int BISHOP_TABLE_SIZE = 5248;

    bb rook_table[ROOK_TABLE_SIZE];
    bb bishop_table[BISHOP_TABLE_SIZE];

    // computed magics
    bb rook_magics[64];
    bb bishop_magics[64];

    // per-square shift = 64 - mask_bits (variable, unlike the old fixed 12/9)
    int rook_shifts[64];
    int bishop_shifts[64];

    // offset into the flat table where each square's subtable begins
    int rook_offsets[64];
    int bishop_offsets[64];

    bb* rook_attacks_ptr[64];
    bb* bishop_attacks_ptr[64];
    bb rook_masks[64];
    bb bishop_masks[64];

    uint32_t random_state = 1804289383;
    uint32_t get_random_U32() {
        uint32_t number = random_state;
        number ^= number << 13;
        number ^= number >> 17;
        number ^= number << 5;
        random_state = number;
        return number;
    }

    bb get_random_U64() {
        uint64_t n1 = (uint64_t)(get_random_U32()) & 0xFFFF;
        uint64_t n2 = (uint64_t)(get_random_U32()) & 0xFFFF;
        uint64_t n3 = (uint64_t)(get_random_U32()) & 0xFFFF;
        uint64_t n4 = (uint64_t)(get_random_U32()) & 0xFFFF;
        return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
    }

    bb get_random_magic() {
        return get_random_U64() & get_random_U64() & get_random_U64();
    }

    bb get_rook_attacks_slow(Square sq, bb block) {
        bb attacks = 0;
        int r = to_idx(sq) / 8, f = to_idx(sq) % 8;
        for (int i = r + 1; i < 8; i++) {
            BitboardHelpers::set_bit(attacks, to_square(i * 8 + f));
            if (BitboardHelpers::bit_is_set(block, to_square(i * 8 + f))) break;
        }
        for (int i = r - 1; i >= 0; i--) {
            BitboardHelpers::set_bit(attacks, to_square(i * 8 + f));
            if (BitboardHelpers::bit_is_set(block, to_square(i * 8 + f))) break;
        }
        for (int i = f + 1; i < 8; i++) {
            BitboardHelpers::set_bit(attacks, to_square(r * 8 + i));
            if (BitboardHelpers::bit_is_set(block, to_square(r * 8 + i))) break;
        }
        for (int i = f - 1; i >= 0; i--) {
            BitboardHelpers::set_bit(attacks, to_square(r * 8 + i));
            if (BitboardHelpers::bit_is_set(block, to_square(r * 8 + i))) break;
        }
        return attacks;
    }

    bb get_bishop_attacks_slow(Square sq, bb block) {
        bb attacks = 0;
        int r = to_idx(sq) / 8, f = to_idx(sq) % 8;
        for (int i = 1; r + i < 8 && f + i < 8; i++) {
            BitboardHelpers::set_bit(attacks, to_square((r + i) * 8 + (f + i)));
            if (BitboardHelpers::bit_is_set(block, to_square((r + i) * 8 + (f + i)))) break;
        }
        for (int i = 1; r - i >= 0 && f + i < 8; i++) {
            BitboardHelpers::set_bit(attacks, to_square((r - i) * 8 + (f + i)));
            if (BitboardHelpers::bit_is_set(block, to_square((r - i) * 8 + (f + i)))) break;
        }
        for (int i = 1; r + i < 8 && f - i >= 0; i++) {
            BitboardHelpers::set_bit(attacks, to_square((r + i) * 8 + (f - i)));
            if (BitboardHelpers::bit_is_set(block, to_square((r + i) * 8 + (f - i)))) break;
        }
        for (int i = 1; r - i >= 0 && f - i >= 0; i++) {
            BitboardHelpers::set_bit(attacks, to_square((r - i) * 8 + (f - i)));
            if (BitboardHelpers::bit_is_set(block, to_square((r - i) * 8 + (f - i)))) break;
        }
        return attacks;
    }

    bb set_occupancy(int index, int bits_in_mask, bb attack_mask) {
        bb occupancy = 0;
        for (int i = 0; i < bits_in_mask; i++) {
            int square = BitboardHelpers::get_lsb(attack_mask);
            BitboardHelpers::pop_bit(attack_mask, to_square(square));
            if (index & (1 << i)) {
                BitboardHelpers::set_bit(occupancy, to_square(square));
            }
        }
        return occupancy;
    }

    // Compute per-square offsets into the flat rook/bishop tables.
    // Must be called before init_magic so the offset arrays are ready.
    void init_offsets() {
        int rook_offset = 0;
        int bishop_offset = 0;

        for (int sq = 0; sq < 64; sq++) {
            int r = sq / 8, f = sq % 8;

            // --- rook mask bit count ---
            bb rmask = 0;
            for(int i=r+1; i<7; i++) BitboardHelpers::set_bit(rmask, to_square(i*8+f));
            for(int i=r-1; i>0; i--) BitboardHelpers::set_bit(rmask, to_square(i*8+f));
            for(int i=f+1; i<7; i++) BitboardHelpers::set_bit(rmask, to_square(r*8+i));
            for(int i=f-1; i>0; i--) BitboardHelpers::set_bit(rmask, to_square(r*8+i));
            int rbits = BitboardHelpers::count_bits(rmask);
            rook_offsets[sq] = rook_offset;
            rook_shifts[sq]  = 64 - rbits;
            rook_offset     += 1 << rbits;

            // --- bishop mask bit count ---
            bb bmask = 0;
            for (int i = 1; r + i < 7 && f + i < 7; i++) BitboardHelpers::set_bit(bmask, to_square((r + i) * 8 + (f + i)));
            for (int i = 1; r - i > 0 && f + i < 7; i++) BitboardHelpers::set_bit(bmask, to_square((r - i) * 8 + (f + i)));
            for (int i = 1; r + i < 7 && f - i > 0; i++) BitboardHelpers::set_bit(bmask, to_square((r + i) * 8 + (f - i)));
            for (int i = 1; r - i > 0 && f - i > 0; i++) BitboardHelpers::set_bit(bmask, to_square((r - i) * 8 + (f - i)));
            int bbits = BitboardHelpers::count_bits(bmask);
            bishop_offsets[sq] = bishop_offset;
            bishop_shifts[sq]  = 64 - bbits;
            bishop_offset     += 1 << bbits;
        }
    }

    void init_magic(Square sq, bool is_rook) {
        bb mask = 0;
        int r = to_idx(sq) / 8, f = to_idx(sq) % 8;
        
        if (is_rook) {
            for(int i=r+1; i<7; i++) BitboardHelpers::set_bit(mask, to_square(i*8+f));
            for(int i=r-1; i>0; i--) BitboardHelpers::set_bit(mask, to_square(i*8+f));
            for(int i=f+1; i<7; i++) BitboardHelpers::set_bit(mask, to_square(r*8+i));
            for(int i=f-1; i>0; i--) BitboardHelpers::set_bit(mask, to_square(r*8+i));
            rook_masks[to_idx(sq)] = mask;
        } else {
            for (int i = 1; r + i < 7 && f + i < 7; i++) BitboardHelpers::set_bit(mask, to_square((r + i) * 8 + (f + i)));
            for (int i = 1; r - i > 0 && f + i < 7; i++) BitboardHelpers::set_bit(mask, to_square((r - i) * 8 + (f + i)));
            for (int i = 1; r + i < 7 && f - i > 0; i++) BitboardHelpers::set_bit(mask, to_square((r + i) * 8 + (f - i)));
            for (int i = 1; r - i > 0 && f - i > 0; i++) BitboardHelpers::set_bit(mask, to_square((r - i) * 8 + (f - i)));
            bishop_masks[to_idx(sq)] = mask;
        }

        // mask_bits drives both the shift (64 - mask_bits) and the subtable size
        // (1 << mask_bits).  This is the key difference from the old fixed-shift
        // approach: each square uses exactly as many index bits as its mask has set
        // bits, so no table space is wasted.
        int mask_bits  = BitboardHelpers::count_bits(mask);
        int shift      = 64 - mask_bits;
        int table_size = 1 << mask_bits;
        int num_indices = table_size; // every occupancy subset maps to a unique index

        bb* table_start = is_rook ? &rook_table[rook_offsets[to_idx(sq)]]
                                  : &bishop_table[bishop_offsets[to_idx(sq)]];

        std::vector<bb> occupancies(num_indices);
        std::vector<bb> attacks(num_indices);

        for (int i = 0; i < num_indices; i++) {
            occupancies[i] = set_occupancy(i, mask_bits, mask);
            if (is_rook) attacks[i] = get_rook_attacks_slow(sq, occupancies[i]);
            else         attacks[i] = get_bishop_attacks_slow(sq, occupancies[i]);
        }

        for (int k = 0; k < 100000000; k++) {
            bb magic = get_random_magic();
            if (BitboardHelpers::count_bits((mask * magic) & 0xFF00000000000000ULL) < 6) continue;

            std::vector<bb> used(table_size, 0); 
            std::vector<bool> index_exists(table_size, false);
            bool fail = false;
            
            for (int i = 0; i < num_indices; i++) {
                int magic_index = (int)((occupancies[i] * magic) >> shift);
                if (!index_exists[magic_index]) {
                    used[magic_index] = attacks[i];
                    index_exists[magic_index] = true;
                } else if (used[magic_index] != attacks[i]) {
                    fail = true;
                    break;
                }
            }

            if (!fail) {
                if (is_rook) {
                    rook_magics[to_idx(sq)]      = magic;
                    rook_attacks_ptr[to_idx(sq)] = table_start;
                } else {
                    bishop_magics[to_idx(sq)]      = magic;
                    bishop_attacks_ptr[to_idx(sq)] = table_start;
                }
                for (int i = 0; i < num_indices; i++) {
                    int magic_index = (int)((occupancies[i] * magic) >> shift);
                    table_start[magic_index] = attacks[i];
                }
                return;
            }
        }
        std::cout << "Magic generation failed for square " << to_idx(sq) << "\n";
    }

    void init_tables() {
        init_offsets();

        for (int sq = 0; sq < 64; ++sq) {
            int rank = sq / 8;
            int file = sq % 8;

            // knight attacks
            bb knight = 0;
            const int n_ranks[] = {2, 1, -1, -2, -2, -1, 1, 2};
            const int n_files[] = {1, 2, 2, 1, -1, -2, -2, -1};
            for (int i = 0; i < 8; ++i) {
                int r = rank + n_ranks[i];
                int f = file + n_files[i];
                if (r >= 0 && r < 8 && f >= 0 && f < 8) {
                    BitboardHelpers::set_bit(knight, to_square(r * 8 + f));
                }
            }
            knight_attacks[sq] = knight;

            // king attacks
            bb king = 0;
            const int k_steps[] = {-1, 0, 1};
            for (int dr : k_steps) {
                for (int df : k_steps) {
                    if (dr == 0 && df == 0) continue;
                    int r = rank + dr;
                    int f = file + df;
                    if (r >= 0 && r < 8 && f >= 0 && f < 8) {
                        BitboardHelpers::set_bit(king, to_square(r * 8 + f));
                    }
                }
            }
            king_attacks[sq] = king;

            // pawn attacks
            bb white_pawn = 0;
            bb black_pawn = 0;
            if (rank < 7) {
                if (file < 7) BitboardHelpers::set_bit(white_pawn, to_square(sq + 9));
                if (file > 0) BitboardHelpers::set_bit(white_pawn, to_square(sq + 7));
            }
            pawn_attacks[to_idx(Color::WHITE)][sq] = white_pawn;
            if (rank > 0) {
                if (file < 7) BitboardHelpers::set_bit(black_pawn, to_square(sq - 7));
                if (file > 0) BitboardHelpers::set_bit(black_pawn, to_square(sq - 9));
            }
            pawn_attacks[to_idx(Color::BLACK)][sq] = black_pawn;
        }

        for (int sq = 0; sq < 64; sq++) {
            init_magic(to_square(sq), true);
            init_magic(to_square(sq), false);
        }
    }

    bb get_knight_attacks(Square sq) { return knight_attacks[to_idx(sq)]; }
    bb get_king_attacks(Square sq) { return king_attacks[to_idx(sq)]; }
    bb get_pawn_attacks(Square sq, Color c) { return pawn_attacks[to_idx(c)][to_idx(sq)]; }

    bb get_rook_attacks(Square sq, bb occupancy) {
        occupancy &= rook_masks[to_idx(sq)];
        occupancy *= rook_magics[to_idx(sq)];
        occupancy >>= rook_shifts[to_idx(sq)];
        return rook_attacks_ptr[to_idx(sq)][occupancy];
    }

    bb get_bishop_attacks(Square sq, bb occupancy) {
        occupancy &= bishop_masks[to_idx(sq)];
        occupancy *= bishop_magics[to_idx(sq)];
        occupancy >>= bishop_shifts[to_idx(sq)];
        return bishop_attacks_ptr[to_idx(sq)][occupancy];
    }

    bb get_queen_attacks(Square sq, bb occupancy) {
        return get_rook_attacks(sq, occupancy) | get_bishop_attacks(sq, occupancy);
    }

    bool is_square_attacked(const Board& board, Square sq, Color attacker) {
        // pawns
        bb pawns = board.get_pieces(attacker, Piece::PAWN);
        if (attacker == Color::WHITE) {
            if (pawn_attacks[to_idx(Color::BLACK)][to_idx(sq)] & pawns) return true;
        } else {
            if (pawn_attacks[to_idx(Color::WHITE)][to_idx(sq)] & pawns) return true;
        }

        // knights
        if (get_knight_attacks(sq) & board.get_pieces(attacker, Piece::KNIGHT)) return true;

        // kings
        if (get_king_attacks(sq) & board.get_pieces(attacker, Piece::KING)) return true;

        // rooks / queens
        bb rooks_queens = board.get_pieces(attacker, Piece::ROOK) | board.get_pieces(attacker, Piece::QUEEN);
        if (get_rook_attacks(sq, board.get_all_occupancy()) & rooks_queens) return true;

        // bishops / queens
        bb bishops_queens = board.get_pieces(attacker, Piece::BISHOP) | board.get_pieces(attacker, Piece::QUEEN);
        if (get_bishop_attacks(sq, board.get_all_occupancy()) & bishops_queens) return true;

        return false;
    }

    // only considers captures and promotions (only queen and knight promotions, regardless of capture)
    void generate_pseudo_moves(const Board& board, MoveList& list) {
        list.count = 0; 
        
        Color us = board.get_side_to_move();
        Color them = ~us;
        
        bb our_pieces = board.get_occupancy(us);
        bb their_pieces = board.get_occupancy(them);
        bb all_pieces = board.get_all_occupancy();

        Square ep_square = board.get_en_passant_square();
        uint8_t castling = board.get_castling_rights();

        // pawns
        bb pawns = board.get_pieces(us, Piece::PAWN);

        bb capture_mask = their_pieces;
        if (ep_square != Square::NONE) {
            BitboardHelpers::set_bit(capture_mask, ep_square);
        }

        if (us == Color::WHITE) {
            bb single_push = (pawns << 8) & ~all_pieces;
            bb double_push = ((single_push & 0x0000000000FF0000ULL) << 8) & ~all_pieces; 
            
            while (single_push) {
                int to = BitboardHelpers::pop_lsb(single_push);
                int from = to - 8;
                if (to >= 56) {
                    list.add(Move(to_square(from), to_square(to), Move::PROMO_Q));
                    list.add(Move(to_square(from), to_square(to), Move::PROMO_N));
                } else {
                    list.add(Move(to_square(from), to_square(to), Move::QUIET));
                }
            }
            while (double_push) {
                int to = BitboardHelpers::pop_lsb(double_push);
                list.add(Move(to_square(to - 16), to_square(to), Move::DOUBLE_PAWN_PUSH));
            }

            bb captures_left = (pawns << 7) & 0x7F7F7F7F7F7F7F7FULL & capture_mask;
            bb captures_right = (pawns << 9) & 0xFEFEFEFEFEFEFEFEULL & capture_mask;
            
            while (captures_left) {
                int to = BitboardHelpers::pop_lsb(captures_left);
                int from = to - 7;
                Square dest = to_square(to);
                if (dest == ep_square) {
                    list.add(Move(to_square(from), dest, Move::EP_CAPTURE));
                } else if (to >= 56) {
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_Q));
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_R));
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_B));
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_N));
                } else {
                    list.add(Move(to_square(from), dest, Move::CAPTURE));
                }
            }
            while (captures_right) {
                int to = BitboardHelpers::pop_lsb(captures_right);
                int from = to - 9;
                Square dest = to_square(to);
                if (dest == ep_square) {
                    list.add(Move(to_square(from), dest, Move::EP_CAPTURE));
                } else if (to >= 56) {
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_Q));
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_R));
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_B));
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_N));
                } else {
                    list.add(Move(to_square(from), dest, Move::CAPTURE));
                }
            }
        }
        else {
            bb single_push = (pawns >> 8) & ~all_pieces;
            bb double_push = ((single_push & 0x0000FF0000000000ULL) >> 8) & ~all_pieces; 
            
            while (single_push) {
                int to = BitboardHelpers::pop_lsb(single_push);
                int from = to + 8;
                if (to <= 7) { 
                    list.add(Move(to_square(from), to_square(to), Move::PROMO_Q));
                    list.add(Move(to_square(from), to_square(to), Move::PROMO_N));
                } else {
                    list.add(Move(to_square(from), to_square(to), Move::QUIET));
                }
            }
            while (double_push) {
                int to = BitboardHelpers::pop_lsb(double_push);
                list.add(Move(to_square(to + 16), to_square(to), Move::DOUBLE_PAWN_PUSH));
            }

            bb captures_right = (pawns >> 7) & 0xFEFEFEFEFEFEFEFEULL & capture_mask;
            bb captures_left  = (pawns >> 9) & 0x7F7F7F7F7F7F7F7FULL & capture_mask;
            
            while (captures_right) {
                int to = BitboardHelpers::pop_lsb(captures_right);
                int from = to + 7;
                Square dest = to_square(to);
                if (dest == ep_square) {
                    list.add(Move(to_square(from), dest, Move::EP_CAPTURE));
                } else if (to <= 7) { 
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_Q));
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_N));
                } else {
                    list.add(Move(to_square(from), dest, Move::CAPTURE));
                }
            }
            while (captures_left) {
                int to = BitboardHelpers::pop_lsb(captures_left);
                int from = to + 9;
                Square dest = to_square(to);
                if (dest == ep_square) {
                    list.add(Move(to_square(from), dest, Move::EP_CAPTURE));
                } else if (to <= 7) { 
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_Q));
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_N));
                } else {
                    list.add(Move(to_square(from), dest, Move::CAPTURE));
                }
            }
        }

        // castling
        if (us == Color::WHITE) {
            // wk
            if (castling & 1) {
                if (!BitboardHelpers::bit_is_set(all_pieces, Square::F1) && 
                    !BitboardHelpers::bit_is_set(all_pieces, Square::G1)) {
                    if (!is_square_attacked(board, Square::E1, them) &&
                        !is_square_attacked(board, Square::F1, them) &&
                        !is_square_attacked(board, Square::G1, them)) {
                        list.add(Move(Square::E1, Square::G1, Move::KING_CASTLE));
                    }
                }
            }
            // wq
            if (castling & 2) {
                if (!BitboardHelpers::bit_is_set(all_pieces, Square::D1) && 
                    !BitboardHelpers::bit_is_set(all_pieces, Square::C1) &&
                    !BitboardHelpers::bit_is_set(all_pieces, Square::B1)) {
                    if (!is_square_attacked(board, Square::E1, them) &&
                        !is_square_attacked(board, Square::D1, them) &&
                        !is_square_attacked(board, Square::C1, them)) {
                        list.add(Move(Square::E1, Square::C1, Move::QUEEN_CASTLE));
                    }
                }
            }
        } else {
            // bk
            if (castling & 4) {
                if (!BitboardHelpers::bit_is_set(all_pieces, Square::F8) && 
                    !BitboardHelpers::bit_is_set(all_pieces, Square::G8)) {
                    if (!is_square_attacked(board, Square::E8, them) &&
                        !is_square_attacked(board, Square::F8, them) &&
                        !is_square_attacked(board, Square::G8, them)) {
                        list.add(Move(Square::E8, Square::G8, Move::KING_CASTLE));
                    }
                }
            }
            // bq
            if (castling & 8) {
                if (!BitboardHelpers::bit_is_set(all_pieces, Square::D8) && 
                    !BitboardHelpers::bit_is_set(all_pieces, Square::C8) &&
                    !BitboardHelpers::bit_is_set(all_pieces, Square::B8)) {
                    if (!is_square_attacked(board, Square::E8, them) &&
                        !is_square_attacked(board, Square::D8, them) &&
                        !is_square_attacked(board, Square::C8, them)) {
                        list.add(Move(Square::E8, Square::C8, Move::QUEEN_CASTLE));
                    }
                }
            }
        }

        // knight
        bb knights = board.get_pieces(us, Piece::KNIGHT);
        while (knights) {
            int from = BitboardHelpers::pop_lsb(knights);
            bb attacks = get_knight_attacks(to_square(from)) & ~our_pieces;
            while (attacks) {
                int to = BitboardHelpers::pop_lsb(attacks);
                if (BitboardHelpers::bit_is_set(their_pieces, to_square(to)))
                    list.add(Move(to_square(from), to_square(to), Move::CAPTURE));
                else
                    list.add(Move(to_square(from), to_square(to), Move::QUIET));
            }
        }

        // king
        bb king = board.get_pieces(us, Piece::KING);
        if (king) {
            int from = BitboardHelpers::pop_lsb(king);
            bb attacks = get_king_attacks(to_square(from)) & ~our_pieces;
            while (attacks) {
                int to = BitboardHelpers::pop_lsb(attacks);
                if (BitboardHelpers::bit_is_set(their_pieces, to_square(to)))
                    list.add(Move(to_square(from), to_square(to), Move::CAPTURE));
                else
                    list.add(Move(to_square(from), to_square(to), Move::QUIET));
            }
        }

        // sliders
        bb rooks = board.get_pieces(us, Piece::ROOK);
        bb bishops = board.get_pieces(us, Piece::BISHOP);
        bb queens = board.get_pieces(us, Piece::QUEEN);
        bb bishop_like = bishops | queens;
        bb rook_like = rooks | queens;

        while (bishop_like) {
            int from = BitboardHelpers::pop_lsb(bishop_like);
            bb attacks = get_bishop_attacks(to_square(from), all_pieces) & ~our_pieces;
            while (attacks) {
                int to = BitboardHelpers::pop_lsb(attacks);
                if (BitboardHelpers::bit_is_set(their_pieces, to_square(to)))
                    list.add(Move(to_square(from), to_square(to), Move::CAPTURE));
                else
                    list.add(Move(to_square(from), to_square(to), Move::QUIET));
            }
        }

        while (rook_like) {
            int from = BitboardHelpers::pop_lsb(rook_like);
            bb attacks = get_rook_attacks(to_square(from), all_pieces) & ~our_pieces;
            while (attacks) {
                int to = BitboardHelpers::pop_lsb(attacks);
                if (BitboardHelpers::bit_is_set(their_pieces, to_square(to)))
                    list.add(Move(to_square(from), to_square(to), Move::CAPTURE));
                else
                    list.add(Move(to_square(from), to_square(to), Move::QUIET));
            }
        }
    }

    // Generates captures, en-passant captures, and promotions for use in
    // quiescence search. Only queen and knight promotions are generated —
    // rook and bishop promotions are strictly dominated (a queen does
    // everything a rook or bishop does, and an underpromotion to rook/bishop
    // is only relevant in rare zugzwang or stalemate-avoidance cases that
    // quiescence search need not handle). No quiet moves, no castling.
    void generate_captures(const Board& board, MoveList& list) {
        list.count = 0;

        Color us = board.get_side_to_move();
        bb their_pieces = board.get_occupancy(~us);
        bb all_pieces = board.get_all_occupancy();

        Square ep_square = board.get_en_passant_square();

        // pawns
        bb pawns = board.get_pieces(us, Piece::PAWN);

        bb capture_mask = their_pieces;
        if (ep_square != Square::NONE) {
            BitboardHelpers::set_bit(capture_mask, ep_square);
        }

        if (us == Color::WHITE) {
            // quiet promotions to queen and knight only
            bb single_push = (pawns << 8) & ~all_pieces;
            bb promo_push = single_push & 0xFF00000000000000ULL;
            while (promo_push) {
                int to = BitboardHelpers::pop_lsb(promo_push);
                int from = to - 8;
                list.add(Move(to_square(from), to_square(to), Move::PROMO_Q));
                list.add(Move(to_square(from), to_square(to), Move::PROMO_N));
            }

            bb captures_left = (pawns << 7) & 0x7F7F7F7F7F7F7F7FULL & capture_mask;
            bb captures_right = (pawns << 9) & 0xFEFEFEFEFEFEFEFEULL & capture_mask;

            while (captures_left) {
                int to = BitboardHelpers::pop_lsb(captures_left);
                int from = to - 7;
                Square dest = to_square(to);
                if (dest == ep_square) {
                    list.add(Move(to_square(from), dest, Move::EP_CAPTURE));
                } else if (to >= 56) {
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_Q));
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_N));
                } else {
                    list.add(Move(to_square(from), dest, Move::CAPTURE));
                }
            }
            while (captures_right) {
                int to = BitboardHelpers::pop_lsb(captures_right);
                int from = to - 9;
                Square dest = to_square(to);
                if (dest == ep_square) {
                    list.add(Move(to_square(from), dest, Move::EP_CAPTURE));
                } else if (to >= 56) {
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_Q));
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_N));
                } else {
                    list.add(Move(to_square(from), dest, Move::CAPTURE));
                }
            }
        }
        else {
            // quiet promotions to queen and knight only
            bb single_push = (pawns >> 8) & ~all_pieces;
            bb promo_push = single_push & 0x00000000000000FFULL;
            while (promo_push) {
                int to = BitboardHelpers::pop_lsb(promo_push);
                int from = to + 8;
                list.add(Move(to_square(from), to_square(to), Move::PROMO_Q));
                list.add(Move(to_square(from), to_square(to), Move::PROMO_N));
            }

            bb captures_right = (pawns >> 7) & 0xFEFEFEFEFEFEFEFEULL & capture_mask;
            bb captures_left  = (pawns >> 9) & 0x7F7F7F7F7F7F7F7FULL & capture_mask;

            while (captures_right) {
                int to = BitboardHelpers::pop_lsb(captures_right);
                int from = to + 7;
                Square dest = to_square(to);
                if (dest == ep_square) {
                    list.add(Move(to_square(from), dest, Move::EP_CAPTURE));
                } else if (to <= 7) {
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_Q));
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_N));
                } else {
                    list.add(Move(to_square(from), dest, Move::CAPTURE));
                }
            }
            while (captures_left) {
                int to = BitboardHelpers::pop_lsb(captures_left);
                int from = to + 9;
                Square dest = to_square(to);
                if (dest == ep_square) {
                    list.add(Move(to_square(from), dest, Move::EP_CAPTURE));
                } else if (to <= 7) {
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_Q));
                    list.add(Move(to_square(from), dest, Move::PROMO_CAPTURE_N));
                } else {
                    list.add(Move(to_square(from), dest, Move::CAPTURE));
                }
            }
        }

        // no castling — castling is never a capture

        // knight
        bb knights = board.get_pieces(us, Piece::KNIGHT);
        while (knights) {
            int from = BitboardHelpers::pop_lsb(knights);
            bb attacks = get_knight_attacks(to_square(from)) & their_pieces;
            while (attacks) {
                int to = BitboardHelpers::pop_lsb(attacks);
                list.add(Move(to_square(from), to_square(to), Move::CAPTURE));
            }
        }

        // king
        bb king = board.get_pieces(us, Piece::KING);
        if (king) {
            int from = BitboardHelpers::pop_lsb(king);
            bb attacks = get_king_attacks(to_square(from)) & their_pieces;
            while (attacks) {
                int to = BitboardHelpers::pop_lsb(attacks);
                list.add(Move(to_square(from), to_square(to), Move::CAPTURE));
            }
        }

        // sliders
        bb rooks = board.get_pieces(us, Piece::ROOK);
        bb bishops = board.get_pieces(us, Piece::BISHOP);
        bb queens = board.get_pieces(us, Piece::QUEEN);
        bb bishop_like = bishops | queens;
        bb rook_like = rooks | queens;

        while (bishop_like) {
            int from = BitboardHelpers::pop_lsb(bishop_like);
            bb attacks = get_bishop_attacks(to_square(from), all_pieces) & their_pieces;
            while (attacks) {
                int to = BitboardHelpers::pop_lsb(attacks);
                list.add(Move(to_square(from), to_square(to), Move::CAPTURE));
            }
        }

        while (rook_like) {
            int from = BitboardHelpers::pop_lsb(rook_like);
            bb attacks = get_rook_attacks(to_square(from), all_pieces) & their_pieces;
            while (attacks) {
                int to = BitboardHelpers::pop_lsb(attacks);
                list.add(Move(to_square(from), to_square(to), Move::CAPTURE));
            }
        }
    }
}
