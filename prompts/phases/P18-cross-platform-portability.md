# P18 — Cross-platform portability and support validation

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Validate what can be made portable without compromising the Windows-first product. A CI compile is not user-support proof.

### Required work
1. Audit platform-neutral core boundaries and identify Win32 leakage.
2. Define native filesystem/provider, UI, IPC/service and packaging differences for macOS and Linux.
3. Create platform-specific specs and ADRs for equivalent indexing approaches; NTFS MFT/USN has no direct portable equivalent.
4. Add build/test CI for portable core and platform adapters.
5. Run real-machine integration tests for enumeration, file operations, permissions, drag/drop, tabs, thumbnails, network providers and packaging on each OS before marking supported.
6. Define feature parity matrix per platform; unsupported Windows-only functions must be disabled gracefully and documented.

### Acceptance gate
Only advertise a platform as supported after real-machine test coverage, packaging, accessibility, update and file-operation safety checks pass. CI-only status remains clearly labelled until then.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P18-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
