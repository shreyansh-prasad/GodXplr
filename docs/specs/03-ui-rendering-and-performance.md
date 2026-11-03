# GodXplr — Spec 03: UI Rendering + Performance

**Document ID:** GX-SPEC-03  
**Status:** Engineering baseline proposal; decisions are explicit, measurements are gates  
**Consumer:** Antigravity coding agents (Gemini 3.8 Flash High)  
**Product:** GodXplr  
**Date:** 3 November 2026  
**Depends on:** Spec 01 — NTFS Name Index + Search Engine; Spec 02 — Content Index + Extractors  
**Primary platform:** Windows 10/11, Windows-first  
**Language:** C++20  
**UI framework:** Qt 6 Widgets, with a custom virtualized file view  

> **Purpose:** This is an implementation specification, not a UI mood board. An agent should be able to create the listed components, implement their contracts, run the stated tests, and know exactly what constitutes completion. Any performance number marked **Target** is a proposed acceptance threshold, not a measured result. No result may be described as achieved until the benchmark harness records it on a named machine.

---

## 0. Executive decision

### 0.1 Rendering decision

Use **Qt 6 Widgets for the application shell and a custom `QAbstractItemView` implementation for the primary file surface**. The file surface paints visible rows/cards directly with `QPainter`; it does not create one `QWidget` per file and does not use `QFileSystemModel`.

The initial architecture is:

- Qt Widgets for the main window, navigation, tab strip, toolbars, command/search inputs, splitters, dialogs and accessibility integration.
- `FileListView : QAbstractItemView` for details/list/grid/gallery surfaces. Its `paintEvent()` paints only visible items; its layout strategy determines item geometry and hit-testing.
- `FileListModel : QAbstractTableModel` for the row/index contract and Qt selection integration. It stores compact row references, not one QObject or widget per file.
- A non-Qt `DirectoryStore` / `DirectorySnapshot` owned by the UI process for the currently browsed folder. It is separate from Spec 01's whole-volume index service.
- Asynchronous filesystem, metadata, icon and thumbnail providers. The GUI thread never performs filesystem I/O, shell extraction, sorting of large collections, thumbnail decoding or waits on futures.
- Qt's normal Widgets raster paint path first. Do not insert an OpenGL/D3D child surface merely to claim hardware acceleration.

### 0.2 Why this decision

The goal is not to reproduce File Pilot's source-level architecture. The goal is to achieve a low-cost, testable view with the Qt 6 stack already selected. Qt documents Widgets as using an optimized rasterizer; `QWidget` painting is double-buffered, coalesces `update()` calls and supports dirty-region painting. `QAbstractScrollArea`/`QAbstractItemView` already provide viewport, scroll and selection infrastructure. The main performance risk is therefore not “Qt is slow”; it is **unbounded per-item work, synchronous I/O, excessive QObject/widget allocation, repeated conversion/layout, and repainting more pixels than necessary**.

### 0.3 Explicit prohibitions

Agents MUST NOT:

1. Use `QFileSystemModel` as GodXplr's authoritative directory model.
2. Create one QWidget, `QLabel`, `QCheckBox`, or `QPushButton` per file row/card.
3. Call filesystem, network, shell, COM extraction, thumbnail or metadata APIs from `paintEvent()`, `data()`, input event handlers or the GUI thread.
4. Call `repaint()` in normal UI updates; schedule `update()` / `viewport()->update()` instead.
5. Run full-folder sorting, folder-size traversal, thumbnail decode or icon extraction on the GUI thread.
6. Call `QFuture::waitForFinished()`, `QThread::wait()`, `std::future::get()` or blocking IPC from GUI-thread code.
7. Store a full absolute path as a separately allocated string for every row when a parent directory + name is sufficient.
8. Use `QQuickWidget` as the main file surface or embed it casually in the Widgets hierarchy.
9. Depend on `QRhi` as a core public abstraction. Qt documents QRhi under `QtGuiPrivate` and gives it no source/binary compatibility guarantee.
10. claim a frame-rate, startup, memory or search target has passed without raw benchmark output and machine metadata.

---

## 1. Product and subsystem boundary

### 1.1 What Spec 03 owns

This specification owns the presentation and responsiveness layer:

- Main window composition and window lifetime.
- Navigation shell: sidebar, address/breadcrumb bar, tab strip, toolbar and status area.
- File list, icon/grid and gallery view rendering.
- Folder tree rendering and lazy expansion.
- Viewport virtualization, scrolling, selection, hit testing and keyboard navigation.
- UI-side state for tabs, current folder, sort, filters, columns, view mode and scroll anchor.
- Visual states: loading, empty, permission denied, offline, stale, partial results, errors and background activity.
- UI-thread policy, worker scheduling, bounded queues and cancellation.
- Icon, thumbnail, text-layout and geometry caches.
- DPI, font scaling, accessibility, reduced motion and theme behaviour.
- UI instrumentation, performance benchmarks and rendering regression tests.

### 1.2 What it consumes, but does not own

| Upstream subsystem | UI consumes | UI must not do |
|---|---|---|
| Spec 01 name/index service | Search pages, stable file identity, name metadata, index status, cancellation | Read the MFT, apply USN records, duplicate the full-volume index in the UI |
| Filesystem provider | Directory enumeration batches, file operations, metadata, volume state | Make Win32 calls synchronously from the GUI thread |
| Spec 02 content service | Content-search hits, snippets, index progress and status | Open/extract documents inside the renderer |
| Shell integration | Icons, shell menus, properties, drag/drop formats and native operations | Load arbitrary shell handlers in the paint path |
| File operations subsystem | Operation state, progress, pause/resume/cancel, conflicts and completion | Implement copy/move/delete as view code |

### 1.3 Identity contract

A row number is **not** a file identity. Row numbers change after sorting, filtering, asynchronous inserts, search ranking changes and view-mode changes.

Use a stable `EntryKey`:

```cpp
// src/ui/api/entry_key.h
#pragma once
#include <cstdint>
#include <compare>

namespace gx::ui {

enum class ProviderKind : std::uint8_t {
    Ntfs, Win32Directory, Network, Archive, ShellVirtual, SearchResult
};

struct EntryKey {
    ProviderKind provider{};
    std::uint64_t volumeOrProviderId{};
    std::uint64_t objectId{};       // full FRN for NTFS where available
    std::uint64_t incarnation{};    // provider generation / fallback identity
    auto operator<=>(const EntryKey&) const = default;
};

} // namespace gx::ui
```

For NTFS, `objectId` carries the complete FRN including sequence bits; `volumeOrProviderId` identifies the volume. For a non-NTFS provider, use a provider-defined stable identity and an incarnation token. Do not assume a path hash is collision-free. Selection, open tabs, pending thumbnail jobs and operation progress must be keyed by `EntryKey`, not by row index.

---

## 2. First-principles rendering model

### 2.1 The frame budget

At 60 Hz, one display interval is approximately 16.67 ms. GodXplr must not spend that entire interval doing application work: the window compositor, Qt event dispatch, input processing and other processes also need time.

Use this working budget for a normal scrolling frame:

| Work | Target budget | Notes |
|---|---:|---|
| Input/event handling | ≤ 1.0 ms | Mouse wheel, key repeat, hover and selection updates |
| View/layout calculations | ≤ 1.0 ms | Visible range, row rectangles, columns and clipping |
| File-surface painting | p95 ≤ 6.0 ms | Text, simple icons and selection backgrounds; 100k-row dataset |
| Other visible widgets | ≤ 2.0 ms | Sidebar, toolbar, status and tabs |
| UI-side scheduling/state | ≤ 1.0 ms | Apply queued batches and progress state |
| Remaining headroom | ≥ 5.67 ms | Qt, compositor, driver, OS scheduling and variance |

These are **engineering budgets**, not promises that each event is always processed in the allotted time. Frame-time percentiles and long stalls are measured separately in §16.

### 2.2 Why painting must be proportional to visible content

Let:

- `N` = total entries in the directory;
- `V` = visible entries in the viewport;
- `P` = small prefetch margin;
- `Cpaint` = cost to paint one visible entry.

A correct virtualized view should have paint cost approximately:

`Tpaint = O((V + P) × Cpaint)`

It must **not** be `O(N)` on every scroll, hover or selection change.

At 1080p with 28 logical-pixel rows and a typical viewport height of 850 logical pixels, about 31 rows are visible. A modest prefetch of 2 viewports means a few dozen extra rows, not 100,000 instantiated controls.

### 2.3 Separate costs that are often confused

A folder opening comprises multiple independent activities:

1. Resolve the path/provider and validate the location.
2. Open an enumeration handle.
3. Enumerate directory entries.
4. Append compact rows to the directory store.
5. Resolve metadata that is absent from enumeration data.
6. Sort or group according to the current view settings.
7. Resolve file-type icons.
8. Resolve thumbnails for views that need them.
9. Paint visible rows.

