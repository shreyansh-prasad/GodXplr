# GodXplr — Integrated Architecture Overview

**Document ID:** GX-ARCH-OVERVIEW  
**Phase:** P01 — Integrated architecture and requirements baseline  
**Date:** 2026-11-03  
**Status:** ACCEPTED BASELINE  
**Author:** Antigravity / Gemini 3.8 Flash High (Principal OS Architect)  

---

## 1. System Process & Security Topology

GodXplr decomposes the file management problem into isolated, least-privileged operating system processes to guarantee that a failure or crash in one subsystem never destabilizes the primary user interface or corrupts user files.

```text
 ┌────────────────────────────────────────────────────────────────────────┐
 │                      SESSION INTERACTIVE BOUNDARY                      │
 │                                                                        │
 │  ┌──────────────────────────────────────────────────────────────────┐  │
 │  │                         godxplr.exe                              │  │
 │  │  • Qt 6 Widgets Shell (Main Window, Tabs, Navigation, Palette)   │  │
 │  │  • FileListView (Custom zero-allocation virtualized view)        │  │
 │  │  • DirectoryStore & FileListModel (Compact UTF-16 Arena)         │  │
 │  │  • Memory-Mapped Ring Buffer Search Client                       │  │
 │  └───────────────┬───────────────────────────────┬──────────────────┘  │
 │                  │ In-Process Library (C++20)    │                     │
 │  ┌───────────────▼──────────────┐                │                     │
 │  │         godxplr-core         │                │                     │
 │  │  • Abstract Provider System  │                │ IPC (Crash Guard)   │
 │  │  • Transaction Intent Log    │                │                     │
 │  │  • Query AST & Normalizer    │                │                     │
 │  └───────────────┬──────────────┘                │                     │
 │                  │                               │                     │
 │                  │ Shared Memory                 ▼                     │
 │                  │ Ring Buffer      ┌───────────────────────────────┐  │
 │                  │ (Queries/Hits)   │     godxplr-shellhost.exe     │  │
 │                  │                  │  • Dedicated COM STA Thread   │  │
 │                  │                  │  • IContextMenu3 Host        │  │
 │                  │                  │  • IShellItem Properties      │  │
 │                  │                  │  • Structured Exception Guard │  │
 │                  │                  └───────────────────────────────┘  │
 │                  │                                                     │
 │                  │ Client Pipe      ┌───────────────────────────────┐  │
 │                  │ \\.\pipe\content │      godxplr-contentd.exe     │  │
 │                  └─────────────────►│  • Signed-in User Process     │  │
 │                                     │  • Roaring Bitmaps Index      │  │
 │                                     │  • Work Queue & Scheduler     │  │
 │                                     └───────────────┬───────────────┘  │
 │                                                     │ Duplicated Handle│
 │                                                     ▼ (Job Object)     │
 │                                     ┌───────────────────────────────┐  │
 │                                     │      godxplr-extract.exe      │  │
 │                                     │  • Low-Integrity / No Network │  │
 │                                     │  • PDFium, libarchive, OOXML  │  │
 │                                     │  • Memory cap 512 MB, Watchdog│  │
 │                                     └───────────────────────────────┘  │
 └────────────────────────────────────────────────────────────────────────┘
                                     ▲
                     Named Pipe Sync │ Security Descriptor Tokens
                                     ▼
 ┌────────────────────────────────────────────────────────────────────────┐
 │                      ELEVATED SYSTEM SERVICE                           │
 │                                                                        │
 │  ┌──────────────────────────────────────────────────────────────────┐  │
 │  │                     godxplr-indexd.exe                           │  │
 │  │  • Runs as NT AUTHORITY\SYSTEM (SeBackupPrivilege)               │  │
 │  │  • Volume Handles (\\.\C: with FILE_FLAG_NO_BUFFERING)           │  │
 │  │  • FSCTL_ENUM_USN_DATA / FSCTL_READ_USN_JOURNAL                 │  │
 │  │  • Contiguous SoA In-Memory File Table (5M Capacity)             │  │
 │  │  • Cached $Secure:$SDS Descriptors for in-memory AccessCheck()   │  │
 │  │  • AVX2/AVX-512 SIMD Substring Needle Scanner                    │  │
 │  │  • Shared Memory Host: Global\GodXplr_Search_Shm_<SessionId>     │  │
 │  └──────────────────────────────────────────────────────────────────┘  │
 └────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Principal Data Flows & Workflows

### 2.1 The Instant Search Pipeline ($< 5\text{ ms}$)

```text
User Keystroke in SearchBox ("budget ext:pdf")
  │
  ├─ 1. QueryPlanner parses AST (Term="budget", Ext="pdf")
  ├─ 2. UI increments QueryGeneration (Atomic u64)
  ├─ 3. UI writes query into Shared Memory Ring Buffer
  ├─ 4. Event signaled: SetEvent(hQueryPending)
  │
  ▼ [Context: godxplr-indexd.exe]
  ├─ 5. Worker thread wakes, reads QueryGeneration
  ├─ 6. ExtFilter bitmap lookup: O(1) filter for "pdf"
  ├─ 7. AVX2 SIMD Needle Scan across candidate name arena (32 bytes/cycle)
  ├─ 8. In-Memory AccessCheck():
  │      Validates caller's impersonation token against cached $SDS Security Descriptor
  │      (Duration: ~50 nanoseconds per match, zero disk I/O)
  ├─ 9. Formats top-100 ResultRow structs into Shared Memory output page
  ├─ 10. Signals SetEvent(hResultsReady)
  │
  ▼ [Context: godxplr.exe]
  └─ 11. Viewport repaints visible 30 rows in < 1 ms (No memory allocation)
