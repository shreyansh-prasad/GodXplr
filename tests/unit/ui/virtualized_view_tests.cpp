#include "godxplr/ui/model/directory_store.h"
#include "godxplr/ui/model/selection_model.h"
#include "godxplr/ui/view/details_layout.h"
#include <iostream>
#include <cstdlib>
#include <chrono>

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

void testDirectoryStorePopulateAndIdentity() {
    std::cout << "[Test] DirectoryStorePopulateAndIdentity...\n";
    DirectoryStore store;
    store.reserve(100);

    EntryKey k1{ProviderKind::NtfsVolume, 0x1234, 101, 1};
    EntryKey k2{ProviderKind::NtfsVolume, 0x1234, 102, 1};

    store.addEntry(k1, "document.txt", 1024, 100000, 0, 0);
    store.addEntry(k2, "Photos", 0, 200000, 0, kEntryDir);

    TEST_ASSERT(store.size() == 2);
    TEST_ASSERT(store.getName(0) == "document.txt");
    TEST_ASSERT(store.getName(1) == "Photos");
    TEST_ASSERT(store.getEntry(1).isDirectory());

    // Identity lookups
    TEST_ASSERT(store.findIndexByKey(k1) == 0);
    TEST_ASSERT(store.findVisibleRowByKey(k2) == 1);

    std::cout << "  -> Passed!\n";
}

void testDirectoryStore100kEntries() {
    std::cout << "[Test] DirectoryStore100kEntries (Memory & High-Capacity Gate)...\n";
    DirectoryStore store;
    constexpr size_t kCount = 100000;
    store.reserve(kCount, kCount * 28);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < kCount; ++i) {
        EntryKey k{ProviderKind::NtfsVolume, 0x1234, 1000 + i, 1};
        std::string name = "file_" + std::to_string(i) + ".dat";
        uint32_t flags = (i % 10 == 0) ? kEntryDir : 0;
        store.addEntry(k, name, i * 1024, 1330000000ULL + i, 0, flags);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << "  Loaded 100,000 entries into DirectoryStore in " << ms << " ms\n";

    TEST_ASSERT(store.size() == kCount);

    size_t totalMem = store.totalMemoryBytes();
    double bytesPerRow = static_cast<double>(totalMem) / kCount;
    std::cout << "  Total memory: " << (totalMem / 1024 / 1024) << " MB ("
              << bytesPerRow << " bytes/row)\n";

    // Strict Spec 03 budget check: <= 160 bytes/entry
    TEST_ASSERT(bytesPerRow <= 160.0);

    // O(1) random access test
    auto tAccess0 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < 10000; ++i) {
        size_t row = (i * 997) % kCount;
        auto name = store.getName(row);
        TEST_ASSERT(!name.empty());
    }
    auto tAccess1 = std::chrono::high_resolution_clock::now();
    double nsPerAccess = std::chrono::duration<double, std::nano>(tAccess1 - tAccess0).count() / 10000.0;
    std::cout << "  Average random row access latency: " << nsPerAccess << " ns\n";
    TEST_ASSERT(nsPerAccess < 100.0);

    std::cout << "  -> Passed!\n";
}

void testNaturalSorting() {
    std::cout << "[Test] NaturalSorting...\n";
    DirectoryStore store;
    EntryKey k1{ProviderKind::NtfsVolume, 1, 1, 1};
    EntryKey k2{ProviderKind::NtfsVolume, 1, 2, 1};
    EntryKey k3{ProviderKind::NtfsVolume, 1, 3, 1};
    EntryKey kDir{ProviderKind::NtfsVolume, 1, 4, 1};

    store.addEntry(k1, "file10.txt", 100, 10, 0, 0);
    store.addEntry(k2, "file2.txt", 200, 20, 0, 0);
    store.addEntry(k3, "file1.txt", 300, 30, 0, 0);
    store.addEntry(kDir, "ArchiveFolder", 0, 40, 0, kEntryDir);

    SortSpec spec;
    spec.column = SortColumn::Name;
    spec.order = SortOrder::Ascending;
    spec.directoriesFirst = true;
    spec.naturalNumeric = true;

    store.sort(spec);

    // Directory first
    TEST_ASSERT(store.getName(0) == "ArchiveFolder");
    // Natural numeric ordering: file1 < file2 < file10
    TEST_ASSERT(store.getName(1) == "file1.txt");
    TEST_ASSERT(store.getName(2) == "file2.txt");
    TEST_ASSERT(store.getName(3) == "file10.txt");

    // Stable identity mapping preserves visible row
    TEST_ASSERT(store.findVisibleRowByKey(kDir) == 0);
    TEST_ASSERT(store.findVisibleRowByKey(k3) == 1);

    std::cout << "  -> Passed!\n";
}

