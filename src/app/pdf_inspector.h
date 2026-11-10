#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cctype>

namespace gx::app {

struct PdfDocumentInfo {
    bool isValid{false};
    std::wstring version{L"PDF 1.7"};
    int pageCount{1};
    double widthPt{612.0};
    double heightPt{792.0};
    std::wstring pageFormat{L"US Letter (8.5\" × 11\")"};
    std::wstring title;
    std::wstring author;
    std::wstring subject;
    std::wstring creator;
    std::wstring producer;
    std::wstring creationDate;
    bool isEncrypted{false};
    std::vector<std::wstring> lines;
    size_t totalWords{0};
    size_t totalCharacters{0};
};

namespace detail {

// RFC 1951 Deflate inflator
struct InflateState {
    const uint8_t* in{nullptr};
    size_t inLen{0};
    size_t inPos{0};
    uint32_t bitBuf{0};
    int bitCount{0};

    uint8_t* out{nullptr};
    size_t outCap{0};
    size_t outPos{0};

    int getBits(int need) {
        while (bitCount < need) {
            if (inPos >= inLen) return -1;
            bitBuf |= static_cast<uint32_t>(in[inPos++]) << bitCount;
            bitCount += 8;
        }
        int val = static_cast<int>(bitBuf & ((1u << need) - 1));
        bitBuf >>= need;
        bitCount -= need;
        return val;
    }

    bool putByte(uint8_t b) {
        if (outPos >= outCap) return false;
        out[outPos++] = b;
        return true;
    }
};

struct HuffTree {
    int counts[16]{};
    int symbols[288]{};

    bool build(const int* lens, int n) {
        std::memset(counts, 0, sizeof(counts));
        for (int i = 0; i < n; ++i) {
            if (lens[i] > 0 && lens[i] < 16) {
                counts[lens[i]]++;
            }
        }
        int offsets[16]{};
        offsets[1] = 0;
        for (int i = 1; i < 15; ++i) {
            offsets[i + 1] = offsets[i] + counts[i];
        }
        for (int i = 0; i < n; ++i) {
            if (lens[i] > 0 && lens[i] < 16) {
                symbols[offsets[lens[i]]++] = i;
            }
        }
        return true;
    }

    int decode(InflateState& s) const {
        int code = 0;
        int first = 0;
        int index = 0;
        for (int len = 1; len <= 15; ++len) {
            int b = s.getBits(1);
            if (b < 0) return -1;
            code |= b;
            int count = counts[len];
            if (code - count < first) {
                return symbols[index + (code - first)];
            }
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        return -1;
    }
};

inline bool inflateBlock(InflateState& s, const HuffTree& lt, const HuffTree& dt) {
    static const int lenBase[29] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
    };
    static const int lenExtra[29] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
    };
    static const int distBase[30] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
    };
    static const int distExtra[30] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
    };

    while (true) {
        int sym = lt.decode(s);
        if (sym < 0) return false;
        if (sym < 256) {
            if (!s.putByte(static_cast<uint8_t>(sym))) return false;
        } else if (sym == 256) {
            break; // End of block
        } else {
            int lIdx = sym - 257;
            if (lIdx >= 29) return false;
            int length = lenBase[lIdx];
            if (lenExtra[lIdx] > 0) {
                int extra = s.getBits(lenExtra[lIdx]);
                if (extra < 0) return false;
                length += extra;
            }

            int dSym = dt.decode(s);
            if (dSym < 0 || dSym >= 30) return false;
            int dist = distBase[dSym];
            if (distExtra[dSym] > 0) {
                int extra = s.getBits(distExtra[dSym]);
                if (extra < 0) return false;
                dist += extra;
            }

            if (static_cast<size_t>(dist) > s.outPos) return false;
            size_t src = s.outPos - dist;
            for (int i = 0; i < length; ++i) {
                if (!s.putByte(s.out[src + i])) return false;
            }
        }
    }
    return true;
}

