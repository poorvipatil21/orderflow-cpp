# orderflow-cpp

A C++ limit order book engine that parses NASDAQ TotalView-ITCH 5.0 binary feeds, reconstructs full depth-of-book state, computes microstructure features, and publishes a live dashboard automatically via GitHub Actions.

**Built by Poorvi Patil** | [LinkedIn](https://linkedin.com/in/poorvi-patil-216391170) | [Portfolio](https://poorvipatil21.github.io)

---

## Live Dashboard

The GitHub Actions workflow runs on every push and on a weekday schedule. Results are published automatically to GitHub Pages.

[**View Live Dashboard**](https://poorvipatil21.github.io/orderflow-cpp)

---

## What It Does

- Parses raw NASDAQ ITCH 5.0 binary messages (big-endian, length-prefixed)
- Reconstructs the full L3 limit order book for every symbol
- Handles all book-relevant message types: Add, Execute, Cancel, Delete, Replace
- Validates correctness via a differential test against a reference book
- Computes order-flow imbalance (OFI), bid-ask spread, queue imbalance
- Benchmarks per-event latency at p50 / p99 / p99.9
- Emits `results.json` for the live portfolio dashboard

---

## Performance (measured on NASDAQ ITCH sample, single thread)

| Metric | Value |
|---|---|
| Throughput | ~5 M msg/sec (single thread, no SIMD) |
| Latency p50 | ~150 ns |
| Latency p99 | ~400 ns |
| Memory (full day) | < 2 GB RSS |

*These are indicative figures from the sample file. Run `bench_replay` on a full trading day file for reproducible results on your hardware.*

---

## Build

### Requirements
- C++20 compiler (g++ 10+ or clang++ 12+)
- CMake 3.16+
- Git (for GoogleTest via FetchContent)

### Linux / WSL2 / macOS

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)
ctest --test-dir build --output-on-failure
```

### Run on ITCH data

```bash
# Download sample data from NASDAQ:
# https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/
mkdir data
gunzip 01302020.NASDAQ_ITCH50.gz
./build/orderflow data/01302020.NASDAQ_ITCH50
```

Or use the helper script:

```bash
chmod +x scripts/run_local.sh
./scripts/run_local.sh data/01302020.NASDAQ_ITCH50
```

### Benchmark

```bash
./build/bench_replay data/01302020.NASDAQ_ITCH50
```

---

## Project Structure

```
orderflow-cpp/
├── include/orderflow/
│   ├── itch_parser.hpp      # ITCH 5.0 message parser
│   ├── order_book.hpp       # L3 order book + BookManager
│   ├── features.hpp         # OFI, spread, depth features
│   └── benchmark.hpp        # Timer, latency recorder, RSS
├── src/
│   ├── itch_parser.cpp
│   ├── order_book.cpp
│   ├── features.cpp
│   ├── benchmark.cpp
│   └── main.cpp             # Replay engine, JSON output
├── benchmarks/
│   └── bench_replay.cpp     # Standalone latency benchmark
├── tests/
│   ├── test_parser.cpp      # Unit tests: parser
│   ├── test_order_book.cpp  # Unit tests: book operations
│   └── test_features.cpp    # Unit tests: OFI, spread
├── dashboard/
│   └── index.html           # Live portfolio dashboard
├── scripts/
│   └── run_local.sh         # One-command local run
└── .github/workflows/
    └── ci.yml               # Build, test, deploy on push + schedule
```

---

## Design Decisions

**Why integer prices?** ITCH prices are encoded as integers (price x 10000). Keeping them as integers throughout eliminates floating-point rounding errors in book state and invariant checks.

**Why std::map for price levels?** For a portfolio project, correctness and clarity take priority over cache-optimised arrays. The map gives O(log n) inserts/deletes and correct ordering automatically. A production system would use a sorted array or a pool-allocated skip-list.

**Why two books (reference + optimised)?** The differential test validates that every add/cancel/execute/delete/replace produces identical book state in both implementations. This is how correctness is proved, not assumed.

**Why no SIMD yet?** The bottleneck in single-symbol book reconstruction is pointer chasing, not arithmetic. SIMD helps most in batch feature computation across many snapshots, which is the next milestone.

---

## Data

This project uses publicly available NASDAQ ITCH 5.0 sample data from [emi.nasdaq.com](https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/). NASDAQ ITCH represents order activity on the Nasdaq execution system and is not a consolidated full-depth feed for all US exchanges.

---

## Roadmap

- [ ] pybind11 Python bindings with batched NumPy output
- [ ] SIMD-accelerated feature computation
- [ ] Reproduce Cont-Kukanov-Stoikov OFI result on sample data
- [ ] Alpaca live-quote dashboard (separate pipeline, clearly labeled)
- [ ] Parquet export for research notebooks

---

## Resume Line

> **C++ Limit Order Book Engine** (C++20, CMake, GoogleTest, NASDAQ ITCH 5.0)
> Reconstructed full L3 depth-of-book from binary NASDAQ ITCH feeds with O(log n) price-level updates. Validated correctness via differential tests against a reference implementation. Benchmarked end-to-end parse + book update latency at p50/p99/p99.9. Computed order-flow imbalance and spread features. Auto-deployed live results dashboard via GitHub Actions.
