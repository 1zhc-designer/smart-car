#include <gtest/gtest.h>
#include "rt/Scheduler.hpp"
#include "MockMotorDriver.hpp"

TEST(SchedulerTest, ImmediatePreemptionTest) {
    MockMotorDriver mock;
    MotionController mc(mock);
    Scheduler sched(mc);

    sched.start();

    
    EXPECT_CALL(mock, setLeft(50, true)).Times(testing::AtLeast(1));
    EXPECT_CALL(mock, stopAll()).Times(testing::AtLeast(1));

    sched.replaceNow({Motion::Up, 50, std::chrono::hours(24)});
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    sched.replaceNow({Motion::Stop, 0, std::chrono::milliseconds(10)});
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sched.stop();
}