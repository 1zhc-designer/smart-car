#include "gimbal/GimbalService.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <wiringPi.h>
#include <wiringPiI2C.h>

void GimbalService::init() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return;
    }

    fd_ = wiringPiI2CSetup(kPca9685Addr);
    if (fd_ < 0) {
        throw std::runtime_error("wiringPiI2CSetup failed for PCA9685");
    }

    wiringPiI2CWriteReg8(fd_, kMode1, 0x00);
    setPwmFreq(50);

    initialized_ = true;
    curTilt_ = kTiltCenter;
    curPan_ = kPanCenter;
    setPwm(kTiltChannel, curTilt_);
    setPwm(kPanChannel, curPan_);
}

void GimbalService::setPwmFreq(int hz) {
    const float prescaleVal = 25000000.0f / (4096.0f * static_cast<float>(hz)) - 1.0f;
    const int prescale = static_cast<int>(std::round(prescaleVal));

    const int oldMode = wiringPiI2CReadReg8(fd_, kMode1);
    wiringPiI2CWriteReg8(fd_, kMode1, (oldMode & 0x7F) | 0x10);
    wiringPiI2CWriteReg8(fd_, kPrescale, prescale);
    wiringPiI2CWriteReg8(fd_, kMode1, oldMode);
    delay(5);
    wiringPiI2CWriteReg8(fd_, kMode1, oldMode | 0xA1);
}

void GimbalService::setPwm(int channel, int pulse) {
    wiringPiI2CWriteReg8(fd_, kLed0OnL + 4 * channel, 0 & 0xFF);
    wiringPiI2CWriteReg8(fd_, kLed0OnL + 4 * channel + 1, 0 >> 8);
    wiringPiI2CWriteReg8(fd_, kLed0OnL + 4 * channel + 2, pulse & 0xFF);
    wiringPiI2CWriteReg8(fd_, kLed0OnL + 4 * channel + 3, pulse >> 8);
}

void GimbalService::reset() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        throw std::runtime_error("GimbalService not initialized");
    }

    curTilt_ = kTiltCenter;
    curPan_ = kPanCenter;
    setPwm(kTiltChannel, curTilt_);
    setPwm(kPanChannel, curPan_);
}

void GimbalService::tiltUp() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        throw std::runtime_error("GimbalService not initialized");
    }

    curTilt_ = std::max(kServoMin, curTilt_ - kStep);
    setPwm(kTiltChannel, curTilt_);
}

void GimbalService::tiltDown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        throw std::runtime_error("GimbalService not initialized");
    }

    curTilt_ = std::min(kServoMax, curTilt_ + kStep);
    setPwm(kTiltChannel, curTilt_);
}

void GimbalService::panLeft() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        throw std::runtime_error("GimbalService not initialized");
    }

    curPan_ = std::min(kServoMax, curPan_ + kStep);
    setPwm(kPanChannel, curPan_);
}

void GimbalService::panRight() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        throw std::runtime_error("GimbalService not initialized");
    }

    curPan_ = std::max(kServoMin, curPan_ - kStep);
    setPwm(kPanChannel, curPan_);
}

int GimbalService::currentTilt() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return curTilt_;
}

int GimbalService::currentPan() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return curPan_;
}