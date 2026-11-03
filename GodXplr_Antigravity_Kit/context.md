# GodXplr — Project Context and Product Contract

**Document:** CONTEXT 1.0  
**Product:** GodXplr  
**Audience:** Antigravity / Gemini coding agents, reviewers and future contributors  
**Status:** Product direction accepted; subsystem specifications are engineering baselines with open validation items.

## 1. Product vision

GodXplr is a Windows-first, C++ desktop file manager intended to become a credible everyday alternative to Windows File Explorer while incorporating the strongest useful capabilities demonstrated by Everything, File Pilot, Files and Directory Opus.

It is also an open-source portfolio project. That does not lower the reliability bar: file operations must protect user data, search must be correct, UI performance must be measured, and features must be real rather than decorative demos.

### Product experience

- Minimal, familiar and fast for everyday users.
- Advanced capabilities are available without overwhelming the default interface.
- Navigation and file operations should be predictable and recoverable.
- Search is a first-class workflow, not an afterthought.
- Local-first operation; no mandatory account, subscription or cloud dependency.
- Accessible by keyboard and assistive technology, high-DPI aware, responsive under large directories.

### Competitive intent

Study and borrow the strongest documented product patterns:

- Everything: disk-wide filename search and query expressiveness.
- File Pilot: responsive interaction and deliberate, compact interface.
- Files: modern presentation and integrations.
- Directory Opus: advanced workflows, customization and power-user tools.
- Windows Explorer/Finder: discoverability, familiar navigation, shell integration and previews.

Do not claim GodXplr is globally “better” without a defined benchmark, feature comparison and repeatable evidence. The product should aim to outperform competitors on selected measurable workloads while reaching broad feature parity over time.

## 2. Primary users and platforms

| Property | Contract |
|---|---|
| Primary users | Everyday Windows users; advanced features for power users |
| Primary OS | Windows 10/11 (exact supported builds must be specified at release) |
| Secondary platforms | macOS/Linux compile and CI targets initially; unsupported for end users until native implementation and real-machine testing pass |
| Minimum design target | 8 GB RAM and SSD; must be validated on representative low-end hardware |
| Development environment | Antigravity IDE and GitHub/Git; the user tests on Windows |
| Coding language | C++20 |
| UI framework baseline | Qt 6 Widgets, custom `QAbstractItemView` file surface |
| Product license intent | MIT, with dependency obligations separately checked |
| Distribution | Installer and portable mode planned; installer technology must be confirmed in a packaging ADR |

No code may assume the developer's personal CPU, GPU, memory or storage. Hardware details are only for reproducible benchmark records.

## 3. Non-negotiable product constraints

1. No AI is required for filename search or content search.
2. No paid API or cloud service is required for core functionality.
3. Search must work locally and remain useful without network connectivity.
4. Main UI must remain responsive while enumerating, sorting, searching, extracting thumbnails or performing file operations.
5. File operations must have explicit error handling and truthful state reporting.
6. Do not access or index cloud placeholders in a way that unexpectedly downloads remote data.
7. Do not expose files or content that the signed-in user is not authorized to access.
8. Every third-party dependency must pass version, license, security, build and maintenance review.
9. Windows-specific code is not considered validated by a Linux compile.
10. Do not silently remove Explorer-parity or power features. Defer them in a visible roadmap with an owner and dependency.

## 4. Product scope

The long-term target is a complete file-manager experience, not a search utility with a file list.

### 4.1 Core navigation and Explorer parity

- Home, Quick Access/favorites, This PC, drives, directories and folder tree.
- Tabs, address/breadcrumb bar, back/forward/up, refresh and reopen-closed-tab behaviour.
- Details, list, icons/grid and gallery/preview-oriented views.
- Sort, group, filter, per-folder view preferences and visible metadata columns.
- File/folder create, open, copy, move, rename, delete, restore and properties.
- Clipboard, drag-and-drop, multi-selection, keyboard navigation and context menus.
- Removable storage, external disks, network shares and cloud-backed folders through provider-specific support.
- Recycle Bin, permissions/security properties and shell integrations where supported.

### 4.2 Search

