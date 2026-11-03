# P15 — Network, cloud and MTP providers

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Add optional provider adapters while preserving local-first behaviour and avoiding surprise downloads or credential leakage.

### Required research/spec
- SMB/network shares: latency, disconnected session, credential and permission model, enumeration/caching/watch limitations.
- OneDrive: placeholder attributes, hydration/recall behaviour, cloud status and user intent before downloading.
- Google Drive/Dropbox: official provider APIs/SDK availability, licensing, authentication/refresh token storage, rate limits and local/offline mode. Do not assume uniform Windows filesystem semantics.
- MTP/phone devices: non-filesystem item model, capability differences, cancellation/partial transfer and device disconnection.
- FTP/SFTP if included: credentials, host-key verification, transfer resume, trust and licensing.
- UI indicators for offline, syncing, placeholder, remote and permission states.

### Rules
Provider integrations are optional and must not require paid services for core operation. Never store credentials in plaintext or logs. Do not hydrate cloud placeholders in background indexing without explicit opt-in. Do not expose a provider as full read/write until its capability matrix is tested.

### Acceptance gate
Each provider passes fake-server/device integration tests and documented live smoke tests; credentials are protected; disconnection and retry behaviour is tested; local browsing works with network unavailable.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P15-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
