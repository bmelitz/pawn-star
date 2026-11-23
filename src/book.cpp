#include "book.hpp"
#include "uci.hpp"
#include "bitboard_helpers.hpp"
#include <deque>
#include <iostream>
#include <random>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace Book {

    namespace {
        // Main lines in UCI notation, all from the standard starting position.
        // A first move appears as often as lines start with it, which sets how
        // often it's played (e4 and d4 most, then c4 and Nf3).
        const char* LINES[] = {
            // 1.e4 e5
            "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 f1e1 b7b5 a4b3 d7d6 c2c3 e8g8", // Ruy Lopez, Closed
            "e2e4 e7e5 g1f3 b8c6 f1b5 g8f6 e1g1 f6e4 d2d4 e4d6 b5c6 d7c6 d4e5 d6f5 d1d8 e8d8", // Ruy Lopez, Berlin
            "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5c6 d7c6 e1g1 f7f6",                                // Ruy Lopez, Exchange
            "e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 c2c3 g8f6 d2d3 d7d6 e1g1 e8g8",                      // Italian, Giuoco Pianissimo
            "e2e4 e7e5 g1f3 b8c6 f1c4 g8f6 d2d3 f8e7 e1g1 e8g8",                                // Two Knights, d3
            "e2e4 e7e5 g1f3 b8c6 d2d4 e5d4 f3d4 g8f6 d4c6 b7c6 e4e5 d8e7 d1e2 f6d5",            // Scotch, Mieses
            "e2e4 e7e5 g1f3 g8f6 f3e5 d7d6 e5f3 f6e4 d2d4 d6d5 f1d3",                           // Petrov
            // Sicilian
            "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6 c1e3 e7e5 d4b3 c8e6",            // Najdorf, English Attack
            "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6 c1g5 e7e6 f2f4",                 // Najdorf, 6.Bg5
            "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 g7g6 c1e3 f8g7 f2f3 e8g8 d1d2 b8c6",  // Dragon, Yugoslav Attack
            "e2e4 c7c5 g1f3 b8c6 d2d4 c5d4 f3d4 g8f6 b1c3 e7e5 d4b5 d7d6 c1g5 a7a6 b5a3 b7b5",  // Sveshnikov
            "e2e4 c7c5 g1f3 e7e6 d2d4 c5d4 f3d4 b8c6 b1c3 d8c7 c1e3 a7a6",                      // Taimanov
            "e2e4 c7c5 c2c3 g8f6 e4e5 f6d5 d2d4 c5d4 g1f3 b8c6",                                // Alapin
            "e2e4 c7c5 g1f3 b8c6 f1b5 g7g6 e1g1 f8g7 f1e1 e7e5",                                // Rossolimo
            // French, Caro-Kann, others
            "e2e4 e7e6 d2d4 d7d5 b1c3 f8b4 e4e5 c7c5 a2a3 b4c3 b2c3 g8e7",                      // French, Winawer
            "e2e4 e7e6 d2d4 d7d5 e4e5 c7c5 c2c3 b8c6 g1f3 d8b6",                                // French, Advance
            "e2e4 e7e6 d2d4 d7d5 b1d2 g8f6 e4e5 f6d7 f1d3 c7c5 c2c3 b8c6",                      // French, Tarrasch
            "e2e4 c7c6 d2d4 d7d5 b1c3 d5e4 c3e4 c8f5 e4g3 f5g6 h2h4 h7h6 g1f3 b8d7",            // Caro-Kann, Classical
            "e2e4 c7c6 d2d4 d7d5 e4e5 c8f5 g1f3 e7e6 f1e2 c6c5",                                // Caro-Kann, Advance
            "e2e4 d7d6 d2d4 g8f6 b1c3 g7g6 c1e3 f8g7 d1d2",                                     // Pirc
            "e2e4 d7d5 e4d5 d8d5 b1c3 d5a5 d2d4 g8f6 g1f3 c8f5",                                // Scandinavian
            // 1.d4
            "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c1g5 f8e7 e2e3 e8g8 g1f3 h7h6 g5h4 b7b6",            // QGD, Tartakower
            "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c4d5 e6d5 c1g5 c7c6 e2e3 f8e7 f1d3",                 // QGD, Exchange
            "d2d4 d7d5 c2c4 d5c4 g1f3 g8f6 e2e3 e7e6 f1c4 c7c5 e1g1 a7a6",                      // Queen's Gambit Accepted
            "d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 d5c4 a2a4 c8f5 e2e3 e7e6 f1c4",                 // Slav
            "d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 e7e6 e2e3 b8d7 f1d3 d5c4 d3c4 b7b5",            // Semi-Slav, Meran
            "d2d4 g8f6 c2c4 e7e6 b1c3 f8b4 e2e3 e8g8 f1d3 d7d5 g1f3 c7c5 e1g1",                 // Nimzo-Indian, Rubinstein
            "d2d4 g8f6 c2c4 e7e6 b1c3 f8b4 d1c2 e8g8 a2a3 b4c3 c2c3 b7b6",                      // Nimzo-Indian, Classical
            "d2d4 g8f6 c2c4 e7e6 g1f3 b7b6 g2g3 c8a6 b2b3 f8b4 c1d2 b4e7",                      // Queen's Indian
            "d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6 g1f3 e8g8 f1e2 e7e5 e1g1 b8c6 d4d5 c6e7",  // King's Indian, Classical
            "d2d4 g8f6 c2c4 g7g6 b1c3 d7d5 c4d5 f6d5 e2e4 d5c3 b2c3 f8g7 g1f3 c7c5",            // Grunfeld, Exchange
            "d2d4 g8f6 c2c4 c7c5 d4d5 e7e6 b1c3 e6d5 c4d5 d7d6 e2e4 g7g6 g1f3 f8g7",            // Modern Benoni
            "d2d4 f7f5 g2g3 g8f6 f1g2 g7g6 g1f3 f8g7 e1g1 e8g8 c2c4 d7d6",                      // Dutch, Leningrad
            "d2d4 d7d5 g1f3 g8f6 c1f4 e7e6 e2e3 c7c5 c2c3 b8c6 b1d2 f8d6",                      // London System
            "d2d4 g8f6 c2c4 e7e6 g2g3 d7d5 f1g2 f8e7 g1f3 e8g8 e1g1 d5c4 d1c2 a7a6",            // Catalan
            // Flank openings
            "c2c4 c7c5 g1f3 g8f6 b1c3 b8c6 g2g3 g7g6 f1g2 f8g7 e1g1 e8g8",                      // English, Symmetrical
            "c2c4 e7e5 b1c3 g8f6 g2g3 d7d5 c4d5 f6d5 f1g2 d5b6 g1f3 b8c6",                      // English, Reversed Sicilian
            "g1f3 d7d5 g2g3 g8f6 f1g2 e7e6 e1g1 f8e7 d2d3 e8g8",                                // Reti / King's Indian Attack
        };

        const char* STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        // position hash -> book moves from it (repeated once per line through it, for weighting)
        std::unordered_map<uint64_t, std::vector<Move>> table;
        std::mt19937 rng{std::random_device{}()};
    }

    bool init() {
        table.clear();
        bool ok = true;
        int line_number = 0;
        for (const char* line : LINES) {
            line_number++;
            Board board;
            board.set_fen(STARTPOS);
            std::deque<StateInfo> states; // deque: StateInfo addresses must stay put
            std::istringstream in(line);
            std::string token;
            while (in >> token) {
                Move m = UCI::parse_move(board, token);
                if (m.data == 0) {
                    std::cerr << "book: illegal move " << token << " in line " << line_number << "\n";
                    ok = false;
                    break;
                }
                table[board.get_hash()].push_back(m);
                states.emplace_back();
                board.make_move(m, states.back());
            }
        }
        return ok;
    }

    Move probe(Board& board) {
        auto it = table.find(board.get_hash());
        if (it == table.end()) {
            return Move();
        }
        const std::vector<Move>& moves = it->second;
        Move m = moves[std::uniform_int_distribution<size_t>(0, moves.size() - 1)(rng)];
        // guard against a hash collision with a position outside the book
        return UCI::parse_move(board, BitboardHelpers::move_str(m)).data != 0 ? m : Move();
    }
}
