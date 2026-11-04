#include "main_window.h"

#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <chrono>

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

// Sort IDs
constexpr UINT_PTR kCmdSortName = 3010;
constexpr UINT_PTR kCmdSortDate = 3011;
constexpr UINT_PTR kCmdSortType = 3012;
constexpr UINT_PTR kCmdSortSize = 3013;
constexpr UINT_PTR kCmdSortAsc = 3014;
constexpr UINT_PTR kCmdSortDesc = 3015;

// View Mode IDs
constexpr UINT_PTR kCmdViewDetails = 3020;
constexpr UINT_PTR kCmdViewTiles = 3021;
constexpr UINT_PTR kCmdViewList = 3022;
constexpr UINT_PTR kCmdViewGrid = 3023;
constexpr UINT_PTR kCmdToggleCompact = 3024;
constexpr UINT_PTR kCmdToggleNavPane = 3025;
constexpr UINT_PTR kCmdToggleDetailsPane = 3026;
constexpr UINT_PTR kCmdTogglePreviewPane = 3027;
constexpr UINT_PTR kCmdToggleCheckBoxes = 3028;
constexpr UINT_PTR kCmdToggleExtensions = 3029;
constexpr UINT_PTR kCmdToggleHidden = 3030;

// Filter Category IDs
constexpr UINT_PTR kCmdFilterAll = 3040;
constexpr UINT_PTR kCmdFilterFolders = 3041;
constexpr UINT_PTR kCmdFilterDocs = 3042;
constexpr UINT_PTR kCmdFilterPics = 3043;
constexpr UINT_PTR kCmdFilterMedia = 3044;
constexpr UINT_PTR kCmdFilterArchives = 3045;
constexpr UINT_PTR kCmdFilterCode = 3046;

// Date Filter IDs
constexpr UINT_PTR kCmdFilterDateAll = 3050;
constexpr UINT_PTR kCmdFilterDateToday = 3051;
constexpr UINT_PTR kCmdFilterDateYesterday = 3052;
constexpr UINT_PTR kCmdFilterDateWeek = 3053;
constexpr UINT_PTR kCmdFilterDateMonth = 3054;
constexpr UINT_PTR kCmdFilterDateYear = 3055;

// Size Filter IDs
constexpr UINT_PTR kCmdFilterSizeAll = 3060;
constexpr UINT_PTR kCmdFilterSizeEmpty = 3061;
constexpr UINT_PTR kCmdFilterSizeTiny = 3062;
constexpr UINT_PTR kCmdFilterSizeSmall = 3063;
constexpr UINT_PTR kCmdFilterSizeMedium = 3064;
constexpr UINT_PTR kCmdFilterSizeLarge = 3065;
constexpr UINT_PTR kCmdFilterSizeHuge = 3066;

// Favorites Actions
constexpr UINT_PTR kCmdAddFavorite = 3070;
constexpr UINT_PTR kCmdRemoveFavorite = 3071;

