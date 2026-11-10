#include "main_window.h"

#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <thumbcache.h>
#include <dwmapi.h>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <chrono>
#include <unordered_set>

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
constexpr UINT_PTR kCmdViewLargeIcons = 3023; // Large Icons (Photos/Grid)
constexpr UINT_PTR kCmdViewGrid = 3023;       // Alias
constexpr UINT_PTR kCmdToggleCompact = 3024;
constexpr UINT_PTR kCmdToggleNavPane = 3025;
constexpr UINT_PTR kCmdToggleDetailsPane = 3026;
constexpr UINT_PTR kCmdTogglePreviewPane = 3027;
constexpr UINT_PTR kCmdToggleCheckBoxes = 3028;
constexpr UINT_PTR kCmdToggleExtensions = 3029;
constexpr UINT_PTR kCmdToggleHidden = 3030;
constexpr UINT_PTR kCmdViewMediumIcons = 3038;

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
constexpr UINT_PTR kCmdDiskCleanup = 3090;
constexpr UINT_PTR kCmdStorageSettings = 3091;
constexpr UINT_PTR kCmdEmptyRecycleBin = 3092;
constexpr UINT_PTR kCmdOpenPowershell = 3093;
constexpr UINT_PTR kCmdFilterLargeFiles = 3094;
constexpr UINT_PTR kCmdFilterToday = 3095;

COLORREF toColorRef(const ui::ColorRgba& c) {
    return RGB(c.r, c.g, c.b);
}

