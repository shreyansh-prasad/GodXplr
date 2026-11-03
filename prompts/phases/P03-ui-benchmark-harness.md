# P03 — UI benchmark harness and telemetry (Spec 03 U0)

**Use:** One Antigravity session for this phase only.  
**Authority:** `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md` and the referenced subsystem specs.

## Session preflight

- Read all root instructions and inspect `git status`, branch and HEAD.
- Confirm this is the next phase in `progress.md`; if not, stop and reconcile.
- Read all named dependencies and inspect the actual source.
- State the phase goal, non-goals, planned files, dependencies, risks and exact gate before editing.
- Do not install packages, modify user machine settings, run destructive tests against real folders/drives or proceed beyond the phase.

### Objective
Implement Spec 03 U0 before UI optimisation work: deterministic large-directory fixtures and measurement infrastructure.

### Required work
1. Read Spec 03 §§15–17 and its U0 gate.
2. Define a versioned benchmark JSON schema for startup, first paint, visible-row paint, input-to-present, scroll frame times, enumeration, sort, cache memory and CPU.
3. Build a deterministic synthetic dataset generator with seed, directory depth, extension mix, Unicode/name length, file count and manifest hash.
4. Add a minimal metrics recorder that can write JSON/CSV without affecting release UI behaviour.
5. Capture environment metadata: commit, compiler, Qt, OS build, CPU, RAM, storage and display scaling.
6. Implement baseline smoke scenarios (10k/100k rows; larger tiers configurable) and verify that repeated generation with same seed has same manifest.
7. Keep telemetry local; do not add network upload.

### Acceptance gate
Deterministic manifest test passes; JSON schema validates; benchmark runner captures raw data; CI build green. A baseline result is stored and clearly labelled synthetic/not a competitor comparison.

### Stop
Do not implement the MainWindow or renderer yet. Stop after U0 gate.

## Required completion report

Use `templates/phase-report-template.md` and save the completed report under `docs/reports/P03-completion.md`. Include actual build/test commands and exit codes. Update `progress.md`; update `decisions.md` only through an accepted ADR. Mark each result PASS, FAIL or BLOCKED honestly. Then stop.
