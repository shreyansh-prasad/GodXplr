#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <objbase.h>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <future>
#include <memory>

namespace gx::platform::win32 {

// RAII COM apartment initializer for the current calling thread
class ComInitializer {
public:
    explicit ComInitializer(DWORD coinitFlags = COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE) noexcept {
        hr_ = ::CoInitializeEx(nullptr, coinitFlags);
    }

    ~ComInitializer() {
        if (SUCCEEDED(hr_)) {
            ::CoUninitialize();
        }
    }

    ComInitializer(const ComInitializer&) = delete;
    ComInitializer& operator=(const ComInitializer&) = delete;

    [[nodiscard]] bool succeeded() const noexcept { return SUCCEEDED(hr_); }
    [[nodiscard]] HRESULT result() const noexcept { return hr_; }

private:
    HRESULT hr_{E_FAIL};
};

// Dedicated background Single-Threaded Apartment (STA) worker thread with message pump
class StaWorkerThread {
public:
    StaWorkerThread();
    ~StaWorkerThread();

    StaWorkerThread(const StaWorkerThread&) = delete;
    StaWorkerThread& operator=(const StaWorkerThread&) = delete;

    void post(std::function<void()> task);

    template <typename F, typename R = std::invoke_result_t<F>>
    std::future<R> submit(F&& fn) {
        auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(fn));
        std::future<R> fut = task->get_future();
        post([task]() {
            (*task)();
        });
        return fut;
    }

    void stop();
    [[nodiscard]] bool isRunning() const noexcept { return isRunning_.load(std::memory_order_relaxed); }
    [[nodiscard]] DWORD threadId() const noexcept { return threadId_; }

private:
    void threadMain();

    std::atomic<bool> isRunning_{false};
    std::atomic<bool> stopRequested_{false};
    DWORD threadId_{0};
    std::thread thread_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> taskQueue_;
};

} // namespace gx::platform::win32

#endif // _WIN32

