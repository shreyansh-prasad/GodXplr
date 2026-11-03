# P06 — Async directory enumeration, bounded mailbox, generations and sorting (Spec 03 U3–U4)

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Connect the view to a real directory-provider abstraction without blocking the GUI thread.

### Required work
1. Produce/confirm the provider interface contract before implementation: capabilities, path/location identity, enumeration batches, cancellation, error outcomes and watch semantics. If absent, write it and stop for review.
2. Implement a Windows directory provider using documented Win32 enumeration APIs, with RAII handles and error mapping.
3. Add bounded UI mailbox/batch queue, backpressure, cancellation and per-navigation generation tokens.
4. Implement A→B→C rapid navigation test; old results must never overwrite current folder.
5. Implement background sort with stable identity, incremental batch arrival and stable selection/scroll anchor restoration.
6. Implement watcher changes only with overflow-to-rescan semantics; do not drop change events silently.
7. Record first row, first paint, enumeration complete and sorted complete separately.

### Acceptance gate
Rapid navigation/cancel stress passes; queue depth bounded; UI thread contains no blocking filesystem operations; sorting/refresh converges to provider truth; permission/offline errors are visible; folder benchmark and raw traces saved.

### Stop
Do not implement destructive file operations; that requires Spec 04.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P06-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
