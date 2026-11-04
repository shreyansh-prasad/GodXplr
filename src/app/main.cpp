#include "main_window.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <string>

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE /*hPrevInstance*/,
    LPWSTR lpCmdLine,
    int nCmdShow)
{
    // Enable Per-Monitor v2 DPI Awareness for razor-sharp rendering on 4K/high-DPI screens
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Initialize STA COM Apartment for Shell extensions & context menus
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    (void)hr;

    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    if (!gx::app::MainWindow::registerWindowClass(hInstance)) {
        CoUninitialize();
        return 1;
    }

    std::wstring initialPath = lpCmdLine ? lpCmdLine : L"";
    // Strip surrounding quotes if present
    if (initialPath.size() >= 2 && initialPath.front() == L'"' && initialPath.back() == L'"') {
        initialPath = initialPath.substr(1, initialPath.size() - 2);
    }

    auto* mainWindow = gx::app::MainWindow::create(hInstance, initialPath);
    if (!mainWindow) {
        CoUninitialize();
        return 1;
    }

    mainWindow->show(nCmdShow);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    delete mainWindow;
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}

