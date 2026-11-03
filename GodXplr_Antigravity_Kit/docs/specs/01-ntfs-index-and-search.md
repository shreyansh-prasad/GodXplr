# GodXplr Spec 01 — NTFS Name Index + Search Engine

Consumer: Antigravity agent. Tags: [Certain] documented Win32/NTFS behaviour; [Likely] engineering inference; [Guessing] estimate, must be benchmarked. Code below is skeleton: **not compiled here**; first agent task is to compile it.

---

## 0. Contract

| Item               | Value                                                   |
| ------------------ | ------------------------------------------------------- |
| Typical / hard cap | 1–2 M / 5 M entries                                     |
| Memory             | ≤ 110 B/entry (5 M ≈ 550 MB, 2 M ≈ 220 MB)              |
| Keystroke latency  | first page ≤ 30 ms @ 1 M entries                        |
| Cold index build   | ≤ 15 s per 1 M files                                    |
| Idle CPU           | < 1 %                                                   |
| Toolchain          | C++20, CMake, MSVC 2022 / clang-cl, **core is Qt-free** |
| Out of scope       | ReFS, content search (only `ContentProvider` hook), AI  |

---

## 1. First principles

**Why a naive explorer search is slow:** it asks the filesystem per directory (open, enumerate, security checks, per-file metadata). ≥ 1 syscall-level round trip per directory, cache-hostile.

**Why Everything is fast [Certain]:** (1) NTFS already stores every file name in the Master File Table (MFT), one sequential structure; reading it is one large sequential I/O. (2) Names are held in RAM in a compact table. (3) Changes arrive from the USN change journal, so no rescans.

**Cost model for a query [Likely]:** 1 M names × \~24 B ≈ 24 MB of folded text. A single-thread SIMD scan runs several GB/s → \~3–8 ms. So brute-force scan of contiguous memory beats any tree/trie for substring search, and costs far less memory than a suffix structure (suffix array ≈ 4–8 B *per character* → \~100–200 MB extra). **Decision: no substring index; scan a contiguous folded arena with SIMD, parallelised, with incremental narrowing.** [Guessing] ms numbers → verify in S6.

---

## 2. NTFS facts the design depends on

1. **File reference number (FRN)** = 64 bit: low 48 = MFT record index, high 16 = sequence number (incremented on record reuse). Parent pointers use the same format. Root dir is record 5. [Certain]
2. **USN journal** per volume: `FSCTL_QUERY_USN_JOURNAL` → `UsnJournalID`, `FirstUsn`, `NextUsn`. If absent: `FSCTL_CREATE_USN_JOURNAL`. [Certain]
3. **`FSCTL_ENUM_USN_DATA`** with `MFT_ENUM_DATA_V1{StartFileReferenceNumber=0, LowUsn=0, HighUsn=NextUsn, MinMajorVersion=2, MaxMajorVersion=2}` returns a buffer: first 8 bytes = next start FRN, then `USN_RECORD_V2` records (FRN, parent FRN, name, attributes). Loop until `ERROR_HANDLE_EOF`. **It gives names/parents/attributes but NOT size or timestamps.** [Certain]
4. **Hard links:** one MFT record can have several `$FILE_NAME` entries → several USN records, same FRN, different parent/name. Treat each as its own entry; key = (FRN, parent FRN, name). [Certain]
5. **Volume handle** `\\.\C:` opened with `GENERIC_READ`, share read/write/delete → requires admin/SYSTEM → the helper service. [Certain]
6. **`FSCTL_READ_USN_JOURNAL`** with `READ_USN_JOURNAL_DATA_V0{StartUsn, ReasonMask, ReturnOnlyOnClose=0, Timeout=0, BytesToWaitFor=1, UsnJournalID}` blocks until new records; output = next USN (8 B) + records. Errors: `ERROR_JOURNAL_ENTRY_DELETED` (StartUsn older than journal start → journal wrapped), `ERROR_JOURNAL_NOT_ACTIVE`, `ERROR_JOURNAL_DELETE_IN_PROGRESS`. [Certain]
7. **Raw MFT record (1 KB typical)**: header `FILE`, **update-sequence fixups** (last 2 bytes of every 512-byte sector are replaced by a sequence value; must be restored before parsing), attributes: `0x10 $STANDARD_INFORMATION` (times, flags), `0x30 $FILE_NAME` (parent, name, namespace; skip namespace 2 = DOS 8.3), `0x80 $DATA` (unnamed = size), `0x20 $ATTRIBUTE_LIST` (large/fragmented files: attributes live in extension records pointing back to a base record). [Certain]

