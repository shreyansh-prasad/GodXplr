# Phase P08 — Completion Report

- **Date:** 2026-11-04
- **Commit:** Pending
- **Status:** PASS
- **Active substep:** Spec 03 U6–U7 (Icons, thumbnails, caches, previews, inspector and scroll prioritization)

## Objective and scope

Implement the asynchronous decoration, thumbnail, and preview pipeline adhering to zero GUI-thread blocking and strict memory budgets:
1. **Core Value Types & Bomb Defense:** Defined `PixelBuffer` in `gx::core` with bounds checking, box downscaling (`downscaleTo`), and physical decompression bomb limits (`kMaxImageDimension = 8192`, `kMaxImageDecodeBytes = 32 MB`).
2. **Cache Keys & Invalidation:** Defined `IconKey` (extension/category, directory/drive flags, size bucket, DPI bucket, theme mode) and `ThumbnailKey` (EntryKey, mtime, size, target dimensions, crop mode, DPI bucket) with custom `std::hash` specializations.
3. **Bounded Caches:**
   - Implemented `IconCache` as a thread-safe LRU cache bounded by entry count (default 2,048) with hit/miss/eviction metrics.
   - Implemented `ThumbnailCache` as a thread-safe LRU cache tracking exact memory footprint in bytes, enforcing the 96 MB budget, and supporting adaptive `shrinkToBudget()` under memory pressure.
4. **Off-GUI COM Apartment Isolation:**
   - Implemented `ComInitializer` for RAII COM lifecycle (`COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE`).
   - Implemented `StaWorkerThread`: dedicated background STA thread with active Win32 message pumping (`PeekMessageW` / `DispatchMessageW`), ensuring Shell COM handlers execute off the GUI thread.
5. **Windows Shell Thumbnail & Icon Provider:**
   - Implemented `ShellThumbnailProvider::extractThumbnail` using `SHCreateItemFromParsingName` and `IShellItemImageFactory::GetImage` with flags `SIIGBF_RESIZETOFIT`, `SIIGBF_INCACHEONLY`, `SIIGBF_MEMORYONLY`, `SIIGBF_THUMBNAILONLY`, and `SIIGBF_BIGGERSIZEOK`.
   - Converted GDI bitmaps/DIBs to 32-bit ARGB `PixelBuffer` with automatic opaque alpha correction for RGBX shell handlers, avoiding GDI resource leaks.
   - Implemented `ShellThumbnailProvider::extractIcon` via `SHGetFileInfoW` (`SHGFI_ICON | SHGFI_USEFILEATTRIBUTES`) for fast directory/file type representations without reading file contents.
6. **Decoration Request Scheduler:**
   - Implemented `DecorationScheduler` with priority tiers (`VisibleNow`, `NearViewport`, `BackgroundTab`).
   - Request deduplication by key.
   - Viewport pruning: drops queued thumbnail/icon requests outside the visible row range during rapid scrolling.
   - Generation invalidation: increments monotonically on folder navigation, dropping stale responses.
7. **Preview & Inspector Model:**
   - Implemented `PreviewModel` extracting synchronous file metadata (size, created/modified/accessed timestamps, Win32 attributes, MIME/file categorization) and decoupled high-resolution preview images (512x512) off-thread without stalling navigation.

## Files changed

| File | Change | Reason |
|---|---|---|
| `src/core/include/godxplr/core/pixel_buffer.h` | Created | Pure C++20 domain value type for 32-bit ARGB images with decompression bomb rejection and box downscaling |
| `src/ui/cache/include/godxplr/ui/cache/cache_keys.h` | Created | `IconKey` and `ThumbnailKey` structs with `std::hash` specializations |
| `src/ui/cache/include/godxplr/ui/cache/pixel_buffer.h` | Created | Forwarding header exposing `PixelBuffer` in `gx::ui` |
| `src/ui/cache/include/godxplr/ui/cache/icon_cache.h` | Created | Thread-safe bounded LRU icon cache header |
| `src/ui/cache/src/icon_cache.cpp` | Created | LRU cache implementation tracking entry counts, hits, misses, and evictions |
| `src/ui/cache/include/godxplr/ui/cache/thumbnail_cache.h` | Created | Thread-safe bounded LRU thumbnail cache header tracking memory byte budget |
| `src/ui/cache/src/thumbnail_cache.cpp` | Created | Thumbnail cache implementation tracking byte count, 96 MB budget enforcement, and `shrinkToBudget` |
| `src/platform/windows/include/godxplr/platform/com_apartment.h` | Created | Win32 STA apartment initializer and `StaWorkerThread` message-pump worker header |
| `src/platform/windows/src/com_apartment.cpp` | Created | Background STA worker thread loop pumping Win32 messages for COM callbacks |
| `src/platform/windows/include/godxplr/platform/shell_thumbnail_provider.h` | Created | Shell COM extraction provider header |
| `src/platform/windows/src/shell_thumbnail_provider.cpp` | Created | Native Windows thumbnail and icon extraction implementation via `IShellItemImageFactory` and `SHGetFileInfoW` |
| `src/platform/windows/CMakeLists.txt` | Updated | Added COM apartment, thumbnail provider, and `gdi32` link library |
| `src/ui/async/include/godxplr/ui/async/decoration_scheduler.h` | Created | Prioritized request broker and viewport pruning scheduler header |
| `src/ui/async/src/decoration_scheduler.cpp` | Created | Request scheduler implementation with priority ordering, viewport pruning, and generation cancellation |
| `src/ui/shell/include/godxplr/ui/shell/preview_model.h` | Created | Inspector/preview panel model header with metadata and decoupled preview image |
| `src/ui/shell/src/preview_model.cpp` | Created | Inspector implementation with file categorization, formatting, and non-blocking preview loading |
| `src/ui/CMakeLists.txt` | Updated | Added `cache/` and `preview_model` to `godxplr-ui-state` |
| `tests/unit/ui/icon_thumbnail_tests.cpp` | Created | 7 comprehensive unit and integration test suites covering all U6–U7 requirements |
| `tests/CMakeLists.txt` | Updated | Registered `UI.IconThumbnail` CTest |

