#pragma once

#include <mutex>
#include <string>

/**
 * @brief Pan/tilt servo controller for a PCA9685 over Linux i2c-dev.
 */
class GimbalService final {
public:
    explicit GimbalService(const std::string& i2cDevice = "/dev/i2c-1", int address = 0x40);
    ~GimbalService();

    GimbalService(const GimbalService&) = delete;
    GimbalService& operator=(const GimbalService&) = delete;

    void init();
    void reset();
    void tiltUp();
    void tiltDown();
    void panLeft();
    void panRight();
    void setTiltPosition(int pulse);
    void setPanPosition(int pulse);

    int currentTilt() const;
    int currentPan() const;

private:
    void setPwmFreq(int hz);
    void setPwm(int channel, int pulse);
    void writeReg8(int reg, int value);
    int readReg8(int reg);

    static constexpr int kMode1 = 0x00;
    static constexpr int kPrescale = 0xFE;
    static constexpr int kLed0OnL = 0x06;
    static constexpr int kServoMin = 102;
    static constexpr int kServoMax = 512;
    static constexpr int kStep = 12;
    static constexpr int kTiltCenter = 180;
    static constexpr int kPanCenter = 307;
    static constexpr int kTiltChannel = 0;
    static constexpr int kPanChannel = 1;

    std::string i2cDevice_;
    int address_{0x40};
    mutable std::mutex mutex_;
    int fd_{-1};
    int curTilt_{kTiltCenter};
    int curPan_{kPanCenter};
    bool initialized_{false};
};