**Consequence:** two enumerators behind one interface:

| Enumerator                                                           | Gives                     | Risk                                                    | When                                                                             |
| -------------------------------------------------------------------- | ------------------------- | ------------------------------------------------------- | -------------------------------------------------------------------------------- |
| `UsnEnumerator` (FSCTL_ENUM_USN_DATA)                                | names, parent, attrs      | very low                                                | **v1**                                                                           |
| `MftRawEnumerator` (parse `$MFT` by data runs, big sequential reads) | + size, times in one pass | medium (fixups, attribute lists, resident/non-resident) | S9, validated differentially against `UsnEnumerator` + a `FindFirstFileEx` crawl |

Until S9, size/date columns are **lazy**: filled by a background worker (`OpenFileById` + `GetFileInformationByHandleEx(FileBasicInfo/FileStandardInfo)`), visible rows prioritised, `kMetaKnown` flag per entry.

---

## 3. Data structures (Structure-of-Arrays)

Entry id = dense `uint32_t`. Parallel columns:

| Column                                                         | Type                                                  | B         |
| -------------------------------------------------------------- | ----------------------------------------------------- | --------- |
| `name_off`                                                     | u32 offset into arena                                 | 4         |
| `name_len`                                                     | u16 (UTF-8 bytes)                                     | 2         |
| `parent`                                                       | u32 entry id (root = self)                            | 4         |
| `frn`                                                          | u64                                                   | 8         |
| `size`                                                         | u64                                                   | 8         |
| `mtime`                                                        | u64 FILETIME                                          | 8         |
| `flags`                                                        | u8 (dir, hidden, system, reparse, deleted, metaKnown) | 1         |
| `letters`                                                      | u32 bitmask a–z, 0–9 present in folded name           | 4         |
| `(off,id)` pairs for reverse lookup                            | u32+u32                                               | 8         |
| FRN→id hash (open addressing, load 0.5)                        |                                                       | \~16      |
| name bytes: **original** + **folded** (same length) + NUL each | avg 24+24+2                                           | \~50      |
| **Total**                                                      |                                                       | **≈ 113** |

Slightly over 100 B; budget raised to 110–115. [Guessing] Drop `letters` (−4) if S6 shows fuzzy prefilter isn't worth it.

**Folded arena rule:** fold is *byte-length-preserving*. ASCII: `|0x20` for A–Z. Non-ASCII: simple case fold via table **only if UTF-8 length is unchanged**; else leave the character as is (documented limitation: e.g. `İ`).

**Arena layout:** `name\0name\0…`. NUL separator guarantees a needle (never contains NUL) cannot span two names.

**Reverse lookup (hit offset → entry id):** sorted vector of `(off,id)`; appends are monotonic so it stays sorted. Validate hit: `name_off[id]==off`, else stale → skip.

**Mutations**

- *Add:* append name to both arenas, append columns, insert FRN hash, append pair.
- *Delete:* flag `kDeleted`, poison both arena regions with byte `0x01` (no needle matches), push id on free list. Children of a deleted dir are deleted by their own USN records; if absent, reaped in compaction by orphan check.
- *Rename (same parent):* append new name, update `name_off/len`, poison old, append new pair.
- *Move (parent change):* just rewrite `parent[id]`. **Whole subtree paths update implicitly** — this is why parent pointers beat stored paths.
- *Compaction:* when poisoned bytes > 20 % of arena or tombstones > 20 % of entries, rebuild arenas and columns in a background copy, swap under writer lock. Ids are remapped; all caches invalidated. [Likely]

