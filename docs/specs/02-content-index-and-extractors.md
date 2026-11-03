# GodXplr Spec 02 — Content Index + Extractors

Consumer: Antigravity agent. Tags: [Certain] documented behaviour; [Likely] engineering inference; [Guessing] estimate, must be measured. Code is skeleton, **not compiled**. Depends on Spec 01 (name index supplies candidates, sizes, FRNs).

---

## 0. Contract

| Item               | Value                                                                                        |
| ------------------ | -------------------------------------------------------------------------------------------- |
| Coverage           | all NTFS volumes; every file whose text is extractable                                       |
| Persistence        | on disk, resumable, crash-safe                                                               |
| Process            | per-user `godxplr-contentd.exe` (never SYSTEM)                                               |
| Extraction         | in sandboxed child processes, never in contentd itself                                       |
| Idle cost          | CPU < 1 %, no disk activity when paused                                                      |
| Query              | `content:` term, phrase, NOT, wildcard, regex, `case:`, `wholeword:`                         |
| Latency [Guessing] | rare term on 1 GB text ≤ 100 ms p95; worst-case common term returns top-K ≤ 1 s, cancellable |
| Licenses           | MIT-compatible only (PDFium, libarchive, Tesseract, SQLite, miniz, expat, RE2)               |
| No                 | AI, network, paid services                                                                   |

---

## 1. First principles

Cost of content search = **read bytes + parse + write index**. Three facts shape everything:

1. **Most bytes on a drive are not text** (video, installers, images, caches). Reading them is the dominant avoidable cost. So *decide from metadata first*: Spec 01's name index already holds name, extension, size, attributes for every file. Candidate selection costs **zero disk I/O**. [Certain: data exists in index; sizes from lazy fill/S9]
2. **Parsing untrusted files is the biggest security risk** (PDF, Office, archives). Crash or exploit in a parser must not take down the indexer or run with user data access beyond one file. → out-of-process, job-limited extractors. [Likely]
3. **Index size, not speed, is the scaling limit.** A positional trigram index can be as large as or larger than the text it indexes. Must be measured early (gate C0) with a fallback plan. [Likely]

Search semantics: the user expects Everything/grep-like **substring** match, not word-stemming. Trigram indexing gives that.

---

## 2. Index technology: decision + alternatives

| Option                                                                             | Substring             | Size                                | Build effort              | License       | Verdict                                                       |
| ---------------------------------------------------------------------------------- | --------------------- | ----------------------------------- | ------------------------- | ------------- | ------------------------------------------------------------- |
| **SQLite FTS5 `trigram` tokenizer**                                                | yes (≥3 chars)        | large (positional) [Likely]         | low                       | public domain | **Primary**                                                   |
| FTS5 `unicode61` words                                                             | no (word/prefix only) | small                               | low                       | PD            | Keep as comparison only                                       |
| **Custom positionless trigram postings** (roaring/delta-varint, verify by re-read) | yes                   | small, \~30–60 % of text [Guessing] | high (weeks)              | own code      | **Plan B** if C0 size gate fails                              |
| Tantivy (Rust)                                                                     | via ngram tokenizer   | medium                              | medium + second toolchain | MIT           | Rejected: toolchain                                           |
| Xapian                                                                             | words                 | medium                              | low                       | **GPL**       | Rejected                                                      |
| Lucene                                                                             | –                     | –                                   | –                         | Java          | Rejected                                                      |
| Windows Search (WSearch) via `ISearchQueryHelper`                                  | words                 | external                            | low                       | OS            | Optional later provider; not guaranteed on/indexed everywhere |

**FTS5 configuration [Likely, verify in C0]:**

```sql
CREATE VIRTUAL TABLE ft USING fts5(
  body,
  content='',                  -- contentless: text NOT stored again
  contentless_delete=1,        -- needs SQLite >= 3.43; enables DELETE by rowid
  tokenize='trigram case_sensitive 0',
  detail=full                  -- phrases (=substring) need positions
);

```

