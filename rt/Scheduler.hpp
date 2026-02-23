#pragma once
#include "motion/MotionController.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

struct MotionTask {
    Motion motion;
    int speed; // 0..100
    std::chrono::milliseconds duration;
};

class Scheduler {
public:
    explicit Scheduler(MotionController& mc);
    ~Scheduler();

    void start();
    void stop();

    // event: push task then wake worker thread
    void enqueue(const MotionTask& task);
    void clear();

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    struct QueuedTask {
        MotionTask task;
        TimePoint enqueue_ts;
    };

    // Fixed-size latency stats (microseconds) to avoid unbounded memory growth.
    static constexpr std::size_t kLatencyBufSize = 4096;

    struct LatencyStats {
        std::array<uint64_t, kLatencyBufSize> samples_us{};
        std::size_t write_idx{0};
        std::size_t count{0};       // total samples seen (can exceed buf size)
        uint64_t min_us{UINT64_MAX};
        uint64_t max_us{0};
        long double sum_us{0.0L};

        void add(uint64_t us);
        void snapshot(std::array<uint64_t, kLatencyBufSize>& out,
                      std::size_t& valid,
                      std::size_t& total_count,
                      uint64_t& min_out,
                      uint64_t& max_out,
                      long double& sum_out) const;
    };

    void workerLoop();
    void startNextTask();
    void recordLatencyUs(uint64_t us);
    void printLatencySummary();

    MotionController& mc_;
    std::thread worker_;
    std::mutex mtx_;
    std::deque<QueuedTask> q_;
    std::atomic<bool> running_{false};

    // Linux event/timer based wake-ups
    int epoll_fd_{-1};
    int timer_fd_{-1};
    int event_fd_{-1};

    // worker state
    bool task_active_{false};
    MotionTask current_{};

    // latency stats
    mutable std::mutex stats_mtx_;
    LatencyStats latency_;
};