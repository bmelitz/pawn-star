#pragma once
#include "board.hpp"

// A small built-in opening book: ~40 main lines of popular openings, so games
// against the engine don't start with the same moves every time. Positions are
// matched by Zobrist hash, so a line is found however its position was reached.
namespace Book {
    // Replays every line once and indexes the positions. Call after MoveGen and
    // Zobrist are initialized. Returns false (and reports to stderr) if a line
    // contains an illegal move.
    bool init();

    // A random book move for this position, weighted by how many book lines
    // continue with it, or a null Move if the position isn't in the book.
    Move probe(Board& board);
}
