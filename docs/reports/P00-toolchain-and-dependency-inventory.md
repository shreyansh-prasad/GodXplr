# P00 Report — Toolchain and Dependency Inventory

**Document ID:** GX-REP-P00-TOOLCHAIN  
**Phase:** P00 — Repository audit and source alignment  
**Date:** 2026-11-03  
**Status:** COMPLETE  
**Author:** Antigravity / Gemini 3.8 Flash High  

---

## 1. Local Host Environment (Measured)

```text
Host Architecture      : x86_64 / x64
Operating System       : Microsoft Windows 11 Home Single Language (Build 10.0.26200)
Processor              : 13th Gen Intel(R) Core(TM) i5-13450HX
Core Count             : 10 Physical Cores (6 P-cores + 4 E-cores)
Logical Processors     : 16 Threads
Instruction Sets       : AVX2, FMA3, BMI1, BMI2, POPCNT, SSE4.2, AES-NI
System Memory (RAM)    : 24.0 GB Physical RAM
Storage Topology       : Local High-Speed NVMe Solid State Drive (NTFS)
```

---

## 2. Toolchain Inventory: Installed vs. Missing

| Tool | Required Version | Detected Status | Exact Binary Location |
|---|---|---|---|
| **MSVC C++ Compiler** | $\ge 19.38$ (VS 2022) | **INSTALLED** (v19.44.35224) | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe` |
| **CMake** | $\ge 3.25$ | **INSTALLED** (v3.31.6) | `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe` |
| **Windows SDK** | $\ge 10.0.22000$ | **INSTALLED** (v10.0.26100.0) | `C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0` |
| **Git** | $\ge 2.30$ | **INSTALLED** (v2.52.0.windows.1) | `C:\Program Files\Git\cmd\git.exe` |
| **Qt 6 SDK** | Qt 6.7 or 6.8 (Widgets) | **MISSING** | Not detected in `PATH`, `C:\`, or user profile directories |
| **vcpkg** | Latest / Manifest mode | **MISSING** | Not detected |

---

## 3. Dependency Catalog & License Review

Every planned third-party library has been reviewed against GodXplr's MIT licensing constraint:

| Dependency | Purpose | Target Version | License | License Compatibility | Acquisition Strategy |
|---|---|---|---|---|---|
| **Qt 6 (Core, Gui, Widgets)** | UI Shell & Base Windowing | 6.8.x LTS | LGPLv3 | Compatible via Dynamic Linking | Official Qt Installer / vcpkg |
| **RE2** | Safe Linear-Time Regex Engine | Latest stable | BSD-3-Clause | 100% MIT Compatible | FetchContent / vcpkg |
| **SQLite (FTS5 / WAL)** | Usage DB & Content Shards | 3.46+ | Public Domain / MIT | 100% MIT Compatible | Single amalgamation `sqlite3.c` |
| **Roaring Bitmaps (CRoaring)** | Trigram Inverted Index Bitsets | 4.x+ | Apache-2.0 | 100% MIT Compatible | FetchContent / vcpkg |
| **PDFium** | PDF Text Extraction | Chromium stable | Apache-2.0 | 100% MIT Compatible | Google prebuilt binaries |
| **libarchive** | Archive Browsing (ZIP/7z/tar) | 3.7.x | BSD-2-Clause | 100% MIT Compatible | CMake FetchContent / vcpkg |
| **miniz** | High-Speed ZIP Decompression | 3.0.x | MIT | 100% MIT Compatible | Single-header vendored |
| **pugixml** | OOXML (Office) XML Streaming | 1.14+ | MIT | 100% MIT Compatible | Single-header vendored |
| **Tesseract OCR** | Image Text Extraction (Optional) | 5.4.x | Apache-2.0 | 100% MIT Compatible | Optional addon package |

*Strict Ban:* GPLv3, AGPLv3, or proprietary commercial libraries (e.g. Poppler, MuPDF, Xapian) are strictly forbidden from entering the tree.

---

## 4. Qt 6 Acquisition Recommendation for Phase P02

Since Qt 6 is not yet present on the system, we have two clean paths:
1. **Option 1 (Recommended for Development):** Install Qt 6.8 MSVC 2022 64-bit via the official Qt Online Installer to `C:\Qt\6.8.0\msvc2022_64`.
2. **Option 2 (Headless / Self-Contained):** Use `vcpkg` manifest mode (`vcpkg.json`) to acquire `qtbase[core,gui,widgets]` and build targets automatically via CMake.

*Note for P01/P02:* Headless C++20 core modules (`godxplr-core`, `godxplr-indexd`) and their unit tests do not require Qt and can be compiled and verified using the existing MSVC/CMake toolchain immediately.