**Concurrency [Likely]:** single writer (journal applier), many readers. Reader–writer scheme: readers take a shared `epoch`-protected view; writer batches deltas (≤ 50 ms or 2000 ops) and applies under exclusive lock for a few hundred µs. Queries abort and retry if compaction swaps (generation counter).

**Path build:** walk `parent` to root, reverse. Only for visible rows (≈ 50) and for `path:` filters. For `path:` over many rows, memoise `dir_id → path_hash/len` per query.

---

## 4. Core API (Qt-free, C++20)

```cpp
// src/core/api/search_types.h
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
namespace gx {

using EntryId = uint32_t;
inline constexpr EntryId kNoEntry = 0xFFFFFFFFu;
enum EntryFlag : uint8_t { kDir=1, kHidden=2, kSystem=4, kReparse=8, kMetaKnown=16, kDeleted=128 };
enum class SortKey : uint8_t { Name, Path, Size, Modified, Extension, Rank };
enum class SortDir : uint8_t { Asc, Desc };

struct QueryOptions {
  SortKey sort = SortKey::Rank; SortDir dir = SortDir::Desc;
  uint32_t offset = 0, count = 100;         // window the UI shows
  bool fuzzy = true, matchPath = false, caseSensitive = false;
  uint32_t maxFuzzy = 200;                  // top-K for fuzzy tier
};

struct ResultRow {
  EntryId id; float score; uint8_t tier;    // tier 0=exact … 4=fuzzy
  uint64_t size; uint64_t mtime; uint8_t flags;
  std::string name; std::string dir;        // UTF-8; filled for window only
};

struct ResultPage {
  uint64_t queryId; uint32_t total;         // total exact+substring matches
  bool totalIsFinal; bool fuzzyPending;     // fuzzy tier arrives later
  std::vector<ResultRow> rows;
};

struct IResultSink { virtual ~IResultSink()=default;
  virtual void onPage(const ResultPage&) = 0; };   // may be called 1..3 times per query

// src/core/api/search_engine.h
class ISearchEngine {
 public:
  virtual ~ISearchEngine()=default;
  virtual uint64_t submit(std::string_view utf8Query, const QueryOptions&, IResultSink&) = 0;
  virtual void cancel(uint64_t queryId) = 0;       // also implied by a newer submit
  virtual void recordOpen(EntryId id) = 0;         // feeds usage DB
};

// src/index/volume_enumerator.h
struct RawEntry { uint64_t frn, parentFrn; std::u16string name; uint32_t attrs;
                  uint64_t size=0, mtime=0; bool metaKnown=false; };
struct IVolumeEnumerator { virtual ~IVolumeEnumerator()=default;
  // calls sink with batches of ~16k entries; returns journal cursor for tailing
  virtual bool enumerate(std::function<void(const std::vector<RawEntry>&)> sink,
                         struct JournalCursor& out) = 0; };

struct JournalCursor { uint64_t journalId=0, nextUsn=0; };
enum class DeltaOp : uint8_t { Add, Remove, Rename, Move, Attr };
struct Delta { DeltaOp op; RawEntry e; uint64_t oldParentFrn=0; };
struct IJournalTail { virtual ~IJournalTail()=default;
  // blocks; returns false => cursor invalid, caller must full-rebuild
  virtual bool next(std::vector<Delta>& out, JournalCursor& cur, std::atomic<bool>& stop)=0; };
} // namespace gx

```

---

## 5. Index build + journal tail (service pseudocode)

