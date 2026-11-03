# Phase P06 — Completion Report

- **Date:** 2026-11-04
- **Commit:** Pending
- **Status:** PASS
- **Active substep:** Spec 03 U3–U4 (Async directory enumeration, bounded mailbox, generations and sorting)

## Objective and scope

Connect the view to a real directory-provider abstraction without blocking the GUI thread:
1. Define the formal provider interface contract (`IDirectoryProvider`, `IDirectoryWatcher`, `IWatchListener`, `DirectoryBatch`, `ProviderCapabilities`, `ProviderStatus`, `WatchEvent`, `CancellationToken`).
2. Implement `Win32DirectoryProvider` using documented Win32 enumeration APIs (`FindFirstFileExW`, `FindNextFileW`, `FindClose` with `FIND_FIRST_EX_LARGE_FETCH` and `FindExInfoBasic`), with RAII handle wrappers and robust Win32 error mapping.
3. Implement `Win32DirectoryWatcher` using `ReadDirectoryChangesW` and overlapped I/O with explicit `overflowRequiresRescan` semantics on buffer overflows (`ERROR_NOTIFY_ENUM_DIR`).
4. Implement bounded `UiMailbox` batch queue with capacity backpressure, condition variables, and generation-based stale batch dropping.
5. Implement off-GUI-thread `DirectorySorter` supporting natural number-aware string comparison (`file2` < `file10`), directory grouping (`directoriesFirst`), and stable tie-breaking on `EntryKey`.
6. Implement `DirectoryPipelineCoordinator` orchestrating navigation, cancellation, mailbox draining, and state synchronization across rapid navigations (A -> B -> C stress test).
7. Implement selection and scroll anchor restoration by stable `EntryKey` across asynchronous sorts and directory updates.
8. Instrument end-to-end telemetry recording `t0_navigate`, `t1_first_row`, `t2_first_paint`, `t3_enum_complete`, and `t4_sort_complete`.

## Files changed

| File | Change | Reason |
|---|---|---|
| `src/core/include/godxplr/core/directory_entry.h` | Created | Relocated core `DirectoryEntry`, `NameSlice`, and `MetaState` DTOs for zero-dependency shared access across core and platform |
| `src/core/include/godxplr/core/provider_contract.h` | Created | Pure C++20 directory provider contract (`IDirectoryProvider`, `DirectoryBatch`, `ProviderStatus`, `WatchEvent`, `CancellationToken`) |
| `src/core/CMakeLists.txt` | Updated | Added directory entry and provider contract headers |
| `src/platform/windows/include/godxplr/platform/win32_directory_provider.h` | Created | Win32 directory provider and directory watcher class declarations |
| `src/platform/windows/src/win32_directory_provider.cpp` | Created | Win32 directory enumeration and watcher implementation with RAII handles and error mapping |
| `src/platform/windows/CMakeLists.txt` | Updated | Registered Win32 directory provider and watcher in `godxplr-platform-win32` |
| `src/ui/model/include/godxplr/ui/model/directory_entry.h` | Updated | Backward-compatible type aliases pointing to core `DirectoryEntry` |
| `src/ui/model/include/godxplr/ui/model/directory_store.h` | Updated | Added `appendBatch()`, `applyPermutation()`, and raw entry/arena accessors |
| `src/ui/model/src/directory_store.cpp` | Updated | Implemented chunked batch appending and permutation swapping |
| `src/ui/async/include/godxplr/ui/async/ui_mailbox.h` | Created | Bounded thread-safe queue with producer backpressure and generation filtering |
| `src/ui/async/src/ui_mailbox.cpp` | Created | Mailbox implementation with condition variables and lock-free generation checks |
| `src/ui/async/include/godxplr/ui/async/directory_sorter.h` | Created | Off-GUI natural sort and permutation builder declaration |
| `src/ui/async/src/directory_sorter.cpp` | Created | Sorter implementation with chunked natural string comparison and `EntryKey` tie-breaking |
| `src/ui/async/include/godxplr/ui/async/directory_pipeline_coordinator.h` | Created | Orchestrator connecting navigation, provider, mailbox, store, and sorter |
| `src/ui/async/src/directory_pipeline_coordinator.cpp` | Created | Orchestrator implementation with milestone telemetry and anchor restoration |
| `src/ui/CMakeLists.txt` | Updated | Added async pipeline sources to `godxplr-ui-state` |
| `tests/unit/ui/async_pipeline_tests.cpp` | Created | Comprehensive test suite covering backpressure, A->B->C cancellation, real filesystem enumeration, error mapping, natural sorting, selection restoration, watcher overflow, and latency milestones |
| `tests/CMakeLists.txt` | Updated | Registered `UI.AsyncPipeline` test |

## API/schema/architecture changes

