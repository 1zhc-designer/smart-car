#include "gimbal/GimbalService.hpp"

#include <vector>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace {
void writeBytes(int fd, std::initializer_list<std::uint8_t> bytes) {
    const std::vector<std::uint8_t> payload(bytes);
    if (::write(fd, payload.data(), payload.size()) != static_cast<ssize_t>(payload.size())) {
        throw std::runtime_error("I2C write failed");
    }
}
}

GimbalService::GimbalService(const std::string& i2cDevice, int address)
    : i2cDevice_(i2cDevice), address_(address) {}

GimbalService::~GimbalService() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

void GimbalService::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        return;
    }

    fd_ = ::open(i2cDevice_.c_str(), O_RDWR);
    if (fd_ < 0) {
        throw std::runtime_error("Failed to open I2C device for PCA9685");
    }
    if (::ioctl(fd_, I2C_SLAVE, address_) < 0) {
        throw std::runtime_error("Failed to select PCA9685 I2C address");
    }

    writeReg8(kMode1, 0x00);
    setPwmFreq(50);

    initialized_ = true;
    curTilt_ = kTiltCenter;
    curPan_ = kPanCenter;
    setPwm(kTiltChannel, curTilt_);
    setPwm(kPanChannel, curPan_);
}

void GimbalService::writeReg8(int reg, int value) {
    writeBytes(fd_, {static_cast<std::uint8_t>(reg), static_cast<std::uint8_t>(value & 0xFF)});
}

int GimbalService::readReg8(int reg) {
    std::uint8_t r = static_cast<std::uint8_t>(reg);
    if (::write(fd_, &r, 1) != 1) {
        throw std::runtime_error("I2C register select failed");
    }
    std::uint8_t value = 0;
    if (::read(fd_, &value, 1) != 1) {
        throw std::runtime_error("I2C register read failed");
    }
    return static_cast<int>(value);
}

void GimbalService::setPwmFreq(int hz) {
    const float prescaleVal = 25000000.0f / (4096.0f * static_cast<float>(hz)) - 1.0f;
    const int prescale = static_cast<int>(std::round(prescaleVal));

    const int oldMode = readReg8(kMode1);
    writeReg8(kMode1, (oldMode & 0x7F) | 0x10);
    writeReg8(kPrescale, prescale);
    writeReg8(kMode1, oldMode);
    ::usleep(5000);
    writeReg8(kMode1, oldMode | 0xA1);
}

void GimbalService::setPwm(int channel, int pulse) {
    writeBytes(fd_, {static_cast<std::uint8_t>(kLed0OnL + 4 * channel),
                     0x00,
                     0x00,
                     static_cast<std::uint8_t>(pulse & 0xFF),
                     static_cast<std::uint8_t>((pulse >> 8) & 0xFF)});
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

void GimbalService::setTiltPosition(int pulse) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        throw std::runtime_error("GimbalService not initialized");
    }
    curTilt_ = std::clamp(pulse, kServoMin, kServoMax);
    setPwm(kTiltChannel, curTilt_);
}

void GimbalService::setPanPosition(int pulse) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        throw std::runtime_error("GimbalService not initialized");
    }
    curPan_ = std::clamp(pulse, kServoMin, kServoMax);
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