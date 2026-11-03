# GodXplr — Master Feature Inventory & Ownership Matrix

**Document ID:** GX-ARCH-FEATURES  
**Phase:** P01 — Integrated architecture and requirements baseline  
**Date:** 2026-11-03  
**Status:** ACCEPTED BASELINE  
**Author:** Antigravity / Gemini 3.8 Flash High  

---

## 1. Feature Lifecycle Tiers

Every feature in GodXplr belongs to an explicit implementation tier, has an authoritative subsystem owner, and references an approved or required specification.

```text
 ┌────────────────────────────────────────────────────────┐
 │ Tier 1: Core Explorer Parity & 120 FPS Navigation      │
 ├────────────────────────────────────────────────────────┤
 │ Tier 2: Disk-Wide Indexed Search & Content Retrieval   │
 ├────────────────────────────────────────────────────────┤
 │ Tier 3: Native Shell Parity & Transactional File Ops   │
 ├────────────────────────────────────────────────────────┤
 │ Tier 4: Power Tools (Batch Rename, Duplicates, MFT Size│
 ├────────────────────────────────────────────────────────┤
 │ Tier 5: Virtual Providers (Archives, Cloud, Network)   │
 ├────────────────────────────────────────────────────────┤
 │ Tier 6: Distribution, Installer & Reversible Opener    │
 └────────────────────────────────────────────────────────┘
```

---

## 2. Feature Matrix & Specification Mapping

| Feature | Tier | Subsystem Owner | Governing Spec | Phase | Status |
|---|---|---|---|---|---|
| **Custom Virtualized View** | 1 | `ui/view` | Spec 03 | P05 | Specification Complete |
| **Tabbed Workspace & History** | 1 | `ui/shell` | Spec 03 | P04 | Specification Complete |
| **Directory Navigation & Breadcrumbs**| 1 | `ui/shell` | Spec 03 | P04/P06 | Specification Complete |
| **Lazy Sidebar Folder Tree** | 1 | `ui/view` | Spec 03 | P07 | Specification Complete |
| **Async Icon & Thumbnail Pipeline**| 1 | `ui/cache` | Spec 03 | P08 | Specification Complete |
| **Disk-Wide Filename Search** | 2 | `godxplr-indexd` | Spec 01 (ADR-0022) | P09 | Specification Complete |
| **AVX2 SIMD Substring Needle Scan**| 2 | `core/search` | Spec 01 | P09 | Specification Complete |
| **In-Memory Security AccessCheck** | 2 | `godxplr-indexd` | Spec 01 (ADR-0019) | P09 | Specification Complete |
| **Fuzzy / Typo Search Ranking** | 2 | `core/search` | Spec 01 | P09 | Specification Complete |
| **Local Sandboxed Content Search** | 2 | `godxplr-contentd`| Spec 02 (ADR-0024) | P10 | Specification Complete |
| **Extractors (PDF, Office, Code)** | 2 | `godxplr-extract` | Spec 02 | P10 | Specification Complete |
| **Out-of-Process Context Menu** | 3 | `godxplr-shellhost`| Spec 06 (New) | P12 | Spec Required |
| **Transactional File Operations** | 3 | `core/operations` | Spec 04 (New) | P11 | Spec Required |
| **Native Windows Recycle Bin** | 3 | `core/operations` | Spec 04 (New) | P11 | Spec Required |
| **Instant Real-Time Folder Size** | 4 | `godxplr-indexd` | Spec 01 (ADR-0025) | P09/P13 | Architecture Defined |
| **Advanced Batch Renaming** | 4 | `core/power` | Spec 07 (New) | P13 | Spec Required |
| **Duplicate File Finder** | 4 | `core/power` | Spec 07 (New) | P13 | Spec Required |
| **Folder Compare & Sync** | 4 | `core/power` | Spec 07 (New) | P13 | Spec Required |
| **Archive Browsing as Folders** | 5 | `core/providers` | Spec 05 (New) | P14 | Spec Required |
| **Network SMB Shares** | 5 | `core/providers` | Spec 05 (New) | P15 | Spec Required |
| **Cloud Placeholder Status** | 5 | `core/providers` | Spec 05 (New) | P15 | Spec Required |
| **Installer & Service Lifecycle**| 6 | `installer` | Spec 08 (New) | P17 | Spec Required |
| **Reversible Default Opener** | 6 | `installer` | Spec 08 (New) | P17 | Spec Required |

---

## 3. Strict Scope Rule for Agents

An Antigravity agent working on Tier 1 (e.g. Virtualized Viewport in P05) **must not implement speculative hooks for Tier 4 (e.g. Duplicate Finder) or Tier 5 (Cloud Providers)**. Each subsystem must be implemented within its assigned phase according to its approved specification.
