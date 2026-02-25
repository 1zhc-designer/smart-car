#include "RainSensor.hpp"

#include <wiringPi.h>
#include <pcf8591.h>

#include <iostream>

RainSensor::RainSensor(int pcfBase, int i2cAddr, int analogChannel, int doPin)
    : pcfBase_(pcfBase),
      i2cAddr_(i2cAddr),
      analogChannel_(analogChannel),
      doPin_(doPin)
{
    if (analogChannel_ < 0) analogChannel_ = 0;
    if (analogChannel_ > 3) analogChannel_ = 3;
}

int RainSensor::analogPinIndex() const
{
    return pcfBase_ + analogChannel_;
}

bool RainSensor::init()
{
    // Dependency: wiringPiSetup() must be called externally before this.
    // wiringPi does not provide an official API to check initialization status.
    // Here we assume minimal conditions and directly set up PCF8591.
    // If the system, permissions, or I2C are not ready,
    // the underlying layer typically reports errors or fails.
    try {
        pcf8591Setup(pcfBase_, i2cAddr_);
    } catch (...) {
        std::cerr << "RainSensor: pcf8591Setup failed (exception)\n";
        inited_ = false;
        return false;
    }

    pinMode(doPin_, INPUT);
    inited_ = true;
    return true;
}

std::optional<int> RainSensor::readAnalog()
{
    if (!inited_) return std::nullopt;
    const int v = analogRead(analogPinIndex());
    return v;
}

std::optional<int> RainSensor::readDigital()
{
    if (!inited_) return std::nullopt;
    const int v = digitalRead(doPin_);
    return v;
}

bool RainSensor::digitalMeansRaining(int digitalValue)
{
    // For common rain drop modules:
    // DO = 0 → triggered (wet / raining)
    // DO = 1 → not triggered (dry / no rain)
    return (digitalValue == 0);
}

std::optional<RainSensor::Reading> RainSensor::read()
{
    if (!inited_) return std::nullopt;

    const int a = analogRead(analogPinIndex());
    const int d = digitalRead(doPin_);

    Reading r{};
    r.analog = a;
    r.digital = d;
    r.raining = digitalMeansRaining(d);

    return r;
}