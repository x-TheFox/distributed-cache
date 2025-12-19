# Benchmarks

This document describes how to run the micro-benchmarks for the cache and how to compare eviction policies (LRU vs LFU).

## Building the benchmark

From the repository root:

```bash
mkdir -p cpp/build && cd cpp/build
cmake ..
cmake --build . -- -j
```

This builds the `perf_bench` binary under `cpp/build`.

## Running the benchmark

The benchmark supports these options:

- `--policy` or `-p` : `lru` (default) or `lfu`
- `--ops` or `-n` : total number of put/get operations to perform (default 100000)
- `--threads` or `-t` : number of worker threads (default 4)
- `--dist` or `-d` : key distribution. Options: `uniform` (default), `zipf`
  - `--zipf-n` : number of distinct keys (Zipf `n` parameter) (default 1000)
  - `--zipf-s` : Zipf exponent `s` (default 1.0)

You can also set the distribution through environment variables:

- `KEY_DISTRIBUTION=zipf` to use zipfian keys
- `ZIPF_N` and `ZIPF_S` to tune Zipf parameters

Examples:

```bash
# Run default (LRU)
./perf_bench

# Run LFU with 1M operations and 8 threads
./perf_bench --policy lfu --ops 1000000 --threads 8

# Use environment variable instead of CLI
EVICTION_POLICY=lfu ./perf_bench --ops 200000
```

The binary prints total time, cache size, and hit/miss statistics. Use these outputs to compare throughput and eviction behavior between LRU and LFU.

## Tips for fair comparison

- Run multiple iterations and take medians.
- Make sure system caches are warmed up, and CPU turbo variations are accounted for.
- Use similar capacity settings and keys distribution to reflect realistic workloads.

## Next steps

- Add randomized workloads (Zipfian distributions) and record latency percentiles.
- Add a harness to generate plots and compare policy trade-offs across memory vs throughput.

## Running automated benchmark harness

A small harness is included at `scripts/bench/run_bench.sh`. It runs `perf_bench` for LRU and LFU across multiple iterations and writes a CSV at `benchmarks/results/<timestamp>/results.csv`.

Example:

```bash
# Build first (from repo root)
mkdir -p cpp/build && cd cpp/build
cmake .. && cmake --build . -- -j

# Run harness (from repo root)
./scripts/bench/run_bench.sh --ops 100000 --threads 8 --iters 10

# Inspect results
cat benchmarks/results/<timestamp>/results.csv
```

