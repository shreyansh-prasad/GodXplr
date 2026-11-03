# GodXplr — Requirements Traceability Register

**Document ID:** GX-ARCH-TRACEABILITY  
**Phase:** P01 — Integrated architecture and requirements baseline  
**Date:** 2026-11-03  
**Status:** ACCEPTED BASELINE  

---

| Requirement ID | Description | Source | Subsystem Owner | Verification Test / Gate |
|---|---|---|---|---|
| **GX-NAV-001** | Zero-widget virtualized file surface scaling to 1M items | Spec 03 | `ui/view` | `tests/unit/ui/layout_tests` + 100k synthetic paint gate |
| **GX-NAV-002** | Tabbed workspace preserving location, scroll anchor, and selection | Spec 03 | `ui/shell` | `tests/unit/ui/tab_state_tests` |
| **GX-NAV-003** | Bounded UI mailbox preventing GUI stalls during rapid folder navigation | Spec 03 | `ui/async` | Rapid A→B→C navigation cancel stress test |
| **GX-NAV-004** | Asynchronous metadata and thumbnail extraction with zero GUI thread I/O | Spec 03 | `ui/cache` | GUI thread affinity instrumentation |
| **GX-SEARCH-001**| Everything-compatible instant filename search across all NTFS drives | Spec 01 | `godxplr-indexd` | VHDX differential test vs Win32 crawler |
| **GX-SEARCH-002**| Sub-30ms first-page keystroke response at 1M items via AVX2 SIMD | Spec 01 | `core/search` | Automated SIMD needle scan benchmark |
| **GX-SEARCH-003**| In-memory AccessCheck on cached SecurityDescriptors (ADR-0019) | ADR-0019 | `godxplr-indexd` | Multi-user permission security test suite |
| **GX-SEARCH-004**| Zero-overhead search IPC via Shared Memory Ring Buffer (ADR-0022) | ADR-0022 | `ipc/search` | High-frequency typing cancellation test |
| **GX-CONTENT-001**| Local, persistent content search using Roaring Bitmaps (ADR-0024) | ADR-0024 | `godxplr-contentd`| Golden corpus substring test vs brute-force |
| **GX-CONTENT-002**| Sandboxed child-process document extraction with CPU/RAM caps | Spec 02 | `godxplr-extract` | Malicious bomb & timeout crash injection |
| **GX-SHELL-001** | Out-of-process COM context menu isolation (ADR-0020) | ADR-0020 | `godxplr-shellhost`| Artificial COM hang & memory corruption test |
| **GX-OPS-001**   | Overlapped / I/O Ring copy engine accelerating small files (ADR-0023)| ADR-0023 | `core/operations` | 50,000 small-file copy throughput benchmark |
| **GX-OPS-002**   | Intent-journaled file operations with atomic same-volume renames | Spec 04 | `core/operations` | Power-loss & process kill injection test |
| **GX-POWER-001** | Instant recursive folder size computation via MFT pointers (ADR-0025)| ADR-0025 | `godxplr-indexd` | In-memory tree sum vs physical disk verification |
| **GX-POWER-002** | Advanced batch renaming with regex and token formatting | Spec 07 | `core/power` | Batch rename dry-run collision test |
| **GX-SEC-001**   | Privileged service never leaks metadata or contents across ACLs | Spec 01 | `godxplr-indexd` | Non-admin client access denial test |
| **GX-PERF-001**  | Locked 60-120 FPS continuous scrolling on 100k-item folder | Spec 03 | `ui/view` | Frame-time distribution p95 <= 8.3 ms |
| **GX-LIC-001**   | 100% MIT project license; all dependencies strictly non-GPL | Context | All targets | Dependency SBOM and license audit |
