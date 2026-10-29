#include "board.hpp"
#include "bitboard_helpers.hpp"
#include "zobrist.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <vector>
#include <stdexcept>

// --- Helper Implementation ---
namespace BitboardHelpers {
    void print_bitboard(bb bitboard) {
        std::cout << "\n";
        for (int rank = 7; rank >= 0; --rank) {
            std::cout << rank + 1 << "  ";
            for (int file = 0; file < 8; ++file) {
                int sq = rank * 8 + file;
                std::cout << (bit_is_set(bitboard, to_square(sq)) ? "1 " : ". ");
            }
            std::cout << "\n";
        }
        std::cout << "\n   a b c d e f g h\n\n";
        std::cout << "    Bitboard: " << bitboard << "\n\n";
    }

    std::string sq_str(Square s) {
        if (s == Square::NONE) return "-";
        int idx = to_idx(s);
        char f = 'a' + (idx % 8);
        char r = '1' + (idx / 8);
        return std::string{f, r};
    }

    void print_move(Move m) {
        if (m.data == 0) {
            std::cout << "0000";
            return;
        }
        std::cout << sq_str(m.from_sq()) << sq_str(m.to_sq());
        if (m.is_promo()) {
            int flag = m.flag() & 3;
            if (flag == 0) std::cout << "n";
            else if (flag == 1) std::cout << "b";
            else if (flag == 2) std::cout << "r";
            else std::cout << "q";
        }
    }
}

// --- Board Implementation ---

// Map chars to Piece enum
constexpr int char_to_piece_index(char c) {
    switch (c) {
        case 'P': return 0; case 'N': return 1; case 'B': return 2; case 'R': return 3; case 'Q': return 4; case 'K': return 5;
        case 'p': return 6; case 'n': return 7; case 'b': return 8; case 'r': return 9; case 'q': return 10; case 'k': return 11;
        default: return -1;
    }
}

// Map Piece indices back to chars (for get_fen)
constexpr char piece_index_to_char(int idx) {
    constexpr char chars[] = "PNBRQKpnbrqk";
    if (idx >= 0 && idx < 12) return chars[idx];
    return '?';
}

Board::Board() {
    state_ = &root_state_;
    clear();
}

Board::~Board() {
    // root_state_ is a member, nothing to free
}

void Board::clear() noexcept {
    std::memset(pieces_, 0, sizeof(pieces_));
    std::memset(occupancies_, 0, sizeof(occupancies_));
    std::memset(mailbox_, -1, sizeof(mailbox_)); // mailbox_ is an array of uint8_t - this is fine
    
    side_to_move_ = Color::WHITE;
    state_ = &root_state_;
    *state_ = StateInfo{};
}

void Board::put_fen_piece(Piece p, Color c, Square s) noexcept {
    BitboardHelpers::set_bit(pieces_[to_idx(c)][to_idx(p)], s);
    mailbox_[to_idx(s)] = piece_index(c, p);
}

void Board::remove_fen_piece(Square s) noexcept {
    int piece_idx = mailbox_[to_idx(s)];
    if (piece_idx != -1) {
        Color c = (piece_idx < 6) ? Color::WHITE : Color::BLACK;
        Piece p = to_piece(piece_idx % 6);
        BitboardHelpers::pop_bit(pieces_[to_idx(c)][to_idx(p)], s);
        mailbox_[to_idx(s)] = -1;
    }
}

void Board::update_occupancies() noexcept {
    occupancies_[0] = 0;
    occupancies_[1] = 0;
    occupancies_[2] = 0;

    for (int p = 0; p < 6; ++p) {
        occupancies_[0] |= pieces_[0][p];
        occupancies_[1] |= pieces_[1][p];
    }
    occupancies_[2] = occupancies_[0] | occupancies_[1];
}