## API/schema/architecture changes

- `gx::core::PixelBuffer`: Safe ARGB32 image buffer enforcing 8K dimension and 32 MB allocation ceiling.
- `gx::ui::IconKey`: Multi-factor key for symbolic icons (`extensionOrType`, `isDirectory`, `isDrive`, `sizeBucket`, `dpiBucket`, `themeMode`).
- `gx::ui::ThumbnailKey`: Content-freshness key (`entryKey`, `mtime`, `size`, `targetWidth`, `targetHeight`, `cropMode`, `dpiBucket`).
- `gx::platform::win32::StaWorkerThread`: Background STA thread executing COM work without blocking GUI thread.
- `gx::platform::win32::ShellThumbnailProvider`: Off-GUI native extraction for thumbnails and icons.
- `gx::ui::DecorationScheduler`: Manages priority queuing (`VisibleNow`, `NearViewport`, `BackgroundTab`), cache checks, viewport pruning, and generation cancellation.
- `gx::ui::PreviewModel`: File inspector model holding synchronous metadata and asynchronous preview images.

## Build evidence

| Command | Exit code | Result |
|---|---:|---|
| `cmake --build build --config Release` | 0 | Clean build with MSVC `/W4 /WX /arch:AVX2 /permissive-` (zero warnings, zero errors) |

## Tests

| Test | Environment | Result | Evidence |
|---|---|---|---|
| `testCacheKeysAndHashing` | Windows 11 MSVC Release | PASS | `IconKey` and `ThumbnailKey` equality and `std::hash` differentiation verified (freshness stamp changes yield distinct keys) |
| `testPixelBufferAndDecompressionBomb` | Windows 11 MSVC Release | PASS | Valid allocation succeeded; 10,000x100 (> 8K) and 4096x4096x4 (64 MB > 32 MB) rejected immediately; box downscale verified |
| `testIconCacheLru` | Windows 11 MSVC Release | PASS | Capacity 3 evicted oldest item upon 4th insert; hits, misses, and evictions counted accurately |
| `testThumbnailCacheBudgetAndShrink` | Windows 11 MSVC Release | PASS | Exact byte tracking verified (64 KB budget); 5th 16 KB item evicted 1st item; `shrinkToBudget(32KB)` evicted down to 2 items |
| `testComApartmentAndShellExtraction` | Windows 11 MSVC Release | PASS | STA COM initialized; worker submitted and executed async task; native folder and text file icons extracted; non-existent file handled gracefully |
| `testDecorationSchedulerPrioritiesAndPruning` | Windows 11 MSVC Release | PASS | Memory cache hit returned immediately; stale generation request rejected; out-of-view row 100 pruned from queue; generation change cancelled all pending jobs |
| `testPreviewModelInspector` | Windows 11 MSVC Release | PASS | Categories detected (Image, Video, Audio, Document, Code, Archive, Application, Folder); formatted file sizes (KB/MB) and attributes verified; temp file inspected and cleared |
| `ctest --test-dir build -C Release` | Windows 11 MSVC Release | PASS | 15/15 CTests passing 100% in 2.11 sec |

## Open defects and risks

- Windows Shell extraction for third-party extensions will be hosted in `godxplr-shellhost.exe` (ADR-0020) in Phase P12 to guarantee complete crash/hang isolation for buggy shell DLLs.
- Thumbnail disk cache persistence (caching decoded thumbnails to disk database) can be integrated with Spec 02 content cache in Phase P10.

## Phase gate status

**PASS**

## Required next action

Proceed to **Phase P09** (Spec 01 Filename index + search engine integration: IPC pipe connecting `godxplr-indexd` to UI search adapter, query grammar, and ranking evaluation) when authorized.

