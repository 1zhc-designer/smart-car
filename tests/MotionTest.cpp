#include <gtest/gtest.h>
#include "tests/MockMotorDriver.hpp"
#include "motion/MotionController.hpp"

using ::testing::_;

TEST(MotionControllerTest, MoveUpCommand) {
    MockMotorDriver mock;
    MotionController controller(mock);

    // 预期：前进指令应同时调用左右电机，速度为 50，方向为 true
    EXPECT_CALL(mock, setLeft(50, true)).Times(1);
    EXPECT_CALL(mock, setRight(50, true)).Times(1);

    controller.apply(Motion::Up, 50);
}

TEST(MotionControllerTest, StopCommand) {
    MockMotorDriver mock;
    MotionController controller(mock);

    // 预期：停止指令应调用 stopAll
    EXPECT_CALL(mock, stopAll()).Times(1);

    controller.apply(Motion::Stop, 0);
}