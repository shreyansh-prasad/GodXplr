# P05 — DirectoryStore, FileListModel and custom virtualized file view (Spec 03 U2)

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Implement the core file surface with synthetic data first. No real filesystem enumeration in this phase.

### Required work
1. Implement `EntryKey` and row DTOs from Spec 03 §1.3/§5. Resolve any type mismatch through P01/API ADR first.
2. Implement DirectorySnapshot/DirectoryStore with compact storage, stable identity lookup and generation-aware replacement/update.
3. Implement FileListModel and custom `FileListView : QAbstractItemView` with geometry/layout contracts.
4. Start with details mode and uniform row height; visible-range-only painting; hit testing; keyboard/mouse selection; scrolling; header/column geometry.
5. Prohibit per-entry QWidget/indexWidget and `QFileSystemModel` as authoritative model.
6. Add layout/selection/scroll geometry tests and 100k synthetic row tests.
7. Instrument paint duration and number of painted entries to prove visible-range behaviour.

### Acceptance gate
100k synthetic entries paint and scroll; selection, hit-testing and keyboard focus are correct; paint work is proportional to visible rows, not N; no file I/O/Shell calls in paint; no per-row widget allocation; benchmark report recorded.

### Stop
Do not add real enumeration, icons, thumbnails or search yet.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P05-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
