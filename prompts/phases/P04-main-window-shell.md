# P04 — MainWindow shell, navigation and state skeleton (Spec 03 U1)

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Build the minimal but coherent application shell and single source of truth for navigation/tab state.

### Required work
1. Implement MainWindow, TabWorkspace, tab strip, navigation bar, address input, search input placeholder, toolbar, sidebar placeholder and status area as Spec 03 describes.
2. Define `TabState`, navigation history, active tab, current location, view mode, sort, filter, column and scroll-anchor state.
3. Persist no complex session schema unless the API is defined; if simple settings are needed, define/version it.
4. Keep UI construction independent of filesystem enumeration. Startup must not crawl drives.
5. Add loading/empty/error/offline/permission-denied status primitives.
6. Wire theme tokens and native window frame; avoid elaborate custom chrome.

### Acceptance gate
Application launches and records first paint; tab/navigation state unit tests pass; startup performs no filesystem I/O; no UI action is a fake working feature—placeholder actions must be clearly disabled/labelled.

### Stop
Do not enumerate files or implement custom file rows yet.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P04-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
