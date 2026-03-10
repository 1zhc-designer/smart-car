#pragma once

#include <mutex>

class GimbalService final {
public:
    GimbalService() = default;
    ~GimbalService() = default;

    GimbalService(const GimbalService&) = delete;
    GimbalService& operator=(const GimbalService&) = delete;

    void init();
    void reset();

    void tiltUp();
    void tiltDown();
    void panLeft();
    void panRight();

    int currentTilt() const;
    int currentPan() const;

private:
    void setPwmFreq(int hz);
    void setPwm(int channel, int pulse);

private:
    static constexpr int kPca9685Addr = 0x40;
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

    mutable std::mutex mutex_;
    int fd_{-1};
    int curTilt_{kTiltCenter};
    int curPan_{kPanCenter};
    bool initialized_{false};
};