```
BuildVolume(vol):
  h = CreateFileW(\\.\X:, GENERIC_READ, SHARE_READ|WRITE|DELETE, ...)
  q = FSCTL_QUERY_USN_JOURNAL(h)          # if NOT_ACTIVE -> FSCTL_CREATE_USN_JOURNAL, retry
  cursor = {q.UsnJournalID, q.NextUsn}    # capture BEFORE enumerating
  start = 0
  loop:
    n = DeviceIoControl(h, FSCTL_ENUM_USN_DATA, &{start,0,cursor.nextUsn,2,2}, buf 1 MiB)
    if err == ERROR_HANDLE_EOF: break
    start = *(u64*)buf
    for rec in records(buf+8): if V2: batch.push(frn,parent,name,attrs)
    if batch >= 16k: index.bulkAppend(batch)
  index.resolveParents()                  # FRN -> id; unknown parent => orphan list
  index.buildSortOrders() async
  Persist(snapshot, cursor)
  StartTail(cursor)

Tail(cursor):          # separate thread per volume
  while !stop:
    r = DeviceIoControl(h, FSCTL_READ_USN_JOURNAL, {cursor.nextUsn, mask, 0, 0, 1, journalId})
    if err in (JOURNAL_ENTRY_DELETED, JOURNAL_NOT_ACTIVE, ID mismatch): raise NeedRebuild
    cursor.nextUsn = *(u64*)buf
    for rec: coalesce(rec)
    flush deltas every 50 ms

```

**Capturing `NextUsn` before enumeration** guarantees no lost changes; changes made *during* enumeration are replayed from the tail. Replay must be **idempotent** (Add of an existing (FRN,parent,name) = no-op). [Likely]

**Record → delta rules** (reason flags): `FILE_CREATE` → Add; `FILE_DELETE` → Remove; `RENAME_OLD_NAME` → remember `(frn, oldParent, oldName)`; `RENAME_NEW_NAME` → if parent same → Rename else Move+Rename; `BASIC_INFO_CHANGE/DATA_*` with `CLOSE` → Attr/meta refresh (size, mtime re-read lazily). Coalesce by FRN within a batch; the last state wins. Filter: ignore `SourceInfo` bits only if our own tooling writes (none in v1).

**Orphans:** a parent FRN not yet seen goes in `pendingChildren[parentFrn]`; resolved when parent arrives. Sequence-number mismatch (stale parent FRN) → treat as orphan, drop at end of build. [Likely]

**Failure modes**

| Failure                                      | Detection                                             | Response                                                              |
| -------------------------------------------- | ----------------------------------------------------- | --------------------------------------------------------------------- |
| Journal wrapped while service was off        | `ERROR_JOURNAL_ENTRY_DELETED` or `nextUsn < FirstUsn` | full rebuild (background; old snapshot keeps serving, marked stale)   |
| Journal deleted/recreated                    | `UsnJournalID` differs                                | full rebuild                                                          |
| Volume removed (USB)                         | `ERROR_FILE_NOT_FOUND/DEVICE_NOT_READY`               | drop volume entries, keep volume record                               |
| Disk with no USN support (FAT/exFAT/network) | `ERROR_INVALID_FUNCTION`                              | `CrawlerVolume` with `ReadDirectoryChangesW`; overflow ⇒ re-crawl dir |
| Service crash mid-write                      | snapshot tmp file never renamed                       | previous snapshot valid; replay from its cursor                       |
| Memory cap hit (5 M)                         | counter                                               | stop adding, set UI banner "index limited"                            |
| Rename storm (build tools)                   | delta rate > 20k/s                                    | enlarge batch interval to 250 ms, never drop                          |

---

## 6. Query language

**Grammar** (Everything-compatible superset; verify exact Everything semantics in P00 against its docs) [Likely]:

```
or      := and ('|' and)*
and     := unary (WS unary)*
unary   := '!' unary | primary
primary := '<' or '>'  |  '"' phrase '"'  |  func ':' value  |  term
func    := ext size dm dc da path parent regex case nocase wholeword file folder len attrib content

```

Precedence: `!` > AND (space) > OR (`|`). `<…>` groups. Wildcards `*` `?` in terms. `content:` compiles to a `ContentProvider` call (stub returns empty).

**Values:** `size:>10mb`, `size:1kb..1mb`, `dm:today`, `dm:2026-01..2026-03`, `dm:last2weeks`, `ext:jpg;png`.

**Pipeline:** `Lexer → Parser(AST) → Normalizer → Planner → Executor`.