The UI must report and measure these independently. “Folder opened in 100 ms” is meaningless unless it says whether that means first paint, first row, enumeration complete, sorted state or all thumbnails ready.

### 2.4 Latency classes

- **Input latency:** time from user input to the first frame that visibly reflects it.
- **First content:** time until at least one real row/card is visible.
- **Enumeration complete:** all directory entries have arrived.
- **Sorted complete:** the required global order is ready.
- **Decoration complete:** visible icon/thumbnail/metadata work has settled.
- **Navigation complete:** the requested view is interactive and no longer marked busy.

Each must have separate telemetry and test assertions.

---

## 3. Rendering technology decision and alternatives

### 3.1 Decision matrix

| Approach | Strengths | Costs / risks | GodXplr decision |
|---|---|---|---|
| Qt Widgets + custom `QAbstractItemView` | Fits selected stack; native input/focus/accessibility integration; optimized raster painting; custom visible-row renderer; easy shell composition | UI thread performs paint; must control per-row work and cache text/icons; custom view needs careful selection/keyboard tests | **Primary v1/v2 architecture** |
| Qt Widgets + stock `QTableView` / `QListView` | Built-in selection, headers, delegates and less custom code | Generic delegate/model path is less controllable; tempting to add costly roles and index widgets; difficult to own precise tile/gallery layout | Use for small administrative tables only; benchmark before using for the main file surface |
| Qt Quick / QML scene graph | Hardware-accelerated scene graph; useful for highly animated, dynamic surfaces; platform graphics backend selection | Separate UI technology and QML runtime; delegates/bindings can become expensive; requires deliberate model-to-QML boundary | **Benchmark alternative**, not the default |
| `QQuickWidget` inside Widgets | Convenient QWidget integration | Qt documents an additional offscreen render pass and that it disables the threaded render loop on all platforms | **Do not use for the main view**; use only for a small isolated feature if a measured need exists |
| `QOpenGLWidget` | Allows custom OpenGL rendering inside Widgets | FBO composition, context lifetime, alpha/backing-store requirements and platform-specific behaviour; does not automatically make the entire UI faster | Not a default renderer; only a measured isolated use |
| `QRhi` | Qt abstraction over D3D/Vulkan/OpenGL/Metal | Current Qt documentation places it in `QtGuiPrivate` and explicitly gives no source/binary compatibility guarantee | **Not a foundational dependency** |
| Native Win32 + Direct2D/DirectWrite/D3D | Maximum Windows-specific control and potential low-level performance | Windows-only rendering, extra text/input/accessibility/DPI work, custom testing and maintenance; conflicts with a maintainable Qt-first cross-platform shell | Research/benchmark escape hatch only |
| Custom C/C++ renderer like File Pilot | Complete control over rendering and dependencies | Reimplements platform, text, controls, accessibility, windowing and input; enormous engineering surface | Not justified for this project unless Qt fails a documented gate |

### 3.2 Important correction to the old report

Do not write “Qt is slower than File Pilot” as an established fact. Qt's own documentation describes Widgets as using a highly optimized rasterizer; it also documents double buffering, event coalescing and dirty-region painting. The meaningful comparison is between **GodXplr's actual visible-row workload** and File Pilot's actual workload on the same machine.

### 3.3 Why QRhi is not the fallback promised in the earlier report

QRhi is not a stable public Qt API. Depending on `Qt6::GuiPrivate` creates a version-coupled dependency. Therefore, the previous idea “if the view misses 60 FPS, swap only its painter for QRhi” is not accepted as an automatic plan.

If the Widgets view fails after profiling and optimisation, run a separate decision gate:

1. Build a standalone Qt Quick / `QQuickView` prototype using the same dataset and interaction requirements.
2. Benchmark it against the Widgets view.
3. Check accessibility, drag/drop, selection, keyboard navigation, shell menu integration and high-DPI behaviour—not just animation FPS.
4. If Qt Quick wins by a material margin, decide whether the **entire file surface** moves to Qt Quick with a narrow C++ model API.
5. Do not use `QQuickWidget` as a shortcut because it changes rendering behaviour and disables the threaded render loop.
6. If both fail, profile a native D3D/Direct2D prototype as a last resort.

A backend migration is an architecture decision, not a prompt-level styling tweak.

---

## 4. Application UI architecture

### 4.1 Component diagram

```text
┌───────────────────────────────────────────────────────────────────┐
│                         godxplr.exe                                │
│                                                                   │
│  ┌──────────────────────── MainWindow ─────────────────────────┐  │
│  │ Title / native frame                                        │  │
│  │ TabStrip                                                     │  │
│  │ NavigationBar + AddressBar + SearchBox + CommandPalette      │  │
│  │ Toolbar                                                      │  │
│  │ ┌───────────────┐ ┌────────────────────────────────────────┐ │  │
│  │ │ Sidebar       │ │ TabWorkspace                            │ │  │
│  │ │ - Home        │ │ ┌──────────── FileSurface ────────────┐ │ │  │
│  │ │ - Quick access│ │ │ FileListView : QAbstractItemView    │ │ │  │
│  │ │ - This PC     │ │ │ custom paint + virtualized layout   │ │ │  │
│  │ │ - Drives      │ │ └─────────────────────────────────────┘ │ │  │
│  │ │ - Tree        │ │ Optional Preview/Inspector pane        │ │  │
│  │ └───────────────┘ └────────────────────────────────────────┘ │  │
│  │ StatusBar / operation centre                                │  │
│  └─────────────────────────────────────────────────────────────┘  │
│                                                                   │
│  UI state + navigation coordinator + view coordinator             │
│  DirectoryStore + FileListModel + FileListView                     │
│  IconCache / ThumbnailCache / TextLayoutCache                      │
│  Async request broker + bounded UI mailbox                         │
│                                                                   │
│  ┌─────────────┐ ┌───────────────┐ ┌───────────────────────────┐  │
│  │ Directory   │ │ Metadata      │ │ Shell/thumbnail STA       │  │
│  │ provider    │ │ workers       │ │ worker(s)                 │  │
│  └──────┬──────┘ └──────┬────────┘ └─────────────┬─────────────┘  │
└─────────┼──────────────┼────────────────────────┼────────────────┘
          │              │                        │
          ▼              ▼                        ▼
   Win32 filesystem   Win32 metadata        Windows Shell COM APIs
   / future providers  / file handles        / thumbnail cache

   Separate processes from Spec 01 / Spec 02:
   godxplr-indexd.exe  <---- framed named-pipe client ----> UI SearchCoordinator
   godxplr-contentd.exe <--- framed named-pipe client ----> UI SearchCoordinator
```

### 4.2 Layer rules

| Layer | Owns | Must not depend on |
|---|---|---|
| `ui/app` | Process startup, MainWindow, top-level lifetime | Win32 directory enumeration details |
| `ui/shell` | Tabs, sidebar, toolbar, status and commands | Search index internals |
| `ui/view` | FileListView, layout strategies, selection visuals, hit testing | Filesystem handles, SQLite, named-pipe implementation |
| `ui/model` | FileListModel and UI-facing model adapters | QWidget painting |
| `ui/state` | Tab state, navigation state, column/view preferences | Win32 handles |
| `ui/cache` | Decoded images, icons, text layout and geometry caches | Filesystem enumeration |
| `ui/async` | Request broker, bounded mailbox, cancellation and scheduling | Direct QWidget manipulation from workers |
| `core/filesystem` | Provider contracts and directory enumeration | Qt Widgets |
| `core/operations` | Copy/move/delete/rename operation contracts | UI rendering |
| `ipc/index` | Spec 01 client | `QWidget` |
| `ipc/content` | Spec 02 client | `QWidget` |
| `platform/windows` | Win32, Shell, COM and DPI adapters | UI layout policy |

### 4.3 Main window layout

Use a normal resizable Windows top-level window initially. Keep the native title bar and system resize/move behaviour. Custom frameless title bars create work around hit-testing, snap layouts, caption buttons, DPI transitions, accessibility and Windows version differences. Revisit only if a tested design requires it.

Recommended default composition:

1. Native title bar.
2. Tab strip below the title bar.
3. A compact navigation row: Back, Forward, Up, Refresh, breadcrumb/address input and search box.
4. Context-sensitive toolbar with the common actions only.
5. Main split region: sidebar + file surface + optional inspector/preview.
6. Status/operation area at the bottom.

The shell must remain calm and minimal. Advanced actions live in menus, command palette, context menus or optional panes; do not permanently expose dozens of buttons.

### 4.4 Tab state

Each tab owns an independent `TabState`:

```cpp
struct TabState {
    std::uint64_t tabId{};
    std::string providerUri;               // canonical provider URI, not a UI label
    std::vector<std::string> backHistory;
    std::vector<std::string> forwardHistory;
    std::optional<gx::ui::EntryKey> focusedEntry;
    std::vector<gx::ui::EntryKey> selectedEntries;
    gx::ui::ViewMode viewMode;
    gx::ui::SortSpec sort;
    gx::ui::FilterSpec filter;
    gx::ui::ScrollAnchor scrollAnchor;
    gx::ui::ColumnLayout columns;
    bool previewVisible{};
};
```

