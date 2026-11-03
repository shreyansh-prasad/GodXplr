#include "godxplr/platform/com_apartment.h"
#include "godxplr/platform/shell_context_menu.h"
#include "godxplr/platform/shell_drag_drop.h"
#include "godxplr/platform/shell_properties.h"

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed at line " << __LINE__ << ": " #cond "\n"; \
            std::abort(); \
        } \
    } while (0)

using namespace gx::platform;

void testHdropPackingAndUnpacking() {
    std::cout << "[Test] CF_HDROP Packing and Unpacking...\n";

    std::vector<std::wstring> inputPaths = {
        L"C:\\Windows\\notepad.exe",
        L"C:\\Windows\\System32\\cmd.exe",
        L"C:\\Users\\Public\\Documents\\Report (2).pdf"
    };

    HGLOBAL hGlobal = ShellDragDrop::createHdropPayload(inputPaths);
    TEST_ASSERT(hGlobal != nullptr);

    auto extracted = ShellDragDrop::extractHdropPayload(hGlobal);
    GlobalFree(hGlobal);

    TEST_ASSERT(extracted.size() == inputPaths.size());
    for (size_t i = 0; i < inputPaths.size(); ++i) {
        TEST_ASSERT(extracted[i] == inputPaths[i]);
    }

    std::cout << "  Extracted " << extracted.size() << " paths successfully.\n";
    std::cout << "  -> PASS\n";
}

void testDropEffectCalculation() {
    std::cout << "[Test] Drop Effect Logic...\n";

    // Same volume default -> Move
    auto effSame = ShellDragDrop::calculateDropEffect(0, true);
    TEST_ASSERT(effSame == DragDropEffect::Move);

    // Cross volume default -> Copy
    auto effCross = ShellDragDrop::calculateDropEffect(0, false);
    TEST_ASSERT(effCross == DragDropEffect::Copy);

    // Ctrl key held -> Forces Copy
    auto effCtrl = ShellDragDrop::calculateDropEffect(MK_CONTROL, true);
    TEST_ASSERT(effCtrl == DragDropEffect::Copy);

    // Shift key held -> Forces Move
    auto effShift = ShellDragDrop::calculateDropEffect(MK_SHIFT, false);
    TEST_ASSERT(effShift == DragDropEffect::Move);

    // Ctrl + Shift -> Link
    auto effLink1 = ShellDragDrop::calculateDropEffect(MK_CONTROL | MK_SHIFT, true);
    TEST_ASSERT(effLink1 == DragDropEffect::Link);

    // Alt -> Link
    auto effLink2 = ShellDragDrop::calculateDropEffect(MK_ALT, false);
    TEST_ASSERT(effLink2 == DragDropEffect::Link);

    std::cout << "  -> PASS\n";
}

void testShellContextMenu() {
    std::cout << "[Test] ShellContextMenu Query...\n";

    win32::ComInitializer comInit(COINIT_APARTMENTTHREADED);

    ShellContextMenu scm;
    std::vector<std::wstring> paths = { L"C:\\Windows\\notepad.exe" };

    bool ok = scm.populate(paths);
    TEST_ASSERT(ok);
    TEST_ASSERT(scm.menuHandle() != nullptr);

    auto items = scm.getMenuItems();
    TEST_ASSERT(!items.empty());

    std::cout << "  Found " << items.size() << " shell menu items on C:\\Windows\\notepad.exe\n";
    for (size_t i = 0; i < std::min<size_t>(items.size(), 5); ++i) {
        std::wcout << L"    Item " << i << L": \"" << items[i].text << L"\"\n";
    }

    std::cout << "  -> PASS\n";
}

void testShellProperties() {
    std::cout << "[Test] ShellProperties Invocation...\n";

    bool ok = ShellProperties::showFileProperties(nullptr, L"C:\\Windows\\notepad.exe");
    TEST_ASSERT(ok);

    std::cout << "  -> PASS\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  GodXplr Spec 05 Shell Integration     \n";
    std::cout << "========================================\n";

    testHdropPackingAndUnpacking();
    testDropEffectCalculation();
    testShellContextMenu();
    testShellProperties();

    std::cout << "========================================\n";
    std::cout << "  ALL SHELL INTEGRATION TESTS PASSED    \n";
    std::cout << "========================================\n";
    return 0;
}