- Trigram tokenizer turns text into overlapping 3-char tokens; a query `"hello"` becomes a phrase of adjacent trigrams → exact substring semantics.
- Queries < 3 chars cannot use the index (see §8).
- Test whether `detail=column` still works for trigram phrase queries; it likely errors → keep `full`. [Guessing]
- `rowid = file_id`.

**Why contentless:** storing text duplicates the file. Snippets are produced on demand by re-reading the file (§8.4). Cost: stale text if the file changed → compare `mtime/size`.

**C0 size gate (before any pipeline work):** index 1 GB of mixed real text (e.g. Linux kernel source + Wikipedia extract + your Documents). Record `index_bytes / text_bytes`.

- ≤ 1.0 → proceed with FTS5.
- 1.0–2.0 → proceed but default caps tighten (§6) and the estimator warns.
-

> 2.0 → switch to Plan B before building more.

---

## 3. Schema (one SQLite DB per volume)

Path: `%LOCALAPPDATA%\GodXplr\content\<volSerialHex>.db`. Separate DB per volume ⇒ delete/rebuild one volume, parallel writers possible, damage contained. [Likely]

```sql
PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL; PRAGMA page_size=4096;
PRAGMA wal_autocheckpoint=2000; PRAGMA mmap_size=0;   -- avoid mmap I/O errors crashing us

CREATE TABLE files(
  file_id    INTEGER PRIMARY KEY,           -- = ft.rowid
  frn        INTEGER NOT NULL,              -- 64-bit FRN (index + sequence)
  size       INTEGER NOT NULL,
  mtime      INTEGER NOT NULL,              -- FILETIME at index time
  extractor  INTEGER NOT NULL,              -- extractor id
  ext_ver    INTEGER NOT NULL,              -- extractor version
  status     INTEGER NOT NULL,              -- 0 ok 1 empty 2 binary 3 toolarge 4 encrypted 5 corrupt
                                            -- 6 timeout 7 unsupported 8 denied 9 failed 10 truncated-ok
  text_bytes INTEGER NOT NULL,
  indexed_at INTEGER NOT NULL,
  attempts   INTEGER NOT NULL DEFAULT 0
);
CREATE UNIQUE INDEX files_frn ON files(frn);

CREATE TABLE pending(                       -- persistent work queue
  frn INTEGER PRIMARY KEY, prio INTEGER NOT NULL, reason INTEGER NOT NULL, queued_at INTEGER
);
CREATE INDEX pending_prio ON pending(prio DESC, queued_at);

CREATE TABLE settings(k TEXT PRIMARY KEY, v);   -- paused, schemaVersion, calibration, caps

```

Keyed by **FRN, not path**: renames/moves do not trigger reindex (names/paths come from Spec 01). Sequence bits in the FRN change on record reuse ⇒ a reused slot is a different file ⇒ old row dropped. [Certain: FRN layout]

---

## 4. Architecture / data flow

```
 indexd (SYSTEM, Spec 01)                      contentd (user session)                      extractor children
 ┌──────────────┐  candidates + deltas   ┌───────────────────────────────┐   job-limited    ┌───────────┐
 │ name index   │ ─────────────────────► │ Reconciler → Scheduler(queue) │ ───file handle─► │ extract.exe│
 │ FRN,ext,size │   (named pipe, names   │      │                        │ ◄───UTF-8 text── │ (sandbox)  │
 └──────────────┘    + attrs only)       │      ▼                        │                  └───────────┘
        ▲                                │ Writer (1 thread, batched)    │
        │ ResolveFrn(path)               │      ▼                        │
 UI ◄───┴── SearchCoordinator ◄─────────►│ SQLite FTS5 per volume        │
            (merges name+content)        └───────────────────────────────┘

```

UI never talks to SQLite. Pipe name `\\.\pipe\godxplr-content-<userSID>`, DACL: that SID only, `PIPE_REJECT_REMOTE_CLIENTS`, first instance flag. Client verifies server PID's image path. [Likely]

---

## 5. Candidate selection and reconciliation

### 5.1 Filter (no I/O)

For every file entry in the name index apply, in order:

1. **Excluded directory?** (parent-chain match against exclusion set, cached per dir id)
2. **Attributes:** skip `REPARSE_POINT` (symlink/junction: avoids duplicates and loops), `SYSTEM`, **`OFFLINE` and `RECALL_ON_DATA_ACCESS` / `RECALL_ON_OPEN` (cloud placeholders: reading them would trigger a download — never read)**, `TEMPORARY`, `SPARSE` over cap. [Certain: these attribute flags exist]
3. **Extension class:** allowlist → candidate; denylist (media/exe/dll/iso/…) → skip; unknown extension → `sniff` queue (low priority, only if size ≤ 1 MB).
4. **Size cap** per class (table §6).

### 5.2 Default exclusions

`Windows\`, `Program Files*\`, `ProgramData\`, `$Recycle.Bin`, `System Volume Information`, `.git\objects`, `node_modules`, `.venv`, `venv`, `__pycache__`, `target\`, `build\`, `dist\`, `.gradle`, `AppData\Local\Temp`, browser profile caches (`...\Cache*`, `...\Code Cache`, `...\GPUCache`), `.ssh`, `.gnupg`, `*.pem`, `*.key`, `*.pfx`, `*.kdbx`, `.env*`. Secrets excluded because the index would otherwise be a convenient file of passwords for malware. [Likely] User may add/remove rules; `.gxignore` files (gitignore syntax) honoured in any folder.

### 5.3 Reconciliation (on start and after any reconnect)

Stream candidates from indexd sorted by FRN; merge-join with `files` sorted by FRN:

- in index, not in DB → enqueue (`reason=new`)
- in both, `size`/`mtime` differ, or `ext_ver` < current extractor version → enqueue (`changed`/`upgrade`)
- in DB, not in index (or now excluded) → `DELETE FROM ft WHERE rowid=?; DELETE FROM files…`
- `status` in (failed, timeout) and `attempts < 2` and extractor version bumped → re-enqueue Cost O(N) merge, seconds for 1 M. Reconciliation makes missed live events harmless (contentd offline, crash, journal wrap). [Likely]

If sizes/mtimes are not yet known in indexd (lazy fill), contentd stats the file when it reads it and compares then.

### 5.4 Live deltas

indexd pushes `Add/Remove/Attr/Rename` (FRN, attrs). Content-relevant: Add, data-change (`Attr` with mtime change). Debounce 5 s per FRN (files being written produce many events; avoid indexing half-written files). Rename/move: no action. Delete: remove row.

---

## 6. Scheduling and resource control

**Priority** (higher first): files modified in last 30 d in user profile folders (Documents, Desktop, Downloads, Pictures for OCR-enabled) > other user-profile files > other volumes' user data > old files > sniff queue > OCR queue.

**Throttle [Certain APIs]:**

- `SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN)` → lowers CPU, I/O and memory priority of contentd; children get the same via job/creation flags.
- Workers = `max(1, logicalCores/4)`; scale down if own CPU > 15 % for 30 s.
- Pause when: on battery (`GetSystemPowerStatus` ACLineStatus=0) unless user overrides; fullscreen/presentation (`SHQueryUserNotificationState`); user input in last 3 s for heavy classes (`GetLastInputInfo`); disk saturated (own read latency > 4× calibrated baseline).
- Resume with exponential-free linear ramp (+1 worker/10 s).
- Writer batches 500 docs or 1 s per transaction.

**Caps (defaults, user-editable):**

| Class                                                                                          | Max file | Max extracted text                                 | Max time |
| ---------------------------------------------------------------------------------------------- | -------- | -------------------------------------------------- | -------- |
| plain text / code / logs                                                                       | 50 MB    | 2 MB (first 2 MB, flag truncated)                  | 5 s      |
| OOXML / ODF / RTF / EPUB                                                                       | 50 MB    | 2 MB                                               | 10 s     |
| PDF                                                                                            | 100 MB   | 2 MB, ≤ 500 pages                                  | 20 s     |
| archive (members)                                                                              | 500 MB   | 5 MB total, ≤ 2000 members, ratio ≤ 100:1, depth 1 | 20 s     |
| OCR image                                                                                      | 25 MB    | 100 KB                                             | 30 s     |
| Huge files (> text cap) are indexed up to the cap and flagged `truncated`. (v2: chunked rows.) |          |                                                    |          |

**Up-front estimator (before first full run, requires user consent):**
count candidate bytes per class from name index → `estTextBytes = Σ classBytes × textRatio[class]` (initial ratios: text 1.0, OOXML 0.05, PDF 0.03, archives 0.3 [Guessing]) → calibrate throughput on first 200 files → show *"\~X GB documents, \~Y min on AC, index ≈ Z GB"* using the measured C0 ratio. Show again whenever caps change.

---

## 7. Extractors

### 7.1 Interface

```cpp
// src/content/api/extractor.h
namespace gx {
struct FileProbe { std::u16string ext; uint64_t size; uint8_t head[4096]; uint32_t headLen; };
struct ExtractLimits { uint64_t maxFileBytes, maxTextBytes; uint32_t maxSeconds; uint32_t maxMembers; };
enum class ExStatus : uint8_t { Ok, Empty, Binary, TooLarge, Encrypted, Corrupt, Timeout,
                                Unsupported, Denied, Failed, TruncatedOk };
struct ITextSink { virtual ~ITextSink()=default;
  virtual bool append(std::string_view utf8) = 0; };      // returns false => cap reached, stop
struct IFileSource { virtual ~IFileSource()=default;
  virtual uint64_t size() const = 0;
  virtual size_t readAt(uint64_t off, void* buf, size_t n) = 0; };  // handle-backed
class IExtractor { public:
  virtual ~IExtractor()=default;
  virtual uint16_t id() const = 0;  virtual uint16_t version() const = 0;
  virtual int  score(const FileProbe&) const = 0;           // 0 = can't handle; highest wins
  virtual ExStatus extract(IFileSource&, const ExtractLimits&, ITextSink&) = 0;
};
class IContentIndex { public:
  virtual ~IContentIndex()=default;
  virtual void put(uint64_t volSerial, uint64_t frn, const DocMeta&, std::string_view text) = 0;
  virtual void remove(uint64_t volSerial, uint64_t frn) = 0;
  virtual uint64_t search(const ContentQuery&, IContentSink&) = 0;
};
}