void Board::make_move(Move m, StateInfo& new_state) {
    // 1. Initialize new state and grab the current hash
    new_state = *state_;
    new_state.previous = state_;
    uint64_t k = state_->key; // Start with the hash of the current board

    // 2. XOR OUT the current "Irreversible" state
    k ^= Zobrist::castle_keys[state_->castling_rights];
    if (state_->en_passant_square != Square::NONE) {
        k ^= Zobrist::ep_keys[to_idx(state_->en_passant_square)];
    }
    k ^= Zobrist::side_key;

    state_ = &new_state;

    state_->half_move_clock++;
    if (side_to_move_ == Color::BLACK) {
        state_->full_move_number++;
    }

    Square from = m.from_sq();
    Square to = m.to_sq();
    int flag = m.flag();
    bool is_capture = m.is_capture();
    
    int moved_piece_idx = mailbox_[to_idx(from)];
    Piece moved_piece = to_piece(moved_piece_idx % 6);
    Color us = side_to_move_;
    Color them = ~us;

    // XOR OUT the moving piece from its 'from' square
    k ^= Zobrist::piece_keys[to_idx(us)][to_idx(moved_piece)][to_idx(from)];

    if (moved_piece == Piece::KING) {
        state_->king_square[to_idx(us)] = to;
    }

    // Handle captures
    if (is_capture) {
        Square cap_sq = to;
        if (flag == Move::EP_CAPTURE) {
            cap_sq = (us == Color::WHITE) ? to_square(to_idx(to) - 8) : to_square(to_idx(to) + 8);
        }

        int cap_piece_idx = mailbox_[to_idx(cap_sq)];
        Piece cap_piece = to_piece(cap_piece_idx % 6);
        
        // XOR OUT the captured piece from its square
        k ^= Zobrist::piece_keys[to_idx(them)][to_idx(cap_piece)][to_idx(cap_sq)];

        state_->captured_piece = cap_piece;
        BitboardHelpers::pop_bit(pieces_[to_idx(them)][to_idx(cap_piece)], cap_sq);
        mailbox_[to_idx(cap_sq)] = -1;
        state_->half_move_clock = 0;
        
        // Update castling rights if rook captured
        if (cap_piece == Piece::ROOK) {
            if (cap_sq == Square::A1) {
                state_->castling_rights &= ~2;
            }
            else if (cap_sq == Square::H1) {
                state_->castling_rights &= ~1;
            }
            else if (cap_sq == Square::A8) {
                state_->castling_rights &= ~8;
            }
            else if (cap_sq == Square::H8) {
                state_->castling_rights &= ~4;
            }
        }
    } else {
        state_->captured_piece = Piece::NONE;
    }

    // Move the piece logic
    BitboardHelpers::pop_bit(pieces_[to_idx(us)][to_idx(moved_piece)], from);
    BitboardHelpers::set_bit(pieces_[to_idx(us)][to_idx(moved_piece)], to);
    mailbox_[to_idx(from)] = -1;
    mailbox_[to_idx(to)] = moved_piece_idx;

    // Pawn logic
    if (moved_piece == Piece::PAWN) {
        state_->half_move_clock = 0;
        
        if (flag == Move::DOUBLE_PAWN_PUSH) {
            state_->en_passant_square = (us == Color::WHITE) ? to_square(to_idx(to) - 8) : to_square(to_idx(to) + 8);
        } else {
            state_->en_passant_square = Square::NONE;
        }

        if (m.is_promo()) {
            Piece promo_piece = Piece::QUEEN;
            if ((flag & 3) == 0) {
                promo_piece = Piece::KNIGHT;
            }
            else if ((flag & 3) == 1) {
                promo_piece = Piece::BISHOP;
            }
            else if ((flag & 3) == 2) {
                promo_piece = Piece::ROOK;
            }
            
            // XOR IN the promoted piece at 'to' (pawn was already XOR'd out from 'from' above)
            k ^= Zobrist::piece_keys[to_idx(us)][to_idx(promo_piece)][to_idx(to)];
            BitboardHelpers::pop_bit(pieces_[to_idx(us)][to_idx(Piece::PAWN)], to);
            BitboardHelpers::set_bit(pieces_[to_idx(us)][to_idx(promo_piece)], to);
            mailbox_[to_idx(to)] = piece_index(us, promo_piece);
        } else {
            // Normal pawn move (non-promo): XOR the pawn into 'to'
            k ^= Zobrist::piece_keys[to_idx(us)][to_idx(Piece::PAWN)][to_idx(to)];
        }
    } else {
        state_->en_passant_square = Square::NONE;
        // Non-pawn move: XOR the moved piece into 'to'
        k ^= Zobrist::piece_keys[to_idx(us)][to_idx(moved_piece)][to_idx(to)];
    }

    // Castling logic
    if (moved_piece == Piece::KING) {
        if (us == Color::WHITE) {
            state_->castling_rights &= ~3;
        }
        else {
            state_->castling_rights &= ~12;
        }
        
        if (flag == Move::KING_CASTLE) {
            Square r_from = (us == Color::WHITE) ? Square::H1 : Square::H8;
            Square r_to = (us == Color::WHITE) ? Square::F1 : Square::F8;
            
            // XOR OUT rook from corner, XOR IN rook to new square
            k ^= Zobrist::piece_keys[to_idx(us)][to_idx(Piece::ROOK)][to_idx(r_from)];
            k ^= Zobrist::piece_keys[to_idx(us)][to_idx(Piece::ROOK)][to_idx(r_to)];

            BitboardHelpers::pop_bit(pieces_[to_idx(us)][to_idx(Piece::ROOK)], r_from);
            BitboardHelpers::set_bit(pieces_[to_idx(us)][to_idx(Piece::ROOK)], r_to);
            mailbox_[to_idx(r_from)] = -1;
            mailbox_[to_idx(r_to)] = piece_index(us, Piece::ROOK);
        }
        else if (flag == Move::QUEEN_CASTLE) {
            Square r_from = (us == Color::WHITE) ? Square::A1 : Square::A8;
            Square r_to = (us == Color::WHITE) ? Square::D1 : Square::D8;

            // XOR OUT rook from corner, XOR IN rook to new square
            k ^= Zobrist::piece_keys[to_idx(us)][to_idx(Piece::ROOK)][to_idx(r_from)];
            k ^= Zobrist::piece_keys[to_idx(us)][to_idx(Piece::ROOK)][to_idx(r_to)];

            BitboardHelpers::pop_bit(pieces_[to_idx(us)][to_idx(Piece::ROOK)], r_from);
            BitboardHelpers::set_bit(pieces_[to_idx(us)][to_idx(Piece::ROOK)], r_to);
            mailbox_[to_idx(r_from)] = -1;
            mailbox_[to_idx(r_to)] = piece_index(us, Piece::ROOK);
        }
    }

    // Update rights if rook moves
    if (moved_piece == Piece::ROOK) {
        if (from == Square::A1) {
            state_->castling_rights &= ~2;
        }
        else if (from == Square::H1) {
            state_->castling_rights &= ~1;
        }
        else if (from == Square::A8) {
            state_->castling_rights &= ~8;
        }
        else if (from == Square::H8) {
            state_->castling_rights &= ~4;
        }
    }

    // 4. XOR IN the NEW "Irreversible" state
    k ^= Zobrist::castle_keys[state_->castling_rights];
    if (state_->en_passant_square != Square::NONE) {
        k ^= Zobrist::ep_keys[to_idx(state_->en_passant_square)];
    }

    // 5. Finalize the key
    state_->key = k;

    update_occupancies();
    side_to_move_ = them;
}

