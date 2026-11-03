#include "godxplr/providers/cloud_storage_provider.h"
#include "godxplr/providers/network_share_provider.h"

#include <iostream>
#include <string>
#include <cstdlib>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed at line " << __LINE__ << ": " #cond "\n"; \
            std::abort(); \
        } \
    } while (0)

using namespace gx::providers;

void testCloudPlaceholderDetection() {
    std::cout << "[Test] Cloud Placeholder Detection & Anti-Hydration...\n";

    // 1. Dehydrated attribute combinations
    DWORD attrRecallDataAccess = FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS | FILE_ATTRIBUTE_ARCHIVE;
    TEST_ASSERT(CloudStorageProvider::isDehydratedPlaceholder(attrRecallDataAccess));

    DWORD attrRecallOpen = FILE_ATTRIBUTE_RECALL_ON_OPEN | FILE_ATTRIBUTE_ARCHIVE;
    TEST_ASSERT(CloudStorageProvider::isDehydratedPlaceholder(attrRecallOpen));

    DWORD attrOffline = FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_ARCHIVE;
    TEST_ASSERT(CloudStorageProvider::isDehydratedPlaceholder(attrOffline));

    // Normal hydrated files must NOT be flagged as dehydrated placeholders
    DWORD attrNormal = FILE_ATTRIBUTE_NORMAL;
    TEST_ASSERT(!CloudStorageProvider::isDehydratedPlaceholder(attrNormal));

    DWORD attrArchive = FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_READONLY;
    TEST_ASSERT(!CloudStorageProvider::isDehydratedPlaceholder(attrArchive));

    // 2. Query file cloud state on local system file
    auto state = CloudStorageProvider::queryFileCloudState(L"C:\\Windows\\notepad.exe");
    TEST_ASSERT(state == CloudSyncState::NotCloudFile);

    std::cout << "  -> PASS\n";
}

void testNetworkShareProvider() {
    std::cout << "[Test] Network Share UNC Path & Connectivity Checks...\n";

    // 1. UNC Path recognition
    TEST_ASSERT(NetworkShareProvider::isUncPath(L"\\\\server\\share\\folder"));
    TEST_ASSERT(NetworkShareProvider::isUncPath(L"\\\\192.168.1.1\\public"));
    TEST_ASSERT(!NetworkShareProvider::isUncPath(L"C:\\Windows\\System32"));
    TEST_ASSERT(!NetworkShareProvider::isUncPath(L"\\\\?\\C:\\extended"));

    // 2. Connectivity check on non-network path
    auto statLocal = NetworkShareProvider::checkShareConnectivity(L"C:\\Windows\\notepad.exe");
    TEST_ASSERT(statLocal == NetworkShareStatus::NotNetworkPath);

    // 3. Connectivity check on disconnected/invalid fake share
    auto statFake = NetworkShareProvider::checkShareConnectivity(L"\\\\fake_server_invalid_987654\\non_existent_share");
    TEST_ASSERT(statFake == NetworkShareStatus::Disconnected || statFake == NetworkShareStatus::InvalidPath);

    std::cout << "  -> PASS\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  GodXplr Spec 08 Network & Cloud Tests \n";
    std::cout << "========================================\n";

    testCloudPlaceholderDetection();
    testNetworkShareProvider();

    std::cout << "========================================\n";
    std::cout << "  ALL NETWORK & CLOUD TESTS PASSED (100%)\n";
    std::cout << "========================================\n";
    return 0;
}
