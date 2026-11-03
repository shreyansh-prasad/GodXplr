# Workflow — Release Readiness

A release candidate requires:

- Declared Windows versions and minimum hardware.
- Reproducible Release build and dependency manifest/SBOM.
- MIT license and all third-party notices included; Qt LGPL obligations reviewed for actual distribution.
- Installer, portable build, service install/upgrade/repair/uninstall and rollback tests.
- No orphaned service/process/pipe/database after uninstall or failed upgrade.
- File-operation recovery and destructive-action safety tests pass.
- NTFS VHDX, non-NTFS fallback and removable-volume tests pass.
- Search correctness and content extraction corpus pass.
- UI benchmark Tier A/B/C report stored with machine metadata.
- Accessibility, high-DPI, keyboard and reduced-motion checks pass.
- Security review closes critical/high findings or documents approved mitigations.
- Default Explorer replacement, if shipped, is opt-in and has a tested one-click restore.
- Known limitations and unsupported features are disclosed.
