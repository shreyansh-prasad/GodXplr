# GodXplr — Integrated Phase Roadmap

The roadmap combines the source build orders in Specs 01–03 with missing file-manager subsystems. The detailed prompts are in `prompts/phases/`. Do not skip acceptance gates to move faster.

| Phase | Scope | Source | Result |
|---|---|---|---|
| P00 | Repository reconnaissance and source alignment | Bootstrap | Accurate inventory, conflicts and blockers |
| P01 | Integrated architecture, requirements and missing-spec plan | All specs | Frozen boundaries, ADRs, traceability |
| P02 | CMake, compiler, Qt pinning, CI and dependency policy | Engineering baseline | Reproducible build foundation |
| P03 | UI benchmark harness and telemetry | Spec 03 U0 | Deterministic data + metric pipeline |
| P04 | MainWindow, tabs, navigation and theme state | Spec 03 U1 | Launchable UI shell |
| P05 | Directory store, model and custom virtualized details surface | Spec 03 U2 | Large synthetic folder view |
| P06 | Async enumeration, bounded mailbox, generations and background sorting | Spec 03 U3–U4 | Responsive real-folder navigation |
| P07 | Tree/sidebar, list/grid layout strategies and input interactions | Spec 03 U5 | Core Explorer interaction patterns |
| P08 | Shell icons, thumbnail pipeline, gallery and preview | Spec 03 U6–U7 | Decorated file views without UI stalls |
| P09 | NTFS filename index, search parser/executor and service integration | Spec 01 S0–S12 | Disk-wide indexed search |
| P10 | Content index, extractors, sandbox and query integration | Spec 02 C0–C12 | Local content search |
| P11 | File operations engine | New Spec 04 | Safe copy/move/rename/delete/undo workflow |
| P12 | Windows Shell integration | New subsystem spec | Native menus, properties, drag/drop and shell interoperability |
| P13 | Advanced power features | New feature specs | Batch rename, duplicates, compare/sync, tags, metadata, scripts |
| P14 | Archive and virtual filesystems | New provider spec | Archive browsing and virtual item support |
| P15 | Network, cloud and MTP providers | New provider/security specs | Optional integrations with explicit offline semantics |
| P16 | Cross-system hardening | All | Reliability, security, memory and performance gates |
| P17 | Installer, updater, service lifecycle and optional Explorer replacement | Release spec | Reversible release package |
| P18 | macOS/Linux portability and actual support validation | Platform specs | End-user support only after native tests |

### Dependency notes

- P03 is intentionally early: measure UI work before implementing expensive UI surfaces.
- P09 and P10 consume the UI/core API boundaries established by P01–P08.
- P11 needs a dedicated Spec 04 before destructive operations are implemented.
- P12–P15 must have detailed subsystem/provider contracts before implementation.
- P17 is late because a privileged service, updater and default-folder opener require stable APIs and recovery behaviour.
- P18 cannot be declared complete from CI compile alone; native filesystem, shell and UI behaviour need real-platform validation.
