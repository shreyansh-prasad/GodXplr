#pragma once

#include "godxplr/ui/theme/design_tokens.h"

namespace gx::ui {

enum class ThemeMode : uint8_t {
    System,
    Light,
    Dark,
    HighContrast
};

class ThemeManager {
public:
    explicit ThemeManager(ThemeMode mode = ThemeMode::System);
    ~ThemeManager() = default;

    void setMode(ThemeMode mode);
    [[nodiscard]] ThemeMode getMode() const noexcept { return mode_; }

    // Returns active resolved design tokens
    [[nodiscard]] const DesignTokens& getTokens() const noexcept { return tokens_; }

    // Detects whether Windows OS is currently configured for dark mode
    [[nodiscard]] static bool isSystemInDarkMode();

    // Detects whether Windows High Contrast mode is active
    [[nodiscard]] static bool isSystemInHighContrast();

private:
    void resolveActiveTokens();

    ThemeMode mode_{ThemeMode::System};
    DesignTokens tokens_{DesignTokens::lightPalette()};
};

} // namespace gx::ui
