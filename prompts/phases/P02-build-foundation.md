# P02 — Build system, toolchain, dependency and CI foundation

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Create a reproducible C++20/Qt 6 project foundation while preserving any existing repository implementation.

### Required work
1. Based on P00/P01, pin a supported compiler, CMake version, Qt version and dependency acquisition strategy. Do not assume latest versions.
2. Create or adapt CMake targets with clear separation among app, Qt-free core, platform adapters, tests and tools.
3. Add Windows CI as required; add Linux compile CI for Qt-free/core and any portable modules that actually support it. Do not call macOS/Linux application support complete.
4. Add format/lint/static-analysis configuration, warnings, test registration and build presets where practical.
5. Add dependency manifest/license inventory process. Do not add optional extractors/renderers until their phase.
6. Add a minimal app entry point only if needed to prove the toolchain; do not build UI features here.
7. Ensure clean configure/build/test instructions work from a fresh checkout.

### Acceptance gate
A clean CI build succeeds with pinned toolchain; tests execute; warning policy is active; dependency versions and licenses are recorded; no unreviewed binaries or unpinned downloads. Record actual output.

### Stop
Update progress and stop.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P02-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
