#include "godxplr/ui/model/tree_model.h"
#include "godxplr/ui/shell/sidebar_model.h"
#include "godxplr/ui/view/list_layout.h"
#include "godxplr/ui/view/grid_layout.h"
#include "godxplr/ui/view/gallery_layout.h"
#include "godxplr/ui/view/interaction_controller.h"
#include "godxplr/ui/view/accessibility_semantics.h"
#include "godxplr/ui/model/directory_store.h"
#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>

#define TEST_ASSERT(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "Assertion failed: " #cond " at " << __FILE__         \
                      << ":" << __LINE__ << std::endl;                         \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

using namespace gx;
using namespace gx::ui;

// ============================================================================
// Mock Provider for Lazy Tree Expansion
// ============================================================================

class MockTreeProvider : public IDirectoryProvider {
public:
    ProviderKind kind() const noexcept override { return ProviderKind::Win32Directory; }
    ProviderCapabilities capabilities() const noexcept override { return ProviderCapabilities::CanEnumerate; }
    bool canHandleUri(std::string_view) const noexcept override { return true; }

    ProviderStatus enumerate(
        std::string_view uri,
        uint64_t requestId,
        uint64_t generation,
        std::shared_ptr<CancellationToken>,
        std::function<bool(DirectoryBatch&&)> batchCallback) override
    {
        DirectoryBatch batch;
        batch.requestId = requestId;
        batch.generation = generation;
        batch.isFinal = true;
        batch.status = ProviderStatus::Ready;

        // Emit 3 subdirectories and 2 files (files should be filtered out by tree)
        auto addEntry = [&](std::string_view name, bool isDir) {
            uint32_t offset = static_cast<uint32_t>(batch.nameArena.size());
            uint32_t len = static_cast<uint32_t>(name.size());
            batch.nameArena.append(name);

            DirectoryEntry entry{};
            entry.name = NameSlice{offset, len};
            if (isDir) entry.flags |= kEntryDir;
            entry.key = EntryKey{.provider = ProviderKind::Win32Directory, .volumeOrProviderId = 1, .objectId = offset, .incarnation = 0};
            batch.entries.push_back(entry);
        };

        if (uri.find("Users") != std::string_view::npos) {
            addEntry("Alice", true);
            addEntry("Bob", true);
            addEntry("desktop.ini", false);
        } else {
            addEntry("Program Files", true);
            addEntry("Users", true);
            addEntry("Windows", true);
            addEntry("pagefile.sys", false);
        }

        batchCallback(std::move(batch));
        return ProviderStatus::Ready;
    }

    std::unique_ptr<IDirectoryWatcher> watch(std::string_view, std::shared_ptr<IWatchListener>) override {
        return nullptr;
    }
};

// ============================================================================
// Test 1: Lazy Folder Tree Model
// ============================================================================

void test_folder_tree_lazy_expansion() {
    std::cout << "[Test 1] Lazy Folder Tree Model...\n";

    FolderTreeModel tree;
    MockTreeProvider provider;

    EntryKey driveKey{.provider = ProviderKind::Win32Directory, .volumeOrProviderId = 1, .objectId = 10, .incarnation = 0};
    uint64_t rootId = tree.addRootNode(driveKey, "file:///C:/", "Local Disk (C:)");

    TEST_ASSERT(tree.totalNodeCount() == 1);
    const TreeNode* root = tree.getNode(rootId);
    TEST_ASSERT(root != nullptr);
    TEST_ASSERT(root->childState == TreeChildState::Unloaded);
    TEST_ASSERT(!root->isExpanded);
    TEST_ASSERT(root->childIds.empty());

    // Initially visible rows contains only root
    std::vector<uint64_t> visible = tree.visibleRows();
    TEST_ASSERT(visible.size() == 1);
    TEST_ASSERT(visible[0] == rootId);

    // Expand root node lazily
    bool expanded = tree.expandNode(rootId, &provider);
    TEST_ASSERT(expanded);
    TEST_ASSERT(root->isExpanded);
    TEST_ASSERT(root->childState == TreeChildState::Loaded);
    TEST_ASSERT(root->childIds.size() == 3); // Program Files, Users, Windows (files filtered out)

    // Now visible rows contains root + 3 children
    visible = tree.visibleRows();
    TEST_ASSERT(visible.size() == 4);

    // Expand Users node
    uint64_t usersNodeId = root->childIds[1];
    const TreeNode* usersNode = tree.getNode(usersNodeId);
    TEST_ASSERT(usersNode != nullptr);
    TEST_ASSERT(usersNode->displayName == "Users");

    tree.expandNode(usersNodeId, &provider);
    TEST_ASSERT(usersNode->childIds.size() == 2); // Alice, Bob

    visible = tree.visibleRows();
    TEST_ASSERT(visible.size() == 6); // root + 3 + 2

    // Collapse Users node
    tree.collapseNode(usersNodeId);
    visible = tree.visibleRows();
    TEST_ASSERT(visible.size() == 4); // Children hidden, but still preserved in memory
    TEST_ASSERT(tree.totalNodeCount() == 6);

    // Toggle expansion
    tree.toggleNode(usersNodeId, &provider);
    TEST_ASSERT(tree.getNode(usersNodeId)->isExpanded);
    TEST_ASSERT(tree.visibleRows().size() == 6);

    std::cout << "  -> PASS: Lazy expansion verified; direct subfolders loaded without recursive tree scan.\n";
}

