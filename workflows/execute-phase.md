# Workflow — Execute One Phase

## Before code

- Confirm phase status and gate.
- Inspect relevant code, public interfaces, tests and current commit.
- Write exact acceptance criteria and likely changed files.
- Confirm dependencies and licenses.
- Confirm whether the environment can execute all required tests.

## During code

- Implement one vertical slice.
- Keep source/test/doc changes together.
- Preserve public API compatibility or introduce a versioned change.
- Use bounded queues, cancellation and explicit ownership.
- Never mix unrelated cleanup into the phase.
- If scope must change, stop and use architecture-change workflow.

## After code

- Build affected targets from a clean or documented incremental state.
- Run targeted tests, then relevant regression suite.
- Run sanitizer/fuzzer/performance tests if the phase requires them.
- Inspect `git diff --check`, full diff and generated files.
- Record commands and outputs in `docs/reports/`.
- Update `progress.md`.
- Stop; do not begin the next phase.
