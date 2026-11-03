# Engineering Rules — Binding Requirements for All Agents

These rules apply to every source file, test, build script, prompt, documentation change and release artifact.

## R1. Scope and architecture

1. Read the relevant specs and current code before editing.
2. One active phase/substep at a time.
3. No unapproved changes to process boundaries, IPC, persistence schema, public APIs, dependency graph, threading model or supported platforms.
4. Use ADRs for architecture changes.
5. Do not duplicate subsystem ownership. UI does not own MFT indexing or SQLite content storage; renderer does not own file I/O; content extractor does not own UI state.
6. Do not implement a feature from a one-line feature list. Require an implementation spec and acceptance tests.

## R2. C++ and build quality

1. C++20; enable compiler warnings at a strong level for MSVC and Clang/GCC.
2. No warning suppression to hide real defects. Any suppression must be narrow, documented and justified.
3. RAII for handles, locks, COM interfaces, SQLite statements and process/job objects.
4. Prefer value semantics, `std::unique_ptr` for unique ownership and `std::shared_ptr` only where shared lifetime is justified.
5. No detached threads. Every worker has explicit shutdown/cancellation and joined lifetime.
6. No undefined behaviour, unchecked integer arithmetic, unchecked buffer length or unsafe parsing of disk/IPC input.
7. Public interfaces document ownership, thread affinity, cancellation, error semantics and stability guarantees.
8. Keep Qt and Win32 out of the platform-neutral core unless the interface explicitly represents a platform adapter.
9. CMake dependencies must be pinned and reproducible. Do not fetch an unpinned `latest` branch.
10. Code formatting and static-analysis settings are repository-owned and consistent.

## R3. GUI responsiveness

1. GUI thread must never perform blocking filesystem I/O, service IPC, Shell/COM extraction, thumbnail/image decode, full-folder sort, recursive folder-size calculation or wait on worker futures.
2. No `QFileSystemModel` as the authoritative large-directory model.
3. No per-file widget or `setIndexWidget` for file rows/cards.
4. Main file surface paints only visible items plus a bounded prefetch margin.
5. `paintEvent()` must be deterministic, bounded and side-effect free; it may read immutable/cached row presentation data only.
6. Use `update()`/viewport invalidation, not ordinary-path `repaint()` loops.
7. All GUI model/view mutations happen on the GUI thread.
8. Use stable `EntryKey`, not row index, for selection, open tabs, job association and operation tracking.
9. Batches, image queues, metadata queues and mailboxes must be bounded and expose backpressure/drop/cancel policy.
10. Every async navigation/search/view request carries a generation or request ID. Stale results are discarded.
11. Avoid perpetual decorative animation. Reduced-motion mode and low-resource behaviour are required.

## R4. Filesystem and data safety

1. Treat user files as valuable. Never silently delete, overwrite or move on ambiguous state.
2. File operations require explicit conflict, cancellation, partial-failure and retry semantics.
3. Cross-volume move is copy-verify-delete, not assumed atomic rename.
4. Do not call a file move “complete” before all required steps succeed.
5. Do not implement automatic destructive cleanup as a background convenience.
6. Handle paths, Unicode, long paths, reparse points, hard links, case sensitivity and removable-volume disappearance deliberately.
7. Never follow reparse points recursively without loop and scope protection.
8. Permission checks use the caller's security context; privileged services must not leak metadata/content.
9. Cloud placeholders/offline files must not be opened in a way that triggers a surprise download.
10. All destructive or undo-sensitive operations must be recorded in an operation journal with recovery semantics.

## R5. Search and indexing correctness

1. Query parser, normalizer, planner and executor must be separate testable stages.
2. Search results must be compared with a simple brute-force/reference implementation.
3. Fuzzy matches must never outrank exact/substring tiers unless a future accepted decision explicitly changes this.
4. Query cancellation must propagate into all search workers and IPC requests.
5. USN journal gaps/wrap/identity mismatch require rebuild or reconciliation; never silently continue with stale state.
6. NTFS index identities include full FRN sequence semantics and volume identity.
7. Content index is per-user; extraction children are restricted and time/memory limited.
8. FTS5 trigram selection must pass the documented feasibility test before becoming a hard dependency for production content search.
9. Unknown/unsupported content types must be reported as such, not described as indexed.
10. Exact compatibility with Everything syntax must be verified before claiming it.

## R6. Security and privacy

1. Privileged code is minimal, separately tested and has a narrow IPC surface.
2. IPC frames have version, length limits, request IDs, validation and caller ACL checks.
3. Reject remote clients where local-only semantics are intended.
4. No arbitrary path or file-content read primitive is exposed from the privileged name-index service.
5. Parser children have restricted tokens/AppContainer where feasible, job-object memory/CPU/process limits, timeouts and no network.
6. XML external entities/DTD expansion, archive bombs, malformed paths and oversized input must be guarded.
7. Secrets folders/extensions are excluded by default from content extraction, and exclusions must be user-visible/configurable.
8. Logs must not include extracted content or sensitive search strings by default.
9. Never store credentials in source, tests, settings plaintext or logs.
10. Security review failures are phase blockers.

## R7. Persistence and recovery

1. Every persisted format has a version, validation and corruption response.
2. Writes use documented atomic/transactional semantics; explain Windows filesystem limitations.
3. SQLite transactions and WAL/checkpoint policy are explicit.
4. Crash recovery tests must kill the process at multiple commit points.
5. Do not treat cache corruption as user-data corruption; rebuild caches safely.
6. Schema migrations and extractor version changes need explicit migration/reindex tests.
7. Disk-space thresholds and queue caps must be enforced.

## R8. Dependencies and licensing

1. MIT is the intended GodXplr project license.
2. Every dependency requires exact version, source URL, license, notices, build options, security status and reason for inclusion.
3. GPL/AGPL dependencies are disallowed under the current project policy unless the user approves a license change through an ADR.
4. Qt LGPL obligations must be reviewed for the actual linking/distribution model; do not assume dynamic linking alone automatically satisfies every obligation.
5. Vendored binaries need provenance, hashes, license files and reproducible acquisition instructions.
6. No paid APIs or online services may be required for core functionality.

## R9. Tests and evidence

1. Every bug fix adds a regression test where feasible.
2. Parser, index, operation and persistence code require unit/property/fuzz tests appropriate to risk.
3. Windows-specific paths require Windows tests.
4. Benchmarks use deterministic manifests and record hardware/software metadata.
5. A target passes only when the actual measured result meets the gate with the prescribed workload.
6. Never delete/skip flaky tests to get a green build; diagnose and quarantine with a linked issue/ADR.
7. Tests that mutate disks or directories run only in temporary fixtures/VHDX and validate target identity before destructive steps.

## R10. Documentation and status

1. Update API docs when public contracts change.
2. Update `decisions.md` for accepted architecture changes.
3. Update `progress.md` at every phase end.
4. Distinguish fact, source-derived claim, inference, target and measurement.
5. State what was not tested.
6. No unverified claim such as “faster than Everything” or “100% Explorer parity”.
