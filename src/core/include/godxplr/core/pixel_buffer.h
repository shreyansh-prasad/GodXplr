#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>

namespace gx::core {

constexpr uint32_t kMaxImageDimension = 8192; // 8K maximum single dimension limit
constexpr size_t kMaxImageDecodeBytes = 32 * 1024 * 1024; // 32 MB max decode memory limit

class PixelBuffer {
public:
    PixelBuffer() = default;

    // Allocates an ARGB32 buffer. Returns false if dimensions or byte size exceed safety thresholds.
    bool allocate(uint32_t w, uint32_t h) {
        if (w == 0 || h == 0 || w > kMaxImageDimension || h > kMaxImageDimension) {
            return false;
        }

        uint64_t totalBytes = static_cast<uint64_t>(w) * h * 4ULL;
        if (totalBytes > kMaxImageDecodeBytes) {
            return false; // Decompression bomb defense triggered
        }

        width_ = w;
        height_ = h;
        stride_ = w * 4;
        pixels_.assign(static_cast<size_t>(totalBytes), 0);
        return true;
    }

    [[nodiscard]] bool isValid() const noexcept {
        return width_ > 0 && height_ > 0 && !pixels_.empty();
    }

    [[nodiscard]] uint32_t width() const noexcept { return width_; }
    [[nodiscard]] uint32_t height() const noexcept { return height_; }
    [[nodiscard]] uint32_t stride() const noexcept { return stride_; }
    [[nodiscard]] size_t byteCount() const noexcept { return pixels_.size(); }
    [[nodiscard]] const uint8_t* data() const noexcept { return pixels_.data(); }
    [[nodiscard]] uint8_t* data() noexcept { return pixels_.data(); }

    void clear() {
        pixels_.clear();
        width_ = 0;
        height_ = 0;
        stride_ = 0;
    }

    // Downsamples buffer to target dimensions using nearest-neighbor box sampling
    [[nodiscard]] PixelBuffer downscaleTo(uint32_t targetW, uint32_t targetH) const {
        if (!isValid() || targetW == 0 || targetH == 0) {
            return PixelBuffer{};
        }

        PixelBuffer dst;
        if (!dst.allocate(targetW, targetH)) {
            return PixelBuffer{};
        }

        const uint32_t* srcPixels = reinterpret_cast<const uint32_t*>(pixels_.data());
        uint32_t* dstPixels = reinterpret_cast<uint32_t*>(dst.data());

        const double xRatio = static_cast<double>(width_) / targetW;
        const double yRatio = static_cast<double>(height_) / targetH;

        for (uint32_t y = 0; y < targetH; ++y) {
            uint32_t srcY = std::min(static_cast<uint32_t>(y * yRatio), height_ - 1);
            for (uint32_t x = 0; x < targetW; ++x) {
                uint32_t srcX = std::min(static_cast<uint32_t>(x * xRatio), width_ - 1);
                dstPixels[y * targetW + x] = srcPixels[srcY * width_ + srcX];
            }
        }

        return dst;
    }

private:
    uint32_t width_{0};
    uint32_t height_{0};
    uint32_t stride_{0};
    std::vector<uint8_t> pixels_;
};

} // namespace gx::core
