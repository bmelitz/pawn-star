#include "board.hpp"
#include "movegen.hpp"
#include "search.hpp"
#include "perft.hpp"
#include "zobrist.hpp"
#include "tt.hpp"
#include "bitboard_helpers.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static const char* STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

static void usage() {
    std::cout <<
        "usage:\n"
        "  pawnstar perft <depth> [fen]    per-move node counts (divide)\n"
        "  pawnstar perft-suite            run the perft test suite (exit code 1 on failure)\n"
        "  pawnstar search <depth> [fen]   iterative-deepening search to <depth>\n"
        "  pawnstar gen-magics             re-derive the magic numbers in magics.hpp\n";
}

// joins args[i..] back into one FEN string (the shell splits it on spaces)
static std::string fen_from(const std::vector<std::string>& args, size_t i) {
    if (i >= args.size()) {
        return STARTPOS;
    }
    std::string fen = args[i];
    for (size_t j = i + 1; j < args.size(); j++) {
        fen += " " + args[j];
    }
    return fen;
}

int main(int argc, char** argv) {
    MoveGen::init_tables();
    Zobrist::init();
    TT::resize(16);

    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) {
        usage();
        return 0;
    }

    try {
        const std::string& cmd = args[0];

        if (cmd == "perft" && args.size() >= 2) {
            Board board;
            board.set_fen(fen_from(args, 2));
            Perft::divide(board, std::stoi(args[1]));
        }
        else if (cmd == "perft-suite") {
            return Perft::run_suite() ? 0 : 1;
        }
        else if (cmd == "search" && args.size() >= 2) {
            Board board;
            board.set_fen(fen_from(args, 2));
            int score = Search::get_best_move(board, std::stoi(args[1]));
            std::cout << "bestmove ";
            BitboardHelpers::print_move(Search::best_move);
            std::cout << "  (score " << score << " cp)\n";
        }
        else if (cmd == "gen-magics") {
            MoveGen::print_magics();
        }
        else {
            usage();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
