#include "godxplr/ui/theme/theme_manager.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace gx::ui {

ThemeManager::ThemeManager(ThemeMode mode)
    : mode_(mode) {
    resolveActiveTokens();
}

void ThemeManager::setMode(ThemeMode mode) {
    mode_ = mode;
    resolveActiveTokens();
}

void ThemeManager::resolveActiveTokens() {
    if (mode_ == ThemeMode::Light) {
        tokens_ = DesignTokens::lightPalette();
    } else if (mode_ == ThemeMode::Dark) {
        tokens_ = DesignTokens::darkPalette();
    } else if (mode_ == ThemeMode::HighContrast) {
        tokens_ = DesignTokens::highContrastPalette();
    } else { // System
        if (isSystemInHighContrast()) {
            tokens_ = DesignTokens::highContrastPalette();
        } else if (isSystemInDarkMode()) {
            tokens_ = DesignTokens::darkPalette();
        } else {
            tokens_ = DesignTokens::lightPalette();
        }
    }
}

bool ThemeManager::isSystemInDarkMode() {
#ifdef _WIN32
    HKEY hKey;
    if (::RegOpenKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0,
            KEY_READ,
            &hKey) == ERROR_SUCCESS) {
        DWORD appsUseLightTheme = 1;
        DWORD bufSize = sizeof(appsUseLightTheme);
        DWORD type = REG_DWORD;
        LONG res = ::RegQueryValueExW(
            hKey,
            L"AppsUseLightTheme",
            nullptr,
            &type,
            reinterpret_cast<LPBYTE>(&appsUseLightTheme),
            &bufSize
        );
        ::RegCloseKey(hKey);
        if (res == ERROR_SUCCESS) {
            return appsUseLightTheme == 0;
        }
    }
#endif
    return false;
}

bool ThemeManager::isSystemInHighContrast() {
#ifdef _WIN32
    HIGHCONTRASTW hc{};
    hc.cbSize = sizeof(hc);
    if (::SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0)) {
        return (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
    }
#endif
    return false;
}

} // namespace gx::ui
