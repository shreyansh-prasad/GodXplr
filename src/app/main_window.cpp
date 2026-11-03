#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "main_window.h"

#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <algorithm>
#include <iomanip>
#include <sstream>

#include "godxplr/core/fuzzy_matcher.h"
#include "godxplr/platform/shell_context_menu.h"

namespace gx::app {

namespace {

const wchar_t* kClassName = L"GodXplrMainWindowClass";
constexpr int kIdAddressEdit = 1001;
constexpr int kIdSearchEdit = 1002;

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

COLORREF toColorRef(const ui::ColorRgba& c) {
    return RGB(c.r, c.g, c.b);
}

} // namespace

bool MainWindow::registerWindowClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = &MainWindow::windowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // Handled in double-buffered WM_PAINT
    wc.lpszClassName = kClassName;
    return RegisterClassExW(&wc) != 0;
}

MainWindow* MainWindow::create(HINSTANCE hInstance, const std::wstring& initialPath) {
    auto* window = new MainWindow(hInstance);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int initW = 1200;
    int initH = 800;
    int initX = (screenW - initW) / 2;
    int initY = (screenH - initH) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        kClassName,
        L"GodXplr — High-Performance File Manager",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VSCROLL,
        initX, initY, initW, initH,
        nullptr, nullptr, hInstance, window);

    if (!hwnd) {
        delete window;
        return nullptr;
    }

    std::filesystem::path startPath = initialPath.empty() 
        ? std::filesystem::current_path() 
        : std::filesystem::path(initialPath);

    window->navigateTo(startPath);
    return window;
}

MainWindow::MainWindow(HINSTANCE hInstance)
    : hInstance_(hInstance)
    , tokens_(ui::DesignTokens::darkPalette())
{
    fontRegular_ = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");

    if (!fontRegular_) {
        fontRegular_ = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }

    fontBold_ = CreateFontW(
        -13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");

    fontSmall_ = CreateFontW(
        -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
}

MainWindow::~MainWindow() {
    if (fontRegular_) DeleteObject(fontRegular_);
    if (fontBold_) DeleteObject(fontBold_);
    if (fontSmall_) DeleteObject(fontSmall_);
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
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            120, 8, 540, 28,
            hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdAddressEdit)), hInstance_, nullptr);

        SendMessageW(editAddress_, WM_SETFONT, (WPARAM)fontRegular_, TRUE);

        // Search Edit Control
        editSearch_ = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            670, 8, 300, 28,
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
            if (std::filesystem::exists(p) && std::filesystem::is_directory(p)) {
                navigateTo(p);
            }
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
        return 1; // Prevent flicker; painting is double-buffered

    case WM_VSCROLL:
        onVScroll(wParam);
        return 0;

    case WM_MOUSEWHEEL:
        onMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;

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
        int addrWidth = std::max(200, width - 420);
        SetWindowPos(editAddress_, nullptr, 110, 10, addrWidth, 26, SWP_NOZORDER);
        SetWindowPos(editSearch_, nullptr, 120 + addrWidth, 10, 260, 26, SWP_NOZORDER);
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