The implementation may use richer types; the contract is that switching tabs restores location, view settings, selection/focus where still valid, scroll anchor and pane visibility without re-enumerating unnecessarily. Do not keep every tab's entire thumbnail set pinned in RAM. Caches are shared and evictable.

---

## 5. The main file surface: virtualized model and renderer

### 5.1 Chosen Qt pattern

Implement:

- `FileListModel : QAbstractTableModel`
- `FileListView : QAbstractItemView`
- `IFileLayout` with `DetailsLayout`, `ListLayout`, `GridLayout` and `GalleryLayout`
- `DirectoryStore` containing compact entries and the current row-order permutation.

`FileListView` uses Qt's selection model, focus and event infrastructure but **overrides the painting/layout path**. Painting reads compact row data from `DirectoryStore` and only accesses the visible range. No per-item child widgets.

### 5.2 Row data layout

The directory store is not the NTFS name index. It is a bounded, current-folder snapshot for navigation and display.

Suggested compact row:

```cpp
struct NameSlice { std::uint32_t offset; std::uint32_t length; };

enum class MetaState : std::uint8_t { Unknown, Queued, Known, Denied, Failed };

struct DirectoryEntry {
    gx::ui::EntryKey key;       // stable identity
    NameSlice name;             // UTF-16 slice in a shared arena
    std::uint64_t size;         // valid only when sizeState == Known
    std::uint64_t mtime;        // FILETIME or provider-normalized UTC value
    std::uint32_t attributes;
    std::uint32_t flags;        // directory, hidden, reparse, offline, etc.
    MetaState sizeState;
    MetaState timeState;
    std::uint32_t iconKey;      // cache key, not a QIcon/QPixmap
};
```

Rules:

- Keep names in a UTF-16 arena because Windows enumeration supplies UTF-16. Store offsets/lengths, not a heap-allocated `QString` for every row.
- Keep `DirectoryEntry` free of `QObject`, `QWidget`, `QIcon`, `QPixmap`, `QVariant`, `std::function` and per-row heap allocations.
- Keep row order in a separate `std::vector<EntryIndex>` permutation. Sorting reorders the permutation, not the underlying records.
- `EntryKey` remains stable when the order changes.
- Full paths are not repeated in every record. The current folder URI/path is tab-level state; visible child path = parent + name.
- Metadata may be unknown. Unknown size/date values must be displayed as pending/blank and requested asynchronously; never block to fill them.
- Set explicit caps and overflow checks for name offsets, entry counts and arena byte count. If the directory count exceeds `uint32_t` index capacity, fail safely with a clear status; do not wrap.

**Memory budget proposal:** target ≤ 160 bytes per row including row metadata, order index, average name storage and small identity structures; measure actual high-water bytes. A 100k-entry folder should normally stay well below 32 MB of row data; 1M rows may require 160 MB before image caches. Caches have separate budgets (§10). If measured row memory exceeds the target, optimise the layout before raising the budget.

### 5.3 Model roles

The model exposes only stable, necessary roles:

```cpp
enum FileRole : int {
    EntryKeyRole = Qt::UserRole + 1,
    EntryFlagsRole,
    SizeStateRole,
    TimeStateRole,
    SortValueRole,
    ProviderKindRole
};
```

`data()` must be O(1) or bounded and must not perform I/O, path resolution, shell calls, thumbnail extraction, expensive formatting or sorting. It should return a value already present in the snapshot/cache. Display formatting can be performed for visible rows and cached.

### 5.4 View interface skeleton

```cpp
// src/ui/view/file_list_view.h
#pragma once
#include <QAbstractItemView>
#include <memory>

namespace gx::ui {
class DirectoryStore;
class IFileLayout;
class IconCache;
class ThumbnailCache;
class TextLayoutCache;

class FileListView final : public QAbstractItemView {
    Q_OBJECT
public:
    explicit FileListView(QWidget* parent = nullptr);

    void setStore(std::shared_ptr<const DirectoryStore> store);
    void setLayoutStrategy(std::unique_ptr<IFileLayout> layout);
    void setViewMode(ViewMode mode);
    void setDensity(ViewDensity density);

    QRect visualRect(const QModelIndex& index) const override;
    void scrollTo(const QModelIndex& index,
                  ScrollHint hint = EnsureVisible) override;
    QModelIndex indexAt(const QPoint& point) const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    QModelIndex moveCursor(CursorAction action,
                           Qt::KeyboardModifiers modifiers) override;
    int horizontalOffset() const override;
    int verticalOffset() const override;
    bool isIndexHidden(const QModelIndex& index) const override;
    void setSelection(const QRect& rect,
                      QItemSelectionModel::SelectionFlags command) override;
    QRegion visualRegionForSelection(const QItemSelection& selection) const override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void paintVisibleRows(QPainter& painter, const QRegion& dirty);
    void updateScrollRanges();
    void ensureCurrentVisible();

    std::shared_ptr<const DirectoryStore> store_;
    std::unique_ptr<IFileLayout> layout_;
};
} // namespace gx::ui
```

This is a structural skeleton, not a complete implementation. `QAbstractItemView` has additional behavioural contracts; the implementing agent must make the subclass compile and must pass the keyboard/selection/accessibility suite before integrating it into MainWindow.

### 5.5 Layout strategy contract

```cpp
struct VisibleRange {
    std::uint64_t first{};
    std::uint64_t lastExclusive{};
};

class IFileLayout {
public:
    virtual ~IFileLayout() = default;
    virtual void setViewportSize(QSize logicalSize) = 0;
    virtual void setColumns(const ColumnLayout&) = 0;
    virtual QSize contentExtent(std::uint64_t rowCount) const = 0;
    virtual VisibleRange visibleRange(QRect exposed,
                                      std::uint64_t rowCount,
                                      QPoint scrollOffset) const = 0;
    virtual QRect itemRect(std::uint64_t row,
                           QPoint scrollOffset) const = 0;
    virtual std::uint64_t hitTest(QPoint viewportPoint,
                                  std::uint64_t rowCount,
                                  QPoint scrollOffset) const = 0;
    virtual std::vector<QRect> selectionRects(
        std::span<const std::uint64_t> rows,
        QPoint scrollOffset) const = 0;
};
```

All geometry is in Qt logical pixels. Convert to device pixels only at the low-level rendering boundary if the renderer actually requires it. `IFileLayout` must be deterministic and side-effect free: no I/O, no Qt widget creation and no image decoding.

### 5.6 View-mode rules

| Mode | Layout | Virtualization requirement | Default use |
|---|---|---|---|
| Details | Uniform row height + columns | Paint visible row range; horizontal scroll; clip columns | Default for power users and large folders |
| List | Uniform row height, compact columns | Paint visible entries only | Dense browsing |
| Grid / Icons | Fixed-size cards in calculated columns | Calculate visible grid rows; never instantiate each card | General visual browsing |
| Large icons | Fixed-size tiles with larger icon slot | Lazy icon/thumbnail requests near viewport | Images and visual files |
| Gallery | Fixed-size image cards/filmstrip-like layout | Page/row virtualization; thumbnail priority by viewport | Pictures and visual assets |
| Tree | Lazy hierarchical model | Expand only requested directories; fetch children asynchronously | Sidebar/navigation pane |

Do not implement arbitrary variable-height rows in v1. Fixed logical row/card heights make hit testing, scroll mapping and virtualization simpler and more predictable. Variable-height previews can be a later isolated mode.

### 5.7 Scroll model

For a uniform row layout:

`firstVisible = floor(scrollY / rowHeight)`  
`lastVisibleExclusive = min(rowCount, ceil((scrollY + viewportHeight) / rowHeight))`

For the supported v1 directory sizes, use 64-bit calculations internally even though Qt scrollbar values are integer-based. The UI must check range conversion for overflow. For more than the supported scrollbar integer range, use a proportional scrollbar mapping while retaining a 64-bit logical row offset; document the loss of one-to-one scrollbar precision.

Configure pixel scrolling. `scrollContentsBy()` should invalidate only the newly exposed strip when safe; if pixel-shifting a backing image is used, it must handle device-pixel ratio changes, selection overlays and stale thumbnails correctly. Otherwise call `viewport()->update()` and repaint the visible region. Correctness takes priority over a clever partial-scroll optimisation.

---

## 6. Directory open and sort pipeline

### 6.1 End-to-end sequence

