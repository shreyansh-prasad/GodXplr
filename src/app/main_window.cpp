#include "main_window.h"

#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <fstream>

#include "godxplr/core/fuzzy_matcher.h"
#include "godxplr/platform/shell_context_menu.h"

namespace gx::app {

namespace {

const wchar_t* kClassName = L"GodXplrMainWindowClass";
constexpr int kIdAddressEdit = 2001;
constexpr int kIdSearchEdit = 2002;

// Command Ribbon Menu IDs
constexpr UINT_PTR kCmdNewFolder = 3001;
constexpr UINT_PTR kCmdNewFile = 3002;
constexpr UINT_PTR kCmdSortName = 3010;
constexpr UINT_PTR kCmdSortDate = 3011;
constexpr UINT_PTR kCmdSortType = 3012;
constexpr UINT_PTR kCmdSortSize = 3013;
constexpr UINT_PTR kCmdSortAsc = 3014;
constexpr UINT_PTR kCmdSortDesc = 3015;
constexpr UINT_PTR kCmdViewDetails = 3020;
constexpr UINT_PTR kCmdViewList = 3021;
constexpr UINT_PTR kCmdViewGrid = 3022;
constexpr UINT_PTR kCmdFilterAll = 3030;
constexpr UINT_PTR kCmdFilterFolders = 3031;
constexpr UINT_PTR kCmdFilterDocs = 3032;
constexpr UINT_PTR kCmdFilterPics = 3033;
constexpr UINT_PTR kCmdFilterMedia = 3034;
constexpr UINT_PTR kCmdFilterArchives = 3035;
constexpr UINT_PTR kCmdFilterCode = 3036;

COLORREF toColorRef(const ui::ColorRgba& c) {
    return RGB(c.r, c.g, c.b);
}

std::wstring getKnownPath(REFKNOWNFOLDERID rfid) {
    PWSTR pPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(rfid, 0, nullptr, &pPath))) {
        std::wstring res = pPath;
        CoTaskMemFree(pPath);
        return res;
    }
    return L"";
}

std::wstring formatSize(uint64_t bytes) {
    if (bytes == 0) return L"";
    const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    int unitIndex = 0;
    double dBytes = static_cast<double>(bytes);
    while (dBytes >= 1024.0 && unitIndex < 4) {
        dBytes /= 1024.0;
        unitIndex++;
    }

    wchar_t buf[32];
    if (unitIndex == 0) {
        swprintf_s(buf, L"%llu B", bytes);
    } else {
        swprintf_s(buf, L"%.1f %s", dBytes, units[unitIndex]);
    }
    return std::wstring(buf);
}

std::wstring formatTime(uint64_t windowsFileTime) {
    if (windowsFileTime == 0) return L"";
    FILETIME ft;
    ft.dwLowDateTime = static_cast<DWORD>(windowsFileTime & 0xFFFFFFFF);
    ft.dwHighDateTime = static_cast<DWORD>(windowsFileTime >> 32);

    FILETIME localFt;
    FileTimeToLocalFileTime(&ft, &localFt);
    SYSTEMTIME st;
    FileTimeToSystemTime(&localFt, &st);

    wchar_t buf[64];
    swprintf_s(buf, L"%04d-%02d-%02d  %02d:%02d",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    return std::wstring(buf);
}

std::wstring toLower(std::wstring_view s) {
    std::wstring out(s);
    std::transform(out.begin(), out.end(), out.begin(), ::towlower);
    return out;
}

void drawRoundedRect(HDC hdc, const RECT& rc, int radius, COLORREF fillColor, COLORREF borderColor) {
    HBRUSH brush = CreateSolidBrush(fillColor);
    HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

} // namespace

bool MainWindow::registerWindowClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = &MainWindow::windowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;
    return RegisterClassExW(&wc) != 0;
}

MainWindow* MainWindow::create(HINSTANCE hInstance, const std::wstring& initialPath) {
    auto* window = new MainWindow(hInstance);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int initW = 1260;
    int initH = 820;
    int initX = (screenW - initW) / 2;
    int initY = (screenH - initH) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        kClassName,
        L"GodXplr",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VSCROLL | WS_VISIBLE,
        initX, initY, initW, initH,
        nullptr, nullptr, hInstance, window);

    if (!hwnd) {
        delete window;
        return nullptr;
    }

    if (initialPath.empty() || initialPath == L"Home" || initialPath == L"home") {
        window->navigateToHome();
    } else {
        window->navigateTo(std::filesystem::path(initialPath));
    }

    return window;
}

