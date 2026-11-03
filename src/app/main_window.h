#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <memory>

#include "godxplr/ui/theme/design_tokens.h"
#include "godxplr/ui/model/directory_store.h"
#include "godxplr/ui/model/selection_model.h"
#include "godxplr/ui/view/details_layout.h"

namespace gx::app {

struct FileItemDisplay {
    std::wstring name;
    std::wstring extension;
    uint64_t size{0};
    uint64_t mtime{0};
    uint32_t attributes{0};
    bool isDirectory{false};
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

    void onPaint(HDC hdc);
    void onResize(int width, int height);
    void onVScroll(WPARAM wParam);
    void onMouseWheel(short delta);
    void onLButtonDown(int x, int y, bool isDoubleClick);
    void onRButtonUp(int x, int y);
    void onKeyDown(WPARAM key);
    void onSearchChanged();

    void navigateTo(const std::filesystem::path& path);
    void navigateUp();
    void navigateBack();
    void navigateForward();
    void refresh();
    void openSelectedItem();

    void updateScrollInfo();
    void filterItems();

    HINSTANCE hInstance_{nullptr};
    HWND hwnd_{nullptr};
    HWND editAddress_{nullptr};
    HWND editSearch_{nullptr};

    std::filesystem::path currentPath_;
    std::vector<std::filesystem::path> backStack_;
    std::vector<std::filesystem::path> forwardStack_;

    std::vector<FileItemDisplay> allItems_;
    std::vector<size_t> filteredIndices_; // Indices into allItems_

    int selectedRow_{-1};
    int hoverRow_{-1};
    int scrollOffset_{0};
    int rowHeight_{28};
    int headerHeight_{28};
    int topBarHeight_{48};
    int statusBarHeight_{24};

    int colNameWidth_{360};
    int colTypeWidth_{120};
    int colSizeWidth_{110};
    int colDateWidth_{160};

    HFONT fontRegular_{nullptr};
    HFONT fontBold_{nullptr};
    HFONT fontSmall_{nullptr};

    ui::DesignTokens tokens_;
};

} // namespace gx::app
