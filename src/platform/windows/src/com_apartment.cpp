#ifdef _WIN32

#include "godxplr/platform/com_apartment.h"

namespace gx::platform::win32 {

StaWorkerThread::StaWorkerThread() {
    thread_ = std::thread(&StaWorkerThread::threadMain, this);

    // Wait until STA worker has successfully initialized COM and message queue
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]() {
        return isRunning_.load(std::memory_order_acquire);
    });
}

StaWorkerThread::~StaWorkerThread() {
    stop();
}

void StaWorkerThread::post(std::function<void()> task) {
    if (!task) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopRequested_.load(std::memory_order_relaxed)) {
            return;
        }
        taskQueue_.push(std::move(task));
    }
    cv_.notify_one();

    // Wake up Win32 message wait if sleeping
    if (threadId_ != 0) {
        ::PostThreadMessageW(threadId_, WM_NULL, 0, 0);
    }
}

void StaWorkerThread::stop() {
    bool expected = false;
    if (stopRequested_.compare_exchange_strong(expected, true)) {
        cv_.notify_all();
        if (threadId_ != 0) {
            ::PostThreadMessageW(threadId_, WM_QUIT, 0, 0);
        }
        if (thread_.joinable()) {
            thread_.join();
        }
    }
}

void StaWorkerThread::threadMain() {
    threadId_ = ::GetCurrentThreadId();
    ComInitializer comInit(COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    // Force creation of Win32 message queue for this thread
    MSG msg{};
    ::PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        isRunning_.store(true, std::memory_order_release);
    }
    cv_.notify_all();

    while (!stopRequested_.load(std::memory_order_acquire)) {
        // Drain pending tasks
        std::function<void()> currentTask;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (!taskQueue_.empty()) {
                currentTask = std::move(taskQueue_.front());
                taskQueue_.pop();
            }
        }

        if (currentTask) {
            currentTask();
            continue; // Keep draining tasks before entering message wait
        }

        // Pump Win32 messages for STA COM callbacks / marshaling
        while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                stopRequested_.store(true, std::memory_order_release);
                break;
            }
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }

        if (stopRequested_.load(std::memory_order_relaxed)) {
            break;
        }

        // Wait for next task or message
        std::unique_lock<std::mutex> lock(mutex_);
        if (taskQueue_.empty() && !stopRequested_.load(std::memory_order_relaxed)) {
            cv_.wait_for(lock, std::chrono::milliseconds(50), [this]() {
                return !taskQueue_.empty() || stopRequested_.load(std::memory_order_relaxed);
            });
        }
    }

    // Drain remaining tasks before thread termination
    while (true) {
        std::function<void()> remainingTask;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (taskQueue_.empty()) {
                break;
            }
            remainingTask = std::move(taskQueue_.front());
            taskQueue_.pop();
        }
        if (remainingTask) {
            remainingTask();
        }
    }

    isRunning_.store(false, std::memory_order_release);
}

} // namespace gx::platform::win32

#endif // _WIN32
