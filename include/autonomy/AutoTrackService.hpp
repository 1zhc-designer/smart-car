#pragma once

#include "dds/LocalDdsBus.hpp"
#include "dds/VehicleTopics.hpp"
#include "gimbal/GimbalService.hpp"

#include <atomic>
#include <optional>
#include <thread>

#include <gpiod.hpp>

/**
 * @brief Autonomous line-tracking and obstacle-avoidance service.
 *
 * Buzzer is intentionally not used here so that the temperature/monitor module
 * can keep exclusive ownership of the buzzer GPIO.
 */
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
    void sensorThreadFunc();
    void obstacleThreadFunc();
    void gimbalThreadFunc();

    void handleLineState(bool leftActive, bool rightActive);
    void publishMotion(Motion motion, int speed, const char* source);
    void publishStop(const char* source);

    static bool obstacleTriggered(gpiod::line::value value);

    static constexpr int kBaseSpeed = 10;
    static constexpr int kOuterSpinSpeed = 50;
    static constexpr int kSweepMin = 150;
    static constexpr int kSweepMax = 450;
    static constexpr int kSweepCenter = 307;
    static constexpr int kSweepStep = 5;

    static constexpr unsigned kSensorLeftPin = 13;
    static constexpr unsigned kSensorRightPin = 26;
    static constexpr unsigned kObstacleLeftPin = 16;
    static constexpr unsigned kObstacleRightPin = 12;

    LocalDdsBus& bus_;
    GimbalService& gimbal_;
    const char* gpioChipPath_;

    std::atomic<bool> running_{false};
    std::atomic<bool> obstacleActive_{false};

    std::optional<gpiod::chip> chip_;
    std::optional<gpiod::line_request> sensorReq_;
    std::optional<gpiod::line_request> obstacleReq_;

    gpiod::line::offsets sensorOffsets_{kSensorLeftPin, kSensorRightPin};
    gpiod::line::offsets obstacleOffsets_{kObstacleLeftPin, kObstacleRightPin};

    std::thread sensorThread_;
    std::thread obstacleThread_;
    std::thread gimbalThread_;
};