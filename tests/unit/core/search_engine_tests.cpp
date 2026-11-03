#include "godxplr/core/name_index.h"
#include "godxplr/core/search_engine.h"
#include <iostream>
#include <cstdlib>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond " at " << __FILE__ << ":" << __LINE__ << "\n"; \
            std::abort(); \
        } \
    } while(0)

void test_end_to_end_search() {
    gx::NameIndex index;
    // C:
    gx::EntryId c = index.add(5, 5, "C:", gx::kDir);
    // C:\Projects
    gx::EntryId proj = index.add(10, 5, "Projects", gx::kDir);
    // C:\Projects\GodXplr
    gx::EntryId god = index.add(20, 10, "GodXplr", gx::kDir);

    // Files in GodXplr
    index.add(100, 20, "main.cpp", 0, 4096, 13350000000ULL);
    index.add(101, 20, "types.h", 0, 2048, 13350000000ULL);
    index.add(102, 20, "budget_report_2026.pdf", 0, 10485760, 13350000000ULL); // 10MB
    index.add(103, 20, "expenses.xlsx", 0, 524288, 13350000000ULL);           // 512KB
    index.add(104, 20, "readme.txt", 0, 512, 13350000000ULL);
    (void)c;
    (void)proj;
    (void)god;

    gx::SearchEngine engine(index);

    // Query 1: "main"
    {
        gx::ResultPage page = engine.search("main");
        TEST_ASSERT(page.total == 1);
        TEST_ASSERT(page.rows[0].name == "main.cpp");
        TEST_ASSERT(page.rows[0].dir == "C:\\Projects\\GodXplr");
    }

    // Query 2: "ext:pdf"
    {
        gx::ResultPage page = engine.search("ext:pdf");
        TEST_ASSERT(page.total == 1);
        TEST_ASSERT(page.rows[0].name == "budget_report_2026.pdf");
    }

    // Query 3: Compound "budget ext:pdf"
    {
        gx::ResultPage page = engine.search("budget ext:pdf");
        TEST_ASSERT(page.total == 1);
        TEST_ASSERT(page.rows[0].name == "budget_report_2026.pdf");
    }

    // Query 4: Negative filter "ext:cpp !main"
    {
        gx::ResultPage page = engine.search("ext:cpp !main");
        TEST_ASSERT(page.total == 0);
    }

    // Query 5: Size filter "size:>1mb"
    {
        gx::ResultPage page = engine.search("size:>1mb");
        TEST_ASSERT(page.total == 1);
        TEST_ASSERT(page.rows[0].name == "budget_report_2026.pdf");
    }

    // Query 6: Folder filter "folder:"
    {
        gx::ResultPage page = engine.search("folder:");
        TEST_ASSERT(page.total == 3); // C:, Projects, GodXplr
    }

    std::cout << "[PASS] test_end_to_end_search\n";
}

int main() {
    std::cout << "Running SearchEngine end-to-end tests...\n";
    test_end_to_end_search();
    std::cout << "All SearchEngine end-to-end tests passed successfully.\n";
    return 0;
}