- **Normalizer:** flatten AND/OR, push NOT down (De Morgan), fold constants, lower-case terms unless `case:`.
- **Planner:** each leaf → `Predicate{cost, selectivity, kind}`. Kinds and cost (cheapest first): 
  1. `ExtEq` → ext bitmap/id-list lookup (ext hash → sorted ids) \~ O(matches)
  2. `Flag` (file/folder/attrib) \~ column test
  3. `Size/Date` \~ column test (needs `kMetaKnown`; unknown rows are *included* and flagged "metadata pending")
  4. `Substr/Phrase` \~ SIMD arena scan
  5. `Wildcard` → longest literal run scanned as `Substr`, remainder verified per hit
  6. `Path` → parent-chain match (expensive, last)
  7. `Regex` → RE2 on names, prefiltered by a required literal extracted from the pattern if any (RE2: linear time, no ReDoS; BSD-3 license [Likely]) AND order: cheap + selective first so later predicates only evaluate on survivors. OR = union of sorted id vectors. NOT = bitmap complement (only evaluated within the candidate set of the other AND siblings; a pure-NOT query scans all).
- **Candidate representation:** sorted `vector<EntryId>` if < N/16, else a bitmap (N bits = 125 KB per 1 M). Convert by threshold.

### 6.1 Incremental narrowing (the main latency trick)

Keep a **query stack** keyed by normalized AST. If the new query = old query AND extra restriction, or a single substring term was extended (`ab` → `abc`), evaluate only against the previous result set. Backspace pops the stack. Not valid for OR / NOT-containing changes → full scan. [Likely]

### 6.2 SIMD substring scan

Scan the *folded arena* as one buffer (not per entry):

1. Generic SIMD: broadcast first byte and last byte of needle; AVX2 compare 32 positions each, AND the masks, verify survivors with `memcmp`. SSE2 fallback; scalar fallback with `memchr`. Runtime dispatch via CPUID. [Likely]
2. Hit at offset h → reverse lookup via binary search on `(off,id)` → entry id; discard stale/poisoned.
3. Skip to the next NUL after a hit so each entry reports once.
4. Parallelise: split arena into \~1 MB chunks **cut on NUL boundaries**, thread pool of `cores-1`, per-thread local vectors, k-way merge by id.
5. Check `cancelGen` every chunk.

Case-sensitive (`case:`) scans the **original** arena instead (same layout, same offsets).

### 6.3 Sorting and paging

- Per-column **sort permutations** (`u32[N]`, 4 B/entry each) built lazily in the background for Name, Path, Size, Modified, Extension. Memory only when used; LRU-evict if > 2 columns resident and RAM tight. [Likely]
- Result < \~50 k: sort the ids directly (`nth_element`/partial sort for the window). Result large: iterate the permutation, test membership in the bitmap, stop after `offset+count` hits. [Likely]
- UI asks windows `(offset,count)`; engine never materialises strings beyond the window.

---

## 7. Fuzzy + ranking

**Tiers** (always ranked in this order; fuzzy never outranks substring):
0 exact name · 1 name prefix · 2 word-boundary substring · 3 any substring · 4 fuzzy.

**Fuzzy pass (runs after exact pass, cancellable, streams as a 2nd/3rd page):**

1. Prefilter: needle letter mask `m`; candidate must satisfy `(letters[i] & m) == m` (SIMD 8 lanes/AVX2). Removes most entries. [Guessing]
2. Subsequence test on folded name (linear).
3. Score survivors fzf-style: +base per matched char; bonus for consecutive, for start-of-name, after separators (`._-/ ` and space), for camelCase/digit transitions; gap-open and gap-extend penalties; prefer shorter names. Keep best K in a bounded heap (`maxFuzzy`).
4. Typo tolerance: for query tokens of length 4–8 only, accept names whose *stem* has length within ±1 and Damerau-Levenshtein ≤ 1 (transposition counts as 1). Candidates drawn from entries passing the letter-mask test with popcount(difference) ≤ 2. Score below any subsequence match. [Guessing] → benchmark; disable if > 15 ms.

**Rank score within a tier:**
`score = matchScore + w_r·exp(-age_days/14) + w_f·log1p(openCount) + w_d·1/(1+depth) − w_l·nameLen`
Weights live in `config/ranking.toml`; defaults: `w_r=3, w_f=2, w_d=0.5, w_l=0.01`. Tie-break: shorter full path, then id. [Guessing] tune on a hand-labelled set of 200 queries (S10).

