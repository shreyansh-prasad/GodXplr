# GodXplr — Phase Prompt Index

Run one phase prompt at a time. Each prompt has a stop condition. Phase numbers are the controlling project sequence; subsystem substeps from Specs 01–03 are nested gates.

| Prompt | Purpose | Key dependency / gate |
|---|---|---|
| P00 | Repository audit and source alignment | No code; baseline report complete |
| P01 | Integrated architecture and requirements | ADR conflicts, missing specs and traceability reviewed |
| P02 | Build system, compiler, Qt pinning and CI | Reproducible clean build on Windows CI |
| P03 | UI benchmark harness / telemetry | Deterministic dataset + machine metadata + JSON schema |
| P04 | MainWindow shell and state skeleton | Launch/first-paint; no startup filesystem I/O |
| P05 | Directory store/model/custom virtualized file view | 100k synthetic entries; geometry/selection correctness |
| P06 | Async enumeration, bounded mailbox, generation and sort | Rapid A→B→C navigation cannot show stale content |
| P07 | Sidebar/tree, list/grid/layouts and core interactions | Input, hit-test, keyboard, selection and accessibility gates |
| P08 | Icon/thumbnail pipeline, gallery and previews | Bounded memory/queue; no GUI-thread extraction/decode |
| P09 | Spec 01 NTFS index and search engine | Execute one S0–S12 step at a time; Windows VHDX gates |
| P10 | Spec 02 content index and extractors | Execute one C0–C12 step at a time; C0 must choose engine |
| P11 | File operations engine | Write/review Spec 04 before destructive operation implementation |
| P12 | Windows Shell integration | Dedicated COM/menu/drag-drop/property spec and Windows tests |
| P13 | Power features | Per-feature spec, UX design, tests and undo/safety contracts |
| P14 | Archives and virtual filesystems | Provider contract + archive threat model |
| P15 | Network/cloud/MTP providers | Credential, placeholder, offline and provider security contracts |
| P16 | Reliability, security and performance hardening | Full regression, soak, threat model and benchmark report |
| P17 | Installer, updater, service lifecycle, optional Explorer replacement | Install/upgrade/rollback/uninstall tests; reversible default opener |
| P18 | macOS/Linux portability | Native tests on real machines before support claims |

## Usage syntax

Paste the entire relevant phase prompt into Antigravity. At the start, set:

- `Active phase`: exact phase ID.
- `Active substep`: the first incomplete substep in `progress.md` or the subsystem spec.
- `Current commit`: current HEAD hash.

If progress and code disagree, stop and reconcile the ledger first. Never skip a failed or blocked acceptance gate.
