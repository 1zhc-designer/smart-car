#include <gtest/gtest.h>
#include "monitor/MonitorService.hpp"

TEST(MonitorTest, TemperatureLimits) {
    MonitorService monitor;
    monitor.setLimits(20, 30);
    
    EXPECT_EQ(monitor.lowLimit(), 20);
    EXPECT_EQ(monitor.highLimit(), 30);
    
    monitor.setLimits(40, 10);
    EXPECT_EQ(monitor.lowLimit(), 20); 
}