# GodXplr Spec 04 — File Operations Engine

**Consumer:** Antigravity agent, Principal Architect, Platform Engineer, QA/Integration Engineer.  
**Status:** SPECIFICATION BASELINE — Gated for implementation review in Phase P11.  
**Dependency:** Spec 01 (MFT/USN Name Index), Spec 03 (Asynchronous Directory Pipeline), ADR-0023 (Overlapped I/O & IORING streaming).

---

## 0. Subsystem Contract and Non-Negotiable Invariants

| Contract Item | Guarantee / Behavior |
|---|---|
| **Data Safety First** | Zero silent data corruption. Cross-volume moves are strictly **Copy $\to$ Verify $\to$ Delete**. Target file is never replaced until verified. |
| **Atomic Same-Volume Move** | Same-volume rename/move uses `SetFileInformationByHandle(FileRenameInfo)` with atomic kernel metadata update; zero file data bytes read or copied. |
| **Crash Recovery & Journaling** | Every operation writes an intent record to an append-only WAL journal (`godxplr-ops.journal`) before touching the filesystem. Unfinished operations recover or roll back cleanly on reboot. |
| **Undo / Redo Fidelity** | Every non-permanent operation produces a reversible `UndoAction` in a LIFO stack. Undo verifies target state before execution to prevent overwriting user modifications. |
| **Cancellation Boundary** | Cancellation occurs at discrete chunk boundaries ($64\text{ KB}$ to $1\text{ MB}$). Unfinished destination files are scrubbed immediately; partial remnants are never left behind with target names. |
| **Path Length & Unicode** | Full support for Windows extended paths ($\le 32,767$ UTF-16 code units via `\\?\` prefix) and complete Unicode code-point preservation. |
| **NTFS Fidelity** | Preserves standard attributes, timestamps ($100\text{ ns}$ precision), Alternate Data Streams (`:$DATA`), and Security Descriptors (DACLs) where permitted. |
| **Performance (ADR-0023)** | Direct unbuffered Overlapped I/O (`FILE_FLAG_NO_BUFFERING \| FILE_FLAG_WRITE_THROUGH`) for files $> 16\text{ MB}$ to bypass OS file-cache pollution, with fallback to standard Overlapped I/O for small files. |
| **Zero UI Blocking** | All directory walking, file I/O, hash verification, and journal commits execute on background thread pools; progress updates stream to UI via lock-free atomic snapshots at $60\text{ Hz}$. |

---

## 1. Architectural Boundaries and Component Layout

```
 ┌─────────────────────────────────────────────────────────────┐
 │                    UI Shell / Presenter                     │
 └──────────────────────────────┬──────────────────────────────┘
                                │ (Job Request / Progress / Conflict Event)
                                ▼
 ┌─────────────────────────────────────────────────────────────┐
 │                     FileOpsCoordinator                      │
 │  - Operation Dispatcher        - Undo / Redo Manager        │
 │  - Concurrency Controller       - Conflict Policy Engine    │
 └──────────────┬───────────────────────────────┬──────────────┘
                │                               │
                ▼                               ▼
 ┌─────────────────────────────┐ ┌─────────────────────────────┐
 │      OperationJournal       │ │   StreamingFileExecutor     │
 │  - Append-only WAL (Disk)   │ │  - Overlapped / IORING I/O  │
 │  - State: Prepared/Committed│ │  - XXH3-64 Verification     │
 │  - Crash Recovery Engine    │ │  - Space Preallocation      │
 └─────────────────────────────┘ └──────────────┬──────────────┘
                                                │
                                                ▼
 ┌─────────────────────────────────────────────────────────────┐
 │             Platform / Win32 Filesystem Layer               │
 │  - CreateFileW (\\?\ paths)   - SetFileInformationByHandle  │
 │  - IFileOperation (Recycle)   - BackupRead / Streams API    │
 └─────────────────────────────────────────────────────────────┘
```

### 1.1 Process & Thread Model
- **UI Thread:** Dispatches high-level requests (`copy(sources, dest, policy)`), subscribes to atomic progress snapshots, and renders conflict resolution dialogs. Never calls Win32 blocking I/O.
- **Coordinator Worker Thread:** Executes the recursive directory planner, builds the operation DAG, allocates total byte counters, and commits journal intent records.
- **I/O Worker Pool:** Worker threads (default: $1$ per physical storage device / spindle) execute streaming Overlapped file reads/writes with double-buffered circular ring buffers ($1\text{ MB}$ each).

---

## 2. Core Operation Semantics and Workflows

### 2.1 Same-Volume Move / Rename
1. Resolve source and destination volume serial numbers via `GetVolumeInformationByHandleW` or cached volume table.
2. If volumes match:
   - Open source file with `GENERIC_READ | GENERIC_WRITE` and `FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE`.
   - Prepare `FILE_RENAME_INFO` structure with `ReplaceIfExists = FALSE` (or `TRUE` if user policy is `Replace`).
   - Call `SetFileInformationByHandle(hSrc, FileRenameInfo, &renameInfo, sizeof(renameInfo))`.
   - Commit completion to journal. Total latency: $< 50\ \mu\text{s}$.

### 2.2 Cross-Volume Move (Copy $\to$ Verify $\to$ Delete)
1. Pre-allocate destination file size via `SetFileInformationByHandle(hDst, FileAllocationInfo)`.
2. Stream source data into destination using temporary suffix: `DestPath.gxpartial-<UUID>`.
3. Compute inline rolling 64-bit checksum (XXH3-64).
4. Verify destination file integrity against source.
5. Replicate metadata:
   - File times via `SetFileTime` (creation, last access, last write).
   - Attributes via `SetFileAttributesW`.
   - Security descriptor via `GetFileSecurityW` / `SetFileSecurityW`.
   - Alternate Data Streams via `FindFirstStreamW` / `FindNextStreamW` backup stream copying.
6. Atomically rename `DestPath.gxpartial-<UUID>` $\to$ `DestPath`.
7. Delete source file via `DeleteFileW` (or `RemoveDirectoryW`).
8. Commit transaction to journal. If a crash or failure occurs before step 6, the source is untouched and partial destination file is cleaned up.

### 2.3 Copy Pipeline & Space Preallocation
1. **Directory Tree Enumeration:** Recursive pre-scan collecting total item count and byte sum to provide deterministic progress bars.
2. **Space Preallocation:** Call `SetFileInformationByHandle(hDst, FileAllocationInfo)` immediately upon creation. This prevents NTFS allocation fragmentation and verifies disk space availability before reading bytes.
3. **Double-Buffered Overlapped I/O:**
   - Buffer size: $1\text{ MB}$ per chunk, 64-byte aligned for AVX2/AVX-512 operations.
   - For files $> 16\text{ MB}$: use `FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH` to avoid blowing out the Windows standby cache.
   - For files $\le 16\text{ MB}$: use standard cached Overlapped I/O.
4. **Metadata Preservation:**
   - Standard Attributes: `Archive`, `Hidden`, `Read-Only`, `System`.
   - Alternate Data Streams (ADS): All named `$DATA` streams are iterated and copied.
   - Timestamps: Synchronized exactly to original timestamps.

### 2.4 Recycle Bin vs. Permanent Delete
- **Recycle Bin (Default):**
  - Performed using COM `IFileOperation` (`DeleteItems`) with `FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT`.
  - Enables user restoration via Windows Explorer Recycle Bin or GodXplr built-in Recycle Bin view.
- **Permanent Delete (Shift+Delete / Configured):**
  - Iterates directory tree bottom-up (children before parents).
  - Clears `FILE_ATTRIBUTE_READONLY` before unlinking.
  - Calls `DeleteFileW` / `RemoveDirectoryW`.
  - For secure erase (opt-in power feature): writes zero-fill passes across allocated clusters before truncation.

---

## 3. Conflict Detection and Policy Engine

Conflicts occur when the destination path already exists. GodXplr evaluates collisions *before* opening destructive write handles.

```cpp
enum class ConflictResolution : uint8_t {
    AskUser,            // Pause and emit conflict event to UI
    Replace,            // Overwrite destination file
    Skip,               // Leave target intact, skip operation
    KeepBoth,           // Auto-rename (e.g. "Report (2).pdf")
    ReplaceIfNewer,     // Overwrite only if source mtime > dest mtime
    ReplaceIfDifferent  // Overwrite if size differs or mtime differs
};
```

### 3.1 Collision Auto-Renaming Logic (`KeepBoth`)
When `KeepBoth` is selected, the engine decomposes the destination filename:
1. Extract base name and extension: `Report.pdf` $\to$ base `Report`, ext `.pdf`.
2. Scan directory for existing pattern `Report (N).pdf`.
3. Pick smallest unused integer $N \ge 2$.
4. Target becomes `Report (2).pdf`. Zero race condition: opened with `CREATE_NEW` (fails if another process created it concurrently; retries with $N+1$).

---

## 4. Progress, Throttling and Cancellation

### 4.1 Progress Snapshot
Progress is exposed as a lock-free atomic struct updated during I/O loop:

```cpp
struct OperationProgress {
    uint64_t totalBytes{0};
    uint64_t completedBytes{0};
    uint64_t totalFiles{0};
    uint64_t completedFiles{0};
    uint64_t bytesPerSec{0};        // EWMA 500ms window
    uint64_t estimatedRemainingMs{0};
    std::wstring currentSourceFile;
    std::wstring currentDestFile;
    OperationState state{OperationState::Running};
};
```

### 4.2 Pause / Resume
- Execution threads wait on a Win32 manual-reset event (`hResumeEvent`).
- When paused by user: `ResetEvent(hResumeEvent)`. Background workers finish their current in-flight $1\text{ MB}$ chunk, flush state to journal, and wait on `WaitForSingleObject(hResumeEvent, INFINITE)`.
- When resumed: `SetEvent(hResumeEvent)`. Zero data loss, zero re-reading.

### 4.3 Cancellation
- Atomic `std::atomic<bool> isCancelled{false}` checked before every file open and after every chunk write.
- When cancelled:
  - Active destination handle is closed.
  - Incomplete destination file (carrying `.gxpartial` suffix) is immediately deleted via `DeleteFileW`.
  - Progress state transitions to `Cancelled`.
  - Journal entry is marked `RolledBack`.

---

## 5. Operation Journal, Crash Recovery and Undo/Redo

### 5.1 Append-Only WAL Journal Layout (`godxplr-ops.journal`)
Path: `%LOCALAPPDATA%\GodXplr\journal\ops.journal`

```
┌──────────────────────────────────────────────────────────────┐
│ Header: Magic 'GXOP' (4B) | Version (2B) | Reserved (10B)     │
├──────────────────────────────────────────────────────────────┤
│ Entry 1:                                                     │
│   RecordLength (4B) | OpId (8B) | Timestamp (8B)             │
│   OpType (1B: Copy, Move, Rename, Delete, Recycle, Attr)     │
│   State (1B: Planned, InProgress, Committed, RolledBack)     │
│   SourceCount (4B)                                           │
│     [Length (2B) | SourcePath (UTF-16)]                      │
│   DestCount (4B)                                             │
│     [Length (2B) | DestPath (UTF-16)]                        │
│   RollbackData (Length-prefixed payload)                     │
│   CRC32 (4B)                                                 │
├──────────────────────────────────────────────────────────────┤
│ Entry 2 ...                                                  │
└──────────────────────────────────────────────────────────────┘
```

### 5.2 Crash Recovery Workflow
On application startup:
1. Scan `ops.journal`.
2. Inspect entries with state `InProgress`:
   - If `Copy`: Delete any incomplete target files found on disk with `.gxpartial` extension. Mark entry `RolledBack`.
   - If `Move` (cross-volume): Check if destination was fully verified. If verified, delete source; if not verified, delete destination `.gxpartial` and leave source intact. Mark entry `RolledBack`.
   - If `Delete`: No-op (file is already gone or partially deleted; surface notification).
3. Compact journal by archiving committed records $> 30$ days old.

### 5.3 Undo / Redo Contract
- Every successful operation pushes an `UndoAction` to the in-memory LIFO stack.
- **Move Undo:** Moves destination back to source (verifying source does not currently exist).
- **Copy Undo:** Deletes copied destination file (verifying file size and modification time match what was copied).
- **Rename Undo:** Renames destination back to original name.
- **Recycle Undo:** Restores item from Recycle Bin using Shell COM restore API.
- **Permanent Delete:** Flagged as *Non-Undoable*. UI explicitly prompts user with clear warning before dispatching.

---

## 6. Error Handling and Edge Cases

| Error Condition | Win32 Error Code | Engine Response |
|---|---|---|
| **Sharing Violation (Locked file)** | `ERROR_SHARING_VIOLATION` | Retry with exponential backoff ($100\text{ ms}, 300\text{ ms}, 1000\text{ ms}$). If still locked, emit conflict event to UI (`Retry`, `Skip`, `Abort`). |
| **Access Denied** | `ERROR_ACCESS_DENIED` | Query file attributes; if `FILE_ATTRIBUTE_READONLY`, clear bit and retry. If permission failure, offer UAC administrator elevation prompt. |
| **Path Too Long** | `ERROR_BUFFER_OVERFLOW` / `ERROR_FILENAME_EXCED_RANGE` | Automatically prepend `\\?\` prefix for NT path canonicalization ($32,767$ char ceiling). |
| **Disk Disconnected / Offline** | `ERROR_DEVICE_NOT_CONNECTED` | Pause operation immediately; do not drop journal; prompt user to reconnect media. |
| **Disk Full** | `ERROR_DISK_FULL` | Abort preallocation before reading source; prompt user with required space vs available space. |
| **Reparse Points / Symlinks** | `FILE_ATTRIBUTE_REPARSE_POINT` | Default: Copy link target as a symlink/junction, do *not* follow recursively to avoid infinite directory loops. |

---

## 7. Verification and Testing Isolation

### 7.1 Safety Isolation Rules
- **No Test Against Production Drives:** Automated tests are prohibited from targeting user drives (`C:\Users`, `D:\`, etc.).
- **Ephemeral Sandbox Fixtures:** Unit and integration tests create temporary directories in `%TEMP%\GodXplr_OpsTests_<UUID>` and cleanly clean them up in RAII fixture destructors.
- **VHDX Virtual Disk Mounting:** Multi-volume cross-drive copy/move integration tests mount temporary VHDX images via Win32 Virtual Disk API (`virtdisk.dll`).

### 7.2 Core Verification Matrix
1. **Atomic Rename Test:** Verifies `FILE_RENAME_INFO` changes name within $1\text{ ms}$ on same volume.
2. **Space Preallocation Test:** Confirms `FileAllocationInfo` allocates physical clusters before bytes are written.
3. **Double-Buffered Overlapped I/O Test:** Streams $100\text{ MB}$ file with concurrent hash verification.
4. **Collision Handling Test:** Validates `KeepBoth`, `Replace`, `Skip`, and `ReplaceIfNewer`.
5. **Cancellation Scrub Test:** Simulates cancel at $50\%$ completion; verifies `.gxpartial` target is wiped and source is untouched.
6. **Journal Recovery Test:** Injects crash mid-operation; verifies restart cleanly recovers and rolls back.
7. **Undo Fidelity Test:** Executes Copy $\to$ Undo $\to$ Redo sequence and verifies bit-for-bit file equality.

---

## 8. Implementation Build Order (P11 Stages)

- **Step 1 (Spec 04 Acceptance):** Formal review and acceptance of this specification.
- **Step 2 (Journal & Primitives):** Implement `OperationJournal`, WAL persistence, and crash recovery.
- **Step 3 (Streaming Engine):** Implement `StreamingFileExecutor`, Overlapped I/O, and space preallocation.
- **Step 4 (Coordinator & Conflicts):** Implement `FileOpsCoordinator`, conflict policies, and undo/redo stack.
- **Step 5 (Integration & VHDX Tests):** Verify full test suite on Windows temporary fixtures and VHDX volume boundaries.

