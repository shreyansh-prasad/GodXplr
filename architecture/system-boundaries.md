# GodXplr — System Boundaries and Ownership Map

## Logical architecture

```text
                         ┌────────────────────────────────┐
                         │          godxplr.exe            │
                         │ Qt 6 UI / navigation / tabs    │
                         │ custom virtualized file view   │
                         └───────┬──────────────┬─────────┘
                                 │              │ pipe IPC (crash isolated)
                                 │ core API     ▼
                                 │     ┌────────────────────────┐
                                 │     │  godxplr-shellhost.exe │
                                 │     │  isolated COM/Shell    │
                                 │     │  IContextMenu3 / props │
                                 │     └────────────────────────┘
                         ┌───────▼────────────────────────┐
                         │           godxplr-core         │
                         │ domain IDs / providers / ops   │
                         │ coordinators / query facade    │
                         └──────┬──────────┬──────────────┘
                                │          │ shared mem + IPC
               ┌────────────────▼──┐   ┌───▼──────────────────────┐
               │ godxplr-indexd    │   │ godxplr-contentd         │
               │ elevated service  │   │ signed-in user process   │
               │ NTFS MFT/USN      │   │ scheduler / roaring index│
               │ filename index    │   │ reconciliation           │
               └───────────────────┘   └────────────┬─────────────┘
                                                    │ bounded job IPC
                                         ┌──────────▼───────────┐
                                         │ godxplr-extract.exe  │
                                         │ restricted child     │
                                         │ parsers / OCR        │
                                         └──────────────────────┘

       Platform adapters: Win32 filesystem / Shell / COM / volume / provider APIs
       Test tools: VHDX harness / synthetic datasets / fuzzers / benchmark runners
```

This is a logical target, not a claim that all processes already exist. Process names, exact privilege tokens and wire protocols are finalized by subsystem ADRs.

## Dependency direction

- `ui` may depend on `core` public APIs and Qt.
- `core` may depend on abstract provider interfaces and Qt-free search/domain contracts.
- `platform/windows` implements interfaces owned by core or platform API contracts.
- `indexd` owns the whole-volume name index and USN journal state.
- `contentd` owns content DBs and extraction scheduling; it receives candidates/identity changes through a defined protocol.
- UI never opens content SQLite databases or directly parses NTFS structures.
- Rendering code never performs provider I/O.
- Service and extractor code never depends on UI widgets.

## Data ownership

| Data | Authoritative owner | UI representation |
|---|---|---|
| Full-volume filename index | indexd | paged query results only |
| Current folder enumeration | DirectoryStore/DirectorySnapshot in UI process | stable EntryKey references |
| Content text index | contentd per-user database | search hit/snippet DTOs |
| File operation state | future FileOperationService (Spec 04) | observable operation rows |
| Thumbnail bytes | bounded UI-side cache/provider cache | immutable image handle/texture/value |
| Tab/navigation state | UI state controller | widgets bind to state, not own duplicate state |
| Preferences | settings service/store | validated settings snapshot |

## Cross-cutting contracts

Every async request carries a request ID or generation. Every result includes source identity and enough freshness/version information to reject stale data. Every queue has a finite capacity and policy. Every process boundary validates message size, version, identity and caller access. Failure must degrade a feature rather than corrupt unrelated state.
