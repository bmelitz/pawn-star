#include "uci.hpp"
#include "bitboard_helpers.hpp"
#include "movegen.hpp"
#include "search.hpp"
#include "tt.hpp"
#include <algorithm>
#include <deque>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace UCI {

    namespace {
        const char* STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

        // Safety margin for the time it takes the GUI and the OS to deliver our move.
        constexpr int64_t MOVE_OVERHEAD_MS = 30;

        Board board;

        // StateInfo for every move played in the game. A deque never moves its
        // elements when it grows, so the `previous` pointers chaining them stay
        // valid. Repetition detection walks this chain back into the game history.
        std::deque<StateInfo> history;

        // The search runs on its own thread so this one can keep reading
        // commands ("stop", "isready") while it thinks.
        std::thread search_thread;

        void stop_search() {
            Search::stop = true;
            if (search_thread.joinable()) {
                search_thread.join();
            }
        }

        // position startpos [moves e2e4 e7e5 ...]
        // position fen <fen> [moves ...]
        void set_position(std::istringstream& in) {
            std::string token, fen;
            in >> token;
            if (token == "startpos") {
                fen = STARTPOS;
                in >> token; // "moves", if present
            } else if (token == "fen") {
                while (in >> token && token != "moves") {
                    fen += token + " ";
                }
            } else {
                return;
            }

            history.clear();
            board.set_fen(fen);

            while (in >> token) {
                Move m = parse_move(board, token);
                if (m.data == 0) {
                    std::cout << "info string illegal move " << token << std::endl;
                    break;
                }
                history.emplace_back();
                board.make_move(m, history.back());
            }
        }

        // Turns "go wtime ... btime ... winc ... binc ... movestogo ... movetime ... depth ..."
        // into search limits.
        Search::Limits parse_go(std::istringstream& in) {
            int64_t wtime = -1, btime = -1, winc = 0, binc = 0, movetime = -1;
            int movestogo = 0, depth = -1;
            bool infinite = false;

            std::string token;
            while (in >> token) {
                if (token == "wtime") in >> wtime;
                else if (token == "btime") in >> btime;
                else if (token == "winc") in >> winc;
                else if (token == "binc") in >> binc;
                else if (token == "movestogo") in >> movestogo;
                else if (token == "movetime") in >> movetime;
                else if (token == "depth") in >> depth;
                else if (token == "infinite") infinite = true;
            }

            Search::Limits limits;
            if (depth > 0) {
                limits.depth = std::min(depth, Search::MAX_PLY - 1);
            }
            if (infinite) {
                return limits;
            }
            if (movetime >= 0) {
                limits.soft_ms = limits.hard_ms = std::max<int64_t>(1, movetime - MOVE_OVERHEAD_MS);
                return limits;
            }

            bool white = board.get_side_to_move() == Color::WHITE;
            int64_t time = white ? wtime : btime;
            int64_t inc = white ? winc : binc;
            if (time < 0) {
                return limits; // no clock given, e.g. "go depth 10"
            }

            // Soft limit: our share of the remaining time, assuming ~20 more moves
            // (or exactly movestogo before the next time control), plus half the increment.
            // Hard limit: never sink more than half the clock (or 4x the soft limit) into one move.
            int64_t usable = std::max<int64_t>(1, time - MOVE_OVERHEAD_MS);
            int64_t moves_left = movestogo > 0 ? std::min(movestogo, 20) : 20;
            int64_t soft = usable / moves_left + inc / 2;
            int64_t hard = std::min(usable / 2, soft * 4);
            limits.hard_ms = std::max<int64_t>(1, hard);
            limits.soft_ms = std::min(soft, limits.hard_ms);
            return limits;
        }

        void go(std::istringstream& in) {
            stop_search();
            Search::Limits limits = parse_go(in);
            Search::stop = false;
            search_thread = std::thread([limits] {
                Move best = Search::think(board, limits);
                std::cout << "bestmove " << BitboardHelpers::move_str(best) << std::endl;
            });
        }

        // setoption name Hash value 64
        void set_option(std::istringstream& in) {
            std::string token, name, value;
            in >> token; // "name"
            while (in >> token && token != "value") {
                name += (name.empty() ? "" : " ") + token;
            }
            in >> value;
            if (name == "Hash") {
                stop_search();
                TT::resize(std::clamp(std::stoi(value), 1, 1024));
            }
        }
    }

    Move parse_move(Board& b, const std::string& str) {
        MoveList list;
        MoveGen::generate_pseudo_moves(b, list);
        Color us = b.get_side_to_move();

        for (int i = 0; i < list.count; ++i) {
            Move m = list.moves[i];
            if (BitboardHelpers::move_str(m) != str) {
                continue;
            }
            StateInfo st;
            b.make_move(m, st);
            bool legal = !MoveGen::is_square_attacked(b, b.get_king_square(us), b.get_side_to_move());
            b.unmake_move(m);
            if (legal) {
                return m;
            }
        }
        return Move();
    }

    void loop() {
        board.set_fen(STARTPOS);

        std::string line;
        while (std::getline(std::cin, line)) {
            std::istringstream in(line);
            std::string cmd;
            in >> cmd;

            if (cmd == "uci") {
                std::cout << "id name Pawn Star\n"
                          << "id author Ben Melitz\n"
                          << "option name Hash type spin default 16 min 1 max 1024\n"
                          << "uciok" << std::endl;
            }
            else if (cmd == "isready") {
                std::cout << "readyok" << std::endl;
            }
            else if (cmd == "ucinewgame") {
                stop_search();
                TT::clear();
            }
            else if (cmd == "setoption") {
                set_option(in);
            }
            else if (cmd == "position") {
                stop_search();
                try {
                    set_position(in);
                } catch (const std::exception& e) {
                    std::cout << "info string bad position: " << e.what() << std::endl;
                }
            }
            else if (cmd == "go") {
                go(in);
            }
            else if (cmd == "stop") {
                stop_search();
            }
            else if (cmd == "quit") {
                break;
            }
            else if (cmd == "d") { // debugging: show the current position
                stop_search();
                board.print();
            }
            else if (!cmd.empty()) {
                std::cout << "info string unknown command " << cmd << std::endl;
            }
        }
        stop_search();
    }
}
