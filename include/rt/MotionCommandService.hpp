#pragma once

#include "dds/LocalDdsBus.hpp"
#include "dds/VehicleTopics.hpp"
#include "motion/MotionController.hpp"

#include <atomic>
#include <mutex>
#include <optional>
#include <thread>

/**
 * @brief Executes motion commands received through DDS-style pub/sub.
 *
 * The worker thread blocks in epoll_wait(). New motion topics wake the worker
 * via eventfd, while command timeout is handled by timerfd. This keeps the
 * design event-driven without a global scheduler object.
 */
class MotionCommandService final {
public:
    MotionCommandService(LocalDdsBus& bus, MotionController& controller);
    ~MotionCommandService();

    MotionCommandService(const MotionCommandService&) = delete;
    MotionCommandService& operator=(const MotionCommandService&) = delete;

    void start();
    void stop();
    [[nodiscard]] bool isRunning() const noexcept { return running_.load(std::memory_order_acquire); }

private:
    void onCommand(const MotionCommandTopic& topic);
    void workerLoop();
    void applyCommand(const MotionCommandTopic& topic);
    void armTimer(std::chrono::milliseconds duration);
    void disarmTimer();
    void notifyWorker();
    void drainEventFd() const;
    void drainTimerFd() const;
    void closeResources();

    LocalDdsBus& bus_;
    MotionController& controller_;
    LocalDdsBus::Subscription subscription_{};

    std::mutex mutex_;
    std::optional<MotionCommandTopic> pendingCommand_{};
    MotionCommandTopic activeCommand_{};
    bool activeCommandArmed_{false};

    std::atomic<bool> running_{false};
    std::thread worker_{};
    int epollFd_{-1};
    int timerFd_{-1};
    int eventFd_{-1};
};