```

All extractors **stream** UTF-8 into `ITextSink`; none may hold a whole large file.

### 7.2 Sandbox

`godxplr-extract.exe` (separate binary, same sources, no UI/Qt).

- contentd opens the file (as user, with `FILE_SHARE_READ|WRITE|DELETE`, `FILE_FLAG_SEQUENTIAL_SCAN`), **duplicates the handle into the child**; the child needs no filesystem access.
- Child created suspended, assigned to a **Job Object**: `JOB_OBJECT_LIMIT_PROCESS_MEMORY` (512 MB), `ACTIVE_PROCESS` = 1, `KILL_ON_JOB_CLOSE`, UI restrictions (no clipboard/desktop), CPU rate cap; plus **restricted token / low integrity** (`CreateRestrictedToken`, `SetTokenInformation(TokenIntegrityLevel=Low)`) or AppContainer with no capabilities. No network capability. [Certain: APIs exist; exact hardening verified in C7]
- Output: length-prefixed UTF-8 frames on an anonymous pipe; child exits after one file or after N files (pool recycles every 200 files or 256 MB RSS).
- Watchdog in contentd: wall-clock timeout kills the job. Crash or kill ⇒ status `Failed/Timeout`, `attempts++`. After 2 attempts: **poison** (skip until extractor version changes).
- Batch small files to one child call to amortise process startup (≈ 5–20 ms per CreateProcess [Guessing]); large/risky formats (PDF, archives, OCR) get a fresh child.

### 7.3 Extractor catalogue

| Extractor                 | Handles                                                                                                                                                    | Method                                                                                                                                        | Notes                                                                                                                                                                                              |
| ------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `text`                    | txt, md, csv, tsv, json, xml, yaml, toml, ini, log, source code (\~300 exts), html/htm (tags stripped), svg, ipynb (cell sources), eml (headers+text body) | sniff → decode → stream                                                                                                                       | see 7.4                                                                                                                                                                                            |
| `ooxml`                   | docx, xlsx, pptx, dotx, xltx, potx                                                                                                                         | open ZIP (miniz, MIT), stream XML with **expat (MIT) SAX**                                                                                    | docx `word/document.xml` `<w:t>`; xlsx `xl/sharedStrings.xml` + `<is><t>`; pptx `ppt/slides/slide*.xml` `<a:t>` + notes. Zip-bomb guards. Encrypted package (CFB `EncryptedPackage`) ⇒ `Encrypted` |
| `odf`                     | odt, ods, odp                                                                                                                                              | ZIP → `content.xml` `<text:p>` etc.                                                                                                           | same guards                                                                                                                                                                                        |
| `rtf`                     | rtf                                                                                                                                                        | control-word stripper                                                                                                                         | ignore `\*` destinations, handle `\uN`, `\'hh`                                                                                                                                                     |
| `epub`                    | epub                                                                                                                                                       | ZIP → xhtml → text                                                                                                                            |                                                                                                                                                                                                    |
| `pdf`                     | pdf                                                                                                                                                        | PDFium (BSD-3/Apache-2.0 [Likely]): `FPDF_LoadPage`, `FPDFText_LoadPage`, `FPDFText_GetText` (UTF-16 → UTF-8) per page                        | prebuilt binaries; record license in CONTEXT.md. 0 text chars on all pages ⇒ `Empty` + queue OCR if enabled. Password ⇒ `Encrypted`                                                                |
| `archive`                 | zip, 7z, tar(.gz/.bz2/.xz), rar (read)                                                                                                                     | **libarchive** (BSD-2 [Likely]); extract text members through the `text`/`ooxml` logic; store `\n[[member/path]]\n` marker before each member | depth 1 only; no nested archives v1; encrypted ⇒ index member names only                                                                                                                           |
| `ocr`                     | png, jpg, bmp, tiff, scanned PDF pages                                                                                                                     | **Tesseract** (Apache-2.0) + Leptonica (BSD-2), `eng` fast model bundled; more languages optional download                                    | **Off by default.** Per-folder opt-in, runs only on AC + idle. 1–3 s/page [Guessing]                                                                                                               |
| `legacyoffice` (optional) | doc, xls, ppt                                                                                                                                              | Windows **IFilter** COM inside the sandbox                                                                                                    | **Disabled by default**, "best-effort": third-party filter DLLs load in the child, so only in Low-IL child; v3                                                                                     |
| `meta` (v3)               | mp3 ID3, EXIF, MP4 tags                                                                                                                                    | metadata → short text                                                                                                                         | out of scope v1                                                                                                                                                                                    |

