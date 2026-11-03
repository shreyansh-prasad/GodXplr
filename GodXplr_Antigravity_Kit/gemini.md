# Gemini / Antigravity Execution Guidance

## 1. Intended consumer

This repository is built with Antigravity coding agents, with Gemini 3.8 Flash High identified as the intended agent in the project brief. Model names, plan limits and IDE capabilities can change; the instructions here are model-agnostic and must not assume hidden tools or unlimited context.

## 2. Work in small, verifiable slices

- Read repository instructions and source specifications from disk at the start of every session. Do not rely on memory or chat history.
- Work on exactly one phase/substep per run.
- Prefer an end-to-end vertical slice over broad speculative scaffolding.
- Keep individual changes reviewable. Avoid rewriting unrelated files.
- Do not create thousands of lines of code in one response without building and testing.
- Before editing, state the exact files and interfaces expected to change.
- After editing, inspect the diff and run the exact build/test commands.

## 3. Do not hallucinate implementation facts

- If an API's signature, flag, minimum OS version or lifecycle is uncertain, consult official documentation or inspect the installed SDK headers.
- Do not invent Qt classes, Win32 flags, COM interfaces, CMake targets or library APIs.
- Do not infer that a library supports a feature because a similar library does.
- If internet research is needed, prefer primary documentation and include the URL, access date and the specific claim it supports in the source register.
- If an environment cannot run a test, label it NOT RUN/BLOCKED with the exact reason.

## 4. Context preservation

At each phase end, update `progress.md`, the relevant ADR, and the phase report. Include enough detail that another agent can resume without reading the previous chat transcript.

Never rewrite `progress.md` to make a failing phase look complete. Do not delete a previously reported limitation unless new evidence resolves it.

## 5. Code generation discipline

- Prefer small, cohesive classes and explicit value types.
- Do not generate placeholder implementations that return success or empty results in production paths.
- If a stub is necessary for an interface boundary, make its unsupported status explicit and test that behaviour.
- Do not use `TODO` as a substitute for a promised phase deliverable.
- Avoid unsafe casts, raw owning pointers, detached threads and unbounded queues.
- Keep exceptions/errors at documented boundaries; no silent catch-all.
- Use UTF-16 at Windows API boundaries and a clearly defined UTF-8/internal text policy.
- Make cancellation and object lifetime explicit.

## 6. Build and test output

Show real command invocations and exit codes. If CMake configuration fails because Qt is missing, report it; do not produce an assumed-green status. If Windows-only code is not executed on Windows, mark it unvalidated. Do not paste fabricated benchmark numbers.

## 7. Stop behaviour

After the phase report and progress update, stop. Do not continue to the next phase, install extra dependencies, change architecture or run destructive commands unless the active phase explicitly authorizes them.
