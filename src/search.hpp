#pragma once
#include "board.hpp"
#include "movegen.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>

namespace Search {
    constexpr int INFINITY_SCORE = 32000;
    constexpr int MATE_SCORE = 30000;
    constexpr int MAX_PLY = 64;

    // How long / deep one search may run. -1 = no limit.
    struct Limits {
        int depth = MAX_PLY - 1;
        int64_t soft_ms = -1;   // don't start a new iteration after this much time
        int64_t hard_ms = -1;   // abort the current iteration at this much time
        bool print_info = true; // print UCI "info" lines after each iteration
        // when the clock started: set when "go" arrives, not when the search thread
        // starts, so parsing and thread startup count against our time too
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    };

    extern Move best_move;
    extern int best_score;
    extern uint64_t nodes_searched;

    // Set from the UCI thread to end the search early ("stop"/"quit").
    // Depth 1 always completes, so there is always a move to play.
    extern std::atomic<bool> stop;

    // Iterative deepening within the limits. Returns the best move of the
    // last *completed* iteration (also stored in best_move / best_score).
    Move think(Board& board, const Limits& limits);

    // forget killers and history, e.g. for a new game
    void clear_heuristics();

    // fixed-depth search for the command line; returns the score
    int get_best_move(Board& board, int depth);

    int quiescence_search(Board& board, int alpha, int beta, int ply);
}
