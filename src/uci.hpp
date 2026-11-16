#pragma once
#include "board.hpp"
#include <string>

// Universal Chess Interface: the text protocol chess GUIs and testing tools
// (fastchess, cutechess, lichess-bot) use to talk to engines over stdin/stdout.
namespace UCI {
    // reads commands from stdin until "quit" or end of input
    void loop();

    // finds the legal move matching UCI notation like "e2e4" or "e7e8q";
    // returns a null Move if there is none
    Move parse_move(Board& board, const std::string& str);
}
