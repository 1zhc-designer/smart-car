#include <gtest/gtest.h>
#include "rt/Scheduler.hpp"
#include "motion/MotionController.hpp"
#include "MockMotorDriver.hpp"
#include <chrono>
#include <thread>

TEST(SchedulerTest, TaskExecution) {
    MockMotorDriver mock;
    MotionController mc(mock);
    Scheduler sched(mc);

    sched.start();
    
    sched.enqueue({Motion::Up, 80, std::chrono::milliseconds(100)});

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    ASSERT_GT(mock.calls.size(), 0);
    EXPECT_EQ(mock.calls[0].speed, 80);
    
    sched.stop();
}