**Usage DB:** SQLite (WAL) `usage(path_hash INTEGER PRIMARY KEY, open_count, last_open, last_query_prefix)`. Keyed by hash of full path, not id (ids change on compaction). Loaded lazily; the ranker reads via an LRU map for result candidates only. Never indexes by FRN (reuse). Privacy: local, user-clearable, off switch.

---

## 8. Service, IPC, security

**Decision change vs the earlier report [Likely]:** the index lives in **one process** (service) and the UI queries over IPC, instead of sending the full snapshot to the UI. Reasons: halves RAM (no second 220–550 MB copy), makes permission filtering enforceable, and a UI crash doesn't reload the index. Cost: ≈ 0.1 ms pipe round trip per keystroke (negligible vs 30 ms). Portable mode links the same engine in-process. **Veto point (see §13).**

**Pipe:** `\\.\pipe\godxplr-index-v1`, `PIPE_TYPE_MESSAGE|PIPE_REJECT_REMOTE_CLIENTS`, DACL: allow `Interactive` (IU) read/write, deny `Network` (NU); first instance `FILE_FLAG_FIRST_PIPE_INSTANCE` to prevent squatting.

**Framing:** `u32 len | u16 type | u16 flags | u64 reqId | payload`. Types: `Hello{protoVer}`, `Query{text,opts}`, `Page`, `Cancel`, `Status{volumes,state}`, `RecordOpen`. Reject frames > 4 MiB; versioned; unknown type = close.

**Permission model:**

- Service runs with minimum privileges needed: `SeBackupPrivilege`-style volume read only; no network; service SID restricted token. [Likely]
- On each page: `ImpersonateNamedPipeClient`, for each of the ≤ 100 visible rows check access to the **parent directory** (`CreateFileW(dir, FILE_LIST_DIRECTORY, …, FILE_FLAG_BACKUP_SEMANTICS)` under impersonation, cached per `dirId` for 5 s), `RevertToSelf`. Rows failing the check are dropped and replaced by the next ones. `total` is labelled approximate when any row was filtered.
- Service never exposes file *contents*, only names + metadata.
- Content index (spec 02) runs as the user, not here.

**Crash handling:** UI keeps a watchdog; if the pipe dies, the UI falls back to in-process crawler mode for the open folder and shows a banner.

---

## 9. Persistence

File: `%ProgramData%\GodXplr\index\<volumeSerialHex>.gxi`

```
Header: magic "GXIX" | fmtVer | volSerial | journalId | nextUsn | entryCount | arenaBytes | crc32 per section
Sections (each 64-byte aligned): name_off, name_len, parent, frn, size, mtime, flags, arena_orig, arena_fold

```

- `letters`, hash, `(off,id)` pairs and sort permutations are **rebuilt on load** (cheap, \~100–300 ms per 1 M [Guessing]); not persisted.
- Write: temp file → `FlushFileBuffers` → `MoveFileExW(MOVEFILE_REPLACE_EXISTING|WRITE_THROUGH)`. Triggers: after full build, every 10 min if dirty, on service stop.
- Load: verify header + CRCs → read whole file into the heap (SSD ≈ 100–200 MB/s·ms scale [Guessing]); mismatch ⇒ rebuild. Then `Tail` from saved `nextUsn` (replay gap).
- Journal-ID mismatch or volume serial mismatch ⇒ discard file.
- Format version bump ⇒ discard (index is a cache; never migrate).
- Disk budget: \~ 60–70 B/entry.

---

## 10. Non-NTFS and portable mode

`CrawlerVolume`: multi-threaded breadth-first `FindFirstFileExW(FindExInfoBasic, FIND_FIRST_EX_LARGE_FETCH)` (UTF-16, no 8.3 names), one thread per physical device, results stream into the same index. Change detection: `ReadDirectoryChangesW` on the volume root with `bWatchSubtree=TRUE`, 64 KB buffer; on `ERROR_NOTIFY_ENUM_DIR` / overflow ⇒ mark subtree dirty and re-crawl it. Network shares: opt-in only, slow, labelled. [Likely]

