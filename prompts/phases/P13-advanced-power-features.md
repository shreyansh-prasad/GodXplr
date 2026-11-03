# P13 — Advanced power features

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Add power-user workflows without damaging the minimal default experience. Every feature must have its own requirement, UX and implementation contract.

### Feature candidates
- Batch rename with preview, token/regex rules, collision detection and undo.
- Duplicate finder with safe hash strategy, file identity and hard-link awareness.
- Folder compare/synchronization with dry-run, direction, conflict and deletion controls.
- Tags, ratings, colour labels, collections and saved searches/virtual folders.
- Metadata view/edit (EXIF, media tags, PDF metadata where supported).
- Keyboard mapping, custom layouts and command palette.
- Lua scripting only after sandbox, API stability, permissions and resource limits are specified.

### Required approach
1. Select one feature by dependency and user value; do not implement all at once.
2. Write a feature spec with user workflows, UI states, data schema, APIs, error semantics, security, tests and undo/recovery.
3. Confirm how feature state persists and migrates.
4. Ensure everyday path stays simple; advanced settings live in deliberate surfaces.
5. Add a traceability ID and phase gate.

### Acceptance gate
Each feature has real functionality, test fixtures, failure-path handling, accessibility/keyboard support and documentation. No placeholder buttons presented as complete. Destructive sync/delete features require dry-run and explicit safeguards.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P13-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
