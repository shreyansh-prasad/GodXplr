#include "godxplr/content/text_extractor.h"
#include "godxplr/content/encoding_detector.h"
#include <cstdio>
#include <vector>
#include <algorithm>

namespace gx::content {

ExtractionStatus TextExtractor::extractFile(const std::wstring& filePath, ITextSink& sink) {
#ifdef _WIN32
    FILE* fp = nullptr;
    errno_t err = _wfopen_s(&fp, filePath.c_str(), L"rb");
    if (err != 0 || !fp) {
        if (err == ENOENT) return ExtractionStatus::FileNotFound;
        if (err == EACCES) return ExtractionStatus::AccessDenied;
        return ExtractionStatus::IoError;
    }

    _fseeki64(fp, 0, SEEK_END);
    uint64_t fileSize = static_cast<uint64_t>(_ftelli64(fp));
    _fseeki64(fp, 0, SEEK_SET);

    if (fileSize == 0) {
        sink.onDocumentComplete(0, 0);
        fclose(fp);
        return ExtractionStatus::Success;
    }

    std::vector<uint8_t> sampleBuf(std::min<size_t>(static_cast<size_t>(fileSize), 8192));
    size_t sampleRead = fread(sampleBuf.data(), 1, sampleBuf.size(), fp);
    if (sampleRead == 0) {
        fclose(fp);
        return ExtractionStatus::IoError;
    }

    size_t bomBytes = 0;
    EncodingType encoding = EncodingDetector::detect(sampleBuf.data(), sampleRead, bomBytes);
    if (encoding == EncodingType::Binary) {
        fclose(fp);
        return ExtractionStatus::BinarySkipped;
    }

    _fseeki64(fp, static_cast<__int64>(bomBytes), SEEK_SET);

    constexpr size_t kChunkSize = 64 * 1024;
    std::vector<uint8_t> chunkBuf(kChunkSize);
    uint64_t totalExtracted = 0;
    bool truncated = false;

    while (totalExtracted < maxBytes_) {
        size_t toRead = std::min<size_t>(chunkBuf.size(), static_cast<size_t>(maxBytes_ - totalExtracted));
        size_t read = fread(chunkBuf.data(), 1, toRead, fp);
        if (read == 0) {
            break;
        }

        std::string utf8Chunk = EncodingDetector::toUtf8(chunkBuf.data(), read, encoding, 0);
        sink.onText(utf8Chunk);
        totalExtracted += utf8Chunk.size();
    }

    if (totalExtracted >= maxBytes_ && (fileSize - bomBytes) > totalExtracted) {
        truncated = true;
    }

    sink.onDocumentComplete(fileSize, totalExtracted);
    fclose(fp);

    return truncated ? ExtractionStatus::Truncated : ExtractionStatus::Success;
#else
    return ExtractionStatus::IoError;
#endif
}

ExtractionStatus TextExtractor::extractBuffer(const uint8_t* data, size_t size, ITextSink& sink) {
    if (!data || size == 0) {
        sink.onDocumentComplete(0, 0);
        return ExtractionStatus::Success;
    }

    size_t bomBytes = 0;
    EncodingType encoding = EncodingDetector::detect(data, size, bomBytes);
    if (encoding == EncodingType::Binary) {
        return ExtractionStatus::BinarySkipped;
    }

    size_t toExtract = std::min(size - bomBytes, maxBytes_);
    std::string utf8 = EncodingDetector::toUtf8(data + bomBytes, toExtract, encoding, 0);
    sink.onText(utf8);
    sink.onDocumentComplete(size, utf8.size());

    return (size - bomBytes > maxBytes_) ? ExtractionStatus::Truncated : ExtractionStatus::Success;
}

} // namespace gx::content

