# P00 — Repository audit and source alignment

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Inspect the real repository and the supplied source documents. Produce an accurate baseline. **Do not implement product code, install tools or change machine configuration.**

### Required work
1. Capture `git status`, branch, HEAD, recent commits and existing tree.
2. Identify existing code, tests, CMake files, package managers, CI, format/lint settings and documentation. Preserve existing work.
3. Detect the actual compiler, CMake, Qt SDK, Windows SDK and available test tools. Separate installed from proposed.
4. Read Specs 01–03 completely. Identify their skeleton/uncompiled portions, assumptions, acceptance gates, API dependencies and unresolved technical points.
5. Compare the earlier research report against the later specs. Explicitly record the service-owned-index decision change, memory-budget inconsistency, conditional FTS5 choice and unmeasured UI targets.
6. List missing specifications: filesystem provider, file operations, shell integration, advanced tools, archives/virtual FS, network/cloud/MTP, settings/session persistence, installer/updater.
7. Produce a risk register and a requirement-to-test gap list.

### Deliverables
- `docs/reports/P00-repository-audit.md`
- `docs/reports/P00-toolchain-and-dependency-inventory.md`
- Updated `progress.md`

### Acceptance gate
No product source changes; repository state captured; source conflicts documented; actual toolchain status evidenced; one next phase recommended. If repository is empty, say so. If Qt is missing, report it and do not install it.

### Stop
Report PASS or BLOCKED and stop. Do not start P01.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P00-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
