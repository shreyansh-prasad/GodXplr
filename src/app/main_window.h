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
#include <unordered_map>

#include "godxplr/ui/theme/design_tokens.h"

namespace gx::app {

enum class ViewMode {
    Details,
    Tiles,
    LargeIcons,
    MediumIcons,
    List,
    Grid = LargeIcons
};

struct CachedThumbnail {
    HBITMAP hBmp{nullptr};
    int width{0};
    int height{0};
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

enum class DateFilter {
    All,
    Today,
    Yesterday,
    ThisWeek,
    ThisMonth,
    ThisYear
};

enum class SizeFilter {
    All,
    Empty,   // 0 KB
    Tiny,    // 0 - 16 KB
    Small,   // 16 KB - 1 MB
    Medium,  // 1 - 128 MB
    Large,   // 128 MB - 1 GB
    Huge     // > 1 GB
};

struct FileItemDisplay {
    std::wstring name;
    std::wstring extension;
    std::wstring location; // For Home/Recent view display
    std::wstring fullPath; // Full absolute filesystem path
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
    COLORREF iconColor{RGB(200, 200, 200)};
    std::wstring subtext; // e.g. "142 GB free"
    bool isHeader{false};
    bool isDrive{false};
    int indentLevel{0};
    bool isExpandable{false};
    bool isExpanded{false};
    bool isLoaded{false};
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

struct FilterPill {
    FilterCategory cat;
    std::wstring label;
    std::wstring iconGlyph;
    RECT rect;
};

struct QuickCard {
    std::wstring name;
    std::wstring subtext;
    std::wstring path;
    std::wstring iconGlyph;
    COLORREF badgeColor;
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
    void paintFilterPills(HDC hdc, int width, int topY);
    void paintSidebar(HDC hdc, int height);
    void paintHomeView(HDC hdc, int width, int height);
    void paintDirectoryView(HDC hdc, int width, int height);
    void paintTilesView(HDC hdc, int width, int height);
    void paintLargeIconsView(HDC hdc, int width, int height, bool isLarge = true);
    void paintDetailsPane(HDC hdc, int width, int height);
    void paintPreviewPane(HDC hdc, int width, int height);
    void paintStatusBar(HDC hdc, int width, int height);
    void paintEmptyState(HDC hdc, const RECT& rc, const wchar_t* iconGlyph, COLORREF iconCol, const wchar_t* title, const wchar_t* subtitle);

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
    void setDateFilter(DateFilter df);
    void setSizeFilter(SizeFilter sf);
    void filterAndSortItems();
    void updateScrollInfo();

    // Sidebar Tree Expansion
    void toggleSidebarExpansion(size_t index);
    void expandSidebarItem(size_t index);
    void collapseSidebarItem(size_t index);

    // Favorites Management
    void addToFavorites(const std::wstring& path);
    void removeFromFavorites(const std::wstring& path);
    bool isFavorite(const std::wstring& path) const;

    // View Toggles
    void toggleCompactView();
    void toggleNavigationPane();
    void toggleDetailsPane();
    void togglePreviewPane();
    void toggleCheckBoxes();
    void toggleExtensions();
    void toggleHiddenItems();

    // Context Menus & Popups
    void showSortMenu(int x, int y);
    void showViewMenu(int x, int y);
    void showFilterMenu(int x, int y);
    void showNewMenu(int x, int y);
    void showMoreMenu(int x, int y);
    int getListTop(int contentTop) const;

    // Helpers
    void initSidebar();
    void initKnownFolders();
    void initFilterPills();
    HICON getFileIcon(const std::wstring& path, bool isDir, const std::wstring& ext, bool isLarge = false);
    void clearThumbnailCache();
    HBITMAP getThumbnailBitmap(const std::wstring& fullPath, int targetSize);
    void drawThumbnail(HDC hdc, HBITMAP hBmp, const RECT& targetRc);
    void copyTextToClipboard(const std::wstring& text);

    std::unordered_map<std::wstring, CachedThumbnail> thumbCache_;

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
    std::vector<bool> checkedRows_;
    std::vector<std::wstring> favorites_;

    std::vector<BreadcrumbPart> breadcrumbs_;
    std::vector<FilterPill> filterPills_;
    int hoverPill_{-1};
    bool addressEditActive_{false};
    bool searchIsPlaceholder_{true};

    std::vector<QuickCard> quickCards_;
    int homeSection_{0}; // 0: Recent, 1: Favorites, 2: Shared
    RECT pillRecentRc_{};
    RECT pillFavRc_{};
    RECT pillSharedRc_{};

    int selectedRow_{-1};
    int hoverRow_{-1};
    int hoverCard_{-1};
    int hoverCmdBtn_{-1};
    int scrollOffset_{0};

    // Layout Dimensions (Modern, Spacious Visual Hierarchy)
    int tabBarHeight_{42};
    int navBarHeight_{50};
    int commandBarHeight_{48};
    int filterBarHeight_{40};
    int headerHeight_{34};
    int rowHeight_{40};
    int statusBarHeight_{32};
    int sidebarWidth_{270};
    int detailsPaneWidth_{320};
    int previewPaneWidth_{380};

    // Column Widths
    int colNameWidth_{380};
    int colDateWidth_{180};
    int colTypeWidth_{140};
    int colSizeWidth_{110};
    int colLocWidth_{220};
    int colAccountWidth_{130};
    int colActivityWidth_{110};

    // Modes & Toggles
    ViewMode viewMode_{ViewMode::Details};
    SortColumn sortCol_{SortColumn::Name};
    bool sortAscending_{true};
    FilterCategory filterCategory_{FilterCategory::All};
    DateFilter dateFilter_{DateFilter::All};
    SizeFilter sizeFilter_{SizeFilter::All};

    bool compactView_{false};
    bool showNavigationPane_{true};
    bool showDetailsPane_{false};
    bool showPreviewPane_{false};
    bool showCheckBoxes_{false};
    bool showExtensions_{true};
    bool showHiddenItems_{false};

    // GDI Typography & Icon Fonts
    HFONT fontRegular_{nullptr};
    HFONT fontBold_{nullptr};
    HFONT fontTitle_{nullptr};
    HFONT fontSmall_{nullptr};
    HFONT fontIcons_{nullptr};
    HFONT fontIconsLarge_{nullptr};
    HFONT fontIconsSmall_{nullptr};

    ui::DesignTokens tokens_;

    // Known Folders cache
    std::wstring pathDesktop_;
    std::wstring pathDownloads_;
    std::wstring pathDocuments_;
    std::wstring pathPictures_;
    std::wstring pathMusic_;
    std::wstring pathVideos_;
    std::wstring pathHome_;
    std::wstring pathOneDrive_;

    HBRUSH darkEditBrush_{nullptr};
};

} // namespace gx::app
