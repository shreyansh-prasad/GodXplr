# Spec 03 UI Subphase Map

This map keeps the integrated phase roadmap aligned with the detailed Spec 03 build sequence.

| Spec 03 substep | Covered by project phase | Scope |
|---|---|---|
| U0 | P03 | Benchmark harness, deterministic dataset generator, telemetry recorder |
| U1 | P04 | MainWindow, theme tokens, tab and navigation state |
| U2 | P05 | DirectoryStore, FileListModel, virtualized details view |
| U3–U4 | P06 | Async directory provider, mailbox, generations, cancellation, background sorting |
| U5 | P07 | Tree/sidebar, list/grid, keyboard/mouse/drag-drop foundations |
| U6–U7 | P08 | Icon/thumbnail workers, caches, gallery/preview/inspector |
| U8 | P09/P10 integration tail | Spec 01/02 search adapters and merge; only after stable APIs |
| U9 | P07/P08/P12/P16 | DPI, theme, accessibility and Windows native integration hardening |
| U10 | P16 | Performance tuning and controlled competitor comparison |
| U11 | P16/P17 | Diagnostics, release polish, shutdown and memory soak |

Any remapping requires an ADR if it changes dependency order or subsystem ownership.