OCR alternative to compare in research: Windows built-in `Windows.Media.Ocr` (no extra license, good quality, WinRT, language packs from OS). Agent must benchmark both on 50 samples (accuracy, s/page, size) before choosing; Tesseract stays default only if it wins or ties. [Guessing]

### 7.4 Text extractor details

1. Read first 8 KB. **Binary test:** any NUL (unless BOM says UTF-16/32), or > 30 % bytes in 0x00–0x08/0x0E–0x1F ⇒ `Binary`. Known magic numbers (`MZ`, `PK` handled by others, `%PDF`, `\x7fELF`, PNG, JPEG, GIF) ⇒ `Binary`/route.
2. **Encoding:** BOM (UTF-8/16LE/16BE/32) ⇒ use; else if valid UTF-8 (strict validator) ⇒ UTF-8; else if NUL pattern suggests UTF-16 (every other byte 0 in ≥ 30 % of first 1 KB) ⇒ UTF-16LE/BE; else system ANSI code page via `MultiByteToWideChar` → UTF-8. (CJK legacy code pages = best effort.) [Likely]
3. Normalise: `\r\n`→`\n`, drop control chars except `\t\n`, **no Unicode normalisation (NFC)** v1 (trigram is on raw code points, case-fold simple only). Document limitation.
4. Long lines: no change; very long single line (minified JS) — cap text at maxTextBytes.
5. Stream in 64 KB chunks; never split a UTF-8 sequence.

---

## 8. Query execution

### 8.1 Compile