```

### 2.2 The Asynchronous Directory Navigation Pipeline

```text
User Clicks Folder / Enters Path
  │
  ├─ 1. NavigationCoordinator increments NavigationGeneration
  ├─ 2. Cancels any in-flight requests from the previous folder
  ├─ 3. Submits DirectoryProvider::open(path, generation) to I/O worker
  │
  ▼ [Context: Worker Lane]
  ├─ 4. Uses FindFirstFileExW (Large Fetch + Basic Info)
  ├─ 5. Accumulates records into batches of 512 entries or 8 ms
  ├─ 6. Enqueues DirectoryBatch into Bounded UiMailbox (Capacity = 8 batches)
  │
  ▼ [Context: GUI Thread]
  ├─ 7. UiMailbox drains within a strict 2 ms slice per event-loop turn
  ├─ 8. Appends compact records to DirectoryStore UTF-16 arena
  ├─ 9. Custom FileListView calculates visible rows (e.g., rows 0–35)
  ├─ 10. Schedules QPainter::drawText() for visible items only
  │
  ▼ [Context: Background Sort Worker]
  ├─ 11. Asynchronously sorts entries in 16k chunks using natural sort
  ├─ 12. Publishes new permutation array; UI swaps row order seamlessly
```

### 2.3 The Transactional File Copy Engine (Windows 11 IORING / Overlapped)

```text
User Copies Directory Tree (e.g. 50,000 files in a repo)
  │
  ├─ 1. FileOperationEngine writes intent record to journal (.gxjournal)
  ├─ 2. Probes target volume capabilities (Same volume = atomic move; Cross volume = copy-verify-delete)
  ├─ 3. If Windows 11 Build >= 22000:
  │      Initializes Windows I/O Ring (CreateIoRing) with queue depth = 64
  │      Submits batched CreateFile, ReadFile, and WriteFile descriptors
  ├─ 4. Else:
  │      Dispatches overlapped I/O worker pool with 64KB aligned ring buffers
  ├─ 5. Progress updates dispatched to UI status bar every 100 ms (Smoothed EWMA throughput)
  ├─ 6. Verification pass: Compares file sizes and FILETIME timestamps
  └─ 7. Intent journal committed and closed; native Recycle Bin notified via SHUpdateImage
```

---

## 3. Concurrency & Thread-Affinity Rules

1. **The GUI Thread Invariant:**
   * The GUI thread owns all `QWidget`, `QPainter`, and Qt view/model mutations.
   * **Absolute Prohibition:** No file I/O, no `FindFirstFileW`, no COM apartment calls (`SHGetFileInfo`), no image decoding, no IPC pipe blocking, and no `std::future::get()` on the GUI thread.
2. **The Shell STA Lane:**
   * COM components implementing `IContextMenu`, `IShellFolder`, or `IShellItemImageFactory` are hosted inside dedicated Single-Threaded Apartment (STA) threads (`CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)`) running a Win32 message pump.
3. **The Index Service Concurrency:**
   * Single-writer, multi-reader architecture.
   * Writer thread continuously drains USN journal deltas every 50 ms and updates the SoA in-memory table under a lightweight read-write spinlock.
   * Reader threads service shared-memory query requests using shared reader locks.