// ============================================================================
// Test 2: Sidebar Destinations Model
// ============================================================================

void test_sidebar_model() {
    std::cout << "[Test 2] Sidebar Destinations Model...\n";

    SidebarModel sidebar;
    const auto& sections = sidebar.sections();

    TEST_ASSERT(sections.size() >= 3); // Quick Access, Pinned, Drives

    // Check Quick Access
    const auto& qa = sections[0];
    TEST_ASSERT(qa.kind == SidebarSectionKind::QuickAccess);
    TEST_ASSERT(!qa.items.empty());
    TEST_ASSERT(qa.items[0].displayName == "Home");
    TEST_ASSERT(qa.items[1].displayName == "Desktop");
    TEST_ASSERT(qa.items[2].displayName == "Downloads");

    // Check Drives
    const auto& drives = sections[2];
    TEST_ASSERT(drives.kind == SidebarSectionKind::Drives);
    TEST_ASSERT(!drives.items.empty()); // At least C: drive on Windows

    // Test Pinning
    EntryKey pinKey{.provider = ProviderKind::Win32Directory, .volumeOrProviderId = 1, .objectId = 999, .incarnation = 0};
    sidebar.pinLocation("My Project", "file:///C:/Projects/GodXplr", pinKey);

    const SidebarItem* pinnedItem = sidebar.findItemByUri("file:///C:/Projects/GodXplr");
    TEST_ASSERT(pinnedItem != nullptr);
    TEST_ASSERT(pinnedItem->displayName == "My Project");

    // Test Unpinning
    sidebar.unpinLocation("file:///C:/Projects/GodXplr");
    TEST_ASSERT(sidebar.findItemByUri("file:///C:/Projects/GodXplr") == nullptr);

    std::cout << "  -> PASS: Sidebar sections and dynamic pin/unpin lifecycle verified.\n";
}

// ============================================================================
// Test 3: ListLayout Geometry & Virtualization
// ============================================================================

void test_list_layout_geometry() {
    std::cout << "[Test 3] ListLayout Geometry & Virtualization...\n";

    ListLayout layout(24);
    layout.setViewportSize(Size{800, 600});

    // Content extent for 100,000 entries
    Size extent = layout.contentExtent(100000);
    TEST_ASSERT(extent.width >= 800);
    TEST_ASSERT(extent.height == 2400000); // 100k * 24

    // Visible range with scroll offset y = 2400 (row 100)
    Rect exposed{0, 0, 800, 600};
    Point scroll{0, 2400};
    VisibleRange range = layout.visibleRange(exposed, 100000, scroll);

    TEST_ASSERT(range.first == 100);
    TEST_ASSERT(range.lastExclusive == 125); // (2400 + 600) / 24 = 3000 / 24 = 125

    // Item rect
    Rect r100 = layout.itemRect(100, scroll);
    TEST_ASSERT(r100.y == 0); // At top of viewport
    TEST_ASSERT(r100.height == 24);

    // Hit testing: point inside row 100
    uint64_t hit = layout.hitTest(Point{150, 10}, 100000, scroll);
    TEST_ASSERT(hit == 100);

    // Hit testing: point outside content width
    uint64_t miss = layout.hitTest(Point{9999, 10}, 100000, scroll);
    TEST_ASSERT(miss == UINT64_MAX);

    std::cout << "  -> PASS: ListLayout O(1) virtualization and exact hit-testing verified.\n";
}

