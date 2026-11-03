#include "godxplr/content/encoding_detector.h"
#include <algorithm>

namespace gx::content {

namespace {

bool isValidUtf8(const uint8_t* data, size_t size, bool& hasMultibyte) {
    hasMultibyte = false;
    size_t i = 0;
    while (i < size) {
        uint8_t b = data[i];
        if (b <= 0x7F) {
            // ASCII
            if (b < 0x09 || (b >= 0x0E && b <= 0x1F && b != 0x1B)) {
                // Suspicious non-printable control character
                return false;
            }
            i++;
        } else if ((b & 0xE0) == 0xC0) {
            // 2-byte sequence
            if (i + 1 >= size || (data[i + 1] & 0xC0) != 0x80) return false;
            hasMultibyte = true;
            i += 2;
        } else if ((b & 0xF0) == 0xE0) {
            // 3-byte sequence
            if (i + 2 >= size || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80) return false;
            hasMultibyte = true;
            i += 3;
        } else if ((b & 0xF8) == 0xF0) {
            // 4-byte sequence
            if (i + 3 >= size || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80 || (data[i + 3] & 0xC0) != 0x80) return false;
            hasMultibyte = true;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

void appendUtf8Codepoint(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

} // namespace

EncodingType EncodingDetector::detect(const uint8_t* data, size_t size, size_t& outBomBytes) {
    outBomBytes = 0;
    if (!data || size == 0) {
        return EncodingType::Ascii;
    }

    // 1. Check BOM signatures
    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        outBomBytes = 3;
        return EncodingType::Utf8;
    }
    if (size >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        outBomBytes = 2;
        return EncodingType::Utf16LE;
    }
    if (size >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
        outBomBytes = 2;
        return EncodingType::Utf16BE;
    }

    // 2. Statistical inspection of first 4096 bytes
    const size_t sampleLen = std::min(size, static_cast<size_t>(4096));

    // Check for UTF-16LE without BOM: every alternate byte is 0x00 for ASCII text
    if (sampleLen >= 4) {
        size_t nulEven = 0;
        size_t nulOdd = 0;
        for (size_t i = 0; i < sampleLen; ++i) {
            if (data[i] == 0x00) {
                if (i % 2 == 0) nulEven++;
                else nulOdd++;
            }
        }
        if (nulOdd > sampleLen / 4 && nulEven == 0) {
            return EncodingType::Utf16LE;
        }
        if (nulEven > sampleLen / 4 && nulOdd == 0) {
            return EncodingType::Utf16BE;
        }
    }

    // Check if buffer contains NUL bytes
    for (size_t i = 0; i < sampleLen; ++i) {
        if (data[i] == 0x00) {
            return EncodingType::Binary;
        }
    }

    // Validate UTF-8
    bool hasMultibyte = false;
    if (isValidUtf8(data, sampleLen, hasMultibyte)) {
        return hasMultibyte ? EncodingType::Utf8 : EncodingType::Ascii;
    }

    return EncodingType::Binary;
}

std::string EncodingDetector::toUtf8(const uint8_t* data, size_t size, EncodingType encoding, size_t bomBytes) {
    if (!data || size <= bomBytes) {
        return {};
    }

    const uint8_t* payload = data + bomBytes;
    size_t len = size - bomBytes;

    if (encoding == EncodingType::Ascii || encoding == EncodingType::Utf8) {
        return std::string(reinterpret_cast<const char*>(payload), len);
    }

    std::string result;
    result.reserve(len);

    if (encoding == EncodingType::Utf16LE) {
        for (size_t i = 0; i + 1 < len; i += 2) {
            uint16_t w = static_cast<uint16_t>(payload[i] | (payload[i + 1] << 8));
            appendUtf8Codepoint(result, w);
        }
    } else if (encoding == EncodingType::Utf16BE) {
        for (size_t i = 0; i + 1 < len; i += 2) {
            uint16_t w = static_cast<uint16_t>((payload[i] << 8) | payload[i + 1]);
            appendUtf8Codepoint(result, w);
        }
    }

    return result;
}

} // namespace gx::content

