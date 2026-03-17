#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <opencv2/opencv.hpp>

struct FruitTarget {
    cv::Rect bounds{};
    int targetX{0};
    int targetY{0};
};

enum class LeafHealthStatus {
    Normal,
    Suspicious,
    Abnormal
};

struct LeafTarget {
    cv::Rect bounds{};
    int centerX{0};
    int centerY{0};

    double yellowRatio{0.0};
    double whiteRatio{0.0};
    double blackRatio{0.0};
    double brownRatio{0.0};
    double abnormalRatio{0.0};

    LeafHealthStatus status{LeafHealthStatus::Normal};
};

struct CameraDetections {
    std::vector<FruitTarget> fruits{};
    std::vector<LeafTarget> leaves{};
    std::string savedImagePath{};
};

class CameraService {
public:
    using DetectionCallback = std::function<void(const CameraDetections&)>;

    explicit CameraService(int cameraIndex = 0,
                           const std::string& savePath = "./captures",
                           bool showPreviewWindow = true);
    ~CameraService();

    CameraService(const CameraService&) = delete;
    CameraService& operator=(const CameraService&) = delete;
    CameraService(CameraService&&) = delete;
    CameraService& operator=(CameraService&&) = delete;

    void start();
    void stop();

    [[nodiscard]] bool isRunning() const noexcept;

    void setPreviewEnabled(bool enabled);
    [[nodiscard]] bool previewEnabled() const noexcept;

    [[nodiscard]] cv::Mat latestFrame() const;
    void setDetectionCallback(DetectionCallback callback);
    [[nodiscard]] std::optional<CameraDetections> latestDetections() const;

private:
    void runLoop();

    void ensureDirectoryExists(const std::string& path) const;
    void updateLatestFrame(const cv::Mat& frame);
    void publishDetections(const CameraDetections& detections);
    void clearLatestDetections();

private:
    int cameraIndex_{0};
    std::string savePath_{"./captures"};

    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> showPreviewWindow_{true};

    mutable std::mutex frameMutex_;
    cv::Mat latestFrame_{};

    mutable std::mutex detectionMutex_;
    std::optional<CameraDetections> latestDetections_{};

    mutable std::mutex callbackMutex_;
    DetectionCallback detectionCallback_{};

    std::thread worker_{};
};