- Disk-wide filename search with NTFS MFT/USN indexing as the primary Windows path.
- Non-NTFS and portable-mode crawler fallback.
- Everything-inspired syntax: whitespace AND, `|` OR, `!` NOT, grouping, quoted phrases, wildcards, `regex:`, `size:`, date operators (`dm:`, `dc:`, `da:`), `ext:`, `path:`, `parent:`, `file:`, `folder:`, `attrib:`, `case:`, `nocase:`, `wholeword:` and content hooks.
- Exact and substring matches rank above fuzzy matches. Fuzzy matching is always available but cancellable and bounded.
- Recency/frequency ranking is local, transparent/configurable and disableable.
- Query semantics must be verified against official Everything behaviour before claiming compatibility.
- Content search uses local extractors/indexing only; no LLM/RAG/AI requirement.

### 4.3 Content index

- Persistent, per-user content indexing for locally accessible files whose text can be extracted.
- Extractors planned for text/code, OOXML, ODF, RTF, EPUB, PDF, archives and optional OCR.
- Encrypted, binary, unsupported, corrupt, over-cap and inaccessible files must be reported truthfully.
- Extraction of untrusted file formats must be isolated in child processes with resource limits and watchdogs.
- Exclusion rules and caps must be visible, user-configurable and documented.

### 4.4 Advanced features (progressive disclosure)

Potential long-term feature set includes dual-pane browsing, batch rename/macros, duplicate detection, folder comparison/synchronization, tags/labels/collections, metadata editing, saved searches/virtual folders, scripting, archive browsing, FTP/SFTP, SMB/network providers, MTP, Git integration and cloud-provider status. These features require their own detailed specifications before implementation; a bullet in the scope list is not an adequate implementation contract.

## 5. Technology baseline and constraints

### 5.1 Application

- C++20.
- Qt 6 Widgets for application shell.
- Custom `QAbstractItemView` file surface and `QAbstractTableModel`-compatible model contract.
- No `QFileSystemModel` for the authoritative large-directory surface.
- No one-widget-per-file design.
- UI painting is visible-range only and performs no file I/O, Shell COM, thumbnail decoding, sorting or blocking IPC.
- Avoid Qt private APIs as foundational dependencies. In particular, QRhi is not the default renderer because its private API compatibility is not guaranteed.

### 5.2 Filename index

- Spec 01 is the source technical baseline.
- NTFS index owner: `godxplr-indexd.exe` service process.
- UI queries through a versioned named-pipe protocol; it must not copy the entire volume index into the UI process.
- NTFS initial enumeration uses USN enumeration in the safe initial path; raw MFT parsing is a later gated optimisation.
- USN journal tailing handles incremental updates; journal invalidation/wrap requires a rebuild/reconciliation path.
- Non-NTFS/portable fallback uses filesystem enumeration and change notifications.
- Search core is Qt-free C++20.

### 5.3 Content index

- Spec 02 is the source technical baseline.
- Per-user `contentd`; extractor child process isolation.
- Per-volume persistent index databases and resumable queue.
- FTS5 trigram is a candidate, not an unconditional final choice: run C0 feasibility measurement on mixed text corpus before committing.
- Content search must merge/intersect with name-side predicates through explicit APIs; UI must not access SQLite directly.

### 5.4 UI rendering

- Spec 03 is the source technical baseline.
- Qt Widgets shell + custom virtualized file surface.
- Compact directory store with stable identity; row position is never identity.
- Asynchronous enumeration, sorting, metadata, Shell icon extraction and thumbnail work.
- Bounded queues/mailboxes, navigation generations and cancellation.
- Per-frame work should scale with visible entries rather than directory size.
- Benchmark Widgets implementation before considering Qt Quick/native rendering alternatives.

## 6. Architecture boundaries

The following conceptual processes and libraries are expected; names may change only through an accepted ADR:

| Component | Responsibility | Must not own |
|---|---|---|
| `godxplr.exe` | Qt UI, user interactions, navigation and view state | Raw MFT processing; content extraction; long-running I/O in GUI thread |
| `godxplr-core` | Qt-independent domain contracts, filesystem abstractions, operation/search coordination | Widgets or platform-specific rendering |
| `godxplr-indexd.exe` | Privileged NTFS metadata index and name-query service | Arbitrary content extraction or unrestricted file reads |
| `godxplr-contentd.exe` | Per-user content scheduling, indexing and SQLite databases | SYSTEM-level processing |
| `godxplr-extract.exe` | Sandboxed parsing/extraction of one bounded input/job | Network, broad filesystem access or unrestricted resource use |
| Platform providers | Win32, Shell, volume, archive/network provider implementations | Leaking platform types into portable core APIs |

