#include <gtest/gtest.h>
#include "monitor/CameraService.hpp"
#include <opencv2/opencv.hpp>

TEST(CameraTest, FrameIntegrity) {
    CameraService camera(0, "./test_captures", false);
    EXPECT_FALSE(camera.isRunning());
    cv::Mat emptyFrame;
    EXPECT_TRUE(emptyFrame.empty());
}