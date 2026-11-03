# Requirements Traceability Register

This is a starter register. P00/P01 must expand it into test IDs and requirement IDs tied to implementation PRs.

| ID | Requirement | Source | Verification |
|---|---|---|---|
| GX-PROD-001 | Minimal, understandable default file-manager experience | User direction / context | Task-based usability review |
| GX-PLAT-001 | Windows is primary supported platform | User direction | Windows CI + supported-build matrix |
| GX-PLAT-002 | macOS/Linux not called supported until native validation | User direction | Platform-specific test evidence |
| GX-SEARCH-001 | No AI or paid service required for filename/content search | User direction | Offline test + dependency audit |
| GX-SEARCH-002 | NTFS disk-wide search driven by MFT/USN index | Spec 01 | VHDX differential tests |
| GX-SEARCH-003 | Search syntax is Everything-inspired superset | Spec 01 | Golden parser/executor corpus |
| GX-SEARCH-004 | Fuzzy matches below exact/substring tiers | Spec 01 | Ranking test corpus |
| GX-CONTENT-001 | Content index persistent and resumable | Spec 02 | Restart/reconciliation tests |
| GX-CONTENT-002 | Untrusted extraction isolated and resource-limited | Spec 02 | Sandbox adversarial tests |
| GX-UI-001 | No per-entry QWidget in primary file view | Spec 03 | Code review + allocation test |
| GX-UI-002 | Paint cost scales with visible items | Spec 03 | 100k/1M synthetic view benchmark |
| GX-UI-003 | No filesystem/Shell/thumbnail I/O on GUI thread | Spec 03 | Thread-affinity instrumentation |
| GX-UI-004 | Async requests are bounded and stale results rejected | Spec 03 | Navigation/cancel stress tests |
| GX-UI-005 | High DPI, keyboard and accessibility supported | Spec 03 | Multi-DPI, keyboard and accessibility test matrix |
| GX-OPS-001 | File operations recover/report partial failure truthfully | New Spec 04 required | Crash/recovery matrix |
| GX-SEC-001 | Elevated index service does not leak unauthorized paths | Spec 01 | Multi-user ACL tests |
| GX-PERF-001 | Performance claims backed by reproducible benchmarks | All | Raw JSON + machine metadata |
| GX-LIC-001 | MIT project license and compatible dependency review | Context/rules | Dependency/license report |
