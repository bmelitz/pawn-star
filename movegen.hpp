#pragma once
#include "board.hpp"
#include "types.hpp"

namespace MoveGen {
    void init_tables();
    
    // leapers and pawns
    bb get_knight_attacks(Square sq);
    bb get_king_attacks(Square sq);
    bb get_pawn_attacks(Square sq, Color c);

    // sliding pieces (magic bitboards)
    // these return the squares a piece attacks given the blockers on the board.
    bb get_bishop_attacks(Square sq, bb occupancy);
    bb get_rook_attacks(Square sq, bb occupancy);
    bb get_queen_attacks(Square sq, bb occupancy);

    void generate_pseudo_moves(const Board& board, MoveList& list);

    // Captures, EP captures, and queen/knight promotions only.
    // No quiet moves, no castling. Used by quiescence search.
    void generate_captures(const Board& board, MoveList& list);

    bool is_square_attacked(const Board& board, Square sq, Color attacker);
}
