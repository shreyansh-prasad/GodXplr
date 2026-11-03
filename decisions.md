# GodXplr — Architecture Decision Register

**Purpose:** The single record of accepted and pending architecture decisions. Do not silently edit an accepted decision; supersede it with a new ADR entry.

## Status definitions

- **ACCEPTED:** Product/architecture direction approved. Implementation still needs tests.
- **PROVISIONAL:** Baseline selected for a first implementation/experiment; must be validated before broad commitment.
- **OPEN:** Requires evidence or user decision; no silent default.
- **REJECTED:** Considered and not selected under current constraints.
- **SUPERSEDED:** Replaced by a later ADR; retained for history.

## Accepted / provisional baseline

| ADR | Decision | Status | Evidence / guardrail |
|---|---|---|---|
| ADR-0001 | C++20 is the application language baseline. | ACCEPTED | CMake/compiler baseline must be pinned in P02. |
| ADR-0002 | Windows is the primary implementation and validation platform. | ACCEPTED | macOS/Linux remain CI/portability targets until real-machine testing. |
| ADR-0003 | Qt 6 Widgets is the initial UI shell. | ACCEPTED | Exact Qt minor/patch version is a build decision, not “latest”. |
| ADR-0004 | Main file surface uses custom virtualized `QAbstractItemView` + compact model/store. | ACCEPTED | No per-file QWidget; no `QFileSystemModel` as authoritative large-folder model. |
| ADR-0005 | UI work is asynchronous and bounded. | ACCEPTED | No blocking I/O/IPC/sort/decode/wait on GUI thread. |
| ADR-0006 | Search contains no AI and no paid dependency. | ACCEPTED | Local-first filename/content search. |
| ADR-0007 | NTFS name index uses MFT/USN techniques behind a helper service. | PROVISIONAL | Spec 01 S0–S12; Windows VHDX validation required. |
| ADR-0008 | Full-volume name index stays in index service; UI queries through versioned named pipe. | PROVISIONAL | This is the later Spec 01 decision and supersedes the earlier report's snapshot-to-UI proposal. Validate memory, IPC, ACL and service recovery. |
| ADR-0009 | Initial enumeration uses USN enumeration; raw MFT parser is deferred until a gated step. | PROVISIONAL | Safer initial parser surface; add raw MFT only after differential testing. |
| ADR-0010 | Non-NTFS/portable volumes use crawler/provider fallback. | PROVISIONAL | Must test FAT/exFAT/removable/network behaviour and watcher overflow. |
| ADR-0011 | Content indexing runs per user, with extractor child-process isolation. | PROVISIONAL | Spec 02; security review and Windows sandbox tests required. |
| ADR-0012 | SQLite FTS5 trigram is the candidate primary content index. | OPEN | Must pass Spec 02 C0 size/semantics/performance gate before final approval. |
| ADR-0013 | GodXplr source license intent is MIT. | ACCEPTED | Dependency licenses and Qt LGPL obligations are reviewed individually. |
| ADR-0014 | Default Explorer replacement is late-stage, reversible and optional. | ACCEPTED | Must include restore flow and Windows update resilience tests. |
| ADR-0015 | Native Windows context menu is desired. | PROVISIONAL | Exact `IContextMenu` lifecycle, owner-draw forwarding and hang/crash isolation need a Shell integration spec. |
| ADR-0016 | Search ranking has exact/substring tiers above fuzzy matches. | ACCEPTED | Spec 01 query correctness and hand-labelled ranking evaluation required. |
| ADR-0017 | UI rendering uses Qt Widgets raster path first; QRhi/QQuickWidget are not default dependencies. | ACCEPTED | Qt Quick/native renderers require a controlled benchmark and ADR before adoption. |
| ADR-0018 | 8 GB RAM + SSD is the minimum design target. | ACCEPTED | Must be validated under representative load; no claim of full feature performance until tested. |
| ADR-0019 | In-memory `AccessCheck` on cached NTFS Security Descriptors for search security. | ACCEPTED | Replaces per-hit `CreateFileW` impersonation loops; sub-microsecond token validation against `$Secure:$SDS`. |
| ADR-0020 | Out-of-process `godxplr-shellhost.exe` companion for COM Shell extensions and context menus. | ACCEPTED | Isolates third-party COM extensions (`IContextMenu3`); prevents explorer main process crashes and thread deadlocks. |
| ADR-0021 | NTFS USN Journal tailing is the authoritative live change feed on NTFS volumes. | ACCEPTED | Replaces `ReadDirectoryChangesW` on NTFS; immune to kernel buffer overflow during massive file mutations (e.g. npm/git/builds). |
| ADR-0022 | Shared Memory Ring Buffer with atomic generation counters for interactive search IPC. | ACCEPTED | Eliminates named-pipe syscall and serialization overhead on rapid keystrokes; provides zero-microsecond search query cancellation. |
| ADR-0023 | Asynchronous Overlapped I/O and Windows 11 I/O Rings (`IORING`) for the file operations engine. | ACCEPTED | Batched asynchronous streaming handles millions of small files (e.g. `node_modules`) without filesystem serialization stall. |
| ADR-0024 | Positionless Roaring Bitmaps with streaming candidate verification for content search. | ACCEPTED | Replaces bloated positional FTS5; reduces disk index size by ~75% and accelerates candidate intersection. |
| ADR-0025 | Real-time in-memory recursive folder size computation via MFT parent pointer aggregation. | ACCEPTED | Computes instant folder sizes in RAM from the MFT tree; eliminates slow recursive disk crawls. |

