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
    // 依赖：外部应先 wiringPiSetup()
    // wiringPi 没有直接“是否已初始化”的官方 API，这里采用最小假设：
    // 直接 setup PCF8591；若系统/权限/I2C 未就绪，通常会在底层打印/失败。
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
    // 按常见雨滴模块：DO=0 触发（潮湿/有雨），DO=1 未触发（干燥/无雨）
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