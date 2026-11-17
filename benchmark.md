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

**Result:** 21/21 pass, 30,247,548 nodes in 0.69 s (**44M nodes/s**, no bulk counting).
CI runs the full suite on every push, on Ubuntu and macOS, in both release and
AddressSanitizer/UBSan builds.

## Bench

`./pawnstar bench` searches 12 positions (openings, middlegames, endgames) to depth 7 with a
cleared TT. The search is deterministic, so the total node count is a fingerprint: it only changes
when search behavior changes, and every commit that changes it records the new value.

| Version | Bench nodes (depth 7) | Speed |
| --- | --: | --: |
| v0 | 36,085,544 | 12.5M nodes/s |
| + null-move pruning | 9,779,211 | 11.6M nodes/s |

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
| Nodes searched | 8,515,203 | 50,324,788 | 5.9× |
| Time | 0.64 s | 3.80 s | 5.9× |

### Where the TT cutoffs come from

| Flag | Cutoffs | Share |
| --- | --: | --: |
| EXACT | 589 | 0.3% |
| ALPHA (upper bound) | 13,970 | 8.0% |
| BETA (lower bound) | 160,266 | 91.7% |

Almost all useful TT hits are **beta cutoffs**: a position already proven "too good" for the
side to move is pruned without searching it again. Much of the remaining gain likely comes from move
ordering: the stored best move is tried first, and it often causes a cutoff immediately.

## Elo progression

Each search feature is tested against the previous version with SPRT self-play
([fastchess](https://github.com/Disservin/fastchess)); results are added here as they land.

| Version | Change | Elo vs previous | ± | Games |
| --- | --- | --: | --: | --: |
| v0 | Baseline (alpha-beta, TT, MVV-LVA, qsearch, PeSTO eval) | — | — | — |
| v1 | Null-move pruning | +83.9 | 29.7 | 418 |

The v1 test ran before a time-management fix: 49 of 418 games were lost on time, split
roughly evenly between the two versions (27 vs 22), so the comparison is still fair but noisier.

Harness sanity check: v0 against itself, 200 games at 8+0.08, scored −10 ± 40 Elo (66 wins,
62 draws, 72 losses), consistent with 0 as it should be, with no crashes or time losses.
