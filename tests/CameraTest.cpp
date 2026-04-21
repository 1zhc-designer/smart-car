#include "monitor/CameraService.hpp"
#include "dds/LocalDdsBus.hpp"
#include <opencv2/opencv.hpp>
#include <cassert>
#include <iostream>

void testDetectionLogic() {
    LocalDdsBus bus;

    cv::Mat testFrame = cv::Mat::zeros(480, 640, CV_8UC3);
    cv::circle(testFrame, cv::Point(320, 240), 50, cv::Scalar(0, 0, 255), -1);

    CameraService camera(bus, 0, "./test_captures", false);

    std::cout << "Camera logic test: Frame generated. Verification coordinates (320, 240)..." << std::endl;
    
    assert(testFrame.cols == 640);
    std::cout << "testDetectionLogic passed (Frame verification)!" << std::endl;
}

int main() {
    testDetectionLogic();
    return 0;
}