inline bool inflateData(const uint8_t* in, size_t inLen, std::vector<uint8_t>& outBuf, size_t maxOut = 1024 * 1024) {
    // If zlib header (0x78) is present, skip 2 bytes
    if (inLen > 2 && in[0] == 0x78 && (in[1] == 0x01 || in[1] == 0x9C || in[1] == 0xDA || in[1] == 0x5E)) {
        in += 2;
        inLen -= 2;
        if (inLen >= 4) inLen -= 4; // Ignore Adler32 at end
    }

    outBuf.resize(maxOut);
    InflateState s;
    s.in = in;
    s.inLen = inLen;
    s.out = outBuf.data();
    s.outCap = outBuf.size();

    int last = 0;
    while (!last) {
        last = s.getBits(1);
        int type = s.getBits(2);
        if (type == 0) { // Stored
            s.bitBuf = 0;
            s.bitCount = 0;
            if (s.inPos + 4 > s.inLen) return false;
            uint16_t len = static_cast<uint16_t>(s.in[s.inPos] | (s.in[s.inPos + 1] << 8));
            s.inPos += 4;
            if (s.inPos + len > s.inLen) return false;
            for (uint16_t i = 0; i < len; ++i) {
                if (!s.putByte(s.in[s.inPos++])) return false;
            }
        } else if (type == 1) { // Fixed Huffman
            int litLens[288];
            for (int i = 0; i <= 143; ++i) litLens[i] = 8;
            for (int i = 144; i <= 255; ++i) litLens[i] = 9;
            for (int i = 256; i <= 279; ++i) litLens[i] = 7;
            for (int i = 280; i <= 287; ++i) litLens[i] = 8;
            int distLens[32];
            for (int i = 0; i < 32; ++i) distLens[i] = 5;
            HuffTree lt, dt;
            lt.build(litLens, 288);
            dt.build(distLens, 32);
            if (!inflateBlock(s, lt, dt)) return false;
        } else if (type == 2) { // Dynamic Huffman
            int nlit = s.getBits(5) + 257;
            int ndist = s.getBits(5) + 1;
            int nclen = s.getBits(4) + 4;
            if (nlit > 286 || ndist > 30 || nclen > 19) return false;

            static const int clOrder[19] = {
                16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
            };
            int clLens[19]{};
            for (int i = 0; i < nclen; ++i) {
                clLens[clOrder[i]] = s.getBits(3);
            }
            HuffTree clt;
            clt.build(clLens, 19);

            std::vector<int> allLens(nlit + ndist, 0);
            int idx = 0;
            while (idx < nlit + ndist) {
                int sym = clt.decode(s);
                if (sym < 0) return false;
                if (sym < 16) {
                    allLens[idx++] = sym;
                } else if (sym == 16) {
                    if (idx == 0) return false;
                    int repeat = s.getBits(2) + 3;
                    int prev = allLens[idx - 1];
                    while (repeat-- > 0 && idx < nlit + ndist) allLens[idx++] = prev;
                } else if (sym == 17) {
                    int repeat = s.getBits(3) + 3;
                    while (repeat-- > 0 && idx < nlit + ndist) allLens[idx++] = 0;
                } else if (sym == 18) {
                    int repeat = s.getBits(7) + 11;
                    while (repeat-- > 0 && idx < nlit + ndist) allLens[idx++] = 0;
                }
            }

            HuffTree lt, dt;
            lt.build(allLens.data(), nlit);
            dt.build(allLens.data() + nlit, ndist);
            if (!inflateBlock(s, lt, dt)) return false;
        } else {
            return false;
        }
    }

    outBuf.resize(s.outPos);
    return true;
}

// Decodes a PDF string (handling parentheses escapes, octal codes, and UTF-16BE BOM)
inline std::wstring decodePdfLiteralString(const std::string& str) {
    if (str.size() >= 2 && static_cast<uint8_t>(str[0]) == 0xFE && static_cast<uint8_t>(str[1]) == 0xFF) {
        std::wstring out;
        for (size_t i = 2; i + 1 < str.size(); i += 2) {
            uint16_t w = static_cast<uint16_t>((static_cast<uint8_t>(str[i]) << 8) | static_cast<uint8_t>(str[i + 1]));
            if (w != 0) out.push_back(static_cast<wchar_t>(w));
        }
        return out;
    }

    std::wstring out;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            i++;
            char c = str[i];
            if (c == 'n') out.push_back(L'\n');
            else if (c == 'r') out.push_back(L'\r');
            else if (c == 't') out.push_back(L'\t');
            else if (c == 'b') out.push_back(L'\b');
            else if (c == 'f') out.push_back(L'\f');
            else if (c == '(' || c == ')' || c == '\\') out.push_back(static_cast<wchar_t>(c));
            else if (c >= '0' && c <= '7') {
                int oct = c - '0';
                if (i + 1 < str.size() && str[i + 1] >= '0' && str[i + 1] <= '7') {
                    oct = oct * 8 + (str[++i] - '0');
                    if (i + 1 < str.size() && str[i + 1] >= '0' && str[i + 1] <= '7') {
                        oct = oct * 8 + (str[++i] - '0');
                    }
                }
                out.push_back(static_cast<wchar_t>(oct));
            } else {
                out.push_back(static_cast<wchar_t>(c));
            }
        } else {
            out.push_back(static_cast<wchar_t>(static_cast<unsigned char>(str[i])));
        }
    }
    return out;
}

