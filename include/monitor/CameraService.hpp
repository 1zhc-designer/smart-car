#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include <opencv2/opencv.hpp>

class CameraService {
public:
    CameraService(int cameraIndex = 0, const std::string& savePath = "./captures");
    ~CameraService();

    CameraService(const CameraService&) = delete;
    CameraService& operator=(const CameraService&) = delete;

    void start();
    void stop();

    bool isRunning() const noexcept;

private:
    void runLoop();
    void ensureDirectory(const std::string& path);

private:
    int cameraIndex_{0};
    std::string savePath_{"./captures"};

    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};

    std::thread worker_;
};