# P10 — Spec 02 — Content index and extractors

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Implement Spec 02 as a separate per-user, persistent, recoverable content-index subsystem. The source file is `docs/specs/02-content-index-and-extractors.md`.

### Execution rule
Run one C-step at a time: C0 through C12. C0 is mandatory and selects whether FTS5 trigram is acceptable or Plan B is required. Do not pre-implement the extractor pipeline before C0's decision.

### Required principles
- Local-only, no AI and no paid network services.
- Per-user `contentd`; extractor children isolated and resource-limited.
- Persistent queue and reconciliation; missing journal events must not permanently desynchronise the index.
- No surprise cloud placeholder downloads.
- Explicit extraction limits, encrypted/unsupported/denied/corrupt/truncated statuses.
- No content DB access from UI; use a versioned client API.
- Snippets are freshness-checked and permission-checked at display time.
- Exclusions are visible/configurable; default exclusions protect secrets and noisy system/cache folders.

### Mandatory review points
- Validate FRN and volume identity storage representation.
- Verify SQLite version and FTS5 tokenizer/detail/contentless-delete semantics with a test DB.
- Measure index bytes/text bytes on real mixed corpus.
- Verify parser license/provenance and sandbox restrictions.
- Verify child process resource caps on Windows.

### Acceptance
Every C-step gate passes; corpus/golden/fuzz/adversarial tests are stored; C0 decision is recorded in `decisions.md`; DB recovery and reconciliation converge to brute-force truth; Windows sandbox tests have actual evidence.

### Stop
Do not switch the UI to content search until protocol, cancellation, snippets and freshness rules pass end-to-end tests.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P10-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
