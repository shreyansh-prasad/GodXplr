#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace gx::content {

enum class EncodingType : uint8_t {
    Ascii,
    Utf8,
    Utf16LE,
    Utf16BE,
    Binary
};

class EncodingDetector {
public:
    // Inspects a byte buffer to identify text encoding or binary format
    static EncodingType detect(const uint8_t* data, size_t size, size_t& outBomBytes);

    // Converts buffer to a normalized UTF-8 string according to detected encoding
    static std::string toUtf8(const uint8_t* data, size_t size, EncodingType encoding, size_t bomBytes = 0);
};

} // namespace gx::content

