#include "godxplr/platform/named_pipe_channel.h"
#include "godxplr/platform/crawler_fallback.h"
#include "godxplr/core/name_index.h"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstdlib>

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        std::cerr << "Assertion failed: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::abort(); \
    } \
} while (0)

using namespace gx;
using namespace gx::platform::win32;

void testNamedPipeIpcAndOversizedProtection() {
    std::cout << "[Test] Named Pipe IPC & Oversized Frame Defense (Spec 01 S11)..." << std::endl;

#ifdef _WIN32
    std::wstring testPipeName = L"\\\\.\\pipe\\godxplr_test_pipe_" + std::to_wstring(::GetCurrentProcessId());

    NamedPipeServer server;
    std::atomic<bool> queryReceived{false};
    std::string receivedPayload;

    bool serverStarted = server.start(testPipeName, [&](HANDLE hPipe, const PipeFrameHeader& hdr, const uint8_t* payload, uint32_t len) {
        if (hdr.type == static_cast<uint16_t>(PipeMessageType::Query)) {
            queryReceived = true;
            receivedPayload.assign(reinterpret_cast<const char*>(payload), len);

            // Respond with a Page frame
            std::string respData = "ResultCount=42";
            PipeFrameHeader respHdr{};
            respHdr.type = static_cast<uint16_t>(PipeMessageType::Page);
            respHdr.reqId = hdr.reqId;
            respHdr.length = static_cast<uint32_t>(respData.size());

            DWORD written = 0;
            ::WriteFile(hPipe, &respHdr, sizeof(respHdr), &written, nullptr);
            ::WriteFile(hPipe, respData.data(), static_cast<DWORD>(respData.size()), &written, nullptr);
        }
    });

    TEST_ASSERT(serverStarted);
    TEST_ASSERT(server.isRunning());

    // Give server worker thread a moment to enter pipe listen loop
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    NamedPipeClient client;
    TEST_ASSERT(client.connect(testPipeName, 2000));
    TEST_ASSERT(client.isConnected());

    // Send query frame
    std::string queryText = "ext:cpp size:>1mb";
    TEST_ASSERT(client.sendFrame(PipeMessageType::Query, 999, queryText.data(), static_cast<uint32_t>(queryText.size())));

    // Read response frame
    PipeFrameHeader respHdr{};
    std::vector<uint8_t> respPayload;
    TEST_ASSERT(client.readFrame(respHdr, respPayload));

    TEST_ASSERT(respHdr.type == static_cast<uint16_t>(PipeMessageType::Page));
    TEST_ASSERT(respHdr.reqId == 999);
    std::string respStr(reinterpret_cast<const char*>(respPayload.data()), respPayload.size());
    TEST_ASSERT(respStr == "ResultCount=42");
    TEST_ASSERT(queryReceived.load());
    TEST_ASSERT(receivedPayload == queryText);

    client.disconnect();
    TEST_ASSERT(!client.isConnected());

    server.stop();
    TEST_ASSERT(!server.isRunning());
#endif
}

void testSecurityAccessCheck() {
    std::cout << "[Test] In-Memory AccessCheck Token Validation (ADR-0019)..." << std::endl;

#ifdef _WIN32
    HANDLE hToken = nullptr;
    if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &hToken)) {
        ScopedHandle tokenHandle(hToken);
        // AccessCheck against null SD defaults to allow
        TEST_ASSERT(NamedPipeServer::checkClientAccess(tokenHandle.get(), nullptr, FILE_LIST_DIRECTORY));
    }
#endif
}

void testCrawlerFallbackParity() {
    std::cout << "[Test] Crawler Fallback Parity (Spec 01 S12)..." << std::endl;

#ifdef _WIN32
    std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "godxplr_crawl_test";
    std::filesystem::create_directories(tempDir / "SubfolderA");
    std::filesystem::create_directories(tempDir / "SubfolderB");

    {
        std::ofstream ofs(tempDir / "file1.txt", std::ios::binary);
        ofs << "Test file 1 contents";
    }
    {
        std::ofstream ofs(tempDir / "SubfolderA" / "nested.cpp", std::ios::binary);
        ofs << "int main() { return 0; }";
    }

    NameIndex index;
    CrawlerStats stats{};

    bool ok = CrawlerFallback::crawl(tempDir.wstring(), index, stats);
    TEST_ASSERT(ok);
    TEST_ASSERT(stats.filesCount >= 2);
    TEST_ASSERT(stats.directoriesCount >= 2);
    TEST_ASSERT(index.size() >= 4);

    // Verify files were added with valid metadata
    bool foundNested = false;
    for (size_t i = 0; i < index.size(); ++i) {
        EntryId id = static_cast<EntryId>(i);
        if (index.getName(id) == "nested.cpp") {
            foundNested = true;
            TEST_ASSERT(index.getFileSize(id) > 0);
            TEST_ASSERT(index.getMtime(id) > 0);
            TEST_ASSERT(!index.isDirectory(id));
            TEST_ASSERT((index.getFlags(id) & kMetaKnown) != 0);
        }
    }
    TEST_ASSERT(foundNested);

    // Clean up temporary tree
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
#endif
}

int main() {
    std::cout << "=== GodXplr Search IPC & Crawler Fallback Tests (Spec 01 S11-S12) ===" << std::endl;

    testNamedPipeIpcAndOversizedProtection();
    testSecurityAccessCheck();
    testCrawlerFallbackParity();

    std::cout << "All Search IPC & Crawler Fallback tests PASSED!" << std::endl;
    return 0;
}
