#pragma once
#include <cstdint>

class IMotorDriver {
public:
    virtual ~IMotorDriver() = default;

    // speed: 0..100
    virtual void setLeft(int speed, bool forward) = 0;
    virtual void setRight(int speed, bool forward) = 0;
    virtual void stopAll() = 0;
};