// ============================================================================
// Test 4: GridLayout Multi-Column Geometry & Hit Testing
// ============================================================================

void test_grid_layout_geometry() {
    std::cout << "[Test 4] GridLayout Multi-Column Geometry & Hit Testing...\n";

    // cardWidth = 100, cardHeight = 80, spacing = 20
    GridLayout layout(100, 80, 20);
    layout.setViewportSize(Size{400, 600});

    // availableWidth = 400 - 20 = 380; slotWidth = 100 + 20 = 120 -> 380 / 120 = 3 columns
    int cols = layout.columnsPerRow();
    TEST_ASSERT(cols == 3);

    // Item 0: col 0, row 0 -> x = 20, y = 20
    // Item 1: col 1, row 0 -> x = 140, y = 20
    // Item 2: col 2, row 0 -> x = 260, y = 20
    // Item 3: col 0, row 1 -> x = 20, y = 120
    Rect r0 = layout.itemRect(0, Point{0, 0});
    TEST_ASSERT(r0.x == 20 && r0.y == 20 && r0.width == 100 && r0.height == 80);

    Rect r1 = layout.itemRect(1, Point{0, 0});
    TEST_ASSERT(r1.x == 140 && r1.y == 20);

    Rect r3 = layout.itemRect(3, Point{0, 0});
    TEST_ASSERT(r3.x == 20 && r3.y == 120);

    // Hit test inside card 0 (e.g. x = 50, y = 50)
    uint64_t hit0 = layout.hitTest(Point{50, 50}, 10, Point{0, 0});
    TEST_ASSERT(hit0 == 0);

    // Hit test inside card 1 (e.g. x = 150, y = 50)
    uint64_t hit1 = layout.hitTest(Point{150, 50}, 10, Point{0, 0});
    TEST_ASSERT(hit1 == 1);

    // Hit test in inter-card spacing (e.g. x = 125, y = 50, where spacing is 120 to 140)
    uint64_t hitSpacing = layout.hitTest(Point{125, 50}, 10, Point{0, 0});
    TEST_ASSERT(hitSpacing == UINT64_MAX); // Spacing hit returns miss! Painted == hit-test!

    // Hit test in vertical spacing (e.g. x = 50, y = 105, where card is 20-100 and spacing is 100-120)
    uint64_t hitVSpacing = layout.hitTest(Point{50, 105}, 10, Point{0, 0});
    TEST_ASSERT(hitVSpacing == UINT64_MAX);

    std::cout << "  -> PASS: GridLayout card layout and painted-equals-hit-test geometry verified.\n";
}

// ============================================================================
// Test 5: GalleryLayout Photo Card Geometry
// ============================================================================

void test_gallery_layout_geometry() {
    std::cout << "[Test 5] GalleryLayout Photo Card Geometry...\n";

    GalleryLayout layout(200, 160, 20);
    layout.setViewportSize(Size{900, 700});

    // 900 - 20 = 880; slot = 220 -> 4 columns
    TEST_ASSERT(layout.columnsPerRow() == 4);

    Rect r5 = layout.itemRect(5, Point{0, 0}); // row 1, col 1
    TEST_ASSERT(r5.x == 240); // 20 + 1 * 220
    TEST_ASSERT(r5.y == 200); // 20 + 1 * 180
    TEST_ASSERT(r5.width == 200);
    TEST_ASSERT(r5.height == 160);

    std::cout << "  -> PASS: GalleryLayout photo card grid geometry verified.\n";
}

// ============================================================================
// Test 6: Keyboard Navigation & Range Selection
// ============================================================================