Spec 01 parser emits `ContentPredicate{ kind: Substr|Phrase|Wildcard|Regex, text, caseSensitive, wholeWord, negate }`. The `SearchCoordinator` (UI-side, Qt-free) splits the AST: **name-side predicates** → indexd; **content-side** → contentd.

### 8.2 Strategy by selectivity

| Situation                                  | Plan                                                                                                                                                                                                     |
| ------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| name filters yield small set (≤ 5 000 ids) | send FRN list to contentd; run FTS restricted: `... WHERE ft MATCH ? AND rowid IN (…)`                                                                                                                   |
| name filters broad / absent                | contentd FTS first, top-K (default 10 000) by rank → FRNs → coordinator intersects with name predicates via indexd                                                                                       |
| needle length < 3 chars                    | no index: allowed only if name-filtered set ≤ 2 000 files → grep those files (parallel, cancellable); otherwise UI shows "type ≥ 3 characters"                                                           |
| wildcard `a*b`                             | longest literal run ≥ 3 ⇒ FTS prefilter; verify on re-read with wildcard matcher                                                                                                                         |
| regex                                      | extract **required literals** (RE2 `RequiredPrefix`/analysis or own simple extractor) ≥ 3 chars ⇒ FTS prefilter; verify with **RE2** on re-read text. No required literal ⇒ treat like short needle rule |
| `case:` or `wholeword:`                    | FTS (case-insensitive) prefilter, then verify against re-read text in window order until `offset+count` verified hits                                                                                    |
| NOT content term                           | evaluate positive terms first; remove FTS hits of negated term (set difference on file_ids)                                                                                                              |
| multiple terms (AND)                       | one FTS query with `"a" AND "b"` (each phrase); OR → `OR`                                                                                                                                                |

Rank: FTS5 `bm25` (`ORDER BY rank`) per shard; coordinator normalises (`score / max(score in shard)`) then merges shards and blends with name/recency ranking from Spec 01 (name match boosts). Cross-shard bm25 is not strictly comparable; acceptable approximation. [Likely]

### 8.3 Cancellation / worst case

Common trigram (e.g. `the`) can match millions of docs. FTS5 returns lazily when `LIMIT` is used with `ORDER BY rank`? Ranking forces scanning all hits [Likely]. Mitigation: for queries whose estimated hits > 100 000 (probe with `SELECT count(*) … LIMIT 100001` under a 150 ms budget), switch to **unranked** mode `ORDER BY rowid LIMIT K`, mark the page "ranking limited". Use `sqlite3_interrupt()` on newer-query arrival. [Guessing] measure in C8.

### 8.4 Snippets and freshness

For the visible window (≤ 50 rows): contentd opens by FRN with `OpenFileById` (volume handle from the volume root; access checked as the user at open [Certain]), runs the same extractor in the sandbox for the first matched region only? Simplest v1: re-extract via the sandbox up to the cap, find first match with the verification matcher, return ±80 chars and line number. Cache LRU 200 snippets keyed (FRN, mtime, query hash).
If `mtime/size` differ from `files` row: mark result `stale`; if re-extracted text no longer matches ⇒ drop row and enqueue reindex.

### 8.5 API

```cpp
struct ContentQuery { std::vector<ContentPredicate> preds; std::vector<uint64_t> restrictFrns;
                      uint32_t topK = 10000; bool rankingAllowed = true; };
struct ContentHit  { uint64_t volSerial, frn; float rank; uint32_t approxMatches; };
struct Snippet     { uint64_t frn; uint32_t line; std::string text; bool stale; };
struct IContentIndexClient { virtual uint64_t search(const ContentQuery&, IContentSink&)=0;
                             virtual void cancel(uint64_t)=0;
                             virtual std::vector<Snippet> snippets(std::span<const uint64_t> frns, const ContentQuery&)=0;
                             virtual IndexStats stats()=0; virtual void pause(bool)=0; };

```

---

## 9. Maintenance and persistence behaviour

