#include "perft.hpp"
#include "movegen.hpp"
#include "bitboard_helpers.hpp"
#include <chrono>
#include <cstdio>
#include <iostream>

namespace Perft {

    uint64_t perft(Board& board, int depth) {
        if (depth == 0) {
            return 1;
        }

        MoveList list;
        MoveGen::generate_pseudo_moves(board, list);
        Color us = board.get_side_to_move();
        uint64_t nodes = 0;

        for (int i = 0; i < list.count; ++i) {
            StateInfo st;
            board.make_move(list.moves[i], st);
            // pseudo-legal -> legal: skip moves that leave our own king attacked
            if (!MoveGen::is_square_attacked(board, board.get_king_square(us), board.get_side_to_move())) {
                nodes += perft(board, depth - 1);
            }
            board.unmake_move(list.moves[i]);
        }
        return nodes;
    }

    uint64_t divide(Board& board, int depth) {
        auto start = std::chrono::steady_clock::now();

        MoveList list;
        MoveGen::generate_pseudo_moves(board, list);
        Color us = board.get_side_to_move();
        uint64_t total = 0;

        for (int i = 0; i < list.count; ++i) {
            Move m = list.moves[i];
            StateInfo st;
            board.make_move(m, st);
            if (!MoveGen::is_square_attacked(board, board.get_king_square(us), board.get_side_to_move())) {
                uint64_t n = depth > 1 ? perft(board, depth - 1) : 1;
                BitboardHelpers::print_move(m);
                std::cout << ": " << n << "\n";
                total += n;
            }
            board.unmake_move(m);
        }

        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << "\nNodes: " << total << "\n";
        std::printf("Time:  %.3f s (%.1f Mnps)\n", secs, secs > 0 ? total / secs / 1e6 : 0.0);
        return total;
    }

    struct PerftCase {
        const char* name;
        const char* fen;
        int depth;
        uint64_t expected;
    };

    // The six standard positions from the Chess Programming Wiki "Perft Results"
    // page, plus position 4 mirrored (exercises Black's promotion/castling code),
    // plus edge cases for en passant, castling and promotion legality.
    const PerftCase SUITE[] = {
        {"startpos",          "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",               5, 4865609},
        {"kiwipete",          "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",   4, 4085603},
        {"cpw pos 3",         "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",                              5, 674624},
        {"cpw pos 4",         "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",       4, 422333},
        {"cpw pos 4 mirror",  "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",       4, 422333},
        {"cpw pos 5",         "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",              4, 2103487},
        {"cpw pos 6",         "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4, 3894594},
        {"illegal ep 1",      "3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1",                                      6, 1134888},
        {"illegal ep 2",      "8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1",                                     6, 1015133},
        {"ep gives check",    "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1",                                    6, 1440467},
        {"short castle check","5k2/8/8/8/8/8/8/4K2R w K - 0 1",                                         6, 661072},
        {"long castle check", "3k4/8/8/8/8/8/8/R3K3 w Q - 0 1",                                         6, 803711},
        {"castle rights",     "r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1",                              4, 1274206},
        {"castle prevented",  "r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1",                               4, 1720476},
        {"promo out of check","2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1",                                      6, 3821001},
        {"discovered check",  "8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 0 1",                                    5, 1004658},
        {"promo gives check", "4k3/1P6/8/8/8/8/K7/8 w - - 0 1",                                         6, 217342},
        {"underpromo check",  "8/P1k5/K7/8/8/8/8/8 w - - 0 1",                                          6, 92683},
        {"self stalemate",    "K1k5/8/P7/8/8/8/8/8 w - - 0 1",                                          6, 2217},
        {"stalemate/mate 1",  "8/k1P5/8/1K6/8/8/8/8 w - - 0 1",                                         7, 567584},
        {"stalemate/mate 2",  "8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1",                                      4, 23527},
    };

    bool run_suite() {
        int passed = 0, total = 0;
        uint64_t all_nodes = 0;
        auto start = std::chrono::steady_clock::now();

        for (const auto& c : SUITE) {
            Board board;
            board.set_fen(c.fen);
            uint64_t got = perft(board, c.depth);
            bool ok = (got == c.expected);
            passed += ok;
            total++;
            all_nodes += got;
            std::printf("%-4s %-20s d%d %12llu", ok ? "ok" : "FAIL", c.name, c.depth, (unsigned long long)got);
            if (!ok) {
                std::printf("  (expected %llu)", (unsigned long long)c.expected);
            }
            std::printf("\n");
        }

        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::printf("\n%d/%d passed, %llu nodes in %.2f s (%.1f Mnps)\n", passed, total,
                    (unsigned long long)all_nodes, secs, all_nodes / secs / 1e6);
        return passed == total;
    }
}
