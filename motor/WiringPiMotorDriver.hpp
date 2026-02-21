#pragma once
#include "motor/IMotorDriver.hpp"
#include <stdexcept>

class WiringPiMotorDriver final : public IMotorDriver {
public:
    struct Pins {
        int PWMA, AIN1, AIN2;
        int PWMB, BIN1, BIN2;
    };

    explicit WiringPiMotorDriver(Pins pins, int pwmRange = 100);
    ~WiringPiMotorDriver() override;

    void setLeft(int speed, bool forward) override;
    void setRight(int speed, bool forward) override;
    void stopAll() override;

    WiringPiMotorDriver(const WiringPiMotorDriver&) = delete;
    WiringPiMotorDriver& operator=(const WiringPiMotorDriver&) = delete;

private:
    Pins pins_;
    int pwmRange_;
    void setOneSide(int pwmPin, int in1, int in2, int speed, bool forward);
};