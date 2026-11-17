#include "bench.hpp"
#include "board.hpp"
#include "search.hpp"
#include "tt.hpp"
#include <chrono>
#include <iostream>

namespace Bench {

    // a mix of openings, middlegames and endgames
    const char* POSITIONS[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "3r1r2/1p3qbk/1p1p2pp/1P1Ppn2/R3N1BP/1Q1P2P1/5P2/2R3K1 w - - 0 1",
        "r3k2r/2pb1ppp/2pp1q2/p7/1nP1B3/1P2P3/P2N1PPP/R2QK2R w KQkq a6 0 14",
        "4rrk1/2p1b1p1/p1p3q1/4p3/2P2n1p/1P1NR2P/PB3PP1/3R1QK1 b - - 2 24",
        "r3qbrk/6p1/2b2pPp/p3pP1Q/PpPpP2P/3P1B2/2PB3K/R5R1 w - - 16 42",
        "7r/2p3k1/1p1p1qp1/1P1Bp3/p1P2r1P/P7/4R3/Q4RK1 w - - 0 36",
        "6k1/1R3p2/6p1/2Bp3p/3P2q1/P7/1P2rQ1K/5R2 b - - 4 44",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "8/8/1p2k1p1/3p3p/1p1P1P1P/1P2PK2/8/8 w - - 3 54",
    };

    uint64_t run(int depth) {
        uint64_t total_nodes = 0;
        auto start = std::chrono::steady_clock::now();

        for (const char* fen : POSITIONS) {
            // each position starts from the same state -> deterministic
            TT::clear();
            Search::clear_heuristics();
            Board board;
            board.set_fen(fen);

            Search::Limits limits;
            limits.depth = depth;
            limits.print_info = false;
            Search::stop = false;
            Search::think(board, limits);
            total_nodes += Search::nodes_searched;
        }

        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        uint64_t nps = static_cast<uint64_t>(total_nodes / (secs > 0 ? secs : 1e-9));
        std::cout << total_nodes << " nodes " << nps << " nps" << std::endl;
        return total_nodes;
    }
}
