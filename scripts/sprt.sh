#!/usr/bin/env bash
# Is the code in the working tree stronger than <base-ref>? Runs an SPRT match.
#
#   scripts/sprt.sh            dev = working tree, base = main
#   scripts/sprt.sh v0         base = the v0 tag (any branch, tag or commit works)
#
# Builds both engines, downloads the opening book the first time, then runs
# fastchess until the SPRT accepts H1 (dev is stronger) or H0 (it isn't).
# Ctrl-C stops early. Games are saved under sprt/.
#
# Overrides (environment variables):
#   TC=8+0.08  CONCURRENCY=<cores-1>  ELO0=0  ELO1=10  FASTCHESS=fastchess
#   NO_SPRT=1 ROUNDS=100   play a fixed number of game pairs instead (sanity checks)
set -euo pipefail

BASE_REF="${1:-main}"
ROOT="$(git rev-parse --show-toplevel)"
OUT="$ROOT/sprt"
BOOK="$OUT/UHO_Lichess_4852_v1.epd"
BOOK_URL="https://raw.githubusercontent.com/official-stockfish/books/master/UHO_Lichess_4852_v1.epd.zip"

TC="${TC:-8+0.08}"
CORES="$(getconf _NPROCESSORS_ONLN)"
CONCURRENCY="${CONCURRENCY:-$(( CORES > 1 ? CORES - 1 : 1 ))}"
ELO0="${ELO0:-0}"
ELO1="${ELO1:-10}"
ROUNDS="${ROUNDS:-20000}"
FASTCHESS="${FASTCHESS:-fastchess}"

if ! command -v "$FASTCHESS" >/dev/null; then
    echo "fastchess not found. Install it, or run with FASTCHESS=/path/to/fastchess" >&2
    exit 1
fi
mkdir -p "$OUT"

if [ ! -f "$BOOK" ]; then
    echo "Downloading opening book (~40 MB, first run only)..."
    curl -fsSL -o "$OUT/book.zip" "$BOOK_URL"
    unzip -o -q "$OUT/book.zip" -d "$OUT"
    rm "$OUT/book.zip"
fi

echo "Building dev from the working tree..."
make -C "$ROOT" -j >/dev/null
# rm first: overwriting a binary in place on macOS (Apple Silicon) leaves a stale
# code-signature cache, and the OS kills the new binary on launch
rm -f "$OUT/dev" "$OUT/base"
cp "$ROOT/pawnstar" "$OUT/dev"

echo "Building base from $BASE_REF..."
TMP="$(mktemp -d)"
trap 'git -C "$ROOT" worktree remove --force "$TMP" >/dev/null 2>&1 || true' EXIT
git -C "$ROOT" worktree add --detach --quiet "$TMP" "$BASE_REF"
make -C "$TMP" -j >/dev/null
cp "$TMP/pawnstar" "$OUT/base"

for e in dev base; do
    nodes="$("$OUT/$e" bench | cut -d' ' -f1)" || true
    if [ -z "$nodes" ]; then
        echo "error: $e engine failed to run (try: \"$OUT/$e\" bench)" >&2
        exit 1
    fi
    echo "$e bench: $nodes"
done

ARGS=(
    -engine cmd="$OUT/dev" name=dev
    -engine cmd="$OUT/base" name=base
    -each tc="$TC" option.Hash=16 option.OwnBook=false
    -openings file="$BOOK" format=epd order=random
    -rounds "$ROUNDS" -repeat -concurrency "$CONCURRENCY" -recover
    -pgnout file="$OUT/games-$(date +%Y%m%d-%H%M%S).pgn"
)
if [ -z "${NO_SPRT:-}" ]; then
    ARGS+=( -sprt elo0="$ELO0" elo1="$ELO1" alpha=0.05 beta=0.05 )
fi

echo "Running: fastchess ${ARGS[*]}"
"$FASTCHESS" "${ARGS[@]}"