// Decodes PDF hex string e.g. <FEFF...> or <48656C6C6F>
inline std::wstring decodePdfHexString(const std::string& hex) {
    std::string clean;
    for (char c : hex) {
        if (std::isxdigit(static_cast<unsigned char>(c))) clean.push_back(c);
    }
    if (clean.empty()) return L"";
    if (clean.size() % 2 != 0) clean.push_back('0');

    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < clean.size(); i += 2) {
        std::string byteStr = clean.substr(i, 2);
        bytes.push_back(static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16)));
    }

    // UTF-16BE detection
    if (bytes.size() >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF) {
        std::wstring out;
        for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
            uint16_t w = static_cast<uint16_t>((bytes[i] << 8) | bytes[i + 1]);
            out.push_back(static_cast<wchar_t>(w));
        }
        return out;
    }

    std::wstring out;
    for (uint8_t b : bytes) {
        out.push_back(static_cast<wchar_t>(b));
    }
    return out;
}

} // namespace detail

class PdfInspector {
public:
    static PdfDocumentInfo inspect(const std::wstring& filePath, size_t maxBytes = 4 * 1024 * 1024) {
        PdfDocumentInfo info;
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return info;

        file.seekg(0, std::ios::end);
        std::streamoff fSize = file.tellg();
        if (fSize < 32) return info;

        size_t readLen = static_cast<size_t>(std::min<std::streamoff>(fSize, static_cast<std::streamoff>(maxBytes)));
        std::string rawData(readLen, '\0');
        file.seekg(0, std::ios::beg);
        file.read(rawData.data(), readLen);

        // 1. Version Check (%PDF-x.y)
        size_t vPos = rawData.find("%PDF-");
        if (vPos != std::string::npos && vPos + 8 <= rawData.size()) {
            info.isValid = true;
            std::string verStr = rawData.substr(vPos + 5, 3);
            info.version = L"PDF " + std::wstring(verStr.begin(), verStr.end());
        } else {
            return info;
        }

        // 2. Encryption Check
        if (rawData.find("/Encrypt") != std::string::npos) {
            info.isEncrypted = true;
        }

        // 3. Page Count & MediaBox
        size_t countPos = rawData.find("/Count");
        if (countPos != std::string::npos) {
            size_t numStart = rawData.find_first_of("0123456789", countPos + 6);
            if (numStart != std::string::npos && numStart < countPos + 16) {
                info.pageCount = std::max(1, std::atoi(&rawData[numStart]));
            }
        }
        if (info.pageCount <= 1) {
            // Count /Type /Page occurrences
            size_t pCount = 0;
            size_t pFind = 0;
            while ((pFind = rawData.find("/Type", pFind)) != std::string::npos) {
                size_t pageCheck = rawData.find_first_not_of(" \t\r\n", pFind + 5);
                if (pageCheck != std::string::npos && rawData.substr(pageCheck, 5) == "/Page" &&
                    (pageCheck + 5 >= rawData.size() || rawData[pageCheck + 5] != 's')) {
                    pCount++;
                }
                pFind += 5;
            }
            if (pCount > 0) info.pageCount = static_cast<int>(pCount);
        }

        // MediaBox Dimensions
        size_t mBoxPos = rawData.find("/MediaBox");
        if (mBoxPos != std::string::npos) {
            size_t brStart = rawData.find('[', mBoxPos);
            size_t brEnd = (brStart != std::string::npos) ? rawData.find(']', brStart) : std::string::npos;
            if (brStart != std::string::npos && brEnd != std::string::npos) {
                std::string coords = rawData.substr(brStart + 1, brEnd - brStart - 1);
                std::istringstream ss(coords);
                double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
                if (ss >> x1 >> y1 >> x2 >> y2) {
                    info.widthPt = std::abs(x2 - x1);
                    info.heightPt = std::abs(y2 - y1);
                    if (std::abs(info.widthPt - 612) < 15 && std::abs(info.heightPt - 792) < 15) {
                        info.pageFormat = L"US Letter (8.5\" × 11\")";
                    } else if (std::abs(info.widthPt - 595) < 15 && std::abs(info.heightPt - 842) < 15) {
                        info.pageFormat = L"A4 (210 × 297 mm)";
                    } else if (std::abs(info.widthPt - 612) < 15 && std::abs(info.heightPt - 1008) < 15) {
                        info.pageFormat = L"US Legal (8.5\" × 14\")";
                    } else if (info.heightPt > 0 && std::abs((info.widthPt / info.heightPt) - (16.0 / 9.0)) < 0.05) {
                        info.pageFormat = L"16:9 Presentation (Widescreen)";
                    } else if (info.heightPt > 0 && std::abs((info.widthPt / info.heightPt) - (4.0 / 3.0)) < 0.05) {
                        info.pageFormat = L"4:3 Presentation (Standard)";
                    } else {
                        wchar_t dimBuf[64];
                        swprintf_s(dimBuf, L"%d × %d pt", static_cast<int>(info.widthPt), static_cast<int>(info.heightPt));
                        info.pageFormat = dimBuf;
                    }
                }
            }
        }

        // 4. Metadata Extraction from Info Dictionary
        auto extractMeta = [&](const std::string& key) -> std::wstring {
            size_t kPos = rawData.find(key);
            if (kPos == std::string::npos) return L"";
            size_t valStart = rawData.find_first_not_of(" \t\r\n", kPos + key.length());
            if (valStart == std::string::npos) return L"";
            if (rawData[valStart] == '(') {
                size_t pEnd = valStart + 1;
                int depth = 1;
                while (pEnd < rawData.size() && depth > 0) {
                    if (rawData[pEnd] == '\\') { pEnd += 2; continue; }
                    if (rawData[pEnd] == '(') depth++;
                    else if (rawData[pEnd] == ')') depth--;
                    pEnd++;
                }
                return detail::decodePdfLiteralString(rawData.substr(valStart + 1, pEnd - valStart - 2));
            } else if (rawData[valStart] == '<') {
                size_t hEnd = rawData.find('>', valStart);
                if (hEnd != std::string::npos) {
                    return detail::decodePdfHexString(rawData.substr(valStart + 1, hEnd - valStart - 1));
                }
            }
            return L"";
        };

        info.title = extractMeta("/Title");
        info.author = extractMeta("/Author");
        info.subject = extractMeta("/Subject");
        info.creator = extractMeta("/Creator");
        info.producer = extractMeta("/Producer");
        std::wstring rawDate = extractMeta("/CreationDate");
        if (rawDate.size() >= 10 && rawDate.rfind(L"D:", 0) == 0) {
            std::wstring d = rawDate.substr(2);
            if (d.size() >= 8) {
                info.creationDate = d.substr(0, 4) + L"-" + d.substr(4, 2) + L"-" + d.substr(6, 2);
                if (d.size() >= 12) {
                    info.creationDate += L" " + d.substr(8, 2) + L":" + d.substr(10, 2);
                }
            }
        }

        // 5. Stream Content & Text Extraction
        std::vector<std::string> streamBuffers;
        size_t sPos = 0;
        while ((sPos = rawData.find("stream", sPos)) != std::string::npos) {
            size_t afterStream = sPos + 6;
            if (afterStream < rawData.size() && rawData[afterStream] == '\r') afterStream++;
            if (afterStream < rawData.size() && rawData[afterStream] == '\n') afterStream++;

            size_t endPos = rawData.find("endstream", afterStream);
            if (endPos == std::string::npos) break;

            // Check if stream is an image, font file, metadata or object stream
            size_t dictStart = rawData.rfind("<<", sPos);
            bool isFlate = false;
            bool shouldSkip = false;
            if (dictStart != std::string::npos && dictStart + 500 >= sPos) {
                std::string dict = rawData.substr(dictStart, sPos - dictStart);
                if (dict.find("/Subtype /Image") != std::string::npos ||
                    dict.find("/Subtype/Image") != std::string::npos ||
                    dict.find("/DCTDecode") != std::string::npos ||
                    dict.find("/JPXDecode") != std::string::npos ||
                    dict.find("/JBIG2Decode") != std::string::npos ||
                    dict.find("/FontFile") != std::string::npos ||
                    dict.find("/Type /Font") != std::string::npos ||
                    dict.find("/Type/Font") != std::string::npos ||
                    dict.find("/Type /ObjStm") != std::string::npos ||
                    dict.find("/Type /XRef") != std::string::npos ||
                    dict.find("/Type /Metadata") != std::string::npos) {
                    shouldSkip = true;
                }
                if (dict.find("/FlateDecode") != std::string::npos) isFlate = true;
            }

            if (shouldSkip) {
                sPos = endPos + 9;
                continue;
            }

            const uint8_t* streamData = reinterpret_cast<const uint8_t*>(&rawData[afterStream]);
            size_t streamLen = endPos - afterStream;

            if (isFlate) {
                std::vector<uint8_t> decomp;
                if (detail::inflateData(streamData, streamLen, decomp)) {
                    // Check if decompressed data is a TrueType/OpenType font or binary blob
                    if (decomp.size() >= 4 && (
                        (decomp[0] == 0x00 && decomp[1] == 0x01 && decomp[2] == 0x00 && decomp[3] == 0x00) ||
                        (decomp[0] == 'O' && decomp[1] == 'T' && decomp[2] == 'T' && decomp[3] == 'O') ||
                        (decomp[0] == 't' && decomp[1] == 'r' && decomp[2] == 'u' && decomp[3] == 'e'))) {
                        // Skip binary font
                    } else {
                        streamBuffers.emplace_back(reinterpret_cast<const char*>(decomp.data()), decomp.size());
                    }
                }
            } else {
                streamBuffers.emplace_back(&rawData[afterStream], streamLen);
            }

            sPos = endPos + 9;
        }

        // 6. Parse Text Operators (BT ... ET, Tj, TJ)
        std::wstring currentLine;
        for (const auto& sb : streamBuffers) {
            size_t btPos = 0;
            while ((btPos = sb.find("BT", btPos)) != std::string::npos) {
                size_t etPos = sb.find("ET", btPos);
                if (etPos == std::string::npos) etPos = sb.size();

                std::string block = sb.substr(btPos + 2, etPos - btPos - 2);
                size_t p = 0;
                while (p < block.size()) {
                    // Check for Td, TD, T* line-breaks
                    if (block[p] == 'T' && p + 1 < block.size() && (block[p + 1] == '*' || block[p + 1] == 'd' || block[p + 1] == 'D')) {
                        if (!currentLine.empty()) {
                            info.lines.push_back(currentLine);
                            currentLine.clear();
                        }
                        p += 2;
                        continue;
                    }

                    // Literal string (...) Tj
                    if (block[p] == '(') {
                        size_t strStart = p + 1;
                        int depth = 1;
                        size_t strEnd = strStart;
                        while (strEnd < block.size() && depth > 0) {
                            if (block[strEnd] == '\\') { strEnd += 2; continue; }
                            if (block[strEnd] == '(') depth++;
                            else if (block[strEnd] == ')') depth--;
                            strEnd++;
                        }
                        std::string literal = block.substr(strStart, strEnd - strStart - 1);
                        p = strEnd;

                        std::wstring text = detail::decodePdfLiteralString(literal);
                        if (!currentLine.empty() && !currentLine.ends_with(L' ') && !text.starts_with(L' ')) {
                            currentLine.push_back(L' ');
                        }
                        currentLine += text;
                        continue;
                    }

                    // Array of strings [(...) 20 (...)] TJ
                    if (block[p] == '[') {
                        size_t arrEnd = block.find(']', p);
                        if (arrEnd != std::string::npos) {
                            std::string arrContent = block.substr(p + 1, arrEnd - p - 1);
                            size_t ap = 0;
                            while (ap < arrContent.size()) {
                                if (arrContent[ap] == '(') {
                                    size_t s1 = ap + 1;
                                    int depth = 1;
                                    size_t s2 = s1;
                                    while (s2 < arrContent.size() && depth > 0) {
                                        if (arrContent[s2] == '\\') { s2 += 2; continue; }
                                        if (arrContent[s2] == '(') depth++;
                                        else if (arrContent[s2] == ')') depth--;
                                        s2++;
                                    }
                                    std::wstring chunk = detail::decodePdfLiteralString(arrContent.substr(s1, s2 - s1 - 1));
                                    currentLine += chunk;
                                    ap = s2;
                                } else if (arrContent[ap] == '<') {
                                    size_t hEnd = arrContent.find('>', ap);
                                    if (hEnd != std::string::npos) {
                                        currentLine += detail::decodePdfHexString(arrContent.substr(ap + 1, hEnd - ap - 1));
                                        ap = hEnd + 1;
                                    } else {
                                        ap++;
                                    }
                                } else if (arrContent[ap] == '-' && ap + 3 < arrContent.size() && std::isdigit(static_cast<unsigned char>(arrContent[ap + 1]))) {
                                    // Kerning break -> space
                                    if (!currentLine.empty() && !currentLine.ends_with(L' ')) {
                                        currentLine.push_back(L' ');
                                    }
                                    ap++;
                                } else {
                                    ap++;
                                }
                            }
                            p = arrEnd + 1;
                            continue;
                        }
                    }

                    // Hex string <...> Tj
                    if (block[p] == '<' && p + 1 < block.size() && block[p + 1] != '<') {
                        size_t hEnd = block.find('>', p);
                        if (hEnd != std::string::npos) {
                            std::wstring chunk = detail::decodePdfHexString(block.substr(p + 1, hEnd - p - 1));
                            if (!currentLine.empty() && !currentLine.ends_with(L' ') && !chunk.starts_with(L' ')) {
                                currentLine.push_back(L' ');
                            }
                            currentLine += chunk;
                            p = hEnd + 1;
                            continue;
                        }
                    }

                    p++;
                }

                if (!currentLine.empty()) {
                    info.lines.push_back(currentLine);
                    currentLine.clear();
                }

                btPos = etPos + 2;
            }
        }

        if (!currentLine.empty()) {
            info.lines.push_back(currentLine);
        }

        // Clean lines & compute metrics with human readability verification
        std::vector<std::wstring> cleanLines;
        size_t totalValidWords = 0;
        for (auto& l : info.lines) {
            // Trim whitespace
            while (!l.empty() && (l.front() == L' ' || l.front() == L'\t' || l.front() == L'\r' || l.front() == L'\n')) l.erase(l.begin());
            while (!l.empty() && (l.back() == L' ' || l.back() == L'\t' || l.back() == L'\r' || l.back() == L'\n')) l.pop_back();
            if (l.empty()) continue;

            // Check printable ratio and human readability (filter unmapped Identity-H glyph codes or binary noise)
            size_t printableCount = 0;
            size_t letterCount = 0;
            size_t glyphNoiseCount = 0;
            for (wchar_t ch : l) {
                if ((ch >= 32 && ch <= 126) || ch == L'\t') {
                    printableCount++;
                    if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9')) {
                        letterCount++;
                    }
                } else if (ch >= 0x00A0 && ch <= 0x024F) {
                    printableCount++;
                    glyphNoiseCount++;
                } else {
                    glyphNoiseCount += 2;
                }
            }

            if (printableCount == 0 || static_cast<double>(glyphNoiseCount) / static_cast<double>(l.length()) > 0.20 || letterCount < 2) {
                continue; // Skip lines that are raw font glyph codes or binary artifacts
            }

            // Count words in this line
            bool inWord = false;
            size_t lineWords = 0;
            for (wchar_t ch : l) {
                if (ch == L' ' || ch == L'\t' || iswpunct(ch)) {
                    inWord = false;
                } else if (!inWord) {
                    inWord = true;
                    lineWords++;
                }
            }

            if (lineWords > 0) {
                totalValidWords += lineWords;
                info.totalCharacters += l.length();
                cleanLines.push_back(l);
            }
        }

        // If the document has fewer than 3 total valid words across all lines, it's a presentation / vector slide deck
        if (totalValidWords < 3) {
            cleanLines.clear();
            info.totalWords = 0;
            info.totalCharacters = 0;
        } else {
            info.totalWords = totalValidWords;
        }
        info.lines = std::move(cleanLines);

        // Clean up title if it contains BOM, whitespace, or empty
        while (!info.title.empty() && (info.title.front() == L' ' || info.title.front() == 0xFEFF || info.title.front() == L'\t' || info.title.front() == 0x00FE || info.title.front() == 0x00FF)) {
            info.title.erase(info.title.begin());
        }
        while (!info.title.empty() && (info.title.back() == L' ' || info.title.back() == 0xFEFF || info.title.back() == L'\t')) {
            info.title.pop_back();
        }

        return info;
    }
};

} // namespace gx::app

