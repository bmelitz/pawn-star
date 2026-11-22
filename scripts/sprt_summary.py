#!/usr/bin/env python3
"""Recompute a match summary from a fastchess PGN, in case the terminal output is lost.

    python3 scripts/sprt_summary.py sprt/games-YYYYMMDD-HHMMSS.pgn

Games are paired by round (same opening, colors swapped) and scored as fastchess does
(pentanomial: each pair scores 0, 0.5, 1, 1.5 or 2), giving Elo for "dev" with a 95% interval.
"""
import re, sys, math, collections
def analyze(path):
    games = open(path).read().split('[Event ')[1:]
    pairs = collections.defaultdict(list); w = d = l = 0; to = 0
    for g in games:
        r = re.search(r'\[Result "([^"]+)"', g).group(1)
        if r == '*': continue
        wh = re.search(r'\[White "([^"]+)"', g).group(1)
        rnd = re.search(r'\[Round "([^"]+)"', g).group(1)
        s = 0.5 if r == '1/2-1/2' else (1.0 if (r == '1-0') == (wh == 'dev') else 0.0)
        w += s == 1; d += s == 0.5; l += s == 0
        to += 'time forfeit' in g
        pairs[rnd].append(s)
    full = [p for p in pairs.values() if len(p) == 2]
    ptn = [0]*5
    for p in full: ptn[int(round((p[0]+p[1])*2))] += 1
    n = len(full); xs = [(i/4) for i in range(5)]  # pair score per game, 0..1
    mean = sum(c*x for c,x in zip(ptn,xs))/n
    var = sum(c*(x-mean)**2 for c,x in zip(ptn,xs))/n
    elo = lambda s: -400*math.log10(1/s-1)
    se = math.sqrt(var/n)
    lo, hi = elo(mean-1.96*se), elo(mean+1.96*se)
    print(f"{path}: games {w+d+l} W{w} L{l} D{d}, pairs {n}, Ptnml {ptn}, timeouts {to}")
    print(f"   Elo {elo(mean):+.2f} +/- {(hi-lo)/2:.2f}   points {(w+d/2)/(w+d+l)*100:.2f}%")
for p in sys.argv[1:]: analyze(p)