void Board::unmake_move(Move m) {
    side_to_move_ = ~side_to_move_;
    Color us = side_to_move_;
    Color them = ~us;
    
    Square from = m.from_sq();
    Square to = m.to_sq();
    int flag = m.flag();
    
    // 1. Restore Piece at 'to' -> 'from'
    int moved_piece_idx = mailbox_[to_idx(to)];
    Piece moved_piece = to_piece(moved_piece_idx % 6);
    
    if (m.is_promo()) {
        BitboardHelpers::pop_bit(pieces_[to_idx(us)][to_idx(moved_piece)], to);
        mailbox_[to_idx(to)] = -1;
        
        moved_piece = Piece::PAWN;
        BitboardHelpers::set_bit(pieces_[to_idx(us)][to_idx(Piece::PAWN)], from);
        mailbox_[to_idx(from)] = piece_index(us, Piece::PAWN);
    } else {
        BitboardHelpers::pop_bit(pieces_[to_idx(us)][to_idx(moved_piece)], to);
        BitboardHelpers::set_bit(pieces_[to_idx(us)][to_idx(moved_piece)], from);
        mailbox_[to_idx(to)] = -1;
        mailbox_[to_idx(from)] = moved_piece_idx;
    }
    
    // 2. Restore Captured Piece
    if (m.is_capture()) {
        Square cap_sq = to;
        if (flag == Move::EP_CAPTURE) {
            cap_sq = (us == Color::WHITE) ? to_square(to_idx(to) - 8) : to_square(to_idx(to) + 8);
        }
        
        // Restore from state BEFORE we step back
        Piece cap_piece = state_->captured_piece;
        
        BitboardHelpers::set_bit(pieces_[to_idx(them)][to_idx(cap_piece)], cap_sq);
        mailbox_[to_idx(cap_sq)] = piece_index(them, cap_piece);
    }
    
    // 3. Unmake Castling (Move Rook Back)
    if (flag == Move::KING_CASTLE) {
        Square r_from = (us == Color::WHITE) ? Square::H1 : Square::H8;
        Square r_to   = (us == Color::WHITE) ? Square::F1 : Square::F8;
        BitboardHelpers::pop_bit(pieces_[to_idx(us)][to_idx(Piece::ROOK)], r_to);
        BitboardHelpers::set_bit(pieces_[to_idx(us)][to_idx(Piece::ROOK)], r_from);
        mailbox_[to_idx(r_to)] = -1;
        mailbox_[to_idx(r_from)] = piece_index(us, Piece::ROOK);
    } 
    else if (flag == Move::QUEEN_CASTLE) {
        Square r_from = (us == Color::WHITE) ? Square::A1 : Square::A8;
        Square r_to   = (us == Color::WHITE) ? Square::D1 : Square::D8;
        BitboardHelpers::pop_bit(pieces_[to_idx(us)][to_idx(Piece::ROOK)], r_to);
        BitboardHelpers::set_bit(pieces_[to_idx(us)][to_idx(Piece::ROOK)], r_from);
        mailbox_[to_idx(r_to)] = -1;
        mailbox_[to_idx(r_from)] = piece_index(us, Piece::ROOK);
    }
    
    // 4. Step Back State
    // The previous state is guaranteed to exist if we called make_move
    state_ = state_->previous;
    
    update_occupancies();
}