// More Menu IDs
constexpr UINT_PTR kCmdOpenTerminal = 3080;
constexpr UINT_PTR kCmdOpenCmd = 3081;
constexpr UINT_PTR kCmdCopyPath = 3082;
constexpr UINT_PTR kCmdSelectAll = 3083;
constexpr UINT_PTR kCmdInvertSelect = 3084;
constexpr UINT_PTR kCmdClearSelect = 3085;
constexpr UINT_PTR kCmdProperties = 3086;

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
    if (bytes == 0) return L"0 B";
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
    int initW = (screenW >= 1440) ? 1380 : std::max(1024, screenW - 30);
    int initH = (screenH >= 900) ? 860 : std::max(720, screenH - 40);
    int initX = std::max(10, (screenW - initW) / 2);
    int initY = std::max(10, (screenH - initH) / 2);

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

    // Native Windows 11 Immersive Dark Mode & Rounded Corners
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));
    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &corner, sizeof(corner));

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

    // Standard High-Quality Windows UI Fonts (Spacious, Ergonomic Visual Hierarchy)
    fontRegular_ = CreateFontW(
        -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
    if (!fontRegular_) fontRegular_ = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    fontBold_ = CreateFontW(
        -15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");

    fontTitle_ = CreateFontW(
        -19, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Display");
    if (!fontTitle_) {
        fontTitle_ = CreateFontW(
            -19, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
    }

    fontSmall_ = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");

    // Native Microsoft Fluent Vector Icon Fonts (Installed on all Windows 10/11)
    fontIcons_ = CreateFontW(
        -20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe Fluent Icons");
    if (!fontIcons_) {
        fontIcons_ = CreateFontW(
            -20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    }

    fontIconsLarge_ = CreateFontW(
        -36, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe Fluent Icons");
    if (!fontIconsLarge_) {
        fontIconsLarge_ = CreateFontW(
            -36, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    }

    fontIconsSmall_ = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe Fluent Icons");
    if (!fontIconsSmall_) {
        fontIconsSmall_ = CreateFontW(
            -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    }

    darkEditBrush_ = CreateSolidBrush(RGB(41, 41, 41));

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
    if (darkEditBrush_) DeleteObject(darkEditBrush_);
}

void MainWindow::initKnownFolders() {
    pathDesktop_ = getKnownPath(FOLDERID_Desktop);
    pathDownloads_ = getKnownPath(FOLDERID_Downloads);
    pathDocuments_ = getKnownPath(FOLDERID_Documents);
    pathPictures_ = getKnownPath(FOLDERID_Pictures);
    pathMusic_ = getKnownPath(FOLDERID_Music);
    pathVideos_ = getKnownPath(FOLDERID_Videos);
    pathHome_ = getKnownPath(FOLDERID_Profile);

    // OneDrive personal directory
    pathOneDrive_ = pathHome_ + L"\\OneDrive";
    if (!std::filesystem::exists(pathOneDrive_)) {
        WCHAR envBuf[MAX_PATH]{};
        if (GetEnvironmentVariableW(L"OneDrive", envBuf, MAX_PATH)) {
            pathOneDrive_ = envBuf;
        }
    }
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

    // 1. Home
    sidebarItems_.push_back({ L"Home", L"godxplr://home", L"\uE80F", RGB(235, 130, 60), L"", false, false, 0, false, false, true });

    // 2. Gallery
    sidebarItems_.push_back({ L"Gallery", pathPictures_, L"\uEB9F", RGB(62, 166, 255), L"", false, false, 0, false, false, true });

    // 3. OneDrive / Cloud account root ("Shreyansh - Personal")
    sidebarItems_.push_back({ L"Shreyansh - Personal", pathOneDrive_, L"\uE753", RGB(0, 120, 215), L"", false, false, 0, true, true, true });

    // Subfolders indented under OneDrive (Level 1)
    sidebarItems_.push_back({ L"Attachments", pathOneDrive_ + L"\\Attachments", L"\uE8B7", RGB(245, 195, 65), L"", false, false, 1, false, false, false });
    sidebarItems_.push_back({ L"Desktop", pathDesktop_, L"\uE7F4", RGB(0, 150, 255), L"", false, false, 1, false, false, false });
    sidebarItems_.push_back({ L"Documents", pathDocuments_, L"\uE8A5", RGB(70, 150, 245), L"", false, false, 1, false, false, false });
    sidebarItems_.push_back({ L"Music", pathMusic_, L"\uE8D6", RGB(255, 140, 0), L"", false, false, 1, false, false, false });
    sidebarItems_.push_back({ L"NSUT", pathOneDrive_ + L"\\NSUT", L"\uE8B7", RGB(245, 195, 65), L"", false, false, 1, false, false, false });
    sidebarItems_.push_back({ L"Pictures", pathPictures_, L"\uEB9F", RGB(177, 70, 194), L"", false, false, 1, true, false, false });
    sidebarItems_.push_back({ L"Videos", pathVideos_, L"\uE714", RGB(232, 17, 35), L"", false, false, 1, true, false, false });

    // Divider
    sidebarItems_.push_back({ L"", L"", L"", 0, L"", true, false, 0, false, false, false });

    // 4. Pinned Items
    sidebarItems_.push_back({ L"Desktop", pathDesktop_, L"\uE7F4", RGB(0, 150, 255), L"", false, false, 0, false, false, false });
    sidebarItems_.push_back({ L"Downloads", pathDownloads_, L"\uE896", RGB(0, 185, 150), L"", false, false, 0, false, false, false });
    sidebarItems_.push_back({ L"Documents", pathDocuments_, L"\uE8A5", RGB(70, 150, 245), L"", false, false, 0, false, false, false });
    sidebarItems_.push_back({ L"Pictures", pathPictures_, L"\uEB9F", RGB(177, 70, 194), L"", false, false, 0, false, false, false });
    sidebarItems_.push_back({ L"Music", pathMusic_, L"\uE8D6", RGB(255, 140, 0), L"", false, false, 0, false, false, false });
    sidebarItems_.push_back({ L"Videos", pathVideos_, L"\uE714", RGB(232, 17, 35), L"", false, false, 0, false, false, false });
    sidebarItems_.push_back({ L"Recycle Bin", L"shell:RecycleBinFolder", L"\uE748", RGB(160, 160, 160), L"", false, false, 0, false, false, false });
    sidebarItems_.push_back({ L"Gallery", pathPictures_, L"\uEB9F", RGB(62, 166, 255), L"", false, false, 0, false, false, false });
    sidebarItems_.push_back({ L"Games", L"C:\\Games", L"\uE8B7", RGB(245, 195, 65), L"", false, false, 0, false, false, false });
    sidebarItems_.push_back({ L"NSUT", pathOneDrive_ + L"\\NSUT", L"\uE8B7", RGB(245, 195, 65), L"", false, false, 0, false, false, false });
    sidebarItems_.push_back({ L"Moksha", L"C:\\Moksha", L"\uE8B7", RGB(245, 195, 65), L"", false, false, 0, false, false, false });
    sidebarItems_.push_back({ L"os fileX", L"C:\\os fileX", L"\uE8B7", RGB(245, 195, 65), L"", false, false, 0, false, false, false });

    // Divider
    sidebarItems_.push_back({ L"", L"", L"", 0, L"", true, false, 0, false, false, false });

    // 5. This PC (Expandable, expanded by default with Windows C indented)
    sidebarItems_.push_back({ L"This PC", L"godxplr://thispc", L"\uE7F4", RGB(180, 180, 180), L"", false, false, 0, true, true, true });

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

            // Indented Level 1 under This PC
            sidebarItems_.push_back({ label, root, L"\uEDA2", RGB(96, 205, 255), freeText, false, true, 1, true, false, false });
            ptr += wcslen(ptr) + 1;
        }
    }

    sidebarItems_.push_back({ L"Network", L"godxplr://network", L"\uE774", RGB(70, 150, 245), L"", false, false, 0, false, false, false });
    sidebarItems_.push_back({ L"Linux", L"\\\\wsl$", L"\uE7F8", RGB(245, 195, 65), L"", false, false, 0, false, false, false });

    // Quick Access cards for Home View (12 items in 4 columns x 3 rows)
    std::wstring nsutPath = pathOneDrive_ + L"\\NSUT";
    quickCards_ = {
        { L"Desktop", L"Shreyansh - Personal", pathDesktop_, L"\uE7F4", RGB(0, 120, 215), {} },
        { L"Downloads", L"Stored locally", pathDownloads_, L"\uE896", RGB(0, 178, 148), {} },
        { L"Documents", L"Shreyansh - Personal", pathDocuments_, L"\uE8A5", RGB(110, 130, 155), {} },
        { L"Pictures", L"Shreyansh - Personal", pathPictures_, L"\uEB9F", RGB(0, 153, 255), {} },
        { L"Music", L"Shreyansh - Personal", pathMusic_, L"\uE8D6", RGB(255, 110, 90), {} },
        { L"Videos", L"Shreyansh - Personal", pathVideos_, L"\uE714", RGB(160, 80, 220), {} },
        { L"Recycle Bin", L"Stored locally", L"shell:RecycleBinFolder", L"\uE74D", RGB(96, 205, 255), {} },
        { L"Gallery", L"Stored locally", pathPictures_, L"\uEB9F", RGB(0, 160, 240), {} },
        { L"Games", L"Windows-SSD (C:)", L"C:\\Games", L"\uE8B7", RGB(245, 195, 65), {} },
        { L"Moksha", L"Windows-SSD (C:)", L"C:\\Moksha", L"\uE8B7", RGB(245, 195, 65), {} },
        { L"NSUT", L"Shreyansh - Personal", nsutPath, L"\uE8B7", RGB(245, 195, 65), {} },
        { L"os fileX", L"Windows-SSD (C:)", L"C:\\os fileX", L"\uE8B7", RGB(245, 195, 65), {} }
    };
}

void MainWindow::toggleSidebarExpansion(size_t index) {
    if (index >= sidebarItems_.size()) return;
    if (sidebarItems_[index].isExpanded) {
        collapseSidebarItem(index);
    } else {
        expandSidebarItem(index);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
    UpdateWindow(hwnd_);
}

void MainWindow::expandSidebarItem(size_t index) {
    if (index >= sidebarItems_.size()) return;
    auto& item = sidebarItems_[index];
    if (!std::filesystem::exists(item.path)) return;

    item.isExpanded = true;
    int targetIndent = item.indentLevel + 1;

    std::vector<SidebarItem> children;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(item.path, std::filesystem::directory_options::skip_permission_denied)) {
            std::error_code ec;
            if (entry.is_directory(ec)) {
                std::wstring name = entry.path().filename().wstring();
                if (!showHiddenItems_ && (name.front() == L'.' || (GetFileAttributesW(entry.path().c_str()) & FILE_ATTRIBUTE_HIDDEN))) {
                    continue;
                }

                // Check if child has any subdirectories
                bool hasSub = false;
                try {
                    for (const auto& sub : std::filesystem::directory_iterator(entry.path(), std::filesystem::directory_options::skip_permission_denied)) {
                        if (sub.is_directory(ec)) { hasSub = true; break; }
                    }
                } catch (...) {}

                children.push_back({
                    name,
                    entry.path().wstring(),
                    L"\uE8B7",
                    RGB(245, 195, 65),
                    L"",
                    false,
                    false,
                    targetIndent,
                    hasSub,
                    false,
                    false
                });
            }
        }
    } catch (...) {}

    // Sort children alphabetically
    std::sort(children.begin(), children.end(), [](const SidebarItem& a, const SidebarItem& b) {
        return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
    });

    sidebarItems_.insert(sidebarItems_.begin() + index + 1, children.begin(), children.end());
}

void MainWindow::collapseSidebarItem(size_t index) {
    if (index >= sidebarItems_.size()) return;
    sidebarItems_[index].isExpanded = false;
    int parentIndent = sidebarItems_[index].indentLevel;

    size_t eraseCount = 0;
    for (size_t i = index + 1; i < sidebarItems_.size(); ++i) {
        if (sidebarItems_[i].indentLevel > parentIndent) {
            eraseCount++;
        } else {
            break;
        }
    }

    if (eraseCount > 0) {
        sidebarItems_.erase(sidebarItems_.begin() + index + 1, sidebarItems_.begin() + index + 1 + eraseCount);
    }
}

void MainWindow::addToFavorites(const std::wstring& path) {
    if (!isFavorite(path)) {
        favorites_.push_back(path);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void MainWindow::removeFromFavorites(const std::wstring& path) {
    auto it = std::find(favorites_.begin(), favorites_.end(), path);
    if (it != favorites_.end()) {
        favorites_.erase(it);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

bool MainWindow::isFavorite(const std::wstring& path) const {
    return std::find(favorites_.begin(), favorites_.end(), path) != favorites_.end();
}

void MainWindow::toggleCompactView() {
    compactView_ = !compactView_;
    rowHeight_ = compactView_ ? 30 : 40;
    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::toggleNavigationPane() {
    showNavigationPane_ = !showNavigationPane_;
    sidebarWidth_ = showNavigationPane_ ? 270 : 0;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::toggleDetailsPane() {
    showDetailsPane_ = !showDetailsPane_;
    if (showDetailsPane_) showPreviewPane_ = false;
    RECT rc;
    GetClientRect(hwnd_, &rc);
    onResize(rc.right, rc.bottom);
    InvalidateRect(hwnd_, nullptr, FALSE);
    UpdateWindow(hwnd_);
}

void MainWindow::togglePreviewPane() {
    showPreviewPane_ = !showPreviewPane_;
    if (showPreviewPane_) showDetailsPane_ = false;
    RECT rc;
    GetClientRect(hwnd_, &rc);
    onResize(rc.right, rc.bottom);
    InvalidateRect(hwnd_, nullptr, FALSE);
    UpdateWindow(hwnd_);
}

void MainWindow::toggleCheckBoxes() {
    showCheckBoxes_ = !showCheckBoxes_;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::toggleExtensions() {
    showExtensions_ = !showExtensions_;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::toggleHiddenItems() {
    showHiddenItems_ = !showHiddenItems_;
    refresh();
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
        editAddress_ = CreateWindowExW(
            0,
            L"EDIT",
            L"",
            WS_CHILD | ES_AUTOHSCROLL,
            165, 50, 550, 28,
            hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdAddressEdit)), hInstance_, nullptr);
        SendMessageW(editAddress_, WM_SETFONT, (WPARAM)fontRegular_, TRUE);

        editSearch_ = CreateWindowExW(
            0,
            L"EDIT",
            L"Search Home",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            750, 50, 280, 28,
            hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdSearchEdit)), hInstance_, nullptr);
        SendMessageW(editSearch_, WM_SETFONT, (WPARAM)fontRegular_, TRUE);
        searchIsPlaceholder_ = true;
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == kIdSearchEdit) {
            if (code == EN_SETFOCUS) {
                if (searchIsPlaceholder_) {
                    searchIsPlaceholder_ = false;
                    SetWindowTextW(editSearch_, L"");
                    InvalidateRect(editSearch_, nullptr, TRUE);
                }
            } else if (code == EN_KILLFOCUS) {
                wchar_t buf[64]{};
                GetWindowTextW(editSearch_, buf, 64);
                if (wcslen(buf) == 0) {
                    searchIsPlaceholder_ = true;
                    std::wstring ph = isHomeView_ ? L"Search Home" : (L"Search " + (currentPath_.filename().empty() ? currentPath_.wstring() : currentPath_.filename().wstring()));
                    SetWindowTextW(editSearch_, ph.c_str());
                    InvalidateRect(editSearch_, nullptr, TRUE);
                }
            } else if (code == EN_CHANGE) {
                if (!searchIsPlaceholder_) {
                    onSearchChanged();
                }
            }
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

        // Sorting
        case kCmdSortName: setSortColumn(SortColumn::Name); break;
        case kCmdSortDate: setSortColumn(SortColumn::DateModified); break;
        case kCmdSortType: setSortColumn(SortColumn::Type); break;
        case kCmdSortSize: setSortColumn(SortColumn::Size); break;
        case kCmdSortAsc: sortAscending_ = true; filterAndSortItems(); InvalidateRect(hwnd_, nullptr, FALSE); break;
        case kCmdSortDesc: sortAscending_ = false; filterAndSortItems(); InvalidateRect(hwnd_, nullptr, FALSE); break;

        // Layout Modes
        case kCmdViewDetails: viewMode_ = ViewMode::Details; InvalidateRect(hwnd_, nullptr, FALSE); break;
        case kCmdViewTiles: viewMode_ = ViewMode::Tiles; InvalidateRect(hwnd_, nullptr, FALSE); break;
        case kCmdViewList: viewMode_ = ViewMode::List; InvalidateRect(hwnd_, nullptr, FALSE); break;
        case kCmdViewGrid: viewMode_ = ViewMode::Grid; InvalidateRect(hwnd_, nullptr, FALSE); break;

        // View Toggles
        case kCmdToggleCompact: toggleCompactView(); break;
        case kCmdToggleNavPane: toggleNavigationPane(); break;
        case kCmdToggleDetailsPane: toggleDetailsPane(); break;
        case kCmdTogglePreviewPane: togglePreviewPane(); break;
        case kCmdToggleCheckBoxes: toggleCheckBoxes(); break;
        case kCmdToggleExtensions: toggleExtensions(); break;
        case kCmdToggleHidden: toggleHiddenItems(); break;

        // Filters
        case kCmdFilterAll: setFilterCategory(FilterCategory::All); break;
        case kCmdFilterFolders: setFilterCategory(FilterCategory::Folders); break;
        case kCmdFilterDocs: setFilterCategory(FilterCategory::Documents); break;
        case kCmdFilterPics: setFilterCategory(FilterCategory::Pictures); break;
        case kCmdFilterMedia: setFilterCategory(FilterCategory::AudioVideo); break;
        case kCmdFilterArchives: setFilterCategory(FilterCategory::Compressed); break;
        case kCmdFilterCode: setFilterCategory(FilterCategory::Code); break;

        // Date Filters
        case kCmdFilterDateAll: setDateFilter(DateFilter::All); break;
        case kCmdFilterDateToday: setDateFilter(DateFilter::Today); break;
        case kCmdFilterDateYesterday: setDateFilter(DateFilter::Yesterday); break;
        case kCmdFilterDateWeek: setDateFilter(DateFilter::ThisWeek); break;
        case kCmdFilterDateMonth: setDateFilter(DateFilter::ThisMonth); break;
        case kCmdFilterDateYear: setDateFilter(DateFilter::ThisYear); break;

        // Size Filters
        case kCmdFilterSizeAll: setSizeFilter(SizeFilter::All); break;
        case kCmdFilterSizeEmpty: setSizeFilter(SizeFilter::Empty); break;
        case kCmdFilterSizeTiny: setSizeFilter(SizeFilter::Tiny); break;
        case kCmdFilterSizeSmall: setSizeFilter(SizeFilter::Small); break;
        case kCmdFilterSizeMedium: setSizeFilter(SizeFilter::Medium); break;
        case kCmdFilterSizeLarge: setSizeFilter(SizeFilter::Large); break;
        case kCmdFilterSizeHuge: setSizeFilter(SizeFilter::Huge); break;

        // Favorites
        case kCmdAddFavorite:
            if (selectedRow_ >= 0 && selectedRow_ < static_cast<int>(filteredIndices_.size())) {
                const auto& item = allItems_[filteredIndices_[selectedRow_]];
                addToFavorites((currentPath_ / item.name).wstring());
            }
            break;
        case kCmdRemoveFavorite:
            if (selectedRow_ >= 0 && selectedRow_ < static_cast<int>(filteredIndices_.size())) {
                const auto& item = allItems_[filteredIndices_[selectedRow_]];
                removeFromFavorites((currentPath_ / item.name).wstring());
            }
            break;

        // More Menu Commands
        case kCmdOpenTerminal:
            ShellExecuteW(hwnd_, L"open", L"wt.exe", (L"-d \"" + currentPath_.wstring() + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
            break;
        case kCmdOpenCmd:
            ShellExecuteW(hwnd_, L"open", L"cmd.exe", (L"/k cd /d \"" + currentPath_.wstring() + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
            break;
        case kCmdCopyPath:
            if (OpenClipboard(hwnd_)) {
                EmptyClipboard();
                std::wstring text = (selectedRow_ >= 0 && selectedRow_ < static_cast<int>(filteredIndices_.size()))
                    ? (currentPath_ / allItems_[filteredIndices_[selectedRow_]].name).wstring()
                    : currentPath_.wstring();
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
                if (hMem) {
                    memcpy(GlobalLock(hMem), text.c_str(), (text.size() + 1) * sizeof(wchar_t));
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
                CloseClipboard();
            }
            break;
        case kCmdSelectAll:
            checkedRows_.assign(allItems_.size(), true);
            InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        case kCmdInvertSelect:
            for (size_t i = 0; i < checkedRows_.size(); ++i) {
                checkedRows_[i] = !checkedRows_[i];
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        case kCmdClearSelect:
            checkedRows_.assign(allItems_.size(), false);
            selectedRow_ = -1;
            InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        case kCmdProperties:
            if (selectedRow_ >= 0 && selectedRow_ < static_cast<int>(filteredIndices_.size())) {
                const auto& item = allItems_[filteredIndices_[selectedRow_]];
                std::wstring fullPath = (currentPath_ / item.name).wstring();
                SHELLEXECUTEINFOW sei{sizeof(sei)};
                sei.fMask = SEE_MASK_INVOKEIDLIST;
                sei.hwnd = hwnd_;
                sei.lpVerb = L"properties";
                sei.lpFile = fullPath.c_str();
                sei.nShow = SW_SHOW;
                ShellExecuteExW(&sei);
            }
            break;
        }
        return 0;
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HWND hCtrl = (HWND)lParam;
        if (hCtrl == editAddress_ || hCtrl == editSearch_) {
            HDC hdcCtrl = (HDC)wParam;
            bool isPh = (hCtrl == editSearch_ && searchIsPlaceholder_);
            SetTextColor(hdcCtrl, isPh ? RGB(140, 140, 140) : RGB(240, 240, 240));
            SetBkColor(hdcCtrl, RGB(41, 41, 41));
            return (LRESULT)darkEditBrush_;
        }
        break;
    }

    case WM_USER + 101: // Set Home section (0: Recent, 1: Fav, 2: Shared)
        homeSection_ = static_cast<int>(wParam);
        filterAndSortItems();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;

    case WM_USER + 102: // Toggle details pane
        toggleDetailsPane();
        return 0;

    case WM_USER + 103: // Toggle sidebar expansion
        toggleSidebarExpansion(static_cast<size_t>(wParam));
        return 0;

    case WM_USER + 104: // Set view mode
        viewMode_ = static_cast<ViewMode>(wParam);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;

    case WM_USER + 105: // Toggle preview pane
        togglePreviewPane();
        return 0;

    case WM_USER + 106: // Toggle checkboxes
        toggleCheckBoxes();
        return 0;

    case WM_USER + 107: // Toggle compact view
        toggleCompactView();
        return 0;

    case WM_USER + 108: // Toggle navigation pane
        toggleNavigationPane();
        return 0;

    case WM_USER + 109: // Navigate to sidebar item index or path
        if (lParam != 0) {
            navigateTo(reinterpret_cast<const wchar_t*>(lParam));
        } else if (wParam < sidebarItems_.size()) {
            sidebarSelectedIndex_ = static_cast<int>(wParam);
            const auto& item = sidebarItems_[wParam];
            if (item.path == L"godxplr://home") {
                navigateToHome();
            } else if (std::filesystem::exists(item.path)) {
                navigateTo(std::filesystem::path(item.path));
            }
        }
        return 0;

    case WM_USER + 110: // Select row
        selectedRow_ = static_cast<int>(wParam);
        InvalidateRect(hwnd_, nullptr, FALSE);
        UpdateWindow(hwnd_);
        return 0;

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
        int rightMargin = (showDetailsPane_ ? detailsPaneWidth_ : (showPreviewPane_ ? previewPaneWidth_ : 0));
        int searchW = 280;
        int searchX = width - rightMargin - searchW - 20;
        int addrX = 165;
        int addrW = searchX - addrX - 16;
        int boxH = 36;
        int boxY = tabBarHeight_ + (navBarHeight_ - boxH) / 2;

        SetWindowPos(editAddress_, nullptr, addrX + 12, boxY + 4, addrW - 24, 28, SWP_NOZORDER);
        SetWindowPos(editSearch_, nullptr, searchX + 14, boxY + 4, searchW - 52, 28, SWP_NOZORDER);
    }
    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::onVScroll(WPARAM wParam) {
    int count = static_cast<int>(filteredIndices_.size());
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    int listTop = getListTop(contentTop);
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
    int listTop = getListTop(contentTop);
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
    int oldHoverCmd = hoverCmdBtn_;

    sidebarHoverIndex_ = -1;
    hoverRow_ = -1;
    hoverCard_ = -1;
    hoverPill_ = -1;
    hoverCmdBtn_ = -1;

    // Command ribbon hover tracking
    int cmdTop = tabBarHeight_ + navBarHeight_;
    int cmdBottom = cmdTop + commandBarHeight_;
    if (y >= cmdTop && y < cmdBottom) {
        RECT clientRc;
        GetClientRect(hwnd_, &clientRc);
        int w = clientRc.right;
        if (x >= 12 && x < 112) hoverCmdBtn_ = 0; // New
        else if (x >= 128 && x < 164) hoverCmdBtn_ = 1; // Cut
        else if (x >= 168 && x < 204) hoverCmdBtn_ = 2; // Copy
        else if (x >= 208 && x < 244) hoverCmdBtn_ = 3; // Paste
        else if (x >= 248 && x < 284) hoverCmdBtn_ = 4; // Rename
        else if (x >= 288 && x < 324) hoverCmdBtn_ = 5; // Share
        else if (x >= 328 && x < 364) hoverCmdBtn_ = 6; // Delete
        else if (x >= 380 && x < 464) hoverCmdBtn_ = 7; // Sort
        else if (x >= 472 && x < 560) hoverCmdBtn_ = 8; // View
        else if (x >= 568 && x < 660) hoverCmdBtn_ = 9; // Filter
        else if (x >= 676 && x < 714) hoverCmdBtn_ = 10; // More
        else if (x >= w - 108 && x < w - 12) hoverCmdBtn_ = 11; // Details
    }
    if (oldHoverCmd != hoverCmdBtn_) {
        RECT cmdBarRc{0, cmdTop, 2500, cmdBottom};
        InvalidateRect(hwnd_, &cmdBarRc, FALSE);
    }

    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;

    // 1. Sidebar Hover
    if (showNavigationPane_ && x < sidebarWidth_ && y >= contentTop) {
        int sideY = y - contentTop - 8;
        int curY = 0;
        for (size_t i = 0; i < sidebarItems_.size(); ++i) {
            int h = sidebarItems_[i].isHeader ? 26 : 36;
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
    int listTop = getListTop(contentTop);
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
    RECT clientRc;
    GetClientRect(hwnd_, &clientRc);

    // 1. Tab Bar Clicks (y in 0 .. tabBarHeight_)
    if (y < tabBarHeight_) {
        int tabX = 12;
        for (size_t i = 0; i < tabs_.size(); ++i) {
            RECT tabRc{tabX, 4, tabX + 180, tabBarHeight_};
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
            tabX += 186;
        }

        // New Tab (+) button
        RECT newTabRc{tabX, 4, tabX + 34, tabBarHeight_};
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
        if (x >= 12 && x < 46) navigateBack();
        else if (x >= 50 && x < 84) navigateForward();
        else if (x >= 88 && x < 122) navigateUp();
        else if (x >= 126 && x < 160) refresh();
        else {
            int rightMargin = (showDetailsPane_ ? detailsPaneWidth_ : (showPreviewPane_ ? previewPaneWidth_ : 0));
            int searchW = 280;
            int searchX = clientRc.right - rightMargin - searchW - 20;

            // Search Clear button (✕)
            if (x >= searchX + searchW - 52 && x < searchX + searchW - 32) {
                searchIsPlaceholder_ = true;
                std::wstring ph = isHomeView_ ? L"Search Home" : (L"Search " + (currentPath_.filename().empty() ? currentPath_.wstring() : currentPath_.filename().wstring()));
                SetWindowTextW(editSearch_, ph.c_str());
                InvalidateRect(editSearch_, nullptr, TRUE);
                onSearchChanged();
                return;
            }

            // Check breadcrumb clicks
            for (const auto& bc : breadcrumbs_) {
                if (PtInRect(&bc.rect, POINT{x, y})) {
                    if (bc.path == L"godxplr://home") navigateToHome();
                    else navigateTo(std::filesystem::path(bc.path));
                    return;
                }
            }

            // Clicked address area -> show edit
            if (x >= 165 && x < searchX - 16) {
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
        if (x >= 12 && x < 112) {
            showNewMenu(x, cmdBottom);
        } else if (x >= 128 && x < 164) {
            cutSelectedItem();
        } else if (x >= 168 && x < 204) {
            copySelectedItem();
        } else if (x >= 208 && x < 244) {
            pasteItem();
        } else if (x >= 248 && x < 284) {
            renameSelectedItem();
        } else if (x >= 288 && x < 324) {
            // Share
        } else if (x >= 328 && x < 364) {
            deleteSelectedItem();
        } else if (x >= 380 && x < 464) {
            showSortMenu(x, cmdBottom);
        } else if (x >= 472 && x < 560) {
            showViewMenu(x, cmdBottom);
        } else if (x >= 568 && x < 660) {
            showFilterMenu(x, cmdBottom);
        } else if (x >= 676 && x < 714) {
            showMoreMenu(x, cmdBottom);
        } else if (x >= clientRc.right - 108 && x < clientRc.right - 12) {
            toggleDetailsPane();
        }
        return;
    }

    // 4. Sidebar Clicks (Expandable Tree with Chevrons)
    int contentTop = cmdBottom;
    if (showNavigationPane_ && x < sidebarWidth_ && y >= contentTop) {
        int sideY = y - contentTop - 8;
        int curY = 0;
        for (size_t i = 0; i < sidebarItems_.size(); ++i) {
            int h = sidebarItems_[i].isHeader ? 26 : 36;
            if (sideY >= curY && sideY < curY + h) {
                if (!sidebarItems_[i].isHeader) {
                    int chevronX = 8 + sidebarItems_[i].indentLevel * 18;
                    // If user clicked the expansion chevron box (22px)
                    if (sidebarItems_[i].isExpandable && x >= chevronX && x < chevronX + 22) {
                        toggleSidebarExpansion(i);
                        return;
                    }

                    // Otherwise navigate to folder
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

    // 5. Details Pane / Preview Pane Close button
    if (showDetailsPane_ || showPreviewPane_) {
        int paneW = showDetailsPane_ ? detailsPaneWidth_ : previewPaneWidth_;
        int paneX = clientRc.right - paneW;
        if (x >= paneX && y >= contentTop) {
            // Close 'X' button
            if (x >= clientRc.right - 30 && x < clientRc.right - 8 && y >= contentTop + 8 && y < contentTop + 30) {
                showDetailsPane_ = false;
                showPreviewPane_ = false;
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            }
        }
    }

    // 6. Directory View: 1-Click Quick Filter Pills Clicks
    if (!isHomeView_ && x >= sidebarWidth_ && y >= contentTop && y < contentTop + filterBarHeight_) {
        for (const auto& pill : filterPills_) {
            if (PtInRect(&pill.rect, POINT{x, y})) {
                setFilterCategory(pill.cat);
                return;
            }
        }
        return;
    }

    // 7. Directory View: Column Header Click (Interactive Sorting)
    if (!isHomeView_ && x >= sidebarWidth_) {
        int hdrTop = contentTop + filterBarHeight_;
        int hdrBottom = hdrTop + headerHeight_;
        if (y >= hdrTop && y < hdrBottom) {
            int relX = x - sidebarWidth_ - (showCheckBoxes_ ? 24 : 0);
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

    // 8. Home View: Quick Cards & Section Tabs Clicks
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
            filterAndSortItems();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        } else if (PtInRect(&pillFavRc_, pt)) {
            homeSection_ = 1;
            filterAndSortItems();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        } else if (PtInRect(&pillSharedRc_, pt)) {
            homeSection_ = 2;
            filterAndSortItems();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
    }

    // 9. File List Row Click & Double-Click (File Execution)
    int listTop = getListTop(contentTop);
    if (x >= sidebarWidth_ && y >= listTop) {
        int clickedRow = scrollOffset_ + ((y - listTop) / rowHeight_);
        if (clickedRow >= 0 && clickedRow < static_cast<int>(filteredIndices_.size())) {
            // Checkbox click
            if (showCheckBoxes_ && x >= sidebarWidth_ + 8 && x < sidebarWidth_ + 26) {
                if (static_cast<size_t>(clickedRow) < checkedRows_.size()) {
                    checkedRows_[clickedRow] = !checkedRows_[clickedRow];
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    return;
                }
            }

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
    int listTop = getListTop(contentTop);

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

            // Favorites Context Menu
            HMENU hMenu = CreatePopupMenu();
            bool fav = isFavorite(fullPath.wstring());
            AppendMenuW(hMenu, MF_STRING, fav ? kCmdRemoveFavorite : kCmdAddFavorite,
                fav ? L"⭐ Remove from Favorites" : L"⭐ Add to Favorites");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

            platform::ShellContextMenu shellMenu;
            std::vector<std::wstring> paths = { fullPath.wstring() };
            if (shellMenu.populate(paths)) {
                shellMenu.showAndInvoke(hwnd_, pt.x, pt.y);
            } else {
                TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
            }
            DestroyMenu(hMenu);
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
            int listTop = getListTop(contentTop);
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

    case '1':
        viewMode_ = ViewMode::Details;
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;

    case '2':
        viewMode_ = ViewMode::Tiles;
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;

    case '3':
        viewMode_ = ViewMode::List;
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;

    case 'D':
        toggleDetailsPane();
        break;

    case 'P':
        togglePreviewPane();
        break;

    case 'T':
        viewMode_ = (viewMode_ == ViewMode::Tiles) ? ViewMode::Details : ViewMode::Tiles;
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;

    case 'C':
        toggleCompactView();
        break;

    case 'H':
        toggleHiddenItems();
        break;
    }
}

void MainWindow::onSearchChanged() {
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
    if (searchIsPlaceholder_) {
        std::wstring ph = L"Search " + (currentPath_.filename().empty() ? currentPath_.wstring() : currentPath_.filename().wstring());
        SetWindowTextW(editSearch_, ph.c_str());
        InvalidateRect(editSearch_, nullptr, TRUE);
    }

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
            item.fullPath = entry.path().wstring();
            item.isDirectory = entry.is_directory(ec);

            // Hidden items filter
            if (!showHiddenItems_ && (item.name.front() == L'.' || (GetFileAttributesW(entry.path().c_str()) & FILE_ATTRIBUTE_HIDDEN))) {
                continue;
            }

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
                item.attributes = fad.dwFileAttributes;
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
    if (searchIsPlaceholder_) {
        SetWindowTextW(editSearch_, L"Search Home");
        InvalidateRect(editSearch_, nullptr, TRUE);
    }

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
                item.fullPath = entry.path().wstring();
                item.location = dir.filename().wstring();
                std::error_code ec;
                item.isDirectory = entry.is_directory(ec);

                if (!showHiddenItems_ && (item.name.front() == L'.' || (GetFileAttributesW(entry.path().c_str()) & FILE_ATTRIBUTE_HIDDEN))) {
                    continue;
                }

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
                    item.attributes = fad.dwFileAttributes;
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
    if (!searchIsPlaceholder_) {
        GetWindowTextW(editSearch_, searchBuf, 256);
    }
    std::wstring query = toLower(searchBuf);

    // Calculate current time for Date filters
    FILETIME currentFt;
    GetSystemTimeAsFileTime(&currentFt);
    ULARGE_INTEGER curUli;
    curUli.LowPart = currentFt.dwLowDateTime;
    curUli.HighPart = currentFt.dwHighDateTime;
    uint64_t oneDay100ns = 864000000000ULL;

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

        // 3. Date Modified Filter
        if (dateFilter_ != DateFilter::All && item.mtime > 0 && curUli.QuadPart >= item.mtime) {
            uint64_t diff = curUli.QuadPart - item.mtime;
            switch (dateFilter_) {
            case DateFilter::Today:
                if (diff > oneDay100ns) continue;
                break;
            case DateFilter::Yesterday:
                if (diff <= oneDay100ns || diff > 2 * oneDay100ns) continue;
                break;
            case DateFilter::ThisWeek:
                if (diff > 7 * oneDay100ns) continue;
                break;
            case DateFilter::ThisMonth:
                if (diff > 30 * oneDay100ns) continue;
                break;
            case DateFilter::ThisYear:
                if (diff > 365 * oneDay100ns) continue;
                break;
            case DateFilter::All:
            default:
                break;
            }
        }

        // 4. Size Filter
        if (sizeFilter_ != SizeFilter::All && !item.isDirectory) {
            uint64_t sz = item.size;
            switch (sizeFilter_) {
            case SizeFilter::Empty:
                if (sz != 0) continue;
                break;
            case SizeFilter::Tiny:
                if (sz == 0 || sz > 16 * 1024) continue;
                break;
            case SizeFilter::Small:
                if (sz <= 16 * 1024 || sz > 1024 * 1024) continue;
                break;
            case SizeFilter::Medium:
                if (sz <= 1024 * 1024 || sz > 128 * 1024 * 1024) continue;
                break;
            case SizeFilter::Large:
                if (sz <= 128 * 1024 * 1024 || sz > 1024ULL * 1024 * 1024) continue;
                break;
            case SizeFilter::Huge:
                if (sz <= 1024ULL * 1024 * 1024) continue;
                break;
            case SizeFilter::All:
            default:
                break;
            }
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

    checkedRows_.assign(filteredIndices_.size(), false);
}

void MainWindow::openSelectedItem() {
    if (selectedRow_ < 0 || selectedRow_ >= static_cast<int>(filteredIndices_.size())) {
        return;
    }
    openItem(allItems_[filteredIndices_[selectedRow_]]);
}

void MainWindow::openItem(const FileItemDisplay& item) {
    std::filesystem::path fullPath = !item.fullPath.empty()
        ? std::filesystem::path(item.fullPath)
        : (isHomeView_ ? std::filesystem::path(item.location) / item.name : currentPath_ / item.name);

    if (item.isDirectory) {
        navigateTo(fullPath);
    } else {
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
    std::filesystem::path fullPath = !item.fullPath.empty()
        ? std::filesystem::path(item.fullPath)
        : (isHomeView_ ? std::filesystem::path(item.location) / item.name : currentPath_ / item.name);

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
    std::filesystem::path fullPath = !item.fullPath.empty()
        ? std::filesystem::path(item.fullPath)
        : (isHomeView_ ? std::filesystem::path(item.location) / item.name : currentPath_ / item.name);

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
        sortAscending_ = !sortAscending_;
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

void MainWindow::setDateFilter(DateFilter df) {
    dateFilter_ = df;
    filterAndSortItems();
    selectedRow_ = filteredIndices_.empty() ? -1 : 0;
    scrollOffset_ = 0;
    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::setSizeFilter(SizeFilter sf) {
    sizeFilter_ = sf;
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

    // Layout Modes
    AppendMenuW(hMenu, (viewMode_ == ViewMode::Details ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdViewDetails, L"Details");
    AppendMenuW(hMenu, (viewMode_ == ViewMode::Tiles ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdViewTiles, L"Tiles");
    AppendMenuW(hMenu, (viewMode_ == ViewMode::List ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdViewList, L"List");
    AppendMenuW(hMenu, (viewMode_ == ViewMode::Grid ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdViewGrid, L"Large icons / Grid");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // Compact View
    AppendMenuW(hMenu, (compactView_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdToggleCompact, L"Compact view");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // Show Submenu
    HMENU hSubShow = CreatePopupMenu();
    AppendMenuW(hSubShow, (showNavigationPane_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdToggleNavPane, L"Navigation pane");
    AppendMenuW(hSubShow, (showDetailsPane_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdToggleDetailsPane, L"Details pane");
    AppendMenuW(hSubShow, (showPreviewPane_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdTogglePreviewPane, L"Preview pane");
    AppendMenuW(hSubShow, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSubShow, (showCheckBoxes_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdToggleCheckBoxes, L"Item check boxes");
    AppendMenuW(hSubShow, (showExtensions_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdToggleExtensions, L"File name extensions");
    AppendMenuW(hSubShow, (showHiddenItems_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdToggleHidden, L"Hidden items");

    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubShow, L"Show");

    POINT pt{x, y};
    ClientToScreen(hwnd_, &pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(hMenu);
}

void MainWindow::showFilterMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();

    // Type Submenu
    HMENU hSubType = CreatePopupMenu();
    AppendMenuW(hSubType, (filterCategory_ == FilterCategory::All ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterAll, L"All Files");
    AppendMenuW(hSubType, (filterCategory_ == FilterCategory::Folders ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterFolders, L"Folders");
    AppendMenuW(hSubType, (filterCategory_ == FilterCategory::Documents ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterDocs, L"Documents (.pdf, .docx, .txt)");
    AppendMenuW(hSubType, (filterCategory_ == FilterCategory::Pictures ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterPics, L"Pictures (.png, .jpg, .svg)");
    AppendMenuW(hSubType, (filterCategory_ == FilterCategory::AudioVideo ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterMedia, L"Audio & Video (.mp4, .mp3)");
    AppendMenuW(hSubType, (filterCategory_ == FilterCategory::Compressed ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterArchives, L"Compressed (.zip, .7z)");
    AppendMenuW(hSubType, (filterCategory_ == FilterCategory::Code ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterCode, L"Code & Dev (.cpp, .py, .js)");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubType, L"Type");

    // Date Submenu
    HMENU hSubDate = CreatePopupMenu();
    AppendMenuW(hSubDate, (dateFilter_ == DateFilter::All ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterDateAll, L"All Dates");
    AppendMenuW(hSubDate, (dateFilter_ == DateFilter::Today ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterDateToday, L"Today");
    AppendMenuW(hSubDate, (dateFilter_ == DateFilter::Yesterday ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterDateYesterday, L"Yesterday");
    AppendMenuW(hSubDate, (dateFilter_ == DateFilter::ThisWeek ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterDateWeek, L"This week");
    AppendMenuW(hSubDate, (dateFilter_ == DateFilter::ThisMonth ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterDateMonth, L"This month");
    AppendMenuW(hSubDate, (dateFilter_ == DateFilter::ThisYear ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterDateYear, L"This year");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubDate, L"Date modified");

    // Size Submenu
    HMENU hSubSize = CreatePopupMenu();
    AppendMenuW(hSubSize, (sizeFilter_ == SizeFilter::All ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterSizeAll, L"All Sizes");
    AppendMenuW(hSubSize, (sizeFilter_ == SizeFilter::Empty ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterSizeEmpty, L"Empty (0 KB)");
    AppendMenuW(hSubSize, (sizeFilter_ == SizeFilter::Tiny ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterSizeTiny, L"Tiny (0 - 16 KB)");
    AppendMenuW(hSubSize, (sizeFilter_ == SizeFilter::Small ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterSizeSmall, L"Small (16 KB - 1 MB)");
    AppendMenuW(hSubSize, (sizeFilter_ == SizeFilter::Medium ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterSizeMedium, L"Medium (1 - 128 MB)");
    AppendMenuW(hSubSize, (sizeFilter_ == SizeFilter::Large ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterSizeLarge, L"Large (128 MB - 1 GB)");
    AppendMenuW(hSubSize, (sizeFilter_ == SizeFilter::Huge ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdFilterSizeHuge, L"Huge (> 1 GB)");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubSize, L"Size");

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

void MainWindow::showMoreMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, kCmdOpenTerminal, L"Open in Windows Terminal");
    AppendMenuW(hMenu, MF_STRING, kCmdOpenCmd, L"Open in Command Prompt");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, kCmdCopyPath, L"Copy path\tCtrl+Shift+C");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, kCmdSelectAll, L"Select all\tCtrl+A");
    AppendMenuW(hMenu, MF_STRING, kCmdInvertSelect, L"Invert selection");
    AppendMenuW(hMenu, MF_STRING, kCmdClearSelect, L"Clear selection");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, kCmdProperties, L"Properties\tAlt+Enter");

    POINT pt{x, y};
    ClientToScreen(hwnd_, &pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(hMenu);
}

int MainWindow::getListTop(int contentTop) const {
    if (isHomeView_) {
        int cols = 4;
        int numRows = (static_cast<int>(quickCards_.size()) + cols - 1) / cols;
        int cardH = 68;
        int cardStartY = contentTop + 48;
        int recentTop = cardStartY + numRows * (cardH + 12) + 16;
        return recentTop + 34 + 10;
    }
    return contentTop + filterBarHeight_ + headerHeight_;
}

void MainWindow::updateScrollInfo() {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    int listTop = getListTop(contentTop);
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
        RECT tabRc{tabX, 4, tabX + 180, tabBarHeight_};

        drawRoundedRect(hdc, tabRc, 8, isActive ? RGB(36, 36, 36) : RGB(27, 27, 27),
            isActive ? RGB(45, 45, 45) : RGB(32, 32, 32));

        if (isActive) {
            RECT indRc{tabRc.left + 10, tabRc.top, tabRc.right - 10, tabRc.top + 3};
            HBRUSH acc = CreateSolidBrush(RGB(96, 205, 255));
            FillRect(hdc, &indRc, acc);
            DeleteObject(acc);
        }

        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, isActive ? RGB(96, 205, 255) : RGB(160, 160, 160));
        RECT tabIconRc{tabRc.left + 12, tabRc.top, tabRc.left + 30, tabRc.bottom};
        const wchar_t* iconGlyph = (tabs_[i].title == L"Home") ? L"\uE80F" : L"\uE8B7";
        DrawTextW(hdc, iconGlyph, 1, &tabIconRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, isActive ? RGB(255, 255, 255) : RGB(175, 175, 175));
        RECT textRc{tabRc.left + 34, tabRc.top, tabRc.right - 28, tabRc.bottom};
        DrawTextW(hdc, tabs_[i].title.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        RECT closeRc{tabRc.right - 24, tabRc.top, tabRc.right - 8, tabRc.bottom};
        SetTextColor(hdc, RGB(140, 140, 140));
        DrawTextW(hdc, L"×", 1, &closeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        tabX += 186;
    }

    RECT plusRc{tabX, 4, tabX + 34, tabBarHeight_};
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

    auto drawNavBtn = [&](int x, const wchar_t* iconGlyph, bool enabled = true) {
        RECT rc{x, navTop + 8, x + 34, navTop + 42};
        drawRoundedRect(hdc, rc, 6, RGB(38, 38, 38), RGB(46, 46, 46));
        SelectObject(hdc, fontIcons_);
        SetTextColor(hdc, enabled ? RGB(230, 230, 230) : RGB(110, 110, 110));
        DrawTextW(hdc, iconGlyph, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    drawNavBtn(12, L"\uE72B", !backStack_.empty());
    drawNavBtn(50, L"\uE72A", !forwardStack_.empty());
    drawNavBtn(88, L"\uE74A", !isHomeView_);
    drawNavBtn(126, L"\uE72C", true);

    int rightMargin = (showDetailsPane_ ? detailsPaneWidth_ : (showPreviewPane_ ? previewPaneWidth_ : 0));
    int searchW = 280;
    int searchX = width - rightMargin - searchW - 20;
    int addrX = 165;
    int addrW = searchX - addrX - 16;
    int boxH = 36;
    int boxY = navTop + (navBarHeight_ - boxH) / 2;
    RECT addrRc{addrX, boxY, addrX + addrW, boxY + boxH};

    drawRoundedRect(hdc, addrRc, 8, RGB(41, 41, 41), RGB(58, 58, 58));

    breadcrumbs_.clear();
    int bcX = addrX + 12;

    if (isHomeView_) {
        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, RGB(96, 205, 255));
        RECT homeIconRc{bcX, addrRc.top, bcX + 20, addrRc.bottom};
        DrawTextW(hdc, L"\uE80F", 1, &homeIconRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        bcX += 22;

        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, RGB(240, 240, 240));
        RECT homeTextRc{bcX, addrRc.top, bcX + 54, addrRc.bottom};
        DrawTextW(hdc, L"Home", 4, &homeTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT fullHomeRc{addrX + 12, addrRc.top, bcX + 54, addrRc.bottom};
        breadcrumbs_.push_back({ L"Home", L"godxplr://home", fullHomeRc });
    } else {
        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, RGB(96, 205, 255));
        RECT homeIconRc{bcX, addrRc.top, bcX + 20, addrRc.bottom};
        DrawTextW(hdc, L"\uE80F", 1, &homeIconRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        breadcrumbs_.push_back({ L"Home", L"godxplr://home", homeIconRc });
        bcX += 24;

        std::filesystem::path buildPath;
        for (const auto& part : currentPath_) {
            buildPath /= part;
            std::wstring partStr = part.wstring();
            if (partStr.back() == L'\\') partStr.pop_back();

            SelectObject(hdc, fontIconsSmall_);
            SetTextColor(hdc, RGB(140, 140, 140));
            RECT chevRc{bcX, addrRc.top, bcX + 14, addrRc.bottom};
            DrawTextW(hdc, L"\uE76C", 1, &chevRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            bcX += 16;

            SelectObject(hdc, fontRegular_);
            SetTextColor(hdc, RGB(235, 235, 235));
            int strPixelW = static_cast<int>(partStr.size() * 9 + 12);
            RECT pRc{bcX, addrRc.top, bcX + strPixelW, addrRc.bottom};
            DrawTextW(hdc, partStr.c_str(), -1, &pRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            breadcrumbs_.push_back({ partStr, buildPath.wstring(), pRc });
            bcX = pRc.right;
        }
    }

    // Search Box Container
    RECT searchRc{searchX, boxY, searchX + searchW, boxY + boxH};
    drawRoundedRect(hdc, searchRc, 8, RGB(41, 41, 41), RGB(58, 58, 58));

    wchar_t searchBuf[64]{};
    GetWindowTextW(editSearch_, searchBuf, 64);
    if (!searchIsPlaceholder_ && wcslen(searchBuf) > 0) {
        // Clear '×' button
        RECT clearRc{searchX + searchW - 52, searchRc.top, searchX + searchW - 32, searchRc.bottom};
        SetTextColor(hdc, RGB(160, 160, 160));
        DrawTextW(hdc, L"×", 1, &clearRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Magnifying glass on right edge
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(160, 160, 160));
    RECT sIconRc{searchX + searchW - 30, searchRc.top, searchX + searchW - 10, searchRc.bottom};
    DrawTextW(hdc, L"\uE721", 1, &sIconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void MainWindow::paintCommandBar(HDC hdc, int width) {
    int cmdTop = tabBarHeight_ + navBarHeight_;
    int cmdBottom = cmdTop + commandBarHeight_;
    RECT cmdRc{0, cmdTop, width, cmdBottom};
    HBRUSH cmdBrush = CreateSolidBrush(RGB(28, 28, 28));
    FillRect(hdc, &cmdRc, cmdBrush);
    DeleteObject(cmdBrush);

    SetBkMode(hdc, TRANSPARENT);

    auto drawBtnHoverOrActive = [&](const RECT& rc, int btnId, bool forceActive = false) {
        if (forceActive) {
            drawRoundedRect(hdc, rc, 6, RGB(27, 50, 72), RGB(96, 205, 255));
        } else if (hoverCmdBtn_ == btnId) {
            drawRoundedRect(hdc, rc, 6, RGB(44, 44, 44), RGB(56, 56, 56));
        }
    };

    HPEN divPen = CreatePen(PS_SOLID, 1, RGB(54, 54, 54));
    SelectObject(hdc, divPen);

    int btnY = cmdTop + 6;

    // 1. "+ New ▾" Button (Clean Fluent styling matching reference)
    RECT newRc{12, btnY, 112, btnY + 36};
    drawBtnHoverOrActive(newRc, 0);

    // Circled plus vector icon
    int plusCx = newRc.left + 18;
    int plusCy = (newRc.top + newRc.bottom) / 2;
    HPEN plusPen = CreatePen(PS_SOLID, 1, RGB(96, 205, 255));
    SelectObject(hdc, plusPen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, plusCx - 7, plusCy - 7, plusCx + 8, plusCy + 8);
    MoveToEx(hdc, plusCx - 4, plusCy, nullptr);
    LineTo(hdc, plusCx + 5, plusCy);
    MoveToEx(hdc, plusCx, plusCy - 4, nullptr);
    LineTo(hdc, plusCx, plusCy + 5);
    DeleteObject(plusPen);
    SelectObject(hdc, divPen); // restore divPen

    SelectObject(hdc, fontRegular_);
    SetTextColor(hdc, RGB(240, 240, 240));
    RECT newTextRc{newRc.left + 34, newRc.top, newRc.right - 18, newRc.bottom};
    DrawTextW(hdc, L"New", 3, &newTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(160, 160, 160));
    RECT newChevRc{newRc.right - 18, newRc.top, newRc.right - 6, newRc.bottom};
    DrawTextW(hdc, L"\uE70D", 1, &newChevRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Divider 1
    MoveToEx(hdc, 120, cmdTop + 11, nullptr);
    LineTo(hdc, 120, cmdTop + 37);

    // 2. Action Icons (Cut, Copy, Paste, Rename, Share, Delete)
    auto drawActionBtn = [&](int x, int btnId, const wchar_t* iconGlyph, COLORREF iconTint) {
        RECT rc{x, btnY, x + 36, btnY + 36};
        drawBtnHoverOrActive(rc, btnId);
        SelectObject(hdc, fontIcons_);
        SetTextColor(hdc, iconTint);
        DrawTextW(hdc, iconGlyph, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    drawActionBtn(128, 1, L"\uE8C6", RGB(96, 205, 255)); // Cut (Cyan)
    drawActionBtn(168, 2, L"\uE8C8", RGB(96, 205, 255)); // Copy (Cyan)
    drawActionBtn(208, 3, L"\uE77F", RGB(96, 205, 255)); // Paste (Cyan)
    drawActionBtn(248, 4, L"\uE8AC", RGB(96, 205, 255)); // Rename (Cyan)
    drawActionBtn(288, 5, L"\uE72D", RGB(96, 205, 255)); // Share (Cyan)
    drawActionBtn(328, 6, L"\uE74D", RGB(105, 105, 105)); // Delete (Disabled Gray)

    // Divider 2
    MoveToEx(hdc, 372, cmdTop + 11, nullptr);
    LineTo(hdc, 372, cmdTop + 37);

    // 3. Dropdown Buttons (Sort, View, Filter)
    auto drawDropdownBtn = [&](int x, int w, int btnId, const wchar_t* iconGlyph, COLORREF iconTint, const wchar_t* labelText) {
        RECT rc{x, btnY, x + w, btnY + 36};
        drawBtnHoverOrActive(rc, btnId);

        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, iconTint);
        RECT iRc{rc.left + 8, rc.top, rc.left + 26, rc.bottom};
        DrawTextW(hdc, iconGlyph, 1, &iRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, RGB(235, 235, 235));
        RECT tRc{rc.left + 28, rc.top, rc.right - 18, rc.bottom};
        DrawTextW(hdc, labelText, -1, &tRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, RGB(160, 160, 160));
        RECT cRc{rc.right - 18, rc.top, rc.right - 6, rc.bottom};
        DrawTextW(hdc, L"\uE70D", 1, &cRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    drawDropdownBtn(380, 84, 7, L"\uE8CB", RGB(96, 205, 255), L"Sort"); // Sort icon Cyan

    const wchar_t* viewLabel = L"View";
    if (viewMode_ == ViewMode::Tiles) viewLabel = L"Tiles";
    else if (viewMode_ == ViewMode::List) viewLabel = L"List";
    else if (viewMode_ == ViewMode::Grid) viewLabel = L"Grid";
    drawDropdownBtn(472, 88, 8, L"\uE8A9", RGB(215, 215, 215), viewLabel);

    std::wstring filterLabel = (filterCategory_ == FilterCategory::All && dateFilter_ == DateFilter::All && sizeFilter_ == SizeFilter::All)
        ? L"Filter" : L"Filtered";
    drawDropdownBtn(568, 92, 9, L"\uE71C", RGB(215, 215, 215), filterLabel.c_str());

    // Divider 3 before More
    MoveToEx(hdc, 668, cmdTop + 11, nullptr);
    LineTo(hdc, 668, cmdTop + 37);

    // 4. More button "···"
    RECT moreRc{676, btnY, 714, btnY + 36};
    drawBtnHoverOrActive(moreRc, 10);
    SelectObject(hdc, fontIcons_);
    SetTextColor(hdc, RGB(215, 215, 215));
    DrawTextW(hdc, L"\uE712", 1, &moreRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Divider 4 before Details
    MoveToEx(hdc, width - 116, cmdTop + 11, nullptr);
    LineTo(hdc, width - 116, cmdTop + 37);

    // 5. Details button on far right (toggles Details Pane)
    RECT detRc{width - 108, btnY, width - 12, btnY + 36};
    drawBtnHoverOrActive(detRc, 11, showDetailsPane_);
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, showDetailsPane_ ? RGB(96, 205, 255) : RGB(215, 215, 215));
    RECT detIconRc{detRc.left + 10, detRc.top, detRc.left + 28, detRc.bottom};
    DrawTextW(hdc, L"\uE9F9", 1, &detIconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, fontRegular_);
    SetTextColor(hdc, showDetailsPane_ ? RGB(96, 205, 255) : RGB(235, 235, 235));
    RECT detTextRc{detRc.left + 30, detRc.top, detRc.right - 8, detRc.bottom};
    DrawTextW(hdc, L"Details", 7, &detTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    DeleteObject(divPen);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(42, 42, 42));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, 0, cmdBottom - 1, nullptr);
    LineTo(hdc, width, cmdBottom - 1);
    DeleteObject(borderPen);
}

void MainWindow::paintFilterPills(HDC hdc, int width, int topY) {
    int rightMargin = (showDetailsPane_ ? detailsPaneWidth_ : (showPreviewPane_ ? previewPaneWidth_ : 0));
    int effWidth = width - rightMargin;

    RECT barRc{sidebarWidth_, topY, effWidth, topY + filterBarHeight_};
    HBRUSH bg = CreateSolidBrush(RGB(24, 24, 24));
    FillRect(hdc, &barRc, bg);
    DeleteObject(bg);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(38, 38, 38));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, sidebarWidth_, topY + filterBarHeight_ - 1, nullptr);
    LineTo(hdc, effWidth, topY + filterBarHeight_ - 1);
    DeleteObject(borderPen);

    SetBkMode(hdc, TRANSPARENT);

    int pillX = sidebarWidth_ + 14;
    for (size_t i = 0; i < filterPills_.size(); ++i) {
        auto& pill = filterPills_[i];
        bool isActive = (pill.cat == filterCategory_);
        bool isHover = (static_cast<int>(i) == hoverPill_);

        int pillW = static_cast<int>(pill.label.size() * 9 + 42);
        pill.rect = RECT{pillX, topY + 5, pillX + pillW, topY + 35};

        COLORREF fillCol = isActive ? RGB(27, 50, 72) : (isHover ? RGB(45, 45, 45) : RGB(33, 33, 33));
        COLORREF borderCol = isActive ? RGB(96, 205, 255) : (isHover ? RGB(65, 65, 65) : RGB(48, 48, 48));
        drawRoundedRect(hdc, pill.rect, 15, fillCol, borderCol);

        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, isActive ? RGB(96, 205, 255) : (isHover ? RGB(255, 255, 255) : RGB(170, 170, 170)));
        RECT iRc{pill.rect.left + 10, pill.rect.top, pill.rect.left + 26, pill.rect.bottom};
        DrawTextW(hdc, pill.iconGlyph.c_str(), 1, &iRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, isActive ? RGB(96, 205, 255) : (isHover ? RGB(255, 255, 255) : RGB(210, 210, 210)));
        RECT tRc{pill.rect.left + 28, pill.rect.top, pill.rect.right - 10, pill.rect.bottom};
        DrawTextW(hdc, pill.label.c_str(), -1, &tRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        pillX += pillW + 8;
    }
}

void MainWindow::paintSidebar(HDC hdc, int height) {
    if (!showNavigationPane_) return;

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
            if (item.name.empty()) {
                HPEN divPen = CreatePen(PS_SOLID, 1, RGB(44, 44, 44));
                SelectObject(hdc, divPen);
                MoveToEx(hdc, 16, itemY + 8, nullptr);
                LineTo(hdc, sidebarWidth_ - 16, itemY + 8);
                DeleteObject(divPen);
                itemY += 16;
            } else {
                RECT headerRc{16, itemY + 4, sidebarWidth_ - 16, itemY + 24};
                SelectObject(hdc, fontSmall_);
                SetTextColor(hdc, RGB(130, 130, 130));
                DrawTextW(hdc, item.name.c_str(), -1, &headerRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                itemY += 26;
            }
            continue;
        }

        RECT rowRc{8, itemY, sidebarWidth_ - 8, itemY + 36};
        bool isSel = (static_cast<int>(i) == sidebarSelectedIndex_);
        bool isHov = (static_cast<int>(i) == sidebarHoverIndex_);

        if (isSel) {
            drawRoundedRect(hdc, rowRc, 6, RGB(37, 37, 37), RGB(48, 48, 48));
            RECT pill{rowRc.left + 2, rowRc.top + 6, rowRc.left + 5, rowRc.bottom - 6};
            HBRUSH acc = CreateSolidBrush(RGB(96, 205, 255));
            FillRect(hdc, &pill, acc);
            DeleteObject(acc);
        } else if (isHov) {
            drawRoundedRect(hdc, rowRc, 6, RGB(32, 32, 32), RGB(40, 40, 40));
        }

        int indentX = rowRc.left + item.indentLevel * 18;

        // Tree Chevron ('>' collapsed, 'v' expanded)
        if (item.isExpandable) {
            SelectObject(hdc, fontIconsSmall_);
            SetTextColor(hdc, RGB(150, 150, 150));
            RECT chevRc{indentX, rowRc.top, indentX + 20, rowRc.bottom};
            DrawTextW(hdc, item.isExpanded ? L"\uE70D" : L"\uE76C", 1, &chevRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // Vector Icon
        SelectObject(hdc, fontIcons_);
        SetTextColor(hdc, item.iconColor);
        RECT iconRc{indentX + 22, rowRc.top, indentX + 46, rowRc.bottom};
        DrawTextW(hdc, item.iconGlyph.c_str(), 1, &iconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Name
        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, isSel ? RGB(255, 255, 255) : (isHov ? RGB(245, 245, 245) : RGB(220, 220, 220)));
        RECT textRc{indentX + 50, rowRc.top, sidebarWidth_ - 30, rowRc.bottom};
        DrawTextW(hdc, item.name.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Pinned item pin indicator
        if (i >= 11 && i <= 22) {
            SelectObject(hdc, fontIconsSmall_);
            SetTextColor(hdc, RGB(120, 120, 120));
            RECT pinRc{sidebarWidth_ - 26, rowRc.top, sidebarWidth_ - 8, rowRc.bottom};
            DrawTextW(hdc, L"\uE718", 1, &pinRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        itemY += 36;
    }

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(40, 40, 40));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, sidebarWidth_ - 1, contentTop, nullptr);
    LineTo(hdc, sidebarWidth_ - 1, height - statusBarHeight_);
    DeleteObject(borderPen);
}

void MainWindow::paintEmptyState(HDC hdc, const RECT& rc, const wchar_t* iconGlyph, COLORREF iconCol, const wchar_t* title, const wchar_t* subtitle) {
    int midX = (rc.left + rc.right) / 2;
    int midY = (rc.top + rc.bottom) / 2 - 30;

    // Center Illustration Circle Container
    RECT circleRc{midX - 36, midY - 36, midX + 36, midY + 36};
    drawRoundedRect(hdc, circleRc, 36, RGB(32, 32, 32), RGB(46, 46, 46));

    SelectObject(hdc, fontIconsLarge_);
    SetTextColor(hdc, iconCol);
    DrawTextW(hdc, iconGlyph, 1, &circleRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Title
    SelectObject(hdc, fontTitle_);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT titleRc{rc.left + 20, midY + 48, rc.right - 20, midY + 74};
    DrawTextW(hdc, title, -1, &titleRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // Subtitle
    SelectObject(hdc, fontSmall_);
    SetTextColor(hdc, RGB(150, 150, 150));
    RECT subRc{rc.left + 40, midY + 76, rc.right - 40, midY + 110};
    DrawTextW(hdc, subtitle, -1, &subRc, DT_CENTER | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
}

void MainWindow::paintHomeView(HDC hdc, int width, int height) {
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    int surfaceLeft = sidebarWidth_;
    int rightMargin = (showDetailsPane_ ? detailsPaneWidth_ : (showPreviewPane_ ? previewPaneWidth_ : 0));
    int effWidth = width - rightMargin;

    SetBkMode(hdc, TRANSPARENT);

    // Section 1: "v Quick access" Title
    RECT titleChevRc{surfaceLeft + 16, contentTop + 14, surfaceLeft + 32, contentTop + 40};
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(180, 180, 180));
    DrawTextW(hdc, L"\uE70D", 1, &titleChevRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT titleRc{surfaceLeft + 36, contentTop + 14, surfaceLeft + 260, contentTop + 40};
    SelectObject(hdc, fontTitle_);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, L"Quick access", -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // Quick Cards Grid (Windows 11 Fluent 4-Column Grid)
    int cardGap = 12;
    int cols = 4;
    int cardAreaW = effWidth - surfaceLeft - 36;
    int cardW = std::clamp((cardAreaW - (cols - 1) * cardGap) / cols, 200, 280);
    int cardH = 68;
    int cardStartX = surfaceLeft + 16;
    int cardStartY = contentTop + 48;
    int numRows = (static_cast<int>(quickCards_.size()) + cols - 1) / cols;

    for (size_t i = 0; i < quickCards_.size(); ++i) {
        int col = static_cast<int>(i % cols);
        int row = static_cast<int>(i / cols);
        int cx = cardStartX + col * (cardW + cardGap);
        int cy = cardStartY + row * (cardH + 12);

        quickCards_[i].rect = RECT{cx, cy, cx + cardW, cy + cardH};

        bool isHov = (static_cast<int>(i) == hoverCard_);
        COLORREF cardBg = isHov ? RGB(42, 42, 42) : RGB(33, 33, 33);
        COLORREF cardBorder = isHov ? RGB(60, 60, 60) : RGB(46, 46, 46);
        drawRoundedRect(hdc, quickCards_[i].rect, 8, cardBg, cardBorder);

        RECT badgeRc{cx + 10, cy + 11, cx + 56, cy + 57};
        drawRoundedRect(hdc, badgeRc, 6, RGB(38, 38, 38), RGB(48, 48, 48));

        SelectObject(hdc, fontIcons_);
        SetTextColor(hdc, quickCards_[i].badgeColor);
        DrawTextW(hdc, quickCards_[i].iconGlyph.c_str(), 1, &badgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontBold_);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT nameRc{cx + 64, cy + 12, cx + cardW - 24, cy + 36};
        DrawTextW(hdc, quickCards_[i].name.c_str(), -1, &nameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(150, 150, 150));
        RECT subRc{cx + 64, cy + 37, cx + cardW - 24, cy + 57};
        DrawTextW(hdc, quickCards_[i].subtext.c_str(), -1, &subRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, RGB(120, 120, 120));
        RECT pinRc{cx + cardW - 24, cy + 37, cx + cardW - 8, cy + 57};
        DrawTextW(hdc, L"\uE718", 1, &pinRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Section 2: Section Tabs (Recent, Favorites, Shared)
    int recentTop = cardStartY + numRows * (cardH + 12) + 16;
    RECT recentChevRc{surfaceLeft + 16, recentTop + 4, surfaceLeft + 32, recentTop + 30};
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(180, 180, 180));
    DrawTextW(hdc, L"\uE70D", 1, &recentChevRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    int pillX = surfaceLeft + 36;
    auto drawSectionPill = [&](RECT& outRc, const wchar_t* iconGlyph, const wchar_t* text, bool isSelected) {
        int w = static_cast<int>(wcslen(text) * 9 + 42);
        outRc = RECT{pillX, recentTop, pillX + w, recentTop + 32};

        COLORREF fillCol = isSelected ? RGB(224, 187, 254) : RGB(28, 28, 28);
        COLORREF borderCol = isSelected ? RGB(224, 187, 254) : RGB(48, 48, 48);
        drawRoundedRect(hdc, outRc, 16, fillCol, borderCol);

        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, isSelected ? RGB(30, 20, 36) : RGB(160, 160, 160));
        RECT iRc{outRc.left + 10, outRc.top, outRc.left + 26, outRc.bottom};
        DrawTextW(hdc, iconGlyph, 1, &iRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, isSelected ? RGB(30, 20, 36) : RGB(180, 180, 180));
        RECT tRc{outRc.left + 28, outRc.top, outRc.right - 10, outRc.bottom};
        DrawTextW(hdc, text, -1, &tRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        pillX += w + 8;
    };

    drawSectionPill(pillRecentRc_, L"\uE823", L"Recent", homeSection_ == 0);
    drawSectionPill(pillFavRc_, L"\uE734", L"Favorites", homeSection_ == 1);
    drawSectionPill(pillSharedRc_, L"\uE716", L"Shared", homeSection_ == 2);

    int contentAreaTop = recentTop + 42;
    RECT contentAreaRc{surfaceLeft, contentAreaTop, effWidth, height - statusBarHeight_};

    // Shared View (Vector Illustration matching user screenshot)
    if (homeSection_ == 2) {
        int midX = (surfaceLeft + effWidth) / 2;
        int midY = (contentAreaTop + height - statusBarHeight_) / 2 - 40;

        // Document sheet tilted slightly
        POINT docPts[4] = {
            { midX - 45, midY - 30 },
            { midX + 10, midY - 60 },
            { midX + 45, midY + 15 },
            { midX - 10, midY + 45 }
        };
        HBRUSH docBrush = CreateSolidBrush(RGB(240, 240, 245));
        HPEN docPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 210));
        SelectObject(hdc, docBrush);
        SelectObject(hdc, docPen);
        Polygon(hdc, docPts, 4);
        DeleteObject(docBrush);
        DeleteObject(docPen);

        // Document text lines (grey bars)
        HPEN linePen = CreatePen(PS_SOLID, 3, RGB(180, 185, 195));
        SelectObject(hdc, linePen);
        MoveToEx(hdc, midX - 25, midY - 10, nullptr);
        LineTo(hdc, midX + 15, midY - 30);
        MoveToEx(hdc, midX - 20, midY + 5, nullptr);
        LineTo(hdc, midX + 20, midY - 15);
        MoveToEx(hdc, midX - 15, midY + 20, nullptr);
        LineTo(hdc, midX + 10, midY + 5);
        DeleteObject(linePen);

        // Cyan loop arrow wrapping around
        HPEN arrowPen = CreatePen(PS_SOLID, 4, RGB(0, 178, 220));
        SelectObject(hdc, arrowPen);
        Arc(hdc, midX - 15, midY - 20, midX + 55, midY + 50, midX + 55, midY + 15, midX + 10, midY + 45);
        DeleteObject(arrowPen);

        // Cyan Avatar circle (user person)
        HBRUSH userBrush = CreateSolidBrush(RGB(0, 180, 230));
        SelectObject(hdc, userBrush);
        SelectObject(hdc, GetStockObject(NULL_PEN));
        // Head
        Ellipse(hdc, midX + 35, midY - 25, midX + 53, midY - 7);
        // Torso
        Chord(hdc, midX + 27, midY - 5, midX + 61, midY + 30, midX + 61, midY + 12, midX + 27, midY + 12);
        DeleteObject(userBrush);

        // Heading: "No shared files yet"
        SelectObject(hdc, fontTitle_);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT sharedTitleRc{surfaceLeft + 20, midY + 65, effWidth - 20, midY + 95};
        DrawTextW(hdc, L"No shared files yet", -1, &sharedTitleRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Subtitle: "When a file is shared with your account, you'll be able to quickly access it here."
        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, RGB(160, 160, 160));
        RECT sharedSubRc{surfaceLeft + 40, midY + 98, effWidth - 40, midY + 135};
        DrawTextW(hdc, L"When a file is shared with your account, you'll be able to quickly access it here.", -1, &sharedSubRc, DT_CENTER | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
        return;
    }

    // Favorites View (Empty State if empty)
    if (homeSection_ == 1 && favorites_.empty()) {
        paintEmptyState(hdc, contentAreaRc, L"\uE734", RGB(255, 200, 59),
            L"No favorites yet",
            L"Right-click any file and select 'Add to Favorites' to pin items here for quick access.");
        return;
    }

    // Section 3: Recent Items Table Headers (Title Case Parity)
    int tableHeaderTop = contentAreaTop;
    RECT hdrRc{surfaceLeft, tableHeaderTop, effWidth, tableHeaderTop + headerHeight_};
    HBRUSH hdrBrush = CreateSolidBrush(RGB(22, 22, 22));
    FillRect(hdc, &hdrRc, hdrBrush);
    DeleteObject(hdrBrush);

    SelectObject(hdc, fontSmall_);
    SetTextColor(hdc, RGB(150, 150, 150));

    int colStart = surfaceLeft + 36 + (showCheckBoxes_ ? 28 : 0);
    RECT colNameRc{colStart, tableHeaderTop, colStart + colNameWidth_, tableHeaderTop + headerHeight_};
    RECT colDateRc{colNameRc.right, tableHeaderTop, colNameRc.right + colDateWidth_, tableHeaderTop + headerHeight_};
    RECT colLocRc{colDateRc.right, tableHeaderTop, colDateRc.right + colLocWidth_, tableHeaderTop + headerHeight_};
    RECT colAccRc{colLocRc.right, tableHeaderTop, colLocRc.right + colAccountWidth_, tableHeaderTop + headerHeight_};
    RECT colActRc{colAccRc.right, tableHeaderTop, effWidth - 20, tableHeaderTop + headerHeight_};

    DrawTextW(hdc, L"Name", 4, &colNameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawTextW(hdc, L"Date accessed", 13, &colDateRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawTextW(hdc, L"File location", 13, &colLocRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawTextW(hdc, L"Account", 7, &colAccRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawTextW(hdc, L"Activity", 8, &colActRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // Rows
    int rowY = tableHeaderTop + headerHeight_;
    int surfaceBottom = height - statusBarHeight_;
    int visibleRows = (surfaceBottom - rowY) / rowHeight_ + 2;

    SelectObject(hdc, fontRegular_);
    for (int i = 0; i < visibleRows; ++i) {
        int r = scrollOffset_ + i;
        if (r >= static_cast<int>(filteredIndices_.size())) break;

        const auto& item = allItems_[filteredIndices_[r]];
        RECT rowRc{surfaceLeft + 4, rowY + i * rowHeight_, effWidth - 8, rowY + (i + 1) * rowHeight_};

        bool isSel = (r == selectedRow_);
        bool isHov = (r == hoverRow_);

        if (isSel) {
            drawRoundedRect(hdc, rowRc, 6, RGB(30, 51, 71), RGB(40, 68, 95));
            RECT pill{rowRc.left + 1, rowRc.top + 4, rowRc.left + 4, rowRc.bottom - 4};
            HBRUSH acc = CreateSolidBrush(RGB(96, 205, 255));
            FillRect(hdc, &pill, acc);
            DeleteObject(acc);
        } else if (isHov) {
            drawRoundedRect(hdc, rowRc, 6, RGB(36, 36, 36), RGB(44, 44, 44));
        }

        // Item check boxes ("Object boxes")
        if (showCheckBoxes_ || isHov) {
            RECT cbRc{surfaceLeft + 10, rowRc.top + (rowHeight_ - 18) / 2, surfaceLeft + 28, rowRc.top + (rowHeight_ - 18) / 2 + 18};
            bool isChecked = (static_cast<size_t>(r) < checkedRows_.size()) && checkedRows_[r];
            drawRoundedRect(hdc, cbRc, 4, isChecked ? RGB(96, 205, 255) : (isHov ? RGB(45, 45, 45) : RGB(36, 36, 36)),
                isChecked ? RGB(96, 205, 255) : (isHov ? RGB(90, 90, 90) : RGB(60, 60, 60)));
            if (isChecked) {
                SelectObject(hdc, fontSmall_);
                SetTextColor(hdc, RGB(0, 0, 0));
                DrawTextW(hdc, L"✓", 1, &cbRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }

        HICON hIcon = getFileIcon(item.name, item.isDirectory, item.extension);
        if (hIcon) {
            int iconLeft = surfaceLeft + 12 + (showCheckBoxes_ || isHov ? 28 : 0);
            DrawIconEx(hdc, iconLeft, rowRc.top + (rowHeight_ - 20) / 2, hIcon, 20, 20, 0, nullptr, DI_NORMAL);
            DestroyIcon(hIcon);
        }

        // File Name (with extension toggle applied)
        std::wstring displayName = item.name;
        if (!showExtensions_ && !item.isDirectory && !item.extension.empty()) {
            size_t dotPos = displayName.find_last_of(L'.');
            if (dotPos != std::wstring::npos) displayName = displayName.substr(0, dotPos);
        }

        SetTextColor(hdc, RGB(255, 255, 255));
        RECT nameCell{colStart, rowRc.top, colStart + colNameWidth_ - 10, rowRc.bottom};
        DrawTextW(hdc, displayName.c_str(), -1, &nameCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Hover quick action buttons (Open Folder, Star, More)
        if (isHov) {
            int btnX = colStart + static_cast<int>(displayName.size() * 9 + 26);
            if (btnX < colNameRc.right - 80) {
                SelectObject(hdc, fontIconsSmall_);
                SetTextColor(hdc, RGB(180, 180, 180));
                RECT fRc{btnX, rowRc.top + 6, btnX + 22, rowRc.bottom - 6};
                DrawTextW(hdc, L"\uE838", 1, &fRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                RECT sRc{btnX + 24, rowRc.top + 6, btnX + 46, rowRc.bottom - 6};
                DrawTextW(hdc, L"\uE734", 1, &sRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                RECT mRc{btnX + 48, rowRc.top + 6, btnX + 70, rowRc.bottom - 6};
                DrawTextW(hdc, L"\uE712", 1, &mRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // Hover floating tooltip pill: "Path: Downloads"
            RECT tipRc{colLocRc.left - 4, rowRc.top + 6, colLocRc.left + 100, rowRc.bottom - 6};
            drawRoundedRect(hdc, tipRc, 6, RGB(38, 38, 38), RGB(70, 70, 70));
            SelectObject(hdc, fontSmall_);
            SetTextColor(hdc, RGB(220, 220, 220));
            DrawTextW(hdc, L"Path: Downloads", -1, &tipRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(160, 160, 160));
        RECT dateCell{colNameRc.right, rowRc.top, colNameRc.right + colDateWidth_, rowRc.bottom};
        std::wstring dt = formatTime(item.mtime);
        DrawTextW(hdc, dt.c_str(), -1, &dateCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT locCell{colDateRc.right, rowRc.top, colLocRc.right, rowRc.bottom};
        DrawTextW(hdc, item.location.c_str(), -1, &locCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
}

void MainWindow::paintTilesView(HDC hdc, int width, int height) {
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    int surfaceLeft = sidebarWidth_;
    int rightMargin = (showDetailsPane_ ? detailsPaneWidth_ : (showPreviewPane_ ? previewPaneWidth_ : 0));
    int effWidth = width - rightMargin;

    paintFilterPills(hdc, width, contentTop);

    int startY = contentTop + filterBarHeight_ + 10;
    int tileW = 300;
    int tileH = 68;
    int cols = std::max(1, (effWidth - surfaceLeft - 20) / (tileW + 14));

    SelectObject(hdc, fontRegular_);
    SetBkMode(hdc, TRANSPARENT);

    for (size_t i = 0; i < filteredIndices_.size(); ++i) {
        int r = static_cast<int>(i);
        int col = r % cols;
        int row = r / cols;
        int tx = surfaceLeft + 14 + col * (tileW + 14);
        int ty = startY + row * (tileH + 12) - scrollOffset_ * 20;

        if (ty + tileH < startY || ty > height - statusBarHeight_) continue;

        RECT tileRc{tx, ty, tx + tileW, ty + tileH};
        bool isSel = (r == selectedRow_);
        bool isHov = (r == hoverRow_);

        COLORREF bgCol = isSel ? RGB(30, 51, 71) : (isHov ? RGB(38, 38, 38) : RGB(30, 30, 30));
        COLORREF bdrCol = isSel ? RGB(96, 205, 255) : (isHov ? RGB(55, 55, 55) : RGB(42, 42, 42));
        drawRoundedRect(hdc, tileRc, 8, bgCol, bdrCol);

        const auto& item = allItems_[filteredIndices_[i]];

        // Real Shell Icon
        HICON hIcon = getFileIcon(item.name, item.isDirectory, item.extension);
        if (hIcon) {
            DrawIconEx(hdc, tx + 12, ty + (tileH - 40) / 2, hIcon, 40, 40, 0, nullptr, DI_NORMAL);
            DestroyIcon(hIcon);
        }

        // Title
        SelectObject(hdc, fontBold_);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT nameRc{tx + 58, ty + 12, tx + tileW - 12, ty + 34};
        DrawTextW(hdc, item.name.c_str(), -1, &nameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Subtitle (Type & Size)
        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(150, 150, 150));
        RECT subRc{tx + 58, ty + 36, tx + tileW - 12, ty + 56};
        std::wstring subtext = item.isDirectory ? L"File folder" : (item.extension + L" File  ·  " + formatSize(item.size));
        DrawTextW(hdc, subtext.c_str(), -1, &subRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
}

void MainWindow::paintDirectoryView(HDC hdc, int width, int height) {
    if (viewMode_ == ViewMode::Tiles) {
        paintTilesView(hdc, width, height);
        return;
    }

    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    int surfaceLeft = sidebarWidth_;
    int rightMargin = (showDetailsPane_ ? detailsPaneWidth_ : (showPreviewPane_ ? previewPaneWidth_ : 0));
    int effWidth = width - rightMargin;

    SetBkMode(hdc, TRANSPARENT);

    // 1. Instant 1-Click Quick Filter Bar
    paintFilterPills(hdc, width, contentTop);

    // 2. Interactive Column Headers (with Sort Arrows)
    int tableHeaderTop = contentTop + filterBarHeight_;
    RECT hdrRc{surfaceLeft, tableHeaderTop, effWidth, tableHeaderTop + headerHeight_};
    HBRUSH hdrBrush = CreateSolidBrush(RGB(26, 26, 26));
    FillRect(hdc, &hdrRc, hdrBrush);
    DeleteObject(hdrBrush);

    SelectObject(hdc, fontSmall_);
    SetTextColor(hdc, RGB(140, 140, 140));

    int colStart = surfaceLeft + 36 + (showCheckBoxes_ ? 28 : 0);
    RECT colNameRc{colStart, tableHeaderTop, colStart + colNameWidth_, tableHeaderTop + headerHeight_};
    RECT colDateRc{colNameRc.right, tableHeaderTop, colNameRc.right + colDateWidth_, tableHeaderTop + headerHeight_};
    RECT colTypeRc{colDateRc.right, tableHeaderTop, colDateRc.right + colTypeWidth_, tableHeaderTop + headerHeight_};
    RECT colSizeRc{colTypeRc.right, tableHeaderTop, colTypeRc.right + colSizeWidth_, tableHeaderTop + headerHeight_};

    std::wstring nameHdr = L"NAME" + std::wstring(sortCol_ == SortColumn::Name ? (sortAscending_ ? L" ▲" : L" ▼") : L"");
    std::wstring dateHdr = L"DATE MODIFIED" + std::wstring(sortCol_ == SortColumn::DateModified ? (sortAscending_ ? L" ▲" : L" ▼") : L"");
    std::wstring typeHdr = L"TYPE" + std::wstring(sortCol_ == SortColumn::Type ? (sortAscending_ ? L" ▲" : L" ▼") : L"");
    std::wstring sizeHdr = L"SIZE" + std::wstring(sortCol_ == SortColumn::Size ? (sortAscending_ ? L" ▲" : L" ▼") : L"");

    DrawTextW(hdc, nameHdr.c_str(), -1, &colNameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawTextW(hdc, dateHdr.c_str(), -1, &colDateRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawTextW(hdc, typeHdr.c_str(), -1, &colTypeRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawTextW(hdc, sizeHdr.c_str(), -1, &colSizeRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(40, 40, 40));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, surfaceLeft, tableHeaderTop + headerHeight_ - 1, nullptr);
    LineTo(hdc, effWidth, tableHeaderTop + headerHeight_ - 1);
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
        RECT rowRc{surfaceLeft + 4, rowY + i * rowHeight_, effWidth - 8, rowY + (i + 1) * rowHeight_};

        bool isSel = (r == selectedRow_);
        bool isHov = (r == hoverRow_);

        if (isSel) {
            drawRoundedRect(hdc, rowRc, 6, RGB(30, 51, 71), RGB(40, 68, 95));
            RECT pill{rowRc.left + 1, rowRc.top + 4, rowRc.left + 4, rowRc.bottom - 4};
            HBRUSH acc = CreateSolidBrush(RGB(96, 205, 255));
            FillRect(hdc, &pill, acc);
            DeleteObject(acc);
        } else if (isHov) {
            drawRoundedRect(hdc, rowRc, 6, RGB(36, 36, 36), RGB(44, 44, 44));
        }

        // Item check boxes ("Object boxes")
        if (showCheckBoxes_) {
            RECT cbRc{surfaceLeft + 10, rowRc.top + (rowHeight_ - 18) / 2, surfaceLeft + 28, rowRc.top + (rowHeight_ - 18) / 2 + 18};
            bool isChecked = (static_cast<size_t>(r) < checkedRows_.size()) && checkedRows_[r];
            drawRoundedRect(hdc, cbRc, 4, isChecked ? RGB(96, 205, 255) : RGB(36, 36, 36),
                isChecked ? RGB(96, 205, 255) : RGB(80, 80, 80));
            if (isChecked) {
                SelectObject(hdc, fontSmall_);
                SetTextColor(hdc, RGB(0, 0, 0));
                DrawTextW(hdc, L"✓", 1, &cbRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }

        // Real Windows Shell File Icon
        HICON hIcon = getFileIcon(item.name, item.isDirectory, item.extension);
        if (hIcon) {
            int iconLeft = surfaceLeft + 12 + (showCheckBoxes_ ? 28 : 0);
            DrawIconEx(hdc, iconLeft, rowRc.top + (rowHeight_ - 20) / 2, hIcon, 20, 20, 0, nullptr, DI_NORMAL);
            DestroyIcon(hIcon);
        }

        // File Name (with extension toggle applied)
        std::wstring displayName = item.name;
        if (!showExtensions_ && !item.isDirectory && !item.extension.empty()) {
            size_t dotPos = displayName.find_last_of(L'.');
            if (dotPos != std::wstring::npos) displayName = displayName.substr(0, dotPos);
        }

        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT nameCell{colStart, rowRc.top, colStart + colNameWidth_ - 10, rowRc.bottom};
        DrawTextW(hdc, displayName.c_str(), -1, &nameCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(160, 160, 160));
        RECT dateCell{colNameRc.right, rowRc.top, colNameRc.right + colDateWidth_, rowRc.bottom};
        std::wstring dt = formatTime(item.mtime);
        DrawTextW(hdc, dt.c_str(), -1, &dateCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT typeCell{colDateRc.right, rowRc.top, colDateRc.right + colTypeWidth_, rowRc.bottom};
        std::wstring typeStr = item.isDirectory ? L"File folder" : (item.extension.empty() ? L"File" : item.extension + L" File");
        DrawTextW(hdc, typeStr.c_str(), -1, &typeCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT sizeCell{colTypeRc.right, rowRc.top, colTypeRc.right + colSizeWidth_, rowRc.bottom};
        std::wstring sz = item.isDirectory ? L"" : formatSize(item.size);
        DrawTextW(hdc, sz.c_str(), -1, &sizeCell, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
}

void MainWindow::paintDetailsPane(HDC hdc, int width, int height) {
    if (!showDetailsPane_) return;

    int paneX = width - detailsPaneWidth_;
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    RECT paneRc{paneX, contentTop, width, height - statusBarHeight_};

    HBRUSH bgBrush = CreateSolidBrush(RGB(25, 25, 25));
    FillRect(hdc, &paneRc, bgBrush);
    DeleteObject(bgBrush);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(42, 42, 42));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, paneX, contentTop, nullptr);
    LineTo(hdc, paneX, height - statusBarHeight_);
    DeleteObject(borderPen);

    SetBkMode(hdc, TRANSPARENT);

    // Header
    RECT titleRc{paneX + 18, contentTop + 14, paneX + 180, contentTop + 40};
    SelectObject(hdc, fontTitle_);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, L"Details", 7, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // Close 'X' Button
    RECT closeRc{width - 34, contentTop + 14, width - 12, contentTop + 36};
    SelectObject(hdc, fontRegular_);
    SetTextColor(hdc, RGB(160, 160, 160));
    DrawTextW(hdc, L"×", 1, &closeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (selectedRow_ < 0 || selectedRow_ >= static_cast<int>(filteredIndices_.size())) {
        RECT msgRc{paneX + 24, contentTop + 90, width - 24, contentTop + 160};
        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, RGB(140, 140, 140));
        DrawTextW(hdc, L"Select a file or folder to view its detailed properties and metadata.", -1, &msgRc, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        return;
    }

    const auto& item = allItems_[filteredIndices_[selectedRow_]];

    // Thumbnail / Icon (Large 64x64px high-contrast Shell Icon)
    HICON hIcon = getFileIcon(item.name, item.isDirectory, item.extension);
    if (hIcon) {
        DrawIconEx(hdc, paneX + (detailsPaneWidth_ - 64) / 2, contentTop + 55, hIcon, 64, 64, 0, nullptr, DI_NORMAL);
        DestroyIcon(hIcon);
    }

    // File Name
    SelectObject(hdc, fontBold_);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT fnRc{paneX + 16, contentTop + 130, width - 16, contentTop + 165};
    DrawTextW(hdc, item.name.c_str(), -1, &fnRc, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);

    // Properties List (32px line spacing for comfortable readability)
    int propY = contentTop + 180;
    auto drawProperty = [&](const wchar_t* label, const std::wstring& value) {
        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(140, 140, 140));
        RECT lRc{paneX + 18, propY, paneX + 115, propY + 24};
        DrawTextW(hdc, label, -1, &lRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, RGB(230, 230, 230));
        RECT vRc{paneX + 115, propY, width - 18, propY + 24};
        DrawTextW(hdc, value.c_str(), -1, &vRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        propY += 32;
    };

    drawProperty(L"Type", item.isDirectory ? L"File folder" : (item.extension + L" File"));
    drawProperty(L"Size", item.isDirectory ? L"—" : (formatSize(item.size) + L" (" + std::to_wstring(item.size) + L" bytes)"));
    drawProperty(L"Modified", formatTime(item.mtime));
    std::wstring loc = !item.fullPath.empty()
        ? std::filesystem::path(item.fullPath).parent_path().wstring()
        : (isHomeView_ ? item.location : currentPath_.wstring());
    drawProperty(L"Location", loc);

    std::wstring attrStr;
    if (item.attributes & FILE_ATTRIBUTE_READONLY) attrStr += L"R ";
    if (item.attributes & FILE_ATTRIBUTE_HIDDEN) attrStr += L"H ";
    if (item.attributes & FILE_ATTRIBUTE_SYSTEM) attrStr += L"S ";
    if (item.attributes & FILE_ATTRIBUTE_ARCHIVE) attrStr += L"A ";
    drawProperty(L"Attributes", attrStr.empty() ? L"Normal" : attrStr);
}

void MainWindow::paintPreviewPane(HDC hdc, int width, int height) {
    if (!showPreviewPane_) return;

    int paneX = width - previewPaneWidth_;
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    RECT paneRc{paneX, contentTop, width, height - statusBarHeight_};

    HBRUSH bgBrush = CreateSolidBrush(RGB(25, 25, 25));
    FillRect(hdc, &paneRc, bgBrush);
    DeleteObject(bgBrush);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(42, 42, 42));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, paneX, contentTop, nullptr);
    LineTo(hdc, paneX, height - statusBarHeight_);
    DeleteObject(borderPen);

    SetBkMode(hdc, TRANSPARENT);

    // Header
    RECT titleRc{paneX + 18, contentTop + 14, paneX + 180, contentTop + 40};
    SelectObject(hdc, fontTitle_);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, L"Preview", 7, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // Close 'X' Button
    RECT closeRc{width - 34, contentTop + 14, width - 12, contentTop + 36};
    SelectObject(hdc, fontRegular_);
    SetTextColor(hdc, RGB(160, 160, 160));
    DrawTextW(hdc, L"×", 1, &closeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (selectedRow_ < 0 || selectedRow_ >= static_cast<int>(filteredIndices_.size())) {
        RECT msgRc{paneX + 24, contentTop + 90, width - 24, contentTop + 160};
        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, RGB(140, 140, 140));
        DrawTextW(hdc, L"Select a file to preview its contents.", -1, &msgRc, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        return;
    }

    const auto& item = allItems_[filteredIndices_[selectedRow_]];
    std::filesystem::path fullPath = !item.fullPath.empty()
        ? std::filesystem::path(item.fullPath)
        : (isHomeView_ ? std::filesystem::path(item.location) / item.name : currentPath_ / item.name);

    // Text File Preview (.txt, .cpp, .h, .json, .md, .py, etc.)
    std::wstring ext = toLower(item.extension);
    if (!item.isDirectory && (ext == L"txt" || ext == L"cpp" || ext == L"h" || ext == L"json" || ext == L"md" || ext == L"py" || ext == L"cmake" || ext == L"xml" || ext == L"log")) {
        std::ifstream file(fullPath);
        if (file.is_open()) {
            std::string line;
            int textY = contentTop + 55;
            SelectObject(hdc, fontSmall_);
            SetTextColor(hdc, RGB(225, 225, 225));

            int lineCount = 0;
            while (std::getline(file, line) && lineCount++ < 35 && textY < height - statusBarHeight_ - 24) {
                std::wstring wLine(line.begin(), line.end());
                RECT lineRc{paneX + 18, textY, width - 18, textY + 22};
                DrawTextW(hdc, wLine.c_str(), -1, &lineRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                textY += 22;
            }
            return;
        }
    }

    // Fallback info preview
    RECT msgRc{paneX + 24, contentTop + 90, width - 24, contentTop + 160};
    SelectObject(hdc, fontRegular_);
    SetTextColor(hdc, RGB(160, 160, 160));
    std::wstring prevMsg = L"No preview available for " + item.name;
    DrawTextW(hdc, prevMsg.c_str(), -1, &msgRc, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
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

    RECT textRc{16, height - statusBarHeight_, width - 100, height};
    DrawTextW(hdc, statusBuf, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
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

    // 4. Sidebar (Expandable Tree Nodes)
    paintSidebar(memDC, height);

    // 5. Main Surface (Home View vs Directory View)
    if (isHomeView_) {
        paintHomeView(memDC, width, height);
    } else {
        paintDirectoryView(memDC, width, height);
    }

    // 6. Right-Side Panes (Details Pane or Preview Pane)
    if (showDetailsPane_) {
        paintDetailsPane(memDC, width, height);
    } else if (showPreviewPane_) {
        paintPreviewPane(memDC, width, height);
    }

    // 7. Status Bar
    paintStatusBar(memDC, width, height);

    // Blit to screen DC
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

} // namespace gx::app
