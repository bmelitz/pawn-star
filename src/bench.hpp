#pragma once
#include <cstdint>

// Searches a fixed set of positions to a fixed depth and reports the total node
// count. Since the search is deterministic, the node count works as a fingerprint
// of the engine's behavior: a change that should only affect speed must leave it
// unchanged, and every commit records it ("Bench: 1234567").
namespace Bench {
    constexpr int DEFAULT_DEPTH = 7;

    uint64_t run(int depth = DEFAULT_DEPTH);
}
