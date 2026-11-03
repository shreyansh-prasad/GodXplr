# GodXplr — U0 Baseline Synthetic Benchmark Report

- **Date:** 2026-11-03
- **Phase:** P03 / Spec 03 U0
- **Commit:** `0d06e67`
- **Compiler:** MSVC v19.44.35224 (x64)
- **Qt Version:** Headless / None
- **OS Build:** Windows 11 Build 26200
- **CPU:** 13th Gen Intel(R) Core(TM) i5-13450HX (16 logical cores, AVX2 enabled)
- **RAM:** 24 GB Physical RAM (23.715 GB detected)
- **Storage:** NVMe SSD (NTFS)

---

## 1. Workload Specification

- **Workload Type:** Synthetic in-memory dataset generator (Spec 03 §16.1)
- **Deterministic Seed:** 42
- **Manifest Hash:** `59358f6ae245a452f5dda752fed66f17`
- **Directory/File Mix:** 8.0% directories (8,000 dirs), 92.0% files (92,000 files)
- **Total Entries:** 100,000 items
- **Total Unpacked Size:** 1.98 TB
- **Extension Mix:** Documents (30%), Code (30%), Media/Images (20%), Binaries/Archives (15%), System/Config (5%)
- **Character Encoding:** UTF-8 including English ASCII, German/French umlauts/accents, and Japanese Kanji/Kana

---

## 2. Benchmark Results

| Scenario | Entries | Iterations | p50 Latency | p95 Latency | p99 Latency | Max Latency | Working Set |
|---|---|---|---|---|---|---|---|
| `dataset-generation-10k` | 10,000 | 10 | 5.11 ms | 8.79 ms | 9.93 ms | 9.93 ms | 15.2 MB |
| `dataset-generation-100k` | 100,000 | 10 | 54.92 ms | 59.21 ms | 59.21 ms | 60.61 ms | 23.9 MB |
| `sort-by-name-100k` | 100,000 | 20 | 60.21 ms | 71.47 ms | 73.75 ms | 73.75 ms | 24.1 MB |
| `simd-search-query-100k` | 100,000 | 100 | 0.66 ms (655 µs) | 0.69 ms (690 µs) | 0.72 ms | 0.78 ms | 24.1 MB |

> [!NOTE]
> This benchmark establishes the U0 baseline on synthetic data. These figures are not competitor comparisons; they define the reproducible baseline for subsequent UI virtualized model and view performance validations.
