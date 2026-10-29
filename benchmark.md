## Transposition Table Benchmark

**Position (FEN):** ```3r1r2/1p3qbk/1p1p2pp/1P1Ppn2/R3N1BP/1Q1P2P1/5P2/2R3K1 w - - 0 1```
**Search Depth:** 7

### Performance Comparison
| Metric         |   With TT | Without TT | Improvement |
| -------------- | --------: | ---------: | ----------: |
| Nodes Searched | 4,250,949 | 10,157,659 |    2.39     |
| Time           |   1.167 s |    2.674 s |    2.29     |

### Transposition Table Hit Distribution
| Flag Type |   Hits | Percentage |
| --------- | -----: | ---------: |
| EXACT     |    396 |       0.4% |
| ALPHA     | 12,117 |      11.3% |
| BETA      | 94,987 |      88.3% |

The vast majority of useful TT entries were **BETA cutoffs**, indicating that the transposition table primarily improved performance by enabling early pruning of branches.