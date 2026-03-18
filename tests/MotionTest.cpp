#include <gtest/gtest.h>
#include "motion/MotionController.hpp"
#include "MockMotorDriver.hpp"

TEST(MotionTest, ForwardCommand) {
    MockMotorDriver mock;
    MotionController controller(mock);
    
    controller.apply(Motion::Up, 50);
    EXPECT_EQ(mock.lastLeftSpeed, 50);
    EXPECT_TRUE(mock.lastLeftForward);
    EXPECT_EQ(mock.lastRightSpeed, 50);
    EXPECT_TRUE(mock.lastRightForward);
}

TEST(MotionTest, StopCommand) {
    MockMotorDriver mock;
    MotionController controller(mock);
    controller.apply(Motion::Stop, 0);
    EXPECT_TRUE(mock.stopped);
}