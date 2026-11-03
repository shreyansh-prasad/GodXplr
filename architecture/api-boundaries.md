# GodXplr — Canonical Identities and API Boundaries

**Document ID:** GX-ARCH-API-BOUNDARIES  
**Phase:** P01 — Integrated architecture and requirements baseline  
**Date:** 2026-11-03  
**Status:** ACCEPTED BASELINE  
**Author:** Antigravity / Gemini 3.8 Flash High  

---

## 1. Canonical Identities

### 1.1 The Stable Entry Key (`EntryKey`)
Row indices and file paths change constantly during navigation, sorting, renaming, and filtering. To guarantee that selection, open tabs, operation tracking, and thumbnail requests remain stable, every file system object is identified by an immutable `EntryKey`.

```cpp
namespace gx {

enum class ProviderKind : uint8_t {
    NtfsVolume       = 0,
    Win32Directory   = 1,
    ArchiveVirtual   = 2,
    NetworkShare     = 3,
    CloudPlaceholder = 4,
    MtpDevice        = 5,
    SearchResult     = 6
};

struct EntryKey {
    ProviderKind provider{ProviderKind::NtfsVolume};
    uint64_t volumeOrProviderId{0}; // Volume serial number or provider hash
    uint64_t objectId{0};           // NTFS FRN (48-bit index + 16-bit sequence) or inode
    uint64_t incarnation{0};        // Provider generation/instance token

    auto operator<=>(const EntryKey&) const = default;
};

} // namespace gx
```

### 1.2 The 64-Bit File Reference Number (FRN) Contract
* **The Problem:** NTFS FRNs are `uint64_t` numbers where the upper 16 bits represent the sequence number and the lower 48 bits represent the MFT record index. SQLite integer fields are signed 64-bit integers (`int64_t`). If an FRN has the high bit set ($> 2^{63}-1$), standard conversion can cause sign errors or undefined behavior in raw queries.
* **The Contract:**
  1. In C++ domain and wire protocols: Always represent FRN as `uint64_t`.
  2. When binding to SQLite: Cast via `static_cast<int64_t>(frn)`.
  3. When reading from SQLite: Cast via `static_cast<uint64_t>(sqlite3_column_int64(stmt, col))`.
  4. Two's-complement bit patterns are fully preserved across insertion, retrieval, and binary index searches.

---

## 2. Request Generations & Cancellation

Every asynchronous request must carry a monotonically increasing 64-bit generation token.

```cpp
namespace gx {

struct GenerationToken {
    uint64_t id{0};
    std::atomic<bool>* cancelFlag{nullptr};

    [[nodiscard]] bool isCancelled() const noexcept {
        return cancelFlag && cancelFlag->load(std::memory_order_relaxed);
    }
};

} // namespace gx
```

* **Navigation Rule:** When navigating to a new folder, `activeNavGeneration++`. When workers finish enumerating a batch, they compare their origin generation with `activeNavGeneration`. Stale batches are dropped immediately.
* **Search Rule:** When typing in the search box, `activeQueryGeneration++`. Workers checking the search queue abandon obsolete generation scans at chunk boundaries in 0 microseconds.

---

## 3. Shared Memory Search IPC Wire Layout (ADR-0022)

To achieve sub-millisecond query response and eliminate named-pipe context switching during live typing, the UI and `godxplr-indexd` communicate over a dedicated shared memory segment named `Local\GodXplr_Search_Shm_<SessionId>`.

```cpp
#pragma pack(push, 8)
namespace gx::ipc {

constexpr size_t kMaxQueryLen = 512;
constexpr size_t kMaxResultRows = 100;

struct ShmResultRow {
    uint64_t objectId;       // FRN
    uint64_t size;
    uint64_t mtime;          // FILETIME
    uint32_t attributes;
    uint16_t nameLen;        // UTF-8 bytes
    uint16_t dirLen;         // UTF-8 bytes
    char name[260];          // Inline fixed buffer
    char dir[512];           // Inline parent directory path
    float score;
    uint8_t tier;            // 0=exact, 1=prefix, 2=boundary, 3=substring, 4=fuzzy
};

struct ShmSearchBuffer {
    // Header (UI -> Service)
    std::atomic<uint64_t> queryGeneration{0};
    uint32_t queryLen{0};
    char queryText[kMaxQueryLen]{0};
    uint32_t optionsFlags{0}; // caseSensitive, matchPath, fuzzy

    // Response (Service -> UI)
    std::atomic<uint64_t> resultGeneration{0};
    uint32_t resultCount{0};
    uint32_t totalMatches{0};
    uint32_t statusFlags{0}; // 1 = final, 2 = fuzzy pending
    ShmResultRow rows[kMaxResultRows];
};

} // namespace gx::ipc
#pragma pack(pop)
```

---

## 4. Shell Host IPC Protocol (ADR-0020)

`godxplr.exe` communicates with `godxplr-shellhost.exe` over an anonymous local pipe pair with strict JSON or length-prefixed binary frames:

```text
Message Types:
  0x01: QueryContextMenuReq { paths: vector<string>, screenPos: Point, flags: u32 }
  0x02: QueryContextMenuResp { hMenuSerialized: MenuNodeTree, status: u32 }
  0x03: InvokeCommandReq { menuId: u32, verb: string, workingDir: string }
  0x04: InvokeCommandResp { exitCode: u32 }
  0x05: PingReq / PingResp (Heartbeat watchdog: 500 ms interval)
```

If `godxplr-shellhost.exe` fails to respond to `PingReq` within 1,000 ms, `godxplr.exe` forcibly terminates the process via `TerminateProcess` on the child job, closes the pipe, and falls back to a clean internal context menu. The main UI thread never blocks.
