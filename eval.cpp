#include "eval.hpp"
#include "board.hpp"
#include "bitboard_helpers.hpp"

namespace Eval {
    // middle game material
    const int mg_value[6] = {82, 337, 365, 477, 1025, 0};
    // end game material
    const int eg_value[6] = {94, 281, 297, 512,  936, 0};

    // game phase (to interpolate between mg and eg)
    // total Phase = 24. p=0, n=1, b=1, r=2, q=4
    const int game_phase_inc[6] = {0, 1, 1, 2, 4, 0};

    // [piece][square] - a1 is 0, h1 is 7
    
    const int mg_pawn_table[64] = {
         0,   0,   0,   0,   0,   0,   0,   0,
        98, 134,  61,  95,  68, 126,  34, -11,
        -6,   7,  26,  31,  65,  56,  25, -20,
       -14,  13,   6,  21,  23,  12,  17, -23,
       -27,  -2,  -5,  12,  17,   6,  10, -25,
       -26,  -4,  -4, -10,   3,   3,  33, -12,
       -35,  -1, -20, -23, -15,  24,  38, -22,
         0,   0,   0,   0,   0,   0,   0,   0,
    };

    const int eg_pawn_table[64] = {
         0,   0,   0,   0,   0,   0,   0,   0,
       178, 173, 158, 134, 147, 132, 165, 187,
        94, 100,  85,  67,  56,  53,  82,  84,
        32,  24,  13,   5,  -2,   4,  17,  17,
        13,   9,  -3,  -7,  -7,  -8,   3,  -1,
         4,   7,  -6,   1,   0,  -5,  -1,  -8,
        13,   8,   8,  10,  13,   0,   2,  -7,
         0,   0,   0,   0,   0,   0,   0,   0,
    };

    const int mg_knight_table[64] = {
       -167, -89, -34, -49,  61, -97, -15, -107,
        -73, -41,  72,  36,  23,  62,   7,  -17,
        -47,  60,  37,  65,  84, 129,  73,   44,
         -9,  17,  19,  53,  37,  69,  18,   22,
        -13,   4,  16,  13,  28,  19,  21,   -8,
        -23,  -9,  12,  10,  19,  17,  25,  -16,
        -29, -53, -12,  -3,  -1,  18, -14,  -19,
       -105, -21, -58, -33, -17, -28, -19,  -23,
    };

    const int eg_knight_table[64] = {
        -58, -38, -13, -28, -31, -27, -63, -99,
        -25,  -8, -25,  -2,  -9, -25, -24, -52,
        -24, -20,  10,   9,  -1,  -9, -19, -41,
        -17,   3,  22,  22,  22,  11,   8, -18,
        -18,  -6,  16,  25,  16,  17,   4, -18,
        -23,  -3,  -1,  15,  10,  -3, -20, -22,
        -42, -20, -10,  -5,  -2, -20, -23, -44,
        -29, -51, -23, -15, -22, -18, -50, -64,
    };

    const int mg_bishop_table[64] = {
        -29,   4, -82, -37, -25, -42,   7,  -8,
        -26,  16, -18, -13,  30,  59,  18, -47,
        -16,  37,  43,  40,  35,  50,  37,  -2,
         -4,   5,  19,  50,  37,  37,   7,  -2,
         -6,  13,  13,  26,  34,  12,  10,   4,
          0,  15,  15,  15,  14,  27,  18,  10,
          4,  15,  16,   0,   7,  21,  33,   1,
        -33,  -3, -14, -21, -13, -12, -39, -21,
    };

    const int eg_bishop_table[64] = {
        -14, -21, -11,  -8, -7,  -9, -17, -24,
         -8,  -4,   7, -12, -3, -13,  -4, -14,
          2,  -8,   0,  -1, -2,   6,   0,   4,
         -3,   9,  12,   9, 14,  10,   3,   2,
         -6,   3,  13,  19,  7,  10,  -3,  -9,
        -12,  -3,   5,  10,  5,  -5,  -7, -23,
        -14, -18,  -7,  -1,  4,  -9, -15, -27,
        -23,  -9, -23,  -5, -9, -16, -5, -17,
    };

