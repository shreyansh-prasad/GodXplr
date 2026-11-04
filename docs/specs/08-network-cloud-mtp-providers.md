# GodXplr Spec 08 — Network, Cloud, and MTP Providers

**Consumer:** Antigravity agent, Principal Architect, Provider Engineer.  
**Status:** SPECIFICATION BASELINE — Approved for Phase P15.  
**Scope:** Cloud Placeholder Detection, Background Hydration Prevention, Network Share Timeout Isolation, MTP Model.

---

## 0. Subsystem Contracts & Security Invariants

| Contract Item | Guarantee / Security Invariant |
|---|---|
| **Zero Surprise Hydration** | GodXplr background indexing and thumbnail extractors **never** open dehydrated cloud placeholders (`RECALL_ON_DATA_ACCESS`, `RECALL_ON_OPEN`, `OFFLINE`). Reading placeholder files triggers cloud network downloads, which incurs bandwidth/battery penalties. |
| **Network Timeout Bounding** | Network share probes and enumeration bound RPC timeouts to $\le 500\text{ ms}$. If an SMB server is unreachable, GodXplr returns `NetworkUnavailable` immediately instead of freezing the GUI for the $30\text{ s}$ Win32 default. |
| **Credential Safety** | Credentials are never stored in plaintext, config files, or debug logs. Network credentials leverage Windows Credential Manager (`CredReadW` / `CredWriteW`). |
| **Portable Device Disconnection** | Disconnection of MTP/USB devices mid-transfer is detected immediately via `ERROR_DEVICE_NOT_CONNECTED`, cleanly terminating transfers without hanging handles. |

---

## 1. Cloud Placeholder Attribute Detection

Windows Cloud Files API (OneDrive, Dropbox, iCloud Drive) flags files with specific NTFS attributes:

```cpp
namespace gx::providers {

enum class CloudSyncState : uint8_t {
    NotCloudFile,
    LocallyAvailable,      // Fully hydrated on disk
    DehydratedPlaceholder, // File metadata only, zero physical disk bytes
    Syncing,
    SyncError
};

class CloudStorageProvider {
public:
    static CloudSyncState detectCloudState(DWORD fileAttributes, DWORD reparseTag);
    static bool isDehydrated(DWORD fileAttributes);
};

}
```

- If `fileAttributes & (FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS | FILE_ATTRIBUTE_RECALL_ON_OPEN | FILE_ATTRIBUTE_OFFLINE)`:
  - File is deemed **Dehydrated**.
  - Background extractors **must skip** opening this file.