bool Board::is_repetition() const noexcept {
    int count = 0;
    StateInfo* st = state_->previous;
    // only need to look back as far as the halfmove clock allows —
    // a capture or pawn move makes prior positions unreachable
    int half_moves = state_->half_move_clock;

    for (int i = 0; i < half_moves && st != nullptr; i++) {
        if (st->key == state_->key) {
            count++;
            if (count >= 2) {
                return true; // current + 2 prior = 3-fold
            }
        }
        st = st->previous;
    }
    return false;
}

bool Board::is_insufficient_material() const noexcept {
    // any pawn, rook, or queen means sufficient material
    for (int c = 0; c < 2; c++) {
        if (pieces_[c][to_idx(Piece::PAWN)]) {
            return false;
        }
        if (pieces_[c][to_idx(Piece::ROOK)]) {
            return false;
        }
        if (pieces_[c][to_idx(Piece::QUEEN)]) {
            return false;
        }
    }

    int wN = BitboardHelpers::count_bits(pieces_[0][to_idx(Piece::KNIGHT)]);
    int bN = BitboardHelpers::count_bits(pieces_[1][to_idx(Piece::KNIGHT)]);
    int wB = BitboardHelpers::count_bits(pieces_[0][to_idx(Piece::BISHOP)]);
    int bB = BitboardHelpers::count_bits(pieces_[1][to_idx(Piece::BISHOP)]);

    // KvK
    if (wN + bN + wB + bB == 0) {
        return true;
    }

    // KNvK or KvKN
    if (wB == 0 && bB == 0 && wN + bN == 1) {
        return true;
    }

    // KBvK or KvKB
    if (wN == 0 && bN == 0) {
        if (wB + bB == 1) {
            return true;
        }

        // KBvKB — draw only if bishops are on the same color square
        if (wB == 1 && bB == 1) {
            int wSq = BitboardHelpers::get_lsb(pieces_[0][to_idx(Piece::BISHOP)]);
            int bSq = BitboardHelpers::get_lsb(pieces_[1][to_idx(Piece::BISHOP)]);
            // square color: (rank + file) % 2, where rank = sq/8, file = sq%8
            int wColor = (wSq / 8 + wSq % 8) % 2;
            int bColor = (bSq / 8 + bSq % 8) % 2;
            if (wColor == bColor) {
                return true;
            }
        }
    }

    return false;
}