void testDetailsLayoutGeometry() {
    std::cout << "[Test] DetailsLayoutGeometry (O(1) Visible Range Gate)...\n";
    DetailsLayout layout(28); // 28 px uniform row height
    layout.setViewportSize(Size{800, 600});

    constexpr uint64_t kRowCount = 100000;
    Size extent = layout.contentExtent(kRowCount);
    TEST_ASSERT(extent.height == 100000 * 28);
    TEST_ASSERT(extent.width >= 600);

    // 1. Visible range at top (scrollOffset = 0)
    Rect exposed{0, 0, 800, 600};
    VisibleRange vrTop = layout.visibleRange(exposed, kRowCount, Point{0, 0});
    TEST_ASSERT(vrTop.first == 0);
    TEST_ASSERT(vrTop.lastExclusive == 22); // 600 / 28 = 21.4 -> 22 rows
    TEST_ASSERT(vrTop.count() == 22);

    // 2. Visible range scrolled down to row 5000 (y = 5000 * 28 = 140000)
    Point scrollPos{0, 140000};
    auto t0 = std::chrono::high_resolution_clock::now();
    VisibleRange vrMid = layout.visibleRange(exposed, kRowCount, scrollPos);
    auto t1 = std::chrono::high_resolution_clock::now();
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    std::cout << "  Visible range computed in: " << ns << " ns\n";

    TEST_ASSERT(vrMid.first == 5000);
    TEST_ASSERT(vrMid.lastExclusive == 5022);
    TEST_ASSERT(vrMid.count() == 22);

    // 3. Item rect calculation
    Rect r5000 = layout.itemRect(5000, scrollPos);
    TEST_ASSERT(r5000.y == 0);
    TEST_ASSERT(r5000.height == 28);

    Rect r5001 = layout.itemRect(5001, scrollPos);
    TEST_ASSERT(r5001.y == 28);

    // 4. Hit test
    // Click at viewport y = 28 -> should hit row 5001
    uint64_t hitRow = layout.hitTest(Point{100, 28}, kRowCount, scrollPos);
    TEST_ASSERT(hitRow == 5001);

    std::cout << "  -> Passed!\n";
}

void testVirtualSelectionModel() {
    std::cout << "[Test] VirtualSelectionModel (Bounded Allocation Gate)...\n";
    VirtualSelectionModel sel;

    // Single select
    sel.select(42);
    TEST_ASSERT(sel.selectedCount() == 1);
    TEST_ASSERT(sel.isSelected(42));
    TEST_ASSERT(!sel.isSelected(41));

    // Range select [10, 20]
    sel.selectRange(10, 20, false);
    TEST_ASSERT(sel.selectedCount() == 11);
    TEST_ASSERT(sel.isSelected(10));
    TEST_ASSERT(sel.isSelected(15));
    TEST_ASSERT(sel.isSelected(20));
    TEST_ASSERT(!sel.isSelected(9));
    TEST_ASSERT(!sel.isSelected(21));

    // Toggle row 15 (punch hole in range)
    sel.toggle(15);
    TEST_ASSERT(sel.selectedCount() == 10);
    TEST_ASSERT(!sel.isSelected(15));
    TEST_ASSERT(sel.isSelected(14));
    TEST_ASSERT(sel.isSelected(16));
    TEST_ASSERT(sel.getRanges().size() == 2); // [10, 15) and [16, 21)

    // Select All 100,000 rows
    sel.selectAll(100000);
    TEST_ASSERT(sel.selectedCount() == 100000);
    TEST_ASSERT(sel.isSelected(0));
    TEST_ASSERT(sel.isSelected(99999));
    // Must occupy exactly 1 range struct, not 100k objects
    TEST_ASSERT(sel.getRanges().size() == 1);

    std::cout << "  -> Passed!\n";
}

void testSelectionCoalescingRects() {
    std::cout << "[Test] SelectionCoalescingRects (Draw Call Minimization)...\n";
    DetailsLayout layout(28);

    std::vector<uint64_t> contiguousRows;
    for (uint64_t r = 10; r <= 29; ++r) {
        contiguousRows.push_back(r);
    }

    // 20 contiguous rows must coalesce into exactly 1 draw rectangle!
    auto rects = layout.selectionRects(contiguousRows, Point{0, 0});
    TEST_ASSERT(rects.size() == 1);
    TEST_ASSERT(rects[0].y == 10 * 28);
    TEST_ASSERT(rects[0].height == 20 * 28);

    std::cout << "  -> Passed!\n";
}

int main() {
    std::cout << "==========================================\n";
    std::cout << "Running GodXplr Virtualized View Tests    \n";
    std::cout << "==========================================\n";

    testDirectoryStorePopulateAndIdentity();
    testDirectoryStore100kEntries();
    testNaturalSorting();
    testDetailsLayoutGeometry();
    testVirtualSelectionModel();
    testSelectionCoalescingRects();

    std::cout << "All Virtualized View tests passed successfully!\n";
    return 0;
}
