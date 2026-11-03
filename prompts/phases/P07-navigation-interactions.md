# P07 — Sidebar/tree, list/grid layout strategies and core interaction (Spec 03 U5)

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Complete the essential navigation and view interaction layer while preserving the virtualized view architecture.

### Required work
1. Implement a lazy folder tree model; query children on expansion; avoid recursive whole-volume tree loads.
2. Implement sidebar destinations (Home, favorites/Quick Access, This PC, drives and configured locations) from provider contracts.
3. Add list and grid layout strategies. Gallery layout can be a minimal geometry contract only if thumbnail phase is not yet complete.
4. Add keyboard navigation, focus/selection range, multi-select, select-all, rename-entry initiation (operation delegated, not implemented here), mouse hit testing and drag/drop scaffolding.
5. Ensure painted geometry equals hit-test geometry for each layout.
6. Add accessibility names/roles and keyboard-only test scenarios.

### Acceptance gate
Tree expansion is lazy; list/grid hit tests and selection are correct; keyboard-only navigation works; stale nodes/entries are invalidated safely; no unbounded tree enumeration; accessible semantics are present.

### Stop
Do not implement Shell context menus or file operations in this phase.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P07-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
