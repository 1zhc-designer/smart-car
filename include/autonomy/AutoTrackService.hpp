#pragma once

#include "dds/LocalDdsBus.hpp"
#include "dds/VehicleTopics.hpp"
#include "gimbal/GimbalService.hpp"

#include <atomic>
#include <optional>
#include <thread>
#include <chrono>
#include <gpiod.hpp>

class AutoTrackService final {
public:
    AutoTrackService(LocalDdsBus& bus,
                     GimbalService& gimbal,
                     const char* gpioChipPath = "/dev/gpiochip0");
    ~AutoTrackService();

    void start();
    void stop();
    [[nodiscard]] bool isRunning() const noexcept { return running_.load(std::memory_order_acquire); }

private:
    void sensorThreadFunc();
    void obstacleThreadFunc();
    void gimbalThreadFunc();

    void handleLineState(bool leftActive, bool rightActive);
    void publishMotion(Motion motion, int speed, const char* source);
    void publishStop(const char* source);
    void onObjectDetected(const ObjectDetectedTopic& msg);

    static bool obstacleTriggered(gpiod::line::value value);

    static constexpr int kBaseSpeed = 10;
    static constexpr int kOuterSpinSpeed = 50;
    static constexpr int kSweepMin = 150;
    static constexpr int kSweepMax = 450;
    static constexpr int kSweepCenter = 307;
    static constexpr int kSweepStep = 2; 

    LocalDdsBus& bus_;
    GimbalService& gimbal_;
    const char* gpioChipPath_;

    std::atomic<bool> running_{false};
    std::atomic<bool> obstacleActive_{false};
    std::atomic<bool> isHandlingObject_{false};
    bool lastObstacleState_{false}; 

    std::chrono::steady_clock::time_point lastActionEndTime_{};
    static constexpr auto kCooldownDuration = std::chrono::seconds(5);
    static constexpr auto kStopWaitDuration = std::chrono::seconds(3);

    std::optional<gpiod::chip> chip_;
    std::optional<gpiod::line_request> sensorReq_;
    std::optional<gpiod::line_request> obstacleReq_;

    gpiod::line::offsets sensorOffsets_{13, 26};
    gpiod::line::offsets obstacleOffsets_{16, 12};

    LocalDdsBus::Subscription detectionSub_{};
    std::thread sensorThread_;
    std::thread obstacleThread_;
    std::thread gimbalThread_;
};