void Board::set_fen(const std::string& fen) {
    clear();
    std::istringstream ss(fen);
    std::string placement, turn, castling, en_passant, half_move, full_move;

    ss >> placement >> turn >> castling >> en_passant >> half_move >> full_move;

    int rank = 7;
    int file = 0;

    int white_king_count = 0;
    int black_king_count = 0;
    
    for (char c : placement) {
        if (c == '/') {
            rank--;
            file = 0;
        } else if (isdigit(c)) {
            file += (c - '0');
        } else {
            int piece_idx = char_to_piece_index(c);
            if (piece_idx != -1) {
                Color col = (piece_idx < 6) ? Color::WHITE : Color::BLACK;
                Piece p = to_piece(piece_idx % 6);

                if (p == Piece::KING) {
                    if (col == Color::WHITE) {
                        white_king_count++;
                    } else {
                        black_king_count++;
                    }
                }

                Square sq = to_square(rank * 8 + file);
                put_fen_piece(p, col, sq);
                file++;
            }
        }
    }

    if (white_king_count != 1 || black_king_count != 1) {
        throw std::invalid_argument("Invalid FEN: position must contain exactly one white king and one black king");
    }

    update_occupancies();

    side_to_move_ = (turn == "w") ? Color::WHITE : Color::BLACK;

    state_->castling_rights = 0;
    if (castling != "-") {
        for (char c : castling) {
            if (c == 'K') state_->castling_rights |= 1;
            else if (c == 'Q') state_->castling_rights |= 2;
            else if (c == 'k') state_->castling_rights |= 4;
            else if (c == 'q') state_->castling_rights |= 8;
        }
    }

    if (en_passant != "-" && en_passant.length() >= 2) {
        int f = en_passant[0] - 'a';
        int r = en_passant[1] - '1';
        if (f >= 0 && f < 8 && r >= 0 && r < 8)
            state_->en_passant_square = to_square(r * 8 + f);
    } else {
        state_->en_passant_square = Square::NONE;
    }

    try {
        state_->half_move_clock = half_move.empty() ? 0 : std::stoi(half_move);
        state_->full_move_number = full_move.empty() ? 1 : std::stoi(full_move);
    } catch (...) {
        state_->half_move_clock = 0;
        state_->full_move_number = 1;
    }

    state_->king_square[to_idx(Color::WHITE)] = to_square(BitboardHelpers::get_lsb(pieces_[to_idx(Color::WHITE)][to_idx(Piece::KING)]));
    state_->king_square[to_idx(Color::BLACK)] = to_square(BitboardHelpers::get_lsb(pieces_[to_idx(Color::BLACK)][to_idx(Piece::KING)]));
    state_->captured_piece = Piece::NONE;
    state_->previous = nullptr;
    state_->key = Zobrist::compute_hash(*this);
}

// Returns the FEN string of the current position
std::string Board::get_fen() const {
    std::stringstream ss;
    
    // 1. Piece placement
    for (int r = 7; r >= 0; --r) {
        int empty_count = 0;
        for (int f = 0; f < 8; ++f) {
            int sq = r * 8 + f;
            int piece = mailbox_[sq];
            if (piece == -1) {
                empty_count++;
            } else {
                if (empty_count > 0) {
                    ss << empty_count;
                    empty_count = 0;
                }
                ss << piece_index_to_char(piece);
            }
        }
        if (empty_count > 0) ss << empty_count;
        if (r > 0) ss << "/";
    }
    
    // 2. Side to move
    ss << " " << (side_to_move_ == Color::WHITE ? "w" : "b");
    
    // 3. Castling
    ss << " ";
    std::string castling = "";
    if (state_->castling_rights & 1) castling += "K";
    if (state_->castling_rights & 2) castling += "Q";
    if (state_->castling_rights & 4) castling += "k";
    if (state_->castling_rights & 8) castling += "q";
    ss << (castling.empty() ? "-" : castling);
    
    // 4. En Passant
    ss << " ";
    if (state_->en_passant_square != Square::NONE) { 
        int sq = to_idx(state_->en_passant_square);
        char file = 'a' + (sq % 8);
        char rank = '1' + (sq / 8);
        ss << file << rank;
    } else {
        ss << "-";
    }
    
    // 5. Clocks
    ss << " " << state_->half_move_clock << " " << state_->full_move_number;
    
    return ss.str();
}

void Board::print() const {
    const char* piece_chars = "PNBRQKpnbrqk";
    
    std::cout << "\n  +---+---+---+---+---+---+---+---+\n";
    for (int r = 7; r >= 0; --r) {
        std::cout << (r + 1) << " |";
        for (int f = 0; f < 8; ++f) {
            int sq = r * 8 + f;
            int piece_idx = mailbox_[sq];
            char c = (piece_idx == -1) ? ' ' : piece_chars[piece_idx];
            std::cout << " " << c << " |";
        }
        std::cout << "\n  +---+---+---+---+---+---+---+---+\n";
    }
    std::cout << "    a   b   c   d   e   f   g   h\n\n";
    
    std::cout << "Side: " << (side_to_move_ == Color::WHITE ? "White" : "Black") << "\n";
    std::cout << "FEN:  " << get_fen() << "\n";
}
