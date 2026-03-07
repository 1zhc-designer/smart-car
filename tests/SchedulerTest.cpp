#include <gtest/gtest.h>
#include "rt/Scheduler.hpp"
#include "MockMotorDriver.hpp"

TEST(SchedulerTest, ImmediatePreemptionTest) {
    MockMotorDriver mock;
    MotionController mc(mock);
    Scheduler sched(mc);

    sched.start();

    // 预期逻辑：
    // 1. 发送一个长时间任务（24小时前进）
    // 2. 立即发送一个停止任务
    // 验证：stopAll 必须在极短时间内被调用，而不是等待 24 小时 
    
    EXPECT_CALL(mock, setLeft(50, true)).Times(testing::AtLeast(1));
    EXPECT_CALL(mock, stopAll()).Times(testing::AtLeast(1));

    sched.replaceNow({Motion::Up, 50, std::chrono::hours(24)});
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // 触发抢占 
    sched.replaceNow({Motion::Stop, 0, std::chrono::milliseconds(10)});
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sched.stop();
}