```text
User navigates to folder
    │
    ▼
NavigationCoordinator increments navigationGeneration
    │ cancels old folder's enumeration/metadata/icon requests
    ▼
DirectoryProvider::open(uri, requestId)
    │
    ├─ validate provider / permissions / offline state (worker)
    ├─ enumerate entries into compact batches (worker)
    └─ emit DirectoryBatch{requestId, generation, records}
             │
             ▼
       Bounded UiMailbox (not one queued Qt event per file)
             │
             ▼
       UI thread drains one bounded batch per event-loop turn
             │
             ├─ append records to DirectoryStore
             ├─ append temporary display-order entries
             ├─ request visible metadata/icons (bounded)
             └─ schedule layout/paint update
                    │
                    ▼
       SortCoordinator builds sorted run(s) in worker
                    │
                    ▼
       merge sorted runs → new row-order permutation
                    │
                    ▼
       UI applies permutation, restoring selection/scroll anchor by EntryKey
                    │
                    ▼
       Directory status becomes Ready (icons/thumbnails may still be pending)
```

### 6.2 Enumeration provider

Windows local-folder provider uses the appropriate Win32 enumeration APIs in a worker. The prior research proposed `FindFirstFileExW` with `FindExInfoBasic` and `FIND_FIRST_EX_LARGE_FETCH`; this is a candidate optimisation, not a guaranteed speed multiplier. The provider must:

- Keep the enumeration handle on the worker that owns it.
- Handle `ERROR_NO_MORE_FILES`, access denied, path-not-found, media removal and cancellation distinctly.
- Support long paths and Unicode names according to the product's path policy.
- Avoid following reparse points during ordinary enumeration unless the user explicitly navigates into the target.
- Return basic attributes and timestamps already present in enumeration results; do not stat each child immediately.
- Emit batches, not one signal per entry.
- Close handles on all exits using RAII.
- Never call `QApplication::processEvents()` inside enumeration loops.

For shell-virtual, archive and network providers, return the same UI-facing record contract with provider-specific capabilities. The view should not branch on Win32 file handles.

### 6.3 Batching and UI backpressure

Proposed initial constants; tune in U0/U2 benchmarks:

```cpp
constexpr std::size_t kDirectoryBatchEntries = 512;
constexpr std::size_t kUiMailboxMaxBatches = 8;
constexpr int kUiDrainBudgetMs = 2;
constexpr int kUiDrainTimerMs = 8;
```

Policy:

- Worker accumulates up to 512 entries or 8 ms, whichever comes first, then submits one immutable batch.
- The UI mailbox is bounded. If full, the producer waits on a cancellable condition variable or pauses enumeration. Do not keep posting unbounded queued events.
- UI drains until its 2 ms budget is exhausted, then returns to the event loop. It may leave remaining batches queued.
- UI notifications are coalesced: at most one “mailbox has data” wake-up is pending.
- A newer navigation generation makes old batches no-ops and releases their memory.
- When navigation is cancelled, cancel enumeration and queued decoration tasks. A worker that cannot be stopped immediately must still discard results for the stale generation.

### 6.4 Sorting large directories

Do not insert each arriving item into a globally sorted vector. Repeated insertion can become O(N²).

Initial strategy:

1. Append enumeration records to the store.
2. Sort chunks (initial run size: 16,384 entries) off the GUI thread using the selected `SortSpec`.
3. Merge sorted runs into a final permutation using a k-way merge.
4. Publish the new permutation as an immutable generation.
5. UI swaps the order and restores selected/focused entries plus the scroll anchor by `EntryKey`.

During initial load, show a truthful loading state such as **“Loading · sorting”**. The first batch can be displayed promptly, but it must not be labelled globally sorted until the final merge completes. For very large directories, the UI may show a sorted first run and a clearly separated “still loading” tail; do not silently interleave unsorted rows with sorted results.

Sort rules:

- Use a deterministic, locale-aware natural-sort policy for user-visible names; define and test numeric segments, punctuation, case, Unicode and directory grouping.
- Use stable tie-breakers (provider identity / `EntryKey`) so equal visible values do not randomly reorder.
- Unknown size/date values remain in a defined “unknown” region until known; they must not be treated as zero.
- Sorting by size/date can request missing metadata asynchronously. Re-sort only in coalesced batches, never once per metadata reply.
- A sort change cancels the previous sort generation. Old sort results are dropped.
- Save scroll anchor by `EntryKey`, not raw pixel offset alone; restore the nearest valid anchor after the order changes.

### 6.5 Incremental file-system changes

Directory watcher events are hints, not a complete source of truth. Coalesce bursts; apply add/remove/rename updates by `EntryKey`; mark a directory dirty and re-enumerate after notification overflow or an unresolvable rename. Keep the open-folder snapshot independent of Spec 01's global index service. A service restart must not blank an already-open folder.

---

## 7. UI thread, workers and concurrency contracts

### 7.1 GUI-thread rule

Only the GUI thread may mutate QWidget/QObject UI state, invoke view/model mutation methods, create `QPixmap`, and apply batches to the Qt model. Workers may create immutable data, `QImage` objects where safe, decode data and perform I/O. All worker-to-UI communication is queued and bounded.

### 7.2 Worker classes

| Worker lane | Work | Concurrency cap (initial) | Priority |
|---|---|---:|---|
| Directory I/O | Enumerate current folder, provider status | 2 per app, 1 per directory | High for active tab |
| Metadata I/O | Lazy size/date/property lookups | 2 | Medium |
| Shell COM STA | Shell icon/thumbnail lookup and shell metadata operations requiring STA | 1 dedicated STA thread initially | Medium/low |
| Image decode | Decode image/thumbnail results not handled by Shell | 2 | Low, viewport-first |
| Sorting | Sort runs and merge permutations | 1–2 | Medium; cancel on newer sort |
| Search client | Async index/content IPC and result merge | Small bounded pool or event-driven pipe | High for active query |
| Background tabs | Low-priority refresh work | Shared bounded budget | Low |

These are initial caps, not universal optimal numbers. Calibrate on the minimum target machine. Do not set the thread count equal to all logical processors: GodXplr is an interactive app sharing CPU, storage and memory with the user's other work.

### 7.3 Bounded scheduling

Each request must declare:

```cpp
enum class WorkPriority : std::uint8_t {
    VisibleNow, NearViewport, ActiveTab, BackgroundTab, Speculative
};

struct UiWorkRequest {
    std::uint64_t requestId;
    std::uint64_t navigationGeneration;
    EntryKey entry;
    WorkPriority priority;
    std::uint32_t requestedPixelSize;
    std::uint32_t dpiBucket;
};
```

Rules:

- Visible-now work preempts speculative work.
- Repeated requests for the same key/size/generation are deduplicated.
- Queue capacity is finite. Excess speculative jobs are dropped, not accumulated.
- Scroll direction influences near-viewport prefetch but never delays visible items.
- When the user scrolls rapidly, cancel requests for items now far outside the viewport.
- Background tabs do not continuously extract thumbnails.
- A request result includes request ID and generation. Stale results are discarded.
- Cancellation must be cooperative for app-owned tasks and best-effort for shell handlers. A timeout/fault in a handler must not take down the main window.

### 7.4 Thread lifetime and shutdown

- The application owns worker pools through `UiWorkScheduler`.
- Closing a tab cancels tab-owned requests but does not destroy global cache workers.
- Closing the application stops accepting new work, cancels outstanding tasks, closes provider handles, drains or discards mailbox batches and then joins workers with bounded timeouts.
- Do not block the UI thread indefinitely while joining. If a third-party shell call does not return, isolate it in a helper process in a later phase rather than holding shutdown forever.
- Each thread interacting with COM calls `CoInitializeEx` with the correct apartment model and calls `CoUninitialize` on that same thread. The UI thread is an STA; background shell operations use a dedicated STA message loop because some Shell objects require apartment/message-pump semantics.

---

## 8. Icons, thumbnails and previews

### 8.1 Split icon and thumbnail work

File-type icons and content thumbnails are different requests.

- **Icon:** small symbolic representation (folder, file type, drive, shortcut, status overlay).
- **Thumbnail:** content-derived preview of an image, video frame, PDF page or supported document.

The renderer must paint a stable placeholder immediately. Icon or thumbnail arrival causes only the affected visible rectangle to be invalidated.

### 8.2 Resolution policy

Use this decision order:

1. GodXplr memory cache hit → return immediately.
2. Windows Shell cache-only lookup where supported → use if available.
3. Asynchronous Shell extraction on the dedicated STA lane.
4. GodXplr extractor fallback for supported types, in a sandbox/helper process if it parses untrusted content.
5. Generic file-type icon placeholder.

Do not reimplement thumbnail extraction for every Windows file type. Reuse the Windows Shell thumbnail system where possible. However, do not call `IShellItemImageFactory::GetImage` on the GUI thread: Microsoft's documentation explicitly warns that retrieving a thumbnail may access disk even when cached unless the memory-only flag is used.

### 8.3 Cache keys and invalidation

A thumbnail cache key must distinguish:

`EntryKey + file freshness (size/mtime/change stamp) + requested physical pixel size + DPI bucket + crop mode + extractor/version`

A filename-only key is invalid: a file can be overwritten without being renamed. For Shell thumbnails, the Shell's own cache identity may be used in addition to the app cache.

Invalidate on:

- file write/replace/rename where the identity or freshness stamp changes;
- provider generation change;
- volume removal/reconnect if identity cannot be verified;
- thumbnail-handler/extractor version change;
- display scale change when the requested pixel size changes.

