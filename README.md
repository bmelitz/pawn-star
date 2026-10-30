# Pawn Star ♟

[![CI](https://github.com/bmelitz/pawn-star/actions/workflows/ci.yml/badge.svg)](https://github.com/bmelitz/pawn-star/actions/workflows/ci.yml)

A chess engine written from scratch in C++17. Bitboard move generation with magic
lookups, alpha-beta search with a transposition table, and a tapered evaluation.

> Status: move generation is complete and perft-verified; search and evaluation work from
> the command line. UCI support (so it can play in any chess GUI) is in progress.

## Features

**Board representation and move generation**
- 64-bit bitboards per piece type and color, plus a mailbox array for O(1) "what's on this square"
- Magic bitboards for rook/bishop/queen attacks, with per-square variable shifts
  (102,400 rook + 5,248 bishop table entries instead of 262,144 + 32,768 with fixed shifts)
- 16-bit move encoding: 6 bits from, 6 bits to, 4 bits of flags (captures, promotions, castling, en passant)
- Make/unmake with a linked list of irreversible state (castling rights, en passant square,
  50-move clock, captured piece), so unmake needs no recomputation
- Incrementally updated Zobrist hashing

**Search**
- Iterative-deepening negamax with alpha-beta pruning
- Transposition table: 8-byte entries, depth-preferred replacement, mate scores adjusted
  for distance from the root
- Move ordering: TT move first, then captures by MVV-LVA (most valuable victim, least valuable attacker)
- Quiescence search over captures and promotions to avoid the horizon effect
- Check extensions and mate-distance pruning
- Draw detection: threefold repetition, 50-move rule, insufficient material

**Evaluation**
- Tapered middlegame/endgame piece-square tables (PeSTO), interpolated by game phase

## Correctness

Move generation is verified with **perft**: counting every leaf of the legal move tree and
comparing against known values. The built-in suite covers the six standard positions plus
15 more targeting en passant, castling and promotion edge cases. 21/21 pass, at about 26M nodes/s.

CI runs the suite on every push, on Ubuntu and macOS, both in a release build and under
AddressSanitizer + UndefinedBehaviorSanitizer.

See [benchmark.md](benchmark.md) for perft, startup and transposition-table measurements.

## Building

Requires a C++17 compiler (GCC or Clang) and `make`.

```bash
make              # optimized build -> ./pawnstar
make test         # build and run the perft suite
make debug        # sanitizer build -> ./pawnstar-debug
make clean
```

## Usage

```bash
./pawnstar perft-suite                       # run the perft test suite
./pawnstar perft 5                           # node count under each move from the start position
./pawnstar perft 4 "<fen>"                   # same, from any position
./pawnstar search 8                          # search the start position to depth 8
./pawnstar search 10 "<fen>"                 # search any position
./pawnstar gen-magics                        # re-derive the magic numbers in src/magics.hpp
```

## Code layout

| File | Purpose |
| --- | --- |
| `src/types.hpp` | Core types: pieces, colors, squares, the 16-bit `Move`, `MoveList` |
| `src/board.*` | Board state, FEN parsing, make/unmake, repetition and material draws |
| `src/movegen.*` | Attack tables, magic bitboards, pseudo-legal move generation |
| `src/magics.hpp` | Hardcoded magic numbers |
| `src/zobrist.*` | Zobrist hashing keys |
| `src/tt.*` | Transposition table |
| `src/search.*` | Iterative deepening, alpha-beta, quiescence search, move ordering |
| `src/eval.*` | PeSTO tapered evaluation |
| `src/perft.*` | Perft, divide, and the test suite |
| `src/main.cpp` | Command-line interface |

## Roadmap

- [x] Bitboard move generation, perft-verified
- [x] Alpha-beta search with transposition table, quiescence search
- [ ] UCI protocol and time management
- [ ] SPRT testing harness, with each search improvement measured in Elo
- [ ] Killer moves, history heuristic, null-move pruning, reverse futility pruning, PVS, late move reductions
- [ ] Static exchange evaluation (SEE)

## Acknowledgements

- [Chess Programming Wiki](https://www.chessprogramming.org/), for perft reference values and much else
- PeSTO piece-square tables by Ronald Friederich