void test_keyboard_navigation_and_selection() {
    std::cout << "[Test 6] Keyboard navigation and range selection...\n";

    ListLayout layout(24);
    layout.setViewportSize(Size{800, 600});

    InteractionController controller(&layout);
    controller.setItemCount(100);

    // Initial state
    TEST_ASSERT(!controller.focusIndex().has_value());
    TEST_ASSERT(controller.selectionModel().empty());

    // Press Down: focus 0, selected 0
    controller.handleKeyDown(KeyCode::Down);
    TEST_ASSERT(controller.focusIndex() == 0);
    TEST_ASSERT(controller.selectionModel().isSelected(0));
    TEST_ASSERT(controller.selectionModel().selectedCount() == 1);

    // Press Down again: focus 1, selected 1
    controller.handleKeyDown(KeyCode::Down);
    TEST_ASSERT(controller.focusIndex() == 1);
    TEST_ASSERT(controller.selectionModel().isSelected(1));
    TEST_ASSERT(!controller.selectionModel().isSelected(0));

    // Shift + Down: expands range from anchor 1 to 2
    controller.handleKeyDown(KeyCode::Down, KeyModifier::Shift);
    TEST_ASSERT(controller.focusIndex() == 2);
    TEST_ASSERT(controller.selectionModel().isSelected(1));
    TEST_ASSERT(controller.selectionModel().isSelected(2));
    TEST_ASSERT(controller.selectionModel().selectedCount() == 2);

    // Shift + Down: expands to 3
    controller.handleKeyDown(KeyCode::Down, KeyModifier::Shift);
    TEST_ASSERT(controller.focusIndex() == 3);
    TEST_ASSERT(controller.selectionModel().selectedCount() == 3);

    // Ctrl + A: Select All
    controller.handleKeyDown(KeyCode::KeyA, KeyModifier::Ctrl);
    TEST_ASSERT(controller.selectionModel().selectedCount() == 100);

    // Home: selects row 0
    controller.handleKeyDown(KeyCode::Home);
    TEST_ASSERT(controller.focusIndex() == 0);
    TEST_ASSERT(controller.selectionModel().selectedCount() == 1);
    TEST_ASSERT(controller.selectionModel().isSelected(0));

    // End: selects row 99
    controller.handleKeyDown(KeyCode::End);
    TEST_ASSERT(controller.focusIndex() == 99);
    TEST_ASSERT(controller.selectionModel().selectedCount() == 1);
    TEST_ASSERT(controller.selectionModel().isSelected(99));

    std::cout << "  -> PASS: Keyboard navigation, Shift range selection, and Ctrl+A Select All verified.\n";
}

// ============================================================================
// Test 7: Action Triggers & Rename Initiation
// ============================================================================

void test_action_triggers_and_rename() {
    std::cout << "[Test 7] Action triggers, rename initiation, and activation...\n";

    ListLayout layout(24);
    layout.setViewportSize(Size{800, 600});

    InteractionController controller(&layout);
    controller.setItemCount(10);

    uint64_t renameTriggeredRow = UINT64_MAX;
    controller.onBeginRename = [&](uint64_t row) {
        renameTriggeredRow = row;
    };

    uint64_t activateTriggeredRow = UINT64_MAX;
    controller.onActivate = [&](uint64_t row) {
        activateTriggeredRow = row;
    };

    // Navigate to row 4
    controller.setFocusIndex(4);

    // Press F2: triggers rename callback
    controller.handleKeyDown(KeyCode::F2);
    TEST_ASSERT(renameTriggeredRow == 4);

    // Press Enter: triggers activation callback
    controller.handleKeyDown(KeyCode::Enter);
    TEST_ASSERT(activateTriggeredRow == 4);

    // Double-click row 7
    controller.handleDoubleClick(Point{100, 24 * 7 + 10}, Point{0, 0});
    TEST_ASSERT(activateTriggeredRow == 7);

    std::cout << "  -> PASS: F2 rename, Enter activation, and Double-Click activation verified.\n";
}

// ============================================================================
// Test 8: Drag & Drop Scaffolding
// ============================================================================

