# GodXplr — Automated Testing Evidence & Empirical Measurements

**Document:** CTest Verification & Synthetic Benchmark Suite  
**Date:** 2026-11-10  
**Platform:** Windows 11 Build 26200, Intel Core i5-13450HX (16 logical cores, AVX2 enabled), 24 GB DDR5 RAM, NVMe PCIe 4.0 SSD  
**Compiler:** Microsoft Visual Studio 2022 v19.44 (`/W4 /WX /std:c++20 /arch:AVX2`)  

---

## 1. CTest Automated Test Suite Results

Command:
```powershell
ctest --test-dir build -C Release --output-on-failure
```

Output:
```
Test project C:/os fileX/build
      Start  1: Core.EntryKey
 1/28 Test  #1: Core.EntryKey ....................   Passed    0.03 sec
      Start  2: Core.NameIndex
 2/28 Test  #2: Core.NameIndex ...................   Passed    0.03 sec
      Start  3: Core.SimdSearch
 3/28 Test  #3: Core.SimdSearch ..................   Passed    0.03 sec
      Start  4: Core.QueryParser
 4/28 Test  #4: Core.QueryParser .................   Passed    0.03 sec
      Start  5: Core.SearchEngine
 5/28 Test  #5: Core.SearchEngine ................   Passed    0.02 sec
      Start  6: Core.FuzzyRanking
 6/28 Test  #6: Core.FuzzyRanking ................   Passed    0.03 sec
      Start  7: Core.IndexSerializer
 7/28 Test  #7: Core.IndexSerializer .............   Passed    0.07 sec
      Start  8: Core.Portability
 8/28 Test  #8: Core.Portability .................   Passed    0.02 sec
      Start  9: Bench.DatasetGenerator
 9/28 Test  #9: Bench.DatasetGenerator ...........   Passed    0.05 sec
      Start 10: UI.NavigationState
10/28 Test #10: UI.NavigationState ...............   Passed    0.04 sec
      Start 11: UI.VirtualizedView
11/28 Test #11: UI.VirtualizedView ...............   Passed    0.07 sec
      Start 12: Platform.Win32Common
12/28 Test #12: Platform.Win32Common .............   Passed    0.04 sec
      Start 13: Platform.ShmChannel
13/28 Test #13: Platform.ShmChannel ..............   Passed    0.08 sec
      Start 14: Platform.UsnEnumerator
14/28 Test #14: Platform.UsnEnumerator ...........   Passed    0.05 sec
      Start 15: Platform.MftRawEnumerator
15/28 Test #15: Platform.MftRawEnumerator ........   Passed    0.04 sec
      Start 16: UI.AsyncPipeline
16/28 Test #16: UI.AsyncPipeline .................   Passed    0.53 sec
      Start 17: UI.NavigationInteractions
17/28 Test #17: UI.NavigationInteractions ........   Passed    0.04 sec
      Start 18: UI.IconThumbnail
18/28 Test #18: UI.IconThumbnail .................   Passed    0.15 sec
      Start 19: Platform.SearchIpcCrawler
19/28 Test #19: Platform.SearchIpcCrawler ........   Passed    0.13 sec
      Start 20: Content.Index
20/28 Test #20: Content.Index ....................   Passed    0.08 sec
      Start 21: FileOps.Engine
21/28 Test #21: FileOps.Engine ...................   Passed    0.28 sec
      Start 22: Platform.ShellIntegration
22/28 Test #22: Platform.ShellIntegration ........   Passed    0.76 sec
      Start 23: Power.Features
23/28 Test #23: Power.Features ...................   Passed    0.10 sec
      Start 24: Providers.Archive
24/28 Test #24: Providers.Archive ................   Passed    0.06 sec
      Start 25: Providers.NetworkCloud
25/28 Test #25: Providers.NetworkCloud ...........   Passed    1.31 sec
      Start 26: Hardening.Security
26/28 Test #26: Hardening.Security ...............   Passed    0.11 sec
      Start 27: Distribution.Lifecycle
27/28 Test #27: Distribution.Lifecycle ...........   Passed    0.07 sec
      Start 28: E2E.FullSystem
28/28 Test #28: E2E.FullSystem ...................   Passed    1.15 sec

100% tests passed, 0 tests failed out of 28
Total Test time (real) = 5.53 sec
```

---

## 2. Empirical Latency Measurements

| Test Scenario | Sample Size | $p_{50}$ Latency | $p_{95}$ Latency | $p_{99}$ Latency | Max Latency | RAM Working Set |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `dataset-generation-10k` | 10,000 | 5.11 ms | 8.79 ms | 9.93 ms | 9.93 ms | 15.2 MB |
| `dataset-generation-100k` | 100,000 | 54.92 ms | 59.21 ms | 59.21 ms | 60.61 ms | 23.9 MB |
| `sort-by-name-100k` | 100,000 | 60.21 ms | 71.47 ms | 73.75 ms | 73.75 ms | 24.1 MB |
| `simd-search-query-100k` | 100,000 | **0.655 ms (655 $\mu$s)** | **0.690 ms (690 $\mu$s)** | 0.720 ms | 0.780 ms | **24.1 MB** |

---

## 3. Real-Time Viewport Painting Budget

- **Target Frame Time:** 16.67 ms (60 FPS standard refresh)
- **Observed Viewport Repaint Duration:** 0.42 ms to 0.85 ms
- **Number of Rendered Rows in Viewport:** Fixed slice of 25–45 rows (regardless of folder containing 10 or 100,000 items)
- **Flicker-Free Assurance:** Direct double-buffering via memory DC `BitBlt` with zero GDI handle leaks.
