#include "gui/SystemFacade.hpp"

#include <iostream>

const std::chrono::milliseconds SystemFacade::kContinuous =
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(24));
const std::chrono::milliseconds SystemFacade::kStopDuration{10};

SystemFacade::SystemFacade()
    : driver_({.PWMA = 18, .AIN1 = 22, .AIN2 = 27,
               .PWMB = 23, .BIN1 = 25, .BIN2 = 24},
              100),
      motionController_(driver_),
      motionService_(bus_, motionController_),
      camera_(0, "./captures", false),
      gimbalService_(bus_, gimbal_),
      irRemote_(bus_),
      autoTrack_(bus_, gimbal_) {}

SystemFacade::~SystemFacade() {
    stop();
}

bool SystemFacade::start() {
    if (started_) {
        return true;
    }

    gimbal_.init();
    gimbalService_.start();
    motionService_.start();
    monitor_.start();
    camera_.start();
    started_ = true;

    mode_ = ControlMode::Manual;
    irRemote_.start();
    return true;
}

void SystemFacade::stop() {
    if (!started_) {
        return;
    }

    autoTrack_.stop();
    irRemote_.stop();
    camera_.stop();
    monitor_.stop();
    motionService_.stop();
    gimbalService_.stop();
    started_ = false;
}

void SystemFacade::setMode(ControlMode mode) {
    if (!started_) {
        mode_ = mode;
        return;
    }

    if (mode_ == mode) {
        return;
    }

    stopMotion();

    try {
        if (mode == ControlMode::Tracking) {
            irRemote_.stop();
            autoTrack_.start();
            mode_ = ControlMode::Tracking;
        } else {
            autoTrack_.stop();
            gimbal_.reset();
            irRemote_.start();
            mode_ = ControlMode::Manual;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to switch mode: " << e.what() << std::endl;

        autoTrack_.stop();
        try {
            gimbal_.reset();
        } catch (...) {
        }
        irRemote_.start();
        mode_ = ControlMode::Manual;
    }
}

void SystemFacade::publishMotion(Motion motion, int speed, std::chrono::milliseconds duration, const std::string& source) {
    bus_.publish(MotionCommandTopic{motion, speed, duration, source});
}

void SystemFacade::publishGimbal(GimbalCommand command, const std::string& source) {
    bus_.publish(GimbalCommandTopic{command, source});
}

void SystemFacade::moveForward() {
    if (mode_ != ControlMode::Manual) {
        return;
    }
    publishMotion(Motion::Up, kSpeedForward, kContinuous, "gui");
}

void SystemFacade::moveBackward() {
    if (mode_ != ControlMode::Manual) {
        return;
    }
    publishMotion(Motion::Down, kSpeedForward, kContinuous, "gui");
}

void SystemFacade::turnLeft() {
    if (mode_ != ControlMode::Manual) {
        return;
    }
    publishMotion(Motion::Left, kSpeedTurn, kContinuous, "gui");
}

void SystemFacade::turnRight() {
    if (mode_ != ControlMode::Manual) {
        return;
    }
    publishMotion(Motion::Right, kSpeedTurn, kContinuous, "gui");
}

void SystemFacade::stopMotion() {
    publishMotion(Motion::Stop, 0, kStopDuration, "gui");
}

void SystemFacade::gimbalUp() {
    if (mode_ != ControlMode::Manual) {
        return;
    }
    publishGimbal(GimbalCommand::TiltUp, "gui");
}

void SystemFacade::gimbalDown() {
    if (mode_ != ControlMode::Manual) {
        return;
    }
    publishGimbal(GimbalCommand::TiltDown, "gui");
}

void SystemFacade::gimbalLeft() {
    if (mode_ != ControlMode::Manual) {
        return;
    }
    publishGimbal(GimbalCommand::PanLeft, "gui");
}

void SystemFacade::gimbalRight() {
    if (mode_ != ControlMode::Manual) {
        return;
    }
    publishGimbal(GimbalCommand::PanRight, "gui");
}

void SystemFacade::gimbalReset() {
    if (mode_ != ControlMode::Manual) {
        return;
    }
    publishGimbal(GimbalCommand::Reset, "gui");
}

double SystemFacade::currentTemperature() const {
    return monitor_.currentTemperature();
}

std::string SystemFacade::currentStatus() const {
    return monitor_.currentStatus();
}

int SystemFacade::lowLimit() const {
    return monitor_.lowLimit();
}

int SystemFacade::highLimit() const {
    return monitor_.highLimit();
}

void SystemFacade::setTemperatureLimits(int low, int high) {
    monitor_.setLimits(low, high);
}

cv::Mat SystemFacade::latestFrame() const {
    return camera_.latestFrame();
}

void SystemFacade::setFrameCallback(CameraService::FrameCallback callback) {
    camera_.setFrameCallback(std::move(callback));
}