void MainWindow::onLButtonDown(int x, int y, bool isDoubleClick) {
    // Check if clicked in top bar navigation buttons
    if (y < topBarHeight_) {
        if (x >= 10 && x < 40) {
            navigateBack();
        } else if (x >= 40 && x < 70) {
            navigateForward();
        } else if (x >= 70 && x < 100) {
            navigateUp();
        }
        return;
    }

    // Check if in file surface
    int surfaceY = y - (topBarHeight_ + headerHeight_);
    if (surfaceY >= 0) {
        int clickedRow = scrollOffset_ + (surfaceY / rowHeight_);
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
    int surfaceY = y - (topBarHeight_ + headerHeight_);
    if (surfaceY >= 0) {
        int clickedRow = scrollOffset_ + (surfaceY / rowHeight_);
        if (clickedRow >= 0 && clickedRow < static_cast<int>(filteredIndices_.size())) {
            selectedRow_ = clickedRow;
            InvalidateRect(hwnd_, nullptr, FALSE);

            const auto& item = allItems_[filteredIndices_[selectedRow_]];
            std::filesystem::path fullPath = currentPath_ / item.name;

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
    if (count == 0) return;

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
            int visibleRows = (clientRc.bottom - topBarHeight_ - headerHeight_ - statusBarHeight_) / rowHeight_;
            if (selectedRow_ >= scrollOffset_ + visibleRows) {
                scrollOffset_ = selectedRow_ - visibleRows + 1;
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        break;

    case VK_PRIOR: // PageUp
        selectedRow_ = std::max(0, selectedRow_ - 15);
        scrollOffset_ = std::max(0, scrollOffset_ - 15);
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;

    case VK_NEXT: // PageDown
        selectedRow_ = std::min(count - 1, selectedRow_ + 15);
        scrollOffset_ = std::min(std::max(0, count - 10), scrollOffset_ + 15);
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;

    case VK_HOME:
        selectedRow_ = 0;
        scrollOffset_ = 0;
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;

    case VK_END:
        selectedRow_ = count - 1;
        scrollOffset_ = std::max(0, count - 10);
        InvalidateRect(hwnd_, nullptr, FALSE);
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
    }
}

void MainWindow::onSearchChanged() {
    filterItems();
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

    SetWindowTextW(editAddress_, currentPath_.c_str());
    SetWindowTextW(editSearch_, L"");

    refresh();
}

void MainWindow::navigateUp() {
    if (currentPath_.has_parent_path() && currentPath_ != currentPath_.parent_path()) {
        navigateTo(currentPath_.parent_path());
    }
}

void MainWindow::navigateBack() {
    if (!backStack_.empty()) {
        auto prev = backStack_.back();
        backStack_.pop_back();
        forwardStack_.push_back(currentPath_);
        currentPath_ = prev;
        SetWindowTextW(editAddress_, currentPath_.c_str());
        refresh();
    }
}

void MainWindow::navigateForward() {
    if (!forwardStack_.empty()) {
        auto next = forwardStack_.back();
        forwardStack_.pop_back();
        backStack_.push_back(currentPath_);
        currentPath_ = next;
        SetWindowTextW(editAddress_, currentPath_.c_str());
        refresh();
    }
}

void MainWindow::refresh() {
    allItems_.clear();
    filteredIndices_.clear();

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
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
                continue;
            }

            FileItemDisplay item;
            item.name = fd.cFileName;
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

    // Sort: directories first, then alphabetically
    std::sort(allItems_.begin(), allItems_.end(), [](const FileItemDisplay& a, const FileItemDisplay& b) {
        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory > b.isDirectory;
        }
        return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
    });

    filterItems();
    selectedRow_ = filteredIndices_.empty() ? -1 : 0;
    scrollOffset_ = 0;
    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::filterItems() {
    filteredIndices_.clear();

    wchar_t searchBuf[256];
    GetWindowTextW(editSearch_, searchBuf, 256);
    std::wstring query = searchBuf;

    if (query.empty()) {
        filteredIndices_.resize(allItems_.size());
        for (size_t i = 0; i < allItems_.size(); ++i) {
            filteredIndices_[i] = i;
        }
        return;
    }

    // Filter using fast substring & case-insensitive search
    std::transform(query.begin(), query.end(), query.begin(), ::towlower);
    for (size_t i = 0; i < allItems_.size(); ++i) {
        std::wstring lowerName = allItems_[i].name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);
        if (lowerName.find(query) != std::wstring::npos) {
            filteredIndices_.push_back(i);
        }
    }
}

void MainWindow::openSelectedItem() {
    if (selectedRow_ < 0 || selectedRow_ >= static_cast<int>(filteredIndices_.size())) {
        return;
    }

    const auto& item = allItems_[filteredIndices_[selectedRow_]];
    std::filesystem::path fullPath = currentPath_ / item.name;

    if (item.isDirectory) {
        navigateTo(fullPath);
    } else {
        // Open file with default Windows associated application
        SHELLEXECUTEINFOW sei{};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_FLAG_NO_UI;
        sei.lpVerb = L"open";
        sei.lpFile = fullPath.c_str();
        sei.nShow = SW_SHOWNORMAL;
        ShellExecuteExW(&sei);
    }
}

void MainWindow::updateScrollInfo() {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int surfaceHeight = rc.bottom - topBarHeight_ - headerHeight_ - statusBarHeight_;
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

void MainWindow::onPaint(HDC hdc) {
    RECT clientRc;
    GetClientRect(hwnd_, &clientRc);
    int width = clientRc.right;
    int height = clientRc.bottom;

    // Double-buffered offscreen bitmap
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    // 1. Fill Background
    HBRUSH bgBrush = CreateSolidBrush(toColorRef(tokens_.surfaceBackground));
    FillRect(memDC, &clientRc, bgBrush);
    DeleteObject(bgBrush);

    // 2. Render Top Bar
    RECT topBarRc{0, 0, width, topBarHeight_};
    HBRUSH topBarBrush = CreateSolidBrush(toColorRef(tokens_.surfaceElevated));
    FillRect(memDC, &topBarRc, topBarBrush);
    DeleteObject(topBarBrush);

    // Draw navigation buttons (<, >, ^)
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, toColorRef(tokens_.textPrimary));
    SelectObject(memDC, fontBold_);

    RECT btnBack{10, 10, 38, 36};
    RECT btnFwd{40, 10, 68, 36};
    RECT btnUp{70, 10, 98, 36};

    HBRUSH btnBg = CreateSolidBrush(toColorRef(tokens_.surfaceSidebar));
    FillRect(memDC, &btnBack, btnBg);
    FillRect(memDC, &btnFwd, btnBg);
    FillRect(memDC, &btnUp, btnBg);
    DeleteObject(btnBg);

    DrawTextW(memDC, L"<", 1, &btnBack, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(memDC, L">", 1, &btnFwd, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(memDC, L"^", 1, &btnUp, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Top Bar Bottom Border
    HPEN borderPen = CreatePen(PS_SOLID, 1, toColorRef(tokens_.borderSubtle));
    HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
    MoveToEx(memDC, 0, topBarHeight_ - 1, nullptr);
    LineTo(memDC, width, topBarHeight_ - 1);

    // 3. Render Column Headers
    RECT headerRc{0, topBarHeight_, width, topBarHeight_ + headerHeight_};
    HBRUSH headerBrush = CreateSolidBrush(toColorRef(tokens_.surfaceSidebar));
    FillRect(memDC, &headerRc, headerBrush);
    DeleteObject(headerBrush);

    SetTextColor(memDC, toColorRef(tokens_.textSecondary));
    SelectObject(memDC, fontSmall_);

    RECT colNameRc{40, topBarHeight_, 40 + colNameWidth_, topBarHeight_ + headerHeight_};
    RECT colTypeRc{colNameRc.right, topBarHeight_, colNameRc.right + colTypeWidth_, topBarHeight_ + headerHeight_};
    RECT colSizeRc{colTypeRc.right, topBarHeight_, colTypeRc.right + colSizeWidth_, topBarHeight_ + headerHeight_};
    RECT colDateRc{colSizeRc.right, topBarHeight_, colSizeRc.right + colDateWidth_, topBarHeight_ + headerHeight_};

    DrawTextW(memDC, L"NAME", 4, &colNameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(memDC, L"TYPE", 4, &colTypeRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(memDC, L"SIZE", 4, &colSizeRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(memDC, L"DATE MODIFIED", 13, &colDateRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    MoveToEx(memDC, 0, topBarHeight_ + headerHeight_ - 1, nullptr);
    LineTo(memDC, width, topBarHeight_ + headerHeight_ - 1);

    // 4. Render Virtualized File Rows
    int surfaceTop = topBarHeight_ + headerHeight_;
    int surfaceBottom = height - statusBarHeight_;
    int visibleRows = (surfaceBottom - surfaceTop) / rowHeight_ + 2;

    SelectObject(memDC, fontRegular_);

    for (int i = 0; i < visibleRows; ++i) {
        int rowIndex = scrollOffset_ + i;
        if (rowIndex >= static_cast<int>(filteredIndices_.size())) break;

        const auto& item = allItems_[filteredIndices_[rowIndex]];
        int rowY = surfaceTop + (i * rowHeight_);
        RECT rowRc{0, rowY, width, rowY + rowHeight_};

        bool isSelected = (rowIndex == selectedRow_);
        if (isSelected) {
            HBRUSH selBrush = CreateSolidBrush(RGB(45, 65, 88));
            FillRect(memDC, &rowRc, selBrush);
            DeleteObject(selBrush);

            // Left accent bar
            RECT accentBar{0, rowY, 3, rowY + rowHeight_};
            HBRUSH accBrush = CreateSolidBrush(toColorRef(tokens_.accent));
            FillRect(memDC, &accentBar, accBrush);
            DeleteObject(accBrush);
        }

        // Draw Icon glyph (Folder or Document)
        SetTextColor(memDC, item.isDirectory ? RGB(255, 204, 0) : toColorRef(tokens_.textSecondary));
        RECT iconRc{12, rowY, 32, rowY + rowHeight_};
        DrawTextW(memDC, item.isDirectory ? L"[D]" : L"[F]", 3, &iconRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Draw Name
        SetTextColor(memDC, toColorRef(tokens_.textPrimary));
        RECT nameRc{40, rowY, 40 + colNameWidth_ - 10, rowY + rowHeight_};
        DrawTextW(memDC, item.name.c_str(), -1, &nameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Draw Type
        SetTextColor(memDC, toColorRef(tokens_.textSecondary));
        RECT typeRc{colTypeRc.left, rowY, colTypeRc.right - 10, rowY + rowHeight_};
        std::wstring typeStr = item.isDirectory ? L"Folder" : (item.extension.empty() ? L"File" : item.extension + L" File");
        DrawTextW(memDC, typeStr.c_str(), -1, &typeRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Draw Size
        RECT sizeRc{colSizeRc.left, rowY, colSizeRc.right, rowY + rowHeight_};
        std::wstring sizeStr = item.isDirectory ? L"" : formatSize(item.size);
        DrawTextW(memDC, sizeStr.c_str(), -1, &sizeRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        // Draw Date
        RECT dateRc{colDateRc.left + 10, rowY, colDateRc.right, rowY + rowHeight_};
        std::wstring dateStr = formatTime(item.mtime);
        DrawTextW(memDC, dateStr.c_str(), -1, &dateRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // 5. Render Status Bar
    RECT statusRc{0, height - statusBarHeight_, width, height};
    HBRUSH statusBrush = CreateSolidBrush(toColorRef(tokens_.surfaceSidebar));
    FillRect(memDC, &statusRc, statusBrush);
    DeleteObject(statusBrush);

    MoveToEx(memDC, 0, height - statusBarHeight_, nullptr);
    LineTo(memDC, width, height - statusBarHeight_);

    SetTextColor(memDC, toColorRef(tokens_.textSecondary));
    SelectObject(memDC, fontSmall_);

    wchar_t statusText[128];
    swprintf_s(statusText, L"%zu items  |  GodXplr High-Performance Local Engine (Zero AI / Zero Cloud)",
        filteredIndices_.size());
    RECT statusTextRc{12, height - statusBarHeight_, width - 12, height};
    DrawTextW(memDC, statusText, -1, &statusTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Blit to screen
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldPen);
    DeleteObject(borderPen);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

} // namespace gx::app
