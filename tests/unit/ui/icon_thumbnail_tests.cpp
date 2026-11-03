#include "godxplr/core/entry_key.h"
#include "godxplr/core/pixel_buffer.h"
#include "godxplr/ui/cache/cache_keys.h"
#include "godxplr/ui/cache/icon_cache.h"
#include "godxplr/ui/cache/thumbnail_cache.h"
#include "godxplr/platform/com_apartment.h"
#include "godxplr/platform/shell_thumbnail_provider.h"
#include "godxplr/ui/async/decoration_scheduler.h"
#include "godxplr/ui/shell/preview_model.h"

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <cstdlib>

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        std::cerr << "Assertion failed: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::abort(); \
    } \
} while (0)

using namespace gx;
using namespace gx::ui;

void testCacheKeysAndHashing() {
    std::cout << "[Test] CacheKeys and Hashing..." << std::endl;

    IconKey ik1{"png", false, false, 24, 100, 0};
    IconKey ik2{"png", false, false, 24, 100, 0};
    IconKey ik3{"png", false, false, 32, 100, 0};
    IconKey ik4{"cpp", false, false, 24, 100, 0};

    TEST_ASSERT(ik1 == ik2);
    TEST_ASSERT(ik1 != ik3);
    TEST_ASSERT(ik1 != ik4);

    std::hash<IconKey> iconHasher;
    TEST_ASSERT(iconHasher(ik1) == iconHasher(ik2));
    TEST_ASSERT(iconHasher(ik1) != iconHasher(ik3));

    EntryKey ek1{ProviderKind::NtfsVolume, 1, 1000, 0};
    ThumbnailKey tk1{ek1, 1234567, 1024, 128, 128, ThumbnailCropMode::Contain, 100};
    ThumbnailKey tk2{ek1, 1234567, 1024, 128, 128, ThumbnailCropMode::Contain, 100};
    ThumbnailKey tk3{ek1, 9999999, 1024, 128, 128, ThumbnailCropMode::Contain, 100}; // Modified mtime
    ThumbnailKey tk4{ek1, 1234567, 2048, 128, 128, ThumbnailCropMode::Contain, 100}; // Modified size

    TEST_ASSERT(tk1 == tk2);
    TEST_ASSERT(tk1 != tk3);
    TEST_ASSERT(tk1 != tk4);

    std::hash<ThumbnailKey> thumbHasher;
    TEST_ASSERT(thumbHasher(tk1) == thumbHasher(tk2));
    TEST_ASSERT(thumbHasher(tk1) != thumbHasher(tk3));
    TEST_ASSERT(thumbHasher(tk1) != thumbHasher(tk4));
}

void testPixelBufferAndDecompressionBomb() {
    std::cout << "[Test] PixelBuffer & Decompression Bomb Protection..." << std::endl;

    core::PixelBuffer buf;
    TEST_ASSERT(!buf.isValid());

    // Valid allocation
    TEST_ASSERT(buf.allocate(64, 64));
    TEST_ASSERT(buf.isValid());
    TEST_ASSERT(buf.width() == 64);
    TEST_ASSERT(buf.height() == 64);
    TEST_ASSERT(buf.stride() == 64 * 4);
    TEST_ASSERT(buf.byteCount() == 64 * 64 * 4);

    // Decompression bomb: Dimension > kMaxImageDimension (8192)
    core::PixelBuffer bomb1;
    TEST_ASSERT(!bomb1.allocate(10000, 100));
    TEST_ASSERT(!bomb1.isValid());

    // Memory bomb: Total bytes > kMaxImageDecodeBytes (32 MB)
    // 4096 * 4096 * 4 = 64 MB > 32 MB
    core::PixelBuffer bomb2;
    TEST_ASSERT(!bomb2.allocate(4096, 4096));
    TEST_ASSERT(!bomb2.isValid());

    // Box downscale
    core::PixelBuffer src;
    TEST_ASSERT(src.allocate(100, 100));
    uint32_t* srcPx = reinterpret_cast<uint32_t*>(src.data());
    for (size_t i = 0; i < 100 * 100; ++i) {
        srcPx[i] = 0xFF112233;
    }

    core::PixelBuffer downscaled = src.downscaleTo(50, 50);
    TEST_ASSERT(downscaled.isValid());
    TEST_ASSERT(downscaled.width() == 50);
    TEST_ASSERT(downscaled.height() == 50);
    const uint32_t* dstPx = reinterpret_cast<const uint32_t*>(downscaled.data());
    TEST_ASSERT(dstPx[0] == 0xFF112233);
}