### 8.4 Cache sizes

Initial per-process cache proposals on an 8 GB RAM baseline:

| Cache | Initial budget | Policy |
|---|---:|---|
| Small icons | 16 MB | Shared by file type/theme/DPI; LRU |
| Thumbnail QImage cache | 96 MB | LRU, visible-first, adaptive shrink under memory pressure |
| Text elision/layout | 8 MB | LRU keyed by name-generation, width, font and DPI |
| Geometry/layout | 4 MB | Current visible layout and a small number of modes |
| In-flight decoded images | 32 MB | Strictly bounded; reject oversized speculative results |

The cache budgets are tunable. Measure the actual RSS and working set. Do not retain a full folder's full-resolution images. Keep decoded results at the size needed for the view; do not load 4K thumbnails for a 48 px icon slot.

### 8.5 Image handling

- Workers return `QImage` or immutable pixel buffers; GUI thread creates/caches `QPixmap` only if the chosen paint path requires it.
- Apply decode dimension and byte limits before allocation; reject decompression bombs and implausible dimensions.
- Respect aspect ratio; use deterministic crop/contain rules by view mode.
- Use the right physical pixel dimensions: `logicalSize × devicePixelRatio`.
- Do not upscale a tiny source thumbnail and pretend it is high quality.
- Preview pane rendering is independent from row rendering. Opening a preview must not block scrolling.

### 8.6 Scroll prioritization

For each scroll event:

1. Compute the new visible entry range.
2. Cancel queued thumbnail requests outside visible range plus a small margin.
3. Queue visible icons first, then visible thumbnails.
4. Prefetch roughly one viewport in the current scroll direction only when worker capacity remains.
5. Do not prefetch the entire directory.

---

## 9. Painting algorithm

### 9.1 Paint pass

`FileListView::paintEvent()` must do only bounded CPU work:

```text
paintEvent(event):
    start frame timer
    painter.begin(viewport)
    set clip to event.region
    fill dirty background (opaque)

    range = layout.visibleRange(event.region, rowCount, scrollOffset)
    for row in range:
        rect = layout.itemRect(row, scrollOffset)
        if rect does not intersect dirty region: continue
        entry = store.entryAt(order[row])
        paint selection/hover/focus background
        paint cached icon or placeholder
        paint visible text columns using already available text data
        paint already-known metadata; show pending state for unknown metadata
        paint optional status/overlay badges

    paint focus/drop/selection overlay if visible
    painter.end
    record paint duration and rows painted
```

### 9.2 Per-row work rules

Allowed in the paint pass:

- O(1) row lookup from the current snapshot/permutation.
- Cheap selection and hover state checks.
- Simple rectangle, line, icon and text drawing.
- Cache lookups that do not block or lock for a long period.
- Reuse precomputed column geometry and font metrics.

Forbidden in the paint pass:

- `CreateFileW`, `FindFirstFileW`, `GetFileAttributesW`, `OpenFileById`, `SHGetFileInfo`, COM activation, Shell thumbnail extraction or any disk/network access.
- `QDir`, `QFileInfo` or `QFileSystemModel` calls that can query the filesystem.
- Regex, query parsing, large path building or full Unicode normalization.
- Sorting, grouping or scanning all entries.
- Creating per-row widgets or long-lived `QPixmap` objects.
- Waiting for worker results.

### 9.3 Dirty regions

- Use `QPaintEvent::region()` and paint only intersecting rows/cells.
- Use `viewport()->update(rect)` for a changed row/icon/thumbnail.
- Coalesce many updates into a single region; Qt can merge update requests.
- When the full visible range changes (scroll, resize, view-mode change), invalidate the viewport.
- Do not call `repaint()` for normal state updates.
- Use `Qt::WA_OpaquePaintEvent` only if the viewport paints every pixel in its dirty area with an opaque background. Test under resizing, scrolling and theme changes.

### 9.4 Text and formatting

- Use one configured font per role, not a new font object per row.
- Precompute column widths, text baseline and row geometry per view generation.
- Avoid `QFontMetrics::elidedText()` repeatedly for the same name/column width. Use a bounded LRU cache keyed by entry/name generation, column width, font key, direction and DPI bucket.
- Do not convert every name in the directory to `QString` on load. Convert only visible names or cache conversions for a short working set.
- Keep number/date formatting locale-aware but do it outside expensive sort/paint loops or cache its result.
- RTL and bidirectional file names must render correctly; do not manually reverse Unicode text.

### 9.5 Visual effects

The default design should use restrained effects:

- solid/near-solid surfaces;
- subtle separators;
- restrained hover/selection transitions;
- no live blur behind every list row;
- no per-row shadow;
- no full-window animation timer while idle;
- no perpetual particle/gradient animation in the file surface.

Animation is decoration and must never delay selection, keyboard navigation, search or file operations. Use short, cancellable transitions only for a small number of shell controls. Honour reduced-motion and high-contrast settings.

---

## 10. State updates and interaction responsiveness

### 10.1 Navigation generation

Every navigation, sort, filter, query and view-mode change gets a monotonically increasing generation/request ID. Every asynchronous response carries its origin generation. If it doesn't match the current target generation, discard it.

This prevents stale folder enumeration, stale thumbnails, old search results and an earlier sort result from overwriting the user's latest action.

### 10.2 Selection model

Support:

- single selection;
- Ctrl-click toggle;
- Shift range selection;
- Ctrl+A;
- keyboard navigation with arrows, Home/End, Page Up/Down;
- type-to-select/search behaviour without stealing input from the explicit search box;
- focus row separate from selected set;
- stable selection across refresh, sorting and view changes when the same `EntryKey` remains present.

For huge selections, use a compact range/bitmap representation where measurement justifies it. Do not create one QObject per selected item. The UI must display selected-count and aggregate-size state asynchronously; do not synchronously sum millions of file sizes on the GUI thread.

### 10.3 Drag and drop

- Compute drag threshold according to platform conventions.
- Prepare expensive data formats only when drag begins.
- For large selections, do not synchronously enumerate all metadata in `mouseMoveEvent()`.
- Use native Windows data-object formats for interoperable copy/move operations.
- Drop-target hit testing must use the same layout geometry as painting.
- Drop highlight is a small overlay invalidation, not a full data refresh.

### 10.4 Search interaction

Search box owns text input and debouncing. The search UI must:

- submit a new query generation when debounce expires;
- cancel the prior Spec 01/02 requests;
- display name results as soon as they arrive;
- accept content results later without resetting the list unnecessarily;
- avoid losing selection/focus when late content results are merged;
- display whether fuzzy/content results are still pending;
- avoid sorting the full result set on the GUI thread;
- use `EntryKey` identity for merged rows.

Search result rows and directory rows share the same renderer contract, but can expose different columns and badges.

---

## 11. Folder tree and navigation surface

### 11.1 Tree model

Use a lazy `QAbstractItemModel` with `canFetchMore()` / `fetchMore()` or an equivalent explicit provider contract. Fetch children only when the user expands a node. Do not enumerate every descendant of every visible root at startup.

Each node stores:

- stable provider identity;
- display name;
- parent key;
- child state: unknown/loading/loaded/empty/failed;
- capabilities (can browse, can write, supports rename, supports shell menu);
- volume/provider status;
- cached icon key.

### 11.2 Sidebar contents

Initial sidebar sections:

- Home / Quick Access;
- pinned folders;
- This PC / volumes;
- network locations (when enabled/available);
- optional libraries and virtual locations.

Keep the top-level list fast and predictable. Pinned locations are user preferences, not hardcoded filesystem paths.

### 11.3 Tree performance

- Fetch only direct children on expand.
- Coalesce filesystem watcher changes.
- Do not calculate recursive folder size during expansion.
- Cache child counts only if a provider can supply them cheaply; never enumerate descendants just to show an expander arrow.
- Collapse cancels speculative metadata/thumbnail work for hidden descendants.

---

## 12. DPI, fonts, themes and accessibility

### 12.1 Coordinate system

Qt uses device-independent pixels for most QWidget geometry and maps them to physical pixels using the window's device pixel ratio. Keep all layout constants in logical pixels. Cache raster assets by physical pixel size/DPI bucket. Do not multiply every QWidget size by DPI manually.

Qt 6 on Windows is documented as Per-Monitor DPI Aware V2 by default. Do not blindly call `SetProcessDpiAwarenessContext()` after Qt has initialized DPI awareness. Use an application manifest only when the chosen deployment requires it and verify that it does not conflict with Qt's default setup.

### 12.2 Required tests

- 100%, 125%, 150%, 175% and 200% scale factors;
- move the window between monitors with different scaling;
- change scaling while the app is open;
- mixed 1080p/4K monitors;
- remote desktop connect/disconnect;
- fractional scaling and non-integer device pixel ratios;
- verify icon cache invalidation and text elision after DPI changes.

