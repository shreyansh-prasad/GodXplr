# Workflow — Start a GodXplr Session

1. Open repository root, not a subfolder.
2. Read `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md`, `progress.md`.
3. Run and record: `git status --short --branch`, `git log -5 --oneline`, repository file inventory, current build/test status if safe.
4. Confirm current phase/substep and its prompt.
5. Read all relevant specs and dependencies.
6. If actual source differs from docs, record discrepancy. Do not silently choose one.
7. Write a short session plan: objective, non-goals, files, APIs, risks, commands and gate.
8. Proceed only after scope is internally consistent. Missing product/security decision → BLOCKED and ADR request.