    const int mg_rook_table[64] = {
         32,  42,  32,  51, 63,  9,  31,  43,
         27,  32,  58,  62, 80, 67,  26,  44,
         -5,  19,  26,  36, 17, 45,  61,  16,
        -24, -11,   7,  26, 24, 35,  -8, -20,
        -36, -26, -12,  -1,  9, -7,   6, -23,
        -45, -25, -16, -17,  3,  0,  -5, -33,
        -44, -16, -20,  -9, -1, 11,  -6, -71,
        -19, -13,   1,  17, 16,  7, -37, -26,
    };

    const int eg_rook_table[64] = {
        13, 10, 18, 15, 12,  12,   8,   5,
        11, 13, 13, 11, -3,   3,   8,   3,
         7,  7,  7,  5,  4,  -3,  -5,  -3,
         4,  3, 13,  1,  2,   1,  -1,   2,
         3,  5,  8,  4, -5,  -6,  -8, -11,
        -4,  0, -5, -1, -7, -12,  -8, -16,
        -6, -6,  0,  2, -9,  -9, -11,  -3,
        -9,  2,  3, -1, -5, -13,   4, -20,
    };

    const int mg_queen_table[64] = {
        -28,   0,  29,  12,  59,  44,  43,  45,
        -24, -39,  -5,   1, -16,  57,  28,  54,
        -13, -17,   7,   8,  29,  56,  47,  57,
        -27, -27, -16, -16,  -1,  17,  -2,   1,
         -9, -26, -28, -10, -10, -11, -33, -10,
        -14,   2, -11,  -2,  -5,   2,  14,   5,
        -35,  -8,  11,   2,   8,  15,  -3,   1,
         -1, -18,  -9,  10, -15, -25, -31, -50,
    };

    const int eg_queen_table[64] = {
         -9,  22,  22,  27,  27,  19,  10,  20,
        -17,  20,  32,  41,  58,  25,  30,   0,
        -20,   6,   9,  49,  47,  35,  19,   9,
          3,  22,  24,  45,  57,  40,  57,  36,
        -18,  28,  19,  47,  31,  34,  39,  23,
        -16, -27,  15,   6,   9,  17,  10,   5,
        -22, -23, -30, -16, -16,  23, -36, -32,
        -33, -28, -22, -43,  -5, -32, -20, -41,
    };

    const int mg_king_table[64] = {
        -65,  23,  16, -15, -56, -34,   2,  13,
         29,  -1, -20,  -7,  -8,  -4, -38, -29,
         -9,  24,   2, -16, -20,   6,  22, -22,
        -17, -20, -12, -27, -30, -25, -14, -36,
        -49,  -1, -27, -39, -46, -44, -33, -51,
        -14, -14, -22, -46, -44, -30, -15, -27,
          1,   7,  -8, -64, -43, -16,   9,   8,
        -15,  36,  12, -54,   8, -28,  24,  14,
    };

    const int eg_king_table[64] = {
        -74, -35, -18, -18, -11,  15,   4, -17,
        -12,  17,  14,  17,  17,  38,  23,  11,
         10,  17,  23,  15,  20,  45,  44,  13,
         -8,  22,  24,  27,  26,  33,  26,   3,
        -18,  -4,  21,  24,  27,  23,   9, -11,
        -19,  -3,  11,  21,  23,  16,   7,  -9,
        -27, -11,   4,  13,  14,   4,  -5, -17,
        -53, -34, -21, -11, -28, -14, -24, -43
    };

    const int* mg_tables[6] = {mg_pawn_table, mg_knight_table, mg_bishop_table, mg_rook_table, mg_queen_table, mg_king_table};
    const int* eg_tables[6] = {eg_pawn_table, eg_knight_table, eg_bishop_table, eg_rook_table, eg_queen_table, eg_king_table};

