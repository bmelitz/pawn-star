#pragma once
#include <cstdint>

using bb = uint64_t;

enum class Piece : int {
    PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
    NONE = 6
};

enum class Color : int {
    WHITE, BLACK,
    NONE = 2
};

enum class Square : int {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NONE = 64
};

// fast casting helpers
constexpr int to_idx(Piece p) noexcept {
    return static_cast<int>(p);
}
constexpr int to_idx(Color c) noexcept {
    return static_cast<int>(c);
}
constexpr int to_idx(Square s) noexcept {
    return static_cast<int>(s);
}

constexpr Piece to_piece(int i) noexcept {
    return static_cast<Piece>(i);
}

constexpr Color to_color(int i) noexcept {
    return static_cast<Color>(i);
}

constexpr Square to_square(int i) noexcept {
    return static_cast<Square>(i);
}

// 16-bit Move: [flags 4 bits] [from 6 bits] [to 6 bits]
struct Move {
    uint16_t data = 0;

    enum Flag : uint16_t {
        QUIET = 0,
        DOUBLE_PAWN_PUSH = 1,
        KING_CASTLE = 2,
        QUEEN_CASTLE = 3,
        CAPTURE = 4,
        EP_CAPTURE = 5,
        PROMO_N = 8,
        PROMO_B = 9,
        PROMO_R = 10,
        PROMO_Q = 11,
        PROMO_CAPTURE_N = 12,
        PROMO_CAPTURE_B = 13,
        PROMO_CAPTURE_R = 14,
        PROMO_CAPTURE_Q = 15
    };

    constexpr Move() = default;
    constexpr Move(Square from, Square to, Flag flag = QUIET) {
        data = (static_cast<uint16_t>(flag) << 12) | 
               (static_cast<uint16_t>(from) << 6) | 
               static_cast<uint16_t>(to);
    } 

    constexpr Square to_sq() const {
        return to_square(data & 0x3F);
    }
    constexpr Square from_sq() const {
        return to_square((data >> 6) & 0x3F);
    }
    constexpr int flag() const {
        return (data >> 12) & 0xF;
    }

    // checks if the 4 bit of the flag is set
    constexpr bool is_capture() const {
        return data & (0x4000);
    }
    // checks if the 8 bit of the flag is set
    constexpr bool is_promo() const {
        return data & (0x8000);
    }
};

// list to hold generated moves
struct MoveList {
    Move moves[256];
    int count = 0;

    void add(Move m) {
        moves[count++] = m;
    }
    Move operator[](int index) const {
        return moves[index];
    }
};

// flip color
constexpr Color operator~(Color c) noexcept {
    return (c == Color::WHITE) ? Color::BLACK : Color::WHITE;
}

// combine color and piece into a unique index (0-11)
constexpr int piece_index(Color c, Piece p) noexcept {
    return (to_idx(c) * 6) + to_idx(p);
}

constexpr bool is_valid(Square s) noexcept {
    return to_idx(s) >= 0 && to_idx(s) < 64;
}
