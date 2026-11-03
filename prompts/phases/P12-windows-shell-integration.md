# P12 — Native Windows Shell integration

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Integrate GodXplr with Windows Shell capabilities without allowing third-party extensions to freeze or crash the primary UI.

### Prerequisite
Write/review a dedicated Shell integration spec first. Cover COM apartment lifetime, `IShellFolder`/`IContextMenu`/`IContextMenu2`/`IContextMenu3`, menu message forwarding, `InvokeCommand`, Unicode arguments, Shell item identity, property pages, icon/thumbnail APIs, drag/drop data formats, shell namespace objects and failure isolation.

### Implementation rules
- GUI thread may show native menus only through a carefully tested message lifecycle; no slow extraction on paint/input path.
- COM objects must be used only in their owning apartment; no raw interface pointer crosses threads without marshaling.
- Third-party handler timeout/crash strategy must be explicit; a watchdog thread alone cannot safely terminate an in-process hung COM handler.
- Use native Shell APIs where they provide real interoperability; never create a fake menu and call it native.
- Test elevated/unelevated, multiple selections, long Unicode paths, missing handlers and malformed extensions.

### Acceptance gate
Context menu works with Windows built-ins and representative third-party handlers; UI remains responsive under a deliberately slow/failing test handler; properties and drag/drop pass; exact Windows versions are recorded.

### Stop
Do not add Registry verbs or replace Explorer defaults until P17 and the release ADR.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P12-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
