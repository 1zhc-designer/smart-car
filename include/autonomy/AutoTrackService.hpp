#pragma once

#include "dds/LocalDdsBus.hpp"
#include "dds/VehicleTopics.hpp"
#include "gimbal/GimbalService.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>

#include <gpiod.hpp>

class AutoTrackService final {
public:
    AutoTrackService(LocalDdsBus& bus,
                     GimbalService& gimbal,
                     const char* gpioChipPath = "/dev/gpiochip0");
    ~AutoTrackService();

    AutoTrackService(const AutoTrackService&) = delete;
    AutoTrackService& operator=(const AutoTrackService&) = delete;

    void start();
    void stop();
    [[nodiscard]] bool isRunning() const noexcept { return running_.load(std::memory_order_acquire); }

private:
    enum class EventType {
        LineChanged,
        ObstacleChanged,
        ObjectDetected,
    };

    struct Event {
        EventType type;
        bool leftActive{false};
        bool rightActive{false};
        bool obstacleActive{false};
    };

    enum class ObjectStage {
        Idle,
        WaitingBeforeTrigger,
        WaitingAfterTrigger,
    };

    void sensorThreadFunc();
    void obstacleThreadFunc();
    void controlThreadFunc();

    void ensureTimersReady();
    void closeKernelObjects() noexcept;

    void queueLineEvent(bool leftActive, bool rightActive);
    void queueObstacleEvent(bool obstacleActive);
    void queueObjectDetectedEvent();

    void processQueuedEvents();
    void handleLineEvent(bool leftActive, bool rightActive);
    void handleObstacleEvent(bool obstacleActive);
    void handleObjectDetectedEvent();
    void handleObjectTimerEvent();
    void handleGimbalTimerEvent();

    void recomputeMotion();
    void publishMotion(Motion motion, int speed, const char* source);
    void publishStop(const char* source);
    void forceStopStateAndPublish(const char* source);
    void setGimbalCenter() noexcept;
    void armObjectTimerOnce(std::chrono::milliseconds delay);
    void armGimbalTimerPeriodic(std::chrono::milliseconds period);
    void disarmObjectTimer() noexcept;
    void disarmGimbalTimer() noexcept;

    static bool obstacleTriggered(gpiod::line::value value);

    static constexpr int kBaseSpeed = 10;
    static constexpr int kOuterSpinSpeed = 50;
    static constexpr int kSweepMin = 150;
    static constexpr int kSweepMax = 450;
    static constexpr int kSweepCenter = 307;
    static constexpr int kSweepStep = 2;

    static constexpr auto kSensorWaitSlice = std::chrono::milliseconds(20);
    static constexpr auto kObstacleWaitSlice = std::chrono::milliseconds(20);
    static constexpr auto kGimbalPeriod = std::chrono::milliseconds(50);
    static constexpr auto kStopWaitDuration = std::chrono::seconds(3);
    static constexpr auto kCaptureSettleDuration = std::chrono::milliseconds(1500);
    static constexpr auto kCooldownDuration = std::chrono::seconds(5);
    static constexpr std::size_t kMaxQueuedEvents = 64;

    LocalDdsBus& bus_;
    GimbalService& gimbal_;
    const char* gpioChipPath_;

    std::atomic<bool> running_{false};
    std::atomic<bool> objectEventQueued_{false};

    std::optional<gpiod::chip> chip_;
    std::optional<gpiod::line_request> sensorReq_;
    std::optional<gpiod::line_request> obstacleReq_;

    gpiod::line::offsets sensorOffsets_{13, 26};
    gpiod::line::offsets obstacleOffsets_{16, 12};

    LocalDdsBus::Subscription detectionSub_{};
    std::thread sensorThread_;
    std::thread obstacleThread_;
    std::thread controlThread_;

    std::mutex queueMutex_;
    std::deque<Event> eventQueue_;

    std::mutex stateMutex_;
    bool leftActive_{false};
    bool rightActive_{false};
    bool obstacleActive_{false};
    bool isHandlingObject_{false};
    bool objectPanLocked_{false};
    ObjectStage objectStage_{ObjectStage::Idle};
    int currentPan_{kSweepCenter};
    int currentStep_{kSweepStep};
    int lockedPan_{kSweepCenter};
    Motion lastMotion_{Motion::Stop};
    int lastSpeed_{0};
    std::chrono::steady_clock::time_point cooldownUntil_{};

    int stopFd_{-1};
    int queueFd_{-1};
    int epollFd_{-1};
    int gimbalTimerFd_{-1};
    int objectTimerFd_{-1};
};