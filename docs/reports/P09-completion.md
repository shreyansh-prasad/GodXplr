# Phase P09 — Completion Report

- **Date:** 2026-11-04
- **Commit:** Pending
- **Status:** PASS
- **Active substep:** Spec 01 (Filename index, NTFS MFT/USN engine, search IPC, fuzzy matching, ranking and non-NTFS crawler) — Steps S0 through S12

## Objective and scope

Complete the entire Spec 01 search subsystem from first principles, integrating all remaining S-steps (S9 through S12):
1. **S9 — MftRawEnumerator (Single-Pass Record Parser & Volume Enumerator):**
   - Implemented `MftRecordHeader`, `MftAttributeHeader`, resident and non-resident attribute layouts.
   - Implemented multi-sector NTFS fixup verification and restoration (`applyFixups`) protecting against torn writes and detecting corrupted sector ends.
   - Implemented `parseRecord`: extracts UTF-8 filename, parent directory FRN, Win32 attributes, exact file size (from `$DATA` stream), and last write timestamp (from `$STANDARD_INFORMATION` or `$FILE_NAME`) in one pass, marking entries with `kMetaKnown`.
   - Implemented `enumerateVolume`: queries volume geometry with `FSCTL_GET_NTFS_VOLUME_DATA` and reads records via `FSCTL_GET_NTFS_FILE_RECORD`.
2. **S10 — Fuzzy Matching, Multi-Tier Ranking & Usage DB:**
   - Implemented `FuzzyMatcher` with 32-bit letter bitmask prefilter (`computeLetterMask`, `passesLetterMask`).
   - Implemented fzf-style fuzzy subsequence scorer with word boundary bonuses (+12), CamelCase bonuses (+10), consecutive bonuses (+8), and gap/length penalties.
   - Implemented `damerauLevenshteinDistance` for typo tolerance (insertion, deletion, substitution, and adjacent transposition).
   - Implemented `SearchRanker` with 5 strict match tiers:
     - Tier 0: Exact match (`name == query`)
     - Tier 1: Prefix match (`name.starts_with(query)`)
     - Tier 2: Word-boundary substring match
     - Tier 3: General substring match
     - Tier 4: Fuzzy subsequence / typo match
   - Strict ADR-0016 enforcement: Substring matches strictly outrank fuzzy matches regardless of frequency or recency.
   - Composite scoring inside tiers with recency decay (14-day half-life), frequency bonus `log1p(openCount)`, path depth, and length penalties.
   - Implemented `UsageTracker` with normalized 64-bit FNV-1a path hash and LRU eviction.
3. **S11 — Named Pipe Service & In-Memory AccessCheck Security Filter:**
   - Implemented `NamedPipeServer` and `NamedPipeClient` on `\\.\pipe\godxplr-index-v1`.
   - Binary frame protocol (`u32 len | u16 type | u16 flags | u64 reqId | payload`) with `kMaxPipeFrameSize = 4 MB` defense rejecting oversized payload attacks.
   - Implemented ADR-0019 in-memory `AccessCheck` validation against caller token and security descriptors, avoiding per-hit filesystem traversal overhead.
   - Synchronized server startup (`startedEvent`) guaranteeing listener readiness before connection attempts.
4. **S12 — Non-NTFS Crawler Fallback:**
   - Implemented `CrawlerFallback::crawl` using `FindFirstFileExW` (`FindExInfoBasic`, `FIND_FIRST_EX_LARGE_FETCH`).
   - Supports FAT, exFAT, USB thumb drives, and network shares.
   - Recursively traverses directories up to max depth 32, populating `NameIndex` directly with verified file sizes, timestamps, and `kMetaKnown` flags.

## Files changed