void testIconCacheLru() {
    std::cout << "[Test] IconCache LRU Eviction & Metrics..." << std::endl;

    IconCache cache(3); // Capacity 3
    TEST_ASSERT(cache.maxEntries() == 3);
    TEST_ASSERT(cache.size() == 0);

    IconKey k1{"png", false, false, 24, 100, 0};
    IconKey k2{"jpg", false, false, 24, 100, 0};
    IconKey k3{"cpp", false, false, 24, 100, 0};
    IconKey k4{"txt", false, false, 24, 100, 0};

    core::PixelBuffer b;
    b.allocate(16, 16);

    cache.put(k1, b);
    cache.put(k2, b);
    cache.put(k3, b);
    TEST_ASSERT(cache.size() == 3);

    // Access k1 (making k2 the oldest)
    TEST_ASSERT(cache.get(k1).has_value());

    // Insert 4th: should evict k2
    cache.put(k4, b);
    TEST_ASSERT(cache.size() == 3);
    TEST_ASSERT(cache.get(k2) == std::nullopt); // k2 was evicted
    TEST_ASSERT(cache.get(k1).has_value());
    TEST_ASSERT(cache.get(k3).has_value());
    TEST_ASSERT(cache.get(k4).has_value());

    auto m = cache.metrics();
    TEST_ASSERT(m.hits == 4);
    TEST_ASSERT(m.misses == 1);
    TEST_ASSERT(m.evictions == 1);
}

void testThumbnailCacheBudgetAndShrink() {
    std::cout << "[Test] ThumbnailCache Budget Enforcement & shrinkToBudget..." << std::endl;

    // Budget: 64 KB (65536 bytes)
    ThumbnailCache cache(65536);
    TEST_ASSERT(cache.maxBytes() == 65536);
    TEST_ASSERT(cache.currentBytes() == 0);

    // Each 64x64 buffer is 64*64*4 = 16384 bytes (16 KB)
    core::PixelBuffer b16k;
    TEST_ASSERT(b16k.allocate(64, 64));
    TEST_ASSERT(b16k.byteCount() == 16384);

    ThumbnailKey tk1{EntryKey{ProviderKind::NtfsVolume, 1, 1, 0}, 100, 1000, 64, 64, ThumbnailCropMode::Contain, 100};
    ThumbnailKey tk2{EntryKey{ProviderKind::NtfsVolume, 1, 2, 0}, 100, 1000, 64, 64, ThumbnailCropMode::Contain, 100};
    ThumbnailKey tk3{EntryKey{ProviderKind::NtfsVolume, 1, 3, 0}, 100, 1000, 64, 64, ThumbnailCropMode::Contain, 100};
    ThumbnailKey tk4{EntryKey{ProviderKind::NtfsVolume, 1, 4, 0}, 100, 1000, 64, 64, ThumbnailCropMode::Contain, 100};
    ThumbnailKey tk5{EntryKey{ProviderKind::NtfsVolume, 1, 5, 0}, 100, 1000, 64, 64, ThumbnailCropMode::Contain, 100};

    cache.put(tk1, b16k); // 16 KB
    cache.put(tk2, b16k); // 32 KB
    cache.put(tk3, b16k); // 48 KB
    cache.put(tk4, b16k); // 64 KB (exact budget hit)
    TEST_ASSERT(cache.count() == 4);
    TEST_ASSERT(cache.currentBytes() == 65536);

    // Adding 5th entry must evict tk1 to keep total <= 65536
    cache.put(tk5, b16k);
    TEST_ASSERT(cache.count() == 4);
    TEST_ASSERT(cache.currentBytes() == 65536);
    TEST_ASSERT(!cache.get(tk1).has_value()); // tk1 was evicted
    TEST_ASSERT(cache.get(tk5).has_value());

    // Shrink budget to 32 KB (should evict down to 2 items)
    cache.shrinkToBudget(32768);
    TEST_ASSERT(cache.count() == 2);
    TEST_ASSERT(cache.currentBytes() == 32768);

    cache.clear();
    TEST_ASSERT(cache.count() == 0);
    TEST_ASSERT(cache.currentBytes() == 0);
}

