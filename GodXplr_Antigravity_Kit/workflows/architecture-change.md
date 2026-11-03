# Workflow — Architecture or API Change

1. Stop implementation at the point the inconsistency is discovered.
2. Create an ADR from `templates/adr-template.md`.
3. State the problem and why the existing accepted decision no longer works.
4. Compare at least two viable alternatives, including operational, security, memory, performance, licensing, migration and maintenance costs.
5. Identify affected specs, APIs, processes, DBs, tests, CI and user-visible behaviour.
6. Provide an implementation and rollback/migration plan.
7. Mark status PROPOSED and request review. Do not implement until ACCEPTED.
8. After acceptance, update `decisions.md`, `context.md`, affected specs and traceability register.
9. Implement in a dedicated phase with migration/compatibility tests.
