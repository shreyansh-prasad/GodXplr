# ADR-0026: Distribution, Portable Mode, Service Lifecycle and Reversible Default Opener

## Status
ACCEPTED

## Context
GodXplr must be safely distributable both as a self-contained portable application and as an installed system package. Additionally, power users often request making GodXplr the default file manager on Windows. Traditional third-party file managers frequently hijack system registry keys irreversibly, corrupt Windows Shell associations upon uninstall, or crash when Windows Update modifies shell verbs.

## Decision
1. Provide a dual-mode distribution architecture:
   - **Portable Mode:** Triggered by `godxplr.portable` sentinel adjacent to the binary. Zero registry writes, zero Windows service dependency; storage resides entirely within `./data`.
   - **Installed Mode:** MSI/Inno/WiX package with clean per-user defaults, signed executables, and managed uninstallation.
2. Default Explorer replacement is late-stage, opt-in, per-user (`HKCU`), and strictly reversible:
   - Always snapshot existing registry verb states to disk prior to overwriting `Directory\shell\open\command`.
   - Provide a 1-click restore mechanism and automated rollback during uninstallation.
3. Index service lifecycle management uses standard Win32 SCM APIs (`OpenSCManagerW`, `CreateServiceW`) with restricted DACLs and explicit timeout boundaries.
4. Auto-update checks verify SHA-256 digests and stage files into an isolated `.gxupdate` directory before atomic restart-replacement.

## Consequences
- No system corruption or registry hijacking.
- Clean uninstallation leaves zero residual services or file association issues.
- Portable mode operates seamlessly from USB drives without administrative rights.

## Approver
Lead OS & Distribution Architect (Antigravity Senior OS Developer)

