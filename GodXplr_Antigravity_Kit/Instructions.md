# Instructions — How to Build GodXplr with Antigravity

## 1. What this kit is for

This kit controls a long-running, multi-subsystem C++/Qt project built with AI coding agents. It is designed to prevent the most common failure mode of AI-built applications: many impressive-looking screens and disconnected features, but no coherent architecture, reliable file operations, measurable performance or verifiable integration.

The goal is not to make the agent produce the maximum amount of code per session. The goal is to make every session leave the repository in a buildable, tested and understandable state.

## 2. First-run procedure

### Step A — Put the kit in the repository root

Copy the contents of this kit into the root of the GodXplr Git repository. Keep the names and directory structure. If the repository already contains useful code or documentation, preserve it; do not overwrite it blindly.

### Step B — Open the repository in Antigravity

Use the repository root as the agent's workspace. Do not start from a single source file or a subfolder because the agent needs repository-wide context.

### Step C — Run the master prompt

Open `prompts/MASTER_PROMPT.md` and give it to the coding agent. Its first assignment is repository reconnaissance and architecture alignment. It must not immediately generate the entire app.

### Step D — Review the P00 report

The agent must identify:

- Current repository state and existing source code.
- Installed compiler, CMake, Qt and Windows SDK availability.
- Existing CI, tests, formatting and dependency management.
- Contradictions among requirements and technical specs.
- Missing subsystem specifications.
- Security, licensing and testability risks.
- The precise next phase and its acceptance gate.

P00 is complete only when the report is saved, reviewed and `progress.md` has been updated. A list of files is not enough; the report must explain architecture implications.

### Step E — Run exactly one phase prompt

Use the next prompt in `prompts/phases/` and do not combine it with the next phase. A prompt may contain a multi-step subsystem sequence (for example Spec 01 S0–S12), but the agent executes only the single current substep and stops at its gate.

## 3. Session discipline

At the start of every session, the agent must:

1. Read `Instructions.md`, `context.md`, `agents.md`, `gemini.md`, `rules.md`, `decisions.md` and `progress.md`.
2. Inspect `git status`, current branch and recent commits.
3. Identify the current phase/substep from `progress.md`.
4. Read only the relevant source specs plus any dependent API contracts.
5. State the objective, non-goals, files expected to change and gate before editing.

At the end of every session, it must:

1. Build the affected targets using documented commands.
2. Run the relevant unit/integration tests.
3. Run static analysis/format checks available in the repository.
4. Report exact commands, exit codes and material output; do not paraphrase a failed command as success.
5. Report changed files, API/architecture changes, known defects and unverified assumptions.
6. Update `progress.md` with PASS, FAIL or BLOCKED and evidence.
7. Stop. Do not continue into another phase automatically.

## 4. Phase state meanings

- **NOT STARTED:** No implementation work has begun.
- **IN PROGRESS:** Work is underway; no gate claim is implied.
- **PASS:** Every mandatory gate passed with recorded evidence.
- **FAIL:** A gate was executed and failed; repair within the same phase.
- **BLOCKED:** A missing decision, dependency, environment, permission or hardware condition prevents a valid gate run.
- **DEFERRED:** Explicitly postponed by an approved decision; not silently skipped.

A compile failure is FAIL, not BLOCKED, if the required toolchain is installed. A Windows-only integration test that cannot run on Linux may be BLOCKED locally, but must be assigned to Windows CI or the user's Windows machine with a reproducible command.

## 5. Do not confuse a mock with a feature

The following are not equivalent:

- A button that displays a toast is not a working feature.
- A mock file list is not filesystem enumeration.
- A search box returning hard-coded rows is not indexed search.
- A progress bar without cancellable work is not an operation manager.
- A context-menu imitation is not the native Windows shell context menu.
- A list of thumbnail placeholders is not a thumbnail pipeline.
- A successful compile is not proof of correct file operations.
- A synthetic benchmark is not a real-world benchmark; label the dataset and test environment.

Mocks are allowed only in named UI development/test fixtures and must be visibly identified in code and tests.

## 6. Decisions and scope changes

Do not allow an agent to rewrite architecture because a simpler implementation is convenient. Any change to process boundaries, persistence format, public interfaces, thread ownership, permissions, dependency/license policy or supported platforms needs an ADR using `templates/adr-template.md` and the `architecture-change` workflow.

A change may be implemented only after the ADR status is ACCEPTED. Rejected and superseded decisions remain in the log for traceability.

## 7. Evidence required for performance claims

Every benchmark report must include:

- Exact commit hash and build configuration.
- Machine CPU, RAM, storage type, OS build, Qt/compiler versions.
- Dataset generator version, seed, entry/file count and dataset manifest hash.
- Cold/warm cache methodology.
- Number of runs and warm-up policy.
- p50, p95 and p99 where appropriate, plus raw output.
- Whether the result was synthetic, VHDX, local real-world or competitor-comparison data.

Never compare results produced on different machines as if they were a controlled A/B test. Never call a proposed threshold an achieved result.

## 8. Windows testing is a hard requirement

The project is Windows-first. Linux compilation of Qt-free code is useful, but it does not validate Win32, NTFS, Shell COM, service, named-pipe, Recycle Bin, ACL, MTP, cloud provider or installer behaviour.

Every Windows-only path must have a Windows CI test or a documented manual test script. Any locally unrun Windows code must be labelled **NOT RUN ON WINDOWS** until actual evidence exists.

## 9. Deliverable expectation

Every completed phase should leave:

- Production code within the agreed architecture.
- Tests and fixtures.
- Relevant technical documentation/API contracts.
- Updated progress and decision records.
- A phase completion report.
- No unrelated cleanup or broad refactor unless explicitly approved.

## 10. Immediate next action

Start with `prompts/MASTER_PROMPT.md`. Do not jump directly to the UI or search implementation. The current source documents contain useful technical proposals, but some skeletons are explicitly uncompiled and some performance/storage assumptions remain unmeasured.