The app must not store one global DPI value for the lifetime of the process. Use the device-pixel ratio of the window/screen currently rendering the view.

### 12.3 Theme

- Light, dark and follow-system theme.
- Respect Windows high-contrast/accessibility colour settings; do not encode semantic state only by colour.
- Store design tokens centrally: surface, elevated surface, text, secondary text, border, hover, selected, focus, warning, error, success.
- Do not style every row via complex per-item stylesheets. Custom painting reads theme tokens.
- Theme switch invalidates colour-dependent caches, not directory data or the search index.

### 12.4 Accessibility

- All interactive controls have accessible names, roles and keyboard focus order.
- File rows expose item name, type and state to accessibility APIs; selection and focus changes are announced appropriately.
- Never make icons the only source of meaning; status badges require accessible text.
- Support keyboard-only navigation and visible focus indication.
- Respect text scaling and system contrast settings.
- Reduced-motion preference disables nonessential transitions.
- Verify the custom `QAbstractItemView` with Windows Narrator and at least one automated accessibility inspection tool. Custom painting must not erase the accessibility semantics of the model/view.

---

## 13. Native Windows integration rules

### 13.1 COM apartment policy

- GUI thread initializes COM as STA before using shell UI/drag-drop objects; integrate with Qt's message loop.
- Shell worker thread that uses STA-only Shell objects has its own message pump and calls `CoInitializeEx(COINIT_APARTMENTTHREADED)`.
- General background workers that use only agile/free-threaded COM components may use MTA.
- Never pass raw apartment-bound COM interface pointers between threads without COM marshaling. Pass immutable request data and marshal interfaces correctly.

### 13.2 Icon and thumbnail APIs

Evaluate `IThumbnailCache` / `IShellItemImageFactory` for the native thumbnail path. All calls that can access disk or invoke a thumbnail handler run off the GUI thread. Use cache-only/memory-only flags when immediate UI behaviour is required. A missing thumbnail falls back to an icon; it does not stall the view.

### 13.3 Shell context menus

The native Windows context menu is owned by the shell-integration subsystem, not the painter. `FileListView` provides the selected `EntryKey` set and anchor point; a platform adapter resolves Shell items and invokes menu interfaces. The view remains responsive and paints selection while the menu request is prepared.

### 13.4 Properties, previews and shell extensions

Potentially slow/third-party Shell handlers must not run from the paint path or synchronous input handler. If the call can hang, isolate it in a helper process with watchdog/timeout and return a generic fallback. The window must remain interactive when a handler is unavailable.

---

## 14. Performance contracts and provisional targets

These targets are starting gates for a local SSD and minimum-class machine with 8 GB RAM. They must be recalibrated after the benchmark harness records the actual CPU, GPU, storage, OS build, Qt version, display resolution and scale factor.

### 14.1 Responsiveness targets

| Metric | Target | Exact measurement |
|---|---:|---|
| Empty shell first paint | ≤ 250 ms warm; ≤ 1.0 s cold | Process start to first non-empty frame, separately report process start overhead |
| Cached folder first content | ≤ 150 ms p95 | Navigate request to first real row painted |
| Uncached local SSD folder first content | ≤ 300 ms p95 | Same, first batch; not full enumeration completion |
| 100k-entry folder enumeration | Report; no fixed universal SLA | Start to last entry received; local test dataset |
| 100k-entry folder sorted state | ≤ 1.0 s target | Last entry received to globally sorted permutation ready; report separately |
| 100k-row viewport paint | p95 ≤ 6 ms; p99 ≤ 12 ms | Paint event with 30–60 visible rows, icons cached |
| Scroll frame time | p95 ≤ 16.7 ms; p99 ≤ 33.3 ms | Continuous 60 Hz scripted scroll, 100k entries |
| Input-to-visible feedback | p95 ≤ 50 ms | Key/mouse event timestamp to frame containing state change |
| UI-thread blocking segment | no intentional I/O; warning at > 8 ms | Instrumented app-owned slots and tasks |
| Idle CPU | < 0.5% average target | After settling, one idle tab, 60 s sample |
| Typical memory | < 250 MB UI-process target | Warm app, one 100k-row folder, details view, standard icon cache; excludes index/content services |
| Thumbnail cache | ≤ 96 MB target | Decoded image memory; configurable/adaptive |

Targets are not to be “made green” by weakening the scenario. Every benchmark report records dataset size, file-name length distribution, icon state, cache state, view mode, selection count and resolution.

### 14.2 Performance tiers

- **Tier A — everyday:** 1k–20k entries, details view, cached icons.
- **Tier B — large:** 100k entries, details view, active scroll, sort and selection.
- **Tier C — stress:** 1M entries or pathological long names/metadata; UI must remain responsive even if enumeration/sort takes longer.
- **Tier D — remote/slow:** network share, removable media and offline provider; show state promptly and never make the app appear hung.

### 14.3 Degradation policy

Under memory or CPU pressure:

1. Stop speculative thumbnail prefetch.
2. Reduce thumbnail concurrency.
3. Evict least-recently-used decoded images.
4. Defer nonvisible metadata.
5. Reduce decorative animation.
6. Keep current visible rows and essential navigation responsive.

Do not silently disable correctness features, skip file rows, or label partial enumeration as complete.

---

## 15. Instrumentation and diagnostics

### 15.1 Required telemetry events

Use a lightweight in-process metrics recorder. No network telemetry is required or permitted by this spec.

Record at least:

- `app_process_started`, `main_window_first_paint`, `main_window_interactive`;
- `navigation_requested`, `directory_first_batch`, `directory_last_batch`, `directory_sorted`;
- `paint_started`, `paint_finished`, rows painted, dirty-region area;
- `scroll_input`, `scroll_frame_presented` where measurable;
- `metadata_request/complete/cancel/fail`;
- `icon_request/cache_hit/cache_miss/complete/fail`;
- `thumbnail_request/cache_hit/cache_miss/complete/fail/cancel`;
- `sort_started/sort_cancelled/sort_finished`;
- `mailbox_depth`, `worker_queue_depth`, dropped speculative work;
- memory cache bytes, directory-store bytes and process working set snapshots.

Each event includes a monotonic timestamp and relevant request/generation id. Avoid logging full file paths or file contents by default; diagnostics should be privacy-conscious.

### 15.2 Debug overlay

A developer-only overlay toggle (for example Ctrl+Shift+F12) displays:

- FPS and frame-time p50/p95/p99 over rolling windows;
- current paint time and rows painted;
- viewport visible range;
- mailbox depth and worker queue lengths;
- icon/thumbnail cache hit ratios;
- directory count and current generation;
- current view mode, DPI and scale factor;
- UI-thread tasks exceeding the warning threshold.

The overlay is disabled in release builds unless explicitly enabled through developer settings. It must not materially alter the benchmark results; benchmark both with it disabled and during diagnostics with it enabled.

### 15.3 Windows profiling

Use Windows Performance Recorder / Windows Performance Analyzer and ETW for representative traces. Correlate application markers with CPU scheduling, disk I/O, DWM/GPU activity and thread blocking. A CPU flame graph alone is not enough: scrolling can be limited by memory bandwidth, text layout, image decode, GPU composition or queue contention.

---

## 16. Benchmark design

### 16.1 Reproducible datasets

Create a dataset generator under `tools/bench/ui/` that produces:

- 1k, 10k, 100k and 1M entries;
- realistic extension distribution (documents, code, images, archives, executables, media);
- directory and file mix;
- names with ASCII, non-ASCII, mixed case, digits, spaces, punctuation and long Unicode sequences;
- mixed known/unknown metadata;
- duplicate names in different directories;
- hidden/system/reparse-point test entries where supported;
- thumbnail test set with small, large, corrupted and unusual-aspect-ratio images.

Use deterministic seeds and write a manifest containing seed, file count, bytes, directory depth, file type distribution and checksums. Never benchmark only a directory full of `file000001.txt`; it does not model the product.

### 16.2 Scenarios

1. Cold app launch; warm app launch.
2. Navigate to empty, 1k, 10k, 100k and 1M directories.
3. First content and complete enumeration timing.
4. Initial sort by name; re-sort by size/date/type; reverse sort.
5. Fast wheel scroll top-to-bottom and back; touchpad-like small scroll events.
6. Select one row, Ctrl multi-select 1k items, range-select large spans, Ctrl+A.
7. Rename one file while visible; create/delete burst; watcher overflow recovery.
8. Switch details/list/grid/gallery rapidly.
9. Rapidly navigate A→B→C while A is still enumerating; verify stale generations are ignored.
10. Populate thumbnail cache; scroll with cold and warm caches; cancel on reverse scroll.
11. Open/close 10 tabs; switch repeatedly; inspect memory growth and reclamation.
12. Move window between monitors with different DPI scales.
13. Simulate slow disk/network provider and access denied/offline errors.
14. Stress with a shell thumbnail handler that times out or returns malformed data.

### 16.3 Report format

Every benchmark run writes JSON + a human-readable Markdown summary:

