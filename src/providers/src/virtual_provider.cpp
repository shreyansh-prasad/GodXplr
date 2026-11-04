#include "godxplr/providers/virtual_provider.h"
#include <algorithm>

namespace gx::providers {

bool PathSanitizer::isSafePath(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    // Reject null bytes
    if (path.find('\0') != std::string::npos) {
        return false;
    }

    // Reject drive letters like "C:" or "D:"
    if (path.size() >= 2 && path[1] == ':') {
        return false;
    }

    // Reject absolute paths starting with '/' or '\'
    if (path[0] == '/' || path[0] == '\\') {
        return false;
    }

    // Reject directory traversal segments "/../", "../", "/.."
    if (path == ".." || path.starts_with("../") || path.starts_with("..\\")) {
        return false;
    }
    if (path.find("/../") != std::string::npos || path.find("\\..\\") != std::string::npos ||
        path.find("/..\\") != std::string::npos || path.find("\\../") != std::string::npos) {
        return false;
    }
    if (path.ends_with("/..") || path.ends_with("\\..")) {
        return false;
    }

    return true;
}

std::wstring PathSanitizer::sanitizeRelativePath(const std::string& path) {
    if (!isSafePath(path)) {
        return {};
    }

    std::wstring sanitized;
    sanitized.reserve(path.size());

    for (char c : path) {
        if (c == '/') {
            sanitized.push_back(L'\\');
        } else {
            sanitized.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));
        }
    }

    // Strip trailing slash if present
    while (!sanitized.empty() && sanitized.back() == L'\\') {
        sanitized.pop_back();
    }

    return sanitized;
}

} // namespace gx::providers

