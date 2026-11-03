# P11 — File operations engine — Spec 04 required

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Design and then implement a safe file operations engine. This is a data-safety-critical subsystem and is not sufficiently specified by the existing files.

### Stage 1 — Write Spec 04 and stop for review
Cover:
- Copy, move, rename, create, delete, Recycle Bin, restore, permanent delete and properties changes.
- Same-volume atomic rename versus cross-volume copy-verify-delete.
- Conflict detection and policies (skip, replace, keep both, compare, user decision).
- Progress in bytes/items, pause/resume/cancel and cancellation boundary semantics.
- Partial failures, locked files, access denied, disappearing disks and path-length/Unicode cases.
- Operation journal, crash recovery, idempotency, undo/redo and undo expiration.
- ACL/security descriptor preservation policy and alternate streams/hard links/reparse points.
- File verification strategy, hash choice, sparse/compressed/encrypted files and performance cost.
- API, queue, concurrency, backpressure, UI event model, test harness and destructive-test isolation.

### Stage 2 — Implement only after Spec 04 is accepted
Implement one vertical slice at a time with VHDX/temp-directory tests and crash injection.

### Acceptance gate
No destructive operation is implemented until Spec 04 is reviewed/accepted. Every operation reports truthful completion/partial failure; crash recovery does not lose or duplicate work; undo semantics are explicit; destructive tests run only against verified temporary fixtures.

### Stop
If no accepted Spec 04 exists, deliver the spec and stop.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P11-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