```json
{
  "app_commit": "<git-sha>",
  "qt_version": "<exact-version>",
  "os_build": "<windows-build>",
  "cpu": "<model>",
  "gpu": "<model-driver>",
  "ram_gb": 8,
  "storage": "<model-and-filesystem>",
  "display": "1920x1080@60Hz",
  "dpi_scale": 1.0,
  "dataset_manifest": "<path>",
  "scenario": "scroll-100k-details-warm-icons",
  "samples": 30,
  "p50_ms": 0.0,
  "p95_ms": 0.0,
  "p99_ms": 0.0,
  "max_ms": 0.0,
  "working_set_mb": 0.0,
  "private_bytes_mb": 0.0
}
```

Values above are placeholders. Never fill them with estimates and mark the run as a pass.

### 16.4 Comparison protocol

When comparing against Windows Explorer, File Pilot, Files or Directory Opus:

- Use the same physical machine, same volume, same dataset and display settings.
- Record each application's version/build.
- Separate first-run/cold cache and warm cache scenarios.
- Give every application the same view mode and comparable visible columns.
- Do not compare one product's cached thumbnails against another product's cold thumbnails.
- Repeat each scenario at least 30 times for latency distributions, where repeatability permits.
- Record crashes, missing items and visual correctness, not only speed.
- Do not disable antivirus or security controls for one application only.
- Publish raw runs and scripts with the project.

---

## 17. Test strategy

### 17.1 Unit tests

- Layout geometry: `itemRect`, `visibleRange`, `hitTest`, `selectionRects` for all four layouts.
- Scroll math at 0, end, page boundaries, large row counts and DPI transitions.
- Column geometry, hidden columns, resized columns and horizontal scroll.
- Text elision cache key invalidation.
- `EntryKey` equality/hash and stable row resolution.
- Sort comparator: natural numbers, punctuation, Unicode, case, directory grouping, unknown metadata.
- Selection restoration after sort, refresh, insert, delete and navigation.
- Generation/cancellation logic: stale response cannot update current tab.
- Cache LRU, byte accounting, duplicate request coalescing and invalidation.

### 17.2 Integration tests

- Open folders with 1, 1k, 100k and stress-level entries.
- Rename/move/delete while the folder is visible.
- Sort during enumeration; change sort twice before first sort ends.
- Navigate away while worker has a queued batch; verify old batch is discarded.
- Open several tabs; close one while its thumbnail requests are running.
- Disconnect removable drive during paint/metadata request.
- Test access denied, path removed, permission changes and unsupported provider.
- Test search results arriving while folder navigation changes.
- Verify no missing or duplicate rows after enumeration and watcher reconciliation.

### 17.3 Responsiveness tests

- Automated scroll at fixed rates over 100k rows.
- Mouse move across the viewport; hover must not cause full-model repaint.
- Shift selection over large ranges.
- Fast keyboard repeat on Up/Down/PageDown.
- Resize window continuously while enumeration is active.
- Rapid view-mode toggles with thumbnails in flight.
- Inspect p95/p99 frame time and event-loop delay, not average FPS only.

### 17.4 Visual regression tests

Capture stable screenshots for:

- light/dark/high-contrast themes;
- 100%, 125%, 150%, 175%, 200% DPI;
- details/list/grid/gallery;
- selected/hovered/focused/disabled rows;
- long file names, Unicode, RTL and mixed-script names;
- missing icon/thumbnail placeholders;
- empty/loading/error/offline states;
- split view and narrow window sizes.

Screenshot tests must mask intentionally dynamic values such as timestamps and progress animation. A visual pass cannot replace interaction/accessibility tests.

### 17.5 Thread-safety tests

- Assert every widget/model mutation is on the GUI thread in debug builds.
- Run ThreadSanitizer on Qt-free components where supported; use Windows Application Verifier / debug heap / stress runs for Windows-specific paths.
- Stress mailbox producer/consumer, cancel, tab close and app shutdown.
- Ensure no stale pointer/reference is held after a snapshot generation is replaced.

---

## 18. Source tree and dependency wiring

Recommended folders (adapt only through an architecture decision):

```text
src/
  app/
    main.cpp
    app_controller.*
  ui/
    app/
      main_window.*
      tab_workspace.*
    shell/
      tab_strip.*
      navigation_bar.*
      address_bar.*
      sidebar.*
      toolbar.*
      status_area.*
      command_palette.*
    model/
      file_list_model.*
      directory_store.*
      directory_snapshot.*
    view/
      file_list_view.*
      file_layout.h
      details_layout.*
      list_layout.*
      grid_layout.*
      gallery_layout.*
      tree_model.*
      row_painter.*
    state/
      tab_state.*
      navigation_coordinator.*
      view_coordinator.*
      selection_state.*
    cache/
      icon_cache.*
      thumbnail_cache.*
      text_layout_cache.*
    async/
      ui_work_scheduler.*
      ui_mailbox.*
      request_generation.*
    theme/
      theme_manager.*
      design_tokens.*
    accessibility/
      accessibility_adapter.*
  core/
    filesystem/
      directory_provider.h
      directory_enumerator.h
    operations/
    search/
  platform/
    windows/
      win32_directory_provider.*
      shell_icon_provider.*
      shell_thumbnail_provider.*
      com_apartment.*
      dpi_adapter.*
  ipc/
    index_client/
    content_client/
  tests/
    unit/ui/
    integration/ui/
    visual/
    performance/
  tools/
    bench/ui/
    gen_ui_dataset/
```

### 18.1 CMake components

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Test)

add_library(godxplr-ui
    # ui/model, ui/view, ui/state, ui/cache, ui/async, ui/theme
)

target_link_libraries(godxplr-ui
    PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets
    PRIVATE godxplr-core
)

target_compile_features(godxplr-ui PUBLIC cxx_std_20)

if (WIN32)
    target_sources(godxplr-ui PRIVATE
        platform/windows/win32_directory_provider.cpp
        platform/windows/shell_icon_provider.cpp
        platform/windows/shell_thumbnail_provider.cpp
        platform/windows/com_apartment.cpp
        platform/windows/dpi_adapter.cpp
    )
    target_link_libraries(godxplr-ui PRIVATE Shell32 Shlwapi Ole32 User32)
endif()