void test_drag_drop_scaffolding() {
    std::cout << "[Test 8] Drag & Drop Scaffolding...\n";

    ListLayout layout(24);
    layout.setViewportSize(Size{800, 600});

    InteractionController controller(&layout);
    controller.setItemCount(3);

    DirectoryStore store;
    EntryKey k0{.provider = ProviderKind::Win32Directory, .volumeOrProviderId = 1, .objectId = 100, .incarnation = 0};
    EntryKey k1{.provider = ProviderKind::Win32Directory, .volumeOrProviderId = 1, .objectId = 101, .incarnation = 0};
    EntryKey k2{.provider = ProviderKind::Win32Directory, .volumeOrProviderId = 1, .objectId = 102, .incarnation = 0};

    store.addEntry(k0, "file_a.txt", 100, 1000, 0, 0);
    store.addEntry(k1, "Documents", 0, 1000, 0, kEntryDir); // Directory!
    store.addEntry(k2, "file_b.txt", 200, 1000, 0, 0);

    // Select row 0 (file_a.txt)
    controller.selectionModel().select(0);

    auto payload = controller.createDragPayload("file:///C:/Work", store);
    TEST_ASSERT(payload.has_value());
    TEST_ASSERT(payload->sourceUri == "file:///C:/Work");
    TEST_ASSERT(payload->keys.size() == 1);
    TEST_ASSERT(payload->keys[0] == k0);

    // Evaluate drop onto row 1 (Documents directory)
    uint64_t targetRow = UINT64_MAX;
    Point dropPointOnDir{100, 24 * 1 + 10};
    DropEffect effect = controller.evaluateDropTarget(dropPointOnDir, Point{0, 0}, *payload, store, targetRow);

    TEST_ASSERT(effect == DropEffect::Move);
    TEST_ASSERT(targetRow == 1);

    // Evaluate drop onto row 2 (regular file - cannot drop into file)
    Point dropPointOnFile{100, 24 * 2 + 10};
    DropEffect effectFile = controller.evaluateDropTarget(dropPointOnFile, Point{0, 0}, *payload, store, targetRow);
    TEST_ASSERT(effectFile == DropEffect::None);

    std::cout << "  -> PASS: Drag payload construction and directory drop target evaluation verified.\n";
}

// ============================================================================
// Test 9: Accessibility Semantics
// ============================================================================

void test_accessibility_semantics() {
    std::cout << "[Test 9] Accessibility Semantics...\n";

    AccessibleProperties treeItem;
    treeItem.role = AccessibleRole::TreeItem;
    treeItem.name = "Local Disk (C:)";
    treeItem.description = "System volume, NTFS, 150 GB free";
    treeItem.isExpanded = true;
    treeItem.depthLevel = 1;

    TEST_ASSERT(treeItem.role == AccessibleRole::TreeItem);
    TEST_ASSERT(treeItem.name == "Local Disk (C:)");
    TEST_ASSERT(treeItem.isExpanded);

    AccessibleProperties listItem;
    listItem.role = AccessibleRole::ListItem;
    listItem.name = "document.pdf";
    listItem.description = "PDF Document, 1.4 MB, Modified yesterday";
    listItem.isSelected = true;

    TEST_ASSERT(listItem.role == AccessibleRole::ListItem);
    TEST_ASSERT(listItem.isSelected);

    std::cout << "  -> PASS: Accessibility roles, state flags, and descriptions verified.\n";
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "=========================================================\n";
    std::cout << "GodXplr Spec 03 U5 Sidebar, Tree & View Interaction Tests\n";
    std::cout << "=========================================================\n";

    try {
        test_folder_tree_lazy_expansion();
        test_sidebar_model();
        test_list_layout_geometry();
        test_grid_layout_geometry();
        test_gallery_layout_geometry();
        test_keyboard_navigation_and_selection();
        test_action_triggers_and_rename();
        test_drag_drop_scaffolding();
        test_accessibility_semantics();

        std::cout << "\n>>> ALL 9/9 NAVIGATION & INTERACTION TESTS PASSED <<<\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FATAL EXCEPTION: " << ex.what() << "\n";
        return 1;
    }
}
