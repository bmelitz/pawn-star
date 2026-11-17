# Pawn Star ♟

[![CI](https://github.com/bmelitz/pawn-star/actions/workflows/ci.yml/badge.svg)](https://github.com/bmelitz/pawn-star/actions/workflows/ci.yml)

A chess engine written from scratch in C++17.

## Features

- **Move generation:** bitboards, magic-bitboard slider attacks, 16-bit move encoding, make/unmake, incremental Zobrist hashing
- **Search:** iterative-deepening alpha-beta, transposition table, null-move pruning, MVV-LVA move ordering, quiescence search, check extensions
- **Evaluation:** PeSTO tapered piece-square tables
- **UCI protocol** with time management, so it runs in any chess GUI or testing tool

## Testing

A built-in perft suite checks move generation against known node counts on 21 positions: the six
standard ones plus en passant, castling and promotion edge cases. CI runs it on every push, on Ubuntu
and macOS, including under AddressSanitizer/UBSan.

Strength changes are measured with SPRT self-play: `scripts/sprt.sh [base]` builds the working tree
and a base version (default `main`) and plays them against each other with
[fastchess](https://github.com/Disservin/fastchess) until the test reaches a verdict.
Measurements are in [benchmark.md](benchmark.md).

## Build and run

```bash
make                         # -> ./pawnstar
make test                    # build + perft suite
./pawnstar                   # UCI mode: point a GUI (Cute Chess, En Croissant, Arena) at this binary
./pawnstar bench             # fixed-depth search over 12 positions; node count fingerprints the search
./pawnstar search 10 "<fen>" # search a position to depth 10
./pawnstar perft 5           # per-move perft counts from the start position
```

Perft reference values from the [Chess Programming Wiki](https://www.chessprogramming.org/Perft_Results);
piece-square tables from PeSTO by Ronald Friederich.
