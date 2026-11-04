# ADR-0027: Cross-Platform Architecture and Feature Parity Matrix

## Status
ACCEPTED

## Context
GodXplr is designed as a Windows-first high-performance file manager, but its core indexing, search algorithms, content indexing, file operation planning, virtual filesystem providers, and UI virtualization layout mathematics are fundamentally generic. A disciplined boundary prevents Windows-specific types from leaking into domain logic.

## Decision
1. Standardize all platform-independent subsystems (`godxplr-core`, `godxplr-content`, `godxplr-fileops` logic, `godxplr-power`, `godxplr-providers` virtual SPI, `godxplr-ui-state`) on pure standard C++20.
2. Maintain zero Win32 header inclusions in core API boundaries.
3. Establish a tiered platform matrix:
   - **Tier 1 (Windows):** Full hardware acceleration, raw MFT parsing, live USN journal tailing, Shell COM integration, Direct2D/GDI double-buffering.
   - **Tier 2 (Linux & macOS):** Portability and test build targets; standard directory crawler fallback until native fanotify/FSEvents adapters are integrated.
4. Mark non-Windows status as CI/Portability-validated until physical real-machine validation passes on target operating systems.

## Consequences
- Clean compile across MSVC, Clang, and GCC.
- Preserves absolute maximum performance on Windows while enabling cross-platform portability.

## Approver
Lead OS & Architecture Lead (Antigravity Senior OS Developer)

