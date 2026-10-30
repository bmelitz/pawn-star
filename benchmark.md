# Benchmarks

All numbers are single-threaded, `make` (release, `-O3`) build, measured on Apple Silicon
(Linux VM). Absolute times will differ on other machines; node counts are deterministic.

## Perft

`./pawnstar perft-suite` runs 21 positions with known leaf counts: the six standard positions
from the [Chess Programming Wiki](https://www.chessprogramming.org/Perft_Results), position 4
mirrored (exercises Black's promotion and castling code), and 14 edge cases covering illegal
en passant, en passant discovered checks, castling through/into check, castling-rights loss,
promotions out of / into check, underpromotion, and stalemate vs. checkmate.

| Position | Depth | Nodes |
| --- | --: | --: |
| Start position | 5 | 4,865,609 |
| Kiwipete | 4 | 4,085,603 |
| CPW position 3 | 5 | 674,624 |
| CPW position 4 (and mirrored) | 4 | 422,333 |
| CPW position 5 | 4 | 2,103,487 |
| CPW position 6 | 4 | 3,894,594 |
| 14 edge-case positions | 4–7 | 13,778,965 total |

**Result:** 21/21 pass, 30,247,548 nodes in 1.14 s (**26.5M nodes/s**, no bulk counting).
CI runs the full suite on every push, on Ubuntu and macOS, in both release and
AddressSanitizer/UBSan builds.

## Startup time

Magic numbers for sliding-piece attack lookup used to be found by random search at every launch.
They are now hardcoded in `src/magics.hpp` (reproducible with `./pawnstar gen-magics`) and
verified collision-free at startup.

| | Startup |
| --- | --: |
| Random magic search at launch | 1.35 s |
| Hardcoded magics | 0.02 s |

## Transposition table

**Position (FEN):** `3r1r2/1p3qbk/1p1p2pp/1P1Ppn2/R3N1BP/1Q1P2P1/5P2/2R3K1 w - - 0 1`
**Search:** iterative deepening to depth 7 (`./pawnstar search 7 <fen>`)

"Without TT" disables the table entirely: no score cutoffs *and* no TT move for move ordering.

| Metric | With TT | Without TT | Improvement |
| --- | --: | --: | --: |
| Nodes searched | 7,008,135 | 50,460,876 | 7.2× |
| Time | 0.77 s | 5.92 s | 7.7× |

### Where the TT cutoffs come from

| Flag | Cutoffs | Share |
| --- | --: | --: |
| EXACT | 429 | 0.4% |
| ALPHA (upper bound) | 12,117 | 11.3% |
| BETA (lower bound) | 95,005 | 88.3% |

Almost all useful TT hits are **beta cutoffs**: a position already proven "too good" for the
side to move is pruned without searching it again. Much of the remaining gain likely comes from move
ordering: the stored best move is tried first, and it often causes a cutoff immediately.

## Elo progression

Each search feature is tested against the previous version with SPRT self-play
([fastchess](https://github.com/Disservin/fastchess)); results are added here as they land.

| Version | Change | Elo vs previous | ± | Games |
| --- | --- | --: | --: | --: |
| v0 | Baseline (alpha-beta, TT, MVV-LVA, qsearch, PeSTO eval) | — | — | — |
