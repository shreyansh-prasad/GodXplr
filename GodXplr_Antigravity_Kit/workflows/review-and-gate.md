# Workflow — Independent Review and Gate

A phase passes only when all mandatory criteria are met.

## Review checklist

- [ ] Implementation matches source spec and accepted ADRs.
- [ ] No out-of-scope feature or unapproved dependency.
- [ ] Interfaces document ownership, threading, cancellation and errors.
- [ ] No UI-thread blocking work or unbounded queues.
- [ ] File/data safety and permissions considered.
- [ ] Unit and integration tests cover success and failure cases.
- [ ] Windows-only code has Windows evidence or is clearly BLOCKED.
- [ ] Build/test commands and exit codes recorded.
- [ ] Benchmark claims supported by raw output and machine metadata.
- [ ] Documentation and progress ledger updated.

## Gate outcomes

- PASS: all mandatory criteria met, evidence linked.
- FAIL: a test/gate ran and failed; repair in the same phase.
- BLOCKED: external prerequisite or execution environment is absent; supply exact unblock action.
- DEFERRED: only allowed with accepted ADR and explicit roadmap update.

A reviewer must inspect the actual diff, not only the agent's summary. Security-sensitive or destructive-operation work requires an independent review where possible.
