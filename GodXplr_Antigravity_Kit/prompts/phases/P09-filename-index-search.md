# P09 — Spec 01 — NTFS index and filename search

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Implement Spec 01 exactly as a gated subsystem. Its skeletons are not assumed to compile. Use the source file `docs/specs/01-ntfs-index-and-search.md` as the primary specification.

### Execution rule
Execute **one S-step at a time**: S0, then S1, …, S12. At the beginning of each run, read the status for the specific S-step and the complete corresponding section. Do not implement multiple S-steps in one session. Stop after each gate.

### Required subsystem principles
- Qt-free C++20 core.
- Correct query parser/normalizer/executor before SIMD optimization.
- Stable entry identity includes full FRN semantics plus volume identity.
- Service owns the full-volume index; UI pages results over versioned named pipe.
- USN enumeration is initial safe path; raw MFT parsing is delayed and differential-tested.
- Journal cursor captured/replayed correctly; invalidation/wrap causes rebuild/reconciliation.
- Exact/substring tiers outrank fuzzy.
- Cancellable queries, bounded result pages and permission-aware service output.
- Non-NTFS/portable fallback is a separate provider path.

### Mandatory pre-implementation checks
1. Resolve or explicitly track Spec 01's 110 B vs ~113 B memory inconsistency.
2. Define exact SQLite/storage/API representation for 64-bit identifiers where applicable.
3. Verify real Windows IOCTL structure/version behaviour from Microsoft documentation and SDK headers.
4. Verify Everything query semantics before compatibility claims.
5. Build the Qt-free core and reference implementation before SIMD.

### Acceptance
Every S-step gate in the spec passes. Windows-specific steps are tested on NTFS VHDX and Windows CI; Linux tests are only for portable core. Store raw benchmark output and update `progress.md` per S-step.

### Stop
Do not integrate UI search until the service/API protocol is stable and tested. Do not proceed past a failed gate.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P09-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
