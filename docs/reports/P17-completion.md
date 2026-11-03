# Phase Completion Report: P17 — Distribution, Installer, Service Lifecycle and Default Opener

## 1. Objective and Approved Scope
- **Objective:** Establish the packaging, update verification, service lifecycle, portable mode, and safe reversible default folder opener for GodXplr without system pollution or registry hijacking.
- **Scope Completed:**
  1. Authored `docs/specs/09-distribution-installer-lifecycle.md` (Spec 09) and accepted ADR-0026.
  2. Implemented `PortableMode` (`godxplr.portable` sentinel detection, dynamic `%LOCALAPPDATA%` vs `./data` redirection).
  3. Implemented `RegistryManager` (safe per-user HKCU registry verb management for `Directory\shell\open\command`, automatic backup before changes, and 1-click restore).
  4. Implemented `ServiceController` (Win32 Service Control Manager interface for background index service installation, startup, shutdown, and clean removal).
  5. Implemented `UpdateVerifier` (Cryptographic Win32 SHA-256 computation, manifest validation, `.gxupdate` staging directory isolation, and rollback).
  6. Added `tests/unit/distribution/distribution_tests.cpp` and registered `Distribution.Lifecycle` CTest.

## 2. Files Created and Modified
- **Created:**
  - `docs/specs/09-distribution-installer-lifecycle.md`: Spec 09.
  - `docs/adr/0026-distribution-installer-and-safe-default-opener.md`: ADR-0026.
  - `src/distribution/include/godxplr/distribution/portable_mode.h`
  - `src/distribution/include/godxplr/distribution/registry_manager.h`
  - `src/distribution/include/godxplr/distribution/service_controller.h`
  - `src/distribution/include/godxplr/distribution/update_verifier.h`
  - `src/distribution/src/portable_mode.cpp`
  - `src/distribution/src/registry_manager.cpp`
  - `src/distribution/src/service_controller.cpp`
  - `src/distribution/src/update_verifier.cpp`
  - `src/distribution/CMakeLists.txt`
  - `tests/unit/distribution/distribution_tests.cpp`
- **Modified:**
  - `CMakeLists.txt`: Added `src/distribution` target.
  - `tests/CMakeLists.txt`: Registered `test-distribution` target and `Distribution.Lifecycle` test.
  - `progress.md`: Marked Phase P17 complete.

## 3. Verification Evidence & Exact Commands Run
- **Build Command:**
  ```powershell
  cmake --build "C:\os fileX\build" --config Release --target test-distribution
  ```
  *Result:* Exit code 0. Zero warnings, zero errors under `/W4 /WX /arch:AVX2 /permissive-`.
- **Test Command:**
  ```powershell
  & "C:\os fileX\build\tests\Release\test-distribution.exe"
  ```
  *Result:*
  ```
  ==============================================
    GodXplr Spec 09 Distribution & Lifecycle   
  ==============================================
  [Test] PortableMode Resolution...
    -> PASS
  [Test] UpdateVerifier SHA-256 & Staging...
    Computed SHA-256: ed3fb7c51b4ad81168364d71a3d1f337cf36fe65e379e43b023d895cb5341422
    -> PASS
  [Test] RegistryManager Reversible Operations...
    -> PASS
  [Test] ServiceController Query...
    -> PASS
  ==============================================
    ALL DISTRIBUTION TESTS PASSED (100%)        
  ==============================================
  ```
- **Full Test Suite Execution:**
  ```powershell
  ctest --test-dir "C:\os fileX\build" -C Release --output-on-failure
  ```
  *Result:* **26/26 tests passed (100%)** in 4.45 seconds.

## 4. Security & System Integrity Review
- Default Explorer replacement is strictly opt-in and stores full rollback state to ensure zero system damage upon reversion or uninstall.
- SHA-256 payload verification blocks tampered binaries from staging.
- Portable mode leaves zero trace on host system.

## 5. Phase Gate Verdict
**PASS** — Spec 09 and Phase P17 gates fulfilled with complete evidence.