MainWindow::MainWindow(HINSTANCE hInstance)
    : hInstance_(hInstance)
    , tokens_(ui::DesignTokens::darkPalette())
{
    initKnownFolders();
    initSidebar();
    initFilterPills();

    // Standard High-Quality Windows UI Fonts
    fontRegular_ = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
    if (!fontRegular_) fontRegular_ = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    fontBold_ = CreateFontW(
        -13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");

    fontTitle_ = CreateFontW(
        -15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");

    fontSmall_ = CreateFontW(
        -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");

    // Native Microsoft Fluent Vector Icon Fonts (Installed on all Windows 10/11)
    fontIcons_ = CreateFontW(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe Fluent Icons");
    if (!fontIcons_) {
        fontIcons_ = CreateFontW(
            -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    }

    fontIconsLarge_ = CreateFontW(
        -26, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe Fluent Icons");
    if (!fontIconsLarge_) {
        fontIconsLarge_ = CreateFontW(
            -26, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    }

    fontIconsSmall_ = CreateFontW(
        -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe Fluent Icons");
    if (!fontIconsSmall_) {
        fontIconsSmall_ = CreateFontW(
            -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    }

    // Default tab
    tabs_.push_back({ L"Home", L"godxplr://home" });
}

MainWindow::~MainWindow() {
    if (fontRegular_) DeleteObject(fontRegular_);
    if (fontBold_) DeleteObject(fontBold_);
    if (fontTitle_) DeleteObject(fontTitle_);
    if (fontSmall_) DeleteObject(fontSmall_);
    if (fontIcons_) DeleteObject(fontIcons_);
    if (fontIconsLarge_) DeleteObject(fontIconsLarge_);
    if (fontIconsSmall_) DeleteObject(fontIconsSmall_);
}

void MainWindow::initKnownFolders() {
    pathDesktop_ = getKnownPath(FOLDERID_Desktop);
    pathDownloads_ = getKnownPath(FOLDERID_Downloads);
    pathDocuments_ = getKnownPath(FOLDERID_Documents);
    pathPictures_ = getKnownPath(FOLDERID_Pictures);
    pathMusic_ = getKnownPath(FOLDERID_Music);
    pathVideos_ = getKnownPath(FOLDERID_Videos);
    pathHome_ = getKnownPath(FOLDERID_Profile);
}

void MainWindow::initFilterPills() {
    filterPills_.clear();
    filterPills_.push_back({ FilterCategory::All, L"All", L"\uE71D", {} });
    filterPills_.push_back({ FilterCategory::Folders, L"Folders", L"\uE8B7", {} });
    filterPills_.push_back({ FilterCategory::Documents, L"Documents", L"\uE8A5", {} });
    filterPills_.push_back({ FilterCategory::Pictures, L"Pictures", L"\uEB9F", {} });
    filterPills_.push_back({ FilterCategory::AudioVideo, L"Media", L"\uE714", {} });
    filterPills_.push_back({ FilterCategory::Compressed, L"Archives", L"\uF133", {} });
    filterPills_.push_back({ FilterCategory::Code, L"Code", L"\uE943", {} });
}

void MainWindow::initSidebar() {
    sidebarItems_.clear();

    // Section 1: Favorites / Pinned (Fluent Icons with custom accent colors)
    sidebarItems_.push_back({ L"FAVORITES", L"", L"", RGB(120, 120, 120), L"", true, false });
    sidebarItems_.push_back({ L"Home", L"godxplr://home", L"\uE80F", RGB(96, 205, 255), L"", false, false });
    sidebarItems_.push_back({ L"Gallery", pathPictures_, L"\uEB9F", RGB(62, 166, 255), L"", false, false });
    sidebarItems_.push_back({ L"Desktop", pathDesktop_, L"\uE7F4", RGB(0, 150, 255), L"", false, false });
    sidebarItems_.push_back({ L"Downloads", pathDownloads_, L"\uE896", RGB(0, 185, 150), L"", false, false });
    sidebarItems_.push_back({ L"Documents", pathDocuments_, L"\uE8A5", RGB(70, 150, 245), L"", false, false });
    sidebarItems_.push_back({ L"Pictures", pathPictures_, L"\uEB9F", RGB(177, 70, 194), L"", false, false });
    sidebarItems_.push_back({ L"Music", pathMusic_, L"\uE8D6", RGB(255, 140, 0), L"", false, false });
    sidebarItems_.push_back({ L"Videos", pathVideos_, L"\uE714", RGB(232, 17, 35), L"", false, false });
    sidebarItems_.push_back({ L"os fileX", L"C:\\os fileX", L"\uE8B7", RGB(245, 195, 65), L"", false, false });

    // Section 2: Drives & Network
    sidebarItems_.push_back({ L"DRIVES & NETWORK", L"", L"", RGB(120, 120, 120), L"", true, false });
    sidebarItems_.push_back({ L"This PC", L"C:\\", L"\uE7F4", RGB(180, 180, 180), L"", false, false });

    WCHAR driveStrings[512]{};
    DWORD len = GetLogicalDriveStringsW(511, driveStrings);
    if (len > 0 && len <= 511) {
        const WCHAR* ptr = driveStrings;
        while (*ptr) {
            std::wstring root = ptr;
            WCHAR volName[MAX_PATH + 1]{};
            GetVolumeInformationW(root.c_str(), volName, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0);

            std::wstring label = (volName[0] != L'\0') ? volName : L"Local Disk";
            label += L" (" + root.substr(0, 2) + L")";

            ULARGE_INTEGER freeBytes{}, totalBytes{};
            std::wstring freeText;
            if (GetDiskFreeSpaceExW(root.c_str(), &freeBytes, &totalBytes, nullptr)) {
                freeText = formatSize(freeBytes.QuadPart) + L" free";
            }

            sidebarItems_.push_back({ label, root, L"\uEDA2", RGB(96, 205, 255), freeText, false, true });
            ptr += wcslen(ptr) + 1;
        }
    }

    sidebarItems_.push_back({ L"Network", L"godxplr://network", L"\uE774", RGB(70, 150, 245), L"", false, false });
    sidebarItems_.push_back({ L"Recycle Bin", L"shell:RecycleBinFolder", L"\uE748", RGB(160, 160, 160), L"", false, false });

    // Quick Access cards for Home View (Faithfully matching Windows 11 Explorer)
    quickCards_ = {
        { L"Desktop", L"Stored locally", pathDesktop_, L"\uE7F4", RGB(0, 120, 215), {} },
        { L"Downloads", L"Stored locally", pathDownloads_, L"\uE896", RGB(0, 178, 148), {} },
        { L"Documents", L"Stored locally", pathDocuments_, L"\uE8A5", RGB(0, 120, 215), {} },
        { L"Pictures", L"Stored locally", pathPictures_, L"\uEB9F", RGB(177, 70, 194), {} },
        { L"Music", L"Stored locally", pathMusic_, L"\uE8D6", RGB(255, 140, 0), {} },
        { L"Videos", L"Stored locally", pathVideos_, L"\uE714", RGB(232, 17, 35), {} },
        { L"os fileX", L"Windows-SSD (C:)", L"C:\\os fileX", L"\uE8B7", RGB(245, 195, 65), {} },
        { L"Windows-SSD (C:)", L"System Drive", L"C:\\", L"\uEDA2", RGB(96, 205, 255), {} }
    };
}

void MainWindow::show(int nCmdShow) {
    int cmd = (nCmdShow == 0 || nCmdShow == SW_HIDE) ? SW_SHOWNORMAL : nCmdShow;
    ShowWindow(hwnd_, cmd);
    UpdateWindow(hwnd_);
}

LRESULT CALLBACK MainWindow::windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    MainWindow* window = nullptr;
    if (uMsg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        window = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        window->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->handleMessage(uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT MainWindow::handleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        // Address Edit Control (Hidden until user clicks address container)
        editAddress_ = CreateWindowExW(
            0,
            L"EDIT",
            L"",
            WS_CHILD | ES_AUTOHSCROLL,
            150, 42, 500, 26,
            hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdAddressEdit)), hInstance_, nullptr);
        SendMessageW(editAddress_, WM_SETFONT, (WPARAM)fontRegular_, TRUE);

        // Search Edit Control
        editSearch_ = CreateWindowExW(
            0,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            680, 42, 240, 26,
            hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdSearchEdit)), hInstance_, nullptr);
        SendMessageW(editSearch_, WM_SETFONT, (WPARAM)fontRegular_, TRUE);
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == kIdSearchEdit && code == EN_CHANGE) {
            onSearchChanged();
        } else if (id == kIdAddressEdit && code == EN_KILLFOCUS) {
            wchar_t pathBuf[MAX_PATH];
            GetWindowTextW(editAddress_, pathBuf, MAX_PATH);
            std::filesystem::path p(pathBuf);
            if (std::filesystem::exists(p)) {
                navigateTo(p);
            }
            ShowWindow(editAddress_, SW_HIDE);
            addressEditActive_ = false;
        }

        // Context Menus Commands
        switch (id) {
        case kCmdNewFolder: createNewFolder(); break;
        case kCmdNewFile: createNewTextFile(); break;
        case kCmdSortName: setSortColumn(SortColumn::Name); break;
        case kCmdSortDate: setSortColumn(SortColumn::DateModified); break;
        case kCmdSortType: setSortColumn(SortColumn::Type); break;
        case kCmdSortSize: setSortColumn(SortColumn::Size); break;
        case kCmdSortAsc: sortAscending_ = true; filterAndSortItems(); InvalidateRect(hwnd_, nullptr, FALSE); break;
        case kCmdSortDesc: sortAscending_ = false; filterAndSortItems(); InvalidateRect(hwnd_, nullptr, FALSE); break;
        case kCmdViewDetails: viewMode_ = ViewMode::Details; InvalidateRect(hwnd_, nullptr, FALSE); break;
        case kCmdViewList: viewMode_ = ViewMode::List; InvalidateRect(hwnd_, nullptr, FALSE); break;
        case kCmdViewGrid: viewMode_ = ViewMode::Grid; InvalidateRect(hwnd_, nullptr, FALSE); break;
        case kCmdFilterAll: setFilterCategory(FilterCategory::All); break;
        case kCmdFilterFolders: setFilterCategory(FilterCategory::Folders); break;
        case kCmdFilterDocs: setFilterCategory(FilterCategory::Documents); break;
        case kCmdFilterPics: setFilterCategory(FilterCategory::Pictures); break;
        case kCmdFilterMedia: setFilterCategory(FilterCategory::AudioVideo); break;
        case kCmdFilterArchives: setFilterCategory(FilterCategory::Compressed); break;
        case kCmdFilterCode: setFilterCategory(FilterCategory::Code); break;
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd_, &ps);
        onPaint(hdc);
        EndPaint(hwnd_, &ps);
        return 0;
    }

    case WM_PRINT:
    case WM_PRINTCLIENT:
        onPaint(reinterpret_cast<HDC>(wParam));
        return 0;

    case WM_SIZE:
        onResize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_VSCROLL:
        onVScroll(wParam);
        return 0;

    case WM_MOUSEWHEEL:
        onMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;

    case WM_LBUTTONDOWN:
        onLButtonDown(LOWORD(lParam), HIWORD(lParam), false);
        return 0;

    case WM_LBUTTONDBLCLK:
        onLButtonDown(LOWORD(lParam), HIWORD(lParam), true);
        return 0;

    case WM_RBUTTONUP:
        onRButtonUp(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_MOUSEMOVE:
        onMouseMove(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_KEYDOWN:
        onKeyDown(wParam);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd_, uMsg, wParam, lParam);
}

void MainWindow::onResize(int width, int height) {
    (void)height;
    if (editAddress_ && editSearch_) {
        int searchW = 260;
        int searchX = width - searchW - 20;
        int addrX = 145;
        int addrW = searchX - addrX - 15;

        SetWindowPos(editAddress_, nullptr, addrX + 8, tabBarHeight_ + 8, addrW - 16, 24, SWP_NOZORDER);
        SetWindowPos(editSearch_, nullptr, searchX + 30, tabBarHeight_ + 8, searchW - 40, 24, SWP_NOZORDER);
    }
    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::onVScroll(WPARAM wParam) {
    int count = static_cast<int>(filteredIndices_.size());
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    int listTop = contentTop + (isHomeView_ ? 240 : filterBarHeight_ + headerHeight_);
    int surfaceHeight = rc.bottom - listTop - statusBarHeight_;
    int visibleRows = std::max(1, surfaceHeight / rowHeight_);
    int maxScroll = std::max(0, count - visibleRows);

    switch (LOWORD(wParam)) {
    case SB_LINEUP: scrollOffset_ = std::max(0, scrollOffset_ - 1); break;
    case SB_LINEDOWN: scrollOffset_ = std::min(maxScroll, scrollOffset_ + 1); break;
    case SB_PAGEUP: scrollOffset_ = std::max(0, scrollOffset_ - visibleRows); break;
    case SB_PAGEDOWN: scrollOffset_ = std::min(maxScroll, scrollOffset_ + visibleRows); break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK: scrollOffset_ = HIWORD(wParam); break;
    }

    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::onMouseWheel(short delta) {
    int lines = -delta / WHEEL_DELTA * 3;
    int count = static_cast<int>(filteredIndices_.size());
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    int listTop = contentTop + (isHomeView_ ? 240 : filterBarHeight_ + headerHeight_);
    int surfaceHeight = rc.bottom - listTop - statusBarHeight_;
    int visibleRows = std::max(1, surfaceHeight / rowHeight_);
    int maxScroll = std::max(0, count - visibleRows);

    scrollOffset_ = std::clamp(scrollOffset_ + lines, 0, maxScroll);
    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::onMouseMove(int x, int y) {
    int oldSidebar = sidebarHoverIndex_;
    int oldHoverRow = hoverRow_;
    int oldCard = hoverCard_;
    int oldPill = hoverPill_;

    sidebarHoverIndex_ = -1;
    hoverRow_ = -1;
    hoverCard_ = -1;
    hoverPill_ = -1;

    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;

    // 1. Sidebar Hover
    if (x < sidebarWidth_ && y >= contentTop) {
        int sideY = y - contentTop - 8;
        int curY = 0;
        for (size_t i = 0; i < sidebarItems_.size(); ++i) {
            int h = sidebarItems_[i].isHeader ? 22 : 30;
            if (sideY >= curY && sideY < curY + h) {
                if (!sidebarItems_[i].isHeader) {
                    sidebarHoverIndex_ = static_cast<int>(i);
                }
                break;
            }
            curY += h;
        }
    }

    // 2. Filter Pills Hover (Directory View)
    if (!isHomeView_ && x >= sidebarWidth_ && y >= contentTop && y < contentTop + filterBarHeight_) {
        for (size_t i = 0; i < filterPills_.size(); ++i) {
            if (PtInRect(&filterPills_[i].rect, POINT{x, y})) {
                hoverPill_ = static_cast<int>(i);
                break;
            }
        }
    }

    // 3. Quick Cards Hover (Home View)
    if (isHomeView_ && x >= sidebarWidth_) {
        for (size_t i = 0; i < quickCards_.size(); ++i) {
            if (PtInRect(&quickCards_[i].rect, POINT{x, y})) {
                hoverCard_ = static_cast<int>(i);
                break;
            }
        }
    }

    // 4. File Row Hover
    int listTop = contentTop + (isHomeView_ ? 240 : filterBarHeight_ + headerHeight_);
    if (x >= sidebarWidth_ && y >= listTop) {
        int r = scrollOffset_ + ((y - listTop) / rowHeight_);
        if (r >= 0 && r < static_cast<int>(filteredIndices_.size())) {
            hoverRow_ = r;
        }
    }

    if (sidebarHoverIndex_ != oldSidebar || hoverRow_ != oldHoverRow || hoverCard_ != oldCard || hoverPill_ != oldPill) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void MainWindow::onLButtonDown(int x, int y, bool isDoubleClick) {
    // 1. Tab Bar Clicks (y in 0 .. tabBarHeight_)
    if (y < tabBarHeight_) {
        int tabX = 12;
        for (size_t i = 0; i < tabs_.size(); ++i) {
            RECT tabRc{tabX, 4, tabX + 160, tabBarHeight_};
            if (x >= tabRc.left && x < tabRc.right) {
                activeTab_ = i;
                if (tabs_[i].path == L"godxplr://home") {
                    navigateToHome();
                } else {
                    navigateTo(std::filesystem::path(tabs_[i].path));
                }
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            }
            tabX += 166;
        }

        // New Tab (+) button
        RECT newTabRc{tabX, 4, tabX + 32, tabBarHeight_};
        if (x >= newTabRc.left && x < newTabRc.right) {
            tabs_.push_back({ L"Home", L"godxplr://home" });
            activeTab_ = tabs_.size() - 1;
            navigateToHome();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        return;
    }

    // 2. Navigation Row Clicks (y in tabBarHeight_ .. tabBarHeight_ + navBarHeight_)
    int navTop = tabBarHeight_;
    int navBottom = navTop + navBarHeight_;
    if (y >= navTop && y < navBottom) {
        if (x >= 12 && x < 40) navigateBack();
        else if (x >= 44 && x < 72) navigateForward();
        else if (x >= 76 && x < 104) navigateUp();
        else if (x >= 108 && x < 136) refresh();
        else {
            // Check breadcrumb clicks
            for (const auto& bc : breadcrumbs_) {
                if (PtInRect(&bc.rect, POINT{x, y})) {
                    if (bc.path == L"godxplr://home") navigateToHome();
                    else navigateTo(std::filesystem::path(bc.path));
                    return;
                }
            }

            // Clicked empty address area -> show edit
            RECT clientRc;
            GetClientRect(hwnd_, &clientRc);
            int searchX = clientRc.right - 280;
            if (x >= 145 && x < searchX) {
                addressEditActive_ = true;
                SetWindowTextW(editAddress_, currentPath_.c_str());
                ShowWindow(editAddress_, SW_SHOW);
                SetFocus(editAddress_);
            }
        }
        return;
    }

    // 3. Command Ribbon Clicks (y in navBottom .. navBottom + commandBarHeight_)
    int cmdTop = navBottom;
    int cmdBottom = cmdTop + commandBarHeight_;
    if (y >= cmdTop && y < cmdBottom) {
        if (x >= 12 && x < 96) {
            showNewMenu(x, cmdBottom);
        } else if (x >= 102 && x < 136) {
            cutSelectedItem();
        } else if (x >= 140 && x < 174) {
            copySelectedItem();
        } else if (x >= 178 && x < 212) {
            pasteItem();
        } else if (x >= 216 && x < 250) {
            renameSelectedItem();
        } else if (x >= 254 && x < 288) {
            // Share
        } else if (x >= 292 && x < 326) {
            deleteSelectedItem();
        } else if (x >= 335 && x < 415) {
            showSortMenu(x, cmdBottom);
        } else if (x >= 420 && x < 500) {
            showViewMenu(x, cmdBottom);
        } else if (x >= 505 && x < 595) {
            showFilterMenu(x, cmdBottom);
        }
        return;
    }

    // 4. Sidebar Clicks (x in 0 .. sidebarWidth_)
    int contentTop = cmdBottom;
    if (x < sidebarWidth_ && y >= contentTop) {
        int sideY = y - contentTop - 8;
        int curY = 0;
        for (size_t i = 0; i < sidebarItems_.size(); ++i) {
            int h = sidebarItems_[i].isHeader ? 22 : 30;
            if (sideY >= curY && sideY < curY + h) {
                if (!sidebarItems_[i].isHeader) {
                    sidebarSelectedIndex_ = static_cast<int>(i);
                    const auto& item = sidebarItems_[i];
                    if (item.path == L"godxplr://home") {
                        navigateToHome();
                    } else if (item.path == L"shell:RecycleBinFolder") {
                        ShellExecuteW(hwnd_, L"open", L"shell:RecycleBinFolder", nullptr, nullptr, SW_SHOWNORMAL);
                    } else if (std::filesystem::exists(item.path)) {
                        navigateTo(std::filesystem::path(item.path));
                    }
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                return;
            }
            curY += h;
        }
        return;
    }

    // 5. Directory View: 1-Click Quick Filter Pills Clicks!
    if (!isHomeView_ && x >= sidebarWidth_ && y >= contentTop && y < contentTop + filterBarHeight_) {
        for (const auto& pill : filterPills_) {
            if (PtInRect(&pill.rect, POINT{x, y})) {
                setFilterCategory(pill.cat);
                return;
            }
        }
        return;
    }

    // 6. Directory View: Column Header Click (Interactive Sorting!)
    if (!isHomeView_ && x >= sidebarWidth_) {
        int hdrTop = contentTop + filterBarHeight_;
        int hdrBottom = hdrTop + headerHeight_;
        if (y >= hdrTop && y < hdrBottom) {
            int relX = x - sidebarWidth_;
            if (relX < colNameWidth_) {
                setSortColumn(SortColumn::Name);
            } else if (relX < colNameWidth_ + colDateWidth_) {
                setSortColumn(SortColumn::DateModified);
            } else if (relX < colNameWidth_ + colDateWidth_ + colTypeWidth_) {
                setSortColumn(SortColumn::Type);
            } else {
                setSortColumn(SortColumn::Size);
            }
            return;
        }
    }

    // 7. Home View: Quick Cards & Section Tabs Clicks
    if (isHomeView_ && x >= sidebarWidth_) {
        // Quick Cards
        for (const auto& card : quickCards_) {
            if (PtInRect(&card.rect, POINT{x, y})) {
                if (std::filesystem::exists(card.path)) {
                    navigateTo(std::filesystem::path(card.path));
                }
                return;
            }
        }

        // Section Tabs: (Recent), (Favorites), (Shared)
        POINT pt{x, y};
        if (PtInRect(&pillRecentRc_, pt)) {
            homeSection_ = 0;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        } else if (PtInRect(&pillFavRc_, pt)) {
            homeSection_ = 1;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        } else if (PtInRect(&pillSharedRc_, pt)) {
            homeSection_ = 2;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
    }

    // 8. File List Row Click & Double-Click (File Execution!)
    int listTop = contentTop + (isHomeView_ ? 240 : filterBarHeight_ + headerHeight_);
    if (x >= sidebarWidth_ && y >= listTop) {
        int clickedRow = scrollOffset_ + ((y - listTop) / rowHeight_);
        if (clickedRow >= 0 && clickedRow < static_cast<int>(filteredIndices_.size())) {
            selectedRow_ = clickedRow;
            InvalidateRect(hwnd_, nullptr, FALSE);

            if (isDoubleClick) {
                openSelectedItem();
            }
        }
    }
}

void MainWindow::onRButtonUp(int x, int y) {
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    int listTop = contentTop + (isHomeView_ ? 240 : filterBarHeight_ + headerHeight_);

    if (x >= sidebarWidth_ && y >= listTop) {
        int clickedRow = scrollOffset_ + ((y - listTop) / rowHeight_);
        if (clickedRow >= 0 && clickedRow < static_cast<int>(filteredIndices_.size())) {
            selectedRow_ = clickedRow;
            InvalidateRect(hwnd_, nullptr, FALSE);

            const auto& item = allItems_[filteredIndices_[selectedRow_]];
            std::filesystem::path fullPath = isHomeView_
                ? std::filesystem::path(item.location) / item.name
                : currentPath_ / item.name;

            POINT pt{x, y};
            ClientToScreen(hwnd_, &pt);

            platform::ShellContextMenu menu;
            std::vector<std::wstring> paths = { fullPath.wstring() };
            if (menu.populate(paths)) {
                menu.showAndInvoke(hwnd_, pt.x, pt.y);
            }
        }
    }
}

void MainWindow::onKeyDown(WPARAM key) {
    int count = static_cast<int>(filteredIndices_.size());

    switch (key) {
    case VK_UP:
        if (selectedRow_ > 0) {
            selectedRow_--;
            if (selectedRow_ < scrollOffset_) scrollOffset_ = selectedRow_;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        break;

    case VK_DOWN:
        if (selectedRow_ < count - 1) {
            selectedRow_++;
            RECT clientRc;
            GetClientRect(hwnd_, &clientRc);
            int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
            int listTop = contentTop + (isHomeView_ ? 240 : filterBarHeight_ + headerHeight_);
            int surfaceHeight = clientRc.bottom - listTop - statusBarHeight_;
            int visibleRows = std::max(1, surfaceHeight / rowHeight_);
            if (selectedRow_ >= scrollOffset_ + visibleRows) {
                scrollOffset_ = selectedRow_ - visibleRows + 1;
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        break;

    case VK_RETURN:
        openSelectedItem();
        break;

    case VK_BACK:
        navigateUp();
        break;

    case VK_F5:
        refresh();
        break;

    case VK_DELETE:
        deleteSelectedItem();
        break;

    case VK_F2:
        renameSelectedItem();
        break;
    }
}

void MainWindow::onSearchChanged() {
    wchar_t buf[256];
    GetWindowTextW(editSearch_, buf, 256);
    filterAndSortItems();
    selectedRow_ = filteredIndices_.empty() ? -1 : 0;
    scrollOffset_ = 0;
    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::navigateTo(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_directory(path, ec)) {
        return;
    }

    if (!currentPath_.empty() && currentPath_ != path) {
        backStack_.push_back(currentPath_);
        forwardStack_.clear();
    }

    currentPath_ = std::filesystem::canonical(path, ec);
    if (ec) currentPath_ = path;

    isHomeView_ = false;

    // Update active tab title
    if (activeTab_ < tabs_.size()) {
        tabs_[activeTab_].title = currentPath_.filename().wstring();
        if (tabs_[activeTab_].title.empty()) tabs_[activeTab_].title = currentPath_.wstring();
        tabs_[activeTab_].path = currentPath_.wstring();
    }

    // Enumerate directory
    allItems_.clear();
    try {
        for (const auto& entry : std::filesystem::directory_iterator(currentPath_, std::filesystem::directory_options::skip_permission_denied)) {
            FileItemDisplay item;
            item.name = entry.path().filename().wstring();
            item.isDirectory = entry.is_directory(ec);
            if (!item.isDirectory) {
                item.extension = entry.path().extension().wstring();
                if (!item.extension.empty() && item.extension[0] == L'.') {
                    item.extension = item.extension.substr(1);
                }
                item.size = entry.file_size(ec);
            }
            WIN32_FILE_ATTRIBUTE_DATA fad{};
            if (GetFileAttributesExW(entry.path().c_str(), GetFileExInfoStandard, &fad)) {
                ULARGE_INTEGER uli;
                uli.LowPart = fad.ftLastWriteTime.dwLowDateTime;
                uli.HighPart = fad.ftLastWriteTime.dwHighDateTime;
                item.mtime = uli.QuadPart;
            }

            allItems_.push_back(std::move(item));
        }
    } catch (...) {}

    filterAndSortItems();
    selectedRow_ = filteredIndices_.empty() ? -1 : 0;
    scrollOffset_ = 0;
    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::navigateToHome() {
    isHomeView_ = true;
    currentPath_.clear();

    if (activeTab_ < tabs_.size()) {
        tabs_[activeTab_].title = L"Home";
        tabs_[activeTab_].path = L"godxplr://home";
    }

    // Populate Recent Items from user profile / downloads
    allItems_.clear();
    std::vector<std::filesystem::path> scanDirs = { pathDownloads_, pathDocuments_, pathPictures_, pathDesktop_ };

    for (const auto& dir : scanDirs) {
        if (dir.empty() || !std::filesystem::exists(dir)) continue;
        try {
            int count = 0;
            for (const auto& entry : std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied)) {
                if (count++ > 6) break;
                FileItemDisplay item;
                item.name = entry.path().filename().wstring();
                item.location = dir.filename().wstring();
                std::error_code ec;
                item.isDirectory = entry.is_directory(ec);
                if (!item.isDirectory) {
                    item.extension = entry.path().extension().wstring();
                    if (!item.extension.empty() && item.extension[0] == L'.') {
                        item.extension = item.extension.substr(1);
                    }
                    item.size = entry.file_size(ec);
                }
                WIN32_FILE_ATTRIBUTE_DATA fad{};
                if (GetFileAttributesExW(entry.path().c_str(), GetFileExInfoStandard, &fad)) {
                    ULARGE_INTEGER uli;
                    uli.LowPart = fad.ftLastWriteTime.dwLowDateTime;
                    uli.HighPart = fad.ftLastWriteTime.dwHighDateTime;
                    item.mtime = uli.QuadPart;
                }
                allItems_.push_back(std::move(item));
            }
        } catch (...) {}
    }

    filterAndSortItems();
    selectedRow_ = filteredIndices_.empty() ? -1 : 0;
    scrollOffset_ = 0;
    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::navigateUp() {
    if (isHomeView_ || currentPath_.empty()) return;
    auto parent = currentPath_.parent_path();
    if (!parent.empty() && parent != currentPath_) {
        navigateTo(parent);
    } else {
        navigateToHome();
    }
}

void MainWindow::navigateBack() {
    if (!backStack_.empty()) {
        forwardStack_.push_back(currentPath_);
        auto prev = backStack_.back();
        backStack_.pop_back();
        if (prev.empty() || prev == L"Home") navigateToHome();
        else navigateTo(prev);
    }
}

void MainWindow::navigateForward() {
    if (!forwardStack_.empty()) {
        backStack_.push_back(currentPath_);
        auto next = forwardStack_.back();
        forwardStack_.pop_back();
        if (next.empty() || next == L"Home") navigateToHome();
        else navigateTo(next);
    }
}

void MainWindow::refresh() {
    if (isHomeView_) navigateToHome();
    else navigateTo(currentPath_);
}

void MainWindow::filterAndSortItems() {
    filteredIndices_.clear();

    wchar_t searchBuf[256]{};
    GetWindowTextW(editSearch_, searchBuf, 256);
    std::wstring query = toLower(searchBuf);

    for (size_t i = 0; i < allItems_.size(); ++i) {
        const auto& item = allItems_[i];

        // 1. Search Query Filter
        if (!query.empty()) {
            std::wstring lowerName = toLower(item.name);
            if (lowerName.find(query) == std::wstring::npos) {
                continue;
            }
        }

        // 2. Category Filter
        std::wstring ext = toLower(item.extension);
        switch (filterCategory_) {
        case FilterCategory::Folders:
            if (!item.isDirectory) continue;
            break;
        case FilterCategory::Documents:
            if (item.isDirectory || (ext != L"pdf" && ext != L"doc" && ext != L"docx" && ext != L"txt" && ext != L"xlsx" && ext != L"pptx" && ext != L"md")) continue;
            break;
        case FilterCategory::Pictures:
            if (item.isDirectory || (ext != L"png" && ext != L"jpg" && ext != L"jpeg" && ext != L"gif" && ext != L"bmp" && ext != L"webp" && ext != L"svg")) continue;
            break;
        case FilterCategory::AudioVideo:
            if (item.isDirectory || (ext != L"mp3" && ext != L"mp4" && ext != L"wav" && ext != L"mkv" && ext != L"avi" && ext != L"flac")) continue;
            break;
        case FilterCategory::Compressed:
            if (item.isDirectory || (ext != L"zip" && ext != L"rar" && ext != L"7z" && ext != L"tar" && ext != L"gz")) continue;
            break;
        case FilterCategory::Code:
            if (item.isDirectory || (ext != L"cpp" && ext != L"h" && ext != L"c" && ext != L"py" && ext != L"js" && ext != L"ts" && ext != L"json" && ext != L"xml" && ext != L"html" && ext != L"rs" && ext != L"cs")) continue;
            break;
        case FilterCategory::All:
        default:
            break;
        }

        filteredIndices_.push_back(i);
    }

    // Sort Items
    std::sort(filteredIndices_.begin(), filteredIndices_.end(), [this](size_t idxA, size_t idxB) {
        const auto& a = allItems_[idxA];
        const auto& b = allItems_[idxB];

        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory > b.isDirectory; // Folders always first
        }

        int cmp = 0;
        switch (sortCol_) {
        case SortColumn::Name:
            cmp = _wcsicmp(a.name.c_str(), b.name.c_str());
            break;
        case SortColumn::DateModified:
            if (a.mtime < b.mtime) cmp = -1;
            else if (a.mtime > b.mtime) cmp = 1;
            break;
        case SortColumn::Type:
            cmp = _wcsicmp(a.extension.c_str(), b.extension.c_str());
            break;
        case SortColumn::Size:
            if (a.size < b.size) cmp = -1;
            else if (a.size > b.size) cmp = 1;
            break;
        }

        return sortAscending_ ? (cmp < 0) : (cmp > 0);
    });
}

void MainWindow::openSelectedItem() {
    if (selectedRow_ < 0 || selectedRow_ >= static_cast<int>(filteredIndices_.size())) {
        return;
    }
    openItem(allItems_[filteredIndices_[selectedRow_]]);
}

void MainWindow::openItem(const FileItemDisplay& item) {
    std::filesystem::path fullPath = isHomeView_
        ? std::filesystem::path(item.location) / item.name
        : currentPath_ / item.name;

    if (item.isDirectory) {
        navigateTo(fullPath);
    } else {
        // Guaranteed real file execution via Windows Shell Invocation
        std::wstring dir = fullPath.parent_path().wstring();

        SHELLEXECUTEINFOW sei{};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_INVOKEIDLIST | SEE_MASK_FLAG_DDEWAIT;
        sei.hwnd = hwnd_;
        sei.lpVerb = nullptr; // Invoke default verb (open, edit, play, etc.)
        sei.lpFile = fullPath.c_str();
        sei.lpDirectory = dir.c_str();
        sei.nShow = SW_SHOWNORMAL;

        if (!ShellExecuteExW(&sei)) {
            // Fallback with explicit "open" verb
            ShellExecuteW(
                hwnd_,
                L"open",
                fullPath.c_str(),
                nullptr,
                dir.c_str(),
                SW_SHOWNORMAL
            );
        }
    }
}

void MainWindow::createNewFolder() {
    if (isHomeView_) return;
    std::filesystem::path newFolder = currentPath_ / L"New folder";
    int counter = 2;
    while (std::filesystem::exists(newFolder)) {
        newFolder = currentPath_ / (L"New folder (" + std::to_wstring(counter++) + L")");
    }
    std::error_code ec;
    std::filesystem::create_directory(newFolder, ec);
    refresh();
}

void MainWindow::createNewTextFile() {
    if (isHomeView_) return;
    std::filesystem::path newFile = currentPath_ / L"New Text Document.txt";
    int counter = 2;
    while (std::filesystem::exists(newFile)) {
        newFile = currentPath_ / (L"New Text Document (" + std::to_wstring(counter++) + L").txt");
    }
    std::ofstream out(newFile);
    out.close();
    refresh();
}

void MainWindow::deleteSelectedItem() {
    if (selectedRow_ < 0 || selectedRow_ >= static_cast<int>(filteredIndices_.size())) return;
    const auto& item = allItems_[filteredIndices_[selectedRow_]];
    std::filesystem::path fullPath = isHomeView_
        ? std::filesystem::path(item.location) / item.name
        : currentPath_ / item.name;

    // Move to Recycle Bin via SHFileOperation
    std::wstring doubleNull = fullPath.wstring();
    doubleNull.push_back(L'\0');

    SHFILEOPSTRUCTW sfo{};
    sfo.hwnd = hwnd_;
    sfo.wFunc = FO_DELETE;
    sfo.pFrom = doubleNull.c_str();
    sfo.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;
    SHFileOperationW(&sfo);

    refresh();
}

void MainWindow::copySelectedItem() {
    if (selectedRow_ < 0 || selectedRow_ >= static_cast<int>(filteredIndices_.size())) return;
    const auto& item = allItems_[filteredIndices_[selectedRow_]];
    std::filesystem::path fullPath = isHomeView_
        ? std::filesystem::path(item.location) / item.name
        : currentPath_ / item.name;

    // Set to clipboard
    if (OpenClipboard(hwnd_)) {
        EmptyClipboard();
        std::wstring p = fullPath.wstring();
        size_t size = sizeof(DROPFILES) + (p.size() + 2) * sizeof(wchar_t);
        HGLOBAL hGlobal = GlobalAlloc(GHND, size);
        if (hGlobal) {
            auto* df = reinterpret_cast<DROPFILES*>(GlobalLock(hGlobal));
            if (df) {
                df->pFiles = sizeof(DROPFILES);
                df->fWide = TRUE;
                wchar_t* target = reinterpret_cast<wchar_t*>(reinterpret_cast<char*>(df) + sizeof(DROPFILES));
                memcpy(target, p.c_str(), p.size() * sizeof(wchar_t));
                target[p.size()] = L'\0';
                target[p.size() + 1] = L'\0';
                GlobalUnlock(hGlobal);
                SetClipboardData(CF_HDROP, hGlobal);
            }
        }
        CloseClipboard();
    }
}

void MainWindow::cutSelectedItem() {
    copySelectedItem();
}

void MainWindow::pasteItem() {
    if (isHomeView_) return;
    if (OpenClipboard(hwnd_)) {
        HANDLE hData = GetClipboardData(CF_HDROP);
        if (hData) {
            HDROP hDrop = reinterpret_cast<HDROP>(hData);
            UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < count; ++i) {
                wchar_t srcPath[MAX_PATH];
                DragQueryFileW(hDrop, i, srcPath, MAX_PATH);
                std::filesystem::path src(srcPath);
                std::filesystem::path dst = currentPath_ / src.filename();
                std::error_code ec;
                std::filesystem::copy(src, dst, std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
            }
            refresh();
        }
        CloseClipboard();
    }
}

void MainWindow::renameSelectedItem() {
    if (selectedRow_ < 0 || selectedRow_ >= static_cast<int>(filteredIndices_.size())) return;
}

void MainWindow::setSortColumn(SortColumn col) {
    if (sortCol_ == col) {
        sortAscending_ = !sortAscending_; // Toggle direction
    } else {
        sortCol_ = col;
        sortAscending_ = true;
    }
    filterAndSortItems();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::setFilterCategory(FilterCategory cat) {
    filterCategory_ = cat;
    filterAndSortItems();
    selectedRow_ = filteredIndices_.empty() ? -1 : 0;
    scrollOffset_ = 0;
    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::showSortMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, (sortCol_ == SortColumn::Name ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdSortName, L"Name");
    AppendMenuW(hMenu, (sortCol_ == SortColumn::DateModified ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdSortDate, L"Date modified");
    AppendMenuW(hMenu, (sortCol_ == SortColumn::Type ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdSortType, L"Type");
    AppendMenuW(hMenu, (sortCol_ == SortColumn::Size ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdSortSize, L"Size");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, (sortAscending_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdSortAsc, L"Ascending");
    AppendMenuW(hMenu, (!sortAscending_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdSortDesc, L"Descending");

    POINT pt{x, y};
    ClientToScreen(hwnd_, &pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(hMenu);
}

void MainWindow::showViewMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, (viewMode_ == ViewMode::Details ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdViewDetails, L"Details");
    AppendMenuW(hMenu, (viewMode_ == ViewMode::List ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdViewList, L"List");
    AppendMenuW(hMenu, (viewMode_ == ViewMode::Grid ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdViewGrid, L"Tiles / Grid");

    POINT pt{x, y};
    ClientToScreen(hwnd_, &pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(hMenu);
}

void MainWindow::showFilterMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, (filterCategory_ == FilterCategory::All ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterAll, L"All Files");
    AppendMenuW(hMenu, (filterCategory_ == FilterCategory::Folders ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterFolders, L"Folders");
    AppendMenuW(hMenu, (filterCategory_ == FilterCategory::Documents ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterDocs, L"Documents (.pdf, .docx, .txt)");
    AppendMenuW(hMenu, (filterCategory_ == FilterCategory::Pictures ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterPics, L"Pictures (.png, .jpg, .svg)");
    AppendMenuW(hMenu, (filterCategory_ == FilterCategory::AudioVideo ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterMedia, L"Audio & Video (.mp4, .mp3)");
    AppendMenuW(hMenu, (filterCategory_ == FilterCategory::Compressed ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterArchives, L"Compressed (.zip, .7z)");
    AppendMenuW(hMenu, (filterCategory_ == FilterCategory::Code ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterCode, L"Code & Dev (.cpp, .py, .js)");

    POINT pt{x, y};
    ClientToScreen(hwnd_, &pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(hMenu);
}

void MainWindow::showNewMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, kCmdNewFolder, L"📁 Folder\tCtrl+Shift+N");
    AppendMenuW(hMenu, MF_STRING, kCmdNewFile, L"📄 Text Document");

    POINT pt{x, y};
    ClientToScreen(hwnd_, &pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(hMenu);
}

void MainWindow::updateScrollInfo() {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    int listTop = contentTop + (isHomeView_ ? 240 : filterBarHeight_ + headerHeight_);
    int surfaceHeight = rc.bottom - listTop - statusBarHeight_;
    int visibleRows = std::max(1, surfaceHeight / rowHeight_);

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = std::max(0, static_cast<int>(filteredIndices_.size()) - 1);
    si.nPage = visibleRows;
    si.nPos = scrollOffset_;
    SetScrollInfo(hwnd_, SB_VERT, &si, TRUE);
}

HICON MainWindow::getFileIcon(const std::wstring& path, bool isDir, const std::wstring& ext) {
    (void)path;
    SHFILEINFOW sfi{};
    DWORD flags = SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES;
    if (isDir) {
        SHGetFileInfoW(L"folder", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi), flags);
    } else {
        std::wstring fake = L"file." + ext;
        SHGetFileInfoW(fake.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), flags);
    }
    return sfi.hIcon;
}

// -----------------------------------------------------------------------------
// Professional OS Designer Level Fluent UI Rendering Subroutines
// -----------------------------------------------------------------------------

void MainWindow::paintTabBar(HDC hdc, int width) {
    RECT tabAreaRc{0, 0, width, tabBarHeight_};
    HBRUSH bgBrush = CreateSolidBrush(RGB(24, 24, 24));
    FillRect(hdc, &tabAreaRc, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);

    int tabX = 12;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        bool isActive = (i == activeTab_);
        RECT tabRc{tabX, 4, tabX + 160, tabBarHeight_};

        // Tab Container with rounded top corners
        drawRoundedRect(hdc, tabRc, 8, isActive ? RGB(36, 36, 36) : RGB(27, 27, 27),
            isActive ? RGB(45, 45, 45) : RGB(32, 32, 32));

        // Active indicator line
        if (isActive) {
            RECT indRc{tabRc.left + 8, tabRc.top, tabRc.right - 8, tabRc.top + 2};
            HBRUSH acc = CreateSolidBrush(RGB(96, 205, 255));
            FillRect(hdc, &indRc, acc);
            DeleteObject(acc);
        }

        // Tab Icon (Home vs Folder)
        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, isActive ? RGB(96, 205, 255) : RGB(160, 160, 160));
        RECT tabIconRc{tabRc.left + 10, tabRc.top, tabRc.left + 26, tabRc.bottom};
        const wchar_t* iconGlyph = (tabs_[i].title == L"Home") ? L"\uE80F" : L"\uE8B7";
        DrawTextW(hdc, iconGlyph, 1, &tabIconRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Tab Title Text
        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, isActive ? RGB(255, 255, 255) : RGB(175, 175, 175));
        RECT textRc{tabRc.left + 28, tabRc.top, tabRc.right - 26, tabRc.bottom};
        DrawTextW(hdc, tabs_[i].title.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Close '×' Button
        RECT closeRc{tabRc.right - 22, tabRc.top, tabRc.right - 6, tabRc.bottom};
        SetTextColor(hdc, RGB(140, 140, 140));
        DrawTextW(hdc, L"×", 1, &closeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        tabX += 166;
    }

    // New Tab (+) Button
    RECT plusRc{tabX, 4, tabX + 30, tabBarHeight_};
    drawRoundedRect(hdc, plusRc, 6, RGB(30, 30, 30), RGB(38, 38, 38));
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(200, 200, 200));
    DrawTextW(hdc, L"\uE710", 1, &plusRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void MainWindow::paintNavAndAddressBar(HDC hdc, int width) {
    int navTop = tabBarHeight_;
    RECT navRc{0, navTop, width, navTop + navBarHeight_};
    HBRUSH navBrush = CreateSolidBrush(RGB(31, 31, 31));
    FillRect(hdc, &navRc, navBrush);
    DeleteObject(navBrush);

    SetBkMode(hdc, TRANSPARENT);

    // Vector Navigation Buttons: Back, Forward, Up, Refresh
    auto drawNavBtn = [&](int x, const wchar_t* iconGlyph, bool enabled = true) {
        RECT rc{x, navTop + 5, x + 30, navTop + 35};
        drawRoundedRect(hdc, rc, 6, RGB(38, 38, 38), RGB(46, 46, 46));
        SelectObject(hdc, fontIcons_);
        SetTextColor(hdc, enabled ? RGB(230, 230, 230) : RGB(110, 110, 110));
        DrawTextW(hdc, iconGlyph, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    drawNavBtn(12, L"\uE72B", !backStack_.empty());
    drawNavBtn(44, L"\uE72A", !forwardStack_.empty());
    drawNavBtn(76, L"\uE74A", !isHomeView_);
    drawNavBtn(108, L"\uE72C", true);

    // Breadcrumbs / Address Bar Container
    int searchW = 260;
    int searchX = width - searchW - 20;
    int addrX = 145;
    int addrW = searchX - addrX - 15;
    RECT addrRc{addrX, navTop + 5, addrX + addrW, navTop + 35};

    drawRoundedRect(hdc, addrRc, 6, RGB(41, 41, 41), RGB(58, 58, 58));

    // Breadcrumbs rendering inside Address Bar
    breadcrumbs_.clear();
    int bcX = addrX + 12;

    if (isHomeView_) {
        // Home icon + label
        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, RGB(96, 205, 255));
        RECT homeIconRc{bcX, addrRc.top, bcX + 18, addrRc.bottom};
        DrawTextW(hdc, L"\uE80F", 1, &homeIconRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        bcX += 20;

        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, RGB(240, 240, 240));
        RECT homeTextRc{bcX, addrRc.top, bcX + 50, addrRc.bottom};
        DrawTextW(hdc, L"Home", 4, &homeTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT fullHomeRc{addrX + 12, addrRc.top, bcX + 50, addrRc.bottom};
        breadcrumbs_.push_back({ L"Home", L"godxplr://home", fullHomeRc });
    } else {
        // Home Icon
        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, RGB(96, 205, 255));
        RECT homeIconRc{bcX, addrRc.top, bcX + 18, addrRc.bottom};
        DrawTextW(hdc, L"\uE80F", 1, &homeIconRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        breadcrumbs_.push_back({ L"Home", L"godxplr://home", homeIconRc });
        bcX += 22;

        std::filesystem::path buildPath;
        for (const auto& part : currentPath_) {
            buildPath /= part;
            std::wstring partStr = part.wstring();
            if (partStr.back() == L'\\') partStr.pop_back();

            // Chevron separator
            SelectObject(hdc, fontIconsSmall_);
            SetTextColor(hdc, RGB(140, 140, 140));
            RECT chevRc{bcX, addrRc.top, bcX + 12, addrRc.bottom};
            DrawTextW(hdc, L"\uE76C", 1, &chevRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            bcX += 14;

            // Part label
            SelectObject(hdc, fontRegular_);
            SetTextColor(hdc, RGB(235, 235, 235));
            int strPixelW = static_cast<int>(partStr.size() * 8 + 10);
            RECT pRc{bcX, addrRc.top, bcX + strPixelW, addrRc.bottom};
            DrawTextW(hdc, partStr.c_str(), -1, &pRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            breadcrumbs_.push_back({ partStr, buildPath.wstring(), pRc });
            bcX = pRc.right;
        }
    }

    // Search Box Container
    RECT searchRc{searchX, navTop + 5, searchX + searchW, navTop + 35};
    drawRoundedRect(hdc, searchRc, 6, RGB(41, 41, 41), RGB(58, 58, 58));

    // Magnifier Icon inside search box
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(160, 160, 160));
    RECT sIconRc{searchX + 8, searchRc.top, searchX + 26, searchRc.bottom};
    DrawTextW(hdc, L"\uE721", 1, &sIconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Placeholder text if edit box is empty
    wchar_t searchBuf[64]{};
    GetWindowTextW(editSearch_, searchBuf, 64);
    if (wcslen(searchBuf) == 0) {
        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, RGB(130, 130, 130));
        RECT phRc{searchX + 32, searchRc.top, searchX + searchW - 10, searchRc.bottom};
        std::wstring phText = isHomeView_ ? L"Search Home" : (L"Search " + currentPath_.filename().wstring());
        DrawTextW(hdc, phText.c_str(), -1, &phRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

void MainWindow::paintCommandBar(HDC hdc, int width) {
    int cmdTop = tabBarHeight_ + navBarHeight_;
    int cmdBottom = cmdTop + commandBarHeight_;
    RECT cmdRc{0, cmdTop, width, cmdBottom};
    HBRUSH cmdBrush = CreateSolidBrush(RGB(31, 31, 31));
    FillRect(hdc, &cmdRc, cmdBrush);
    DeleteObject(cmdBrush);

    SetBkMode(hdc, TRANSPARENT);

    // 1. "+ New ▾" Button (Look at user image 2!)
    RECT newRc{12, cmdTop + 5, 96, cmdTop + 35};
    drawRoundedRect(hdc, newRc, 6, RGB(38, 38, 38), RGB(50, 50, 50));

    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(96, 205, 255));
    RECT newPlusRc{newRc.left + 8, newRc.top, newRc.left + 24, newRc.bottom};
    DrawTextW(hdc, L"\uE710", 1, &newPlusRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, fontRegular_);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT newTextRc{newRc.left + 26, newRc.top, newRc.right - 18, newRc.bottom};
    DrawTextW(hdc, L"New", 3, &newTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(160, 160, 160));
    RECT newChevRc{newRc.right - 18, newRc.top, newRc.right - 6, newRc.bottom};
    DrawTextW(hdc, L"\uE70D", 1, &newChevRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Hairline vertical divider 1
    HPEN divPen = CreatePen(PS_SOLID, 1, RGB(52, 52, 52));
    SelectObject(hdc, divPen);
    MoveToEx(hdc, 102, cmdTop + 8, nullptr);
    LineTo(hdc, 102, cmdTop + 32);

    // Action Buttons: Cut, Copy, Paste, Rename, Share, Delete (Image 2 Parity)
    auto drawActionBtn = [&](int x, const wchar_t* iconGlyph, COLORREF iconTint = RGB(210, 210, 210)) {
        RECT rc{x, cmdTop + 5, x + 34, cmdTop + 35};
        SelectObject(hdc, fontIcons_);
        SetTextColor(hdc, iconTint);
        DrawTextW(hdc, iconGlyph, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    drawActionBtn(106, L"\uE8C6");                     // Cut (scissors)
    drawActionBtn(144, L"\uE8C8", RGB(96, 205, 255));   // Copy (two sheets with subtle cyan accent)
    drawActionBtn(182, L"\uE77F");                     // Paste (clipboard)
    drawActionBtn(220, L"\uE8AC");                     // Rename (sheet with cursor / 'A')
    drawActionBtn(258, L"\uE72D");                     // Share (sheet with arrow out)
    drawActionBtn(296, L"\uE74D");                     // Delete (trash can)

    // Hairline vertical divider 2
    MoveToEx(hdc, 335, cmdTop + 8, nullptr);
    LineTo(hdc, 335, cmdTop + 32);

    // Dropdown Buttons: Sort ▾, View ▾, Filter ▾
    auto drawDropdownBtn = [&](int x, int w, const wchar_t* iconGlyph, const wchar_t* labelText) {
        RECT rc{x, cmdTop + 5, x + w, cmdTop + 35};
        drawRoundedRect(hdc, rc, 6, RGB(36, 36, 36), RGB(46, 46, 46));

        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, RGB(96, 205, 255));
        RECT iRc{rc.left + 8, rc.top, rc.left + 24, rc.bottom};
        DrawTextW(hdc, iconGlyph, 1, &iRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, RGB(230, 230, 230));
        RECT tRc{rc.left + 26, rc.top, rc.right - 18, rc.bottom};
        DrawTextW(hdc, labelText, -1, &tRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, RGB(160, 160, 160));
        RECT cRc{rc.right - 18, rc.top, rc.right - 6, rc.bottom};
        DrawTextW(hdc, L"\uE70D", 1, &cRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    drawDropdownBtn(342, 80, L"\uE8CB", L"Sort");
    drawDropdownBtn(428, 80, L"\uE8A9", L"View");

    std::wstring filterLabel = (filterCategory_ == FilterCategory::All) ? L"Filter" : L"Filtered";
    drawDropdownBtn(514, 90, L"\uE71C", filterLabel.c_str());

    // More options '...'
    RECT moreRc{610, cmdTop + 5, 642, cmdTop + 35};
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(180, 180, 180));
    DrawTextW(hdc, L"\uE712", 1, &moreRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Details button on far right
    RECT detRc{width - 95, cmdTop + 5, width - 15, cmdTop + 35};
    drawRoundedRect(hdc, detRc, 6, RGB(36, 36, 36), RGB(46, 46, 46));
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(96, 205, 255));
    RECT detIconRc{detRc.left + 8, detRc.top, detRc.left + 24, detRc.bottom};
    DrawTextW(hdc, L"\uE9F9", 1, &detIconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, fontRegular_);
    SetTextColor(hdc, RGB(230, 230, 230));
    RECT detTextRc{detRc.left + 26, detRc.top, detRc.right - 6, detRc.bottom};
    DrawTextW(hdc, L"Details", 7, &detTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DeleteObject(divPen);

    // Hairline bottom border
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(44, 44, 44));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, 0, cmdBottom - 1, nullptr);
    LineTo(hdc, width, cmdBottom - 1);
    DeleteObject(borderPen);
}

void MainWindow::paintFilterPills(HDC hdc, int width, int topY) {
    RECT barRc{sidebarWidth_, topY, width, topY + filterBarHeight_};
    HBRUSH bg = CreateSolidBrush(RGB(24, 24, 24));
    FillRect(hdc, &barRc, bg);
    DeleteObject(bg);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(38, 38, 38));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, sidebarWidth_, topY + filterBarHeight_ - 1, nullptr);
    LineTo(hdc, width, topY + filterBarHeight_ - 1);
    DeleteObject(borderPen);

    SetBkMode(hdc, TRANSPARENT);

    int pillX = sidebarWidth_ + 14;
    for (size_t i = 0; i < filterPills_.size(); ++i) {
        auto& pill = filterPills_[i];
        bool isActive = (pill.cat == filterCategory_);
        bool isHover = (static_cast<int>(i) == hoverPill_);

        int pillW = static_cast<int>(pill.label.size() * 8 + 38);
        pill.rect = RECT{pillX, topY + 4, pillX + pillW, topY + filterBarHeight_ - 4};

        COLORREF fillCol = isActive ? RGB(27, 50, 72) : (isHover ? RGB(45, 45, 45) : RGB(33, 33, 33));
        COLORREF borderCol = isActive ? RGB(96, 205, 255) : (isHover ? RGB(65, 65, 65) : RGB(48, 48, 48));
        drawRoundedRect(hdc, pill.rect, 12, fillCol, borderCol);

        // Icon
        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, isActive ? RGB(96, 205, 255) : (isHover ? RGB(255, 255, 255) : RGB(170, 170, 170)));
        RECT iRc{pill.rect.left + 8, pill.rect.top, pill.rect.left + 22, pill.rect.bottom};
        DrawTextW(hdc, pill.iconGlyph.c_str(), 1, &iRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Label
        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, isActive ? RGB(96, 205, 255) : (isHover ? RGB(255, 255, 255) : RGB(200, 200, 200)));
        RECT tRc{pill.rect.left + 24, pill.rect.top, pill.rect.right - 8, pill.rect.bottom};
        DrawTextW(hdc, pill.label.c_str(), -1, &tRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        pillX += pillW + 8;
    }
}

void MainWindow::paintSidebar(HDC hdc, int height) {
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    RECT sideRc{0, contentTop, sidebarWidth_, height - statusBarHeight_};
    HBRUSH bgBrush = CreateSolidBrush(RGB(24, 24, 24));
    FillRect(hdc, &sideRc, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);

    int itemY = contentTop + 8;
    for (size_t i = 0; i < sidebarItems_.size(); ++i) {
        const auto& item = sidebarItems_[i];

        if (item.isHeader) {
            RECT headerRc{14, itemY + 4, sidebarWidth_ - 14, itemY + 20};
            SelectObject(hdc, fontSmall_);
            SetTextColor(hdc, RGB(120, 120, 120));
            DrawTextW(hdc, item.name.c_str(), -1, &headerRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            itemY += 22;
            continue;
        }

        RECT rowRc{6, itemY, sidebarWidth_ - 6, itemY + 30};
        bool isSel = (static_cast<int>(i) == sidebarSelectedIndex_);
        bool isHov = (static_cast<int>(i) == sidebarHoverIndex_);

        if (isSel) {
            drawRoundedRect(hdc, rowRc, 6, RGB(37, 37, 37), RGB(48, 48, 48));
            // Left Accent Pill
            RECT pill{rowRc.left + 1, rowRc.top + 6, rowRc.left + 4, rowRc.bottom - 6};
            HBRUSH acc = CreateSolidBrush(RGB(96, 205, 255));
            FillRect(hdc, &pill, acc);
            DeleteObject(acc);
        } else if (isHov) {
            drawRoundedRect(hdc, rowRc, 6, RGB(32, 32, 32), RGB(40, 40, 40));
        }

        // Draw Vector Icon with authentic color
        SelectObject(hdc, fontIcons_);
        SetTextColor(hdc, item.iconColor);
        RECT iconRc{rowRc.left + 10, rowRc.top, rowRc.left + 30, rowRc.bottom};
        DrawTextW(hdc, item.iconGlyph.c_str(), 1, &iconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Draw Name
        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, isSel ? RGB(255, 255, 255) : (isHov ? RGB(245, 245, 245) : RGB(215, 215, 215)));
        RECT textRc{rowRc.left + 36, rowRc.top, rowRc.right - 10, rowRc.bottom};
        DrawTextW(hdc, item.name.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        itemY += 30;
    }

    // Right hairline border for Sidebar
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(40, 40, 40));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, sidebarWidth_ - 1, contentTop, nullptr);
    LineTo(hdc, sidebarWidth_ - 1, height - statusBarHeight_);
    DeleteObject(borderPen);
}

void MainWindow::paintHomeView(HDC hdc, int width, int height) {
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    int surfaceLeft = sidebarWidth_;

    SetBkMode(hdc, TRANSPARENT);

    // Section 1: "v Quick access" Title (Look at user image 1!)
    RECT titleChevRc{surfaceLeft + 16, contentTop + 14, surfaceLeft + 30, contentTop + 36};
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(180, 180, 180));
    DrawTextW(hdc, L"\uE70D", 1, &titleChevRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT titleRc{surfaceLeft + 34, contentTop + 14, surfaceLeft + 250, contentTop + 36};
    SelectObject(hdc, fontTitle_);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, L"Quick access", -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Render Quick Cards (Grid of cards matching user image 1)
    int cardW = 190;
    int cardH = 54;
    int cardStartX = surfaceLeft + 16;
    int cardStartY = contentTop + 44;

    for (size_t i = 0; i < quickCards_.size(); ++i) {
        int col = static_cast<int>(i % 4);
        int row = static_cast<int>(i / 4);
        int cx = cardStartX + col * (cardW + 14);
        int cy = cardStartY + row * (cardH + 10);
        if (cx + cardW > width - 10) continue;

        quickCards_[i].rect = RECT{cx, cy, cx + cardW, cy + cardH};

        bool isHov = (static_cast<int>(i) == hoverCard_);
        COLORREF cardBg = isHov ? RGB(42, 42, 42) : RGB(33, 33, 33);
        COLORREF cardBorder = isHov ? RGB(60, 60, 60) : RGB(46, 46, 46);
        drawRoundedRect(hdc, quickCards_[i].rect, 8, cardBg, cardBorder);

        // Icon Badge Container
        RECT badgeRc{cx + 8, cy + 9, cx + 44, cy + 45};
        drawRoundedRect(hdc, badgeRc, 6, RGB(38, 38, 38), RGB(48, 48, 48));

        // Vibrant Icon Glyph
        SelectObject(hdc, fontIcons_);
        SetTextColor(hdc, quickCards_[i].badgeColor);
        DrawTextW(hdc, quickCards_[i].iconGlyph.c_str(), 1, &badgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Name
        SelectObject(hdc, fontBold_);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT nameRc{cx + 50, cy + 9, cx + cardW - 24, cy + 27};
        DrawTextW(hdc, quickCards_[i].name.c_str(), -1, &nameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Subtext (e.g. "Stored locally")
        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(140, 140, 140));
        RECT subRc{cx + 50, cy + 28, cx + cardW - 24, cy + 45};
        DrawTextW(hdc, quickCards_[i].subtext.c_str(), -1, &subRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Small Pin Icon at bottom-right
        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, RGB(110, 110, 110));
        RECT pinRc{cx + cardW - 22, cy + 28, cx + cardW - 6, cy + 45};
        DrawTextW(hdc, L"\uE718", 1, &pinRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Section 2: Section Tabs (Recent, Favorites, Shared - matching user image 1)
    int recentTop = contentTop + 172;
    RECT recentChevRc{surfaceLeft + 16, recentTop + 2, surfaceLeft + 30, recentTop + 26};
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(180, 180, 180));
    DrawTextW(hdc, L"\uE70D", 1, &recentChevRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    int pillX = surfaceLeft + 36;
    auto drawSectionPill = [&](RECT& outRc, const wchar_t* iconGlyph, const wchar_t* text, bool isSelected) {
        int w = static_cast<int>(wcslen(text) * 8 + 36);
        outRc = RECT{pillX, recentTop, pillX + w, recentTop + 26};

        COLORREF fillCol = isSelected ? RGB(45, 35, 52) : RGB(28, 28, 28);
        COLORREF borderCol = isSelected ? RGB(227, 0, 140) : RGB(48, 48, 48);
        drawRoundedRect(hdc, outRc, 13, fillCol, borderCol);

        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, isSelected ? RGB(255, 255, 255) : RGB(160, 160, 160));
        RECT iRc{outRc.left + 8, outRc.top, outRc.left + 22, outRc.bottom};
        DrawTextW(hdc, iconGlyph, 1, &iRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, isSelected ? RGB(255, 255, 255) : RGB(180, 180, 180));
        RECT tRc{outRc.left + 24, outRc.top, outRc.right - 8, outRc.bottom};
        DrawTextW(hdc, text, -1, &tRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        pillX += w + 8;
    };

    drawSectionPill(pillRecentRc_, L"\uE823", L"Recent", homeSection_ == 0);
    drawSectionPill(pillFavRc_, L"\uE734", L"Favorites", homeSection_ == 1);
    drawSectionPill(pillSharedRc_, L"\uE716", L"Shared", homeSection_ == 2);

    // Section 3: Recent Items Table Headers
    int tableHeaderTop = recentTop + 36;
    RECT hdrRc{surfaceLeft, tableHeaderTop, width, tableHeaderTop + headerHeight_};
    HBRUSH hdrBrush = CreateSolidBrush(RGB(26, 26, 26));
    FillRect(hdc, &hdrRc, hdrBrush);
    DeleteObject(hdrBrush);

    SelectObject(hdc, fontSmall_);
    SetTextColor(hdc, RGB(130, 130, 130));

    RECT colNameRc{surfaceLeft + 36, tableHeaderTop, surfaceLeft + 36 + colNameWidth_, tableHeaderTop + headerHeight_};
    RECT colDateRc{colNameRc.right, tableHeaderTop, colNameRc.right + colDateWidth_, tableHeaderTop + headerHeight_};
    RECT colLocRc{colDateRc.right, tableHeaderTop, width - 20, tableHeaderTop + headerHeight_};

    DrawTextW(hdc, L"NAME", 4, &colNameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, L"DATE ACCESSED", 13, &colDateRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, L"FILE LOCATION", 13, &colLocRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(40, 40, 40));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, surfaceLeft, tableHeaderTop + headerHeight_ - 1, nullptr);
    LineTo(hdc, width, tableHeaderTop + headerHeight_ - 1);
    DeleteObject(borderPen);

    // Rows
    int rowY = tableHeaderTop + headerHeight_;
    int surfaceBottom = height - statusBarHeight_;
    int visibleRows = (surfaceBottom - rowY) / rowHeight_ + 2;

    SelectObject(hdc, fontRegular_);
    for (int i = 0; i < visibleRows; ++i) {
        int r = scrollOffset_ + i;
        if (r >= static_cast<int>(filteredIndices_.size())) break;

        const auto& item = allItems_[filteredIndices_[r]];
        RECT rowRc{surfaceLeft + 4, rowY + i * rowHeight_, width - 8, rowY + (i + 1) * rowHeight_};

        bool isSel = (r == selectedRow_);
        bool isHov = (r == hoverRow_);

        if (isSel) {
            drawRoundedRect(hdc, rowRc, 6, RGB(30, 51, 71), RGB(40, 68, 95));
            // Left Accent Pill
            RECT pill{rowRc.left + 1, rowRc.top + 4, rowRc.left + 4, rowRc.bottom - 4};
            HBRUSH acc = CreateSolidBrush(RGB(96, 205, 255));
            FillRect(hdc, &pill, acc);
            DeleteObject(acc);
        } else if (isHov) {
            drawRoundedRect(hdc, rowRc, 6, RGB(36, 36, 36), RGB(44, 44, 44));
        }

        // Real Shell Icon
        HICON hIcon = getFileIcon(item.name, item.isDirectory, item.extension);
        if (hIcon) {
            DrawIconEx(hdc, surfaceLeft + 12, rowRc.top + 7, hIcon, 16, 16, 0, nullptr, DI_NORMAL);
            DestroyIcon(hIcon);
        }

        // File Name
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT nameCell{surfaceLeft + 36, rowRc.top, surfaceLeft + 36 + colNameWidth_ - 10, rowRc.bottom};
        DrawTextW(hdc, item.name.c_str(), -1, &nameCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Date Accessed
        SetTextColor(hdc, RGB(150, 150, 150));
        RECT dateCell{colNameRc.right, rowRc.top, colNameRc.right + colDateWidth_, rowRc.bottom};
        std::wstring dt = formatTime(item.mtime);
        DrawTextW(hdc, dt.c_str(), -1, &dateCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // File Location
        RECT locCell{colDateRc.right, rowRc.top, width - 20, rowRc.bottom};
        DrawTextW(hdc, item.location.c_str(), -1, &locCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
}

void MainWindow::paintDirectoryView(HDC hdc, int width, int height) {
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    int surfaceLeft = sidebarWidth_;

    SetBkMode(hdc, TRANSPARENT);

    // 1. Instant 1-Click Quick Filter Bar
    paintFilterPills(hdc, width, contentTop);

    // 2. Interactive Column Headers (with Sort Arrows)
    int tableHeaderTop = contentTop + filterBarHeight_;
    RECT hdrRc{surfaceLeft, tableHeaderTop, width, tableHeaderTop + headerHeight_};
    HBRUSH hdrBrush = CreateSolidBrush(RGB(26, 26, 26));
    FillRect(hdc, &hdrRc, hdrBrush);
    DeleteObject(hdrBrush);

    SelectObject(hdc, fontSmall_);
    SetTextColor(hdc, RGB(140, 140, 140));

    RECT colNameRc{surfaceLeft + 36, tableHeaderTop, surfaceLeft + 36 + colNameWidth_, tableHeaderTop + headerHeight_};
    RECT colDateRc{colNameRc.right, tableHeaderTop, colNameRc.right + colDateWidth_, tableHeaderTop + headerHeight_};
    RECT colTypeRc{colDateRc.right, tableHeaderTop, colDateRc.right + colTypeWidth_, tableHeaderTop + headerHeight_};
    RECT colSizeRc{colTypeRc.right, tableHeaderTop, colTypeRc.right + colSizeWidth_, tableHeaderTop + headerHeight_};

    std::wstring nameHdr = L"NAME" + std::wstring(sortCol_ == SortColumn::Name ? (sortAscending_ ? L" ▲" : L" ▼") : L"");
    std::wstring dateHdr = L"DATE MODIFIED" + std::wstring(sortCol_ == SortColumn::DateModified ? (sortAscending_ ? L" ▲" : L" ▼") : L"");
    std::wstring typeHdr = L"TYPE" + std::wstring(sortCol_ == SortColumn::Type ? (sortAscending_ ? L" ▲" : L" ▼") : L"");
    std::wstring sizeHdr = L"SIZE" + std::wstring(sortCol_ == SortColumn::Size ? (sortAscending_ ? L" ▲" : L" ▼") : L"");

    DrawTextW(hdc, nameHdr.c_str(), -1, &colNameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, dateHdr.c_str(), -1, &colDateRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, typeHdr.c_str(), -1, &colTypeRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, sizeHdr.c_str(), -1, &colSizeRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(40, 40, 40));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, surfaceLeft, tableHeaderTop + headerHeight_ - 1, nullptr);
    LineTo(hdc, width, tableHeaderTop + headerHeight_ - 1);
    DeleteObject(borderPen);

    // 3. File Rows
    int rowY = tableHeaderTop + headerHeight_;
    int surfaceBottom = height - statusBarHeight_;
    int visibleRows = (surfaceBottom - rowY) / rowHeight_ + 2;

    SelectObject(hdc, fontRegular_);
    for (int i = 0; i < visibleRows; ++i) {
        int r = scrollOffset_ + i;
        if (r >= static_cast<int>(filteredIndices_.size())) break;

        const auto& item = allItems_[filteredIndices_[r]];
        RECT rowRc{surfaceLeft + 4, rowY + i * rowHeight_, width - 8, rowY + (i + 1) * rowHeight_};

        bool isSel = (r == selectedRow_);
        bool isHov = (r == hoverRow_);

        if (isSel) {
            drawRoundedRect(hdc, rowRc, 6, RGB(30, 51, 71), RGB(40, 68, 95));
            // Left Accent Pill
            RECT pill{rowRc.left + 1, rowRc.top + 4, rowRc.left + 4, rowRc.bottom - 4};
            HBRUSH acc = CreateSolidBrush(RGB(96, 205, 255));
            FillRect(hdc, &pill, acc);
            DeleteObject(acc);
        } else if (isHov) {
            drawRoundedRect(hdc, rowRc, 6, RGB(36, 36, 36), RGB(44, 44, 44));
        }

        // Real Windows Shell File Icon
        HICON hIcon = getFileIcon(item.name, item.isDirectory, item.extension);
        if (hIcon) {
            DrawIconEx(hdc, surfaceLeft + 12, rowRc.top + 7, hIcon, 16, 16, 0, nullptr, DI_NORMAL);
            DestroyIcon(hIcon);
        }

        // File Name
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT nameCell{surfaceLeft + 36, rowRc.top, surfaceLeft + 36 + colNameWidth_ - 10, rowRc.bottom};
        DrawTextW(hdc, item.name.c_str(), -1, &nameCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Date Modified
        SetTextColor(hdc, RGB(150, 150, 150));
        RECT dateCell{colNameRc.right, rowRc.top, colNameRc.right + colDateWidth_, rowRc.bottom};
        std::wstring dt = formatTime(item.mtime);
        DrawTextW(hdc, dt.c_str(), -1, &dateCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // File Type
        RECT typeCell{colDateRc.right, rowRc.top, colDateRc.right + colTypeWidth_, rowRc.bottom};
        std::wstring typeStr = item.isDirectory ? L"File folder" : (item.extension.empty() ? L"File" : item.extension + L" File");
        DrawTextW(hdc, typeStr.c_str(), -1, &typeCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // File Size
        RECT sizeCell{colTypeRc.right, rowRc.top, colTypeRc.right + colSizeWidth_, rowRc.bottom};
        std::wstring sz = item.isDirectory ? L"" : formatSize(item.size);
        DrawTextW(hdc, sz.c_str(), -1, &sizeCell, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
}

void MainWindow::paintStatusBar(HDC hdc, int width, int height) {
    RECT statusRc{0, height - statusBarHeight_, width, height};
    HBRUSH bg = CreateSolidBrush(RGB(24, 24, 24));
    FillRect(hdc, &statusRc, bg);
    DeleteObject(bg);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(38, 38, 38));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, 0, height - statusBarHeight_, nullptr);
    LineTo(hdc, width, height - statusBarHeight_);
    DeleteObject(borderPen);

    SelectObject(hdc, fontSmall_);
    SetTextColor(hdc, RGB(170, 170, 170));
    SetBkMode(hdc, TRANSPARENT);

    wchar_t statusBuf[256];
    if (selectedRow_ >= 0 && selectedRow_ < static_cast<int>(filteredIndices_.size())) {
        const auto& item = allItems_[filteredIndices_[selectedRow_]];
        swprintf_s(statusBuf, L"%zu items  |  1 item selected (%s)  |  GodXplr Local Engine",
            filteredIndices_.size(), formatSize(item.size).c_str());
    } else {
        swprintf_s(statusBuf, L"%zu items  |  GodXplr High-Performance Local Engine (Zero AI / Zero Cloud)",
            filteredIndices_.size());
    }

    RECT textRc{14, height - statusBarHeight_, width - 100, height};
    DrawTextW(hdc, statusBuf, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void MainWindow::onPaint(HDC hdc) {
    RECT clientRc;
    GetClientRect(hwnd_, &clientRc);
    int width = clientRc.right;
    int height = clientRc.bottom;

    // Double-buffered rendering to prevent any flicker
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    // Deep Dark Canvas Background (RGB 22, 22, 22)
    HBRUSH bgBrush = CreateSolidBrush(RGB(22, 22, 22));
    FillRect(memDC, &clientRc, bgBrush);
    DeleteObject(bgBrush);

    // 1. Tab Bar
    paintTabBar(memDC, width);

    // 2. Navigation & Address Bar
    paintNavAndAddressBar(memDC, width);

    // 3. Command Bar / Ribbon (Windows 11 Parity)
    paintCommandBar(memDC, width);

    // 4. Sidebar (Favorites, Quick Access, Drives)
    paintSidebar(memDC, height);

    // 5. Main Surface (Home View vs Directory View)
    if (isHomeView_) {
        paintHomeView(memDC, width, height);
    } else {
        paintDirectoryView(memDC, width, height);
    }

    // 6. Status Bar
    paintStatusBar(memDC, width, height);

    // Blit to screen DC
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

} // namespace gx::app
