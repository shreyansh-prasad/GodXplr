#include "godxplr/core/simd_search.h"
#include <immintrin.h>
#include <cstring>
#include <algorithm>

namespace gx::search {

size_t scanArenaScalar(const NameIndex& index,
                       std::string_view needle,
                       std::vector<EntryId>& outIds,
                       size_t maxMatches) {
    if (needle.empty()) {
        return 0;
    }

    std::string foldedNeedle = NameIndex::foldCase(needle);
    std::span<const char> arena = index.getFoldedArena();
    if (arena.empty() || arena.size() < foldedNeedle.size()) {
        return 0;
    }

    const char* const start = arena.data();
    const char* const end = start + arena.size();
    const char* curr = start;

    size_t count = 0;
    while (curr + foldedNeedle.size() <= end && count < maxMatches) {
        const char* hit = static_cast<const char*>(
            std::memchr(curr, foldedNeedle[0], end - curr)
        );
        if (!hit || hit + foldedNeedle.size() > end) {
            break;
        }

        if (std::memcmp(hit, foldedNeedle.data(), foldedNeedle.size()) == 0) {
            uint32_t offset = static_cast<uint32_t>(hit - start);
            EntryId id = index.findByOffset(offset);
            if (id != kNoEntry) {
                outIds.push_back(id);
                count++;
            }
            // Skip to the end of this string (delimited by \0)
            const char* nextNul = static_cast<const char*>(
                std::memchr(hit, '\0', end - hit)
            );
            if (!nextNul) {
                break;
            }
            curr = nextNul + 1;
        } else {
            curr = hit + 1;
        }
    }

    return count;
}

size_t scanArenaAVX2(const NameIndex& index,
                     std::string_view needle,
                     std::vector<EntryId>& outIds,
                     size_t maxMatches) {
    if (needle.empty()) {
        return 0;
    }

    // Fold query to lowercase for case-insensitive matching
    std::string foldedNeedle = NameIndex::foldCase(needle);
    std::span<const char> arena = index.getFoldedArena();
    if (arena.empty() || arena.size() < foldedNeedle.size()) {
        return 0;
    }

    const size_t needleLen = foldedNeedle.size();
    const char* const arenaStart = arena.data();
    const char* const arenaEnd = arenaStart + arena.size();
    const char* curr = arenaStart;

    size_t count = 0;

    // Single character search optimization
    if (needleLen == 1) {
        const char target = foldedNeedle[0];
        const __m256i targetVec = _mm256_set1_epi8(target);

        while (curr + 32 <= arenaEnd && count < maxMatches) {
            __m256i block = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(curr));
            __m256i eq = _mm256_cmpeq_epi8(targetVec, block);
            uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(eq));

            while (mask != 0 && count < maxMatches) {
                int bit = _tzcnt_u32(mask);
                uint32_t offset = static_cast<uint32_t>((curr + bit) - arenaStart);
                EntryId id = index.findByOffset(offset);
                if (id != kNoEntry) {
                    outIds.push_back(id);
                    count++;
                }

                // Skip to next string delimiter to avoid duplicate reporting
                const char* nextNul = static_cast<const char*>(
                    std::memchr(curr + bit, '\0', arenaEnd - (curr + bit))
                );
                if (nextNul && nextNul + 1 > curr + bit) {
                    curr = nextNul + 1;
                    goto next_block;
                }
                mask &= (mask - 1);
            }
            curr += 32;
        next_block:;
        }
    } else {
        // Multi-character needle: broadcast first and last characters
        const char firstChar = foldedNeedle.front();
        const char lastChar = foldedNeedle.back();
        const size_t tailOffset = needleLen - 1;

        const __m256i firstVec = _mm256_set1_epi8(firstChar);
        const __m256i lastVec = _mm256_set1_epi8(lastChar);

        while (curr + 32 + tailOffset <= arenaEnd && count < maxMatches) {
            __m256i blockHead = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(curr));
            __m256i blockTail = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(curr + tailOffset));

            __m256i eqHead = _mm256_cmpeq_epi8(firstVec, blockHead);
            __m256i eqTail = _mm256_cmpeq_epi8(lastVec, blockTail);
            uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_and_si256(eqHead, eqTail)));

            while (mask != 0 && count < maxMatches) {
                int bit = _tzcnt_u32(mask);
                const char* hitPtr = curr + bit;

                // Verify the middle characters if needleLen > 2
                if (needleLen == 2 || std::memcmp(hitPtr + 1, foldedNeedle.data() + 1, needleLen - 2) == 0) {
                    uint32_t offset = static_cast<uint32_t>(hitPtr - arenaStart);
                    EntryId id = index.findByOffset(offset);
                    if (id != kNoEntry) {
                        outIds.push_back(id);
                        count++;
                    }

                    // Advance pointer past the current file entry's NUL terminator
                    const char* nextNul = static_cast<const char*>(
                        std::memchr(hitPtr, '\0', arenaEnd - hitPtr)
                    );
                    if (nextNul && nextNul + 1 > curr) {
                        curr = nextNul + 1;
                        goto next_multi_block;
                    }
                }
                mask &= (mask - 1);
            }
            curr += 32;
        next_multi_block:;
        }
    }

    // Scalar fallback for the tail remainder (< 32 bytes)
    if (curr + needleLen <= arenaEnd && count < maxMatches) {
        while (curr + needleLen <= arenaEnd && count < maxMatches) {
            const char* hit = static_cast<const char*>(
                std::memchr(curr, foldedNeedle[0], arenaEnd - curr)
            );
            if (!hit || hit + needleLen > arenaEnd) {
                break;
            }

            if (std::memcmp(hit, foldedNeedle.data(), needleLen) == 0) {
                uint32_t offset = static_cast<uint32_t>(hit - arenaStart);
                EntryId id = index.findByOffset(offset);
                if (id != kNoEntry) {
                    outIds.push_back(id);
                    count++;
                }
                const char* nextNul = static_cast<const char*>(
                    std::memchr(hit, '\0', arenaEnd - hit)
                );
                if (!nextNul) {
                    break;
                }
                curr = nextNul + 1;
            } else {
                curr = hit + 1;
            }
        }
    }

    return count;
}

} // namespace gx::search
