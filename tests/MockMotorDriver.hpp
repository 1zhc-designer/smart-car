#pragma once
#include "motor/IMotorDriver.hpp"
#include <vector>

struct MotorCall {
    int speed;
    bool forward;
};

class MockMotorDriver : public IMotorDriver {
public:
    int lastLeftSpeed{0};
    bool lastLeftForward{true};
    int lastRightSpeed{0};
    bool lastRightForward{true};
    bool stopped{false};

    std::vector<MotorCall> calls;

    void setLeft(int speed, bool forward) override { 
        lastLeftSpeed = speed;
        lastLeftForward = forward;
        calls.push_back({speed, forward}); 
    }

    void setRight(int speed, bool forward) override { 
        lastRightSpeed = speed;
        lastRightForward = forward;
    }

    void stopAll() override { 
        stopped = true;
        lastLeftSpeed = 0;
        lastRightSpeed = 0;
        calls.push_back({0, false}); 
    }
};