#pragma once
#include "motor/IMotorDriver.hpp"

enum class Motion {
    Up, Down, Left, Right, Stop
};

class MotionController {
public:
    explicit MotionController(IMotorDriver& driver) : driver_(driver) {}

    void apply(Motion m, int speed);

private:
    IMotorDriver& driver_;
};