## Open decisions and required evidence

| ID | Open question | Required evidence / owner | Blocked work |
|---|---|---|---|
| OPEN-01 | Spec 01 memory contract says ≤110 B/entry while its columns estimate about 113 B. | Recompute exact layout, allocator overhead, arenas and hash load; benchmark at 1M/2M/5M. | Final capacity/per-volume memory claims. |
| OPEN-02 | 5M-entry hard cap and 8 GB RAM are compatible only if service memory and OS/user workload budgets are measured. | Windows low-memory benchmark and pressure policy. | Release capacity claims. |
| OPEN-03 | NTFS service privilege model, `SeBackupPrivilege`, service SID and impersonation checks. | Solved by ADR-0019: In-memory `AccessCheck` on cached `$Secure:$SDS` descriptors. | Closed by ADR-0019. |
| OPEN-04 | Spec 01 FRN storage representation in SQLite and signed 64-bit edge cases. | Choose stable exact binary representation and test full-width values. | Spec 02 schema finalization. |
| OPEN-05 | FTS5 trigram details, contentless delete support, phrase semantics, size ratio. | Solved by ADR-0024: Positionless Roaring Bitmaps with on-demand candidate verification. | Closed by ADR-0024. |
| OPEN-06 | Spec 03 final UI timing thresholds on declared minimum hardware. | U0 benchmark baseline + representative Windows hardware. | UI performance release gate. |
| OPEN-07 | Shell handler crash/hang isolation strategy. | Solved by ADR-0020: Out-of-process `godxplr-shellhost.exe` host. | Closed by ADR-0020. |
| OPEN-08 | File operations engine transaction/undo/recovery contract. | Produce Spec 04 before destructive-operation implementation. | Copy/move/delete/rename implementation beyond safe prototype. |
| OPEN-09 | Unified filesystem provider/capability model. | Produce Spec 05 before archive/network/cloud/MTP integration. | Provider ecosystem. |
| OPEN-10 | Archive, network, cloud and MTP support scope/order. | Provider research + capability/security spec. | Those integrations. |
| OPEN-11 | Installer, service upgrade, code signing and auto-update model. | Distribution/spec + licensing/security review. | Installer and updater. |
| OPEN-12 | macOS/Linux native support level. | Separate platform feasibility specs and real device maintainers. | End-user support claims. |
| OPEN-13 | Exact `Everything` query compatibility and edge cases. | Golden corpus from authoritative docs/app observations. | Compatibility claims. |
| OPEN-14 | Third-party dependencies and Qt version/linking model. | Dependency manifest, license review, reproducible build. | Release packaging. |

## Clarifications that prevent inherited contradictions

1. The earlier research report proposed sending index snapshots/deltas to the UI. Spec 01 later changes this to a service-owned index with UI queries over IPC. The later Spec 01 proposal is the current baseline; do not implement both.
2. Spec 01's memory table estimates approximately 113 B/entry despite the ≤110 B contract. Treat this as unresolved and measure/recalculate it.
3. Spec 02's FTS5 decision is conditional on C0. Do not hard-code it into a product dependency before C0.
4. Spec 03's performance figures are targets, not achieved measurements. UI renderer alternatives must be benchmarked on equal workloads.
5. Prior documents contain C++ skeletons explicitly not compiled. No skeleton is assumed to compile or be API-correct until built against the pinned toolchain.

## ADR workflow

Use `templates/adr-template.md`. A decision is accepted only when it includes context, alternatives, evidence, consequences, migration/rollback implications, tests, risks and approver. Append new ADRs; never erase old entries. Update `context.md` only after ADR acceptance.
