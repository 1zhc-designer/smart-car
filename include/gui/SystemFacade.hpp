#pragma once

#include <chrono>
#include <string>

#include <opencv2/opencv.hpp>

#include "gimbal/GimbalService.hpp"
#include "monitor/CameraService.hpp"
#include "monitor/MonitorService.hpp"
#include "motion/MotionController.hpp"
#include "motor/GpiodMotorDriver.hpp"
#include "rt/Scheduler.hpp"

/**
 * @brief High-level façade used by the GUI.
 */
class SystemFacade {
public:
    SystemFacade();
    ~SystemFacade();

    SystemFacade(const SystemFacade&) = delete;
    SystemFacade& operator=(const SystemFacade&) = delete;

    bool start();
    void stop();

    void moveForward();
    void moveBackward();
    void turnLeft();
    void turnRight();
    void stopMotion();

    void gimbalUp();
    void gimbalDown();
    void gimbalLeft();
    void gimbalRight();
    void gimbalReset();

    double currentTemperature() const;
    std::string currentStatus() const;
    int lowLimit() const;
    int highLimit() const;
    void setTemperatureLimits(int low, int high);

    cv::Mat latestFrame() const;

private:
    static constexpr int kSpeedForward = 50;
    static constexpr int kSpeedTurn = 70;
    static const std::chrono::milliseconds kContinuous;
    static const std::chrono::milliseconds kStopDur;

    GpiodMotorDriver driver_;
    MotionController motion_;
    Scheduler sched_;
    MonitorService monitor_;
    CameraService camera_;
    GimbalService gimbal_;
    bool started_{false};
};
