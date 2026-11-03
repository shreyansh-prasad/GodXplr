# P14 — Archives and virtual filesystem providers

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Design a provider abstraction and archive browsing implementation. Do not treat an archive as a normal directory without specifying write semantics.

### Required spec topics
- Provider capabilities and item identity.
- Read-only versus writable archives and when changes require full repack.
- ZIP/7z/tar/compressed/rar read support and exact library feature/license limits.
- Nested archive depth and recursion caps.
- Path traversal, symlink, zip-bomb, decompression ratio, file-count, size and CPU limits.
- Virtual item metadata, sort/filter, open/preview, drag/drop and copy-out behaviour.
- Cancellation, random access, streaming, corrupted/encrypted archive errors.
- Write/replace/rename semantics, temp files and crash-safe commit.

### Implementation
Start read-only. Use provider-specific tests and malicious archive corpus. Write support only after explicit consistency/recovery design.

### Acceptance gate
Archive traversal cannot escape extraction root; limits are enforced; huge/corrupt/encrypted inputs fail gracefully; browsing/search integration works through provider APIs; library licenses/provenance verified.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P14-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
