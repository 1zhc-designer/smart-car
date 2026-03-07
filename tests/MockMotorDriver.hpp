#pragma once
#include <gmock/gmock.h>
#include "motor/IMotorDriver.hpp"

class MockMotorDriver : public IMotorDriver {
public:
    MOCK_METHOD(void, setLeft, (int speed, bool forward), (override));
    MOCK_METHOD(void, setRight, (int speed, bool forward), (override));
    MOCK_METHOD(void, stopAll, (), (override));
};