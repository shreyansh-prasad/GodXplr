#pragma once

#include <cstdint>
#include <string>

namespace gx::ui {

struct ColorRgba {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
    uint8_t a{255};

    constexpr ColorRgba() = default;
    constexpr ColorRgba(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}

    [[nodiscard]] constexpr uint32_t toRgba() const noexcept {
        return (static_cast<uint32_t>(r) << 24) |
               (static_cast<uint32_t>(g) << 16) |
               (static_cast<uint32_t>(b) << 8)  |
               static_cast<uint32_t>(a);
    }

    [[nodiscard]] std::string toHex() const {
        char buf[10];
        snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
        return std::string(buf);
    }
};

struct DesignTokens {
    // Surfaces
    ColorRgba surfaceBackground;
    ColorRgba surfaceElevated;
    ColorRgba surfaceSidebar;
    ColorRgba surfaceCard;

    // Text & Content
    ColorRgba textPrimary;
    ColorRgba textSecondary;
    ColorRgba textDisabled;
    ColorRgba textOnAccent;

    // Interaction states
    ColorRgba borderSubtle;
    ColorRgba borderFocus;
    ColorRgba rowHover;
    ColorRgba rowSelected;
    ColorRgba rowSelectedBorder;
    ColorRgba accent;

    // Status / Semantics
    ColorRgba statusError;
    ColorRgba statusWarning;
    ColorRgba statusSuccess;
    ColorRgba statusInfo;

    // Dimensions (Logical Pixels)
    int tabHeight{34};
    int navBarHeight{38};
    int toolbarHeight{40};
    int statusBarHeight{24};
    int rowHeightDetailsCompact{24};
    int rowHeightDetailsComfortable{28};
    int rowHeightDetailsSpacious{34};
    int gridTileWidth{120};
    int gridTileHeight{140};
    int sidebarWidth{240};

    // Standard palettes
    static DesignTokens lightPalette() {
        DesignTokens t;
        t.surfaceBackground   = ColorRgba(255, 255, 255);
        t.surfaceElevated     = ColorRgba(243, 243, 243);
        t.surfaceSidebar      = ColorRgba(249, 249, 249);
        t.surfaceCard         = ColorRgba(255, 255, 255);

        t.textPrimary         = ColorRgba(0, 0, 0);
        t.textSecondary       = ColorRgba(95, 95, 95);
        t.textDisabled        = ColorRgba(160, 160, 160);
        t.textOnAccent        = ColorRgba(255, 255, 255);

        t.borderSubtle        = ColorRgba(229, 229, 229);
        t.borderFocus         = ColorRgba(0, 103, 192);
        t.rowHover            = ColorRgba(240, 244, 248);
        t.rowSelected         = ColorRgba(204, 232, 255);
        t.rowSelectedBorder   = ColorRgba(153, 209, 255);
        t.accent              = ColorRgba(0, 103, 192);

        t.statusError         = ColorRgba(216, 59, 1);
        t.statusWarning       = ColorRgba(255, 185, 0);
        t.statusSuccess       = ColorRgba(16, 124, 16);
        t.statusInfo          = ColorRgba(0, 120, 212);
        return t;
    }

    static DesignTokens darkPalette() {
        DesignTokens t;
        t.surfaceBackground   = ColorRgba(32, 32, 32);
        t.surfaceElevated     = ColorRgba(44, 44, 44);
        t.surfaceSidebar      = ColorRgba(37, 37, 37);
        t.surfaceCard         = ColorRgba(44, 44, 44);

        t.textPrimary         = ColorRgba(255, 255, 255);
        t.textSecondary       = ColorRgba(158, 158, 158);
        t.textDisabled        = ColorRgba(90, 90, 90);
        t.textOnAccent        = ColorRgba(0, 0, 0);

        t.borderSubtle        = ColorRgba(56, 56, 56);
        t.borderFocus         = ColorRgba(76, 194, 255);
        t.rowHover            = ColorRgba(50, 56, 66);
        t.rowSelected         = ColorRgba(30, 58, 95);
        t.rowSelectedBorder   = ColorRgba(41, 90, 138);
        t.accent              = ColorRgba(76, 194, 255);

        t.statusError         = ColorRgba(255, 153, 164);
        t.statusWarning       = ColorRgba(252, 225, 0);
        t.statusSuccess       = ColorRgba(108, 203, 95);
        t.statusInfo          = ColorRgba(76, 194, 255);
        return t;
    }

    static DesignTokens highContrastPalette() {
        DesignTokens t;
        t.surfaceBackground   = ColorRgba(0, 0, 0);
        t.surfaceElevated     = ColorRgba(0, 0, 0);
        t.surfaceSidebar      = ColorRgba(0, 0, 0);
        t.surfaceCard         = ColorRgba(0, 0, 0);

        t.textPrimary         = ColorRgba(255, 255, 255);
        t.textSecondary       = ColorRgba(255, 255, 0); // Yellow
        t.textDisabled        = ColorRgba(0, 255, 0);   // Green
        t.textOnAccent        = ColorRgba(0, 0, 0);

        t.borderSubtle        = ColorRgba(255, 255, 255);
        t.borderFocus         = ColorRgba(255, 255, 0);
        t.rowHover            = ColorRgba(26, 26, 26);
        t.rowSelected         = ColorRgba(26, 77, 128);
        t.rowSelectedBorder   = ColorRgba(255, 255, 0);
        t.accent              = ColorRgba(128, 255, 255); // Cyan

        t.statusError         = ColorRgba(255, 64, 64);
        t.statusWarning       = ColorRgba(255, 255, 0);
        t.statusSuccess       = ColorRgba(0, 255, 0);
        t.statusInfo          = ColorRgba(128, 255, 255);
        return t;
    }
};

} // namespace gx::ui