- **Writer:** one thread per volume DB. Update = `DELETE FROM ft WHERE rowid=?` + `INSERT INTO ft(rowid, body) VALUES(?,?)` + upsert `files`, all in a batch transaction. [Likely]
- **Index merge:** FTS5 segments accumulate. Idle task: `INSERT INTO ft(ft,rank) VALUES('merge', 500)` incrementally; `'optimize'` only on user request or after > 30 % rows changed. `crisismerge` default.
- **Compaction:** `PRAGMA incremental_vacuum` (set `auto_vacuum=INCREMENTAL` at creation); full `VACUUM` never automatic.
- **Crash safety:** WAL; on start: `PRAGMA integrity_check` skipped (slow) → run `quick_check` only if previous exit was unclean (marker file). `pending` survives; items stuck "in progress" simply re-run (idempotent).
- **Corruption:** `SQLITE_CORRUPT` ⇒ rename DB to `.bad`, rebuild that volume; other volumes unaffected.
- **Schema/extractor upgrade:** `ext_ver` bump enqueues affected files at low priority; schema bump ⇒ rebuild.
- **User controls:** pause/resume, "index now", per-folder include/exclude, per-type toggles, caps, OCR toggle, **delete index**, **estimate**, status (files indexed / pending / failed / size on disk / ETA).
- **Disk guard:** stop indexing if free space on index drive < 2 GB or index > configured max (default 10 GB); UI banner.

---

## 10. Security / privacy

1. contentd and extractors run **as the signed-in user** → index only contains files that user could read at index time.
2. DB directory ACL: user + SYSTEM only. Pipe DACL: user SID only. No remote clients.
3. **Re-check at display:** every returned row is opened by FRN as the user (snippets) and passes Spec 01's row permission filter; files whose ACL changed since indexing never show a snippet.
4. Defaults exclude secrets (§5.2). "Delete index" wipes DB and WAL (`DeleteFile` + overwrite not guaranteed on SSD; document it, recommend BitLocker).
5. Index not encrypted v1. Documented risk: malware running as the user can read the DB (it could read the files anyway). [Likely]
6. Extractor parser bugs ⇒ confined by job + low integrity + no network; memory cap; time cap.
7. Zip/archive bombs: ratio cap, absolute cap, member count cap; XML: disable DTD/entity expansion (expat `XML_SetEntityDeclHandler` stop / no external entities), depth cap 256.

---

## 11. Failure modes

| Failure                  | Detection                    | Response                                                    |
| ------------------------ | ---------------------------- | ----------------------------------------------------------- |
| Parser crash/hang        | child exit code / timeout    | kill job, mark `Failed/Timeout`, attempts++, poison after 2 |
| Zip/XML bomb             | ratio/size/depth caps        | `TooLarge/Corrupt`, no retry                                |
| Encrypted file           | format signal                | `Encrypted`, index name only                                |
| Cloud placeholder        | attribute check              | skip, never open                                            |
| File locked              | `ERROR_SHARING_VIOLATION`    | retry 3× with backoff (1, 10, 60 min) then `Denied`         |
| Access denied            | `ERROR_ACCESS_DENIED`        | `Denied`, retry only on ACL/mtime change                    |
| File changed mid-read    | mtime/size differ after read | discard, re-enqueue with debounce                           |
| Disk full / index cap    | free-space check             | pause + banner                                              |
| SQLite busy/locked       | `SQLITE_BUSY`                | single writer, busy_timeout 5 s                             |
| Power/battery drop       | `GetSystemPowerStatus`       | pause, resume on AC                                         |
| Volume offline           | I/O error                    | suspend volume queue, keep DB                               |
| Version skew UI↔contentd | `Hello` protocol version     | refuse, prompt restart                                      |

---

## 12. Testing

