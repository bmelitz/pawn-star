#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <array>

// stores the irreversible state of the game
struct StateInfo {
    uint8_t castling_rights = 0; // 1=wk, 2=wq, 4=bk, 8=bq
    Square en_passant_square = Square::NONE;
    int half_move_clock = 0;
    int full_move_number = 1;
    Piece captured_piece = Piece::NONE;
    Square king_square[2] = {Square::NONE, Square::NONE};
    StateInfo* previous = nullptr;
    uint64_t key = 0; // zobrist hash
};

class Board {
public:
    Board();
    ~Board();

    void set_fen(const std::string& fen);

    [[nodiscard]] std::string get_fen() const;

    // accessors
    [[nodiscard]] constexpr bb get_pieces(Color c, Piece p) const noexcept { 
        return pieces_[to_idx(c)][to_idx(p)]; 
    }
    [[nodiscard]] constexpr bb get_occupancy(Color c) const noexcept { 
        return occupancies_[to_idx(c)]; 
    }
    [[nodiscard]] constexpr bb get_all_occupancy() const noexcept { 
        return occupancies_[2];
    }
    [[nodiscard]] constexpr Color get_side_to_move() const noexcept { 
        return side_to_move_; 
    }
    [[nodiscard]] constexpr Square get_en_passant_square() const noexcept {
        return state_->en_passant_square;
    }
    [[nodiscard]] constexpr uint8_t get_castling_rights() const noexcept {
        return state_->castling_rights;
    }

    [[nodiscard]] constexpr uint64_t get_hash() const noexcept { 
        return state_->key; 
    }
    
    // returns the encoded piece (color * 6 + piece) or -1 if empty
    [[nodiscard]] constexpr int get_full_piece(Square s) const noexcept { 
        return mailbox_[to_idx(s)]; 
    }

    [[nodiscard]] constexpr Square get_king_square(Color c) const noexcept {
        return state_->king_square[to_idx(c)];
    }

    [[nodiscard]] constexpr int get_half_move_clock() const noexcept {
        return state_->half_move_clock;
    }

    [[nodiscard]] bool is_repetition() const noexcept;
    [[nodiscard]] bool is_insufficient_material() const noexcept;

    void print() const;

    void make_move(Move m, StateInfo& new_state);
    void unmake_move(Move m);

private:
    void clear() noexcept;
    // put_fen_piece and remove_fen_piece update pieces_ and mailbox_ but not occuapncies_
    // Caller is responsible for updating occupanies (with update_occupancies)
    void put_fen_piece(Piece p, Color c, Square s) noexcept;
    void remove_fen_piece(Square s) noexcept;
    void update_occupancies() noexcept;

private:
    bb pieces_[2][6];
    bb occupancies_[3];
    int8_t mailbox_[64];
    Color side_to_move_;
    StateInfo root_state_;  // owned root state (no heap allocation needed)
    StateInfo* state_ = nullptr;
};
