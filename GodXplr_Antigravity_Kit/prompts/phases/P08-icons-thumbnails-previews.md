# P08 — Icon, thumbnail and preview pipeline (Spec 03 U6–U7)

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Provide file decoration and preview without introducing UI stalls or unbounded memory.

### Required work
1. Design cache keys that include EntryKey/provider identity, source freshness (mtime/size/version), requested size, theme and device-pixel ratio.
2. Implement bounded LRU icon and thumbnail caches with observable hit/miss/eviction/bytes.
3. Separate cheap file-type icon resolution from expensive thumbnail extraction and preview decoding.
4. Schedule visible items first, then near-viewport prefetch, then background; cancel jobs when tab closes/navigation changes.
5. Use worker/COM apartment rules from the Windows shell API documentation. Never call Shell extraction from paint or GUI input handlers.
6. Decode/downscale to requested target dimensions; defend against huge dimensions, malformed images and decompression bombs.
7. Implement gallery surface/preview pane/inspector incrementally, each behind feature tests.

### Acceptance gate
Cold-thumbnail scrolling remains within the measured frame budget or produces a documented profile; queues and cache are bounded; repeated navigation does not leak memory; stale results rejected; UI thread has no Shell/decoder work; cancellation and shutdown tests pass.

### Stop
Do not integrate content search or shell context menu yet.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P08-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
