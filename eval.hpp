#pragma once
#include "board.hpp"

namespace Eval {
    int evaluate(const Board& board);
    void trace_evaluate(const Board& board);
}