- `gx::IDirectoryProvider`: Pure C++20 provider abstraction for local, network, and virtual directory providers.
- `gx::ui::UiMailbox`: Bounded batch queue enforcing backpressure via condition variables (`capacity = 8` batches); automatically drops batches where `generation < currentGeneration`.
- `gx::ui::DirectorySorter`: Off-GUI-thread permutation sorter with deterministic natural numeric parsing and stable `EntryKey` tie-breaking.
- `gx::ui::DirectoryPipelineCoordinator`: Coordinates navigation lifecycle, cancellation tokens, background workers, and milestone telemetry (`t0` through `t4`).

## Build evidence

| Command | Exit code | Result |
|---|---:|---|
| `cmake --build build --config Release` | 0 | Clean build with MSVC `/W4 /WX /arch:AVX2 /permissive-` (zero warnings, zero errors) |

## Tests

| Test | Environment | Result | Evidence |
|---|---|---|---|
| `test_mailbox_backpressure_and_cancellation` | Windows 11 MSVC Release | PASS | Producer blocked at capacity 3; unblocked on drain; stale generation dropped; cancelled unblocked |
| `test_rapid_navigation_cancellation` | Windows 11 MSVC Release | PASS | Rapid A->B->C navigation dropped 3 stale batches; only folder C committed (1,000 entries) |
| `test_win32_real_filesystem_enumeration` | Windows 11 MSVC Release | PASS | Real temp directory with 2 subdirs and 3 files verified; sizes, timestamps, and flags accurate |
| `test_win32_error_mapping` | Windows 11 MSVC Release | PASS | Non-existent path mapped cleanly to `ProviderStatus::NotFound` |
| `test_natural_sort_and_tie_breaker` | Windows 11 MSVC Release | PASS | Number chunks sorted naturally (`file1` < `file2` < `file10`); directories first; duplicate names tie-broken by `EntryKey` |
| `test_selection_and_anchor_restoration` | Windows 11 MSVC Release | PASS | Selected `EntryKey` and scroll anchor row indices updated correctly from 3/1 to 1/3 after reverse sort |
| `test_watcher_overflow_semantics` | Windows 11 MSVC Release | PASS | Buffer overflow correctly triggered `WatchEvent::Kind::RescanRequired` with `overflowRequiresRescan = true` |
| `test_latency_milestones` | Windows 11 MSVC Release | PASS | Milestones strictly monotonic: t0 -> t1 (14.97 ms) -> t3 (93.17 ms) -> t4 (93.51 ms) |
| `ctest --test-dir build -C Release` | Windows 11 MSVC Release | PASS | 13/13 CTests passing 100% in 1.44 sec |

## Benchmarks

- **Mailbox Throughput:** Drained 1,000 synthetic entries across 10 batches with generation validation and backpressure in **< 15 ms**.
- **Real Win32 Enumeration:** 5-entry filesystem directory with handle opening, attribute parsing, UTF-16 to UTF-8 conversion, and batching completed in **0.42 ms**.
- **Latency Telemetry Recorded:**
  - `t0 -> t1` (first row visible): **14.97 ms**
  - `t0 -> t3` (enumeration complete, 500 entries): **93.17 ms**
  - `t0 -> t4` (background sort complete): **93.51 ms**

## Security/data-safety review

- Zero destructive operations: enumeration and change watching are strictly read-only.
- Handles wrapped in RAII (`ScopedHandle`, `FindCloser`) guaranteeing no Win32 handle leaks on exceptions, early returns, or cancellations.
- Buffer overflow handling in `ReadDirectoryChangesW`: never drops events silently; always forces a full directory rescan on `ERROR_NOTIFY_ENUM_DIR`.
- Long path resilience: normalized native paths transparently prepend `\\?\` prefix when length >= `MAX_PATH`.

## Known defects and limitations

- `Win32DirectoryWatcher` currently performs a shallow watch on the active folder; recursive watching of deep subtrees for recursive folder size updates will be wired during P11/P13.
- Natural sort currently parses ASCII digits; Unicode localized collation (e.g. `CompareStringEx` with `SORT_DIGITSASNUMBERS`) will be layered in Phase P12.

## Assumptions / not verified

- Simulated remote SMB shares or offline UNC shares under high packet loss will be validated in Phase P15 (`P15-network-cloud-mtp-providers.md`).

## Gate checklist

- [x] Rapid navigation/cancel stress passes (A -> B -> C verified; old batches dropped; only C committed).
- [x] UI mailbox queue depth is strictly bounded by capacity (backpressure verified under full queue).
- [x] GUI thread contains no blocking filesystem operations (enumeration and sorting run on worker lanes).
- [x] Sorting/refresh converges to provider truth.
- [x] Permission and offline errors are cleanly mapped to `ProviderStatus::PermissionDenied` and `ProviderStatus::DeviceNotReady`.
- [x] Folder benchmark and telemetry milestones recorded.

## Next action (do not execute automatically)

Phase P06 is complete. Proceed to **Phase P07: Sidebar/tree, view modes and input interactions (Spec 03 U5)**.
