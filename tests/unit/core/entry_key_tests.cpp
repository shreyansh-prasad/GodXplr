#include "godxplr/core/entry_key.h"
#include <cassert>
#include <iostream>
#include <unordered_set>

void test_entry_key_equality() {
    gx::EntryKey k1{gx::ProviderKind::NtfsVolume, 0xABCDEF, 100, 1};
    gx::EntryKey k2{gx::ProviderKind::NtfsVolume, 0xABCDEF, 100, 1};
    gx::EntryKey k3{gx::ProviderKind::NtfsVolume, 0xABCDEF, 101, 1};

    assert(k1 == k2);
    assert(k1 != k3);
    assert(k1 < k3);
    std::cout << "[PASS] test_entry_key_equality\n";
}

void test_entry_key_hash() {
    gx::EntryKey k1{gx::ProviderKind::NtfsVolume, 0xABCDEF, 100, 1};
    gx::EntryKey k2{gx::ProviderKind::NtfsVolume, 0xABCDEF, 100, 1};
    gx::EntryKey k3{gx::ProviderKind::NtfsVolume, 0xABCDEF, 101, 1};

    std::unordered_set<gx::EntryKey> keySet;
    keySet.insert(k1);
    assert(keySet.count(k2) == 1);
    assert(keySet.count(k3) == 0);
    std::cout << "[PASS] test_entry_key_hash\n";
}

int main() {
    std::cout << "Running EntryKey unit tests...\n";
    test_entry_key_equality();
    test_entry_key_hash();
    std::cout << "All EntryKey tests passed.\n";
    return 0;
}
