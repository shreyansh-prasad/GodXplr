# GodXplr Spec 06 — Advanced Power Features

**Consumer:** Antigravity agent, Principal Architect, UI Engineer.  
**Status:** SPECIFICATION BASELINE — Approved for Phase P13.  
**Features Covered:** Batch Renamer, Fast Multi-Tier Duplicate Finder, Folder Synchronization & Comparison, Command Palette.

---

## 0. Subsystem Contracts

| Feature | Contract / Invariant |
|---|---|
| **Batch Renamer** | Pure functional rule evaluation with real-time preview; dry-run pre-commit collision detection; full rollback via `FileOpsCoordinator` undo stack. |
| **Duplicate Finder** | 3-stage cascade (Size $\to$ Head/Tail Partial Hash $\to$ Full Stream Hash); hard-link and FRN deduplication; zero modification without user dry-run consent. |
| **Folder Synchronizer** | Comparison by size/mtime/hash; direction controls (Left $\to$ Right, Right $\to$ Left, Two-Way); non-destructive dry-run preview before execution. |
| **Command Palette** | Instant $< 1\text{ ms}$ fuzzy lookup across commands, bookmarks, and settings; keyboard-first navigation. |

---

## 1. Batch Renamer Architecture

### 1.1 Transformation Rules
- **Search & Replace:** Plain text or Regex with capture groups (`$1`, `$2`).
- **Token Insertion:**
  - `<counter:start:step:digits>`: Incremental sequence number (e.g. `001`, `002`).
  - `<date:format>`: YYYY-MM-DD or custom formatting.
  - `<ext>`: File extension preservation.
  - `<parent>`: Parent directory name.
- **Case Transformations:** `UPPERCASE`, `lowercase`, `Title Case`, `camelCase`.

### 1.2 Collision Detection & Reversibility
- Before executing: simulates entire batch in memory.
- If two files map to the same name: flags conflict, refuses to commit until resolved.
- Dispatched through `FileOpsCoordinator` as a single compound undo transaction.

---

## 2. Multi-Tier Duplicate Finder

### 2.1 3-Stage Cascade Algorithm
1. **Tier 1 (Size Clustering):** Group all candidate files by exact file size. All size groups with count $= 1$ are immediately discarded ($0\text{ disk reads}$).
2. **Tier 2 (Head/Tail Fast Hash):** For files in groups with count $> 1$, read $4\text{ KB}$ head and $4\text{ KB}$ tail. Compute fast checksum (CRC32/XXH3). Non-matching hashes are split out.
3. **Tier 3 (Full Hash):** Only candidate pairs with matching size and matching partial hash are streamed for full bit-for-bit checksum comparison.
4. **Hard-link Awareness:** Files sharing the same volume serial and FRN (File Reference Number) are tagged as hard-links, not duplicate disk waste.

---

## 3. Folder Synchronization & Comparison

### 3.1 Comparison Engine
- Compares Left Path vs Right Path.
- Status categories per file:
  - `LeftOnly` (present only on Left).
  - `RightOnly` (present only on Right).
  - `Identical` (matching size and mtime).
  - `Different` (size or mtime differs).
  - `Conflict` (both modified since last baseline).

### 3.2 Action Planning
- Generates action plan (`CopyLeftToRight`, `CopyRightToLeft`, `DeleteLeft`, `DeleteRight`, `Skip`).
- Requires explicit user confirmation of the planned actions before execution.

