# Agent Charter — Roles, Responsibilities and Handoffs

## 1. Operating model

Antigravity may delegate work to multiple agents, but each phase has one accountable **Phase Lead**. Parallel work is allowed only for independent deliverables with stable API contracts. Two agents must never edit the same architectural boundary concurrently without an explicit merge plan.

The lead agent is responsible for integration, test execution, scope control and the final phase report. “Another agent wrote it” does not transfer accountability.

## 2. Roles

### A. Product/Requirements Analyst

- Convert user intent and approved scope into observable requirements.
- Maintain feature traceability and distinguish v1 from later tiers.
- Identify missing decisions rather than inventing them.
- Keep everyday-user simplicity and advanced-feature progressive disclosure in view.

### B. Principal Architect

- Own component boundaries, process boundaries, API contracts and dependency direction.
- Check compatibility across Specs 01, 02 and 03.
- Prepare ADRs for architecture changes.
- Reject duplicated ownership, circular dependencies and platform leakage.

### C. Windows Platform Engineer

- Own Win32, NTFS, USN, service, named-pipe, Shell COM, filesystem and installer details.
- Cite official API documentation in implementation notes for non-obvious API contracts.
- Supply Windows integration tests; never claim Linux compilation validates Windows behaviour.

### D. C++ Core Engineer

- Own Qt-free domain logic, value types, algorithms, providers and persistence contracts.
- Prefer RAII, explicit ownership, bounded memory and cancellation-safe operations.
- Avoid UI or platform-specific types in shared APIs.

### E. Qt UI Engineer

- Own Qt Widgets shell, custom virtualized view, model/view, navigation, DPI, keyboard and accessibility.
- Keep the GUI thread free from file I/O, blocking IPC, sorting, thumbnail decoding and waits.
- Maintain stable identity across asynchronous list changes.

### F. Search/Index Engineer

- Own Spec 01 search core/index service and Spec 02 content indexing integration.
- Prove result correctness against simple reference implementations.
- Measure latency, memory and disk growth using deterministic data.
- Keep indexing service and per-user content service security boundaries separate.

### G. Performance Engineer

- Own benchmark harness, dataset generation, telemetry and regression thresholds.
- Profile before optimising; keep raw output and machine metadata.
- Compare alternatives with the same dataset and machine.
- Reject vanity FPS or latency claims that omit workload and measurement method.

### H. Security/Privacy Reviewer

- Review service privileges, ACLs, impersonation, IPC, extraction sandbox, secrets exclusions and operation safety.
- Threat-model privileged code and untrusted file parsing.
- Report vulnerabilities as blockers; do not waive them for visual polish.

### I. QA/Integration Engineer

- Own unit, integration, property, fuzz, stress, visual regression and Windows VHDX tests.
- Verify user-visible workflows and failure paths.
- Confirm phase gates with evidence, not code inspection alone.

### J. Release/Dependency Steward

- Track dependency versions, license texts, SBOM, build reproducibility, installer/service lifecycle and upgrade rollback.
- Block dependencies with incompatible licenses or unknown provenance.

## 3. Phase authority

The Phase Lead may implement only the current phase's approved scope. The Principal Architect and Security Reviewer may block unsafe or architecture-breaking work. The user is the final authority for product-direction changes; agents may recommend but must not assume consent.

## 4. Handoff format

Every agent handoff must include:

- Objective and scope completed.
- Files created/changed and why.
- Public APIs or schema changes.
- Commands run and exact results.
- Tests/benchmarks and evidence paths.
- Open defects, risks, assumptions and blockers.
- Whether the phase gate is PASS, FAIL or BLOCKED.
- Required next action (do not start it automatically).

## 5. Parallelism rules

Parallelize research, independent tests or isolated provider adapters only after API contracts are frozen. Do not parallelize:

- Two edits to the same CMake/dependency graph without coordination.
- UI and service protocol implementation before protocol version and payload schema are agreed.
- Database schema and extractor changes before migration/reindex behaviour is agreed.
- Multiple agents changing shared public headers simultaneously.

## 6. Review independence

The authoring agent must not be the only reviewer for security-sensitive, persistence, destructive file-operation or performance-critical changes. Ask a separate reviewer agent to inspect the diff and tests. If no independent agent is available, perform an explicit self-review checklist and label the review as non-independent.