std::wstring getKnownPath(REFKNOWNFOLDERID rfid) {
    PWSTR pPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(rfid, KF_FLAG_DONT_VERIFY, nullptr, &pPath))) {
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

std::wstring toUpper(std::wstring_view s) {
    std::wstring out(s);
    std::transform(out.begin(), out.end(), out.begin(), ::towupper);
    return out;
}

bool isWordDelimiter(wchar_t c) {
    return c == L' ' || c == L'_' || c == L'-' || c == L'.' || c == L':' || c == L';' || c == L',' ||
           c == L'/' || c == L'\\' || c == L'(' || c == L')' || c == L'[' || c == L']' || c == L'{' || c == L'}';
}

std::vector<std::wstring> tokenizeQuery(std::wstring_view text) {
    std::vector<std::wstring> tokens;
    std::wstring cur;
    for (wchar_t c : text) {
        if (isWordDelimiter(c)) {
            if (!cur.empty()) {
                if (cur.length() >= 2) tokens.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(static_cast<wchar_t>(towlower(c)));
        }
    }
    if (cur.length() >= 2) tokens.push_back(cur);
    return tokens;
}

uint32_t damerauLevenshtein(std::wstring_view s1, std::wstring_view s2) {
    const size_t len1 = s1.size(), len2 = s2.size();
    if (len1 == 0) return static_cast<uint32_t>(len2);
    if (len2 == 0) return static_cast<uint32_t>(len1);
    std::vector<uint32_t> col(len2 + 1);
    for (size_t y = 0; y <= len2; ++y) col[y] = static_cast<uint32_t>(y);
    for (size_t x = 1; x <= len1; ++x) {
        col[0] = static_cast<uint32_t>(x);
        uint32_t lastDiag = static_cast<uint32_t>(x - 1);
        for (size_t y = 1; y <= len2; ++y) {
            uint32_t oldCol = col[y];
            uint32_t cost = (s1[x - 1] == s2[y - 1]) ? 0 : 1;
            col[y] = std::min({ col[y] + 1, col[y - 1] + 1, lastDiag + cost });
            lastDiag = oldCol;
        }
    }
    return col[len2];
}

struct MatchResult {
    int score{0};
    bool isExact{false};
    bool isApproximate{false};
    std::wstring reason;
};

MatchResult scoreCandidate(
    std::wstring_view candName,
    std::wstring_view rawQuery,
    const std::vector<std::wstring>& qTokens,
    const std::wstring& qLower,
    const std::wstring& qCompact)
{
    MatchResult out;
    if (candName.empty() || rawQuery.empty()) return out;

    std::wstring candLower = toLower(candName);

    // 1. Direct continuous substring match
    size_t subPos = candLower.find(qLower);
    if (subPos != std::wstring::npos) {
        out.score = 1000 + static_cast<int>(qLower.length() * 10);
        if (subPos == 0) out.score += 50; // Prefix bonus
        out.isExact = true;
        out.reason = L"Exact substring match";
        return out;
    }

    auto candTokens = tokenizeQuery(candName);
    std::wstring candCompact;
    for (const auto& t : candTokens) candCompact += t;

    // 2. Compact sequence match (e.g. "batmanark" in "batmanark.exe")
    if (!qCompact.empty() && candCompact.find(qCompact) != std::wstring::npos) {
        out.score = 850;
        out.isExact = true;
        out.reason = L"Compact sequence match";
        return out;
    }

    int matchedTokens = 0;
    std::vector<std::wstring> matchedList;

    for (const auto& qt : qTokens) {
        bool tokMatched = false;

        // Substring inside candidate lowercase
        if (candLower.find(qt) != std::wstring::npos) {
            out.score += 200;
            tokMatched = true;
            matchedList.push_back(qt);
        } else {
            // Check prefix or partial match against candidate tokens
            for (const auto& ct : candTokens) {
                if ((ct.length() >= 3 && qt.find(ct) != std::wstring::npos) ||
                    (qt.length() >= 3 && ct.find(qt) != std::wstring::npos)) {
                    out.score += 150;
                    tokMatched = true;
                    matchedList.push_back(ct);
                    break;
                } else if (qt.length() >= 4 && ct.length() >= 4 && damerauLevenshtein(qt, ct) <= 2) {
                    out.score += 90;
                    tokMatched = true;
                    matchedList.push_back(ct);
                    break;
                }
            }
        }

        if (tokMatched) {
            matchedTokens++;
        }
    }

    // Subsequence match on compacted candidate (e.g. "BATMANARK" vs "batman" + "arkham")
    if (matchedTokens < static_cast<int>(qTokens.size()) && candCompact.length() >= 3) {
        for (const auto& qt : qTokens) {
            if (qt.length() >= 3) {
                std::wstring prefix3 = qt.substr(0, std::min<size_t>(qt.length(), 3));
                if (candCompact.find(prefix3) != std::wstring::npos) {
                    bool already = false;
                    for (const auto& m : matchedList) {
                        if (m == qt || m == prefix3) { already = true; break; }
                    }
                    if (!already) {
                        out.score += 120;
                        matchedTokens++;
                        matchedList.push_back(qt);
                    }
                }
            }
        }
    }

    if (matchedTokens >= static_cast<int>(qTokens.size()) && !qTokens.empty()) {
        out.score += 300;
        out.isExact = true;
        out.reason = L"All search terms matched";
    } else if (matchedTokens > 0) {
        out.isApproximate = true;
        std::wstring r = L"Matches: ";
        for (size_t i = 0; i < matchedList.size(); ++i) {
            if (i > 0) r += L", ";
            r += matchedList[i];
        }
        out.reason = r;
    }

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

// Apple Fluid Motion: Exact Rubberband Formula (WWDC Designing Fluid Interfaces / apple-design §9)
static inline double appleRubberband(double overshoot, double dimension, double constant = 0.55) {
    if (dimension <= 0.0) dimension = 100.0;
    return (overshoot * dimension * constant) / (dimension + constant * std::abs(overshoot));
}

bool getImageDimensions(const std::wstring& path, uint32_t& outW, uint32_t& outH) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    unsigned char buf[32];
    f.read(reinterpret_cast<char*>(buf), sizeof(buf));
    if (f.gcount() < 24) return false;

    // PNG: signature 89 50 4E 47
    if (buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') {
        outW = (static_cast<uint32_t>(buf[16]) << 24) | (static_cast<uint32_t>(buf[17]) << 16) | (static_cast<uint32_t>(buf[18]) << 8) | buf[19];
        outH = (static_cast<uint32_t>(buf[20]) << 24) | (static_cast<uint32_t>(buf[21]) << 16) | (static_cast<uint32_t>(buf[22]) << 8) | buf[23];
        return (outW > 0 && outH > 0);
    }
    // BMP: signature 'BM'
    if (buf[0] == 'B' && buf[1] == 'M' && f.gcount() >= 26) {
        outW = static_cast<uint32_t>(buf[18]) | (static_cast<uint32_t>(buf[19]) << 8) | (static_cast<uint32_t>(buf[20]) << 16) | (static_cast<uint32_t>(buf[21]) << 24);
        outH = static_cast<uint32_t>(buf[22]) | (static_cast<uint32_t>(buf[23]) << 8) | (static_cast<uint32_t>(buf[24]) << 16) | (static_cast<uint32_t>(buf[25]) << 24);
        return (outW > 0 && outH > 0);
    }
    // JPEG: signature FF D8
    if (buf[0] == 0xFF && buf[1] == 0xD8) {
        f.seekg(2, std::ios::beg);
        unsigned char marker[4];
        while (f.read(reinterpret_cast<char*>(marker), 4)) {
            if (marker[0] != 0xFF) break;
            if (marker[1] == 0xC0 || marker[1] == 0xC1 || marker[1] == 0xC2) {
                unsigned char sizeBuf[5];
                if (f.read(reinterpret_cast<char*>(sizeBuf), 5)) {
                    outH = (static_cast<uint32_t>(sizeBuf[1]) << 8) | sizeBuf[2];
                    outW = (static_cast<uint32_t>(sizeBuf[3]) << 8) | sizeBuf[4];
                    return (outW > 0 && outH > 0);
                }
                break;
            } else {
                uint16_t len = (static_cast<uint16_t>(marker[2]) << 8) | marker[3];
                if (len < 2) break;
                f.seekg(len - 2, std::ios::cur);
            }
        }
    }
    return false;
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

    {
        std::ofstream ofs("C:\\os fileX\\build\\godxplr_hwnd.txt");
        if (ofs) ofs << reinterpret_cast<uintptr_t>(hwnd) << std::endl;
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

    fontCode_ = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Code");
    if (!fontCode_) {
        fontCode_ = CreateFontW(
            -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    }

    darkEditBrush_ = CreateSolidBrush(RGB(41, 41, 41));

    // Default tab
    tabs_.push_back({ L"Home", L"godxplr://home" });
}

MainWindow::~MainWindow() {
    clearThumbnailCache();
    if (fontRegular_) DeleteObject(fontRegular_);
    if (fontBold_) DeleteObject(fontBold_);
    if (fontTitle_) DeleteObject(fontTitle_);
    if (fontSmall_) DeleteObject(fontSmall_);
    if (fontIcons_) DeleteObject(fontIcons_);
    if (fontIconsLarge_) DeleteObject(fontIconsLarge_);
    if (fontIconsSmall_) DeleteObject(fontIconsSmall_);
    if (fontCode_) DeleteObject(fontCode_);
    if (darkEditBrush_) DeleteObject(darkEditBrush_);
}

void MainWindow::drawAppleGlassCard(HDC hdc, const RECT& rc, int radius, COLORREF topGrad, COLORREF botGrad, COLORREF borderColor, COLORREF topHighlight, bool isPressed) {
    RECT drawRc = rc;
    if (isPressed) {
        OffsetRect(&drawRc, 0, 1);
    }

    // 1. Soft Diffused Ambient Drop Shadow (Apple Depth Foundation §12)
    if (!isPressed) {
        RECT shadowRc{drawRc.left + 2, drawRc.top + 3, drawRc.right + 2, drawRc.bottom + 4};
        HBRUSH shadowBr = CreateSolidBrush(RGB(10, 12, 16));
        HPEN nullPen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
        HGDIOBJ oldBr = SelectObject(hdc, shadowBr);
        HGDIOBJ oldPn = SelectObject(hdc, nullPen);
        RoundRect(hdc, shadowRc.left, shadowRc.top, shadowRc.right, shadowRc.bottom, radius + 2, radius + 2);
        SelectObject(hdc, oldBr);
        SelectObject(hdc, oldPn);
        DeleteObject(shadowBr);
        DeleteObject(nullPen);
    }

    // 2. Translucent Glass Surface with Vertical Gradient
    int h = drawRc.bottom - drawRc.top;
    if (h > 0) {
        COLORREF fillBase = isPressed ? RGB(18, 22, 30) : botGrad;
        drawRoundedRect(hdc, drawRc, radius, fillBase, isPressed ? RGB(0, 113, 227) : borderColor);

        int gradSteps = std::min(h / 2, 28);
        for (int step = 1; step < gradSteps; ++step) {
            int y = drawRc.top + step;
            double t = 1.0 - static_cast<double>(step) / gradSteps;
            int r = static_cast<int>(GetRValue(botGrad) + (GetRValue(topGrad) - GetRValue(botGrad)) * t);
            int g = static_cast<int>(GetGValue(botGrad) + (GetGValue(topGrad) - GetGValue(botGrad)) * t);
            int b = static_cast<int>(GetBValue(botGrad) + (GetBValue(topGrad) - GetBValue(botGrad)) * t);
            if (isPressed) {
                r = std::max(0, r - 8);
                g = std::max(0, g - 8);
                b = std::max(0, b - 8);
            }
            int inset = (step < radius / 2) ? (radius / 2 - step) : 1;
            RECT lineRc{drawRc.left + inset + 1, y, drawRc.right - inset - 1, y + 1};
            HBRUSH lbr = CreateSolidBrush(RGB(r, g, b));
            FillRect(hdc, &lineRc, lbr);
            DeleteObject(lbr);
        }
    }

    // 3. Ambient Specular Top Edge Light Highlight Line (apple-design §12: light catching the material)
    if (!isPressed) {
        HPEN specPen = CreatePen(PS_SOLID, 1, topHighlight);
        HGDIOBJ oldP = SelectObject(hdc, specPen);
        MoveToEx(hdc, drawRc.left + radius / 2, drawRc.top + 1, nullptr);
        LineTo(hdc, drawRc.right - radius / 2, drawRc.top + 1);
        SelectObject(hdc, oldP);
        DeleteObject(specPen);
    }
}

void MainWindow::drawApplePill(HDC hdc, const RECT& rc, COLORREF fillCol, COLORREF borderCol, COLORREF topHighlight, bool isPressed) {
    RECT drawRc = rc;
    if (isPressed) {
        OffsetRect(&drawRc, 0, 1);
    }
    int radius = std::min(drawRc.right - drawRc.left, drawRc.bottom - drawRc.top);
    COLORREF activeFill = isPressed ? RGB(std::max(0, GetRValue(fillCol) - 15), std::max(0, GetGValue(fillCol) - 15), std::max(0, GetBValue(fillCol) - 15)) : fillCol;
    COLORREF activeBorder = isPressed ? RGB(0, 113, 227) : borderCol;
    drawRoundedRect(hdc, drawRc, radius, activeFill, activeBorder);

    if (!isPressed && topHighlight != RGB(0, 0, 0)) {
        HPEN specPen = CreatePen(PS_SOLID, 1, topHighlight);
        HGDIOBJ oldP = SelectObject(hdc, specPen);
        MoveToEx(hdc, drawRc.left + radius / 3, drawRc.top + 1, nullptr);
        LineTo(hdc, drawRc.right - radius / 3, drawRc.top + 1);
        SelectObject(hdc, oldP);
        DeleteObject(specPen);
    }
}

void MainWindow::drawAppleTrafficLights(HDC hdc, bool isHovered, PressedTarget pressed) {
    auto drawOneLight = [&](const RECT& rc, COLORREF baseCol, COLORREF borderCol, COLORREF glyphCol, const wchar_t* glyph, bool isPressed) {
        RECT drawRc = rc;
        if (isPressed) OffsetRect(&drawRc, 0, 1);

        COLORREF fill = isPressed ? RGB(std::max(0, GetRValue(baseCol) - 40), std::max(0, GetGValue(baseCol) - 40), std::max(0, GetBValue(baseCol) - 40)) : baseCol;
        HBRUSH br = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, borderCol);
        HGDIOBJ oldBr = SelectObject(hdc, br);
        HGDIOBJ oldPn = SelectObject(hdc, pen);

        Ellipse(hdc, drawRc.left, drawRc.top, drawRc.right, drawRc.bottom);

        // Specular inner sheen arc on top half
        if (!isPressed) {
            HPEN sheenPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
            SelectObject(hdc, sheenPen);
            Arc(hdc, drawRc.left + 1, drawRc.top + 1, drawRc.right - 1, drawRc.bottom - 1,
                drawRc.right - 2, drawRc.top + 4, drawRc.left + 2, drawRc.top + 4);
            DeleteObject(sheenPen);
        }

        // Vector glyph on hover
        if (isHovered && glyph && *glyph) {
            SelectObject(hdc, fontSmall_);
            SetTextColor(hdc, glyphCol);
            RECT gRc = drawRc;
            DrawTextW(hdc, glyph, 1, &gRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        SelectObject(hdc, oldBr);
        SelectObject(hdc, oldPn);
        DeleteObject(br);
        DeleteObject(pen);
    };

    // Close (#FF5F56, border #E0443E)
    drawOneLight(trafficCloseRc_, RGB(255, 95, 86), RGB(224, 68, 62), RGB(100, 15, 15), L"×", pressed == PressedTarget::TrafficClose);
    // Minimize (#FFBD2E, border #DEA123)
    drawOneLight(trafficMinRc_, RGB(255, 189, 46), RGB(222, 161, 35), RGB(130, 80, 10), L"–", pressed == PressedTarget::TrafficMin);
    // Zoom (#27C93F, border #1AAB29)
    drawOneLight(trafficZoomRc_, RGB(39, 201, 63), RGB(26, 171, 41), RGB(10, 80, 20), L"+", pressed == PressedTarget::TrafficZoom);
}

void MainWindow::clearThumbnailCache() {
    for (auto& pair : thumbCache_) {
        if (pair.second.hBmp) {
            DeleteObject(pair.second.hBmp);
        }
    }
    thumbCache_.clear();
}

HBITMAP MainWindow::getThumbnailBitmap(const std::wstring& fullPath, int targetSize) {
    if (fullPath.empty()) return nullptr;

    auto it = thumbCache_.find(fullPath);
    if (it != thumbCache_.end()) {
        return it->second.hBmp;
    }

    IShellItem* pShellItem = nullptr;
    HRESULT hr = ::SHCreateItemFromParsingName(fullPath.c_str(), nullptr, IID_PPV_ARGS(&pShellItem));
    if (FAILED(hr) || !pShellItem) {
        thumbCache_[fullPath] = { nullptr, 0, 0 };
        return nullptr;
    }

    IShellItemImageFactory* pFactory = nullptr;
    hr = pShellItem->QueryInterface(IID_PPV_ARGS(&pFactory));
    pShellItem->Release();

    if (FAILED(hr) || !pFactory) {
        thumbCache_[fullPath] = { nullptr, 0, 0 };
        return nullptr;
    }

    SIZE sz{ static_cast<LONG>(targetSize), static_cast<LONG>(targetSize) };
    HBITMAP hBmp = nullptr;
    hr = pFactory->GetImage(sz, SIIGBF_RESIZETOFIT, &hBmp);
    pFactory->Release();

    if (FAILED(hr) || !hBmp) {
        thumbCache_[fullPath] = { nullptr, 0, 0 };
        return nullptr;
    }

    BITMAP bm{};
    ::GetObjectW(hBmp, sizeof(bm), &bm);
    thumbCache_[fullPath] = { hBmp, bm.bmWidth, bm.bmHeight };
    return hBmp;
}

void MainWindow::drawThumbnail(HDC hdc, HBITMAP hBmp, const RECT& targetRc) {
    if (!hBmp) return;
    BITMAP bm{};
    if (::GetObjectW(hBmp, sizeof(bm), &bm) == 0) return;
    int srcW = bm.bmWidth;
    int srcH = bm.bmHeight;
    if (srcW <= 0 || srcH <= 0) return;

    int boxW = targetRc.right - targetRc.left;
    int boxH = targetRc.bottom - targetRc.top;
    if (boxW <= 0 || boxH <= 0) return;

    float scale = std::min(static_cast<float>(boxW) / srcW, static_cast<float>(boxH) / srcH);
    int fitW = std::clamp(static_cast<int>(srcW * scale), 1, boxW);
    int fitH = std::clamp(static_cast<int>(srcH * scale), 1, boxH);
    int dstX = targetRc.left + (boxW - fitW) / 2;
    int dstY = targetRc.top + (boxH - fitH) / 2;

    HDC memDC = ::CreateCompatibleDC(hdc);
    if (!memDC) return;
    HGDIOBJ oldBmp = ::SelectObject(memDC, hBmp);

    ::SetStretchBltMode(hdc, HALFTONE);
    ::SetBrushOrgEx(hdc, 0, 0, nullptr);
    ::StretchBlt(hdc, dstX, dstY, fitW, fitH, memDC, 0, 0, srcW, srcH, SRCCOPY);

    ::SelectObject(memDC, oldBmp);
    ::DeleteDC(memDC);
}

void MainWindow::copyTextToClipboard(const std::wstring& text) {
    if (text.empty() || !::OpenClipboard(hwnd_)) return;
    ::EmptyClipboard();
    size_t bytes = (text.length() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem) {
        void* pMem = ::GlobalLock(hMem);
        if (pMem) {
            memcpy(pMem, text.c_str(), bytes);
            ::GlobalUnlock(hMem);
            ::SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }
    ::CloseClipboard();
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
    WCHAR envBuf[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"OneDrive", envBuf, MAX_PATH)) {
        pathOneDrive_ = envBuf;
    } else {
        pathOneDrive_ = pathHome_ + L"\\OneDrive";
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
    filterPills_.push_back({ FilterCategory::LargeFiles, L"Large (>100MB)", L"\uE945", {} });
    filterPills_.push_back({ FilterCategory::ModifiedToday, L"Today", L"\uE823", {} });
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
    std::error_code ecExist;
    if (!std::filesystem::exists(item.path, ecExist) || ecExist) return;

    item.isExpanded = true;
    int targetIndent = item.indentLevel + 1;

    std::vector<SidebarItem> children;
    try {
        std::error_code ecIter;
        for (const auto& entry : std::filesystem::directory_iterator(item.path, std::filesystem::directory_options::skip_permission_denied, ecIter)) {
            if (ecIter) break;
            std::error_code ec;
            if (entry.is_directory(ec)) {
                std::wstring name = entry.path().filename().wstring();
                if (!showHiddenItems_ && (!name.empty() && name.front() == L'.' || (GetFileAttributesW(entry.path().c_str()) & FILE_ATTRIBUTE_HIDDEN))) {
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
            std::error_code ec;
            if (std::filesystem::exists(p, ec) && !ec) {
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
        case kCmdViewDetails: viewMode_ = ViewMode::Details; scrollOffset_ = 0; updateScrollInfo(); InvalidateRect(hwnd_, nullptr, FALSE); break;
        case kCmdViewTiles: viewMode_ = ViewMode::Tiles; scrollOffset_ = 0; updateScrollInfo(); InvalidateRect(hwnd_, nullptr, FALSE); break;
        case kCmdViewLargeIcons: viewMode_ = ViewMode::LargeIcons; scrollOffset_ = 0; updateScrollInfo(); InvalidateRect(hwnd_, nullptr, FALSE); break;
        case kCmdViewMediumIcons: viewMode_ = ViewMode::MediumIcons; scrollOffset_ = 0; updateScrollInfo(); InvalidateRect(hwnd_, nullptr, FALSE); break;
        case kCmdViewList: viewMode_ = ViewMode::List; scrollOffset_ = 0; updateScrollInfo(); InvalidateRect(hwnd_, nullptr, FALSE); break;

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

        case kCmdDiskCleanup:
            ShellExecuteW(hwnd_, L"open", L"cleanmgr.exe", L"/lowdisk", nullptr, SW_SHOWNORMAL);
            break;
        case kCmdStorageSettings:
            ShellExecuteW(hwnd_, L"open", L"ms-settings:storagesense", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case kCmdEmptyRecycleBin:
            SHEmptyRecycleBinW(hwnd_, nullptr, 0);
            break;
        case kCmdOpenPowershell: {
            std::wstring loc = (isHomeView_ || isThisPcView_ || currentPath_.empty()) ? L"C:\\" : currentPath_.wstring();
            ShellExecuteW(hwnd_, L"open", L"powershell.exe", (L"-NoExit -Command \"Set-Location '" + loc + L"'\"").c_str(), nullptr, SW_SHOWNORMAL);
            break;
        }
        case kCmdFilterLargeFiles:
            setFilterCategory(FilterCategory::LargeFiles);
            break;
        case kCmdFilterToday:
            setFilterCategory(FilterCategory::ModifiedToday);
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
            } else if (item.path == L"godxplr://thispc") {
                navigateToThisPc();
            } else {
                std::error_code ec;
                if (std::filesystem::exists(item.path, ec) && !ec) {
                    navigateTo(std::filesystem::path(item.path));
                }
            }
        }
        return 0;

    case WM_USER + 110: // Select row
        selectedRow_ = static_cast<int>(wParam);
        InvalidateRect(hwnd_, nullptr, FALSE);
        UpdateWindow(hwnd_);
        return 0;

    case WM_USER + 112: // Navigate to This PC
        navigateToThisPc();
        return 0;

    case WM_USER + 114: // Navigate to Home
        navigateToHome();
        return 0;

    case WM_USER + 113: { // Set preview pane width (wParam: target width, e.g. 520, 600, or 0 = reset 420)
        previewPaneWidth_ = (wParam == 0) ? 420 : static_cast<int>(wParam);
        RECT rc;
        GetClientRect(hwnd_, &rc);
        onResize(rc.right, rc.bottom);
        InvalidateRect(hwnd_, nullptr, FALSE);
        UpdateWindow(hwnd_);
        return 0;
    }

    case WM_USER + 111: // Set search query (wParam: 0 = clear, 1 = Batman: Arkham Knight, 2 = Arkham, 3 = BATMANARK, 4 = Dark Knight, 5 = knight, 6 = Batman Arkham Trilogy)
        if (wParam == 0) {
            searchIsPlaceholder_ = true;
            std::wstring ph = isHomeView_ ? L"Search Home" : (L"Search " + (currentPath_.filename().empty() ? currentPath_.wstring() : currentPath_.filename().wstring()));
            SetWindowTextW(editSearch_, ph.c_str());
            onSearchChanged();
        } else {
            searchIsPlaceholder_ = false;
            const wchar_t* q = L"Batman: Arkham Knight";
            if (wParam == 2) q = L"Arkham";
            else if (wParam == 3) q = L"BATMANARK";
            else if (wParam == 4) q = L"Dark Knight";
            else if (wParam == 5) q = L"knight";
            else if (wParam == 6) q = L"Batman Arkham Trilogy";
            SetWindowTextW(editSearch_, q);
            onSearchChanged();
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        UpdateWindow(hwnd_);
        return 0;

    case WM_COPYDATA: {
        auto* pcds = reinterpret_cast<PCOPYDATASTRUCT>(lParam);
        if (pcds && pcds->lpData) {
            if (pcds->dwData == 1) { // Search query
                const wchar_t* text = reinterpret_cast<const wchar_t*>(pcds->lpData);
                if (text && wcslen(text) > 0) {
                    searchIsPlaceholder_ = false;
                    SetWindowTextW(editSearch_, text);
                    onSearchChanged();
                } else {
                    searchIsPlaceholder_ = true;
                    std::wstring ph = isHomeView_ ? L"Search Home" : (L"Search " + (currentPath_.filename().empty() ? currentPath_.wstring() : currentPath_.filename().wstring()));
                    SetWindowTextW(editSearch_, ph.c_str());
                    onSearchChanged();
                }
                InvalidateRect(hwnd_, nullptr, FALSE);
                UpdateWindow(hwnd_);
                return TRUE;
            } else if (pcds->dwData == 2) { // Navigate to path
                const wchar_t* path = reinterpret_cast<const wchar_t*>(pcds->lpData);
                if (path && wcslen(path) > 0) {
                    navigateTo(path);
                    return TRUE;
                }
            }
        }
        return FALSE;
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

    case WM_LBUTTONUP:
        onLButtonUp(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_RBUTTONUP:
        onRButtonUp(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_MOUSEMOVE:
        onMouseMove(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd_, &pt);
            RECT clientRc;
            GetClientRect(hwnd_, &clientRc);
            int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
            if (showPreviewPane_) {
                int paneX = static_cast<int>(clientRc.right) - previewPaneWidth_;
                if (std::abs(pt.x - paneX) <= 6 && pt.y >= contentTop && pt.y < clientRc.bottom - statusBarHeight_) {
                    SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32644)));
                    return TRUE;
                }
            }
            if (showNavigationPane_) {
                if (std::abs(pt.x - sidebarWidth_) <= 6 && pt.y >= contentTop && pt.y < clientRc.bottom - statusBarHeight_) {
                    SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32644)));
                    return TRUE;
                }
            }
        }
        break;

    case WM_KEYDOWN:
        onKeyDown(wParam);
        return 0;

    case WM_TIMER:
        if (wParam == 101 /* TIMER_RUBBERBAND */) {
            overscrollY_ *= 0.68;
            if (std::abs(overscrollY_) < 0.5) {
                overscrollY_ = 0.0;
                KillTimer(hwnd_, 101);
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        break;

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

    int newOffset = scrollOffset_ + lines;
    if (newOffset < 0) {
        // Scrolled above top boundary -> soft rubber-banding resistance (apple-design §9)
        double overshoot = static_cast<double>(-newOffset * 16);
        overscrollY_ = std::min(45.0, appleRubberband(overshoot, static_cast<double>(surfaceHeight), 0.55));
        scrollOffset_ = 0;
        SetTimer(hwnd_, 101, 16, nullptr);
    } else if (newOffset > maxScroll) {
        // Scrolled below bottom boundary -> soft rubber-banding resistance (apple-design §9)
        double overshoot = static_cast<double>((newOffset - maxScroll) * 16);
        overscrollY_ = -std::min(45.0, appleRubberband(overshoot, static_cast<double>(surfaceHeight), 0.55));
        scrollOffset_ = maxScroll;
        SetTimer(hwnd_, 101, 16, nullptr);
    } else {
        scrollOffset_ = newOffset;
    }
    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::onLButtonUp(int x, int y) {
    (void)x; (void)y;
    if (GetCapture() == hwnd_) {
        ReleaseCapture();
    }

    if (isResizingPreviewPane_ || isResizingSidebar_) {
        isResizingPreviewPane_ = false;
        isResizingSidebar_ = false;
        RECT clientRc;
        GetClientRect(hwnd_, &clientRc);
        onResize(clientRc.right, clientRc.bottom);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    if (pressedTarget_ != PressedTarget::None) {
        PressedTarget target = pressedTarget_;
        int id = pressedId_;
        pressedTarget_ = PressedTarget::None;
        pressedId_ = -1;
        InvalidateRect(hwnd_, nullptr, FALSE);

        RECT clientRc;
        GetClientRect(hwnd_, &clientRc);

        switch (target) {
        case PressedTarget::TrafficClose:
            PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            return;
        case PressedTarget::TrafficMin:
            ShowWindow(hwnd_, SW_MINIMIZE);
            return;
        case PressedTarget::TrafficZoom: {
            WINDOWPLACEMENT wp{};
            wp.length = sizeof(wp);
            GetWindowPlacement(hwnd_, &wp);
            if (wp.showCmd == SW_MAXIMIZE) {
                ShowWindow(hwnd_, SW_RESTORE);
            } else {
                ShowWindow(hwnd_, SW_MAXIMIZE);
            }
            return;
        }
        case PressedTarget::Tab:
            if (id >= 0 && id < static_cast<int>(tabs_.size())) {
                activeTab_ = static_cast<size_t>(id);
                if (tabs_[id].path == L"godxplr://home") navigateToHome();
                else if (tabs_[id].path == L"godxplr://thispc") navigateToThisPc();
                else navigateTo(std::filesystem::path(tabs_[id].path));
            }
            return;
        case PressedTarget::NewTab:
            tabs_.push_back({ L"Home", L"godxplr://home" });
            activeTab_ = tabs_.size() - 1;
            navigateToHome();
            return;
        case PressedTarget::NavBack:
            navigateBack();
            return;
        case PressedTarget::NavForward:
            navigateForward();
            return;
        case PressedTarget::NavUp:
            navigateUp();
            return;
        case PressedTarget::NavRefresh:
            refresh();
            return;
        case PressedTarget::CmdBtn:
            if (id == 1) cutSelectedItem();
            else if (id == 2) copySelectedItem();
            else if (id == 3) pasteItem();
            else if (id == 4) renameSelectedItem();
            else if (id == 6) deleteSelectedItem();
            else if (id == 11) togglePreviewPane();
            else if (id == 12) toggleDetailsPane();
            return;
        case PressedTarget::FilterPill:
            if (id >= 0 && id < static_cast<int>(filterPills_.size())) {
                setFilterCategory(filterPills_[id].cat);
            }
            return;
        case PressedTarget::QuickCard:
            if (id >= 0 && id < static_cast<int>(quickCards_.size())) {
                std::error_code ec;
                if (std::filesystem::exists(quickCards_[id].path, ec) && !ec) {
                    navigateTo(std::filesystem::path(quickCards_[id].path));
                }
            }
            return;
        case PressedTarget::HomeSectionRecent:
            homeSection_ = 0; filterAndSortItems(); InvalidateRect(hwnd_, nullptr, FALSE); return;
        case PressedTarget::HomeSectionFav:
            homeSection_ = 1; filterAndSortItems(); InvalidateRect(hwnd_, nullptr, FALSE); return;
        case PressedTarget::HomeSectionShared:
            homeSection_ = 2; filterAndSortItems(); InvalidateRect(hwnd_, nullptr, FALSE); return;
        case PressedTarget::PaneClose:
            showPreviewPane_ = false; onResize(clientRc.right, clientRc.bottom); InvalidateRect(hwnd_, nullptr, FALSE); return;
        case PressedTarget::PaneNarrow:
            previewPaneWidth_ = std::max(280, previewPaneWidth_ - 70); onResize(clientRc.right, clientRc.bottom); InvalidateRect(hwnd_, nullptr, FALSE); return;
        case PressedTarget::PaneReset:
            previewPaneWidth_ = 420; onResize(clientRc.right, clientRc.bottom); InvalidateRect(hwnd_, nullptr, FALSE); return;
        case PressedTarget::PaneWiden:
            previewPaneWidth_ = std::min<int>(static_cast<int>(clientRc.right) - sidebarWidth_ - 80, previewPaneWidth_ + 70); onResize(clientRc.right, clientRc.bottom); InvalidateRect(hwnd_, nullptr, FALSE); return;
        case PressedTarget::ActionDockOpen:
        case PressedTarget::ActionDockInFolder:
        case PressedTarget::ActionDockCopyPath:
        case PressedTarget::ActionDockCopyText: {
            int targetIdx = (hoverRow_ >= 0 && hoverRow_ < static_cast<int>(filteredIndices_.size())) ? hoverRow_ : selectedRow_;
            if (targetIdx >= 0 && targetIdx < static_cast<int>(filteredIndices_.size())) {
                const auto& item = allItems_[filteredIndices_[targetIdx]];
                std::filesystem::path fullP = !item.fullPath.empty()
                    ? std::filesystem::path(item.fullPath)
                    : (isHomeView_ ? std::filesystem::path(item.location) / item.name : currentPath_ / item.name);
                if (target == PressedTarget::ActionDockOpen) {
                    openItem(item);
                } else if (target == PressedTarget::ActionDockInFolder) {
                    showInFolder(fullP.wstring());
                } else if (target == PressedTarget::ActionDockCopyPath) {
                    copyTextToClipboard(fullP.wstring());
                } else if (target == PressedTarget::ActionDockCopyText) {
                    const auto& pdf = getOrLoadPdfInfo(fullP.wstring());
                    std::wstring allText;
                    for (const auto& line : pdf.lines) allText += line + L"\r\n";
                    if (!allText.empty()) copyTextToClipboard(allText);
                    else copyTextToClipboard(fullP.wstring());
                }
            }
            return;
        }
        default:
            break;
        }
    }
}

void MainWindow::onMouseMove(int x, int y) {
    if (isResizingPreviewPane_) {
        RECT clientRc;
        GetClientRect(hwnd_, &clientRc);
        int newW = static_cast<int>(clientRc.right) - x;
        int maxW = std::max<int>(320, static_cast<int>(clientRc.right) - sidebarWidth_ - 80);
        previewPaneWidth_ = std::clamp(newW, 280, maxW);
        onResize(clientRc.right, clientRc.bottom);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    if (isResizingSidebar_) {
        RECT clientRc;
        GetClientRect(hwnd_, &clientRc);
        int maxW = std::max<int>(200, static_cast<int>(clientRc.right) - (showPreviewPane_ ? previewPaneWidth_ : 0) - 80);
        sidebarWidth_ = std::clamp(x, 180, std::min<int>(550, maxW));
        onResize(clientRc.right, clientRc.bottom);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    // Direct Manipulation: Cancel pressed feedback if pointer moves away by > 18px (apple-design §1, §10)
    if (pressedTarget_ != PressedTarget::None) {
        int dx = x - pressStartPt_.x;
        int dy = y - pressStartPt_.y;
        if (dx * dx + dy * dy > 324) { // 18^2
            pressedTarget_ = PressedTarget::None;
            pressedId_ = -1;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }

    // Apple macOS Traffic light hover tracking
    bool oldTraffic = trafficHover_;
    trafficHover_ = (x >= 12 && x <= 72 && y >= 10 && y <= 30);
    if (oldTraffic != trafficHover_) {
        RECT trc{12, 10, 72, 30};
        InvalidateRect(hwnd_, &trc, FALSE);
    }

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
        else if (x >= 472 && x < 604) hoverCmdBtn_ = 8; // View
        else if (x >= 608 && x < 704) hoverCmdBtn_ = 9; // Filter
        else if (x >= 716 && x < 754) hoverCmdBtn_ = 10; // More
        else if (x >= w - 224 && x < w - 118) hoverCmdBtn_ = 11; // Preview
        else if (x >= w - 110 && x < w - 12) hoverCmdBtn_ = 12; // Details
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
    if (isHomeView_ && !isSearching_ && x >= sidebarWidth_) {
        for (size_t i = 0; i < quickCards_.size(); ++i) {
            if (PtInRect(&quickCards_[i].rect, POINT{x, y})) {
                hoverCard_ = static_cast<int>(i);
                break;
            }
        }
    }

    // 4. File Row / Grid Card Hover
    if ((!isHomeView_ || isSearching_) && (viewMode_ == ViewMode::LargeIcons || viewMode_ == ViewMode::MediumIcons || viewMode_ == ViewMode::Tiles)) {
        int cardW = (viewMode_ == ViewMode::Tiles) ? 300 : (viewMode_ == ViewMode::LargeIcons ? 164 : 124);
        int cardH = (viewMode_ == ViewMode::Tiles) ? 68 : (viewMode_ == ViewMode::LargeIcons ? 180 : 140);
        int gapX = 14;
        int gapY = (viewMode_ == ViewMode::Tiles) ? 12 : 16;
        int startY = contentTop + filterBarHeight_ + ((viewMode_ == ViewMode::Tiles) ? 10 : 14);
        RECT clientRc;
        GetClientRect(hwnd_, &clientRc);
        int rightMargin = (showDetailsPane_ ? detailsPaneWidth_ : (showPreviewPane_ ? previewPaneWidth_ : 0));
        int effWidth = clientRc.right - rightMargin;
        int cols = std::max(1, (effWidth - sidebarWidth_ - 24) / (cardW + gapX));

        int relX = x - (sidebarWidth_ + 16);
        int relY = y - startY + scrollOffset_ * 20;
        if (relX >= 0 && relY >= 0) {
            int c = relX / (cardW + gapX);
            int r = relY / (cardH + gapY);
            int inCardX = relX % (cardW + gapX);
            int inCardY = relY % (cardH + gapY);
            if (c < cols && inCardX < cardW && inCardY < cardH) {
                int hIdx = r * cols + c;
                if (hIdx >= 0 && hIdx < static_cast<int>(filteredIndices_.size())) {
                    hoverRow_ = hIdx;
                }
            }
        }
    } else {
        int listTop = getListTop(contentTop);
        if (x >= sidebarWidth_ && y >= listTop) {
            int r = scrollOffset_ + ((y - listTop) / rowHeight_);
            if (r >= 0 && r < static_cast<int>(filteredIndices_.size())) {
                hoverRow_ = r;
            }
        }
    }

    if (sidebarHoverIndex_ != oldSidebar || hoverRow_ != oldHoverRow || hoverCard_ != oldCard || hoverPill_ != oldPill) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void MainWindow::onLButtonDown(int x, int y, bool isDoubleClick) {
    RECT clientRc;
    GetClientRect(hwnd_, &clientRc);

    // 1. Tab Bar & macOS Traffic Lights (y in 0 .. tabBarHeight_)
    if (y < tabBarHeight_) {
        // Traffic lights hit testing (apple-design §16)
        if (PtInRect(&trafficCloseRc_, POINT{x, y})) {
            pressedTarget_ = PressedTarget::TrafficClose;
            pressedId_ = 0;
            pressStartPt_ = {x, y};
            SetCapture(hwnd_);
            InvalidateRect(hwnd_, &trafficCloseRc_, FALSE);
            return;
        }
        if (PtInRect(&trafficMinRc_, POINT{x, y})) {
            pressedTarget_ = PressedTarget::TrafficMin;
            pressedId_ = 0;
            pressStartPt_ = {x, y};
            SetCapture(hwnd_);
            InvalidateRect(hwnd_, &trafficMinRc_, FALSE);
            return;
        }
        if (PtInRect(&trafficZoomRc_, POINT{x, y})) {
            pressedTarget_ = PressedTarget::TrafficZoom;
            pressedId_ = 0;
            pressStartPt_ = {x, y};
            SetCapture(hwnd_);
            InvalidateRect(hwnd_, &trafficZoomRc_, FALSE);
            return;
        }

        int tabX = 86;
        for (size_t i = 0; i < tabs_.size(); ++i) {
            RECT tabRc{tabX, 4, tabX + 180, tabBarHeight_};
            if (x >= tabRc.left && x < tabRc.right) {
                // If clicked close '×' button
                if (x >= tabRc.right - 24 && x < tabRc.right - 6 && tabs_.size() > 1) {
                    tabs_.erase(tabs_.begin() + i);
                    if (activeTab_ >= tabs_.size()) activeTab_ = tabs_.size() - 1;
                    if (tabs_[activeTab_].path == L"godxplr://home") navigateToHome();
                    else if (tabs_[activeTab_].path == L"godxplr://thispc") navigateToThisPc();
                    else navigateTo(std::filesystem::path(tabs_[activeTab_].path));
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    return;
                }
                pressedTarget_ = PressedTarget::Tab;
                pressedId_ = static_cast<int>(i);
                pressStartPt_ = {x, y};
                SetCapture(hwnd_);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            }
            tabX += 186;
        }

        // New Tab (+) button
        RECT newTabRc{tabX, 4, tabX + 34, tabBarHeight_};
        if (x >= newTabRc.left && x < newTabRc.right) {
            pressedTarget_ = PressedTarget::NewTab;
            pressedId_ = 0;
            pressStartPt_ = {x, y};
            SetCapture(hwnd_);
            InvalidateRect(hwnd_, &newTabRc, FALSE);
            return;
        }
        return;
    }

    // 2. Navigation Row Clicks (y in tabBarHeight_ .. tabBarHeight_ + navBarHeight_)
    int navTop = tabBarHeight_;
    int navBottom = navTop + navBarHeight_;
    if (y >= navTop && y < navBottom) {
        if (x >= 12 && x < 46) {
            if (!backStack_.empty()) {
                pressedTarget_ = PressedTarget::NavBack;
                pressStartPt_ = {x, y};
                SetCapture(hwnd_);
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return;
        } else if (x >= 50 && x < 84) {
            if (!forwardStack_.empty()) {
                pressedTarget_ = PressedTarget::NavForward;
                pressStartPt_ = {x, y};
                SetCapture(hwnd_);
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return;
        } else if (x >= 88 && x < 122) {
            if (!isHomeView_ && !isThisPcView_) {
                pressedTarget_ = PressedTarget::NavUp;
                pressStartPt_ = {x, y};
                SetCapture(hwnd_);
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return;
        } else if (x >= 126 && x < 160) {
            pressedTarget_ = PressedTarget::NavRefresh;
            pressStartPt_ = {x, y};
            SetCapture(hwnd_);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        } else {
            int rightMargin = (showDetailsPane_ ? detailsPaneWidth_ : (showPreviewPane_ ? previewPaneWidth_ : 0));
            int searchW = 280;
            int searchX = clientRc.right - rightMargin - searchW - 20;

            // Search Clear button (✕)
            if (x >= searchX + searchW - 52 && x < searchX + searchW - 32) {
                searchIsPlaceholder_ = true;
                std::wstring ph = isThisPcView_ ? L"Search This PC" : (isHomeView_ ? L"Search Home" : (L"Search " + (currentPath_.filename().empty() ? currentPath_.wstring() : currentPath_.filename().wstring())));
                SetWindowTextW(editSearch_, ph.c_str());
                InvalidateRect(editSearch_, nullptr, TRUE);
                onSearchChanged();
                return;
            }

            // Check breadcrumb clicks
            for (const auto& bc : breadcrumbs_) {
                if (PtInRect(&bc.rect, POINT{x, y})) {
                    if (bc.path == L"godxplr://home") navigateToHome();
                    else if (bc.path == L"godxplr://thispc") navigateToThisPc();
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
            pressedTarget_ = PressedTarget::CmdBtn; pressedId_ = 1; pressStartPt_ = {x, y}; SetCapture(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
        } else if (x >= 168 && x < 204) {
            pressedTarget_ = PressedTarget::CmdBtn; pressedId_ = 2; pressStartPt_ = {x, y}; SetCapture(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
        } else if (x >= 208 && x < 244) {
            pressedTarget_ = PressedTarget::CmdBtn; pressedId_ = 3; pressStartPt_ = {x, y}; SetCapture(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
        } else if (x >= 248 && x < 284) {
            pressedTarget_ = PressedTarget::CmdBtn; pressedId_ = 4; pressStartPt_ = {x, y}; SetCapture(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
        } else if (x >= 288 && x < 324) {
            // Share
        } else if (x >= 328 && x < 364) {
            pressedTarget_ = PressedTarget::CmdBtn; pressedId_ = 6; pressStartPt_ = {x, y}; SetCapture(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
        } else if (x >= 380 && x < 464) {
            showSortMenu(x, cmdBottom);
        } else if (x >= 472 && x < 604) {
            showViewMenu(x, cmdBottom);
        } else if (x >= 608 && x < 704) {
            showFilterMenu(x, cmdBottom);
        } else if (x >= 716 && x < 754) {
            showMoreMenu(x, cmdBottom);
        } else if (x >= clientRc.right - 224 && x < clientRc.right - 118) {
            pressedTarget_ = PressedTarget::CmdBtn; pressedId_ = 11; pressStartPt_ = {x, y}; SetCapture(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
        } else if (x >= clientRc.right - 110 && x < clientRc.right - 12) {
            pressedTarget_ = PressedTarget::CmdBtn; pressedId_ = 12; pressStartPt_ = {x, y}; SetCapture(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
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
                    } else if (item.path == L"godxplr://thispc") {
                        navigateToThisPc();
                    } else if (item.path == L"shell:RecycleBinFolder") {
                        ShellExecuteW(hwnd_, L"open", L"shell:RecycleBinFolder", nullptr, nullptr, SW_SHOWNORMAL);
                    } else {
                        std::error_code ec;
                        if (std::filesystem::exists(item.path, ec) && !ec) {
                            navigateTo(std::filesystem::path(item.path));
                        }
                    }
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                return;
            }
            curY += h;
        }
        return;
    }

    // Check splitter dragging on preview pane left edge
    if (showPreviewPane_) {
        int paneX = clientRc.right - previewPaneWidth_;
        if (std::abs(x - paneX) <= 6 && y >= contentTop && y < clientRc.bottom - statusBarHeight_) {
            isResizingPreviewPane_ = true;
            resizeStartX_ = x;
            resizeStartWidth_ = previewPaneWidth_;
            SetCapture(hwnd_);
            return;
        }
    }

    // Check splitter dragging on sidebar right edge
    if (showNavigationPane_) {
        if (std::abs(x - sidebarWidth_) <= 6 && y >= contentTop && y < clientRc.bottom - statusBarHeight_) {
            isResizingSidebar_ = true;
            resizeStartX_ = x;
            resizeStartWidth_ = sidebarWidth_;
            SetCapture(hwnd_);
            return;
        }
    }

    // 5. Details Pane / Preview Pane Clicks (Close button & Action Buttons)
    if (showDetailsPane_ || showPreviewPane_) {
        int paneW = showDetailsPane_ ? detailsPaneWidth_ : previewPaneWidth_;
        int paneX = clientRc.right - paneW;
        if (x >= paneX && y >= contentTop) {
            // Header buttons (Widen, Reset, Narrow, Close)
            if (showPreviewPane_ && y >= contentTop + 10 && y < contentTop + 40) {
                if (PtInRect(&btnPaneCloseRc_, POINT{x, y})) {
                    pressedTarget_ = PressedTarget::PaneClose;
                    pressStartPt_ = {x, y}; SetCapture(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
                    return;
                }
                if (PtInRect(&btnPaneNarrowRc_, POINT{x, y})) {
                    pressedTarget_ = PressedTarget::PaneNarrow;
                    pressStartPt_ = {x, y}; SetCapture(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
                    return;
                }
                if (PtInRect(&btnPaneResetRc_, POINT{x, y})) {
                    pressedTarget_ = PressedTarget::PaneReset;
                    pressStartPt_ = {x, y}; SetCapture(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
                    return;
                }
                if (PtInRect(&btnPaneWidenRc_, POINT{x, y})) {
                    pressedTarget_ = PressedTarget::PaneWiden;
                    pressStartPt_ = {x, y}; SetCapture(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
                    return;
                }
            } else if (showDetailsPane_) {
                if (x >= clientRc.right - 36 && x < clientRc.right - 10 && y >= contentTop + 10 && y < contentTop + 40) {
                    showDetailsPane_ = false;
                    onResize(clientRc.right, clientRc.bottom);
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    return;
                }
            }

            // Preview Pane Quick Actions at bottom
            if (showPreviewPane_) {
                int btnY = clientRc.bottom - statusBarHeight_ - 52;
                if (y >= btnY && y < btnY + 36) {
                    int targetIdx = (hoverRow_ >= 0 && hoverRow_ < static_cast<int>(filteredIndices_.size())) ? hoverRow_ : selectedRow_;
                    if (targetIdx >= 0 && targetIdx < static_cast<int>(filteredIndices_.size())) {
                        const auto& item = allItems_[filteredIndices_[targetIdx]];
                        bool isPdf = (!item.isDirectory && toLower(item.extension) == L"pdf");

                        int availW = (clientRc.right - 16) - (paneX + 16);
                        int numBtns = (isPdf && previewPaneWidth_ >= 460) ? 4 : 3;
                        int spacing = 8;
                        int singleW = (availW - spacing * (numBtns - 1)) / numBtns;

                        RECT b1{paneX + 16, btnY, paneX + 16 + singleW, btnY + 36};
                        if (PtInRect(&b1, POINT{x, y})) {
                            pressedTarget_ = PressedTarget::ActionDockOpen;
                            pressStartPt_ = {x, y}; SetCapture(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
                            return;
                        }

                        RECT b2{b1.right + spacing, btnY, b1.right + spacing + singleW, btnY + 36};
                        if (PtInRect(&b2, POINT{x, y})) {
                            pressedTarget_ = PressedTarget::ActionDockInFolder;
                            pressStartPt_ = {x, y}; SetCapture(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
                            return;
                        }

                        if (numBtns == 4) {
                            RECT b3{b2.right + spacing, btnY, b2.right + spacing + singleW, btnY + 36};
                            if (PtInRect(&b3, POINT{x, y})) {
                                pressedTarget_ = PressedTarget::ActionDockCopyPath;
                                pressStartPt_ = {x, y}; SetCapture(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
                                return;
                            }
                            RECT b4{b3.right + spacing, btnY, clientRc.right - 16, btnY + 36};
                            if (PtInRect(&b4, POINT{x, y})) {
                                pressedTarget_ = PressedTarget::ActionDockCopyText;
                                pressStartPt_ = {x, y}; SetCapture(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
                                return;
                            }
                        } else {
                            RECT b3{b2.right + spacing, btnY, clientRc.right - 16, btnY + 36};
                            if (PtInRect(&b3, POINT{x, y})) {
                                pressedTarget_ = isPdf ? PressedTarget::ActionDockCopyText : PressedTarget::ActionDockCopyPath;
                                pressStartPt_ = {x, y}; SetCapture(hwnd_); InvalidateRect(hwnd_, nullptr, FALSE);
                                return;
                            }
                        }
                    }
                }
            }
            return;
        }
    }

    // 6. Directory View: 1-Click Quick Filter Pills Clicks
    if (!isHomeView_ && x >= sidebarWidth_ && y >= contentTop && y < contentTop + filterBarHeight_) {
        for (size_t i = 0; i < filterPills_.size(); ++i) {
            if (PtInRect(&filterPills_[i].rect, POINT{x, y})) {
                pressedTarget_ = PressedTarget::FilterPill;
                pressedId_ = static_cast<int>(i);
                pressStartPt_ = {x, y};
                SetCapture(hwnd_);
                InvalidateRect(hwnd_, nullptr, FALSE);
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
    if (isHomeView_ && !isSearching_ && x >= sidebarWidth_) {
        // Quick Cards
        for (size_t i = 0; i < quickCards_.size(); ++i) {
            if (PtInRect(&quickCards_[i].rect, POINT{x, y})) {
                pressedTarget_ = PressedTarget::QuickCard;
                pressedId_ = static_cast<int>(i);
                pressStartPt_ = {x, y};
                SetCapture(hwnd_);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return;
            }
        }

        // Section Tabs: (Recent), (Favorites), (Shared)
        POINT pt{x, y};
        if (PtInRect(&pillRecentRc_, pt)) {
            pressedTarget_ = PressedTarget::HomeSectionRecent;
            pressStartPt_ = {x, y};
            SetCapture(hwnd_);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        } else if (PtInRect(&pillFavRc_, pt)) {
            pressedTarget_ = PressedTarget::HomeSectionFav;
            pressStartPt_ = {x, y};
            SetCapture(hwnd_);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        } else if (PtInRect(&pillSharedRc_, pt)) {
            pressedTarget_ = PressedTarget::HomeSectionShared;
            pressStartPt_ = {x, y};
            SetCapture(hwnd_);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
    }

    // 8b. This PC View Clicks (Drives, Folders, and Maintenance Tools)
    if (isThisPcView_ && x >= sidebarWidth_) {
        // Drive Cards
        for (const auto& drive : thisPcDrives_) {
            if (PtInRect(&drive.rect, POINT{x, y})) {
                navigateTo(drive.root);
                return;
            }
        }
        // Folder Cards
        for (const auto& folder : thisPcFolderCards_) {
            if (PtInRect(&folder.rect, POINT{x, y})) {
                navigateTo(folder.path);
                return;
            }
        }
        // Maintenance Tool Cards
        for (const auto& tool : thisPcTools_) {
            if (PtInRect(&tool.rect, POINT{x, y})) {
                if (tool.cmdId == static_cast<int>(kCmdFilterLargeFiles)) {
                    navigateTo(L"C:\\");
                    setFilterCategory(FilterCategory::LargeFiles);
                } else {
                    SendMessageW(hwnd_, WM_COMMAND, MAKEWPARAM(tool.cmdId, 0), 0);
                }
                return;
            }
        }
        return;
    }

    // 8c. Status Bar View Mode Switcher Buttons Clicks
    if (y >= clientRc.bottom - statusBarHeight_) {
        if (PtInRect(&statusViewDetailsRc_, POINT{x, y})) {
            viewMode_ = ViewMode::Details;
            scrollOffset_ = 0;
            updateScrollInfo();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        } else if (PtInRect(&statusViewTilesRc_, POINT{x, y})) {
            viewMode_ = ViewMode::Tiles;
            scrollOffset_ = 0;
            updateScrollInfo();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        } else if (PtInRect(&statusViewLargeRc_, POINT{x, y})) {
            viewMode_ = ViewMode::LargeIcons;
            scrollOffset_ = 0;
            updateScrollInfo();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
    }

    // 9. File List / Grid Cards Click & Double-Click (File Execution)
    if ((!isHomeView_ || isSearching_) && (viewMode_ == ViewMode::LargeIcons || viewMode_ == ViewMode::MediumIcons || viewMode_ == ViewMode::Tiles)) {
        int cardW = (viewMode_ == ViewMode::Tiles) ? 300 : (viewMode_ == ViewMode::LargeIcons ? 164 : 124);
        int cardH = (viewMode_ == ViewMode::Tiles) ? 68 : (viewMode_ == ViewMode::LargeIcons ? 180 : 140);
        int gapX = 14;
        int gapY = (viewMode_ == ViewMode::Tiles) ? 12 : 16;
        int startY = contentTop + filterBarHeight_ + ((viewMode_ == ViewMode::Tiles) ? 10 : 14);
        int rightMargin = (showDetailsPane_ ? detailsPaneWidth_ : (showPreviewPane_ ? previewPaneWidth_ : 0));
        int effWidth = clientRc.right - rightMargin;
        int cols = std::max(1, (effWidth - sidebarWidth_ - 24) / (cardW + gapX));

        int relX = x - (sidebarWidth_ + 16);
        int relY = y - startY + scrollOffset_ * 20;
        if (relX >= 0 && relY >= 0) {
            int c = relX / (cardW + gapX);
            int r = relY / (cardH + gapY);
            int inCardX = relX % (cardW + gapX);
            int inCardY = relY % (cardH + gapY);
            if (c < cols && inCardX < cardW && inCardY < cardH) {
                int clickedIdx = r * cols + c;
                if (clickedIdx >= 0 && clickedIdx < static_cast<int>(filteredIndices_.size())) {
                    // Checkbox click
                    if (showCheckBoxes_ && inCardX <= 30 && inCardY <= 30) {
                        if (static_cast<size_t>(clickedIdx) < checkedRows_.size()) {
                            checkedRows_[clickedIdx] = !checkedRows_[clickedIdx];
                            InvalidateRect(hwnd_, nullptr, FALSE);
                            return;
                        }
                    }

                    selectedRow_ = clickedIdx;
                    InvalidateRect(hwnd_, nullptr, FALSE);

                    if (isDoubleClick) {
                        openSelectedItem();
                    }
                    return;
                }
            }
        }
    } else {
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
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

    if (ctrl) {
        if (key == 'T') {
            if (shift) {
                std::wstring loc = (isHomeView_ || isThisPcView_ || currentPath_.empty()) ? L"C:\\" : currentPath_.wstring();
                ShellExecuteW(hwnd_, L"open", L"powershell.exe", (L"-NoExit -Command \"Set-Location '" + loc + L"'\"").c_str(), nullptr, SW_SHOWNORMAL);
            } else {
                tabs_.push_back({ L"Home", L"godxplr://home" });
                activeTab_ = tabs_.size() - 1;
                navigateToHome();
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return;
        } else if (key == 'W') {
            if (tabs_.size() > 1) {
                tabs_.erase(tabs_.begin() + activeTab_);
                if (activeTab_ >= tabs_.size()) activeTab_ = tabs_.size() - 1;
                if (tabs_[activeTab_].path == L"godxplr://home") navigateToHome();
                else if (tabs_[activeTab_].path == L"godxplr://thispc") navigateToThisPc();
                else navigateTo(std::filesystem::path(tabs_[activeTab_].path));
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return;
        } else if (key == 'C') {
            if (selectedRow_ >= 0 && selectedRow_ < static_cast<int>(filteredIndices_.size())) {
                const auto& item = allItems_[filteredIndices_[selectedRow_]];
                std::wstring fullPath = isHomeView_
                    ? (std::filesystem::path(item.location) / item.name).wstring()
                    : (currentPath_ / item.name).wstring();
                copyTextToClipboard(fullPath);
            } else if (!currentPath_.empty()) {
                copyTextToClipboard(currentPath_.wstring());
            }
            return;
        } else if (key == 'A') {
            checkedRows_.assign(allItems_.size(), true);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        } else if (key == 'F') {
            SetFocus(editSearch_);
            SendMessageW(editSearch_, EM_SETSEL, 0, -1);
            return;
        } else if (key == 'L') {
            addressEditActive_ = true;
            SetWindowTextW(editAddress_, currentPath_.c_str());
            ShowWindow(editAddress_, SW_SHOW);
            SetFocus(editAddress_);
            SendMessageW(editAddress_, EM_SETSEL, 0, -1);
            return;
        } else if (key == 'N' && shift) {
            createNewFolder();
            return;
        } else if (key == '1') {
            viewMode_ = ViewMode::Details; InvalidateRect(hwnd_, nullptr, FALSE); return;
        } else if (key == '2') {
            viewMode_ = ViewMode::Tiles; InvalidateRect(hwnd_, nullptr, FALSE); return;
        } else if (key == '3') {
            viewMode_ = ViewMode::LargeIcons; InvalidateRect(hwnd_, nullptr, FALSE); return;
        } else if (key == VK_OEM_4) { // Ctrl+[ : Narrow preview panel
            RECT rc;
            GetClientRect(hwnd_, &rc);
            previewPaneWidth_ = std::max(280, previewPaneWidth_ - 50);
            onResize(rc.right, rc.bottom);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        } else if (key == VK_OEM_6) { // Ctrl+] : Widen preview panel
            RECT rc;
            GetClientRect(hwnd_, &rc);
            int maxW = std::max<int>(320, static_cast<int>(rc.right) - sidebarWidth_ - 80);
            previewPaneWidth_ = std::min<int>(maxW, previewPaneWidth_ + 50);
            onResize(rc.right, rc.bottom);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        } else if (key == '0') { // Ctrl+0 : Reset preview panel width
            RECT rc;
            GetClientRect(hwnd_, &rc);
            previewPaneWidth_ = 420;
            onResize(rc.right, rc.bottom);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
    }

    if (alt) {
        if (key == VK_RETURN) {
            SendMessageW(hwnd_, WM_COMMAND, MAKEWPARAM(kCmdProperties, 0), 0);
            return;
        } else if (key == VK_LEFT) {
            navigateBack();
            return;
        } else if (key == VK_RIGHT) {
            navigateForward();
            return;
        } else if (key == VK_UP) {
            navigateUp();
            return;
        } else if (key == 'D') {
            addressEditActive_ = true;
            SetWindowTextW(editAddress_, currentPath_.c_str());
            ShowWindow(editAddress_, SW_SHOW);
            SetFocus(editAddress_);
            SendMessageW(editAddress_, EM_SETSEL, 0, -1);
            return;
        }
    }

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
        scrollOffset_ = 0;
        updateScrollInfo();
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;

    case '2':
        viewMode_ = ViewMode::Tiles;
        scrollOffset_ = 0;
        updateScrollInfo();
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;

    case '3':
        viewMode_ = ViewMode::LargeIcons;
        scrollOffset_ = 0;
        updateScrollInfo();
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;

    case '4':
        viewMode_ = ViewMode::MediumIcons;
        scrollOffset_ = 0;
        updateScrollInfo();
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;

    case '5':
        viewMode_ = ViewMode::List;
        scrollOffset_ = 0;
        updateScrollInfo();
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
    wchar_t searchBuf[256]{};
    if (!searchIsPlaceholder_) {
        GetWindowTextW(editSearch_, searchBuf, 256);
    }
    std::wstring query = searchBuf;
    while (!query.empty() && iswspace(query.front())) query.erase(query.begin());
    while (!query.empty() && iswspace(query.back())) query.pop_back();

    if (query.empty()) {
        isSearching_ = false;
        currentSearchQuery_.clear();
        hasExactSearchMatches_ = false;
        fuzzySuggestion_.clear();
        allItems_ = folderItems_;
        filterAndSortItems();
    } else {
        executeDeepSearch(query);
    }

    selectedRow_ = filteredIndices_.empty() ? -1 : 0;
    scrollOffset_ = 0;
    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::navigateTo(const std::filesystem::path& path) {
    if (path == L"godxplr://thispc") {
        navigateToThisPc();
        return;
    }
    if (path == L"godxplr://home") {
        navigateToHome();
        return;
    }

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
    isThisPcView_ = false;
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
    clearThumbnailCache();
    allItems_.clear();
    try {
        for (const auto& entry : std::filesystem::directory_iterator(currentPath_, std::filesystem::directory_options::skip_permission_denied)) {
            FileItemDisplay item;
            item.name = entry.path().filename().wstring();
            item.fullPath = entry.path().wstring();
            item.location = entry.path().parent_path().wstring();
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

    folderItems_ = allItems_;

    if (isSearching_) {
        executeDeepSearch(currentSearchQuery_);
    } else {
        filterAndSortItems();
        selectedRow_ = filteredIndices_.empty() ? -1 : 0;
        scrollOffset_ = 0;
        updateScrollInfo();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void MainWindow::navigateToThisPc() {
    isHomeView_ = false;
    isThisPcView_ = true;
    isSearching_ = false;
    currentSearchQuery_.clear();
    hasExactSearchMatches_ = false;
    fuzzySuggestion_.clear();
    searchIsPlaceholder_ = true;

    if (!currentPath_.empty() && currentPath_ != L"godxplr://thispc") {
        backStack_.push_back(currentPath_);
        forwardStack_.clear();
    }
    currentPath_ = L"godxplr://thispc";

    SetWindowTextW(editSearch_, L"Search This PC");
    InvalidateRect(editSearch_, nullptr, TRUE);

    if (activeTab_ < tabs_.size()) {
        tabs_[activeTab_].title = L"This PC";
        tabs_[activeTab_].path = L"godxplr://thispc";
    }

    allItems_.clear();
    folderItems_.clear();
    filteredIndices_.clear();
    selectedRow_ = -1;
    scrollOffset_ = 0;
    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::navigateToHome() {
    isHomeView_ = true;
    isThisPcView_ = false;
    isSearching_ = false;
    currentSearchQuery_.clear();
    hasExactSearchMatches_ = false;
    fuzzySuggestion_.clear();
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
    clearThumbnailCache();
    allItems_.clear();
    std::vector<std::filesystem::path> scanDirs = { pathDownloads_, pathDocuments_, pathPictures_, pathDesktop_ };

    for (const auto& dir : scanDirs) {
        std::error_code ec;
        if (dir.empty() || !std::filesystem::exists(dir, ec) || ec) continue;
        try {
            int count = 0;
            for (const auto& entry : std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
                if (ec) break;
                if (count++ > 6) break;
                FileItemDisplay item;
                item.name = entry.path().filename().wstring();
                item.fullPath = entry.path().wstring();
                item.location = dir.filename().wstring();
                item.isDirectory = entry.is_directory(ec);

                if (!showHiddenItems_ && (!item.name.empty() && item.name.front() == L'.' || (GetFileAttributesW(entry.path().c_str()) & FILE_ATTRIBUTE_HIDDEN))) {
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

    folderItems_ = allItems_;

    if (isSearching_) {
        executeDeepSearch(currentSearchQuery_);
    } else {
        filterAndSortItems();
        selectedRow_ = filteredIndices_.empty() ? -1 : 0;
        scrollOffset_ = 0;
        updateScrollInfo();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void MainWindow::executeDeepSearch(const std::wstring& rawQuery) {
    allItems_.clear();
    filteredIndices_.clear();
    hasExactSearchMatches_ = false;
    fuzzySuggestion_.clear();

    if (rawQuery.empty()) {
        isSearching_ = false;
        currentSearchQuery_.clear();
        allItems_ = folderItems_;
        filterAndSortItems();
        return;
    }

    isSearching_ = true;
    currentSearchQuery_ = rawQuery;

    std::vector<std::filesystem::path> roots;
    std::error_code ecRoot;
    if (!isHomeView_ && !currentPath_.empty() && std::filesystem::exists(currentPath_, ecRoot) && !ecRoot) {
        roots.push_back(currentPath_);
    } else {
        // Home view: search user profile directories and workspace
        std::vector<std::wstring> homeDirs = {
            pathDesktop_, pathDownloads_, pathDocuments_,
            pathPictures_, pathVideos_, pathMusic_, L"C:\\os fileX"
        };
        for (const auto& d : homeDirs) {
            std::error_code ecD;
            if (!d.empty() && std::filesystem::exists(d, ecD) && !ecD) {
                roots.push_back(d);
            }
        }
    }

    struct CandidateScore {
        FileItemDisplay item;
        int score{0};
        bool isExact{false};
        bool isApprox{false};
        std::wstring reason;
    };

    std::vector<CandidateScore> exactList;
    std::vector<CandidateScore> approxList;
    std::unordered_set<std::wstring> matchedTokenSet;

    auto qTokens = tokenizeQuery(rawQuery);
    std::wstring qLower = toLower(rawQuery);
    std::wstring qCompact;
    for (const auto& t : qTokens) qCompact += t;

    int totalFilesExamined = 0;
    const int maxExamine = 15000;
    const size_t maxResults = 500;

    for (const auto& root : roots) {
        if (totalFilesExamined >= maxExamine) break;
        std::error_code ec;

        try {
            auto it = std::filesystem::recursive_directory_iterator(
                root,
                std::filesystem::directory_options::skip_permission_denied,
                ec);
            auto endIt = std::filesystem::recursive_directory_iterator();

            for (; it != endIt; it.increment(ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }
                if (++totalFilesExamined >= maxExamine) break;

                const auto& entry = *it;
                std::wstring name = entry.path().filename().wstring();

                if (name.empty()) continue;
                if (!showHiddenItems_ && (name[0] == L'.' || (GetFileAttributesW(entry.path().c_str()) & FILE_ATTRIBUTE_HIDDEN))) {
                    if (entry.is_directory(ec)) {
                        it.disable_recursion_pending();
                    }
                    continue;
                }

                // Skip scanning deep junk folders
                if (entry.is_directory(ec)) {
                    std::wstring lowerDir = toLower(name);
                    if (lowerDir == L"node_modules" || lowerDir == L".git" || lowerDir == L".vs" ||
                        lowerDir == L"$recycle.bin" || lowerDir == L"system volume information" ||
                        lowerDir == L"appdata" || lowerDir == L"build" || lowerDir == L"bin" || lowerDir == L"obj") {
                        it.disable_recursion_pending();
                        continue;
                    }
                }

                auto matchRes = scoreCandidate(name, rawQuery, qTokens, qLower, qCompact);
                if (matchRes.score <= 0) continue;

                FileItemDisplay item;
                item.name = name;
                item.fullPath = entry.path().wstring();
                item.location = entry.path().parent_path().wstring();
                item.isDirectory = entry.is_directory(ec);
                item.matchScore = matchRes.score;
                item.isApproximateMatch = matchRes.isApproximate;
                item.matchReason = matchRes.reason;

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

                CandidateScore cs;
                cs.item = std::move(item);
                cs.score = matchRes.score;
                cs.isExact = matchRes.isExact;
                cs.isApprox = matchRes.isApproximate;
                cs.reason = matchRes.reason;

                if (cs.isExact) {
                    exactList.push_back(std::move(cs));
                } else if (cs.isApprox) {
                    approxList.push_back(std::move(cs));
                    for (const auto& qt : qTokens) {
                        if (toLower(name).find(qt) != std::wstring::npos) {
                            matchedTokenSet.insert(qt);
                        }
                    }
                }

                if (exactList.size() + approxList.size() >= maxResults) break;
            }
        } catch (...) {}
    }

    std::sort(exactList.begin(), exactList.end(), [](const CandidateScore& a, const CandidateScore& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.item.mtime > b.item.mtime;
    });

    std::sort(approxList.begin(), approxList.end(), [](const CandidateScore& a, const CandidateScore& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.item.mtime > b.item.mtime;
    });

    hasExactSearchMatches_ = !exactList.empty();

    for (auto& cs : exactList) {
        allItems_.push_back(std::move(cs.item));
    }
    for (auto& cs : approxList) {
        allItems_.push_back(std::move(cs.item));
    }

    if (!matchedTokenSet.empty()) {
        std::wstring sugg;
        for (const auto& s : matchedTokenSet) {
            if (!sugg.empty()) sugg += L", ";
            sugg += s;
        }
        fuzzySuggestion_ = sugg;
    } else if (!approxList.empty()) {
        fuzzySuggestion_ = approxList[0].item.name;
    }

    filterAndSortItems();
    selectedRow_ = filteredIndices_.empty() ? -1 : 0;
    scrollOffset_ = 0;
    updateScrollInfo();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::showInFolder(const std::wstring& filePath) {
    if (filePath.empty()) return;
    std::filesystem::path p(filePath);
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) return;

    if (isSearching_) {
        isSearching_ = false;
        currentSearchQuery_.clear();
        hasExactSearchMatches_ = false;
        fuzzySuggestion_.clear();
        searchIsPlaceholder_ = true;
    }

    if (std::filesystem::is_directory(p, ec)) {
        navigateTo(p);
        return;
    }

    std::filesystem::path parent = p.parent_path();
    navigateTo(parent);

    std::wstring ph = L"Search " + (parent.filename().empty() ? parent.wstring() : parent.filename().wstring());
    SetWindowTextW(editSearch_, ph.c_str());

    std::wstring targetName = p.filename().wstring();
    for (size_t i = 0; i < filteredIndices_.size(); ++i) {
        if (allItems_[filteredIndices_[i]].name == targetName) {
            selectedRow_ = static_cast<int>(i);
            if (selectedRow_ < scrollOffset_) {
                scrollOffset_ = selectedRow_;
            }
            updateScrollInfo();
            InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        }
    }
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

        // 1. Search Query Filter (only when not deep searching, since executeDeepSearch already scored & filtered)
        if (!isSearching_ && !query.empty()) {
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
        case FilterCategory::LargeFiles:
            if (item.isDirectory || item.size < 100ULL * 1024 * 1024) continue;
            break;
        case FilterCategory::ModifiedToday:
            if (item.mtime == 0 || curUli.QuadPart < item.mtime || (curUli.QuadPart - item.mtime) > oneDay100ns) continue;
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

        // When deep searching and default sorting by Name: preserve relevance score ranking!
        if (isSearching_ && sortCol_ == SortColumn::Name) {
            if (a.matchScore != b.matchScore) {
                return sortAscending_ ? (a.matchScore > b.matchScore) : (a.matchScore < b.matchScore);
            }
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

    // Layout Modes (with Photos / Large icons mode first!)
    AppendMenuW(hMenu, (viewMode_ == ViewMode::LargeIcons ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdViewLargeIcons, L"Large icons (Photos)");
    AppendMenuW(hMenu, (viewMode_ == ViewMode::MediumIcons ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdViewMediumIcons, L"Medium icons");
    AppendMenuW(hMenu, (viewMode_ == ViewMode::Tiles ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdViewTiles, L"Tiles");
    AppendMenuW(hMenu, (viewMode_ == ViewMode::List ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdViewList, L"List");
    AppendMenuW(hMenu, (viewMode_ == ViewMode::Details ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdViewDetails, L"Details");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // Panes directly in View menu for 1-click discovery
    AppendMenuW(hMenu, (showPreviewPane_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdTogglePreviewPane, L"Preview pane\tP");
    AppendMenuW(hMenu, (showDetailsPane_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdToggleDetailsPane, L"Details pane\tD");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // Compact View
    AppendMenuW(hMenu, (compactView_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdToggleCompact, L"Compact view\tC");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // Show Submenu
    HMENU hSubShow = CreatePopupMenu();
    AppendMenuW(hSubShow, (showNavigationPane_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdToggleNavPane, L"Navigation pane");
    AppendMenuW(hSubShow, (showPreviewPane_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdTogglePreviewPane, L"Preview pane");
    AppendMenuW(hSubShow, (showDetailsPane_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdToggleDetailsPane, L"Details pane");
    AppendMenuW(hSubShow, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSubShow, (showCheckBoxes_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdToggleCheckBoxes, L"Item check boxes");
    AppendMenuW(hSubShow, (showExtensions_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdToggleExtensions, L"File name extensions");
    AppendMenuW(hSubShow, (showHiddenItems_ ? MF_CHECKED : MF_UNCHECKED) | MF_STRING, kCmdToggleHidden, L"Hidden items\tH");

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
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, kCmdDiskCleanup, L"⚡ Disk Cleanup (cleanmgr)");
    AppendMenuW(hMenu, MF_STRING, kCmdStorageSettings, L"📊 Windows Storage Settings");
    AppendMenuW(hMenu, MF_STRING, kCmdEmptyRecycleBin, L"🗑 Empty Recycle Bin");

    POINT pt{x, y};
    ClientToScreen(hwnd_, &pt);
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(hMenu);
}

int MainWindow::getListTop(int contentTop) const {
    if (isHomeView_ && !isSearching_) {
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

    int totalUnits = static_cast<int>(filteredIndices_.size());
    int pageSize = std::max(1, surfaceHeight / rowHeight_);

    if ((!isHomeView_ || isSearching_) && (viewMode_ == ViewMode::LargeIcons || viewMode_ == ViewMode::MediumIcons || viewMode_ == ViewMode::Tiles)) {
        int cardW = (viewMode_ == ViewMode::Tiles) ? 300 : (viewMode_ == ViewMode::LargeIcons ? 164 : 124);
        int gapX = 14;
        int rightMargin = (showDetailsPane_ ? detailsPaneWidth_ : (showPreviewPane_ ? previewPaneWidth_ : 0));
        int effWidth = rc.right - rightMargin;
        int cols = std::max(1, (effWidth - sidebarWidth_ - 24) / (cardW + gapX));
        int totalRows = (totalUnits + cols - 1) / cols;
        totalUnits = totalRows;
        pageSize = std::max(1, surfaceHeight / (viewMode_ == ViewMode::Tiles ? 80 : 190));
    }

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = std::max(0, totalUnits - 1);
    si.nPage = pageSize;
    si.nPos = scrollOffset_;
    SetScrollInfo(hwnd_, SB_VERT, &si, TRUE);
}

HICON MainWindow::getFileIcon(const std::wstring& path, bool isDir, const std::wstring& ext, bool isLarge) {
    (void)path;
    SHFILEINFOW sfi{};
    DWORD flags = SHGFI_ICON | (isLarge ? SHGFI_LARGEICON : SHGFI_SMALLICON) | SHGFI_USEFILEATTRIBUTES;
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
    HBRUSH bgBrush = CreateSolidBrush(RGB(19, 22, 30));
    FillRect(hdc, &tabAreaRc, bgBrush);
    DeleteObject(bgBrush);

    // Subtle bottom divider for floating tab chrome
    HPEN divPen = CreatePen(PS_SOLID, 1, RGB(36, 42, 56));
    HGDIOBJ oldPen = SelectObject(hdc, divPen);
    MoveToEx(hdc, 0, tabBarHeight_ - 1, nullptr);
    LineTo(hdc, width, tabBarHeight_ - 1);
    SelectObject(hdc, oldPen);
    DeleteObject(divPen);

    SetBkMode(hdc, TRANSPARENT);

    // Apple macOS Finder Window Traffic Lights (apple-design §16: Familiarity)
    drawAppleTrafficLights(hdc, trafficHover_, pressedTarget_);

    int tabX = 86;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        bool isActive = (i == activeTab_);
        bool isPressed = (pressedTarget_ == PressedTarget::Tab && pressedId_ == static_cast<int>(i));
        RECT tabRc{tabX, 4, tabX + 180, tabBarHeight_ - 2};

        if (isActive) {
            drawAppleGlassCard(hdc, tabRc, 8, RGB(38, 44, 60), RGB(26, 32, 44), RGB(56, 68, 92), RGB(88, 105, 142), isPressed);
            // Apple Blue top indicator
            RECT indRc{tabRc.left + 12, tabRc.top, tabRc.right - 12, tabRc.top + 3};
            HBRUSH acc = CreateSolidBrush(RGB(0, 113, 227));
            FillRect(hdc, &indRc, acc);
            DeleteObject(acc);
        } else {
            drawAppleGlassCard(hdc, tabRc, 8, RGB(26, 30, 40), RGB(20, 24, 32), RGB(38, 44, 58), RGB(50, 60, 80), isPressed);
        }

        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, isActive ? RGB(0, 113, 227) : RGB(150, 160, 180));
        RECT tabIconRc{tabRc.left + 12, tabRc.top, tabRc.left + 30, tabRc.bottom};
        const wchar_t* iconGlyph = (tabs_[i].title == L"Home") ? L"\uE80F" : L"\uE8B7";
        DrawTextW(hdc, iconGlyph, 1, &tabIconRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, isActive ? RGB(255, 255, 255) : RGB(180, 190, 210));
        SetTextCharacterExtra(hdc, 0);
        RECT textRc{tabRc.left + 34, tabRc.top, tabRc.right - 28, tabRc.bottom};
        DrawTextW(hdc, tabs_[i].title.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        RECT closeRc{tabRc.right - 24, tabRc.top, tabRc.right - 8, tabRc.bottom};
        SetTextColor(hdc, RGB(140, 150, 170));
        DrawTextW(hdc, L"×", 1, &closeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        tabX += 186;
    }

    RECT plusRc{tabX, 4, tabX + 34, tabBarHeight_ - 2};
    bool isPlusPressed = (pressedTarget_ == PressedTarget::NewTab);
    drawApplePill(hdc, plusRc, RGB(28, 33, 44), RGB(46, 56, 76), RGB(68, 80, 108), isPlusPressed);
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(210, 220, 240));
    DrawTextW(hdc, L"\uE710", 1, &plusRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void MainWindow::paintNavAndAddressBar(HDC hdc, int width) {
    int navTop = tabBarHeight_;
    RECT navRc{0, navTop, width, navTop + navBarHeight_};
    HBRUSH navBrush = CreateSolidBrush(RGB(22, 26, 36));
    FillRect(hdc, &navRc, navBrush);
    DeleteObject(navBrush);

    // Specular top light line catching the material (apple-design §12)
    HPEN topPen = CreatePen(PS_SOLID, 1, RGB(56, 66, 90));
    HGDIOBJ oldP = SelectObject(hdc, topPen);
    MoveToEx(hdc, 0, navTop, nullptr);
    LineTo(hdc, width, navTop);
    SelectObject(hdc, oldP);
    DeleteObject(topPen);

    SetBkMode(hdc, TRANSPARENT);

    auto drawNavBtn = [&](int x, const wchar_t* iconGlyph, PressedTarget pt, bool enabled = true) {
        RECT rc{x, navTop + 8, x + 34, navTop + 42};
        bool isPressed = (pressedTarget_ == pt);
        drawAppleGlassCard(hdc, rc, 7,
            isPressed ? RGB(18, 22, 30) : RGB(34, 40, 54),
            isPressed ? RGB(14, 18, 24) : RGB(26, 30, 42),
            isPressed ? RGB(0, 113, 227) : RGB(46, 56, 76),
            RGB(68, 80, 108), isPressed);
        SelectObject(hdc, fontIcons_);
        SetTextColor(hdc, enabled ? RGB(235, 240, 250) : RGB(105, 115, 135));
        DrawTextW(hdc, iconGlyph, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    drawNavBtn(12, L"\uE72B", PressedTarget::NavBack, !backStack_.empty());
    drawNavBtn(50, L"\uE72A", PressedTarget::NavForward, !forwardStack_.empty());
    drawNavBtn(88, L"\uE74A", PressedTarget::NavUp, (!isHomeView_ && !isThisPcView_));
    drawNavBtn(126, L"\uE72C", PressedTarget::NavRefresh, true);

    int rightMargin = (showDetailsPane_ ? detailsPaneWidth_ : (showPreviewPane_ ? previewPaneWidth_ : 0));
    int searchW = 280;
    int searchX = width - rightMargin - searchW - 20;
    int addrX = 165;
    int addrW = searchX - addrX - 16;
    int boxH = 36;
    int boxY = navTop + (navBarHeight_ - boxH) / 2;
    RECT addrRc{addrX, boxY, addrX + addrW, boxY + boxH};

    drawAppleGlassCard(hdc, addrRc, 8, RGB(26, 30, 42), RGB(20, 24, 34), RGB(48, 58, 80), RGB(72, 84, 114), false);

    breadcrumbs_.clear();
    int bcX = addrX + 12;

    if (isThisPcView_) {
        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, RGB(0, 113, 227));
        RECT pcIconRc{bcX, addrRc.top, bcX + 20, addrRc.bottom};
        DrawTextW(hdc, L"\uE7F4", 1, &pcIconRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        bcX += 22;

        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, RGB(240, 240, 240));
        RECT pcTextRc{bcX, addrRc.top, bcX + 64, addrRc.bottom};
        DrawTextW(hdc, L"This PC", 7, &pcTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT fullPcRc{addrX + 12, addrRc.top, bcX + 64, addrRc.bottom};
        breadcrumbs_.push_back({ L"This PC", L"godxplr://thispc", fullPcRc });
    } else if (isHomeView_) {
        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, RGB(0, 113, 227));
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
        SetTextColor(hdc, RGB(0, 113, 227));
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
            SetTextColor(hdc, RGB(140, 150, 170));
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

    // Search Box Container (Spotlight Pill)
    RECT searchRc{searchX, boxY, searchX + searchW, boxY + boxH};
    drawAppleGlassCard(hdc, searchRc, 8, RGB(26, 30, 42), RGB(20, 24, 34), RGB(48, 58, 80), RGB(72, 84, 114), false);

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
    HBRUSH cmdBrush = CreateSolidBrush(RGB(24, 28, 38));
    FillRect(hdc, &cmdRc, cmdBrush);
    DeleteObject(cmdBrush);

    // Ambient Specular top edge line (apple-design §12)
    HPEN topPen = CreatePen(PS_SOLID, 1, RGB(56, 66, 90));
    HGDIOBJ oldP = SelectObject(hdc, topPen);
    MoveToEx(hdc, 0, cmdTop, nullptr);
    LineTo(hdc, width, cmdTop);
    SelectObject(hdc, oldP);
    DeleteObject(topPen);

    SetBkMode(hdc, TRANSPARENT);

    auto drawBtnHoverOrActive = [&](const RECT& rc, int btnId, bool forceActive = false) {
        bool isPressed = (pressedTarget_ == PressedTarget::CmdBtn && pressedId_ == btnId);
        if (isPressed) {
            drawAppleGlassCard(hdc, rc, 7, RGB(18, 22, 30), RGB(14, 18, 24), RGB(0, 113, 227), RGB(56, 68, 92), true);
        } else if (forceActive) {
            drawAppleGlassCard(hdc, rc, 7, RGB(20, 52, 86), RGB(14, 38, 64), RGB(0, 113, 227), RGB(96, 205, 255), false);
        } else if (hoverCmdBtn_ == btnId) {
            drawAppleGlassCard(hdc, rc, 7, RGB(38, 46, 64), RGB(28, 34, 48), RGB(56, 68, 92), RGB(78, 92, 124), false);
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
    if (viewMode_ == ViewMode::LargeIcons) viewLabel = L"Large icons";
    else if (viewMode_ == ViewMode::MediumIcons) viewLabel = L"Medium icons";
    else if (viewMode_ == ViewMode::Tiles) viewLabel = L"Tiles";
    else if (viewMode_ == ViewMode::List) viewLabel = L"List";
    else if (viewMode_ == ViewMode::Details) viewLabel = L"Details";
    drawDropdownBtn(472, 128, 8, L"\uE8A9", RGB(215, 215, 215), viewLabel);

    std::wstring filterLabel = (filterCategory_ == FilterCategory::All && dateFilter_ == DateFilter::All && sizeFilter_ == SizeFilter::All)
        ? L"Filter" : L"Filtered";
    drawDropdownBtn(608, 92, 9, L"\uE71C", RGB(215, 215, 215), filterLabel.c_str());

    // Divider 3 before More
    MoveToEx(hdc, 708, cmdTop + 11, nullptr);
    LineTo(hdc, 708, cmdTop + 37);

    // 4. More button "···"
    RECT moreRc{716, btnY, 754, btnY + 36};
    drawBtnHoverOrActive(moreRc, 10);
    SelectObject(hdc, fontIcons_);
    SetTextColor(hdc, RGB(215, 215, 215));
    DrawTextW(hdc, L"\uE712", 1, &moreRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Divider 4 before Preview & Details
    MoveToEx(hdc, width - 232, cmdTop + 11, nullptr);
    LineTo(hdc, width - 232, cmdTop + 37);

    // 5a. Preview button (toggles Preview Pane)
    RECT prevRc{width - 224, btnY, width - 118, btnY + 36};
    drawBtnHoverOrActive(prevRc, 11, showPreviewPane_);
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, showPreviewPane_ ? RGB(96, 205, 255) : RGB(215, 215, 215));
    RECT prevIconRc{prevRc.left + 10, prevRc.top, prevRc.left + 28, prevRc.bottom};
    DrawTextW(hdc, L"\uE8A1", 1, &prevIconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, fontRegular_);
    SetTextColor(hdc, showPreviewPane_ ? RGB(96, 205, 255) : RGB(235, 235, 235));
    RECT prevTextRc{prevRc.left + 30, prevRc.top, prevRc.right - 8, prevRc.bottom};
    DrawTextW(hdc, L"Preview", 7, &prevTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // 5b. Details button on far right (toggles Details Pane)
    RECT detRc{width - 110, btnY, width - 12, btnY + 36};
    drawBtnHoverOrActive(detRc, 12, showDetailsPane_);
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
    HBRUSH bg = CreateSolidBrush(RGB(19, 23, 32));
    FillRect(hdc, &barRc, bg);
    DeleteObject(bg);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(36, 42, 58));
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
        bool isPressed = (pressedTarget_ == PressedTarget::FilterPill && pressedId_ == static_cast<int>(i));

        int pillW = static_cast<int>(pill.label.size() * 9 + 42);
        pill.rect = RECT{pillX, topY + 5, pillX + pillW, topY + 35};

        if (isActive) {
            drawApplePill(hdc, pill.rect, RGB(0, 113, 227), RGB(0, 113, 227), RGB(96, 175, 255), isPressed);
        } else if (isHover) {
            drawApplePill(hdc, pill.rect, RGB(36, 44, 60), RGB(56, 68, 92), RGB(78, 92, 120), isPressed);
        } else {
            drawApplePill(hdc, pill.rect, RGB(24, 29, 40), RGB(40, 48, 66), RGB(58, 68, 90), isPressed);
        }

        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, isActive ? RGB(255, 255, 255) : (isHover ? RGB(96, 175, 255) : RGB(160, 175, 200)));
        RECT iRc{pill.rect.left + 10, pill.rect.top, pill.rect.left + 26, pill.rect.bottom};
        DrawTextW(hdc, pill.iconGlyph.c_str(), 1, &iRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, isActive ? RGB(255, 255, 255) : (isHover ? RGB(250, 250, 255) : RGB(200, 210, 225)));
        RECT tRc{pill.rect.left + 28, pill.rect.top, pill.rect.right - 10, pill.rect.bottom};
        DrawTextW(hdc, pill.label.c_str(), -1, &tRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        pillX += pillW + 8;
    }
}

void MainWindow::paintSidebar(HDC hdc, int height) {
    if (!showNavigationPane_) return;

    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    RECT sideRc{0, contentTop, sidebarWidth_, height - statusBarHeight_};
    HBRUSH bgBrush = CreateSolidBrush(RGB(17, 20, 27));
    FillRect(hdc, &sideRc, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);

    int itemY = contentTop + 8;
    for (size_t i = 0; i < sidebarItems_.size(); ++i) {
        const auto& item = sidebarItems_[i];

        if (item.isHeader) {
            if (item.name.empty()) {
                HPEN divPen = CreatePen(PS_SOLID, 1, RGB(36, 42, 56));
                SelectObject(hdc, divPen);
                MoveToEx(hdc, 16, itemY + 8, nullptr);
                LineTo(hdc, sidebarWidth_ - 16, itemY + 8);
                DeleteObject(divPen);
                itemY += 16;
            } else {
                RECT headerRc{16, itemY + 4, sidebarWidth_ - 16, itemY + 24};
                SelectObject(hdc, fontSmall_);
                SetTextColor(hdc, RGB(130, 142, 165));
                SetTextCharacterExtra(hdc, 1); // Apple Optical Tracking §15
                DrawTextW(hdc, item.name.c_str(), -1, &headerRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                SetTextCharacterExtra(hdc, 0);
                itemY += 26;
            }
            continue;
        }

        RECT rowRc{8, itemY, sidebarWidth_ - 8, itemY + 36};
        bool isSel = (static_cast<int>(i) == sidebarSelectedIndex_);
        bool isHov = (static_cast<int>(i) == sidebarHoverIndex_);

        if (isSel) {
            drawApplePill(hdc, rowRc, RGB(0, 113, 227), RGB(0, 113, 227), RGB(96, 175, 255), false);
        } else if (isHov) {
            drawApplePill(hdc, rowRc, RGB(30, 36, 50), RGB(46, 56, 78), RGB(68, 80, 108), false);
        }

        int indentX = rowRc.left + item.indentLevel * 18;

        // Tree Chevron ('>' collapsed, 'v' expanded)
        if (item.isExpandable) {
            SelectObject(hdc, fontIconsSmall_);
            SetTextColor(hdc, isSel ? RGB(255, 255, 255) : RGB(140, 150, 170));
            RECT chevRc{indentX, rowRc.top, indentX + 20, rowRc.bottom};
            DrawTextW(hdc, item.isExpanded ? L"\uE70D" : L"\uE76C", 1, &chevRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // Vector Icon
        SelectObject(hdc, fontIcons_);
        SetTextColor(hdc, isSel ? RGB(255, 255, 255) : item.iconColor);
        RECT iconRc{indentX + 22, rowRc.top, indentX + 46, rowRc.bottom};
        DrawTextW(hdc, item.iconGlyph.c_str(), 1, &iconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Name and mini capacity progress bar if subtext is present
        if (!item.subtext.empty()) {
            SelectObject(hdc, fontRegular_);
            SetTextColor(hdc, isSel ? RGB(255, 255, 255) : (isHov ? RGB(245, 245, 245) : RGB(220, 220, 220)));
            RECT textRc{indentX + 50, rowRc.top + 2, sidebarWidth_ - 30, rowRc.top + 18};
            DrawTextW(hdc, item.name.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

            // Mini Capacity Meter & Free Text
            int miniBarX = indentX + 50;
            int miniBarY = rowRc.top + 21;
            int miniBarW = 50;
            int miniBarH = 4;
            RECT miniTrackRc{miniBarX, miniBarY, miniBarX + miniBarW, miniBarY + miniBarH};
            drawRoundedRect(hdc, miniTrackRc, 2, RGB(45, 45, 45), RGB(45, 45, 45));

            RECT miniFillRc{miniBarX, miniBarY, miniBarX + 32, miniBarY + miniBarH};
            drawRoundedRect(hdc, miniFillRc, 2, RGB(0, 153, 255), RGB(0, 153, 255));

            SelectObject(hdc, fontSmall_);
            SetTextColor(hdc, isSel ? RGB(210, 230, 255) : RGB(140, 155, 175));
            RECT freeRc{miniBarX + miniBarW + 6, rowRc.top + 18, sidebarWidth_ - 8, rowRc.bottom};
            DrawTextW(hdc, item.subtext.c_str(), -1, &freeRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        } else {
            SelectObject(hdc, fontRegular_);
            SetTextColor(hdc, isSel ? RGB(255, 255, 255) : (isHov ? RGB(245, 245, 245) : RGB(220, 220, 220)));
            RECT textRc{indentX + 50, rowRc.top, sidebarWidth_ - 30, rowRc.bottom};
            DrawTextW(hdc, item.name.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }

        // Pinned item pin indicator
        if (i >= 11 && i <= 22) {
            SelectObject(hdc, fontIconsSmall_);
            SetTextColor(hdc, isSel ? RGB(230, 240, 255) : RGB(120, 130, 150));
            RECT pinRc{sidebarWidth_ - 26, rowRc.top, sidebarWidth_ - 8, rowRc.bottom};
            DrawTextW(hdc, L"\uE718", 1, &pinRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        itemY += 36;
    }

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(38, 46, 64));
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
    SetTextCharacterExtra(hdc, -1); // Apple Optical Typography §15
    DrawTextW(hdc, L"Quick access", -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SetTextCharacterExtra(hdc, 0);

    // Quick Cards Grid (Apple Squircle Glass Cards with Ambient Drop Shadows)
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
        bool isPressed = (pressedTarget_ == PressedTarget::QuickCard && pressedId_ == static_cast<int>(i));

        drawAppleGlassCard(hdc, quickCards_[i].rect, 12,
            isHov ? RGB(38, 46, 64) : RGB(30, 36, 48),
            isHov ? RGB(26, 32, 44) : RGB(20, 24, 34),
            isHov ? RGB(64, 78, 108) : RGB(42, 50, 70),
            RGB(78, 92, 124), isPressed);

        RECT badgeRc{cx + 10, cy + 11, cx + 56, cy + 57};
        drawAppleGlassCard(hdc, badgeRc, 8, RGB(36, 44, 60), RGB(24, 28, 38), RGB(56, 68, 92), RGB(88, 105, 142), false);

        SelectObject(hdc, fontIcons_);
        SetTextColor(hdc, quickCards_[i].badgeColor);
        DrawTextW(hdc, quickCards_[i].iconGlyph.c_str(), 1, &badgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontBold_);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT nameRc{cx + 64, cy + 12, cx + cardW - 24, cy + 36};
        DrawTextW(hdc, quickCards_[i].name.c_str(), -1, &nameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(155, 168, 190));
        RECT subRc{cx + 64, cy + 37, cx + cardW - 24, cy + 57};
        DrawTextW(hdc, quickCards_[i].subtext.c_str(), -1, &subRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, RGB(120, 130, 150));
        RECT pinRc{cx + cardW - 24, cy + 37, cx + cardW - 8, cy + 57};
        DrawTextW(hdc, L"\uE718", 1, &pinRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Section 2: Unified Segmented Control Track (apple-design §4, §12)
    int recentTop = cardStartY + numRows * (cardH + 12) + 16;
    RECT recentChevRc{surfaceLeft + 16, recentTop + 4, surfaceLeft + 32, recentTop + 30};
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(180, 180, 180));
    DrawTextW(hdc, L"\uE70D", 1, &recentChevRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    int trackW = 104 + 116 + 104 + 16;
    RECT trackRc{surfaceLeft + 36, recentTop - 2, surfaceLeft + 36 + trackW, recentTop + 34};
    drawApplePill(hdc, trackRc, RGB(22, 26, 36), RGB(42, 50, 70), RGB(56, 68, 92), false);

    int pillX = surfaceLeft + 40;
    auto drawSectionPill = [&](RECT& outRc, const wchar_t* iconGlyph, const wchar_t* text, bool isSelected, bool isPressed) {
        int w = static_cast<int>(wcslen(text) * 9 + 40);
        outRc = RECT{pillX, recentTop + 1, pillX + w, recentTop + 31};

        if (isSelected) {
            drawApplePill(hdc, outRc, RGB(0, 113, 227), RGB(0, 113, 227), RGB(96, 175, 255), isPressed);
        } else if (isPressed) {
            drawApplePill(hdc, outRc, RGB(30, 36, 50), RGB(46, 56, 78), RGB(68, 80, 108), true);
        }

        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, isSelected ? RGB(255, 255, 255) : RGB(160, 175, 205));
        RECT iRc{outRc.left + 8, outRc.top, outRc.left + 24, outRc.bottom};
        DrawTextW(hdc, iconGlyph, 1, &iRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, isSelected ? RGB(255, 255, 255) : RGB(190, 200, 220));
        RECT tRc{outRc.left + 26, outRc.top, outRc.right - 8, outRc.bottom};
        DrawTextW(hdc, text, -1, &tRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        pillX += w + 6;
    };

    drawSectionPill(pillRecentRc_, L"\uE823", L"Recent", homeSection_ == 0, pressedTarget_ == PressedTarget::HomeSectionRecent);
    drawSectionPill(pillFavRc_, L"\uE734", L"Favorites", homeSection_ == 1, pressedTarget_ == PressedTarget::HomeSectionFav);
    drawSectionPill(pillSharedRc_, L"\uE716", L"Shared", homeSection_ == 2, pressedTarget_ == PressedTarget::HomeSectionShared);

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
        int ty = startY + row * (tileH + 12) - scrollOffset_ * 20 + static_cast<int>(overscrollY_);

        if (ty + tileH < startY || ty > height - statusBarHeight_) continue;

        RECT tileRc{tx, ty, tx + tileW, ty + tileH};
        bool isSel = (r == selectedRow_);
        bool isHov = (r == hoverRow_);

        drawAppleGlassCard(hdc, tileRc, 10,
            isSel ? RGB(20, 52, 86) : (isHov ? RGB(36, 44, 60) : RGB(26, 30, 42)),
            isSel ? RGB(14, 38, 64) : (isHov ? RGB(26, 32, 44) : RGB(20, 24, 34)),
            isSel ? RGB(0, 113, 227) : (isHov ? RGB(56, 68, 92) : RGB(44, 52, 72)),
            RGB(78, 92, 124), false);

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

void MainWindow::paintLargeIconsView(HDC hdc, int width, int height, bool isLarge) {
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    int surfaceLeft = sidebarWidth_;
    int rightMargin = (showDetailsPane_ ? detailsPaneWidth_ : (showPreviewPane_ ? previewPaneWidth_ : 0));
    int effWidth = width - rightMargin;

    paintFilterPills(hdc, width, contentTop);

    int startY = contentTop + filterBarHeight_ + 14;
    int cardW = isLarge ? 164 : 124;
    int cardH = isLarge ? 180 : 140;
    int thumbW = isLarge ? 144 : 108;
    int thumbH = isLarge ? 112 : 82;
    int gapX = 14;
    int gapY = 16;
    int cols = std::max(1, (effWidth - surfaceLeft - 24) / (cardW + gapX));

    SelectObject(hdc, fontRegular_);
    SetBkMode(hdc, TRANSPARENT);

    for (size_t i = 0; i < filteredIndices_.size(); ++i) {
        int r = static_cast<int>(i);
        int col = r % cols;
        int row = r / cols;
        int cx = surfaceLeft + 16 + col * (cardW + gapX);
        int cy = startY + row * (cardH + gapY) - scrollOffset_ * 20 + static_cast<int>(overscrollY_);

        if (cy + cardH < startY || cy > height - statusBarHeight_) continue;

        RECT cardRc{cx, cy, cx + cardW, cy + cardH};
        bool isSel = (r == selectedRow_);
        bool isHov = (r == hoverRow_);

        drawAppleGlassCard(hdc, cardRc, 12,
            isSel ? RGB(20, 52, 86) : (isHov ? RGB(36, 44, 60) : RGB(26, 30, 42)),
            isSel ? RGB(14, 38, 64) : (isHov ? RGB(26, 32, 44) : RGB(20, 24, 34)),
            isSel ? RGB(0, 113, 227) : (isHov ? RGB(56, 68, 92) : RGB(44, 52, 72)),
            RGB(78, 92, 124), false);

        const auto& item = allItems_[filteredIndices_[i]];
        std::filesystem::path fullPath = !item.fullPath.empty()
            ? std::filesystem::path(item.fullPath)
            : (isHomeView_ ? std::filesystem::path(item.location) / item.name : currentPath_ / item.name);

        // Thumbnail Frame inside card
        RECT thumbBox{cx + (cardW - thumbW) / 2, cy + 8, cx + (cardW - thumbW) / 2 + thumbW, cy + 8 + thumbH};
        COLORREF thumbBg = isSel ? RGB(18, 34, 54) : RGB(18, 19, 24);
        COLORREF thumbBdr = isSel ? RGB(60, 140, 200) : RGB(40, 44, 54);
        drawRoundedRect(hdc, thumbBox, 6, thumbBg, thumbBdr);

        // Render actual photo/image/video thumbnail
        std::wstring ext = toLower(item.extension);
        bool isImageOrVideo = (!item.isDirectory && (
            ext == L"png" || ext == L"jpg" || ext == L"jpeg" || ext == L"bmp" ||
            ext == L"webp" || ext == L"gif" || ext == L"ico" || ext == L"jfif" ||
            ext == L"mp4" || ext == L"mkv" || ext == L"avi" || ext == L"mov"));

        bool thumbDrawn = false;
        if (isImageOrVideo) {
            HBITMAP hBmp = getThumbnailBitmap(fullPath.wstring(), isLarge ? 256 : 160);
            if (hBmp) {
                // Inset by 2px inside thumbBox
                RECT drawRc{thumbBox.left + 2, thumbBox.top + 2, thumbBox.right - 2, thumbBox.bottom - 2};
                drawThumbnail(hdc, hBmp, drawRc);
                thumbDrawn = true;
            }
        }

        if (!thumbDrawn) {
            // Draw Shell Icon (48x48 or 32x32)
            HICON hIcon = getFileIcon(item.name, item.isDirectory, item.extension, isLarge);
            if (hIcon) {
                int iconSz = isLarge ? 48 : 36;
                int ix = thumbBox.left + (thumbW - iconSz) / 2;
                int iy = thumbBox.top + (thumbH - iconSz) / 2;
                DrawIconEx(hdc, ix, iy, hIcon, iconSz, iconSz, 0, nullptr, DI_NORMAL);
                DestroyIcon(hIcon);
            }
        }

        // Checkbox on top-left of card (Apple style pill checkbox)
        if (showCheckBoxes_ || (checkedRows_.size() > i && checkedRows_[i])) {
            RECT cbRc{cx + 8, cy + 10, cx + 26, cy + 28};
            bool isChecked = (checkedRows_.size() > i && checkedRows_[i]);
            COLORREF cbBg = isChecked ? RGB(96, 205, 255) : (isHov ? RGB(45, 45, 55) : RGB(30, 32, 40));
            COLORREF cbBdr = isChecked ? RGB(96, 205, 255) : RGB(80, 85, 100);
            drawRoundedRect(hdc, cbRc, 4, cbBg, cbBdr);
            if (isChecked) {
                SelectObject(hdc, fontIconsSmall_);
                SetTextColor(hdc, RGB(0, 0, 0));
                DrawTextW(hdc, L"\uE73E", 1, &cbRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }

        // File Name (with extension toggle applied)
        std::wstring displayName = item.name;
        if (!showExtensions_ && !item.isDirectory && !item.extension.empty()) {
            size_t dotPos = displayName.find_last_of(L'.');
            if (dotPos != std::wstring::npos) displayName = displayName.substr(0, dotPos);
        }

        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, isSel ? RGB(255, 255, 255) : (isHov ? RGB(245, 245, 245) : RGB(225, 225, 225)));
        RECT nameRc{cx + 6, cy + thumbH + 12, cx + cardW - 6, cy + cardH - 6};
        DrawTextW(hdc, displayName.c_str(), -1, &nameRc, DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
}

void MainWindow::paintDirectoryView(HDC hdc, int width, int height) {
    if (viewMode_ == ViewMode::Tiles) {
        paintTilesView(hdc, width, height);
        return;
    }
    if (viewMode_ == ViewMode::LargeIcons) {
        paintLargeIconsView(hdc, width, height, true);
        return;
    }
    if (viewMode_ == ViewMode::MediumIcons) {
        paintLargeIconsView(hdc, width, height, false);
        return;
    }

    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    int surfaceLeft = sidebarWidth_;
    int rightMargin = (showDetailsPane_ ? detailsPaneWidth_ : (showPreviewPane_ ? previewPaneWidth_ : 0));
    int effWidth = width - rightMargin;

    SetBkMode(hdc, TRANSPARENT);

    // 1. Instant 1-Click Quick Filter Bar / Search Results Header
    if (isSearching_) {
        RECT searchHdrRc{surfaceLeft + 8, contentTop + 4, effWidth - 8, contentTop + 36};
        drawRoundedRect(hdc, searchHdrRc, 8, RGB(20, 26, 40), RGB(52, 70, 108));

        // Radiant search icon
        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, RGB(96, 205, 255));
        RECT sIconRc{searchHdrRc.left + 10, searchHdrRc.top, searchHdrRc.left + 30, searchHdrRc.bottom};
        DrawTextW(hdc, L"\uE721", 1, &sIconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontBold_);
        SetTextColor(hdc, RGB(255, 255, 255));
        std::wstring sLoc = isHomeView_ ? L"Home" : currentPath_.filename().wstring();
        if (sLoc.empty()) sLoc = currentPath_.wstring();
        std::wstring sText = L"Search in \"" + sLoc + L"\" for \"" + currentSearchQuery_ + L"\"";

        bool showBanner = (!hasExactSearchMatches_ && !fuzzySuggestion_.empty());
        int bannerW = showBanner ? 360 : 0;
        int maxTextW = effWidth - 160 - (showBanner ? (bannerW + 16) : 0) - (searchHdrRc.left + 34);
        if (maxTextW < 120) maxTextW = 120;
        RECT sTextRc{searchHdrRc.left + 34, searchHdrRc.top, searchHdrRc.left + 34 + maxTextW, searchHdrRc.bottom};
        DrawTextW(hdc, sText.c_str(), -1, &sTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Result count pill
        RECT countPill{effWidth - 148, searchHdrRc.top + 4, effWidth - 16, searchHdrRc.bottom - 4};
        drawRoundedRect(hdc, countPill, 6, RGB(32, 42, 64), RGB(64, 86, 128));
        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(120, 205, 255));
        wchar_t countBuf[64];
        swprintf_s(countBuf, L"%zu results found", filteredIndices_.size());
        DrawTextW(hdc, countBuf, -1, &countPill, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Approximate recommendation banner
        if (showBanner) {
            RECT bannerRc{effWidth - 156 - bannerW, searchHdrRc.top + 4, effWidth - 156, searchHdrRc.bottom - 4};
            drawRoundedRect(hdc, bannerRc, 6, RGB(46, 36, 18), RGB(130, 95, 36));
            SetTextColor(hdc, RGB(255, 205, 90));
            std::wstring bannerText = L"✦ Approximate (Did you mean: " + fuzzySuggestion_ + L"?)";
            DrawTextW(hdc, bannerText.c_str(), -1, &bannerRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
    } else {
        paintFilterPills(hdc, width, contentTop);
    }

    // 2. Interactive Column Headers (with Sort Arrows)
    int tableHeaderTop = contentTop + filterBarHeight_;
    RECT hdrRc{surfaceLeft, tableHeaderTop, effWidth, tableHeaderTop + headerHeight_};
    HBRUSH hdrBrush = CreateSolidBrush(RGB(26, 26, 26));
    FillRect(hdc, &hdrRc, hdrBrush);
    DeleteObject(hdrBrush);

    SelectObject(hdc, fontSmall_);
    SetTextColor(hdc, RGB(140, 150, 170));
    SetTextCharacterExtra(hdc, 1); // Apple Optical Tracking §15

    int colStart = surfaceLeft + 36 + (showCheckBoxes_ ? 28 : 0);
    int curNameW = isSearching_ ? 280 : colNameWidth_;
    RECT colNameRc{colStart, tableHeaderTop, colStart + curNameW, tableHeaderTop + headerHeight_};
    RECT colLocRc{colNameRc.right, tableHeaderTop, colNameRc.right + (isSearching_ ? 200 : 0), tableHeaderTop + headerHeight_};
    RECT colDateRc{colLocRc.right, tableHeaderTop, colLocRc.right + (isSearching_ ? 160 : colDateWidth_), tableHeaderTop + headerHeight_};
    RECT colTypeRc{colDateRc.right, tableHeaderTop, colDateRc.right + (isSearching_ ? 120 : colTypeWidth_), tableHeaderTop + headerHeight_};
    RECT colSizeRc{colTypeRc.right, tableHeaderTop, colTypeRc.right + colSizeWidth_, tableHeaderTop + headerHeight_};

    std::wstring nameHdr = L"NAME" + std::wstring(sortCol_ == SortColumn::Name ? (sortAscending_ ? L" ▲" : L" ▼") : L"");
    std::wstring dateHdr = L"DATE MODIFIED" + std::wstring(sortCol_ == SortColumn::DateModified ? (sortAscending_ ? L" ▲" : L" ▼") : L"");
    std::wstring typeHdr = L"TYPE" + std::wstring(sortCol_ == SortColumn::Type ? (sortAscending_ ? L" ▲" : L" ▼") : L"");
    std::wstring sizeHdr = L"SIZE" + std::wstring(sortCol_ == SortColumn::Size ? (sortAscending_ ? L" ▲" : L" ▼") : L"");

    DrawTextW(hdc, nameHdr.c_str(), -1, &colNameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if (isSearching_) {
        DrawTextW(hdc, L"FOLDER LOCATION", 15, &colLocRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    DrawTextW(hdc, dateHdr.c_str(), -1, &colDateRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawTextW(hdc, typeHdr.c_str(), -1, &colTypeRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DrawTextW(hdc, sizeHdr.c_str(), -1, &colSizeRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SetTextCharacterExtra(hdc, 0);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(40, 40, 40));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, surfaceLeft, tableHeaderTop + headerHeight_ - 1, nullptr);
    LineTo(hdc, effWidth, tableHeaderTop + headerHeight_ - 1);
    DeleteObject(borderPen);

    // 3. File Rows with Apple Rubber-Banding (§9) and Fluid Selection (§1)
    int rowY = tableHeaderTop + headerHeight_ + static_cast<int>(overscrollY_);
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
            drawApplePill(hdc, rowRc, RGB(0, 113, 227), RGB(0, 113, 227), RGB(96, 175, 255), false);
        } else if (isHov) {
            drawApplePill(hdc, rowRc, RGB(30, 36, 50), RGB(46, 56, 78), RGB(68, 80, 108), false);
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
        int textMaxW = curNameW - (item.isApproximateMatch ? 86 : 10);
        RECT nameCell{colStart, rowRc.top, colStart + textMaxW, rowRc.bottom};
        DrawTextW(hdc, displayName.c_str(), -1, &nameCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Approximate recommendation badge
        if (item.isApproximateMatch) {
            RECT appRc{colStart + textMaxW + 4, rowRc.top + 10, colStart + curNameW - 6, rowRc.bottom - 10};
            drawRoundedRect(hdc, appRc, 4, RGB(46, 36, 18), RGB(120, 95, 45));
            SelectObject(hdc, fontSmall_);
            SetTextColor(hdc, RGB(255, 195, 75));
            DrawTextW(hdc, L"Approx", 6, &appRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // Location Column (in search mode)
        if (isSearching_) {
            SelectObject(hdc, fontSmall_);
            SetTextColor(hdc, RGB(140, 180, 220));
            RECT locCell{colLocRc.left, rowRc.top, colLocRc.right - 10, rowRc.bottom};
            std::wstring locDisplay = item.location;
            if (locDisplay.length() > 28) {
                locDisplay = L"..." + locDisplay.substr(locDisplay.length() - 25);
            }
            DrawTextW(hdc, locDisplay.c_str(), -1, &locCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }

        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(160, 160, 160));
        RECT dateCell{colDateRc.left, rowRc.top, colDateRc.right, rowRc.bottom};
        std::wstring dt = formatTime(item.mtime);
        DrawTextW(hdc, dt.c_str(), -1, &dateCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT typeCell{colTypeRc.left, rowRc.top, colTypeRc.right, rowRc.bottom};
        std::wstring typeStr = item.isDirectory ? L"File folder" : (item.extension.empty() ? L"File" : item.extension + L" File");
        DrawTextW(hdc, typeStr.c_str(), -1, &typeCell, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT sizeCell{colSizeRc.left, rowRc.top, colSizeRc.right, rowRc.bottom};
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

    // 1. Apple Frosted Obsidian Liquid Glass Background
    HBRUSH bgBrush = CreateSolidBrush(RGB(16, 18, 24));
    FillRect(hdc, &paneRc, bgBrush);
    DeleteObject(bgBrush);

    // 2. Liquid Glass Top Specular Gradient (Refraction Shimmer)
    for (int y = 0; y < 140; ++y) {
        int r = 16 + (32 - 16) * (140 - y) / 140;
        int g = 18 + (38 - 18) * (140 - y) / 140;
        int b = 24 + (52 - 24) * (140 - y) / 140;
        RECT gradLine{paneX, contentTop + y, width, contentTop + y + 1};
        HBRUSH gbr = CreateSolidBrush(RGB(r, g, b));
        FillRect(hdc, &gradLine, gbr);
        DeleteObject(gbr);
    }

    // 3. 1px Luminous specular edge line on the left
    HPEN topPen = CreatePen(PS_SOLID, 1, RGB(78, 92, 128));
    SelectObject(hdc, topPen);
    MoveToEx(hdc, paneX, contentTop, nullptr);
    LineTo(hdc, paneX, contentTop + 140);
    DeleteObject(topPen);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(42, 48, 64));
    SelectObject(hdc, borderPen);
    MoveToEx(hdc, paneX, contentTop + 140, nullptr);
    LineTo(hdc, paneX, height - statusBarHeight_);
    DeleteObject(borderPen);

    // Draggable Splitter Affordance Handle (Center of Left Edge - apple-design §2 Direct Manipulation)
    int midSplitterY = (contentTop + height - statusBarHeight_) / 2;
    RECT gripRc{paneX - 3, midSplitterY - 18, paneX + 3, midSplitterY + 18};
    drawApplePill(hdc, gripRc, RGB(34, 44, 62), RGB(0, 113, 227), RGB(96, 205, 255), false);

    SetBkMode(hdc, TRANSPARENT);

    // 4. Header Bar
    RECT iconRc{paneX + 18, contentTop + 14, paneX + 38, contentTop + 38};
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(0, 113, 227));
    DrawTextW(hdc, L"\uE8A1", 1, &iconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT titleRc{paneX + 42, contentTop + 14, paneX + 145, contentTop + 38};
    SelectObject(hdc, fontTitle_);
    SetTextColor(hdc, RGB(255, 255, 255));
    SetTextCharacterExtra(hdc, -1); // Apple Optical Tracking §15
    DrawTextW(hdc, L"Quick Look", 10, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SetTextCharacterExtra(hdc, 0);

    // Header Width Sizing Controls (◀ Widen, ⟲ Reset 420px, ▶ Narrow, × Close)
    btnPaneCloseRc_ = RECT{width - 34, contentTop + 14, width - 12, contentTop + 36};
    btnPaneNarrowRc_ = RECT{width - 60, contentTop + 14, width - 38, contentTop + 36};
    btnPaneResetRc_ = RECT{width - 86, contentTop + 14, width - 64, contentTop + 36};
    btnPaneWidenRc_ = RECT{width - 112, contentTop + 14, width - 90, contentTop + 36};

    auto drawHeaderBtn = [&](const RECT& rc, const wchar_t* glyph, COLORREF textCol, PressedTarget pt) {
        bool isPressed = (pressedTarget_ == pt);
        drawApplePill(hdc, rc, RGB(28, 34, 48), RGB(50, 60, 84), RGB(78, 92, 124), isPressed);
        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, textCol);
        DrawTextW(hdc, glyph, 1, const_cast<RECT*>(&rc), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    drawHeaderBtn(btnPaneWidenRc_, L"◀", RGB(160, 200, 255), PressedTarget::PaneWiden);
    drawHeaderBtn(btnPaneResetRc_, L"⟲", RGB(180, 190, 210), PressedTarget::PaneReset);
    drawHeaderBtn(btnPaneNarrowRc_, L"▶", RGB(160, 200, 255), PressedTarget::PaneNarrow);
    drawHeaderBtn(btnPaneCloseRc_, L"×", RGB(220, 140, 150), PressedTarget::PaneClose);

    // Determine target item: active hover item or selected item
    int targetIdx = (hoverRow_ >= 0 && hoverRow_ < static_cast<int>(filteredIndices_.size()))
        ? hoverRow_
        : selectedRow_;

    if (targetIdx < 0 || targetIdx >= static_cast<int>(filteredIndices_.size())) {
        // Liquid Glass Empty State
        RECT cardRc{paneX + 24, contentTop + 120, width - 24, contentTop + 300};
        drawRoundedRect(hdc, cardRc, 12, RGB(20, 24, 34), RGB(48, 56, 78));

        RECT eIconRc{cardRc.left, cardRc.top + 30, cardRc.right, cardRc.top + 80};
        SelectObject(hdc, fontIconsLarge_);
        SetTextColor(hdc, RGB(96, 120, 170));
        DrawTextW(hdc, L"\uE8A1", 1, &eIconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT msgRc{cardRc.left + 16, cardRc.top + 95, cardRc.right - 16, cardRc.top + 125};
        SelectObject(hdc, fontBold_);
        SetTextColor(hdc, RGB(225, 230, 245));
        DrawTextW(hdc, L"Quick Look Inspector", -1, &msgRc, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT subRc{cardRc.left + 16, cardRc.top + 128, cardRc.right - 16, cardRc.top + 175};
        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(145, 155, 175));
        DrawTextW(hdc, L"Select or hover over any document, photo, or folder to inspect contents in full Liquid Glass fidelity.", -1, &subRc, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        return;
    }

    const auto& item = allItems_[filteredIndices_[targetIdx]];
    std::filesystem::path fullPath = !item.fullPath.empty()
        ? std::filesystem::path(item.fullPath)
        : (isHomeView_ ? std::filesystem::path(item.location) / item.name : currentPath_ / item.name);

    std::wstring ext = toLower(item.extension);
    bool isPdf = (!item.isDirectory && ext == L"pdf");
    bool isImage = (!item.isDirectory && (ext == L"png" || ext == L"jpg" || ext == L"jpeg" || ext == L"bmp" || ext == L"webp" || ext == L"gif" || ext == L"ico" || ext == L"jfif" || ext == L"tiff"));
    bool isVideo = (!item.isDirectory && (ext == L"mp4" || ext == L"mkv" || ext == L"avi" || ext == L"mov" || ext == L"wmv" || ext == L"webm"));
    bool isAudio = (!item.isDirectory && (ext == L"mp3" || ext == L"wav" || ext == L"flac" || ext == L"aac" || ext == L"m4a"));
    bool isCode = (!item.isDirectory && (ext == L"txt" || ext == L"cpp" || ext == L"h" || ext == L"c" || ext == L"hpp" || ext == L"json" || ext == L"md" || ext == L"py" || ext == L"cmake" || ext == L"xml" || ext == L"log" || ext == L"csv" || ext == L"ini" || ext == L"bat" || ext == L"ps1" || ext == L"sh" || ext == L"yaml" || ext == L"yml" || ext == L"rs" || ext == L"js" || ext == L"ts" || ext == L"html" || ext == L"css"));

    // Glowing Category Pill in header
    COLORREF catPillBg = item.isDirectory ? RGB(48, 38, 18) : (isPdf ? RGB(52, 20, 28) : (isImage ? RGB(20, 38, 54) : (isVideo ? RGB(42, 26, 56) : (isCode ? RGB(18, 44, 32) : RGB(32, 36, 48)))));
    COLORREF catPillBdr = item.isDirectory ? RGB(120, 90, 36) : (isPdf ? RGB(170, 52, 68) : (isImage ? RGB(46, 92, 130) : (isVideo ? RGB(100, 60, 140) : (isCode ? RGB(40, 100, 70) : RGB(56, 64, 84)))));
    COLORREF catPillText = item.isDirectory ? RGB(255, 195, 70) : (isPdf ? RGB(255, 125, 145) : (isImage ? RGB(96, 205, 255) : (isVideo ? RGB(210, 140, 255) : (isCode ? RGB(90, 220, 150) : RGB(190, 200, 220)))));
    std::wstring catLabel = item.isDirectory ? L"Folder" : (isPdf ? L"PDF Document" : (isImage ? L"Photo" : (isVideo ? L"Video" : (isAudio ? L"Audio" : (isCode ? L"Document" : L"File")))));

    int pillW = static_cast<int>(catLabel.length() * 8 + 24);
    RECT catPill{paneX + 150, contentTop + 14, std::min<int>(static_cast<int>(btnPaneWidenRc_.left) - 8, paneX + 150 + pillW), contentTop + 36};
    drawRoundedRect(hdc, catPill, 11, catPillBg, catPillBdr);
    SelectObject(hdc, fontSmall_);
    SetTextColor(hdc, catPillText);
    DrawTextW(hdc, catLabel.c_str(), -1, &catPill, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // Hero File Name Title
    RECT nameRc{paneX + 18, contentTop + 48, width - 18, contentTop + 80};
    SelectObject(hdc, fontBold_);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, item.name.c_str(), -1, &nameRc, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);

    // Location Breadcrumb Pill
    std::wstring parentStr = fullPath.has_parent_path() ? fullPath.parent_path().filename().wstring() : L"";
    if (parentStr.empty()) parentStr = isHomeView_ ? item.location : currentPath_.filename().wstring();
    std::wstring locText = L"📁 " + parentStr;
    RECT locPill{paneX + 18, contentTop + 82, paneX + 18 + static_cast<int>(locText.length() * 8 + 20), contentTop + 104};
    drawRoundedRect(hdc, locPill, 10, RGB(24, 28, 38), RGB(46, 54, 72));
    SelectObject(hdc, fontSmall_);
    SetTextColor(hdc, RGB(160, 175, 205));
    DrawTextW(hdc, locText.c_str(), -1, &locPill, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    int curY = contentTop + 112;

    // Main Preview Viewport
    if (isImage || isVideo) {
        int frameH = 220;
        RECT frameRc{paneX + 16, curY, width - 16, curY + frameH};
        drawRoundedRect(hdc, frameRc, 10, RGB(12, 14, 18), RGB(52, 62, 86));

        // Specular top rim highlight line
        HPEN rimPen = CreatePen(PS_SOLID, 1, RGB(75, 88, 124));
        SelectObject(hdc, rimPen);
        MoveToEx(hdc, frameRc.left + 8, frameRc.top, nullptr);
        LineTo(hdc, frameRc.right - 8, frameRc.top);
        DeleteObject(rimPen);

        HBITMAP hBmp = getThumbnailBitmap(fullPath.wstring(), 512);
        if (hBmp) {
            RECT drawRc{frameRc.left + 4, frameRc.top + 4, frameRc.right - 4, frameRc.bottom - 4};
            drawThumbnail(hdc, hBmp, drawRc);
        } else {
            HICON hIcon = getFileIcon(item.name, item.isDirectory, item.extension, true);
            if (hIcon) {
                DrawIconEx(hdc, frameRc.left + (frameRc.right - frameRc.left - 64) / 2, frameRc.top + (frameH - 64) / 2, hIcon, 64, 64, 0, nullptr, DI_NORMAL);
                DestroyIcon(hIcon);
            }
        }

        curY += frameH + 12;

        // Overlay Dimension & Details Pill Bar
        uint32_t imgW = 0, imgH = 0;
        bool hasDim = getImageDimensions(fullPath.wstring(), imgW, imgH);
        if (hasDim) {
            wchar_t dimBuf[64];
            swprintf_s(dimBuf, L"%u × %u px", imgW, imgH);
            RECT dimPill{paneX + 16, curY, paneX + 140, curY + 28};
            drawRoundedRect(hdc, dimPill, 6, RGB(26, 32, 46), RGB(56, 68, 96));
            SelectObject(hdc, fontSmall_);
            SetTextColor(hdc, RGB(225, 230, 245));
            DrawTextW(hdc, dimBuf, -1, &dimPill, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        std::wstring extUpper = toUpper(ext) + (isImage ? L" Image" : L" Video");
        RECT extPill{hasDim ? paneX + 146 : paneX + 16, curY, hasDim ? paneX + 270 : paneX + 150, curY + 28};
        drawRoundedRect(hdc, extPill, 6, RGB(26, 32, 46), RGB(56, 68, 96));
        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(96, 205, 255));
        DrawTextW(hdc, extUpper.c_str(), -1, &extPill, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT szPill{hasDim ? paneX + 276 : paneX + 156, curY, width - 16, curY + 28};
        drawRoundedRect(hdc, szPill, 6, RGB(26, 32, 46), RGB(56, 68, 96));
        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(210, 215, 230));
        std::wstring szStr = formatSize(item.size);
        DrawTextW(hdc, szStr.c_str(), -1, &szPill, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        curY += 38;
    } else if (isPdf) {
        const auto& pdf = getOrLoadPdfInfo(fullPath.wstring());

        // 1. PDF Specs & Metric Capsule Bar
        int pillH = 26;
        int p1W = 100;
        RECT p1Rc{paneX + 16, curY, paneX + 16 + p1W, curY + pillH};
        drawRoundedRect(hdc, p1Rc, 6, RGB(38, 20, 26), RGB(120, 40, 52));
        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(255, 140, 160));
        wchar_t pgBuf[32];
        swprintf_s(pgBuf, L"📄 %d Pages", pdf.pageCount);
        DrawTextW(hdc, pgBuf, -1, &p1Rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        int p2W = 80;
        RECT p2Rc{p1Rc.right + 8, curY, p1Rc.right + 8 + p2W, curY + pillH};
        drawRoundedRect(hdc, p2Rc, 6, RGB(26, 32, 46), RGB(56, 68, 96));
        SetTextColor(hdc, RGB(96, 205, 255));
        DrawTextW(hdc, pdf.version.c_str(), -1, &p2Rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        int p3W = std::clamp(static_cast<int>(width - 16 - (p2Rc.right + 8) - 90), 60, 150);
        RECT p3Rc{p2Rc.right + 8, curY, p2Rc.right + 8 + p3W, curY + pillH};
        drawRoundedRect(hdc, p3Rc, 6, RGB(26, 32, 46), RGB(56, 68, 96));
        SetTextColor(hdc, RGB(210, 215, 230));
        DrawTextW(hdc, pdf.pageFormat.c_str(), -1, &p3Rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT p4Rc{p3Rc.right + 8, curY, width - 16, curY + pillH};
        drawRoundedRect(hdc, p4Rc, 6, RGB(26, 32, 46), RGB(56, 68, 96));
        SetTextColor(hdc, RGB(210, 215, 230));
        std::wstring szStr = formatSize(item.size);
        DrawTextW(hdc, szStr.c_str(), -1, &p4Rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        curY += pillH + 12;

        // 2. High-Fidelity Document Reader Canvas (Simulated Executive Document Paper)
        int boxBottom = height - statusBarHeight_ - 195;
        RECT docBox{paneX + 16, curY, width - 16, boxBottom};
        drawRoundedRect(hdc, docBox, 10, RGB(18, 22, 32), RGB(54, 64, 88));

        // Specular top rim
        HPEN rimPen = CreatePen(PS_SOLID, 1, RGB(80, 95, 130));
        SelectObject(hdc, rimPen);
        MoveToEx(hdc, docBox.left + 8, docBox.top, nullptr);
        LineTo(hdc, docBox.right - 8, docBox.top);
        DeleteObject(rimPen);

        // Header bar inside Document Reader Canvas
        int hH = 32;
        RECT inHeadRc{docBox.left, docBox.top, docBox.right, docBox.top + hH};
        drawRoundedRect(hdc, inHeadRc, 10, RGB(24, 28, 40), RGB(24, 28, 40));

        // Crimson PDF badge inside reader header
        RECT pdfBadgeRc{docBox.left + 12, docBox.top + 6, docBox.left + 54, docBox.top + 26};
        drawRoundedRect(hdc, pdfBadgeRc, 4, RGB(180, 36, 48), RGB(230, 60, 75));
        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextW(hdc, L"PDF", 3, &pdfBadgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Reader Title & Page Marker
        wchar_t rMarkerBuf[128];
        swprintf_s(rMarkerBuf, L"Document Content  •  Page 1 of %d", pdf.pageCount);
        RECT rMarkerRc{docBox.left + 62, docBox.top, docBox.right - 130, docBox.top + hH};
        SelectObject(hdc, fontBold_);
        SetTextColor(hdc, RGB(235, 240, 255));
        DrawTextW(hdc, rMarkerBuf, -1, &rMarkerRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Right word count
        wchar_t wCountBuf[64];
        swprintf_s(wCountBuf, L"%zu words", pdf.totalWords);
        RECT wCountRc{docBox.right - 120, docBox.top, docBox.right - 12, docBox.top + hH};
        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(150, 165, 190));
        DrawTextW(hdc, wCountBuf, -1, &wCountRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        // Divider
        HPEN divPen = CreatePen(PS_SOLID, 1, RGB(42, 50, 70));
        SelectObject(hdc, divPen);
        MoveToEx(hdc, docBox.left, docBox.top + hH, nullptr);
        LineTo(hdc, docBox.right, docBox.top + hH);
        DeleteObject(divPen);

        // Render Extracted Document Content Lines
        int textY = docBox.top + hH + 10;
        if (pdf.lines.empty()) {
            RECT noticeRc{docBox.left + 20, textY + 20, docBox.right - 20, textY + 80};
            SelectObject(hdc, fontBold_);
            SetTextColor(hdc, RGB(215, 225, 240));
            DrawTextW(hdc, pdf.isEncrypted ? L"🔒 Password Protected / Encrypted PDF" : L"📄 Scanned / Raster Image PDF Document", -1, &noticeRc, DT_CENTER | DT_NOPREFIX);

            RECT subNoticeRc{docBox.left + 20, textY + 50, docBox.right - 20, textY + 120};
            SelectObject(hdc, fontSmall_);
            SetTextColor(hdc, RGB(145, 155, 175));
            DrawTextW(hdc, pdf.isEncrypted ? L"This document requires credentials to decrypt content streams." : L"No embedded selectable text layer detected. Document contains scanned image pages.", -1, &subNoticeRc, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        } else {
            SelectObject(hdc, fontRegular_);
            size_t maxLinesToDraw = static_cast<size_t>(std::max<int>(0, static_cast<int>((docBox.bottom - textY - 26) / 20)));
            size_t drawCount = std::min(pdf.lines.size(), maxLinesToDraw);

            for (size_t li = 0; li < drawCount; ++li) {
                const auto& line = pdf.lines[li];
                RECT lineRc{docBox.left + 16, textY, docBox.right - 16, textY + 19};

                // Bold first line / heading
                if (li == 0) {
                    SelectObject(hdc, fontBold_);
                    SetTextColor(hdc, RGB(255, 255, 255));
                } else {
                    SelectObject(hdc, fontRegular_);
                    SetTextColor(hdc, RGB(218, 226, 240));
                }

                DrawTextW(hdc, line.c_str(), -1, &lineRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                textY += 20;
            }

            // Bottom Reader Status Bar inside canvas
            RECT readStatRc{docBox.left + 16, docBox.bottom - 22, docBox.right - 16, docBox.bottom - 4};
            SelectObject(hdc, fontSmall_);
            SetTextColor(hdc, RGB(130, 145, 170));
            wchar_t bStatBuf[128];
            swprintf_s(bStatBuf, L"Extracted text  •  %zu total lines  •  %zu characters", pdf.lines.size(), pdf.totalCharacters);
            DrawTextW(hdc, bStatBuf, -1, &readStatRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        curY = boxBottom + 10;
    } else if (isCode) {
        // Liquid Glass Code Inspector
        int boxBottom = height - statusBarHeight_ - 180;
        RECT codeBox{paneX + 16, curY, width - 16, boxBottom};
        drawRoundedRect(hdc, codeBox, 10, RGB(12, 14, 18), RGB(48, 56, 78));

        // Specular top rim
        HPEN rimPen = CreatePen(PS_SOLID, 1, RGB(70, 82, 114));
        SelectObject(hdc, rimPen);
        MoveToEx(hdc, codeBox.left + 8, codeBox.top, nullptr);
        LineTo(hdc, codeBox.right - 8, codeBox.top);
        DeleteObject(rimPen);

        // Gutter divider
        int gutterW = 38;
        RECT gutterRc{codeBox.left, codeBox.top, codeBox.left + gutterW, codeBox.bottom};
        drawRoundedRect(hdc, gutterRc, 10, RGB(16, 18, 24), RGB(16, 18, 24));
        HPEN divPen = CreatePen(PS_SOLID, 1, RGB(44, 50, 68));
        SelectObject(hdc, divPen);
        MoveToEx(hdc, codeBox.left + gutterW, codeBox.top, nullptr);
        LineTo(hdc, codeBox.left + gutterW, codeBox.bottom);
        DeleteObject(divPen);

        std::ifstream file(fullPath);
        if (file.is_open()) {
            std::string line;
            int textY = curY + 8;
            int lineNum = 1;
            SelectObject(hdc, fontCode_ ? fontCode_ : fontSmall_);

            while (std::getline(file, line) && lineNum <= 24 && textY < boxBottom - 20) {
                // Strip UTF-8 BOM if present on line 1
                if (lineNum == 1 && line.size() >= 3 &&
                    static_cast<unsigned char>(line[0]) == 0xEF &&
                    static_cast<unsigned char>(line[1]) == 0xBB &&
                    static_cast<unsigned char>(line[2]) == 0xBF) {
                    line.erase(0, 3);
                }

                // Line Number in Gutter
                wchar_t numBuf[16];
                swprintf_s(numBuf, L"%2d", lineNum);
                RECT numRc{codeBox.left + 4, textY, codeBox.left + gutterW - 6, textY + 18};
                SetTextColor(hdc, RGB(90, 100, 125));
                DrawTextW(hdc, numBuf, -1, &numRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

                // Syntax Highlighted Code Line
                int wlen = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, nullptr, 0);
                std::wstring wLine;
                if (wlen > 1) {
                    wLine.resize(wlen - 1);
                    MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, &wLine[0], wlen);
                }
                RECT lineRc{codeBox.left + gutterW + 8, textY, codeBox.right - 8, textY + 18};

                COLORREF lineCol = RGB(225, 230, 245);
                std::string trimL = line;
                while (!trimL.empty() && (trimL.front() == ' ' || trimL.front() == '\t')) trimL.erase(trimL.begin());
                if (trimL.rfind("//", 0) == 0 || trimL.rfind("/*", 0) == 0 || trimL.rfind("#", 0) == 0) {
                    lineCol = RGB(130, 140, 165); // Comment / Preprocessor
                } else if (trimL.find("class ") == 0 || trimL.find("struct ") == 0 || trimL.find("void ") == 0 ||
                           trimL.find("int ") == 0 || trimL.find("return ") == 0 || trimL.find("if ") == 0 ||
                           trimL.find("for ") == 0 || trimL.find("while ") == 0 || trimL.find("def ") == 0 ||
                           trimL.find("import ") == 0 || trimL.find("const ") == 0) {
                    lineCol = RGB(110, 205, 255); // Keyword highlight
                }

                SetTextColor(hdc, lineCol);
                DrawTextW(hdc, wLine.c_str(), -1, &lineRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                textY += 18;
                lineNum++;
            }
        }

        curY = boxBottom + 10;
    } else if (item.isDirectory) {
        // Liquid Glass Folder Inspector Card
        int frameH = 150;
        RECT frameRc{paneX + 16, curY, width - 16, curY + frameH};
        drawRoundedRect(hdc, frameRc, 10, RGB(14, 16, 22), RGB(52, 62, 86));

        // Specular top rim
        HPEN rimPen = CreatePen(PS_SOLID, 1, RGB(75, 88, 124));
        SelectObject(hdc, rimPen);
        MoveToEx(hdc, frameRc.left + 8, frameRc.top, nullptr);
        LineTo(hdc, frameRc.right - 8, frameRc.top);
        DeleteObject(rimPen);

        // Warm luminous folder icon
        RECT iconBox{frameRc.left + 16, frameRc.top + 16, frameRc.left + 64, frameRc.top + 64};
        drawRoundedRect(hdc, iconBox, 10, RGB(38, 30, 16), RGB(100, 75, 30));
        SelectObject(hdc, fontIconsLarge_);
        SetTextColor(hdc, RGB(255, 195, 70));
        DrawTextW(hdc, L"\uE8B7", 1, &iconBox, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Folder Title & Count
        std::error_code ec;
        size_t fileCount = 0;
        size_t dirCount = 0;
        std::vector<std::wstring> childNames;
        try {
            for (const auto& entry : std::filesystem::directory_iterator(fullPath, std::filesystem::directory_options::skip_permission_denied, ec)) {
                if (entry.is_directory(ec)) dirCount++;
                else fileCount++;
                if (childNames.size() < 3) {
                    childNames.push_back(entry.path().filename().wstring());
                }
            }
        } catch (...) {}

        RECT fTitleRc{frameRc.left + 76, frameRc.top + 18, frameRc.right - 12, frameRc.top + 40};
        SelectObject(hdc, fontBold_);
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextW(hdc, L"Directory Contents", -1, &fTitleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        wchar_t countBuf[128];
        swprintf_s(countBuf, L"%zu items (%zu files, %zu subfolders)", fileCount + dirCount, fileCount, dirCount);
        RECT fSubRc{frameRc.left + 76, frameRc.top + 42, frameRc.right - 12, frameRc.top + 62};
        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(160, 170, 195));
        DrawTextW(hdc, countBuf, -1, &fSubRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // Child Items Preview inside card
        if (!childNames.empty()) {
            int childY = frameRc.top + 74;
            for (size_t ci = 0; ci < childNames.size(); ++ci) {
                RECT cRc{frameRc.left + 16, childY, frameRc.right - 16, childY + 20};
                std::wstring cText = L"•  " + childNames[ci];
                SetTextColor(hdc, RGB(185, 195, 215));
                DrawTextW(hdc, cText.c_str(), -1, &cRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                childY += 22;
            }
        }

        curY += frameH + 12;
    } else {
        // Generic File Card
        int frameH = 130;
        RECT frameRc{paneX + 16, curY, width - 16, curY + frameH};
        drawRoundedRect(hdc, frameRc, 10, RGB(14, 16, 22), RGB(50, 58, 80));

        HICON hIcon = getFileIcon(item.name, item.isDirectory, item.extension, true);
        if (hIcon) {
            DrawIconEx(hdc, frameRc.left + (frameRc.right - frameRc.left - 64) / 2, frameRc.top + 16, hIcon, 64, 64, 0, nullptr, DI_NORMAL);
            DestroyIcon(hIcon);
        }

        RECT typeRc{frameRc.left + 10, frameRc.top + 88, frameRc.right - 10, frameRc.bottom - 8};
        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, RGB(220, 225, 240));
        std::wstring typeDesc = item.extension.empty() ? L"Binary File" : (toUpper(item.extension) + L" Document");
        DrawTextW(hdc, typeDesc.c_str(), -1, &typeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        curY += frameH + 12;
    }

    // Structured Metadata Liquid Glass Card
    int metaBottom = height - statusBarHeight_ - 62;
    if (metaBottom - curY >= 80) {
        RECT metaCard{paneX + 16, curY, width - 16, metaBottom};
        drawRoundedRect(hdc, metaCard, 8, RGB(18, 22, 30), RGB(44, 52, 72));

        int mY = curY + 8;
        SelectObject(hdc, fontSmall_);

        auto drawMetaRow = [&](const wchar_t* key, const std::wstring& val, COLORREF valCol = RGB(220, 225, 240)) {
            if (mY + 18 >= metaBottom) return;
            RECT kRc{metaCard.left + 12, mY, metaCard.left + 80, mY + 18};
            SetTextColor(hdc, RGB(130, 140, 160));
            DrawTextW(hdc, key, -1, &kRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            RECT vRc{metaCard.left + 85, mY, metaCard.right - 12, mY + 18};
            SetTextColor(hdc, valCol);
            DrawTextW(hdc, val.c_str(), -1, &vRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            mY += 22;
        };

        if (isPdf) {
            const auto& pdf = getOrLoadPdfInfo(fullPath.wstring());
            drawMetaRow(L"Title", pdf.title.empty() ? item.name : pdf.title);
            drawMetaRow(L"Author", pdf.author.empty() ? L"Not specified" : pdf.author);
            if (!pdf.creationDate.empty()) drawMetaRow(L"Created", pdf.creationDate);
            else drawMetaRow(L"Modified", formatTime(item.mtime));
            drawMetaRow(L"Pages", std::to_wstring(pdf.pageCount) + L" pages (" + pdf.pageFormat + L")");
            drawMetaRow(L"Security", pdf.isEncrypted ? L"Protected / Encrypted" : L"Standard (Open)", pdf.isEncrypted ? RGB(255, 140, 140) : RGB(140, 230, 170));
        } else {
            // Row 1: Size
            RECT kSizeRc{metaCard.left + 12, mY, metaCard.left + 80, mY + 18};
            SetTextColor(hdc, RGB(130, 140, 160));
            DrawTextW(hdc, L"Size", 4, &kSizeRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            wchar_t rawSizeBuf[64];
            if (item.isDirectory) swprintf_s(rawSizeBuf, L"Folder");
            else swprintf_s(rawSizeBuf, L"%s (%llu bytes)", formatSize(item.size).c_str(), static_cast<unsigned long long>(item.size));
            RECT vSizeRc{metaCard.left + 85, mY, metaCard.right - 12, mY + 18};
            SetTextColor(hdc, RGB(220, 225, 240));
            DrawTextW(hdc, rawSizeBuf, -1, &vSizeRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            mY += 22;

            // Row 2: Date Modified
            RECT kModRc{metaCard.left + 12, mY, metaCard.left + 80, mY + 18};
            SetTextColor(hdc, RGB(130, 140, 160));
            DrawTextW(hdc, L"Modified", 8, &kModRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            RECT vModRc{metaCard.left + 85, mY, metaCard.right - 12, mY + 18};
            SetTextColor(hdc, RGB(220, 225, 240));
            std::wstring modStr = formatTime(item.mtime);
            DrawTextW(hdc, modStr.c_str(), -1, &vModRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            mY += 22;

            // Row 3: Attributes
            if (mY + 18 < metaBottom) {
                RECT kAttrRc{metaCard.left + 12, mY, metaCard.left + 80, mY + 18};
                SetTextColor(hdc, RGB(130, 140, 160));
                DrawTextW(hdc, L"Attributes", 10, &kAttrRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                std::wstring attrStr;
                if (item.attributes & FILE_ATTRIBUTE_READONLY) attrStr += L"Read-only ";
                if (item.attributes & FILE_ATTRIBUTE_HIDDEN) attrStr += L"Hidden ";
                if (item.attributes & FILE_ATTRIBUTE_SYSTEM) attrStr += L"System ";
                if (attrStr.empty()) attrStr = L"Normal / Archive";

                RECT vAttrRc{metaCard.left + 85, mY, metaCard.right - 12, mY + 18};
                SetTextColor(hdc, RGB(160, 205, 245));
                DrawTextW(hdc, attrStr.c_str(), -1, &vAttrRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
        }
    }

    // Liquid Glass Action Dock (Bottom - apple-design §12 Translucent Action Dock)
    int btnY = height - statusBarHeight_ - 52;
    int availW = (width - 16) - (paneX + 16);
    int numBtns = (isPdf && previewPaneWidth_ >= 460) ? 4 : 3;
    int spacing = 8;
    int singleW = (availW - spacing * (numBtns - 1)) / numBtns;

    // 1. Open Button (Primary Apple Blue Capsule)
    RECT b1Rc{paneX + 16, btnY, paneX + 16 + singleW, btnY + 36};
    drawApplePill(hdc, b1Rc, RGB(0, 113, 227), RGB(0, 113, 227), RGB(96, 175, 255), pressedTarget_ == PressedTarget::ActionDockOpen);
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT b1Icon{b1Rc.left + 8, btnY, b1Rc.left + 26, btnY + 36};
    DrawTextW(hdc, L"\uE8A7", 1, &b1Icon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, fontRegular_);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT b1Text{b1Rc.left + 28, btnY, b1Rc.right - 6, btnY + 36};
    DrawTextW(hdc, isPdf ? L"Open PDF" : L"Open File", -1, &b1Text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // 2. In Folder Button (Secondary Frosted Capsule)
    RECT b2Rc{b1Rc.right + spacing, btnY, b1Rc.right + spacing + singleW, btnY + 36};
    drawApplePill(hdc, b2Rc, RGB(28, 34, 48), RGB(52, 62, 86), RGB(78, 92, 124), pressedTarget_ == PressedTarget::ActionDockInFolder);
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(255, 195, 70));
    RECT b2Icon{b2Rc.left + 8, btnY, b2Rc.left + 26, btnY + 36};
    DrawTextW(hdc, L"\uE8B7", 1, &b2Icon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, fontRegular_);
    SetTextColor(hdc, RGB(225, 230, 245));
    RECT b2Text{b2Rc.left + 28, btnY, b2Rc.right - 6, btnY + 36};
    DrawTextW(hdc, L"In Folder", 9, &b2Text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    if (numBtns == 4) {
        // 3. Copy Path Button
        RECT b3Rc{b2Rc.right + spacing, btnY, b2Rc.right + spacing + singleW, btnY + 36};
        drawApplePill(hdc, b3Rc, RGB(28, 34, 48), RGB(52, 62, 86), RGB(78, 92, 124), pressedTarget_ == PressedTarget::ActionDockCopyPath);
        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, RGB(160, 175, 205));
        RECT b3Icon{b3Rc.left + 8, btnY, b3Rc.left + 26, btnY + 36};
        DrawTextW(hdc, L"\uE8C8", 1, &b3Icon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, RGB(225, 230, 245));
        RECT b3Text{b3Rc.left + 28, btnY, b3Rc.right - 6, btnY + 36};
        DrawTextW(hdc, L"Copy Path", 9, &b3Text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        // 4. Copy Text Button
        RECT b4Rc{b3Rc.right + spacing, btnY, width - 16, btnY + 36};
        drawApplePill(hdc, b4Rc, RGB(38, 28, 52), RGB(160, 96, 255), RGB(200, 140, 255), pressedTarget_ == PressedTarget::ActionDockCopyText);
        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, RGB(210, 150, 255));
        RECT b4Icon{b4Rc.left + 8, btnY, b4Rc.left + 26, btnY + 36};
        DrawTextW(hdc, L"\uE8C8", 1, &b4Icon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, RGB(245, 230, 255));
        RECT b4Text{b4Rc.left + 28, btnY, b4Rc.right - 6, btnY + 36};
        DrawTextW(hdc, L"Copy Text", 9, &b4Text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    } else {
        // 3. Copy Text (or Copy Path if not PDF)
        RECT b3Rc{b2Rc.right + spacing, btnY, width - 16, btnY + 36};
        COLORREF bgC = isPdf ? RGB(38, 28, 52) : RGB(28, 34, 48);
        COLORREF bdrC = isPdf ? RGB(160, 96, 255) : RGB(52, 62, 86);
        COLORREF hiC = isPdf ? RGB(200, 140, 255) : RGB(78, 92, 124);
        drawApplePill(hdc, b3Rc, bgC, bdrC, hiC, pressedTarget_ == (isPdf ? PressedTarget::ActionDockCopyText : PressedTarget::ActionDockCopyPath));
        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, isPdf ? RGB(210, 150, 255) : RGB(160, 175, 205));
        RECT b3Icon{b3Rc.left + 8, btnY, b3Rc.left + 26, btnY + 36};
        DrawTextW(hdc, L"\uE8C8", 1, &b3Icon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, fontRegular_);
        SetTextColor(hdc, isPdf ? RGB(245, 230, 255) : RGB(225, 230, 245));
        RECT b3Text{b3Rc.left + 28, btnY, b3Rc.right - 6, btnY + 36};
        DrawTextW(hdc, isPdf ? L"Copy Text" : L"Copy Path", -1, &b3Text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
}

const PdfDocumentInfo& MainWindow::getOrLoadPdfInfo(const std::wstring& fullPath) {
    auto it = pdfCache_.find(fullPath);
    if (it != pdfCache_.end()) {
        return it->second;
    }
    PdfDocumentInfo info = PdfInspector::inspect(fullPath);
    auto [insertedIt, _] = pdfCache_.emplace(fullPath, std::move(info));
    return insertedIt->second;
}

void MainWindow::paintThisPcView(HDC hdc, int width, int height) {
    (void)height;
    int contentTop = tabBarHeight_ + navBarHeight_ + commandBarHeight_;
    int surfaceLeft = sidebarWidth_;
    int rightMargin = (showDetailsPane_ ? detailsPaneWidth_ : (showPreviewPane_ ? previewPaneWidth_ : 0));
    int effWidth = width - rightMargin;

    SetBkMode(hdc, TRANSPARENT);

    // 1. System Health & Storage Overview Header Card (The Whole Picture)
    RECT sysCardRc{surfaceLeft + 16, contentTop + 14, effWidth - 16, contentTop + 96};
    drawRoundedRect(hdc, sysCardRc, 10, RGB(27, 32, 42), RGB(46, 56, 76));

    // Computer Icon
    SelectObject(hdc, fontIcons_);
    SetTextColor(hdc, RGB(96, 205, 255));
    RECT pcGlyphRc{sysCardRc.left + 16, sysCardRc.top, sysCardRc.left + 54, sysCardRc.bottom};
    DrawTextW(hdc, L"\uE7F4", 1, &pcGlyphRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Computer Name & Specs
    WCHAR compName[MAX_COMPUTERNAME_LENGTH + 1]{};
    DWORD compLen = MAX_COMPUTERNAME_LENGTH + 1;
    GetComputerNameW(compName, &compLen);
    std::wstring compStr = (compName[0] != L'\0') ? compName : L"Local PC";

    MEMORYSTATUSEX memStatus{};
    memStatus.dwLength = sizeof(memStatus);
    GlobalMemoryStatusEx(&memStatus);
    uint64_t totalRamGB = (memStatus.ullTotalPhys + (1024ULL * 1024 * 1024 / 2)) / (1024ULL * 1024 * 1024);

    SelectObject(hdc, fontTitle_);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT pcNameRc{sysCardRc.left + 62, sysCardRc.top + 14, sysCardRc.left + 340, sysCardRc.top + 40};
    DrawTextW(hdc, compStr.c_str(), -1, &pcNameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, fontSmall_);
    SetTextColor(hdc, RGB(160, 175, 195));
    wchar_t specBuf[128];
    swprintf_s(specBuf, L"Windows 11 NT 10.0  •  %llu GB Physical RAM  •  64-bit Architecture", totalRamGB);
    RECT specRc{sysCardRc.left + 62, sysCardRc.top + 44, sysCardRc.left + 480, sysCardRc.top + 68};
    DrawTextW(hdc, specBuf, -1, &specRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // Enumerate drives & calculate aggregate storage
    thisPcDrives_.clear();
    WCHAR driveStrings[512]{};
    DWORD len = GetLogicalDriveStringsW(511, driveStrings);
    uint64_t totalAllStorage = 0;
    uint64_t totalAllFree = 0;

    if (len > 0 && len <= 511) {
        const WCHAR* ptr = driveStrings;
        while (*ptr) {
            std::wstring root = ptr;
            DriveCardItem di;
            di.root = root;
            WCHAR volName[MAX_PATH + 1]{};
            WCHAR fsName[MAX_PATH + 1]{};
            GetVolumeInformationW(root.c_str(), volName, MAX_PATH, nullptr, nullptr, nullptr, fsName, MAX_PATH);
            di.label = (volName[0] != L'\0') ? volName : L"Local Disk";
            di.fileSystem = (fsName[0] != L'\0') ? fsName : L"NTFS";
            ULARGE_INTEGER freeBytes{}, totalBytes{};
            if (GetDiskFreeSpaceExW(root.c_str(), &freeBytes, &totalBytes, nullptr)) {
                di.totalBytes = totalBytes.QuadPart;
                di.freeBytes = freeBytes.QuadPart;
                totalAllStorage += totalBytes.QuadPart;
                totalAllFree += freeBytes.QuadPart;
            }
            thisPcDrives_.push_back(std::move(di));
            ptr += wcslen(ptr) + 1;
        }
    }

    // Right side of Header Card: Total Aggregated Storage + Health Pill
    RECT healthPillRc{sysCardRc.right - 170, sysCardRc.top + 16, sysCardRc.right - 20, sysCardRc.top + 42};
    drawRoundedRect(hdc, healthPillRc, 6, RGB(16, 45, 30), RGB(24, 120, 68));
    SelectObject(hdc, fontSmall_);
    SetTextColor(hdc, RGB(52, 211, 153));
    DrawTextW(hdc, L"● System Healthy", -1, &healthPillRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT aggStorageRc{sysCardRc.right - 360, sysCardRc.top + 46, sysCardRc.right - 20, sysCardRc.top + 68};
    wchar_t aggBuf[128];
    double freePct = (totalAllStorage > 0) ? (100.0 * static_cast<double>(totalAllFree) / static_cast<double>(totalAllStorage)) : 0.0;
    swprintf_s(aggBuf, L"Capacity: %s  |  %s Free (%.0f%%)",
        formatSize(totalAllStorage).c_str(), formatSize(totalAllFree).c_str(), freePct);
    SetTextColor(hdc, RGB(200, 215, 235));
    DrawTextW(hdc, aggBuf, -1, &aggStorageRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // 2. Section: Devices and Drives
    int driveSectionTop = sysCardRc.bottom + 18;
    RECT dTitleChevRc{surfaceLeft + 16, driveSectionTop, surfaceLeft + 32, driveSectionTop + 24};
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(180, 180, 180));
    DrawTextW(hdc, L"\uE70D", 1, &dTitleChevRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT dTitleRc{surfaceLeft + 36, driveSectionTop, surfaceLeft + 320, driveSectionTop + 24};
    SelectObject(hdc, fontBold_);
    SetTextColor(hdc, RGB(245, 245, 245));
    wchar_t dHdrBuf[64];
    swprintf_s(dHdrBuf, L"Devices and drives (%zu)", thisPcDrives_.size());
    DrawTextW(hdc, dHdrBuf, -1, &dTitleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    int cardGap = 16;
    int driveCols = std::clamp((effWidth - surfaceLeft - 32) / 340, 1, 3);
    int driveCardW = (effWidth - surfaceLeft - 32 - (driveCols - 1) * cardGap) / driveCols;
    int driveCardH = 96;
    int driveStartY = driveSectionTop + 32;

    for (size_t i = 0; i < thisPcDrives_.size(); ++i) {
        int col = static_cast<int>(i % driveCols);
        int row = static_cast<int>(i / driveCols);
        int cx = surfaceLeft + 16 + col * (driveCardW + cardGap);
        int cy = driveStartY + row * (driveCardH + cardGap);

        auto& drive = thisPcDrives_[i];
        drive.rect = RECT{cx, cy, cx + driveCardW, cy + driveCardH};

        drawRoundedRect(hdc, drive.rect, 8, RGB(33, 33, 33), RGB(48, 48, 48));

        // Drive Glyph (large radiant vector)
        SelectObject(hdc, fontIcons_);
        SetTextColor(hdc, RGB(96, 205, 255));
        RECT iconBox{cx + 12, cy + 14, cx + 56, cy + 58};
        DrawTextW(hdc, L"\uEDA2", 1, &iconBox, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Drive Label + Letter
        std::wstring driveTitle = drive.label + L" (" + drive.root.substr(0, 2) + L")";
        SelectObject(hdc, fontBold_);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT nameRc{cx + 64, cy + 14, cx + driveCardW - 60, cy + 34};
        DrawTextW(hdc, driveTitle.c_str(), -1, &nameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        // FileSystem Tag
        RECT fsRc{cx + driveCardW - 54, cy + 14, cx + driveCardW - 14, cy + 32};
        drawRoundedRect(hdc, fsRc, 4, RGB(24, 38, 54), RGB(40, 70, 105));
        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(120, 205, 255));
        DrawTextW(hdc, drive.fileSystem.c_str(), -1, &fsRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Storage Progress Bar
        int barX = cx + 64;
        int barY = cy + 42;
        int barW = driveCardW - 78;
        int barH = 7;
        RECT barTrackRc{barX, barY, barX + barW, barY + barH};
        drawRoundedRect(hdc, barTrackRc, 3, RGB(45, 45, 45), RGB(45, 45, 45));

        double usedPct = 0;
        if (drive.totalBytes > 0) {
            uint64_t usedBytes = (drive.totalBytes > drive.freeBytes) ? (drive.totalBytes - drive.freeBytes) : 0;
            usedPct = static_cast<double>(usedBytes) / static_cast<double>(drive.totalBytes);
        }
        int fillW = std::clamp(static_cast<int>(barW * usedPct), 0, barW);
        COLORREF barFill = (usedPct > 0.95) ? RGB(232, 17, 35) : ((usedPct > 0.85) ? RGB(255, 185, 0) : RGB(0, 153, 255));
        if (fillW > 0) {
            RECT barFillRc{barX, barY, barX + fillW, barY + barH};
            drawRoundedRect(hdc, barFillRc, 3, barFill, barFill);
        }

        // Subtext: Free of Total
        wchar_t capBuf[96];
        swprintf_s(capBuf, L"%s free of %s", formatSize(drive.freeBytes).c_str(), formatSize(drive.totalBytes).c_str());
        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(160, 160, 160));
        RECT capRc{barX, cy + 57, cx + driveCardW - 14, cy + 77};
        DrawTextW(hdc, capBuf, -1, &capRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    int numDriveRows = (static_cast<int>(thisPcDrives_.size()) + driveCols - 1) / driveCols;
    int nextSectionTop = driveStartY + numDriveRows * (driveCardH + cardGap) + 16;

    // 3. Section: System Folders (Desktop, Downloads, Documents, Pictures, Music, Videos)
    RECT fTitleChevRc{surfaceLeft + 16, nextSectionTop, surfaceLeft + 32, nextSectionTop + 24};
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(180, 180, 180));
    DrawTextW(hdc, L"\uE70D", 1, &fTitleChevRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT fTitleRc{surfaceLeft + 36, nextSectionTop, surfaceLeft + 320, nextSectionTop + 24};
    SelectObject(hdc, fontBold_);
    SetTextColor(hdc, RGB(245, 245, 245));
    DrawTextW(hdc, L"Folders (6)", -1, &fTitleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    if (thisPcFolderCards_.empty()) {
        thisPcFolderCards_ = {
            { L"Desktop", L"Personal folder", pathDesktop_, L"\uE7F4", RGB(0, 120, 215), {} },
            { L"Downloads", L"User downloads", pathDownloads_, L"\uE896", RGB(0, 178, 148), {} },
            { L"Documents", L"Personal documents", pathDocuments_, L"\uE8A5", RGB(110, 130, 155), {} },
            { L"Pictures", L"Photo library", pathPictures_, L"\uEB9F", RGB(0, 153, 255), {} },
            { L"Music", L"Audio collection", pathMusic_, L"\uE8D6", RGB(255, 110, 90), {} },
            { L"Videos", L"Video library", pathVideos_, L"\uE714", RGB(160, 80, 220), {} },
        };
    }

    int fCols = std::clamp((effWidth - surfaceLeft - 32) / 220, 1, 6);
    int fCardW = (effWidth - surfaceLeft - 32 - (fCols - 1) * cardGap) / fCols;
    int fCardH = 64;
    int fStartY = nextSectionTop + 32;

    for (size_t i = 0; i < thisPcFolderCards_.size(); ++i) {
        int col = static_cast<int>(i % fCols);
        int row = static_cast<int>(i / fCols);
        int cx = surfaceLeft + 16 + col * (fCardW + cardGap);
        int cy = fStartY + row * (fCardH + 12);

        thisPcFolderCards_[i].rect = RECT{cx, cy, cx + fCardW, cy + fCardH};
        drawRoundedRect(hdc, thisPcFolderCards_[i].rect, 8, RGB(33, 33, 33), RGB(48, 48, 48));

        RECT bRc{cx + 10, cy + 10, cx + 50, cy + 50};
        drawRoundedRect(hdc, bRc, 6, RGB(38, 38, 38), RGB(48, 48, 48));

        SelectObject(hdc, fontIcons_);
        SetTextColor(hdc, thisPcFolderCards_[i].badgeColor);
        DrawTextW(hdc, thisPcFolderCards_[i].iconGlyph.c_str(), 1, &bRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontBold_);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT nRc{cx + 58, cy + 12, cx + fCardW - 10, cy + 32};
        DrawTextW(hdc, thisPcFolderCards_[i].name.c_str(), -1, &nRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(150, 150, 150));
        RECT sRc{cx + 58, cy + 34, cx + fCardW - 10, cy + 52};
        DrawTextW(hdc, thisPcFolderCards_[i].subtext.c_str(), -1, &sRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    int numFolderRows = (static_cast<int>(thisPcFolderCards_.size()) + fCols - 1) / fCols;
    int toolSectionTop = fStartY + numFolderRows * (fCardH + 12) + 16;

    // 4. Section: Storage Maintenance & Optimization Tools
    RECT mTitleChevRc{surfaceLeft + 16, toolSectionTop, surfaceLeft + 32, toolSectionTop + 24};
    SelectObject(hdc, fontIconsSmall_);
    SetTextColor(hdc, RGB(180, 180, 180));
    DrawTextW(hdc, L"\uE70D", 1, &mTitleChevRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT mTitleRc{surfaceLeft + 36, toolSectionTop, surfaceLeft + 420, toolSectionTop + 24};
    SelectObject(hdc, fontBold_);
    SetTextColor(hdc, RGB(245, 245, 245));
    DrawTextW(hdc, L"Storage Maintenance & Optimization Tools", -1, &mTitleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    if (thisPcTools_.empty()) {
        thisPcTools_ = {
            { L"Disk Cleanup", L"Purge temporary system & cache files", L"\uE74D", RGB(96, 205, 255), static_cast<int>(kCmdDiskCleanup), {} },
            { L"Storage Sense", L"Configure automated Windows cleanup", L"\uE945", RGB(52, 211, 153), static_cast<int>(kCmdStorageSettings), {} },
            { L"Open PowerShell", L"Launch high-privilege console in C:\\", L"\uE756", RGB(255, 185, 0), static_cast<int>(kCmdOpenPowershell), {} },
            { L"Large Files (>100MB)", L"Scan root volume for space hogs", L"\uE721", RGB(168, 85, 247), static_cast<int>(kCmdFilterLargeFiles), {} }
        };
    }

    int tCols = std::clamp((effWidth - surfaceLeft - 32) / 250, 1, 4);
    int tCardW = (effWidth - surfaceLeft - 32 - (tCols - 1) * cardGap) / tCols;
    int tCardH = 68;
    int tStartY = toolSectionTop + 32;

    for (size_t i = 0; i < thisPcTools_.size(); ++i) {
        int col = static_cast<int>(i % tCols);
        int row = static_cast<int>(i / tCols);
        int cx = surfaceLeft + 16 + col * (tCardW + cardGap);
        int cy = tStartY + row * (tCardH + 12);

        thisPcTools_[i].rect = RECT{cx, cy, cx + tCardW, cy + tCardH};
        drawRoundedRect(hdc, thisPcTools_[i].rect, 8, RGB(30, 34, 44), RGB(48, 56, 72));

        RECT tbRc{cx + 10, cy + 12, cx + 52, cy + 54};
        drawRoundedRect(hdc, tbRc, 6, RGB(36, 42, 56), RGB(50, 60, 80));

        SelectObject(hdc, fontIcons_);
        SetTextColor(hdc, thisPcTools_[i].color);
        DrawTextW(hdc, thisPcTools_[i].glyph.c_str(), 1, &tbRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, fontBold_);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT tnRc{cx + 60, cy + 14, cx + tCardW - 10, cy + 34};
        DrawTextW(hdc, thisPcTools_[i].title.c_str(), -1, &tnRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(160, 175, 195));
        RECT tdRc{cx + 60, cy + 36, cx + tCardW - 10, cy + 56};
        DrawTextW(hdc, thisPcTools_[i].desc.c_str(), -1, &tdRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
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
    if (isThisPcView_) {
        swprintf_s(statusBuf, L"This PC  |  Devices & Drives  |  GodXplr Storage & System Health Lens");
    } else if (isSearching_) {
        swprintf_s(statusBuf, L"%zu results found for \"%s\"  |  %s  |  GodXplr Local Engine",
            filteredIndices_.size(),
            currentSearchQuery_.c_str(),
            hasExactSearchMatches_ ? L"Deep Recursive Search" : L"Intelligent Fuzzy Recommendation Engine");
    } else if (selectedRow_ >= 0 && selectedRow_ < static_cast<int>(filteredIndices_.size())) {
        const auto& item = allItems_[filteredIndices_[selectedRow_]];
        swprintf_s(statusBuf, L"%zu items  |  1 item selected (%s)  |  GodXplr Local Engine",
            filteredIndices_.size(), formatSize(item.size).c_str());
    } else {
        swprintf_s(statusBuf, L"%zu items  |  GodXplr High-Performance Local Engine (Zero AI / Zero Cloud)",
            filteredIndices_.size());
    }

    RECT textRc{16, height - statusBarHeight_, width - 360, height};
    DrawTextW(hdc, statusBuf, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // Right Side: Drive Storage Telemetry
    ULARGE_INTEGER freeBytes{}, totalBytes{};
    std::wstring driveRoot = (isHomeView_ || isThisPcView_ || currentPath_.empty()) ? L"C:\\" : (currentPath_.root_path().empty() ? L"C:\\" : currentPath_.root_path().wstring());
    if (GetDiskFreeSpaceExW(driveRoot.c_str(), &freeBytes, &totalBytes, nullptr)) {
        wchar_t storageBuf[128];
        swprintf_s(storageBuf, L"%s %s free of %s",
            driveRoot.substr(0, 2).c_str(),
            formatSize(freeBytes.QuadPart).c_str(),
            formatSize(totalBytes.QuadPart).c_str());

        RECT driveTextRc{width - 340, height - statusBarHeight_, width - 110, height};
        SelectObject(hdc, fontSmall_);
        SetTextColor(hdc, RGB(165, 175, 190));
        DrawTextW(hdc, storageBuf, -1, &driveTextRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    // Quick View Mode Switcher Buttons: Details, Tiles, Large Icons
    int btnW = 28;
    int btnH = 22;
    int btnY = height - statusBarHeight_ + (statusBarHeight_ - btnH) / 2;

    statusViewDetailsRc_ = RECT{width - 96, btnY, width - 96 + btnW, btnY + btnH};
    statusViewTilesRc_ = RECT{width - 64, btnY, width - 64 + btnW, btnY + btnH};
    statusViewLargeRc_ = RECT{width - 32, btnY, width - 32 + btnW, btnY + btnH};

    auto drawViewBtn = [&](const RECT& rc, const wchar_t* glyph, bool isActive) {
        if (isActive) {
            drawRoundedRect(hdc, rc, 4, RGB(25, 48, 72), RGB(96, 205, 255));
        } else {
            drawRoundedRect(hdc, rc, 4, RGB(28, 28, 28), RGB(40, 40, 40));
        }
        SelectObject(hdc, fontIconsSmall_);
        SetTextColor(hdc, isActive ? RGB(96, 205, 255) : RGB(160, 160, 160));
        DrawTextW(hdc, glyph, 1, const_cast<RECT*>(&rc), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    drawViewBtn(statusViewDetailsRc_, L"\uE8BC", viewMode_ == ViewMode::Details);
    drawViewBtn(statusViewTilesRc_, L"\uE8A5", viewMode_ == ViewMode::Tiles);
    drawViewBtn(statusViewLargeRc_, L"\uE74C", viewMode_ == ViewMode::LargeIcons);
}

void MainWindow::onPaint(HDC hdc) {
    RECT clientRc;
    GetClientRect(hwnd_, &clientRc);
    int width = clientRc.right;
    int height = clientRc.bottom;

    // Double-buffered rendering to prevent any flicker (using window DC as color reference)
    HDC refDC = GetDC(hwnd_);
    HDC memDC = CreateCompatibleDC(refDC ? refDC : hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(refDC ? refDC : hdc, width, height);
    if (refDC) ReleaseDC(hwnd_, refDC);
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

    // 5. Main Surface (This PC vs Home View vs Directory View)
    if (isThisPcView_) {
        paintThisPcView(memDC, width, height);
    } else if (isHomeView_ && !isSearching_) {
        paintHomeView(memDC, width, height);
    } else if (viewMode_ == ViewMode::Tiles) {
        paintTilesView(memDC, width, height);
    } else if (viewMode_ == ViewMode::LargeIcons || viewMode_ == ViewMode::MediumIcons) {
        paintLargeIconsView(memDC, width, height, viewMode_ == ViewMode::LargeIcons);
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
