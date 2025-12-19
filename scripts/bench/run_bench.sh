#!/usr/bin/env bash
set -euo pipefail

# Simple benchmark harness that runs perf_bench for multiple iterations and policies
# Usage: ./scripts/bench/run_bench.sh [--ops N] [--threads T] [--iters I]

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../" && pwd)"
BUILD_BIN="$ROOT_DIR/cpp/build/perf_bench"
OUT_DIR="$ROOT_DIR/benchmarks/results/$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUT_DIR"

OPS=100000
THREADS=4
ITERS=5
POLICIES=(lru lfu)

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ops|-n) OPS="$2"; shift 2;;
    --threads|-t) THREADS="$2"; shift 2;;
    --iters) ITERS="$2"; shift 2;;
    --help) echo "Usage: $0 [--ops N] [--threads T] [--iters I]"; exit 0;;
    *) echo "Unknown arg $1"; exit 1;;
  esac
done

CSV="$OUT_DIR/results.csv"
echo "policy,iter,ops,threads,duration_s,cache_size,hits,misses" > "$CSV"

for policy in "${POLICIES[@]}"; do
  for ((i=1;i<=ITERS;i++)); do
    echo "Running policy=$policy iter=$i ops=$OPS threads=$THREADS"
    # Run the benchmark; capture output
    OUT="$($BUILD_BIN --policy "$policy" --ops "$OPS" --threads "$THREADS")"
    # Parse output lines (expects the perf_bench format)
    # Example output lines:
    # Completed 10000 ops (4 threads) in 0.065953s
    # Cache size: 1024, hits=10000, misses=0
    DURATION_LINE=$(echo "$OUT" | head -n1)
    STATS_LINE=$(echo "$OUT" | tail -n1)
    DURATION=$(echo "$DURATION_LINE" | sed -n 's/.*in \([0-9.]*\)s$/\1/p')
    CACHE_SIZE=$(echo "$STATS_LINE" | sed -n 's/Cache size: \([0-9]*\),.*/\1/p')
    HITS=$(echo "$STATS_LINE" | sed -n 's/.*hits=\([0-9]*\),.*/\1/p')
    MISSES=$(echo "$STATS_LINE" | sed -n 's/.*misses=\([0-9]*\).*/\1/p')

    echo "${policy},${i},${OPS},${THREADS},${DURATION},${CACHE_SIZE},${HITS},${MISSES}" >> "$CSV"
  done
done

echo "Results written to $CSV"

echo "Done. To upload results, add $OUT_DIR as an artifact in CI or inspect locally." 