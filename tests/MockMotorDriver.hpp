#pragma once
#include "motor/IMotorDriver.hpp"
#include <vector>

class MockMotorDriver : public IMotorDriver {
public:
    struct CallLog {
        int leftSpeed;
        bool leftForward;
        int rightSpeed;
        bool rightForward;
        bool stopCalled;
    };

    void setLeft(int speed, bool forward) override {
        lastCall.leftSpeed = speed;
        lastCall.leftForward = forward;
    }

    void setRight(int speed, bool forward) override {
        lastCall.rightSpeed = speed;
        lastCall.rightForward = forward;
    }

    void stopAll() override {
        lastCall.stopCalled = true;
        lastCall.leftSpeed = 0;
        lastCall.rightSpeed = 0;
    }

    CallLog lastCall{0, false, 0, false, false};
};