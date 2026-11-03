# P16 — Reliability, performance and security hardening

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Perform evidence-driven system integration hardening after major subsystems exist.

### Required work
1. Run full unit/integration/regression suite and fuzz targets.
2. Execute NTFS VHDX create/move/rename/delete/hardlink/reparse/journal-wrap/restart/corruption scenarios.
3. Execute content adversarial corpus, extractor crash/timeout/resource limits and database corruption/reconciliation tests.
4. Execute file-operation crash injection and recovery matrix.
5. Execute UI 100k/1M synthetic scenarios, multi-tab memory soak, rapid navigation, thumbnail churn, DPI monitor movement and shutdown stress.
6. Execute service/IPC ACL, oversized frame, spoofing, stale client and cross-user tests.
7. Run performance profiles before optimising; do not change architecture to chase one benchmark without ADR.
8. Review dependency licenses, vulnerabilities, provenance and build reproducibility.
9. Fix leaks, races, deadlocks, unbounded queues and stale result bugs before visual polish.

### Acceptance gate
All critical/high security issues resolved or explicitly accepted with mitigation; no known data-loss bug; no unbounded growth in soak; core UX stays responsive under declared benchmark workload; all failures are visible and recoverable; report raw evidence and residual limitations.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P16-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
