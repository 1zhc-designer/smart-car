#include "motor/WiringPiMotorDriver.hpp"
#include <wiringPi.h>
#include <softPwm.h>
#include <algorithm>

static int clampSpeed(int s) { return std::clamp(s, 0, 100); }

WiringPiMotorDriver::WiringPiMotorDriver(Pins pins, int pwmRange)
    : pins_(pins), pwmRange_(pwmRange)
{
    if (wiringPiSetup() == -1) {
        throw std::runtime_error("wiringPiSetup failed");
    }

    pinMode(pins_.PWMA, OUTPUT);
    pinMode(pins_.AIN1, OUTPUT);
    pinMode(pins_.AIN2, OUTPUT);

    pinMode(pins_.PWMB, OUTPUT);
    pinMode(pins_.BIN1, OUTPUT);
    pinMode(pins_.BIN2, OUTPUT);

    if (softPwmCreate(pins_.PWMA, 0, pwmRange_) != 0)
        throw std::runtime_error("softPwmCreate PWMA failed");
    if (softPwmCreate(pins_.PWMB, 0, pwmRange_) != 0)
        throw std::runtime_error("softPwmCreate PWMB failed");

    stopAll();
}

WiringPiMotorDriver::~WiringPiMotorDriver() {
    // failsafe: exit => stop motors
    stopAll();
}

void WiringPiMotorDriver::setOneSide(int pwmPin, int in1, int in2, int speed, bool forward) {
    speed = clampSpeed(speed);
    digitalWrite(in1, forward ? HIGH : LOW);
    digitalWrite(in2, forward ? LOW : HIGH);
    softPwmWrite(pwmPin, speed);
}

void WiringPiMotorDriver::setLeft(int speed, bool forward) {
    setOneSide(pins_.PWMA, pins_.AIN1, pins_.AIN2, speed, forward);
}

void WiringPiMotorDriver::setRight(int speed, bool forward) {
    setOneSide(pins_.PWMB, pins_.BIN1, pins_.BIN2, speed, forward);
}

void WiringPiMotorDriver::stopAll() {
    digitalWrite(pins_.AIN1, LOW);
    digitalWrite(pins_.AIN2, LOW);
    softPwmWrite(pins_.PWMA, 0);

    digitalWrite(pins_.BIN1, LOW);
    digitalWrite(pins_.BIN2, LOW);
    softPwmWrite(pins_.PWMB, 0);
}