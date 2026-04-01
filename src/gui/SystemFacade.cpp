#include "gui/SystemFacade.hpp"

const std::chrono::milliseconds SystemFacade::kContinuous =
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(24));

const std::chrono::milliseconds SystemFacade::kStopDur{10};

SystemFacade::SystemFacade()
    : driver_({.PWMA = 18, .AIN1 = 22, .AIN2 = 27,
               .PWMB = 23, .BIN1 = 25, .BIN2 = 24}, 100),
      motion_(driver_),
      sched_(motion_),
      camera_(0, "./captures", false) {}

SystemFacade::~SystemFacade() {
    stop();
}

bool SystemFacade::start() {
    if (started_) {
        return true;
    }

    gimbal_.init();
    sched_.start();
    monitor_.start();
    camera_.start();

    started_ = true;
    return true;
}

void SystemFacade::stop() {
    if (!started_) {
        return;
    }

    camera_.stop();
    monitor_.stop();
    sched_.stop();
    started_ = false;
}

void SystemFacade::moveForward() {
    sched_.replaceNow({Motion::Up, kSpeedForward, kContinuous});
}

void SystemFacade::moveBackward() {
    sched_.replaceNow({Motion::Down, kSpeedForward, kContinuous});
}

void SystemFacade::turnLeft() {
    sched_.replaceNow({Motion::Left, kSpeedTurn, kContinuous});
}

void SystemFacade::turnRight() {
    sched_.replaceNow({Motion::Right, kSpeedTurn, kContinuous});
}

void SystemFacade::stopMotion() {
    sched_.replaceNow({Motion::Stop, 0, kStopDur});
}

void SystemFacade::gimbalUp() {
    gimbal_.tiltUp();
}

void SystemFacade::gimbalDown() {
    gimbal_.tiltDown();
}

void SystemFacade::gimbalLeft() {
    gimbal_.panLeft();
}

void SystemFacade::gimbalRight() {
    gimbal_.panRight();
}

void SystemFacade::gimbalReset() {
    gimbal_.reset();
}

double SystemFacade::currentTemperature() const {
    return monitor_.currentTemperature();
}

int SystemFacade::currentLightLevel() const {
    return monitor_.currentLightLevel();
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
