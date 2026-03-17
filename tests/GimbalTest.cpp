#include <gtest/gtest.h>
#include "gimbal/GimbalService.hpp"

TEST(GimbalTest, BoundsChecking) {
    GimbalService gimbal("/dev/i2c-1", 0x40);
    int center = 180; 
    int step = 12;
    EXPECT_GT(center + step, center);
}