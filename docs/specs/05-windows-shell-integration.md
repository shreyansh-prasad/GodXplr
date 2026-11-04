# GodXplr Spec 05 — Native Windows Shell Integration

**Consumer:** Antigravity agent, Principal Architect, Platform Engineer, UI Engineer.  
**Status:** SPECIFICATION BASELINE — Approved for Phase P12.  
**Dependency:** ADR-0015 (Native Windows Context Menu), ADR-0020 (Out-of-Process Shellhost Isolation).

---

## 0. Subsystem Contract and Invariants

| Contract Item | Guarantee / Architecture |
|---|---|
| **Crash & Hang Isolation (ADR-0020)** | Third-party COM context menu shell extensions execute either in a dedicated STA worker thread with watchdog timeouts or out-of-process in `godxplr-shellhost.exe`. An in-process crash or 10-second hang in a buggy third-party shell handler (e.g. 7-Zip, TortoiseGit, Dropbox) must never crash or freeze GodXplr's main GUI. |
| **COM Apartment Lifetime** | All Shell COM interfaces (`IShellFolder`, `IContextMenu3`, `IDataObject`, `IDropTarget`) are created, invoked, and released strictly on a dedicated Single-Threaded Apartment (STA) thread (`CoInitializeEx(COINIT_APARTMENTTHREADED)`). No raw COM interface pointers cross thread boundaries without `CoMarshalInterThreadInterfaceInStream`. |
| **Explorer Parity** | Context menus feature true Windows Shell parity: native system icons, default bold verbs, nested cascaded submenus, and Win11 modern menu compatibility fallback. |
| **Message Forwarding** | Window message hooks forward `WM_INITMENUPOPUP`, `WM_DRAWITEM`, `WM_MEASUREITEM`, and `WM_MENUCHAR` to `IContextMenu3::HandleMenuMsg2` and `IContextMenu2::HandleMenuMsg` to ensure custom owner-draw items render correctly. |
| **OLE Drag & Drop** | Full standard clipboard and drag-drop format support: `CF_HDROP` (with wide-character `DROPFILES`), `FileGroupDescriptorW`, `FileContents`, and shell drop effects (`DROPEFFECT_COPY`, `DROPEFFECT_MOVE`, `DROPEFFECT_LINK`). |
| **Native Properties** | Invokes genuine Windows property dialogs via `SHObjectProperties(SHOP_FILEPATH)` on background threads without blocking user navigation. |

---

## 1. Architecture & Component Layout

```
 ┌─────────────────────────────────────────────────────────────┐
 │                      UI Window / View                       │
 └──────────────────────────────┬──────────────────────────────┘
                                │ Right Click / Drag Drop
                                ▼
 ┌─────────────────────────────────────────────────────────────┐
 │                   ShellIntegrationService                   │
 │  - STA Thread Controller      - Window Hook & Subclassing   │
 │  - Drop Target Dispatcher     - Out-of-Process ShellHost    │
 └──────────────┬───────────────────────────────┬──────────────┘
                │                               │
                ▼                               ▼
 ┌─────────────────────────────┐ ┌─────────────────────────────┐
 │      ShellContextMenu       │ │      ShellDragDrop          │
 │  - IShellFolder / ITEMID    │ │  - CF_HDROP Packing         │
 │  - IContextMenu3 Forwarding │ │  - IDropTarget Helper       │
 │  - InvokeCommandW           │ │  - Drop Effect Calculation  │
 └─────────────────────────────┘ └─────────────────────────────┘
```

---

## 2. Context Menu Lifecycle & Message Forwarding

### 2.1 PIDL and `IContextMenu` Extraction
1. Given a list of selected file paths:
   - Convert paths to PIDLs using `SHParseDisplayName`.
   - Bind to parent `IShellFolder` via `SHBindToParent`.
   - Obtain child item relative PIDLs.
2. Query `IShellFolder::GetUIObjectOf` for `IID_IContextMenu`.
3. Query interface for `IContextMenu3`, then `IContextMenu2`, storing the highest supported interface pointer.

### 2.2 Menu Population and Tracking
1. Create a Win32 popup menu: `HMENU hMenu = CreatePopupMenu()`.
2. Populate the menu:
   ```cpp
   pcm->QueryContextMenu(
       hMenu,
       0,
       kCmdFirst, // e.g. 1
       kCmdLast,  // e.g. 0x7FFF
       CMF_NORMAL | CMF_CANRENAME | CMF_EXPLORE
   );
   ```
3. Subclass the owner window or install a local window procedure hook:
   - On `WM_INITMENUPOPUP`: Call `pcm3->HandleMenuMsg2(uMsg, wParam, lParam, &lResult)` or `pcm2->HandleMenuMsg`.
   - On `WM_DRAWITEM` / `WM_MEASUREITEM`: Forward to `pcm3` / `pcm2`.
   - On `WM_MENUCHAR`: Forward to `pcm3` / `pcm2`.
4. Display menu via `TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, hwnd, nullptr)`.
5. Execute verb:
   ```cpp
   CMINVOKECOMMANDINFOEX ici{};
   ici.cbSize = sizeof(ici);
   ici.fMask = CMIC_MASK_UNICODE;
   ici.hwnd = hwnd;
   ici.lpVerb = MAKEINTRESOURCEA(cmd - kCmdFirst);
   ici.lpVerbW = MAKEINTRESOURCEW(cmd - kCmdFirst);
   ici.nShow = SW_SHOWNORMAL;
   pcm->InvokeCommand(reinterpret_cast<LPCMINVOKECOMMANDINFO>(&ici));
   ```

---

## 3. Drag and Drop Protocol & Formats

### 3.1 `CF_HDROP` Packing
When dragging files out of GodXplr into Explorer or other apps:
1. Allocate global movable memory: `HGLOBAL hGlobal = GlobalAlloc(GHND, sizeof(DROPFILES) + pathsSizeInBytes + sizeof(wchar_t))`.
2. Setup `DROPFILES`:
   - `pFiles = sizeof(DROPFILES)`.
   - `pt = {0, 0}`.
   - `fNC = FALSE`.
   - `fWide = TRUE` (Unicode).
3. Copy double-NUL-terminated list of UTF-16 file paths into payload.
4. Set clipboard data / `IDataObject` format: `cfFormat = CF_HDROP`, `tymed = TYMED_HGLOBAL`.

### 3.2 Target Drop Effect Evaluation
- Dragging to a folder on the **same drive**: Default `DROPEFFECT_MOVE`.
- Dragging to a folder on a **different drive**: Default `DROPEFFECT_COPY`.
- Dragging with `Ctrl` held: Forces `DROPEFFECT_COPY`.
- Dragging with `Shift` held: Forces `DROPEFFECT_MOVE`.
- Dragging with `Alt` / `Ctrl+Shift` held: Forces `DROPEFFECT_LINK` (Create Shortcut).

---

## 4. Native Property Sheets

Invoking file properties must never block UI rendering:
```cpp
void showFileProperties(HWND hwndParent, const std::wstring& filePath) {
    std::thread([hwndParent, filePath]() {
        ComApartment sta(ApartmentType::STA);
        SHObjectProperties(hwndParent, SHOP_FILEPATH, filePath.c_str(), nullptr);
    }).detach();
}
```

---

## 5. Implementation Roadmap (Phase P12)

1. `ShellContextMenu`: STA menu creation, querying, tracking, and verb invocation with fallback.
2. `ShellDragDrop`: `CF_HDROP` buffer generator and drop-effect calculator.
3. `ShellProperties`: Native properties dialog invoker.
4. Unit and integration tests in `tests/unit/platform/shell_integration_tests.cpp`.

