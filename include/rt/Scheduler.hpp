#pragma once

#include "motion/MotionController.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

/**
 * @brief A single motion task to be executed by the scheduler.
 */
struct MotionTask {
    Motion motion;
    int speed;
    std::chrono::milliseconds duration;
};

/**
 * @brief Real-time motion scheduler using blocking Linux I/O primitives.
 *
 * A worker thread blocks in epoll_wait(). New commands wake the worker via
 * eventfd, while task expiration is handled by timerfd.
 */
class Scheduler {
public:
    explicit Scheduler(MotionController& mc);
    ~Scheduler();

    void start();
    void stop();
    void enqueue(const MotionTask& task);
    void clear();
    void replaceNow(const MotionTask& task);

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    struct QueuedTask {
        MotionTask task;
        TimePoint enqueueTs;
    };

    static constexpr std::size_t kLatencyBufSize = 4096;

    struct LatencyStats {
        std::array<uint64_t, kLatencyBufSize> samplesUs{};
        std::size_t writeIdx{0};
        std::size_t count{0};
        uint64_t minUs{UINT64_MAX};
        uint64_t maxUs{0};
        long double sumUs{0.0L};

        void add(uint64_t us);
        void snapshot(std::array<uint64_t, kLatencyBufSize>& out,
                      std::size_t& valid,
                      std::size_t& totalCount,
                      uint64_t& minOut,
                      uint64_t& maxOut,
                      long double& sumOut) const;
    };

    void workerLoop();
    void startNextTask();
    void recordLatencyUs(uint64_t us);
    void printLatencySummary();

    MotionController& mc_;
    std::thread worker_;
    std::mutex mutex_;
    std::deque<QueuedTask> queue_;
    std::atomic<bool> running_{false};

    int epollFd_{-1};
    int timerFd_{-1};
    int eventFd_{-1};

    bool taskActive_{false};
    MotionTask current_{};

    mutable std::mutex statsMutex_;
    LatencyStats latency_;
};
