#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include <opencv2/opencv.hpp>

class CameraService {
public:
    CameraService(int cameraIndex = 0,
                  const std::string& savePath = "./captures",
                  bool showPreviewWindow = true);
    ~CameraService();

    CameraService(const CameraService&) = delete;
    CameraService& operator=(const CameraService&) = delete;

    void start();
    void stop();

    bool isRunning() const noexcept;
    void setPreviewEnabled(bool enabled);
    bool previewEnabled() const noexcept;

    cv::Mat latestFrame() const;

private:
    void runLoop();
    void ensureDirectory(const std::string& path);

private:
    int cameraIndex_{0};
    std::string savePath_{"./captures"};

    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};

    std::thread worker_;

    mutable std::mutex frameMutex_;
    cv::Mat latestFrame_;

    std::atomic<bool> showPreviewWindow_{true};
};