- **Golden extraction:** corpus of ≥ 200 files across formats (generated by tools: python-docx/openpyxl/python-pptx/reportlab in `tools/gen_corpus.py`; plus public-domain PDFs, odt, rtf, epub, zip/7z/tar). Expected text snapshots compared token-wise.
- **Hostile corpus:** zip bomb, billion-laughs XML, truncated/corrupted PDFs and OOXML, password-protected docs, 4 GB sparse file, deeply nested archives, files with NULs, mixed encodings. Pass = no crash of contentd, caps honoured, bounded time/memory.
- **Fuzz:** libFuzzer/AFL on each extractor in the child binary (Linux build) 30 min per extractor in CI-nightly.
- **Differential:** FTS result sets vs brute-force substring scan over the same corpus for 500 random queries (ASCII, Unicode, ≥3 chars, phrases, boundaries across chunk limits). Must be identical on files fully indexed (non-truncated).
- **Incremental:** edit/delete/rename/move/replace-with-same-name loops; reconcile must converge to brute-force truth; kill contentd at random points (crash-safety), restart, converge.
- **VHDX integration (Windows):** reuse Spec 01's VHDX harness; add placeholder-attribute files (set via `fsutil`/API) ⇒ never opened (verify via file-access audit or a probe filter driver is overkill: use `ProcMon`-less check by making unreadable ACL and attribute only).
- **Resource tests:** throttle works (CPU cap, pause on battery simulated through injected power provider), memory of contentd < 150 MB steady [Guessing], children killed at cap.
- **Benchmarks (record in `docs/bench/content.md`):** index throughput MB/s per core per class; `index_bytes/text_bytes`; query latency p50/p95/p99 for rare/mid/common terms on 1 GB and 10 GB corpora; WAL growth; merge time.
- **Security tests:** pipe from other user denied; restricted child cannot open another file or network (try from a test extractor).

---

## 13. Build order (one step per agent session; gate before next)

| Step | Deliverable                                                                                                                                    | Gate                                            |
| ---- | ---------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------- |
| C0   | **Feasibility spike:** FTS5 trigram contentless on 1 GB corpus, measure size/speed, test `detail=column`, `contentless_delete`, SQLite version | size ratio recorded; decision FTS5 vs Plan B    |
| C1   | `text` extractor + encoding/binary detection + `ITextSink`                                                                                     | golden + differential on text corpus            |
| C2   | `ContentIndexSqlite`: schema, writer, put/remove, search (substring/phrase/AND/OR/NOT)                                                         | differential vs brute force, 500 queries        |
| C3   | Reconciler + persistent queue + restart/kill tests                                                                                             | convergence tests                               |
| C4   | `ooxml`, `odf`, `rtf`, `epub` extractors (+ bomb guards)                                                                                       | golden + hostile corpus                         |
| C5   | `pdf` extractor (PDFium)                                                                                                                       | golden + fuzz 30 min                            |
| C6   | `archive` extractor (libarchive)                                                                                                               | golden + bomb tests                             |
| C7   | Sandbox child + job object + low IL + watchdog + pool                                                                                          | kill/crash/timeout tests; restricted-child test |
| C8   | Query planner: restrict-by-FRN, top-K, short needle, wildcard/regex/case/wholeword, cancellation                                               | latency gates §0                                |
| C9   | Scheduler + throttle + battery/idle/fullscreen + estimator + caps                                                                              | resource tests                                  |
| C10  | Pipe protocol, ACLs, SearchCoordinator merge with Spec 01, snippets, freshness                                                                 | security tests; end-to-end on VHDX              |
| C11  | Maintenance (merge, vacuum, corruption recovery, disk guard), settings API                                                                     | corruption + disk-full tests                    |
| C12  | OCR (Tesseract vs Windows OCR bench), optional IFilter provider                                                                                | benchmark doc; off by default                   |

---

## 14. Decisions needing your approval

1. **FTS5 trigram contentless + Plan B switch rule at C0.** (Recommended.)
2. **Per-user `contentd` + sandboxed extractor child processes** (more code, much safer).
3. **Cloud placeholders and secret folders excluded by default.**
4. **OCR off by default; legacy .doc/.xls/.ppt via IFilter off by default and deferred.**
5. **Reconciliation-based consistency** (no dependency on journal history).
6. Default **index cap 10 GB** and **per-file text cap 2 MB**.

## 15. Known unknowns (agent must verify, not assume)

FTS5 trigram index size ratio; `detail=column` behaviour with trigram; SQLite ≥ 3.43 availability in the build; PDFium prebuilt license text and Windows build flags; Windows OCR vs Tesseract quality; ranking cost on very common trigrams; exact Low-IL/AppContainer behaviour with duplicated file handles; throughput of CreateProcess batching.