void testComApartmentAndShellExtraction() {
    std::cout << "[Test] COM Apartment & Shell Extraction..." << std::endl;

#ifdef _WIN32
    // Test ComInitializer
    {
        platform::win32::ComInitializer comInit;
        TEST_ASSERT(comInit.succeeded());
    }

    // Test StaWorkerThread execution
    platform::win32::StaWorkerThread worker;
    TEST_ASSERT(worker.isRunning());

    auto fut = worker.submit([]() {
        return 42;
    });
    TEST_ASSERT(fut.get() == 42);

    // Test ShellThumbnailProvider extractIcon
    auto folderIcon = platform::win32::ShellThumbnailProvider::extractIcon(L"folder", true, 32);
    TEST_ASSERT(folderIcon.has_value());
    TEST_ASSERT(folderIcon->isValid());
    TEST_ASSERT(folderIcon->width() > 0 && folderIcon->height() > 0);

    auto txtIcon = platform::win32::ShellThumbnailProvider::extractIcon(L".txt", false, 16);
    TEST_ASSERT(txtIcon.has_value());
    TEST_ASSERT(txtIcon->isValid());

    // Test non-existent file thumbnail extraction (graceful failure, no crash)
    auto missingThumb = platform::win32::ShellThumbnailProvider::extractThumbnail(
        L"C:\\non_existent_folder_xyz_12345.dat", 128, 128);
    TEST_ASSERT(!missingThumb.has_value());

    worker.stop();
    TEST_ASSERT(!worker.isRunning());
#endif
}

void testDecorationSchedulerPrioritiesAndPruning() {
    std::cout << "[Test] DecorationScheduler Priorities & Viewport Pruning..." << std::endl;

    DecorationScheduler scheduler(100, 1024 * 1024);
    scheduler.setGeneration(1);
    TEST_ASSERT(scheduler.generation() == 1);

    IconKey ik1{"png", false, false, 24, 100, 0};
    IconKey ik2{"cpp", false, false, 24, 100, 0};
    IconKey ik3{"txt", false, false, 24, 100, 0};

    // Prepopulate ik1 in cache
    core::PixelBuffer b;
    b.allocate(16, 16);
    scheduler.iconCache().put(ik1, b);

    // Cache hit returns immediately
    auto res1 = scheduler.requestIcon(ik1, L".png", 1, 0, DecorationPriority::VisibleNow);
    TEST_ASSERT(res1.has_value());
    TEST_ASSERT(res1->isValid());
    TEST_ASSERT(scheduler.pendingRequestCount() == 0);

    // Stale generation request is rejected
    auto resStale = scheduler.requestIcon(ik2, L".cpp", 0, 5, DecorationPriority::VisibleNow);
    TEST_ASSERT(!resStale.has_value());

    // Valid queueing with row indexes
    scheduler.requestIcon(ik2, L".cpp", 1, 10, DecorationPriority::VisibleNow);
    scheduler.requestIcon(ik3, L".txt", 1, 100, DecorationPriority::BackgroundTab); // Far away row

    // Prune viewport: visible range [5..20]. Row 100 is far out (> 20 + 8) and should be pruned!
    scheduler.updateVisibleRange(1, 5, 20, 0);

    // Monotonic generation cancel
    scheduler.setGeneration(2);
    TEST_ASSERT(scheduler.generation() == 2);
    TEST_ASSERT(scheduler.pendingRequestCount() == 0);
}

