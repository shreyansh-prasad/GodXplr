#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <knownfolders.h>

#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <functional>

#include "godxplr/ui/theme/design_tokens.h"

namespace gx::app {

enum class ViewMode {
    Details,
    List,
    Grid
};

enum class SortColumn {
    Name,
    DateModified,
    Type,
    Size
};

enum class FilterCategory {
    All,
    Folders,
    Documents,
    Pictures,
    AudioVideo,
    Compressed,
    Code
};

struct FileItemDisplay {
    std::wstring name;
    std::wstring extension;
    std::wstring location; // For Home/Recent view
    uint64_t size{0};
    uint64_t mtime{0};
    uint32_t attributes{0};
    bool isDirectory{false};
    HICON hIcon{nullptr};
};

struct SidebarItem {
    std::wstring name;
    std::wstring path;
    std::wstring iconGlyph;
    bool isDrive{false};
    bool isHeader{false};
};

struct TabInfo {
    std::wstring title;
    std::wstring path;
};

struct BreadcrumbPart {
    std::wstring label;
    std::wstring path;
    RECT rect;
};

class MainWindow {
public:
    static bool registerWindowClass(HINSTANCE hInstance);
    static MainWindow* create(HINSTANCE hInstance, const std::wstring& initialPath = L"");

    ~MainWindow();
    HWND hwnd() const noexcept { return hwnd_; }
    void show(int nCmdShow);

private:
    explicit MainWindow(HINSTANCE hInstance);

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Message Handlers
    void onPaint(HDC hdc);
    void onResize(int width, int height);
    void onVScroll(WPARAM wParam);
    void onMouseWheel(short delta);
    void onLButtonDown(int x, int y, bool isDoubleClick);
    void onRButtonUp(int x, int y);
    void onMouseMove(int x, int y);
    void onKeyDown(WPARAM key);
    void onSearchChanged();

    // Painting Subroutines
    void paintTabBar(HDC hdc, int width);
    void paintNavAndAddressBar(HDC hdc, int width);
    void paintCommandBar(HDC hdc, int width);
    void paintSidebar(HDC hdc, int height);
    void paintHomeView(HDC hdc, int width, int height);
    void paintDirectoryView(HDC hdc, int width, int height);
    void paintStatusBar(HDC hdc, int width, int height);

    // Navigation & Operations
    void navigateTo(const std::filesystem::path& path);
    void navigateToHome();
    void navigateUp();
    void navigateBack();
    void navigateForward();
    void refresh();
    void openSelectedItem();
    void openItem(const FileItemDisplay& item);

    // Power & Command Ribbon Actions
    void createNewFolder();
    void createNewTextFile();
    void deleteSelectedItem();
    void copySelectedItem();
    void cutSelectedItem();
    void pasteItem();
    void renameSelectedItem();

    // Sorting & Filtering
    void setSortColumn(SortColumn col);
    void setFilterCategory(FilterCategory cat);
    void filterAndSortItems();
    void updateScrollInfo();

    // Context Menus & Popups
    void showSortMenu(int x, int y);
    void showViewMenu(int x, int y);
    void showFilterMenu(int x, int y);
    void showNewMenu(int x, int y);

    // Helpers
    void initSidebar();
    void initKnownFolders();
    HICON getFileIcon(const std::wstring& path, bool isDir, const std::wstring& ext);

    HINSTANCE hInstance_{nullptr};
    HWND hwnd_{nullptr};
    HWND editAddress_{nullptr};
    HWND editSearch_{nullptr};

    std::filesystem::path currentPath_;
    bool isHomeView_{true};

    std::vector<TabInfo> tabs_;
    size_t activeTab_{0};

    std::vector<std::filesystem::path> backStack_;
    std::vector<std::filesystem::path> forwardStack_;

    std::vector<SidebarItem> sidebarItems_;
    int sidebarHoverIndex_{-1};
    int sidebarSelectedIndex_{-1};

    std::vector<FileItemDisplay> allItems_;
    std::vector<size_t> filteredIndices_;

    std::vector<BreadcrumbPart> breadcrumbs_;
    bool addressEditActive_{false};

    // Quick Access cards for Home view
    struct QuickCard {
        std::wstring name;
        std::wstring subtext;
        std::wstring path;
        std::wstring iconGlyph;
        RECT rect;
    };
    std::vector<QuickCard> quickCards_;

    int selectedRow_{-1};
    int hoverRow_{-1};
    int hoverCard_{-1};
    int scrollOffset_{0};

    // Layout Dimensions
    int tabBarHeight_{36};
    int navBarHeight_{42};
    int commandBarHeight_{44};
    int headerHeight_{28};
    int rowHeight_{30};
    int statusBarHeight_{26};
    int sidebarWidth_{220};

    // Column Widths
    int colNameWidth_{340};
    int colDateWidth_{160};
    int colTypeWidth_{130};
    int colSizeWidth_{100};

    // Modes
    ViewMode viewMode_{ViewMode::Details};
    SortColumn sortCol_{SortColumn::Name};
    bool sortAscending_{true};
    FilterCategory filterCategory_{FilterCategory::All};

    // GDI Resources
    HFONT fontRegular_{nullptr};
    HFONT fontBold_{nullptr};
    HFONT fontTitle_{nullptr};
    HFONT fontSmall_{nullptr};

    ui::DesignTokens tokens_;

    // Known Folders cache
    std::wstring pathDesktop_;
    std::wstring pathDownloads_;
    std::wstring pathDocuments_;
    std::wstring pathPictures_;
    std::wstring pathMusic_;
    std::wstring pathVideos_;
    std::wstring pathHome_;
};

} // namespace gx::app
