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
    int initW = 1240;
    int initH = 800;
    int initX = (screenW - initW) / 2;
    int initY = (screenH - initH) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        kClassName,
        L"GodXplr",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VSCROLL,
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

    // Default tab
    tabs_.push_back({ L"Home", L"godxplr://home" });
}

MainWindow::~MainWindow() {
    if (fontRegular_) DeleteObject(fontRegular_);
    if (fontBold_) DeleteObject(fontBold_);
    if (fontTitle_) DeleteObject(fontTitle_);
    if (fontSmall_) DeleteObject(fontSmall_);
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

void MainWindow::initSidebar() {
    sidebarItems_.clear();

    // Section 1: Quick access / Pinned
    sidebarItems_.push_back({ L"Home", L"godxplr://home", L"🏠", false, false });
    sidebarItems_.push_back({ L"Gallery", pathPictures_, L"🖼", false, false });
    sidebarItems_.push_back({ L"Desktop", pathDesktop_, L"💻", false, false });
    sidebarItems_.push_back({ L"Downloads", pathDownloads_, L"⬇", false, false });
    sidebarItems_.push_back({ L"Documents", pathDocuments_, L"📁", false, false });
    sidebarItems_.push_back({ L"Pictures", pathPictures_, L"🌄", false, false });
    sidebarItems_.push_back({ L"Music", pathMusic_, L"🎵", false, false });
    sidebarItems_.push_back({ L"Videos", pathVideos_, L"🎬", false, false });
    sidebarItems_.push_back({ L"os fileX", L"C:\\os fileX", L"📂", false, false });

    // Section 2: This PC / Drives
    sidebarItems_.push_back({ L"This PC", L"C:\\", L"💻", false, false });

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

            sidebarItems_.push_back({ label, root, L"💾", true, false });
            ptr += wcslen(ptr) + 1;
        }
    }

    sidebarItems_.push_back({ L"Network", L"godxplr://network", L"🌐", false, false });
    sidebarItems_.push_back({ L"Recycle Bin", L"shell:RecycleBinFolder", L"🗑", false, false });

    // Quick Access cards for Home View
    quickCards_ = {
        { L"Desktop", L"Stored locally", pathDesktop_, L"💻", {} },
        { L"Downloads", L"Stored locally", pathDownloads_, L"⬇", {} },
        { L"Documents", L"Stored locally", pathDocuments_, L"📁", {} },
        { L"Pictures", L"Stored locally", pathPictures_, L"🌄", {} },
        { L"Music", L"Stored locally", pathMusic_, L"🎵", {} },
        { L"Videos", L"Stored locally", pathVideos_, L"🎬", {} },
        { L"os fileX", L"Windows-SSD (C:)", L"C:\\os fileX", L"📂", {} },
        { L"Windows-SSD (C:)", L"System Drive", L"C:\\", L"💾", {} }
    };
}

void MainWindow::show(int nCmdShow) {
    ShowWindow(hwnd_, nCmdShow);
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
        // Address Edit Control
        editAddress_ = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | ES_AUTOHSCROLL,
            160, 42, 500, 28,
            hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdAddressEdit)), hInstance_, nullptr);
        SendMessageW(editAddress_, WM_SETFONT, (WPARAM)fontRegular_, TRUE);

        // Search Edit Control
        editSearch_ = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            680, 42, 260, 28,
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
            InvalidateRect(hwnd_, nullptr, FALSE);
        }

        // Context / Menu actions
        switch (id) {
        case kCmdNewFolder: createNewFolder(); break;
        case kCmdNewFile: createNewTextFile(); break;
        case kCmdSortName: setSortColumn(SortColumn::Name); break;
        case kCmdSortDate: setSortColumn(SortColumn::DateModified); break;
        case kCmdSortType: setSortColumn(SortColumn::Type); break;
        case kCmdSortSize: setSortColumn(SortColumn::Size); break;
        case kCmdSortAsc: sortAscending_ = true; filterAndSortItems(); break;
        case kCmdSortDesc: sortAscending_ = false; filterAndSortItems(); break;
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

    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        onResize(w, h);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd_, &ps);
        onPaint(hdc);
        EndPaint(hwnd_, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // Double buffered, avoid flicker

    case WM_VSCROLL:
        onVScroll(wParam);
        return 0;

    case WM_MOUSEWHEEL:
        onMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;

    case WM_MOUSEMOVE: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        onMouseMove(x, y);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        onLButtonDown(x, y, false);
        return 0;
    }

    case WM_LBUTTONDBLCLK: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        onLButtonDown(x, y, true);
        return 0;
    }

    case WM_RBUTTONUP: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        onRButtonUp(x, y);
        return 0;
    }

    case WM_KEYDOWN:
        onKeyDown(wParam);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd_, uMsg, wParam, lParam);
}

