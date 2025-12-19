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
