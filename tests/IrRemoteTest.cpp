#include <gtest/gtest.h>
#include "ir/IrRemote.hpp"
#include "rt/Scheduler.hpp"
#include "gimbal/GimbalService.hpp"
#include "motion/MotionController.hpp"
#include "MockMotorDriver.hpp"

TEST(IrRemoteTest, InitTest) {
    MockMotorDriver mock;
    MotionController mc(mock);
    Scheduler sched(mc);
    GimbalService gimbal;
    IrRemote remote(sched, gimbal);
    SUCCEED(); 
}