add_executable(godxplr-ui-tests
    tests/unit/ui/layout_tests.cpp
    tests/unit/ui/generation_tests.cpp
    tests/unit/ui/cache_tests.cpp
)
target_link_libraries(godxplr-ui-tests PRIVATE godxplr-ui Qt6::Test)
```

The exact Qt package version and deployment strategy must be pinned in the dependency manifest and CI. Do not use an unpinned “latest Qt” dependency in reproducible builds.

---

## 19. Phased implementation plan

**Rule:** Give Antigravity one phase at a time. It must not start the next phase until the current phase's tests and gate pass. Agents may create scaffolding for later phases only when the interface is already specified; they must not implement those phases early.

| Phase | Deliverable | Depends on | Mandatory gate |
|---|---|---|---|
| U0 | UI benchmark harness, deterministic dataset generator, app metrics recorder, CI skeleton | CMake baseline | Dataset manifest deterministic; benchmark JSON schema validated; CI build green |
| U1 | MainWindow shell, theme tokens, tab state and navigation state skeleton | U0 | App launches, first paint measured, no filesystem I/O in UI startup |
| U2 | DirectoryStore, FileListModel and custom details `FileListView` | U1 | 100k fake rows scroll/select/hit-test correctly; no per-row QWidget; geometry tests pass |
| U3 | Async directory provider, bounded mailbox, generations and cancellation | U2 | A→B→C rapid navigation cannot show stale rows; mailbox depth bounded; UI remains responsive |
| U4 | Background sort runs, stable identity, selection/scroll restoration, columns | U3 | Sort/refresh/mutate stress converges; 100k sorted result meets measured target or has a documented bottleneck |
| U5 | Sidebar tree + list/grid layout strategies + keyboard/mouse/drag-drop basics | U4 | Keyboard-only navigation and selection tests pass; all layout hit-tests match painted geometry |
| U6 | Icon cache + Shell icon/thumbnail worker + image decode queue | U5 | No shell/disk calls on UI thread; queue bounded; cancellation and timeout tests pass |
| U7 | Gallery/preview/inspector and thumbnail prioritization | U6 | Scrolling with cold thumbnails stays within frame gates; memory cap respected |
| U8 | Spec 01 search result adapter + Spec 02 content result adapter | Specs 01/02 API contracts | Late results merge without selection loss; new query cancels old query; no UI-thread IPC wait |
| U9 | DPI, theme, accessibility and Windows native integration hardening | U5–U8 | Multi-DPI, Narrator, high contrast, reduced motion and screenshot suite pass |
| U10 | Performance tuning and competitor comparison report | U0–U9 | Raw benchmark report for Tier A/B/C; all regressions explained; no unsupported speed claims |
| U11 | Release polish and diagnostics controls | U10 | Debug overlay disabled by default; telemetry privacy check; shutdown and memory soak pass |

### 19.1 Stop conditions

Stop and report instead of proceeding if:

- UI code performs blocking I/O or waits on workers;
- mailbox/thumbnail queues are unbounded;
- selection loses identity after sort or refresh;
- paint time grows with total directory count rather than visible rows;
- a third-party handler can freeze the main window;
- memory grows after repeated tab open/close and does not return near baseline;
- a view-mode switch corrupts selection/scroll state;
- the custom view lacks accessibility semantics;
- an agent claims an unmeasured target passed.

---

## 20. Antigravity agent operating instructions

### 20.1 Before changing code

The agent must:

1. Read `context.md`, `agents.md`, `gemini.md`, all applicable rules/workflows and `progress.md`.
2. Read Spec 01 and Spec 02 API contracts before integrating search, metadata or content results.
3. Inspect the existing repository. Do not overwrite existing working architecture without documenting the reason.
4. Identify the current phase and its acceptance gate.
5. List exact files to create/change and external dependencies before editing.
6. State assumptions separately from confirmed facts.

### 20.2 During implementation

- Work only on the current phase.
- Keep rendering independent of filesystem/index service implementations.
- Make UI mutations on the GUI thread only.
- Add tests with each implementation change.
- Do not hide compiler warnings or disable tests to obtain a green build.
- Do not add a third-party rendering, model or animation library without a dependency/license/performance comparison.
- Do not modify existing visual design outside the current task unless required for correctness; document any change.

### 20.3 Before marking a phase complete

Return:

1. Files created/changed.
2. Architecture and API changes.
3. Build command and actual output/exit code.
4. Tests run and actual result.
5. Benchmark command, machine details and raw measurements, if relevant.
6. Known defects and remaining assumptions.
7. Gate result: PASS / FAIL / BLOCKED, with evidence.
8. `progress.md` update.

“Implemented” without a build and test report is not completion.

---

## 21. Risks and mitigations

| Risk | Why it matters | Mitigation / trigger |
|---|---|---|
| Custom view is incomplete | Qt's standard view behaviour is extensive | Implement incrementally; selection/keyboard/accessibility tests are hard gates |
| First-load sort delays | User sees unsorted entries | Show truthful loading/sorting state; sort asynchronously; preserve stable identity |
| Unbounded queued batches | UI memory spike and input lag | Bounded mailbox + backpressure + cancellation |
| Shell handlers block or crash | Third-party extension runs in process | Dedicated worker at minimum; helper-process isolation for risky/hanging handlers |
| Thumbnail cache pressure | Large images can consume hundreds of MB | Decode to target size, bounded LRU, visible-first scheduling |
| Qt version coupling | Rendering APIs can change | Avoid `QtGuiPrivate` QRhi as a base dependency; pin Qt minor/patch |
| Over-customized window chrome | Breaks snapping, DPI and accessibility | Keep native frame in v1 |
| Animation damages scroll latency | Work runs during interaction | No perpetual animation; reduce motion and benchmark with effects enabled |
| DPI cache bugs | Blurry icons or incorrect geometry on monitor move | DPI-keyed cache, per-window DPR, multi-monitor tests |
| Directory watcher overflow | View becomes stale | Mark dirty and re-enumerate affected scope |
| Over-ambitious frame targets | Agents game benchmark or ship regressions | Publish raw percentiles; define workload; tune only after profiling |

---

## 22. Accepted decisions vs. decisions still requiring measurement

### 22.1 Accepted engineering baseline

- Qt 6 Widgets shell.
- Custom `QAbstractItemView` as main file surface.
- Custom `QAbstractTableModel` with stable `EntryKey` role.
- Compact directory store + UTF-16 name arena + separate row-order permutation.
- Visible-range painting only; no per-file widgets.
- No `QFileSystemModel` for the main directory view.
- All enumeration, sorting, metadata, icon and thumbnail work asynchronous.
- Bounded mailboxes and queues; generation-based cancellation.
- Shell COM work isolated from painting and normal input handling.
- High-DPI logical coordinate model; test multiple monitor scales.
- Benchmarks and performance instrumentation mandatory.

### 22.2 Must be measured before final tuning

- Whether the default Widgets painter meets the Tier B frame target on the minimum machine.
- Best enumeration batch size and UI drain budget.
- Best sort run size and merge strategy.
- Whether shell thumbnail extraction should be in-process STA or helper-process isolated for the chosen handlers.
- Actual working-set cost of row data, icon cache and thumbnail cache.
- Whether Qt Quick prototype materially improves the actual file-surface workload.
- Exact startup, paint, scroll and directory-load thresholds after U0 establishes a baseline.

### 22.3 Not approved as default architecture

- QQuickWidget for the main file view.
- QRhi / `QtGuiPrivate` dependency.
- QOpenGLWidget as a blanket acceleration layer.
- A custom Direct3D renderer before Widgets profiling.
- Per-row QWidget/indexWidget patterns.
- Synchronous filesystem or Shell calls from UI code.

---

## 23. Source notes and technical references

The following official documentation supports the framework/API statements in this specification. It does not prove GodXplr's performance; that requires the benchmark plan in §16.

1. Qt 6.12 — [QAbstractItemView](https://doc.qt.io/qt-6/qabstractitemview.html): custom item-view API, viewport painting note and delegate support.
2. Qt 6.12 — [QAbstractScrollArea](https://doc.qt.io/qt-6.12/qabstractscrollarea.html): viewport, scroll bars, `scrollContentsBy()` and viewport updates.
3. Qt 6.12 — [QAbstractItemModel](https://doc.qt.io/qt-6/qabstractitemmodel.html): model contract and incremental `fetchMore()` behaviour.
4. Qt 6.12 — [QWidget](https://doc.qt.io/qt-6/qwidget.html): double buffering, dirty-region painting, merged `update()` requests and opaque painting.
5. Qt 6.12 — [High DPI](https://doc.qt.io/qt-6/highdpi.html): device-independent coordinates, device-pixel ratio and Windows per-monitor DPI behaviour.
6. Qt 6.12 — [Qt OpenGL overview](https://doc.qt.io/qt-6.12/qtopengl-index.html): Widgets raster path versus Qt Quick hardware-accelerated scene graph.
7. Qt 6.12 — [QQuickWidget](https://doc.qt.io/qt-6/qquickwidget.html): extra render pass and threaded-render-loop limitation.
8. Qt 6.12 — [QRhi](https://doc.qt.io/qt-6/qrhi.html): private-module linkage and lack of source/binary compatibility guarantee.
9. Qt 6.12 — [QOpenGLWidget](https://doc.qt.io/qt-6.12/qopenglwidget.html): OpenGL framebuffer integration and composition considerations.
10. Qt 6.12 — [Threads and QObjects](https://doc.qt.io/qt-6/threads-qobject.html): queued connections and thread-affinity rules.
11. Qt 6.12 — [QThreadPool](https://doc.qt.io/qt-6/qthreadpool.html): pooled worker execution and thread-safe pool methods.
12. Qt 6.12 — [Qt Quick performance](https://doc.qt.io/qt-6/qtquick-performance.html): delegate simplicity, reuse and cache-buffer trade-offs.
13. Microsoft Learn — [High DPI Desktop Application Development](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development): PMv2 behaviour and mixed-monitor testing.
14. Microsoft Learn — [High DPI Reference](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-reference): DPI APIs and messages.
15. Microsoft Learn — [SetProcessDpiAwarenessContext](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setprocessdpiawarenesscontext): awareness setup and requirement to set it before DPI-dependent UI creation.
16. Microsoft Learn — [IShellItemImageFactory::GetImage](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ishellitemimagefactory-getimage): disk-access warning and cache/memory flags.
17. Microsoft Learn — [IThumbnailCache::GetThumbnail](https://learn.microsoft.com/en-us/windows/win32/api/thumbcache/nf-thumbcache-ithumbnailcache-getthumbnail): Shell thumbnail cache flags and extraction outcomes.
18. Microsoft Learn — [Processes, Threads, and Apartments](https://learn.microsoft.com/en-us/windows/win32/com/processes--threads--and-apartments): STA/MTA, message pump and COM apartment constraints.

---

## 24. Completion definition

Spec 03 is implemented only when:

- MainWindow and tab/navigation shell are functional.
- The details view renders 100k synthetic entries using visible-range painting.
- Grid/list/details modes use layout strategies without per-entry widgets.
- Sorting, selection, focus and scroll position survive refresh and async updates by stable identity.
- Enumeration and metadata are asynchronous and cancellation-safe.
- Mailbox and thumbnail queues are bounded and observable.
- No filesystem/Shell/thumbnail work occurs on the GUI thread.
- The app handles multiple DPI scales, keyboard navigation and accessibility checks.
- All required unit, integration, visual and stress tests pass.
- Tier A/B/C benchmark results are stored with machine metadata and raw JSON.
- Any target not achieved is documented with a trace, cause and next experiment—not hidden or silently relaxed.

**End of Spec 03.**
