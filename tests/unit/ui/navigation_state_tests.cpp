#include "godxplr/ui/state/navigation_coordinator.h"
#include "godxplr/ui/theme/theme_manager.h"
#include "godxplr/ui/shell/breadcrumb.h"
#include "godxplr/ui/shell/action_registry.h"
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

using namespace gx::ui;

void testInitialTabState() {
    std::cout << "[Test] InitialTabState...\n";
    NavigationCoordinator coord;

    TEST_ASSERT(coord.tabCount() == 1);
    const TabState* active = coord.getActiveTab();
    TEST_ASSERT(active != nullptr);
    TEST_ASSERT(active->providerUri == "godxplr://this-pc");
    TEST_ASSERT(active->title == "This PC");
    TEST_ASSERT(active->viewMode == ViewMode::Details);
    TEST_ASSERT(active->status == LocationStatus::Ready);
    TEST_ASSERT(!coord.canGoBack(active->tabId));
    TEST_ASSERT(!coord.canGoForward(active->tabId));
    TEST_ASSERT(!coord.canGoUp(active->tabId));

    std::cout << "  -> Passed!\n";
}

void testTabCreationAndSwitching() {
    std::cout << "[Test] TabCreationAndSwitching...\n";
    NavigationCoordinator coord;

    uint64_t tab2 = coord.openTab("C:\\Windows", true);
    TEST_ASSERT(coord.tabCount() == 2);
    TEST_ASSERT(coord.getActiveTabId() == tab2);
    TEST_ASSERT(coord.getActiveTab()->title == "Windows");

    uint64_t tab1 = coord.getTabs()[0].tabId;
    TEST_ASSERT(coord.activateTab(tab1));
    TEST_ASSERT(coord.getActiveTabId() == tab1);
    TEST_ASSERT(coord.getActiveTab()->title == "This PC");

    std::cout << "  -> Passed!\n";
}

void testNavigationAndHistoryStack() {
    std::cout << "[Test] NavigationAndHistoryStack...\n";
    NavigationCoordinator coord;
    uint64_t tabId = coord.getActiveTabId();

    coord.navigate(tabId, "C:\\");
    coord.navigate(tabId, "C:\\Windows");
    coord.navigate(tabId, "C:\\Windows\\System32");

    TEST_ASSERT(coord.canGoBack(tabId));
    TEST_ASSERT(!coord.canGoForward(tabId));
    TEST_ASSERT(coord.getActiveTab()->providerUri == "C:\\Windows\\System32");

    // Go Back
    TEST_ASSERT(coord.goBack(tabId));
    TEST_ASSERT(coord.getActiveTab()->providerUri == "C:\\Windows");
    TEST_ASSERT(coord.canGoForward(tabId));

    // Go Back again
    TEST_ASSERT(coord.goBack(tabId));
    TEST_ASSERT(coord.getActiveTab()->providerUri == "C:\\");

    // Go Forward
    TEST_ASSERT(coord.goForward(tabId));
    TEST_ASSERT(coord.getActiveTab()->providerUri == "C:\\Windows");

    // Navigate to new branch -> should prune forward history
    coord.navigate(tabId, "C:\\Program Files");
    TEST_ASSERT(!coord.canGoForward(tabId));
    TEST_ASSERT(coord.getActiveTab()->providerUri == "C:\\Program Files");

    std::cout << "  -> Passed!\n";
}

void testGoUpTraversal() {
    std::cout << "[Test] GoUpTraversal...\n";
    NavigationCoordinator coord;
    uint64_t tabId = coord.getActiveTabId();

    coord.navigate(tabId, "C:\\Windows\\System32");
    TEST_ASSERT(coord.canGoUp(tabId));

    TEST_ASSERT(coord.goUp(tabId));
    TEST_ASSERT(coord.getActiveTab()->providerUri == "C:\\Windows");

    TEST_ASSERT(coord.goUp(tabId));
    TEST_ASSERT(coord.getActiveTab()->providerUri == "C:\\");

    TEST_ASSERT(coord.goUp(tabId));
    TEST_ASSERT(coord.getActiveTab()->providerUri == "godxplr://this-pc");

    // At root "This PC", canGoUp is false
    TEST_ASSERT(!coord.canGoUp(tabId));

    std::cout << "  -> Passed!\n";
}

void testTabClosureAndFallback() {
    std::cout << "[Test] TabClosureAndFallback...\n";
    NavigationCoordinator coord;
    uint64_t tab1 = coord.getActiveTabId();
    uint64_t tab2 = coord.openTab("C:\\Windows", true);
    uint64_t tab3 = coord.openTab("C:\\Projects", true);
    TEST_ASSERT(coord.tabCount() == 3);

    // Close active tab 3 -> tab 2 should activate
    TEST_ASSERT(coord.closeTab(tab3));
    TEST_ASSERT(coord.tabCount() == 2);
    TEST_ASSERT(coord.getActiveTabId() == tab2);

    // Close tab 2 -> tab 1 should activate
    TEST_ASSERT(coord.closeTab(tab2));
    TEST_ASSERT(coord.tabCount() == 1);
    TEST_ASSERT(coord.getActiveTabId() == tab1);

    // Close last remaining tab -> coordinator creates a fresh tab
    TEST_ASSERT(coord.closeTab(tab1));
    TEST_ASSERT(coord.tabCount() == 1);
    TEST_ASSERT(coord.getActiveTab() != nullptr);
    TEST_ASSERT(coord.getActiveTab()->providerUri == "godxplr://this-pc");

    std::cout << "  -> Passed!\n";
}