The UI calls core/API interfaces. It does not directly call index service internals or use SQLite connections. Cross-process calls must be versioned, cancellable, bounded and failure-tolerant.

## 7. Quality attributes

- Correctness before speed; speed without correct results is a defect.
- Data safety before convenience; destructive operations require explicit semantics and recoverability.
- Responsiveness under load; no unbounded queue or unbounded per-row object allocation.
- Bounded memory and CPU, especially on the 8 GB design target.
- Strong diagnostics: errors and partial/index-limited states are visible, not hidden.
- Stable IDs and generation-based cancellation for asynchronous views.
- Observability and reproducible tests from the beginning.
- Accessibility, keyboard support, high-DPI, high contrast and reduced motion.
- Graceful degradation when service, volume, provider or extractor is unavailable.

## 8. Performance intentions — not yet proven

The source documents contain provisional targets, including:

- 60 Hz scrolling target with frame-time budget around 16.67 ms.
- First page of a one-million-entry filename search target around 30 ms for a specified query/workload.
- 100k-entry folder first paint and sorted-completion targets.
- Bounded memory per name-index entry and UI cache limits.
- Idle CPU and content indexing resource caps.

These are targets to validate, not currently achieved values. Benchmark methodology and minimum hardware must be frozen before thresholds become release gates. Never tune only to a benchmark if it makes typical user behaviour worse.

## 9. Security and privacy posture

- Least privilege; separate elevated index service from per-user content processing.
- Named pipes need explicit ACLs, remote-client rejection, message size limits and protocol versioning.
- No arbitrary file read API exposed by privileged index service.
- Permission checks must be performed as the calling user; design and validate against ACL edge cases.
- Content DB ACLs are per-user. Do not index secret/key material by default; users can configure exclusions.
- Do not read cloud placeholder content unless the user explicitly directs it and understands the download implication.
- Parser children must have memory/CPU/time limits, no network and narrow file access.
- Log paths and query strings carefully; avoid leaking names/content in telemetry. Telemetry is local and opt-in unless an approved decision says otherwise.

## 10. Current project state

At the time this bootstrap kit was created:

- Spec 01 exists as a Markdown engineering proposal; its skeleton code was not compiled in the source document.
- Spec 02 exists as a Markdown engineering proposal; the content-index feasibility gate and extractor work have not been reported as completed.
- Spec 03 exists as a detailed UI rendering/performance engineering proposal; its performance targets are proposed and must be benchmarked.
- The earlier competitor research report is retained in `docs/research/` as source context.
- No implementation phase is marked complete by this kit.
- Repository-specific state, current build tooling and pre-existing code must be discovered in P00.

## 11. Missing specifications that must be produced before implementation

At minimum, do not invent these subsystem contracts:

1. **File operations engine:** copy/move/delete/rename semantics, atomicity, conflict handling, cancellation, crash recovery, undo/redo, Recycle Bin, cross-volume behaviour, progress and event model.
2. **Filesystem/provider abstraction:** provider identity, enumeration contracts, path semantics, errors, watcher overflow and capability model across local, removable, network, archive and virtual items.
3. **Shell integration:** COM apartment strategy, `IContextMenu` lifecycle, third-party handler isolation, property dialogs, drag/drop data formats and safety boundaries.
4. **Navigation and settings persistence:** tab/session restore, path canonicalisation, invalid-location recovery and migration.
5. **Advanced power features:** batch rename grammar/preview/rollback, duplicate detection hashing, folder comparison/synchronisation safety, tags/metadata model and scripting sandbox.
6. **Archive/virtual filesystem provider:** archive path identity, read/write policy, nested depth, extraction security and large archive behaviour.
7. **Network/cloud/MTP provider strategy:** credentials, offline/placeholder semantics, retries, sync status and provider-specific tests.
8. **Installer/update/default-folder-opening:** service lifecycle, upgrade/rollback, signing, registry changes and reversible Explorer replacement.

Each missing area requires a spec with API contracts, error/failure modes, tests and staged build order before production implementation begins.

## 12. Definition of a successful release

A release is not “all planned buttons are visible”. It is an installable, testable application with reliable core navigation and operations, correct search, responsiveness on declared minimum hardware, safe permission handling, usable keyboard/accessibility support, diagnostics, and reproducible benchmark results. Unsupported or incomplete features must be marked honestly in product documentation.
