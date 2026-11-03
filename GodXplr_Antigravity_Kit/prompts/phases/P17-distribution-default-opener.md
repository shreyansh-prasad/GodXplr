# P17 — Installer, updater, service lifecycle and optional default-folder opening

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Package the product safely only after stable APIs and reliability/security gates exist.

### Required work
1. Produce release/distribution spec and ADR for installer technology and signing/update source.
2. Decide service install, upgrade, repair, rollback and uninstall. Handle existing index DBs and version migration.
3. Provide portable mode with clearly described capability limitations (e.g. no privileged service unless explicitly supported).
4. Include licenses/notices, dependency manifest/SBOM and reproducible build notes.
5. Add crash-safe updater only after signature verification, rollback and interrupted-update handling are designed.
6. Default folder opener/Explorer replacement is optional, explicit, reversible and late. Save original registry state, provide restore and test Windows update resilience. Do not hijack file associations unexpectedly.
7. Ensure no orphan processes, services, pipes, scheduled tasks or registry entries after uninstall.

### Acceptance gate
Fresh install, upgrade, repair, rollback, portable run and uninstall all pass on clean Windows VMs; service privilege/ACLs validated; default opener opt-in and restore verified; package hashes and signatures checked; release notes list known limitations.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P17-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
