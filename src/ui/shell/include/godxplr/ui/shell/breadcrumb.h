#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace gx::ui {

struct BreadcrumbSegment {
    std::string label;
    std::string targetUri;
    bool isRoot{false};

    auto operator<=>(const BreadcrumbSegment&) const = default;
};

class BreadcrumbParser {
public:
    static std::vector<BreadcrumbSegment> parse(std::string_view uri);
};

} // namespace gx::ui
