#include "godxplr/platform/shm_channel.h"
#include <iostream>
#include <cstdlib>
#include <thread>
#include <chrono>

#define TEST_ASSERT(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "Assertion failed: " #cond " at " << __FILE__         \
                      << ":" << __LINE__ << std::endl;                         \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

using namespace gx::platform::win32;

void testShmBasicRoundtrip() {
    std::cout << "[Test] ShmBasicRoundtrip...\n";

    constexpr uint32_t kTestSessionId = 9999;

    ShmServer server;
    bool created = server.create(kTestSessionId, false);
    TEST_ASSERT(created);
    TEST_ASSERT(server.isCreated());
    TEST_ASSERT(server.getBuffer() != nullptr);

    ShmClient client;
    bool connected = client.open(kTestSessionId, false);
    TEST_ASSERT(connected);
    TEST_ASSERT(client.isConnected());
    TEST_ASSERT(client.getBuffer() != nullptr);

    // Client submits query
    uint64_t gen = client.submitQuery("invoice ext:pdf", 0x01);
    TEST_ASSERT(gen == 1);

    // Server should receive query notification
    bool queryArrived = server.waitForQuery(1000);
    TEST_ASSERT(queryArrived);

    auto* sBuffer = server.getBuffer();
    TEST_ASSERT(sBuffer->queryLen == 15);
    TEST_ASSERT(std::string_view(sBuffer->queryText) == "invoice ext:pdf");
    TEST_ASSERT(sBuffer->optionsFlags == 0x01);

    // Server populates response
    sBuffer->resultCount = 1;
    sBuffer->totalMatches = 1;
    sBuffer->statusFlags = 1;
    sBuffer->rows[0].objectId = 42;
    sBuffer->rows[0].size = 1048576;
    sBuffer->rows[0].nameLen = 11;
    std::memcpy(sBuffer->rows[0].name, "invoice.pdf", 11);
    sBuffer->rows[0].name[11] = '\0';
    sBuffer->resultGeneration.store(gen, std::memory_order_release);

    server.notifyResultsReady();

    // Client waits for results
    bool resultsReady = client.waitForResults(gen, 1000);
    TEST_ASSERT(resultsReady);

    const auto* cBuffer = client.getBuffer();
    TEST_ASSERT(cBuffer->resultCount == 1);
    TEST_ASSERT(cBuffer->totalMatches == 1);
    TEST_ASSERT(cBuffer->rows[0].objectId == 42);
    TEST_ASSERT(cBuffer->rows[0].size == 1048576);
    TEST_ASSERT(std::string_view(cBuffer->rows[0].name) == "invoice.pdf");

    client.close();
    server.close();
    std::cout << "  -> Passed!\n";
}

void testShmCancellation() {
    std::cout << "[Test] ShmCancellation...\n";

    constexpr uint32_t kTestSessionId = 9998;

    ShmServer server;
    TEST_ASSERT(server.create(kTestSessionId, false));

    ShmClient client;
    TEST_ASSERT(client.open(kTestSessionId, false));

    // Submit initial query
    uint64_t gen1 = client.submitQuery("a");
    TEST_ASSERT(gen1 == 1);

    // Rapid second keystroke before server finishes
    uint64_t gen2 = client.submitQuery("ab");
    TEST_ASSERT(gen2 == 2);

    // Server wakes up and checks generation
    TEST_ASSERT(server.waitForQuery(500));
    auto* sBuffer = server.getBuffer();
    uint64_t currentGen = sBuffer->queryGeneration.load(std::memory_order_relaxed);
    TEST_ASSERT(currentGen == gen2);
    TEST_ASSERT(std::string_view(sBuffer->queryText) == "ab");

    client.close();
    server.close();
    std::cout << "  -> Passed!\n";
}

void testShmCleanShutdown() {
    std::cout << "[Test] ShmCleanShutdown...\n";

    constexpr uint32_t kTestSessionId = 9997;

    ShmServer server;
    TEST_ASSERT(server.create(kTestSessionId, false));

    // Server waiting in separate thread
    std::atomic<bool> threadUnblocked{false};
    std::thread serverThread([&]() {
        bool gotQuery = server.waitForQuery(5000);
        // Should unblock with false due to requestStop
        TEST_ASSERT(!gotQuery);
        threadUnblocked.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    server.requestStop();

    if (serverThread.joinable()) {
        serverThread.join();
    }
    TEST_ASSERT(threadUnblocked.load());

    server.close();
    std::cout << "  -> Passed!\n";
}

int main() {
    std::cout << "======================================\n";
    std::cout << "Running GodXplr ShmChannel Unit Tests\n";
    std::cout << "======================================\n";

    testShmBasicRoundtrip();
    testShmCancellation();
    testShmCleanShutdown();

    std::cout << "All ShmChannel unit tests passed successfully!\n";
    return 0;
}