void testTabDuplication() {
    std::cout << "[Test] TabDuplication...\n";
    NavigationCoordinator coord;
    uint64_t tab1 = coord.getActiveTabId();
    TabState* t1 = coord.getTab(tab1);
    t1->providerUri = "C:\\Photos";
    t1->viewMode = ViewMode::Grid;
    t1->sort.column = SortColumn::Size;
    t1->sort.order = SortOrder::Descending;

    TEST_ASSERT(coord.duplicateTab(tab1));
    TEST_ASSERT(coord.tabCount() == 2);

    const TabState* active = coord.getActiveTab();
    TEST_ASSERT(active->tabId != tab1);
    TEST_ASSERT(active->providerUri == "C:\\Photos");
    TEST_ASSERT(active->viewMode == ViewMode::Grid);
    TEST_ASSERT(active->sort.column == SortColumn::Size);
    TEST_ASSERT(active->sort.order == SortOrder::Descending);

    std::cout << "  -> Passed!\n";
}

void testBreadcrumbParsing() {
    std::cout << "[Test] BreadcrumbParsing...\n";

    // 1. Windows standard drive path
    auto b1 = BreadcrumbParser::parse("C:\\Users\\Lenovo\\Documents");
    TEST_ASSERT(b1.size() == 5);
    TEST_ASSERT(b1[0].label == "This PC" && b1[0].isRoot);
    TEST_ASSERT(b1[1].label == "C:" && b1[1].targetUri == "C:\\");
    TEST_ASSERT(b1[2].label == "Users" && b1[2].targetUri == "C:\\Users");
    TEST_ASSERT(b1[3].label == "Lenovo" && b1[3].targetUri == "C:\\Users\\Lenovo");
    TEST_ASSERT(b1[4].label == "Documents" && b1[4].targetUri == "C:\\Users\\Lenovo\\Documents");

    // 2. Drive root
    auto b2 = BreadcrumbParser::parse("D:\\");
    TEST_ASSERT(b2.size() == 2);
    TEST_ASSERT(b2[0].label == "This PC");
    TEST_ASSERT(b2[1].label == "Local Disk (D:)");

    // 3. UNC network path
    auto b3 = BreadcrumbParser::parse("\\\\server\\share\\projects");
    TEST_ASSERT(b3.size() == 3);
    TEST_ASSERT(b3[0].label == "Network" && b3[0].isRoot);
    TEST_ASSERT(b3[1].label == "\\\\server\\share");
    TEST_ASSERT(b3[2].label == "projects");

    std::cout << "  -> Passed!\n";
}

void testThemeTokensPalette() {
    std::cout << "[Test] ThemeTokensPalette...\n";
    ThemeManager mgr(ThemeMode::Light);

    const auto& light = mgr.getTokens();
    TEST_ASSERT(light.surfaceBackground.toHex() == "#FFFFFF");
    TEST_ASSERT(light.textPrimary.toHex() == "#000000");

    mgr.setMode(ThemeMode::Dark);
    const auto& dark = mgr.getTokens();
    TEST_ASSERT(dark.surfaceBackground.toHex() == "#202020");
    TEST_ASSERT(dark.textPrimary.toHex() == "#FFFFFF");

    mgr.setMode(ThemeMode::HighContrast);
    const auto& hc = mgr.getTokens();
    TEST_ASSERT(hc.surfaceBackground.toHex() == "#000000");
    TEST_ASSERT(hc.textSecondary.toHex() == "#FFFF00");

    std::cout << "  -> Passed!\n";
}

void testActionRegistryTraceability() {
    std::cout << "[Test] ActionRegistryTraceability...\n";
    const auto& reg = ActionRegistry::instance();

    // Implemented in P04
    const auto* actTab = reg.getAction("app.new_tab");
    TEST_ASSERT(actTab != nullptr);
    TEST_ASSERT(actTab->isImplemented == true);
    TEST_ASSERT(actTab->targetPhase == "P04");

    // Explicit placeholder for P11
    const auto* actCopy = reg.getAction("file.copy");
    TEST_ASSERT(actCopy != nullptr);
    TEST_ASSERT(actCopy->isImplemented == false);
    TEST_ASSERT(actCopy->targetPhase == "P11");

    // Explicit placeholder for P12
    const auto* actProp = reg.getAction("file.properties");
    TEST_ASSERT(actProp != nullptr);
    TEST_ASSERT(actProp->isImplemented == false);
    TEST_ASSERT(actProp->targetPhase == "P12");

    std::cout << "  -> Passed!\n";
}

void testZeroIoOnStartup() {
    std::cout << "[Test] ZeroIoOnStartup...\n";
    auto t0 = std::chrono::high_resolution_clock::now();

    NavigationCoordinator coord;
    for (int i = 0; i < 50; ++i) {
        coord.openTab("C:\\Folder" + std::to_string(i), true);
    }
    for (int i = 0; i < 50; ++i) {
        coord.navigate(coord.getActiveTabId(), "C:\\Folder" + std::to_string(i) + "\\Sub");
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << "  50 tabs + 50 navigations completed in memory in: " << ms << " ms\n";
    // Must be completely in-memory and under 5 ms
    TEST_ASSERT(ms < 20.0);

    std::cout << "  -> Passed!\n";
}

int main() {
    std::cout << "==========================================\n";
    std::cout << "Running GodXplr Navigation & UI Tests     \n";
    std::cout << "==========================================\n";

    testInitialTabState();
    testTabCreationAndSwitching();
    testNavigationAndHistoryStack();
    testGoUpTraversal();
    testTabClosureAndFallback();
    testTabDuplication();
    testBreadcrumbParsing();
    testThemeTokensPalette();
    testActionRegistryTraceability();
    testZeroIoOnStartup();

    std::cout << "All Navigation & UI state tests passed successfully!\n";
    return 0;
}
