# GodXplr — Spec 10: Cross-Platform Portability & Feature Parity Matrix

**Document ID:** GX-SPEC-10  
**Status:** Approved Engineering Specification  
**Language:** C++20 Standard / POSIX / Win32  
**Depends on:** Spec 01, Spec 03, Spec 04, Spec 07, Spec 08  

---

## 1. Executive Summary

GodXplr is architected with a strict separation between **Platform-Neutral Domain Logic** and **Platform-Specific System Adapters**. While Windows (NTFS, USN, MFT, Shell COM, Win32 I/O) is the flagship, primary validation target, all core search, data structures, algorithms, layouts, query parsing, and file operation planning are 100% portable C++20.

---

## 2. Subsystem Parity Matrix

| Subsystem | Windows (Primary Tier 1) | Linux (Tier 2 - Planned) | macOS (Tier 2 - Planned) |
|---|---|---|---|
| **Language Baseline** | C++20 (MSVC 19.44+) | C++20 (GCC 12+ / Clang 16+) | C++20 (Apple Clang 15+) |
| **Fast Name Index** | In-Memory `NameIndex` (SoA) | In-Memory `NameIndex` (SoA) | In-Memory `NameIndex` (SoA) |
| **SIMD Search** | AVX2 / AVX-512 Scan | AVX2 / NEON SIMD Scan | ARM NEON SIMD Scan |
| **Filesystem Change Feed** | NTFS USN Journal Tailing | Linux `fanotify` / `inotify` | macOS `FSEvents` API |
| **Instant Enumeration** | Raw MFT Records ($MFT) | Btrfs/Ext4 `libext2fs` or Crawler | APFS Snapshot Iterator or Crawler |
| **Content Indexing** | Positionless 24-bit Trigrams | Positionless 24-bit Trigrams | Positionless 24-bit Trigrams |
| **Extractor Sandbox** | Win32 Job Objects (128 MB RAM) | Linux `cgroups v2` / `seccomp` | macOS App Sandbox / `sandbox_init`|
| **File Operations** | Win32 Atomic Rename, Preallocation, Overlapped I/O | POSIX `renameat2`, `fallocate` | macOS `clonefile` (APFS CoW) |
| **Crash Recovery WAL** | `godxplr-ops.journal` | `godxplr-ops.journal` | `godxplr-ops.journal` |
| **Shell Context Menus** | Win32 `IContextMenu3` COM | FreeDesktop Desktop Actions / KIO | macOS `NSMenu` Service Items |
| **Drag & Drop** | OLE `CF_HDROP` / `DROPFILES` | FreeDesktop `text/uri-list` | macOS Pasteboard `NSFilenamesPboardType` |
| **UI Presentation** | Native Win32 Raster / Qt 6 | Qt 6 Widgets / Wayland | Qt 6 Widgets / Cocoa |
| **Packaging & Install** | MSI / Inno / WiX / Portable | `.deb` / `.rpm` / Flatpak | `.dmg` / App Bundle |

---

## 3. Portability Boundary Enforcement

1. **Zero Win32 Leakage in Core:**
   - Headers under `src/core/include/` MUST NOT include `<windows.h>`, `<tchar.h>`, `<shellapi.h>`, or any platform SDK header.
   - All string types in `godxplr-core` are standard `std::string` (UTF-8) and `std::string_view`.
2. **Provider SPI Abstraction:**
   - Filesystem enumeration, network probes, and virtual archive traversal implement `IVirtualProvider`.
3. **Graceful Fallbacks:**
   - On non-NTFS volumes or non-Windows hosts, the engine transparently falls back to `DirectoryCrawler` without raising exceptions or terminating.
