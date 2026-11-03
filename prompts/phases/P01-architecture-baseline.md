# P01 — Integrated architecture and requirements baseline

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Turn the existing product intent and Specs 01–03 into one coherent, traceable architecture. Resolve only decisions supported by evidence or prior user approval; leave the rest OPEN.

### Required work
1. Produce architecture diagrams for process, module, dependency, data ownership and principal workflows.
2. Define canonical identities and DTO boundaries: file/entry identity, volume/provider identity, request ID/generation, result paging, metadata freshness and errors.
3. Reconcile Spec 01/02/03 interfaces and record mismatches. In particular, inspect 64-bit FRN representation across C++/SQLite, volume identity, IPC request cancellation, content/name result merge, and UI stable `EntryKey` mapping.
4. Create or update ADRs for any decisions that can be accepted now. Do not decide FTS5 until C0 and do not invent operation semantics.
5. Establish a feature inventory: core v1, later Explorer parity, advanced power features, deferred/unsupported. Each feature has an owner subsystem and spec requirement.
6. Produce missing-spec backlog and dependencies.
7. Expand requirements traceability into IDs and proposed test IDs.

### Deliverables
- `architecture/architecture-overview.md`
- `architecture/api-boundaries.md`
- `architecture/feature-inventory.md`
- `architecture/missing-specs.md`
- Updated `decisions.md`, `context.md`, `requirements-traceability.md`, `progress.md`

### Acceptance gate
No circular ownership; every process/data store has one authoritative owner; dependencies point inward; open questions are explicit; all source conflicts are logged; no implementation starts for a subsystem whose semantics remain undefined.

### Stop
Ask for review of the architecture baseline; do not start P02.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P01-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
