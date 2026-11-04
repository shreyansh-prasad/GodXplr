# GodXplr Spec 07 — Archives and Virtual Filesystem Providers

**Consumer:** Antigravity agent, Principal Architect, Provider Engineer, Security Reviewer.  
**Status:** SPECIFICATION BASELINE — Approved for Phase P14.  
**Scope:** Virtual Filesystem Provider SPI, Archive Navigation, Security Isolation & Bomb Protection.

---

## 0. Subsystem Contracts & Security Invariants

| Contract Item | Guarantee / Security Rule |
|---|---|
| **Path Traversal Protection** | **Zero path traversal.** Any archive entry containing `..`, leading drive letters (`C:`), leading slashes (`/`, `\`), or embedded NUL bytes is immediately scrubbed or rejected. Extracted paths are strictly confined inside destination roots (`IsParentPath`). |
| **Decompression Bomb Guard** | **Ratio Cap:** Max $100:1$ expansion ratio. **Absolute Cap:** Max $500\text{ MB}$ uncompressed total. **Member Count Cap:** Max $10,000$ items. **Depth Cap:** Max $2$ levels of nested archives. Violation aborts with `DecompressionBombDetected`. |
| **Virtual Tree Navigation** | Archives mount as read-only virtual folders in the UI. Double-clicking a `.zip` or `.tar` presents internal folders and files with instantaneous virtual metadata. |
| **Encrypted & Corrupted Feeds** | Encrypted headers/entries emit `RequiresPassword` or `Encrypted` status without hanging or crashing. Malformed headers emit `CorruptedArchive`. |
| **Provider Capability SPI** | Unified `IVirtualProvider` interface exposing capability bitmasks (`Read`, `Enumerate`, `Seek`, `FastMetadata`). |

---

## 1. Provider Architecture & Interface

```cpp
namespace gx::providers {

enum class ProviderCapability : uint32_t {
    None         = 0,
    Enumerate    = 1 << 0,
    ReadStream   = 1 << 1,
    WriteStream  = 1 << 2,
    RandomSeek   = 1 << 3,
    Delete       = 1 << 4
};

struct VirtualItem {
    std::wstring name;
    std::wstring relativePath;
    uint64_t uncompressedSize{0};
    uint64_t compressedSize{0};
    uint64_t mtime{0};
    bool isDirectory{false};
    bool isEncrypted{false};
    uint32_t crc32{0};
};

class IVirtualProvider {
public:
    virtual ~IVirtualProvider() = default;
    virtual ProviderCapability capabilities() const = 0;
    virtual bool open(const std::wstring& containerPath) = 0;
    virtual std::vector<VirtualItem> enumerateDirectory(const std::wstring& virtualDirPath) = 0;
    virtual bool extractItem(const std::wstring& relativePath, const std::wstring& destDiskPath) = 0;
    virtual void close() = 0;
};

}
```

---

## 2. ZIP Format Engine & Traversal Guard

1. **Central Directory Parsing:**
   - Reads End of Central Directory (EOCD, `0x06054b50`) from file tail.
   - Parses Central Directory Headers (`0x02014b50`), populating in-memory virtual tree.
2. **Security Normalization:**
   ```cpp
   std::wstring sanitizePath(std::string_view rawName) {
       // Replaces backslashes, strips leading '/', prevents '..' segments
   }
   ```
3. **Extraction & Inflation:**
   - Reads Local File Header (`0x04034b50`).
   - Supports Stored (Method 0) and Deflate (Method 8).
   - Verifies CRC32 upon extraction completion.