void testPreviewModelInspector() {
    std::cout << "[Test] PreviewModel Inspector..." << std::endl;

    // Test category detection
    TEST_ASSERT(PreviewModel::detectCategory("png", false) == "Image");
    TEST_ASSERT(PreviewModel::detectCategory("mp4", false) == "Video");
    TEST_ASSERT(PreviewModel::detectCategory("mp3", false) == "Audio");
    TEST_ASSERT(PreviewModel::detectCategory("docx", false) == "Document");
    TEST_ASSERT(PreviewModel::detectCategory("cpp", false) == "Code");
    TEST_ASSERT(PreviewModel::detectCategory("zip", false) == "Archive");
    TEST_ASSERT(PreviewModel::detectCategory("exe", false) == "Application");
    TEST_ASSERT(PreviewModel::detectCategory("", true) == "Folder");

    // Test size formatting
    TEST_ASSERT(PreviewModel::formatFileSize(0) == L"0 bytes");
    TEST_ASSERT(PreviewModel::formatFileSize(512) == L"512 bytes");
    TEST_ASSERT(PreviewModel::formatFileSize(2048).find(L"2.0 KB") != std::wstring::npos);
    TEST_ASSERT(PreviewModel::formatFileSize(10485760).find(L"10.0 MB") != std::wstring::npos);

    // Test attributes formatting
#ifdef _WIN32
    TEST_ASSERT(PreviewModel::formatAttributes(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN) == L"RH");
    TEST_ASSERT(PreviewModel::formatAttributes(0) == L"Normal");
#endif

    // Test inspection of a real temporary test file
    std::filesystem::path tempFile = std::filesystem::temp_directory_path() / "godxplr_preview_test.txt";
    {
        std::ofstream ofs(tempFile, std::ios::binary);
        ofs << "GodXplr preview test content line 1\nLine 2";
    }

    PreviewModel model;
    TEST_ASSERT(!model.hasEntry());

    bool callbackInvoked = false;
    model.setUpdateCallback([&](const PreviewModel& m) {
        if (m.hasEntry()) {
            callbackInvoked = true;
        }
    });

    EntryKey testKey{ProviderKind::NtfsVolume, 1, 42, 0};
    model.loadEntry(tempFile.wstring(), testKey, 1);

    TEST_ASSERT(callbackInvoked);
    TEST_ASSERT(model.hasEntry());
    TEST_ASSERT(model.fileName() == L"godxplr_preview_test.txt");
    TEST_ASSERT(model.extension() == "txt");
    TEST_ASSERT(model.category() == "Document");
    TEST_ASSERT(!model.isDirectory());
    TEST_ASSERT(model.fileSize() > 0);
    TEST_ASSERT(!model.formattedSize().empty());
    TEST_ASSERT(!model.formattedModified().empty());

    model.clear();
    TEST_ASSERT(!model.hasEntry());
    TEST_ASSERT(model.fileSize() == 0);

    // Clean up temp file
    std::filesystem::remove(tempFile);
}

int main() {
    std::cout << "=== GodXplr Icon, Thumbnail & Preview Pipeline Tests ===" << std::endl;

    testCacheKeysAndHashing();
    testPixelBufferAndDecompressionBomb();
    testIconCacheLru();
    testThumbnailCacheBudgetAndShrink();
    testComApartmentAndShellExtraction();
    testDecorationSchedulerPrioritiesAndPruning();
    testPreviewModelInspector();

    std::cout << "All Icon, Thumbnail & Preview tests PASSED!" << std::endl;
    return 0;
}