void MainWindow::onResize(int width, int height) {
    (void)height;
    if (editAddress_ && editSearch_) {
        int navY = tabBarHeight_ + 7;
        int searchWidth = 260;
        int searchX = width - searchWidth - 15;
        int addrX = 145;
        int addrWidth = std::max(120, searchX - addrX - 15);

        SetWindowPos(editAddress_, nullptr, addrX, navY, addrWidth, 26, SWP_NOZORDER);
        SetWindowPos(editSearch_, nullptr, searchX, navY, searchWidth, 26, SWP_NOZORDER);
    }
    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::onVScroll(WPARAM wParam) {
    int code = LOWORD(wParam);
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwnd_, SB_VERT, &si);

    int newPos = si.nPos;
    switch (code) {
    case SB_LINEUP: newPos -= 1; break;
    case SB_LINEDOWN: newPos += 1; break;
    case SB_PAGEUP: newPos -= si.nPage; break;
    case SB_PAGEDOWN: newPos += si.nPage; break;
    case SB_THUMBTRACK: newPos = si.nTrackPos; break;
    }

    newPos = std::clamp(newPos, 0, std::max(0, si.nMax - static_cast<int>(si.nPage) + 1));
    if (newPos != si.nPos) {
        scrollOffset_ = newPos;
        SetScrollPos(hwnd_, SB_VERT, newPos, TRUE);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void MainWindow::onMouseWheel(short delta) {
    int rows = -delta / WHEEL_DELTA * 3;
    int maxScroll = std::max(0, static_cast<int>(filteredIndices_.size()) - 10);
    int newScroll = std::clamp(scrollOffset_ + rows, 0, maxScroll);
    if (newScroll != scrollOffset_) {
        scrollOffset_ = newScroll;
        SetScrollPos(hwnd_, SB_VERT, scrollOffset_, TRUE);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void MainWindow::onMouseMove(int x, int y) {
    int oldSidebar = sidebarHoverIndex_;
    int oldHoverRow = hoverRow_;
    int oldCard = hoverCard_;

    sidebarHoverIndex_ = -1;
    hoverRow_ = -1;
    hoverCard_ = -1;

    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;

    // Check Sidebar hover
    if (x >= 0 && x < sidebarWidth_ && y >= contentTop) {
        int sideY = y - contentTop - 10;
        if (sideY >= 0) {
            int idx = sideY / 32;
            if (idx >= 0 && idx < static_cast<int>(sidebarItems_.size())) {
                sidebarHoverIndex_ = idx;
            }
        }
    }

    // Check Main surface hover
    if (x >= sidebarWidth_ && y >= contentTop) {
        if (isHomeView_) {
            // Check Quick cards hover
            for (size_t i = 0; i < quickCards_.size(); ++i) {
                if (PtInRect(&quickCards_[i].rect, POINT{x, y})) {
                    hoverCard_ = static_cast<int>(i);
                    break;
                }
            }

            int tableTop = contentTop + 180 + 32 + headerHeight_;
            if (y >= tableTop) {
                int r = scrollOffset_ + ((y - tableTop) / rowHeight_);
                if (r >= 0 && r < static_cast<int>(filteredIndices_.size())) {
                    hoverRow_ = r;
                }
            }
        } else {
            int tableTop = contentTop + headerHeight_;
            if (y >= tableTop) {
                int r = scrollOffset_ + ((y - tableTop) / rowHeight_);
                if (r >= 0 && r < static_cast<int>(filteredIndices_.size())) {
                    hoverRow_ = r;
                }
            }
        }
    }

    if (sidebarHoverIndex_ != oldSidebar || hoverRow_ != oldHoverRow || hoverCard_ != oldCard) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void MainWindow::onLButtonDown(int x, int y, bool isDoubleClick) {
    // 1. Tab Bar Clicks (y in 0 .. tabBarHeight_)
    if (y < tabBarHeight_) {
        int tabX = 10;
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
            tabX += 165;
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
        if (x >= 10 && x < 38) navigateBack();
        else if (x >= 42 && x < 70) navigateForward();
        else if (x >= 74 && x < 102) navigateUp();
        else if (x >= 106 && x < 134) refresh();
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
            int searchX = clientRc.right - 275;
            if (x >= 140 && x < searchX) {
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
        if (x >= 10 && x < 90) {
            showNewMenu(x, cmdBottom);
        } else if (x >= 95 && x < 130) {
            cutSelectedItem();
        } else if (x >= 135 && x < 170) {
            copySelectedItem();
        } else if (x >= 175 && x < 210) {
            pasteItem();
        } else if (x >= 215 && x < 255) {
            renameSelectedItem();
        } else if (x >= 260 && x < 300) {
            deleteSelectedItem();
        } else if (x >= 310 && x < 380) {
            showSortMenu(x, cmdBottom);
        } else if (x >= 385 && x < 455) {
            showViewMenu(x, cmdBottom);
        } else if (x >= 460 && x < 540) {
            showFilterMenu(x, cmdBottom);
        }
        return;
    }

    // 4. Sidebar Clicks (x in 0 .. sidebarWidth_)
    int contentTop = cmdBottom;
    if (x < sidebarWidth_ && y >= contentTop) {
        int sideY = y - contentTop - 10;
        if (sideY >= 0) {
            int idx = sideY / 32;
            if (idx >= 0 && idx < static_cast<int>(sidebarItems_.size())) {
                sidebarSelectedIndex_ = idx;
                const auto& item = sidebarItems_[idx];
                if (item.path == L"godxplr://home") {
                    navigateToHome();
                } else if (item.path == L"shell:RecycleBinFolder") {
                    ShellExecuteW(hwnd_, L"open", L"shell:RecycleBinFolder", nullptr, nullptr, SW_SHOWNORMAL);
                } else if (std::filesystem::exists(item.path)) {
                    navigateTo(std::filesystem::path(item.path));
                }
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
        }
        return;
    }

    // 5. Column Header Click (Sorting!)
    if (x >= sidebarWidth_ && !isHomeView_) {
        int hdrTop = contentTop;
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

    // 6. Home View Quick Cards Click
    if (x >= sidebarWidth_ && isHomeView_) {
        for (const auto& card : quickCards_) {
            if (PtInRect(&card.rect, POINT{x, y})) {
                if (std::filesystem::exists(card.path)) {
                    navigateTo(std::filesystem::path(card.path));
                }
                return;
            }
        }
    }

    // 7. File List Click & Double-Click (File Opening!)
    int listTop = contentTop + (isHomeView_ ? 180 + 32 + headerHeight_ : headerHeight_);
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
    int listTop = contentTop + (isHomeView_ ? 180 + 32 + headerHeight_ : headerHeight_);

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
            int surfaceHeight = clientRc.bottom - (tabBarHeight_ + navBarHeight_ + commandBarHeight_ + headerHeight_ + statusBarHeight_);
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

    if (!isHomeView_ && currentPath_ != path) {
        backStack_.push_back(currentPath_);
        forwardStack_.clear();
    }

    isHomeView_ = false;
    currentPath_ = std::filesystem::canonical(path, ec);
    if (ec) currentPath_ = path;

    // Update active tab title
    std::wstring folderName = currentPath_.filename().wstring();
    if (folderName.empty()) folderName = currentPath_.wstring();
    if (activeTab_ < tabs_.size()) {
        tabs_[activeTab_].title = folderName;
        tabs_[activeTab_].path = currentPath_.wstring();
    }

    SetWindowTextW(editAddress_, currentPath_.c_str());
    SetWindowTextW(editSearch_, L"");

    refresh();
}

void MainWindow::navigateToHome() {
    isHomeView_ = true;
    currentPath_ = L"godxplr://home";

    if (activeTab_ < tabs_.size()) {
        tabs_[activeTab_].title = L"Home";
        tabs_[activeTab_].path = L"godxplr://home";
    }

    SetWindowTextW(editAddress_, L"Home");
    SetWindowTextW(editSearch_, L"");

    refresh();
}

void MainWindow::navigateUp() {
    if (isHomeView_) return;
    if (currentPath_.has_parent_path() && currentPath_ != currentPath_.parent_path()) {
        navigateTo(currentPath_.parent_path());
    } else {
        navigateToHome();
    }
}

void MainWindow::navigateBack() {
    if (!backStack_.empty()) {
        auto prev = backStack_.back();
        backStack_.pop_back();
        forwardStack_.push_back(currentPath_);
        navigateTo(prev);
    } else if (!isHomeView_) {
        navigateToHome();
    }
}

void MainWindow::navigateForward() {
    if (!forwardStack_.empty()) {
        auto next = forwardStack_.back();
        forwardStack_.pop_back();
        backStack_.push_back(currentPath_);
        navigateTo(next);
    }
}

void MainWindow::refresh() {
    allItems_.clear();
    filteredIndices_.clear();

    if (isHomeView_) {
        // Populate recent files from Downloads, Documents, Desktop
        std::vector<std::wstring> scanRoots = { pathDownloads_, pathDocuments_, pathDesktop_ };
        for (const auto& root : scanRoots) {
            if (root.empty() || !std::filesystem::exists(root)) continue;

            WIN32_FIND_DATAW fd;
            std::wstring pat = root + L"\\*";
            HANDLE hFind = FindFirstFileExW(pat.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
            if (hFind != INVALID_HANDLE_VALUE) {
                int count = 0;
                do {
                    if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                    FileItemDisplay item;
                    item.name = fd.cFileName;
                    item.location = root;
                    item.attributes = fd.dwFileAttributes;
                    item.isDirectory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                    item.size = (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
                    item.mtime = (static_cast<uint64_t>(fd.ftLastWriteTime.dwHighDateTime) << 32) | fd.ftLastWriteTime.dwLowDateTime;

                    size_t dotPos = item.name.rfind(L'.');
                    if (dotPos != std::wstring::npos && !item.isDirectory) {
                        item.extension = item.name.substr(dotPos + 1);
                    }
                    allItems_.push_back(std::move(item));
                    if (++count > 15) break; // Limit recent items per folder
                } while (FindNextFileW(hFind, &fd));
                FindClose(hFind);
            }
        }
    } else {
        std::wstring searchPattern = (currentPath_ / L"*").wstring();
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileExW(
            searchPattern.c_str(),
            FindExInfoBasic,
            &fd,
            FindExSearchNameMatch,
            nullptr,
            FIND_FIRST_EX_LARGE_FETCH);

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

                FileItemDisplay item;
                item.name = fd.cFileName;
                item.location = currentPath_.wstring();
                item.attributes = fd.dwFileAttributes;
                item.isDirectory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                item.size = (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
                item.mtime = (static_cast<uint64_t>(fd.ftLastWriteTime.dwHighDateTime) << 32) | fd.ftLastWriteTime.dwLowDateTime;

                size_t dotPos = item.name.rfind(L'.');
                if (dotPos != std::wstring::npos && !item.isDirectory) {
                    item.extension = item.name.substr(dotPos + 1);
                }

                allItems_.push_back(std::move(item));
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }

    filterAndSortItems();
    selectedRow_ = filteredIndices_.empty() ? -1 : 0;
    scrollOffset_ = 0;
    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
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
            df->pFiles = sizeof(DROPFILES);
            df->fWide = TRUE;
            wchar_t* target = reinterpret_cast<wchar_t*>(reinterpret_cast<char*>(df) + sizeof(DROPFILES));
            memcpy(target, p.c_str(), p.size() * sizeof(wchar_t));
            target[p.size()] = L'\0';
            target[p.size() + 1] = L'\0';
            GlobalUnlock(hGlobal);
            SetClipboardData(CF_HDROP, hGlobal);
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
    // Basic prompt or status notice
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
    int surfaceHeight = rc.bottom - (contentTop + (isHomeView_ ? 180 + 32 + headerHeight_ : headerHeight_) + statusBarHeight_);
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
// Rendering Subroutines (Pure Windows Dark Theme, Zero AI Slop)
// -----------------------------------------------------------------------------

void MainWindow::paintTabBar(HDC hdc, int width) {
    RECT tabAreaRc{0, 0, width, tabBarHeight_};
    HBRUSH bgBrush = CreateSolidBrush(RGB(25, 25, 25));
    FillRect(hdc, &tabAreaRc, bgBrush);
    DeleteObject(bgBrush);

    SelectObject(hdc, fontRegular_);
    SetBkMode(hdc, TRANSPARENT);

    int tabX = 10;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        bool isActive = (i == activeTab_);
        RECT tabRc{tabX, 6, tabX + 160, tabBarHeight_};

        HBRUSH tBrush = CreateSolidBrush(isActive ? RGB(45, 45, 45) : RGB(30, 30, 30));
        FillRect(hdc, &tabRc, tBrush);
        DeleteObject(tBrush);

        // Active top indicator line
        if (isActive) {
            RECT indRc{tabRc.left, 6, tabRc.right, 8};
            HBRUSH acc = CreateSolidBrush(toColorRef(tokens_.accent));
            FillRect(hdc, &indRc, acc);
            DeleteObject(acc);
        }

        SetTextColor(hdc, isActive ? RGB(255, 255, 255) : RGB(170, 170, 170));
        RECT textRc{tabRc.left + 12, tabRc.top, tabRc.right - 25, tabRc.bottom};
        DrawTextW(hdc, tabs_[i].title.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Close 'x'
        RECT closeRc{tabRc.right - 22, tabRc.top, tabRc.right - 6, tabRc.bottom};
        DrawTextW(hdc, L"×", 1, &closeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        tabX += 165;
    }

    // New Tab (+) Button
    RECT plusRc{tabX, 6, tabX + 28, tabBarHeight_};
    HBRUSH plusBg = CreateSolidBrush(RGB(32, 32, 32));
    FillRect(hdc, &plusRc, plusBg);
    DeleteObject(plusBg);

    SetTextColor(hdc, RGB(200, 200, 200));
    DrawTextW(hdc, L"+", 1, &plusRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void MainWindow::paintNavAndAddressBar(HDC hdc, int width) {
    int navTop = tabBarHeight_;
    RECT navRc{0, navTop, width, navTop + navBarHeight_};
    HBRUSH navBrush = CreateSolidBrush(RGB(32, 32, 32));
    FillRect(hdc, &navRc, navBrush);
    DeleteObject(navBrush);

    SelectObject(hdc, fontBold_);
    SetBkMode(hdc, TRANSPARENT);

    // Nav Buttons: Back, Forward, Up, Refresh
    auto drawBtn = [&](int x, const wchar_t* glyph) {
        RECT rc{x, navTop + 6, x + 28, navTop + 34};
        HBRUSH btnBg = CreateSolidBrush(RGB(42, 42, 42));
        FillRect(hdc, &rc, btnBg);
        DeleteObject(btnBg);
        SetTextColor(hdc, RGB(220, 220, 220));
        DrawTextW(hdc, glyph, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    drawBtn(10, L"←");
    drawBtn(42, L"→");
    drawBtn(74, L"↑");
    drawBtn(106, L"↻");

    // Address / Breadcrumb Bar
    int searchW = 260;
    int searchX = width - searchW - 15;
    int addrX = 145;
    int addrW = searchX - addrX - 15;
    RECT addrRc{addrX, navTop + 6, addrX + addrW, navTop + 34};

    HBRUSH addrBg = CreateSolidBrush(RGB(40, 40, 40));
    FillRect(hdc, &addrRc, addrBg);
    DeleteObject(addrBg);

    // Breadcrumbs inside Address Bar
    breadcrumbs_.clear();
    int bcX = addrX + 10;
    SelectObject(hdc, fontRegular_);
    SetTextColor(hdc, RGB(220, 220, 220));

    if (isHomeView_) {
        RECT bcRc{bcX, addrRc.top, bcX + 60, addrRc.bottom};
        DrawTextW(hdc, L"🏠 Home", -1, &bcRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        breadcrumbs_.push_back({ L"Home", L"godxplr://home", bcRc });
    } else {
        RECT homeRc{bcX, addrRc.top, bcX + 22, addrRc.bottom};
        DrawTextW(hdc, L"🏠", 1, &homeRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        breadcrumbs_.push_back({ L"Home", L"godxplr://home", homeRc });
        bcX += 26;

        std::filesystem::path buildPath;
        for (const auto& part : currentPath_) {
            buildPath /= part;
            std::wstring partStr = part.wstring();
            if (partStr.back() == L'\\') partStr.pop_back();

            RECT chevRc{bcX, addrRc.top, bcX + 14, addrRc.bottom};
            SetTextColor(hdc, RGB(140, 140, 140));
            DrawTextW(hdc, L">", 1, &chevRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            bcX += 16;

            RECT pRc{bcX, addrRc.top, bcX + static_cast<int>(partStr.size() * 8 + 8), addrRc.bottom};
            SetTextColor(hdc, RGB(230, 230, 230));
            DrawTextW(hdc, partStr.c_str(), -1, &pRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            breadcrumbs_.push_back({ partStr, buildPath.wstring(), pRc });
            bcX = pRc.right;
        }
    }

    // Search Box Placeholder icon
    SetTextColor(hdc, RGB(160, 160, 160));
    RECT searchIconRc{searchX - 25, addrRc.top, searchX, addrRc.bottom};
    DrawTextW(hdc, L"🔍", 1, &searchIconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void MainWindow::paintCommandBar(HDC hdc, int width) {
    int cmdTop = tabBarHeight_ + navBarHeight_;
    int cmdBottom = cmdTop + commandBarHeight_;
    RECT cmdRc{0, cmdTop, width, cmdBottom};
    HBRUSH cmdBrush = CreateSolidBrush(RGB(32, 32, 32));
    FillRect(hdc, &cmdRc, cmdBrush);
    DeleteObject(cmdBrush);

    SelectObject(hdc, fontRegular_);
    SetBkMode(hdc, TRANSPARENT);

    // 1. "+ New ▾" Button
    RECT newRc{10, cmdTop + 6, 90, cmdTop + 38};
    HBRUSH newBg = CreateSolidBrush(RGB(45, 45, 45));
    FillRect(hdc, &newRc, newBg);
    DeleteObject(newBg);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, L"+ New ▾", -1, &newRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Divider
    HPEN divPen = CreatePen(PS_SOLID, 1, RGB(55, 55, 55));
    SelectObject(hdc, divPen);
    MoveToEx(hdc, 96, cmdTop + 10, nullptr);
    LineTo(hdc, 96, cmdTop + 34);

    // Action Icons: Cut, Copy, Paste, Rename, Delete
    auto drawActionBtn = [&](int x, int w, const wchar_t* glyph) {
        RECT rc{x, cmdTop + 6, x + w, cmdTop + 38};
        SetTextColor(hdc, RGB(200, 200, 200));
        DrawTextW(hdc, glyph, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    drawActionBtn(102, 30, L"✂");
    drawActionBtn(136, 30, L"📄");
    drawActionBtn(170, 30, L"📋");
    drawActionBtn(204, 30, L"🏷");
    drawActionBtn(238, 30, L"🗑");

    // Divider
    MoveToEx(hdc, 276, cmdTop + 10, nullptr);
    LineTo(hdc, 276, cmdTop + 34);

    // Dropdowns: Sort ▾, View ▾, Filter ▾
    auto drawDropdownBtn = [&](int x, int w, const wchar_t* label) {
        RECT rc{x, cmdTop + 6, x + w, cmdTop + 38};
        HBRUSH bg = CreateSolidBrush(RGB(42, 42, 42));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);
        SetTextColor(hdc, RGB(220, 220, 220));
        DrawTextW(hdc, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    drawDropdownBtn(285, 75, L"⇅ Sort ▾");
    drawDropdownBtn(365, 75, L"⊞ View ▾");

    // Filter Dropdown with active filter label
    std::wstring filterLabel = L"⚚ Filter ▾";
    if (filterCategory_ != FilterCategory::All) filterLabel = L"⚚ Filtered ▾";
    drawDropdownBtn(445, 95, filterLabel.c_str());

    DeleteObject(divPen);

    // Bottom Border for Command Ribbon
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(48, 48, 48));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, 0, cmdBottom - 1, nullptr);
    LineTo(hdc, width, cmdBottom - 1);
    DeleteObject(borderPen);
}

void MainWindow::paintSidebar(HDC hdc, int height) {
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    RECT sideRc{0, contentTop, sidebarWidth_, height - statusBarHeight_};
    HBRUSH bgBrush = CreateSolidBrush(RGB(28, 28, 28));
    FillRect(hdc, &sideRc, bgBrush);
    DeleteObject(bgBrush);

    SelectObject(hdc, fontRegular_);
    SetBkMode(hdc, TRANSPARENT);

    int itemY = contentTop + 10;
    for (size_t i = 0; i < sidebarItems_.size(); ++i) {
        const auto& item = sidebarItems_[i];
        RECT rowRc{6, itemY, sidebarWidth_ - 6, itemY + 28};

        bool isSel = (static_cast<int>(i) == sidebarSelectedIndex_);
        bool isHov = (static_cast<int>(i) == sidebarHoverIndex_);

        if (isSel) {
            HBRUSH selBg = CreateSolidBrush(RGB(50, 50, 50));
            FillRect(hdc, &rowRc, selBg);
            DeleteObject(selBg);

            // Left accent pill
            RECT pill{rowRc.left, rowRc.top + 4, rowRc.left + 3, rowRc.bottom - 4};
            HBRUSH acc = CreateSolidBrush(toColorRef(tokens_.accent));
            FillRect(hdc, &pill, acc);
            DeleteObject(acc);
        } else if (isHov) {
            HBRUSH hovBg = CreateSolidBrush(RGB(38, 38, 38));
            FillRect(hdc, &rowRc, hovBg);
            DeleteObject(hovBg);
        }

        // Draw Icon glyph
        RECT iconRc{rowRc.left + 8, rowRc.top, rowRc.left + 28, rowRc.bottom};
        SetTextColor(hdc, RGB(255, 204, 0));
        DrawTextW(hdc, item.iconGlyph.c_str(), -1, &iconRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Draw Item Name
        SetTextColor(hdc, isSel ? RGB(255, 255, 255) : RGB(220, 220, 220));
        RECT textRc{rowRc.left + 32, rowRc.top, rowRc.right - 8, rowRc.bottom};
        DrawTextW(hdc, item.name.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        itemY += 32;
    }

    // Right border for Sidebar
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(45, 45, 45));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, sidebarWidth_ - 1, contentTop, nullptr);
    LineTo(hdc, sidebarWidth_ - 1, height - statusBarHeight_);
    DeleteObject(borderPen);
}

void MainWindow::paintHomeView(HDC hdc, int width, int height) {
    (void)width;
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    int surfaceLeft = sidebarWidth_;

    SetBkMode(hdc, TRANSPARENT);

    // Section 1: "Quick access" Title
    RECT titleRc{surfaceLeft + 20, contentTop + 12, surfaceLeft + 200, contentTop + 36};
    SelectObject(hdc, fontTitle_);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, L"Quick access", -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Render Quick Cards (Grid of cards)
    int cardW = 180;
    int cardH = 62;
    int cardStartX = surfaceLeft + 20;
    int cardStartY = contentTop + 45;

    SelectObject(hdc, fontBold_);
    for (size_t i = 0; i < quickCards_.size(); ++i) {
        int col = static_cast<int>(i % 4);
        int row = static_cast<int>(i / 4);
        int cx = cardStartX + col * (cardW + 16);
        int cy = cardStartY + row * (cardH + 12);
        if (cx + cardW > width - 10) continue;

        quickCards_[i].rect = RECT{cx, cy, cx + cardW, cy + cardH};

        bool isHov = (static_cast<int>(i) == hoverCard_);
        HBRUSH cardBrush = CreateSolidBrush(isHov ? RGB(48, 48, 48) : RGB(38, 38, 38));
        FillRect(hdc, &quickCards_[i].rect, cardBrush);
        DeleteObject(cardBrush);

        // Icon
        RECT iconRc{cx + 10, cy + 12, cx + 42, cy + 44};
        SetTextColor(hdc, RGB(255, 204, 0));
        DrawTextW(hdc, quickCards_[i].iconGlyph.c_str(), -1, &iconRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Name
        SelectObject(hdc, fontBold_);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT nameRc{cx + 42, cy + 10, cx + cardW - 6, cy + 30};
        DrawTextW(hdc, quickCards_[i].name.c_str(), -1, &nameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Subtext ("Stored locally")
        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(150, 150, 150));
        RECT subRc{cx + 42, cy + 30, cx + cardW - 6, cy + 50};
        DrawTextW(hdc, quickCards_[i].subtext.c_str(), -1, &subRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // Section 2: "Recent" Header & Pill tabs
    int recentTop = contentTop + 180;
    RECT recentTitleRc{surfaceLeft + 20, recentTop, surfaceLeft + 120, recentTop + 28};
    SelectObject(hdc, fontTitle_);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, L"Recent", -1, &recentTitleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Table Headers for Recent List
    int tableHeaderTop = recentTop + 32;
    RECT hdrRc{surfaceLeft, tableHeaderTop, width, tableHeaderTop + headerHeight_};
    HBRUSH hdrBrush = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdc, &hdrRc, hdrBrush);
    DeleteObject(hdrBrush);

    SelectObject(hdc, fontSmall_);
    SetTextColor(hdc, RGB(160, 160, 160));

    RECT colNameRc{surfaceLeft + 30, tableHeaderTop, surfaceLeft + 30 + colNameWidth_, tableHeaderTop + headerHeight_};
    RECT colDateRc{colNameRc.right, tableHeaderTop, colNameRc.right + colDateWidth_, tableHeaderTop + headerHeight_};
    RECT colLocRc{colDateRc.right, tableHeaderTop, width - 20, tableHeaderTop + headerHeight_};

    DrawTextW(hdc, L"NAME", 4, &colNameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, L"DATE ACCESSED", 13, &colDateRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, L"FILE LOCATION", 13, &colLocRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Rows
    int rowY = tableHeaderTop + headerHeight_;
    int surfaceBottom = height - statusBarHeight_;
    int visibleRows = (surfaceBottom - rowY) / rowHeight_ + 2;

    SelectObject(hdc, fontRegular_);
    for (int i = 0; i < visibleRows; ++i) {
        int r = scrollOffset_ + i;
        if (r >= static_cast<int>(filteredIndices_.size())) break;

        const auto& item = allItems_[filteredIndices_[r]];
        RECT rowRc{surfaceLeft, rowY + i * rowHeight_, width, rowY + (i + 1) * rowHeight_};

        bool isSel = (r == selectedRow_);
        bool isHov = (r == hoverRow_);

        if (isSel) {
            HBRUSH sel = CreateSolidBrush(RGB(45, 65, 88));
            FillRect(hdc, &rowRc, sel);
            DeleteObject(sel);

            RECT pill{rowRc.left, rowRc.top, rowRc.left + 3, rowRc.bottom};
            HBRUSH acc = CreateSolidBrush(toColorRef(tokens_.accent));
            FillRect(hdc, &pill, acc);
            DeleteObject(acc);
        } else if (isHov) {
            HBRUSH hov = CreateSolidBrush(RGB(35, 35, 35));
            FillRect(hdc, &rowRc, hov);
            DeleteObject(hov);
        }

        // Icon
        HICON hIcon = getFileIcon(item.name, item.isDirectory, item.extension);
        if (hIcon) {
            DrawIconEx(hdc, surfaceLeft + 8, rowRc.top + 6, hIcon, 16, 16, 0, nullptr, DI_NORMAL);
            DestroyIcon(hIcon);
        }

        // Name
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT nameCell{surfaceLeft + 32, rowRc.top, surfaceLeft + 32 + colNameWidth_ - 10, rowRc.bottom};
        DrawTextW(hdc, item.name.c_str(), -1, &nameCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Date
        SetTextColor(hdc, RGB(160, 160, 160));
        RECT dateCell{colNameRc.right, rowRc.top, colNameRc.right + colDateWidth_, rowRc.bottom};
        std::wstring dt = formatTime(item.mtime);
        DrawTextW(hdc, dt.c_str(), -1, &dateCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Location
        RECT locCell{colDateRc.right, rowRc.top, width - 20, rowRc.bottom};
        DrawTextW(hdc, item.location.c_str(), -1, &locCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

void MainWindow::paintDirectoryView(HDC hdc, int width, int height) {
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    int surfaceLeft = sidebarWidth_;

    SetBkMode(hdc, TRANSPARENT);

    // Column Headers (Interactive sorting!)
    RECT hdrRc{surfaceLeft, contentTop, width, contentTop + headerHeight_};
    HBRUSH hdrBrush = CreateSolidBrush(RGB(32, 32, 32));
    FillRect(hdc, &hdrRc, hdrBrush);
    DeleteObject(hdrBrush);

    SelectObject(hdc, fontSmall_);
    SetTextColor(hdc, RGB(160, 160, 160));

    RECT colNameRc{surfaceLeft + 32, contentTop, surfaceLeft + 32 + colNameWidth_, contentTop + headerHeight_};
    RECT colDateRc{colNameRc.right, contentTop, colNameRc.right + colDateWidth_, contentTop + headerHeight_};
    RECT colTypeRc{colDateRc.right, contentTop, colDateRc.right + colTypeWidth_, contentTop + headerHeight_};
    RECT colSizeRc{colTypeRc.right, contentTop, colTypeRc.right + colSizeWidth_, contentTop + headerHeight_};

    std::wstring nameHdr = L"NAME" + std::wstring(sortCol_ == SortColumn::Name ? (sortAscending_ ? L" ▲" : L" ▼") : L"");
    std::wstring dateHdr = L"DATE MODIFIED" + std::wstring(sortCol_ == SortColumn::DateModified ? (sortAscending_ ? L" ▲" : L" ▼") : L"");
    std::wstring typeHdr = L"TYPE" + std::wstring(sortCol_ == SortColumn::Type ? (sortAscending_ ? L" ▲" : L" ▼") : L"");
    std::wstring sizeHdr = L"SIZE" + std::wstring(sortCol_ == SortColumn::Size ? (sortAscending_ ? L" ▲" : L" ▼") : L"");

    DrawTextW(hdc, nameHdr.c_str(), -1, &colNameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, dateHdr.c_str(), -1, &colDateRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, typeHdr.c_str(), -1, &colTypeRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(hdc, sizeHdr.c_str(), -1, &colSizeRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(45, 45, 45));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, surfaceLeft, contentTop + headerHeight_ - 1, nullptr);
    LineTo(hdc, width, contentTop + headerHeight_ - 1);
    DeleteObject(borderPen);

    // Rows
    int rowY = contentTop + headerHeight_;
    int surfaceBottom = height - statusBarHeight_;
    int visibleRows = (surfaceBottom - rowY) / rowHeight_ + 2;

    SelectObject(hdc, fontRegular_);
    for (int i = 0; i < visibleRows; ++i) {
        int r = scrollOffset_ + i;
        if (r >= static_cast<int>(filteredIndices_.size())) break;

        const auto& item = allItems_[filteredIndices_[r]];
        RECT rowRc{surfaceLeft, rowY + i * rowHeight_, width, rowY + (i + 1) * rowHeight_};

        bool isSel = (r == selectedRow_);
        bool isHov = (r == hoverRow_);

        if (isSel) {
            HBRUSH sel = CreateSolidBrush(RGB(45, 65, 88));
            FillRect(hdc, &rowRc, sel);
            DeleteObject(sel);

            RECT pill{rowRc.left, rowRc.top, rowRc.left + 3, rowRc.bottom};
            HBRUSH acc = CreateSolidBrush(toColorRef(tokens_.accent));
            FillRect(hdc, &pill, acc);
            DeleteObject(acc);
        } else if (isHov) {
            HBRUSH hov = CreateSolidBrush(RGB(35, 35, 35));
            FillRect(hdc, &rowRc, hov);
            DeleteObject(hov);
        }

        // Real Windows Shell File Icon
        HICON hIcon = getFileIcon(item.name, item.isDirectory, item.extension);
        if (hIcon) {
            DrawIconEx(hdc, surfaceLeft + 8, rowRc.top + 7, hIcon, 16, 16, 0, nullptr, DI_NORMAL);
            DestroyIcon(hIcon);
        }

        // Name
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT nameCell{surfaceLeft + 32, rowRc.top, surfaceLeft + 32 + colNameWidth_ - 10, rowRc.bottom};
        DrawTextW(hdc, item.name.c_str(), -1, &nameCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Date Modified
        SetTextColor(hdc, RGB(160, 160, 160));
        RECT dateCell{colNameRc.right, rowRc.top, colNameRc.right + colDateWidth_, rowRc.bottom};
        std::wstring dt = formatTime(item.mtime);
        DrawTextW(hdc, dt.c_str(), -1, &dateCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Type
        RECT typeCell{colDateRc.right, rowRc.top, colDateRc.right + colTypeWidth_, rowRc.bottom};
        std::wstring typeStr = item.isDirectory ? L"File folder" : (item.extension.empty() ? L"File" : item.extension + L" File");
        DrawTextW(hdc, typeStr.c_str(), -1, &typeCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Size
        RECT sizeCell{colTypeRc.right, rowRc.top, colTypeRc.right + colSizeWidth_, rowRc.bottom};
        std::wstring sz = item.isDirectory ? L"" : formatSize(item.size);
        DrawTextW(hdc, sz.c_str(), -1, &sizeCell, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
}

void MainWindow::paintStatusBar(HDC hdc, int width, int height) {
    RECT statusRc{0, height - statusBarHeight_, width, height};
    HBRUSH bg = CreateSolidBrush(RGB(28, 28, 28));
    FillRect(hdc, &statusRc, bg);
    DeleteObject(bg);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(45, 45, 45));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, 0, height - statusBarHeight_, nullptr);
    LineTo(hdc, width, height - statusBarHeight_);
    DeleteObject(borderPen);

    SelectObject(hdc, fontSmall_);
    SetTextColor(hdc, RGB(180, 180, 180));
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

    // Double-buffered rendering
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    // Root background
    HBRUSH bgBrush = CreateSolidBrush(RGB(24, 24, 24));
    FillRect(memDC, &clientRc, bgBrush);
    DeleteObject(bgBrush);

    // 1. Tab Bar
    paintTabBar(memDC, width);

    // 2. Navigation & Address Bar
    paintNavAndAddressBar(memDC, width);

    // 3. Command Bar / Ribbon (User Image 2!)
    paintCommandBar(memDC, width);

    // 4. Sidebar (User Image 1!)
    paintSidebar(memDC, height);

    // 5. Main Content Area (Home View vs Directory View)
    if (isHomeView_) {
        paintHomeView(memDC, width, height);
    } else {
        paintDirectoryView(memDC, width, height);
    }

    // 6. Status Bar
    paintStatusBar(memDC, width, height);

    // Blit to screen
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

} // namespace gx::app