    int evaluate(const Board& board) {
        int mg[2] = {0, 0};
        int eg[2] = {0, 0};
        int game_phase = 0;

        for (int c = 0; c < 2; ++c) {
            Color color = to_color(c);
            
            for (int p = 0; p < 6; ++p) {
                Piece piece = to_piece(p);
                bb bitboard = board.get_pieces(color, piece);
                
                while (bitboard) {
                    Square sq = to_square(BitboardHelpers::pop_lsb(bitboard));
                    
                    // material
                    mg[c] += mg_value[p];
                    eg[c] += eg_value[p];
                    game_phase += game_phase_inc[p];

                    // positional (flip square for black)
                    int table_idx = (color == Color::WHITE) ? (to_idx(sq) ^ 56) : to_idx(sq);
                    
                    mg[c] += mg_tables[p][table_idx];
                    eg[c] += eg_tables[p][table_idx];
                }
            }
        }

        int mg_score = mg[to_idx(board.get_side_to_move())] - mg[to_idx(~board.get_side_to_move())];
        int eg_score = eg[to_idx(board.get_side_to_move())] - eg[to_idx(~board.get_side_to_move())];
        
        // ensure phase doesn't exceed 24
        int phase = (game_phase > 24) ? 24 : game_phase;
        
        // interpolate
        return (mg_score * phase + eg_score * (24 - phase)) / 24;
    }

    // debugging
    void trace_evaluate(const Board& board) {
        int mg[2] = {0, 0};
        int eg[2] = {0, 0};
        int game_phase = 0;

        std::cout << "======================== EVALUATION TRACE ========================\n";
        std::cout << "Piece Sq | MG Mat | EG Mat | MG PSQT | EG PSQT | MG Tot | EG Tot\n";
        std::cout << "------------------------------------------------------------------\n";

        for (int c = 0; c < 2; ++c) {
            Color color = to_color(c);
            for (int p = 0; p < 6; ++p) {
                Piece piece = to_piece(p);
                bb bitboard = board.get_pieces(color, piece);

                while (bitboard) {
                    Square sq = to_square(BitboardHelpers::pop_lsb(bitboard));
                    
                    // The crucial square flip for White!
                    int table_idx = (color == Color::WHITE) ? (to_idx(sq) ^ 56) : to_idx(sq);

                    int mg_m = mg_value[p];
                    int eg_m = eg_value[p];
                    int mg_p = mg_tables[p][table_idx];
                    int eg_p = eg_tables[p][table_idx];

                    mg[c] += mg_m + mg_p;
                    eg[c] += eg_m + eg_p;
                    game_phase += game_phase_inc[p];

                    // Print row data
                    char pc = "PNBRQKpnbrqk"[c * 6 + p];
                    char f = 'a' + (to_idx(sq) % 8);
                    char r = '1' + (to_idx(sq) / 8);

                    printf("%c     %c%c | %6d | %6d | %7d | %7d | %6d | %6d\n",
                           pc, f, r, mg_m, eg_m, mg_p, eg_p, mg_m + mg_p, eg_m + eg_p);
                }
            }
            if (c == 0) std::cout << "------------------------------------------------------------------\n";
        }

        int phase = (game_phase > 24) ? 24 : game_phase;
        
        // Calculate diffs from the perspective of the side to move
        int stm = to_idx(board.get_side_to_move());
        int enemy = to_idx(~board.get_side_to_move());
        
        int mg_score = mg[stm] - mg[enemy];
        int eg_score = eg[stm] - eg[enemy];
        
        int final_score = (mg_score * phase + eg_score * (24 - phase)) / 24;

        std::cout << "------------------------------------------------------------------\n";
        std::cout << "White MG Total: " << mg[0] << "\n";
        std::cout << "Black MG Total: " << mg[1] << "\n";
        std::cout << "White EG Total: " << eg[0] << "\n";
        std::cout << "Black EG Total: " << eg[1] << "\n";
        std::cout << "Game Phase:     " << game_phase << " (capped at " << phase << ") / 24\n";
        std::cout << "Side to Move:   " << (stm == 0 ? "White" : "Black") << "\n";
        std::cout << "MG Difference:  " << mg_score << " (from STM perspective)\n";
        std::cout << "EG Difference:  " << eg_score << " (from STM perspective)\n";
        std::cout << "Final Score:    " << final_score << "\n";
        std::cout << "==================================================================\n\n";
    }
}