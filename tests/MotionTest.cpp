#include <gtest/gtest.h>
#include "tests/MockMotorDriver.hpp"
#include "motion/MotionController.hpp"

using ::testing::_;

TEST(MotionControllerTest, MoveUpCommand) {
    MockMotorDriver mock;
    MotionController controller(mock);

    EXPECT_CALL(mock, setLeft(50, true)).Times(1);
    EXPECT_CALL(mock, setRight(50, true)).Times(1);

    controller.apply(Motion::Up, 50);
}

TEST(MotionControllerTest, StopCommand) {
    MockMotorDriver mock;
    MotionController controller(mock);

    EXPECT_CALL(mock, stopAll()).Times(1);

    controller.apply(Motion::Stop, 0);
}