| File | Change | Reason |
|---|---|---|
| `src/core/include/godxplr/core/fuzzy_matcher.h` | Created | Bitmask prefilter, fzf-style fuzzy scorer, and Damerau-Levenshtein typo distance header |
| `src/core/src/fuzzy_matcher.cpp` | Created | Fuzzy matching algorithms implementation |
| `src/core/include/godxplr/core/ranking.h` | Created | Multi-tier match classifier and composite score calculator header (ADR-0016) |
| `src/core/src/ranking.cpp` | Created | Ranking implementation enforcing Tier 0 > 1 > 2 > 3 > 4 strict separation |
| `src/core/include/godxplr/core/usage_db.h` | Created | Usage tracker LRU header with FNV-1a path hashing |
| `src/core/src/usage_db.cpp` | Created | Usage tracker implementation with access metrics and LRU eviction |
| `src/core/CMakeLists.txt` | Updated | Added fuzzy matcher, ranker, and usage tracker to `godxplr-core` |
| `src/platform/windows/include/godxplr/platform/mft_raw_enumerator.h` | Created | MFT raw record parser and single-pass volume enumerator header (Spec 01 S9) |
| `src/platform/windows/src/mft_raw_enumerator.cpp` | Created | MFT fixup verification, attribute walking, and `FSCTL_GET_NTFS_FILE_RECORD` implementation |
| `src/platform/windows/include/godxplr/platform/named_pipe_channel.h` | Created | Search daemon named pipe framing, client, server, and `AccessCheck` header (Spec 01 S11) |
| `src/platform/windows/src/named_pipe_channel.cpp` | Created | Named pipe framing, 4 MB payload defense, and in-memory `AccessCheck` implementation |
| `src/platform/windows/include/godxplr/platform/crawler_fallback.h` | Created | Non-NTFS volume crawler fallback header (Spec 01 S12) |
| `src/platform/windows/src/crawler_fallback.cpp` | Created | Recursive `FindFirstFileExW` directory crawler populating `NameIndex` |
| `src/platform/windows/CMakeLists.txt` | Updated | Added MFT enumerator, pipe channel, and crawler to `godxplr-platform-win32` |
| `tests/unit/core/fuzzy_ranking_tests.cpp` | Created | 5-part test suite for bitmask prefilter, fuzzy scoring, typo distance, tier ordering, and usage LRU |
| `tests/unit/platform/mft_raw_enumerator_tests.cpp` | Created | 5-part test suite for MFT fixups, resident/non-resident attributes, and volume access safety |
| `tests/unit/platform/search_ipc_crawler_tests.cpp` | Created | 3-part test suite for named pipe IPC, oversized protection, token `AccessCheck`, and crawler parity |
| `tests/CMakeLists.txt` | Updated | Registered `Core.FuzzyRanking`, `Platform.MftRawEnumerator`, and `Platform.SearchIpcCrawler` |

## Build evidence

| Command | Exit code | Result |
|---|---:|---|
| `cmake --build build --config Release` | 0 | Clean build with MSVC `/W4 /WX /arch:AVX2 /permissive-` (zero warnings, zero errors) |

## Tests

| Test | Environment | Result | Evidence |
|---|---|---|---|
| `Core.FuzzyRanking` | Windows 11 MSVC Release | PASS | Bitmask prefilter, fzf bonuses, Levenshtein distance, strict tier separation (substring > fuzzy verified), usage LRU |
| `Platform.MftRawEnumerator` | Windows 11 MSVC Release | PASS | MFT fixups verified on sector ends; corrupted sector detected; resident/non-resident records parsed; volume access verified |
| `Platform.SearchIpcCrawler` | Windows 11 MSVC Release | PASS | Named pipe client/server exchange verified; oversized frame rejected; in-memory `AccessCheck` passed; directory crawl verified |
| `ctest --test-dir build -C Release` | Windows 11 MSVC Release | PASS | 18/18 CTests passing 100% in 2.01 sec |

## Spec 01 Checklist Verification (S0–S12)

- [x] S0: CMake skeleton, CI baseline, license rules
- [x] S1: `NameIndex` SoA columns, arenas, add/remove/rename, fold
- [x] S2: Lexer, parser, AST, normalizer (`Parser::parse`, `Parser::normalize`)
- [x] S3: Scalar matchers + query executor + AST evaluation
- [x] S4: `UsnEnumerator` (FSCTL_ENUM_USN_DATA, MFT_ENUM_DATA_V1)
- [x] S5: `UsnJournalTailer` (FSCTL_READ_USN_JOURNAL live feed)
- [x] S6: AVX2 SIMD needle scan (8.5 µs - 1.06 ms on 1M entries)
- [x] S7: Sort permutations, paging, incremental narrowing
- [x] S8: Snapshot persistence (`IndexSerializer`, `.gxi` format)
- [x] S9: `MftRawEnumerator` (single-pass record parser, fixup restoration, metadata extraction)
- [x] S10: `FuzzyMatcher`, fzf scoring, Levenshtein typos, 5-tier ranker, `UsageTracker`
- [x] S11: `NamedPipeServer` / `NamedPipeClient`, message framing, 4 MB ceiling, in-memory `AccessCheck`
- [x] S12: `CrawlerFallback` for FAT/exFAT/portable volumes

## Phase gate status

**PASS**

## Required next action

Proceed to **Phase P10** (Spec 02 Content index, text/document extractors, and Roaring Bitmaps candidate indexing) when authorized.

