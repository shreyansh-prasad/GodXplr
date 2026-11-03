# GodXplr — Missing Specifications Backlog & Standards

**Document ID:** GX-ARCH-MISSING-SPECS  
**Phase:** P01 — Integrated architecture and requirements baseline  
**Date:** 2026-11-03  
**Status:** ACCEPTED BASELINE  
**Author:** Antigravity / Gemini 3.8 Flash High  

---

## 1. Governance Rule

Any phase dealing with destructive operations, IPC boundaries, or third-party plugins must not begin coding from a one-line feature bullet. A formal specification must be authored in `docs/specs/` using `templates/subsystem-spec-template.md` and approved before implementation commences.

---

## 2. Missing Specifications Inventory

### Spec 04: File Operations Engine (Required for Phase P11)
* **Authoring Timing:** Phase P11 Stage 1.
* **Core Topics:**
  * Intent logging: `.gxjournal` sparse record before modifying filesystem.
  * Same-volume atomic rename vs cross-volume copy-verify-delete.
  * Overlapped asynchronous streaming and Windows 11 I/O Rings (`IORING`).
  * Conflict resolution policies (Skip, Replace, Keep Both, Compare by Hash, Ask User).
  * Smooth EWMA throughput calculation and cancellable progress reporting.
  * Windows Shell Recycle Bin integration (`IFileOperation`).
  * Crash recovery and undo/redo expiration semantics.

### Spec 05: Unified Filesystem Provider Architecture (Required for Phases P14 & P15)
* **Authoring Timing:** Before Phase P14.
* **Core Topics:**
  * Provider interface contract: URI schemes (`file://`, `archive://`, `smb://`, `cloud://`).
  * Capability bits (`CanRead`, `CanWrite`, `CanWatch`, `SupportsRandomAccess`, `IsRemote`).
  * Archive streaming via `libarchive` (ZIP, 7z, tar, RAR read).
  * Cloud placeholder dehydration/hydration safety (avoiding surprise downloads).
  * Network disconnect and offline degradation policies.

### Spec 06: Out-of-Process Shell Host (`godxplr-shellhost.exe`) (Required for Phase P12)
* **Authoring Timing:** Before Phase P12.
* **Core Topics:**
  * COM Single-Threaded Apartment (STA) lifecycle and message pump.
  * `IContextMenu`, `IContextMenu2`, `IContextMenu3` interrogation.
  * Win32 owner-draw menu serialization (`WM_DRAWITEM` and `WM_MEASUREITEM`).
  * Anonymous pipe frame protocol and heartbeat ping/pong.
  * Watchdog timeout (1,000 ms) and child process termination safety.

### Spec 07: Advanced Power Features (Required for Phase P13)
* **Authoring Timing:** Before Phase P13.
* **Core Topics:**
  * Batch Rename regex engine and token variables (`<num>`, `<date>`, `<ext>`).
  * Cryptographic and size-prefiltered duplicate file detector.
  * Two-way folder synchronization and diff engine.
  * Custom command palette and keyboard binding store.

### Spec 08: Distribution, Installer & Service Lifecycle (Required for Phase P17)
* **Authoring Timing:** Before Phase P17.
* **Core Topics:**
  * Inno Setup script architecture and Windows service installation.
  * Portable mode configuration and fallback crawler behavior.
  * Registry verb registration for optional Explorer replacement (`HKCR\Directory\shell`).
  * Clean uninstaller leaving zero orphan services or registry keys.

