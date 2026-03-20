#pragma once

#include "dds/LocalDdsBus.hpp"
#include "dds/VehicleTopics.hpp"
#include "gimbal/GimbalCommandService.hpp"
#include "gimbal/GimbalService.hpp"
#include "ir/IrRemote.hpp"
#include "monitor/CameraService.hpp"
#include "monitor/MonitorService.hpp"
#include "motion/MotionController.hpp"
#include "motor/GpiodMotorDriver.hpp"
#include "rt/MotionCommandService.hpp"

#include <chrono>
#include <string>

#include <opencv2/opencv.hpp>

/**
 * @brief High-level façade used by the GUI and CLI.
 *
 * GUI buttons and IR input both publish DDS-style topics into the same bus.
 * Runtime services subscribe independently, which removes the need for a
 * global scheduler object.
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
    static const std::chrono::milliseconds kStopDuration;

    void publishMotion(Motion motion, int speed, std::chrono::milliseconds duration, const std::string& source);
    void publishGimbal(GimbalCommand command, const std::string& source);

    LocalDdsBus bus_{};
    GpiodMotorDriver driver_;
    MotionController motionController_;
    MotionCommandService motionService_;
    MonitorService monitor_;
    CameraService camera_;
    GimbalService gimbal_;
    GimbalCommandService gimbalService_;
    IrRemote irRemote_;
    bool started_{false};
};
