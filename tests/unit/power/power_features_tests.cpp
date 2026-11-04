#include "godxplr/power/batch_renamer.h"
#include "godxplr/power/duplicate_finder.h"
#include "godxplr/power/folder_sync.h"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cstdlib>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed at line " << __LINE__ << ": " #cond "\n"; \
            std::abort(); \
        } \
    } while (0)

using namespace gx::power;

class PowerTestFixture {
public:
    PowerTestFixture() {
        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        root_ = std::filesystem::path(tempPath) / (L"GodXplr_PowerTests_" + std::to_wstring(GetTickCount64()));
        std::error_code ec;
        std::filesystem::create_directories(root_, ec);
    }

    ~PowerTestFixture() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    [[nodiscard]] std::wstring getPath(const std::wstring& rel) const {
        return (root_ / rel).wstring();
    }

    void createFile(const std::wstring& rel, const std::string& data) {
        std::filesystem::path p = root_ / rel;
        std::filesystem::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary);
        out.write(data.data(), data.size());
    }

private:
    std::filesystem::path root_;
};

void testBatchRenamer() {
    std::cout << "[Test] BatchRenamer Rule Transformation & Preview...\n";

    // 1. Single name transform test
    RenameRule r1;
    r1.findPattern = L"photo";
    r1.replacePattern = L"image";
    r1.counterStart = 1;
    r1.counterStep = 1;
    r1.counterPadding = 3;

    std::wstring t1 = BatchRenamer::transformName(L"my_photo_<counter>.jpg", r1, 0);
    TEST_ASSERT(t1 == L"my_image_001.jpg");

    // Case transform
    RenameRule r2;
    r2.caseMode = CaseTransform::TitleCase;
    std::wstring t2 = BatchRenamer::transformName(L"hello_world_spec.txt", r2, 0);
    TEST_ASSERT(t2 == L"Hello_World_Spec.txt");

    // 2. Batch plan with collision detection
    std::vector<std::wstring> files = {
        L"C:\\Docs\\report_2026.pdf",
        L"C:\\Docs\\notes_2026.pdf"
    };

    RenameRule rBatch;
    rBatch.findPattern = L"_2026";
    rBatch.replacePattern = L"_v1";

    auto plan = BatchRenamer::generatePlan(files, rBatch);
    TEST_ASSERT(plan.items.size() == 2);
    TEST_ASSERT(plan.totalConflicts == 0);
    TEST_ASSERT(plan.canCommit);
    TEST_ASSERT(plan.items[0].proposedPath.find(L"report_v1.pdf") != std::wstring::npos);

    // Conflict generation test (mapping two items to the same proposed name)
    RenameRule rConflict;
    rConflict.findPattern = L"notes";
    rConflict.replacePattern = L"report"; // Both would become report_2026.pdf

    auto conflictPlan = BatchRenamer::generatePlan(files, rConflict);
    TEST_ASSERT(conflictPlan.totalConflicts > 0);
    TEST_ASSERT(!conflictPlan.canCommit);

    std::cout << "  -> PASS\n";
}

void testDuplicateFinder(PowerTestFixture& fix) {
    std::cout << "[Test] DuplicateFinder 3-Tier Cascade...\n";

    std::string dataA(16 * 1024, 'A');
    std::string dataB(16 * 1024, 'A'); // Exact duplicate of A
    std::string dataC(16 * 1024, 'B'); // Same size, different bytes
    std::string dataD(8 * 1024,  'A'); // Different size

    fix.createFile(L"fileA.bin", dataA);
    fix.createFile(L"fileB.bin", dataB);
    fix.createFile(L"fileC.bin", dataC);
    fix.createFile(L"fileD.bin", dataD);

    std::vector<DuplicateFileEntry> candidates = {
        { fix.getPath(L"fileA.bin"), static_cast<uint64_t>(dataA.size()), 1000, 1 },
        { fix.getPath(L"fileB.bin"), static_cast<uint64_t>(dataB.size()), 1050, 2 },
        { fix.getPath(L"fileC.bin"), static_cast<uint64_t>(dataC.size()), 1100, 3 },
        { fix.getPath(L"fileD.bin"), static_cast<uint64_t>(dataD.size()), 1200, 4 },
    };

    auto duplicates = DuplicateFinder::findDuplicates(candidates);
    TEST_ASSERT(duplicates.size() == 1);
    TEST_ASSERT(duplicates[0].fileSize == 16 * 1024);
    TEST_ASSERT(duplicates[0].filePaths.size() == 2);

    std::cout << "  Found duplicate set: " << duplicates[0].filePaths.size() << " files matching.\n";
    std::cout << "  -> PASS\n";
}

void testFolderSync(PowerTestFixture& fix) {
    std::cout << "[Test] FolderSync Directory Compare & Planning...\n";

    fix.createFile(L"left/common.txt", "Identical content");
    fix.createFile(L"right/common.txt", "Identical content");

    fix.createFile(L"left/left_only.txt", "Left only payload");
    fix.createFile(L"right/right_only.txt", "Right only payload");

    fix.createFile(L"left/diff.txt", "Left newer version with more bytes");
    fix.createFile(L"right/diff.txt", "Right old");

    std::wstring leftDir = fix.getPath(L"left");
    std::wstring rightDir = fix.getPath(L"right");

    auto plan = FolderSync::compareFolders(leftDir, rightDir, SyncDirection::LeftToRight);
    TEST_ASSERT(!plan.items.empty());

    bool foundLeftOnly = false;
    bool foundRightOnly = false;
    bool foundDifferent = false;

    for (const auto& item : plan.items) {
        if (item.state == SyncItemState::LeftOnly) foundLeftOnly = true;
        if (item.state == SyncItemState::RightOnly) foundRightOnly = true;
        if (item.state == SyncItemState::Different) foundDifferent = true;
    }

    TEST_ASSERT(foundLeftOnly);
    TEST_ASSERT(foundRightOnly);
    TEST_ASSERT(foundDifferent);
    TEST_ASSERT(!plan.plannedActions.empty());

    std::cout << "  Planned " << plan.plannedActions.size() << " sync actions.\n";
    std::cout << "  -> PASS\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  GodXplr Spec 06 Power Features Tests  \n";
    std::cout << "========================================\n";

    PowerTestFixture fix;

    testBatchRenamer();
    testDuplicateFinder(fix);
    testFolderSync(fix);

    std::cout << "========================================\n";
    std::cout << "  ALL POWER FEATURES TESTS PASSED (100%)\n";
    std::cout << "========================================\n";
    return 0;
}

