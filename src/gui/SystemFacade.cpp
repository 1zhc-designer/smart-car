#include "gui/SystemFacade.hpp"

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
      irRemote_(bus_) {}

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
    irRemote_.start();
    started_ = true;
    return true;
}

void SystemFacade::stop() {
    if (!started_) {
        return;
    }

    irRemote_.stop();
    camera_.stop();
    monitor_.stop();
    motionService_.stop();
    gimbalService_.stop();
    started_ = false;
}

void SystemFacade::publishMotion(Motion motion, int speed, std::chrono::milliseconds duration, const std::string& source) {
    bus_.publish(MotionCommandTopic{motion, speed, duration, source});
}

void SystemFacade::publishGimbal(GimbalCommand command, const std::string& source) {
    bus_.publish(GimbalCommandTopic{command, source});
}

void SystemFacade::moveForward() {
    publishMotion(Motion::Up, kSpeedForward, kContinuous, "gui");
}

void SystemFacade::moveBackward() {
    publishMotion(Motion::Down, kSpeedForward, kContinuous, "gui");
}

void SystemFacade::turnLeft() {
    publishMotion(Motion::Left, kSpeedTurn, kContinuous, "gui");
}

void SystemFacade::turnRight() {
    publishMotion(Motion::Right, kSpeedTurn, kContinuous, "gui");
}

void SystemFacade::stopMotion() {
    publishMotion(Motion::Stop, 0, kStopDuration, "gui");
}

void SystemFacade::gimbalUp() {
    publishGimbal(GimbalCommand::TiltUp, "gui");
}

void SystemFacade::gimbalDown() {
    publishGimbal(GimbalCommand::TiltDown, "gui");
}

void SystemFacade::gimbalLeft() {
    publishGimbal(GimbalCommand::PanLeft, "gui");
}

void SystemFacade::gimbalRight() {
    publishGimbal(GimbalCommand::PanRight, "gui");
}

void SystemFacade::gimbalReset() {
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
