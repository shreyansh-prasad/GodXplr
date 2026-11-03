#pragma once

#ifdef _WIN32

#include "godxplr/core/name_index.h"
#include "godxplr/platform/usn_enumerator.h"
#include "godxplr/platform/win32_common.h"
#include <atomic>
#include <thread>

namespace gx::platform::win32 {

class UsnJournalTailer {
public:
    UsnJournalTailer(NameIndex& index, wchar_t volumeLetter = L'C');
    ~UsnJournalTailer();

    // Starts background tailing from cursor
    bool start(UsnJournalCursor cursor);

    // Stops background tailing thread cleanly
    void stop();

    [[nodiscard]] bool isRunning() const noexcept { return isRunning_.load(); }
    [[nodiscard]] uint64_t getLatestUsn() const noexcept { return currentUsn_.load(); }
    [[nodiscard]] size_t getMutationsApplied() const noexcept { return mutationsApplied_.load(); }

private:
    void tailLoop();

    NameIndex& index_;
    wchar_t volumeLetter_{L'C'};
    UsnJournalCursor cursor_{};

    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> isRunning_{false};
    std::atomic<uint64_t> currentUsn_{0};
    std::atomic<size_t> mutationsApplied_{0};

    ScopedHandle stopEvent_{};
    std::thread workerThread_;
};

} // namespace gx::platform::win32

#endif // _WIN32
