# GodXplr — Spec 09: Distribution, Lifecycle & Safe Default Opener

**Document ID:** GX-SPEC-09  
**Status:** Approved Engineering Specification  
**Language:** C++20 / Win32 API  
**Depends on:** Spec 01, Spec 03, Spec 04, Spec 05  

---

## 1. Executive Summary

Spec 09 governs the packaging, installation, update verification, service lifecycle, portable execution, and optional default-folder opening for GodXplr.

### Core Principles
1. **Zero System Pollution:** Uninstallation or removal leaves zero orphaned services, driver hooks, registry residue, or locked files.
2. **First-Class Portable Mode:** When a `godxplr.portable` sentinel file is present alongside `godxplr.exe`, configuration, logs, and database indexes are routed to `./data/`. No system directories or services are touched.
3. **Reversible Default Opener:** Replacing Windows File Explorer as the default folder handler is strictly opt-in, non-destructive, and 100% reversible. Original registry verbs (`Folder\\shell\\open\\command` and `Directory\\shell\\open\\command`) are backed up to a local rollback manifest prior to registration.
4. **Crash-Safe Atomic Updates:** Application updates verify SHA-256 digests and cryptographic signatures prior to staging in a temporary `.gxupdate` transaction folder. Interrupted updates automatically revert to the active release without leaving corrupted binaries.
5. **Privileged Service Lifecycle:** The index helper service (`godxplr-indexd`) runs under LocalSystem or dedicated service SID with strictly configured DACLs, supporting automated install, start, pause, stop, and clean deregistration.

---

## 2. Component Architecture

### 2.1 Portable Mode Manager (`PortableMode`)
- Checks for the presence of `godxplr.portable` in the executable folder.
- Resolves application paths:
  - Installed mode: `%LOCALAPPDATA%\GodXplr\data`, `%LOCALAPPDATA%\GodXplr\logs`.
  - Portable mode: `<app_dir>\data`, `<app_dir>\logs`.
- Disables automatic service registration prompts in portable mode.

### 2.2 Reversible Default Opener (`RegistryManager`)
- Modifies per-user registry keys under `HKEY_CURRENT_USER\Software\Classes`:
  - `Directory\shell\open\command`
  - `Drive\shell\open\command`
  - `Folder\shell\open\command`
- Reads and preserves previous values into `godxplr-registry-backup.json` prior to applying `godxplr.exe "%1"`.
- Provides `restoreDefaultExplorer()` which restores the backed-up registry values or resets them to the standard Windows Explorer handler (`%SystemRoot%\Explorer.exe "%1"`).

### 2.3 Service Lifecycle Controller (`ServiceController`)
- Interacts with Win32 Service Control Manager (`OpenSCManagerW`).
- Methods:
  - `installService(serviceName, binaryPath, displayName, startType)`
  - `startService(serviceName, timeoutMs)`
  - `stopService(serviceName, timeoutMs)`
  - `uninstallService(serviceName)`
  - `queryStatus(serviceName)`

### 2.4 Update Verifier (`UpdateVerifier`)
- Validates SHA-256 payload integrity before any replacement occurs.
- Writes new binaries to `.gxupdate` directory.
- Replaces binaries via atomic transactional move or swap on restart.
- Restores original binaries if any checksum mismatch or error is encountered.

---

## 3. Verification Criteria
- Unit tests verify registry backup and restore cycles.
- Portable mode path redirection tested with synthetic sentinel.
- Update verifier validates checksum matching and rejects corrupted payloads.
- Service status query and argument formatting verified under standard privileges.