---

## 11. Testing strategy

**Unit (Linux + Windows CI):** lexer/parser golden tests (≥ 200 queries); normalizer equivalence; matcher vs naive reference.
**Property/fuzz:** random ASTs → executor vs brute-force `std::string::find` implementation must return identical id sets; libFuzzer on parser (no crash, no hang, bounded memory); regex timeouts.
**Differential:** SIMD scan vs scalar scan on random UTF-8 with poisoned regions and boundary cases (needle at chunk edge).
**Synthetic benchmark:** generator makes 1 M / 5 M realistic paths (Zipf-distributed names, extension mix, depth 1–15). Report p50/p95/p99 keystroke latency for 1–6 character queries, memory/entry, fuzzy latency. Gate: S6/S10 numbers in §0.
**Windows integration (the only way to prove the NTFS code):** CI/agent script creates a VHDX (`diskpart` / PowerShell `New-VHD`), formats NTFS, mounts it, populates 50 k files incl. hard links, junctions, Unicode names, 255-char names, then runs:

1. full enumerate → compare to `FindFirstFile` crawl (set equality on (path)).
2. mutate (create/rename/move dir/delete/hardlink) while tailing → compare again.
3. stop service, mutate, restart → replay equality.
4. fill/force journal wrap (tiny journal via `fsutil usn createjournal m=… a=…`) → verify rebuild. **Persistence:** truncate/corrupt file at random byte → must rebuild, never crash. **Security:** non-admin client cannot read a file name inside a folder it can't list; pipe rejects remote and oversized frames.

---

## 12. Build order (give the agent ONE step at a time; each ends with its gate)

| Step | Deliverable                                                      | Gate                                             |
| ---- | ---------------------------------------------------------------- | ------------------------------------------------ |
| S0   | CMake skeleton, CI (Windows + Linux), `CONTEXT.md` license rules | builds green                                     |
| S1   | `NameIndex` columns, arenas, add/remove/rename, fold             | unit tests                                       |
| S2   | Lexer, parser, AST, normalizer                                   | 200 golden tests, fuzz 10 min clean              |
| S3   | Scalar matchers + executor + naive reference                     | differential tests pass                          |
| S4   | `UsnEnumerator` + builder (Windows)                              | VHDX test 1                                      |
| S5   | `JournalTail` + delta apply                                      | VHDX tests 2–4                                   |
| S6   | SIMD scan + parallel chunks + reverse lookup                     | **1 M entries, 4-char query ≤ 30 ms**            |
| S7   | Sort permutations, paging, incremental narrowing                 | window fetch ≤ 5 ms                              |
| S8   | Snapshot persistence                                             | corrupt-file test, load ≤ 1 s/1 M                |
| S9   | `MftRawEnumerator` (size/date in one pass)                       | equality vs S4 + crawl on VHDX; build ≤ 15 s/1 M |
| S10  | Fuzzy + ranking + usage DB                                       | fuzzy ≤ 15 ms extra; ranking eval set            |
| S11  | Service + pipe + permission filter                               | security tests                                   |
| S12  | Crawler fallback for FAT/exFAT/portable                          | parity with S4 on a non-NTFS test volume         |

---

## 13. Decisions needing your approval

1. **Index in service only, UI queries over pipe** (§8) instead of snapshot-to-UI. Recommended.
2. **Memory budget 110–115 B/entry** instead of 100 (§3).
3. **RE2** for regex (BSD-3) instead of `std::regex`.
4. **v1 uses USN enum + lazy size/date; raw MFT parse in S9.** Safer for AI-written code you can only test on one PC.

## 14. Known unknowns (agent must verify, not assume)

Everything's exact operator semantics; actual SIMD throughput on your CPU; fuzzy prefilter selectivity; behaviour of `FSCTL_ENUM_USN_DATA` on volumes with huge journals; raw-MFT edge cases (attribute lists, compressed/sparse files).