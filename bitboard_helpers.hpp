#pragma once
#include "types.hpp"
#include <iostream>

// Prefers compiler intrinsics (single instruction) but falls back to C++20 <bit> or a manual loop if neither is available.
#if defined(__GNUC__) || defined(__clang__)
    #define HAS_BUILTIN_CTZ
    #define HAS_BUILTIN_POPCOUNT
#elif defined(_MSC_VER)
    #include <intrin.h>
    #define HAS_MSVC_BITSCAN
    #define HAS_MSVC_POPCOUNT
#elif defined(__cplusplus) && __cplusplus >= 202002L
    #include <bit>
    #define HAS_STD_BIT
#endif

namespace BitboardHelpers {

    // count trailing zeros (index of LSB)
    // PRECONDITION: bitboard != 0 (undefined behavior if zero)
    [[nodiscard]] inline int get_lsb(bb bitboard) noexcept {
    #if defined(HAS_BUILTIN_CTZ)
        return __builtin_ctzll(bitboard);
    #elif defined(HAS_MSVC_BITSCAN)
        unsigned long idx;
        _BitScanForward64(&idx, bitboard);
        return static_cast<int>(idx);
    #elif defined(HAS_STD_BIT)
        return std::countr_zero(bitboard);
    #else
        // manual fallback
        int count = 0;
        while (!(bitboard & 1ULL)) { bitboard >>= 1; count++; }
        return count;
    #endif
    }

    // remove the LSB and return its index
    // PRECONDITION: bitboard != 0
    [[nodiscard]] inline int pop_lsb(bb& bitboard) noexcept {
        const int lsb = get_lsb(bitboard);
        bitboard &= bitboard - 1;
        return lsb;
    }

    // check if a bit is set
    [[nodiscard]] constexpr bool bit_is_set(bb bitboard, Square sq) noexcept {
        return bitboard & (1ULL << to_idx(sq));
    }

    // set a bit
    constexpr void set_bit(bb& bitboard, Square sq) noexcept {
        bitboard |= (1ULL << to_idx(sq));
    }

    // clear a bit
    constexpr void pop_bit(bb& bitboard, Square sq) noexcept {
        bitboard &= ~(1ULL << to_idx(sq));
    }
    
    // population count
    [[nodiscard]] inline int count_bits(bb bitboard) noexcept {
    #if defined(HAS_BUILTIN_POPCOUNT)
        return __builtin_popcountll(bitboard);
    #elif defined(HAS_MSVC_POPCOUNT)
        return static_cast<int>(__popcnt64(bitboard));
    #elif defined(HAS_STD_BIT)
        return std::popcount(bitboard);
    #else
        // Kernighan's bit counting
        int count = 0;
        while (bitboard) {
            bitboard &= bitboard - 1; count++;
        }
        return count;
    #endif
    }
    
    void print_bitboard(bb bitboard);

    std::string sq_str(Square s);

    void print_move(Move m);
}