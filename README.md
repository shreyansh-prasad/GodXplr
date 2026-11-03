# GodXplr — Antigravity Engineering Bootstrap Kit

**Purpose:** Establish the project context, engineering rules, decision record, progress discipline, subsystem references and phased prompts that Antigravity agents must follow while building GodXplr.

> This kit is an engineering control plane, not an implementation claim. It does not contain a completed file explorer. It tells the coding agent how to inspect, design, implement, test and report each phase without inventing missing requirements.

## Start here

1. Read [`Instructions.md`](Instructions.md) completely.
2. Read [`context.md`](context.md), [`agents.md`](agents.md), [`gemini.md`](gemini.md), [`rules.md`](rules.md), [`decisions.md`](decisions.md) and [`progress.md`](progress.md).
3. Read the three source specifications in `docs/specs/` before touching their subsystems.
4. Run [`prompts/MASTER_PROMPT.md`](prompts/MASTER_PROMPT.md) once for repository reconnaissance and alignment.
5. Execute **one phase prompt per Antigravity session** from `prompts/phases/`. Do not skip a phase gate.
6. Use the workflows in `workflows/` for phase execution, review, benchmarking and architecture changes.

## Important source hierarchy

When two documents disagree, do not merge them silently. Apply this hierarchy:

1. A later, explicitly accepted Architecture Decision Record in `decisions.md`.
2. The latest approved subsystem specification in `docs/specs/`.
3. `context.md` and `rules.md` for product-wide constraints.
4. The earlier research report in `docs/research/` as historical research context only.
5. Agent inference — must be labelled and approved before it changes architecture.

The current documents intentionally retain a few open technical questions. See `decisions.md` and the `Known unknowns` sections in each spec. An open question is not permission to guess.

## Current baseline

- Product: GodXplr, a full-featured desktop file manager.
- Primary platform: Windows 10/11; macOS/Linux are CI/portability targets until tested on real machines.
- Language: C++20.
- UI baseline: Qt 6 Widgets + custom virtualized `QAbstractItemView` file surface.
- Search: Everything-inspired local filename search; no AI and no paid search dependency.
- Index architecture: NTFS MFT/USN index in a helper service, queried through versioned IPC; fallback crawlers for non-NTFS/portable mode.
- Content search: per-user service, sandboxed extractor children, persistent local index; FTS5 trigram is a measured feasibility decision, not an assumed win.
- Intended license: MIT for GodXplr; every third-party component must be independently license-reviewed.
- Intended minimum target: 8 GB RAM and SSD. This is a design target to validate, not a claim that every feature will run optimally on every such PC.

## Folder map

```text
GodXplr/
├── README.md
├── Instructions.md
├── context.md
├── agents.md
├── gemini.md
├── rules.md
├── decisions.md
├── progress.md
├── architecture/
│   ├── system-boundaries.md
│   ├── phase-roadmap.md
│   └── requirements-traceability.md
├── docs/
│   ├── specs/                 # Spec 01, 02, 03 source documents
│   └── research/              # Earlier competitor research/source register
├── prompts/
│   ├── MASTER_PROMPT.md
│   ├── PHASE_INDEX.md
│   └── phases/                # One tightly-scoped prompt per project phase
├── workflows/
│   ├── start-session.md
│   ├── execute-phase.md
│   ├── review-and-gate.md
│   ├── benchmark.md
│   ├── architecture-change.md
│   └── release-readiness.md
└── templates/
    ├── adr-template.md
    ├── phase-report-template.md
    ├── benchmark-report-template.md
    └── subsystem-spec-template.md
```

The implementation source tree is created by the agent only after Phase P00/P01 has inspected the actual repository and confirmed the architecture. Do not create speculative production code before then.

## Key truthfulness rule

A line in a spec that says **Target**, **Likely**, **Guessing**, **proposed**, **skeleton**, **not compiled**, **must be verified**, or **must be measured** must retain that status until evidence is produced. Never promote an estimate to a measured result in prose, README, UI, benchmark report or portfolio description without raw evidence.
