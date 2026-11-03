#include "godxplr/core/version.h"
#include "godxplr/core/entry_key.h"
#include "godxplr/platform/win32_common.h"
#include <iostream>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << gx::core::kApplicationName << " Index Service v"
              << gx::core::kVersionString << " (NTFS MFT/USN Daemon)\n";
    std::cout << "Status: Ready. Waiting for IPC queries.\n";

    // Quick verification of core types and platform bindings
    gx::EntryKey rootKey{gx::ProviderKind::NtfsVolume, 0x12345678, 5, 1};
    std::cout << "Root volume entry identity: " << gx::to_string(rootKey) << "\n";

    return 0;
}
