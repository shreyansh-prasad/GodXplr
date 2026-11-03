#pragma once

#include <cstdint>
#include <algorithm>

namespace gx::ui {

struct Point {
    int x{0};
    int y{0};

    auto operator<=>(const Point&) const = default;
};

struct Size {
    int width{0};
    int height{0};

    auto operator<=>(const Size&) const = default;
    [[nodiscard]] bool isEmpty() const noexcept { return width <= 0 || height <= 0; }
};

struct Rect {
    int x{0};
    int y{0};
    int width{0};
    int height{0};

    auto operator<=>(const Rect&) const = default;

    [[nodiscard]] int left() const noexcept { return x; }
    [[nodiscard]] int top() const noexcept { return y; }
    [[nodiscard]] int right() const noexcept { return x + width; }
    [[nodiscard]] int bottom() const noexcept { return y + height; }
    [[nodiscard]] bool isEmpty() const noexcept { return width <= 0 || height <= 0; }

    [[nodiscard]] bool contains(Point pt) const noexcept {
        return pt.x >= x && pt.x < (x + width) &&
               pt.y >= y && pt.y < (y + height);
    }

    [[nodiscard]] bool intersects(const Rect& other) const noexcept {
        return left() < other.right() && right() > other.left() &&
               top() < other.bottom() && bottom() > other.top();
    }
};

struct VisibleRange {
    uint64_t first{0};
    uint64_t lastExclusive{0};

    [[nodiscard]] uint64_t count() const noexcept {
        return (lastExclusive > first) ? (lastExclusive - first) : 0;
    }
    [[nodiscard]] bool isEmpty() const noexcept { return first >= lastExclusive; }
    [[nodiscard]] bool contains(uint64_t row) const noexcept {
        return row >= first && row < lastExclusive;
    }

    auto operator<=>(const VisibleRange&) const = default;
};

} // namespace gx::ui
