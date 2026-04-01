#include "monitor/CameraService.hpp"

#include <sys/stat.h>

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kFrameWidth = 640;
constexpr int kFrameHeight = 480;
constexpr int kFrameFps = 30;

constexpr double kMinFruitContourArea = 2500.0;
constexpr double kMinLeafContourArea = 1800.0;

constexpr float kMinAspectRatio = 0.4F;
constexpr float kMaxAspectRatio = 2.5F;

constexpr int kMinLeafWidth = 20;
constexpr int kMinLeafHeight = 20;

constexpr int kSaveIntervalSeconds = 2;

const char* kWindowName = "Improved Monitor";

struct FrameDetection {
    cv::Rect rect{};
    int targetX{0};
    int targetY{0};
};

struct LeafColorStats {
    double yellowRatio{0.0};
    double whiteRatio{0.0};
    double blackRatio{0.0};
    double brownRatio{0.0};
    double abnormalRatio{0.0};
    LeafHealthStatus status{LeafHealthStatus::Normal};
};

const char* toStatusText(const LeafHealthStatus status) {
    switch (status) {
        case LeafHealthStatus::Normal:
            return "Normal";
        case LeafHealthStatus::Suspicious:
            return "Suspicious";
        case LeafHealthStatus::Abnormal:
            return "Abnormal";
        default:
            return "Unknown";
    }
}

cv::Scalar statusColor(const LeafHealthStatus status) {
    switch (status) {
        case LeafHealthStatus::Normal:
            return cv::Scalar(0, 255, 0);
        case LeafHealthStatus::Suspicious:
            return cv::Scalar(0, 255, 255);
        case LeafHealthStatus::Abnormal:
            return cv::Scalar(0, 0, 255);
        default:
            return cv::Scalar(255, 255, 255);
    }
}

void configureCamera(cv::VideoCapture& capture) {
    capture.set(cv::CAP_PROP_FRAME_WIDTH, kFrameWidth);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, kFrameHeight);
    capture.set(cv::CAP_PROP_FPS, kFrameFps);
}

void preprocessFrame(const cv::Mat& frame, cv::Mat& hsv) {
    cv::Mat filteredFrame;
    cv::medianBlur(frame, filteredFrame, 5);
    cv::cvtColor(filteredFrame, hsv, cv::COLOR_BGR2HSV);
}

cv::Mat buildFruitMask(const cv::Mat& hsv) {
    cv::Mat lowerRedMask;
    cv::Mat upperRedMask;
    cv::Mat fruitMask;

    cv::inRange(hsv, cv::Scalar(0, 160, 100), cv::Scalar(10, 255, 255), lowerRedMask);
    cv::inRange(hsv, cv::Scalar(170, 160, 100), cv::Scalar(180, 255, 255), upperRedMask);

    fruitMask = lowerRedMask | upperRedMask;

    const cv::Mat kernel =
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));

    cv::morphologyEx(fruitMask, fruitMask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(fruitMask, fruitMask, cv::MORPH_CLOSE, kernel);

    return fruitMask;
}

cv::Mat buildLeafMask(const cv::Mat& hsv) {
    cv::Mat leafMask;

    cv::inRange(hsv, cv::Scalar(25, 35, 35), cv::Scalar(95, 255, 255), leafMask);

    const cv::Mat kernel =
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));

    cv::morphologyEx(leafMask, leafMask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(leafMask, leafMask, cv::MORPH_CLOSE, kernel);

    return leafMask;
}

bool isValidFruitContour(const std::vector<cv::Point>& contour) {
    const double area = cv::contourArea(contour);
    if (area < kMinFruitContourArea) {
        return false;
    }

    const cv::Rect rect = cv::boundingRect(contour);
    if (rect.height == 0) {
        return false;
    }

    const float aspectRatio =
        static_cast<float>(rect.width) / static_cast<float>(rect.height);

    return aspectRatio >= kMinAspectRatio && aspectRatio <= kMaxAspectRatio;
}

bool isValidLeafContour(const std::vector<cv::Point>& contour) {
    const double area = cv::contourArea(contour);
    if (area < kMinLeafContourArea) {
        return false;
    }

    const cv::Rect rect = cv::boundingRect(contour);
    return rect.width >= kMinLeafWidth && rect.height >= kMinLeafHeight;
}

std::vector<FrameDetection> detectFruits(const cv::Mat& fruitMask) {
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(fruitMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<FrameDetection> detections;
    detections.reserve(contours.size());

    for (const auto& contour : contours) {
        if (!isValidFruitContour(contour)) {
            continue;
        }

        const cv::Rect rect = cv::boundingRect(contour);

        detections.push_back(FrameDetection{
            rect,
            rect.x + rect.width / 2,
            rect.y + rect.height / 2
        });
    }

    std::sort(detections.begin(),
              detections.end(),
              [](const FrameDetection& left, const FrameDetection& right) {
                  if (left.rect.y != right.rect.y) {
                      return left.rect.y < right.rect.y;
                  }
                  return left.rect.x < right.rect.x;
              });

    return detections;
}

LeafColorStats analyzeLeafColors(const cv::Mat& hsv,
                                 const cv::Mat& singleLeafMask,
                                 const cv::Rect& bounds) {
    LeafColorStats stats{};

    const cv::Mat hsvRegion = hsv(bounds);
    const cv::Mat maskRegion = singleLeafMask(bounds);

    int totalLeafPixels = 0;
    int yellowPixels = 0;
    int whitePixels = 0;
    int blackPixels = 0;
    int brownPixels = 0;

    for (int y = 0; y < hsvRegion.rows; ++y) {
        for (int x = 0; x < hsvRegion.cols; ++x) {
            if (maskRegion.at<uchar>(y, x) == 0) {
                continue;
            }

            ++totalLeafPixels;

            const cv::Vec3b hsvPixel = hsvRegion.at<cv::Vec3b>(y, x);
            const int hue = hsvPixel[0];
            const int saturation = hsvPixel[1];
            const int value = hsvPixel[2];

            if (value < 55) {
                ++blackPixels;
                continue;
            }

            if (saturation < 35 && value > 160) {
                ++whitePixels;
                continue;
            }

            if (hue >= 15 && hue <= 40 && saturation >= 45 && value >= 80) {
                ++yellowPixels;
                continue;
            }

            if (hue >= 5 && hue <= 22 && saturation >= 50 && value >= 40 && value <= 180) {
                ++brownPixels;
            }
        }
    }

    if (totalLeafPixels == 0) {
        return stats;
    }

    stats.yellowRatio = static_cast<double>(yellowPixels) / totalLeafPixels;
    stats.whiteRatio = static_cast<double>(whitePixels) / totalLeafPixels;
    stats.blackRatio = static_cast<double>(blackPixels) / totalLeafPixels;
    stats.brownRatio = static_cast<double>(brownPixels) / totalLeafPixels;
    stats.abnormalRatio = stats.yellowRatio + stats.whiteRatio +
                          stats.blackRatio + stats.brownRatio;

    if (stats.abnormalRatio >= 0.20 || stats.blackRatio >= 0.08 || stats.brownRatio >= 0.10) {
        stats.status = LeafHealthStatus::Abnormal;
    } else if (stats.abnormalRatio >= 0.08 ||
               stats.yellowRatio >= 0.05 ||
               stats.whiteRatio >= 0.05) {
        stats.status = LeafHealthStatus::Suspicious;
    } else {
        stats.status = LeafHealthStatus::Normal;
    }

    return stats;
}

std::vector<LeafTarget> detectLeaves(const cv::Mat& hsv, const cv::Mat& leafMask) {
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(leafMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<LeafTarget> leaves;
    leaves.reserve(contours.size());

    for (const auto& contour : contours) {
        if (!isValidLeafContour(contour)) {
            continue;
        }

        const cv::Rect bounds = cv::boundingRect(contour);

        cv::Mat singleLeafMask = cv::Mat::zeros(leafMask.size(), CV_8UC1);
        std::vector<std::vector<cv::Point>> singleContour{contour};
        cv::drawContours(singleLeafMask, singleContour, 0, cv::Scalar(255), cv::FILLED);

        const LeafColorStats stats = analyzeLeafColors(hsv, singleLeafMask, bounds);

        leaves.push_back(LeafTarget{
            bounds,
            bounds.x + bounds.width / 2,
            bounds.y + bounds.height / 2,
            stats.yellowRatio,
            stats.whiteRatio,
            stats.blackRatio,
            stats.brownRatio,
            stats.abnormalRatio,
            stats.status
        });
    }

    std::sort(leaves.begin(),
              leaves.end(),
              [](const LeafTarget& left, const LeafTarget& right) {
                  if (left.bounds.y != right.bounds.y) {
                      return left.bounds.y < right.bounds.y;
                  }
                  return left.bounds.x < right.bounds.x;
              });

    return leaves;
}

void drawFruitDetections(cv::Mat& frame, const std::vector<FrameDetection>& fruits) {
    for (std::size_t index = 0; index < fruits.size(); ++index) {
        const auto& fruit = fruits[index];

        cv::rectangle(frame, fruit.rect, cv::Scalar(0, 255, 0), 2);

        const std::string label =
            "Fruit " + std::to_string(index + 1) +
            " (" + std::to_string(fruit.targetX) +
            "," + std::to_string(fruit.targetY) + ")";

        cv::putText(frame,
                    label,
                    cv::Point(fruit.rect.x, std::max(20, fruit.rect.y - 10)),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.5,
                    cv::Scalar(0, 255, 0),
                    2);
    }
}

void drawLeafDetections(cv::Mat& frame, const std::vector<LeafTarget>& leaves) {
    for (std::size_t index = 0; index < leaves.size(); ++index) {
        const auto& leaf = leaves[index];
        const cv::Scalar color = statusColor(leaf.status);

        cv::rectangle(frame, leaf.bounds, color, 2);

        const std::string label =
            "Leaf " + std::to_string(index + 1) + " " +
            toStatusText(leaf.status) + " A=" +
            std::to_string(static_cast<int>(leaf.abnormalRatio * 100.0)) + "%";

        cv::putText(frame,
                    label,
                    cv::Point(leaf.bounds.x, std::max(20, leaf.bounds.y - 10)),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.5,
                    color,
                    2);
    }
}

bool hasAbnormalLeaf(const std::vector<LeafTarget>& leaves) {
    return std::any_of(leaves.begin(),
                       leaves.end(),
                       [](const LeafTarget& leaf) {
                           return leaf.status != LeafHealthStatus::Normal;
                       });
}

bool shouldSaveNow(const std::chrono::steady_clock::time_point now,
                   const std::chrono::steady_clock::time_point lastSaveTime) {
    const auto elapsedSeconds =
        std::chrono::duration_cast<std::chrono::seconds>(now - lastSaveTime).count();
    return elapsedSeconds >= kSaveIntervalSeconds;
}

std::string buildFileName(const std::string& savePath,
                          const std::vector<FrameDetection>& fruits,
                          const std::vector<LeafTarget>& leaves) {
    std::size_t abnormalLeafCount = 0;
    for (const auto& leaf : leaves) {
        if (leaf.status != LeafHealthStatus::Normal) {
            ++abnormalLeafCount;
        }
    }

    if (abnormalLeafCount > 0U) {
        return savePath + "/LeafAlert_" + std::to_string(abnormalLeafCount) + ".jpg";
    }

    if (fruits.size() > 1U) {
        return savePath + "/Fruits_" + std::to_string(fruits.size()) + ".jpg";
    }

    if (fruits.size() == 1U) {
        return savePath + "/Fruit_X" + std::to_string(fruits.front().targetX) +
               "_Y" + std::to_string(fruits.front().targetY) + ".jpg";
    }

    return savePath + "/Capture.jpg";
}

CameraDetections buildPublishedDetections(const std::vector<FrameDetection>& fruits,
                                          const std::vector<LeafTarget>& leaves) {
    CameraDetections detections{};
    detections.leaves = leaves;
    detections.fruits.reserve(fruits.size());

    for (const auto& fruit : fruits) {
        detections.fruits.push_back(FruitTarget{
            fruit.rect,
            fruit.targetX,
            fruit.targetY
        });
    }

    return detections;
}

}  // namespace

CameraService::CameraService(const int cameraIndex,
                             const std::string& savePath,
                             const bool showPreviewWindow)
    : cameraIndex_(cameraIndex),
      savePath_(savePath),
      showPreviewWindow_(showPreviewWindow) {
}

CameraService::~CameraService() {
    stop();
}

bool CameraService::isRunning() const noexcept {
    return running_.load(std::memory_order_acquire);
}

void CameraService::setPreviewEnabled(const bool enabled) {
    showPreviewWindow_.store(enabled, std::memory_order_release);
}

bool CameraService::previewEnabled() const noexcept {
    return showPreviewWindow_.load(std::memory_order_acquire);
}

cv::Mat CameraService::latestFrame() const {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return latestFrame_.clone();
}

void CameraService::setDetectionCallback(DetectionCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    detectionCallback_ = std::move(callback);
}

void CameraService::setFrameCallback(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    frameCallback_ = std::move(callback);
}

std::optional<CameraDetections> CameraService::latestDetections() const {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    return latestDetections_;
}

void CameraService::ensureDirectoryExists(const std::string& path) const {
    struct stat info {};
    if (::stat(path.c_str(), &info) != 0) {
        ::mkdir(path.c_str(), 0777);
    }
}

void CameraService::updateLatestFrame(const cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(frameMutex_);
    latestFrame_ = frame.clone();
}

void CameraService::publishDetections(const CameraDetections& detections) {
    DetectionCallback callbackCopy;

    {
        std::lock_guard<std::mutex> lock(detectionMutex_);
        latestDetections_ = detections;
    }

    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callbackCopy = detectionCallback_;
    }

    if (callbackCopy) {
        callbackCopy(detections);
    }
}

void CameraService::clearLatestDetections() {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    latestDetections_.reset();
}

void CameraService::notifyFrameReady() {
    FrameCallback callbackCopy;

    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callbackCopy = frameCallback_;
    }

    if (callbackCopy) {
        callbackCopy();
    }
}

void CameraService::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    stopRequested_.store(false, std::memory_order_release);

    try {
        worker_ = std::thread([this]() {
            try {
                runLoop();
            } catch (...) {
            }
            running_.store(false, std::memory_order_release);
        });
    } catch (...) {
        running_.store(false, std::memory_order_release);
        throw;
    }
}

void CameraService::stop() {
    stopRequested_.store(true, std::memory_order_release);

    if (worker_.joinable()) {
        worker_.join();
    }
}

void CameraService::runLoop() {
    cv::VideoCapture capture(cameraIndex_);
    if (!capture.isOpened()) {
        throw std::runtime_error("Cannot open camera");
    }

    configureCamera(capture);
    ensureDirectoryExists(savePath_);

    cv::Mat frame;
    cv::Mat hsv;

    auto lastSaveTime = std::chrono::steady_clock::now();

    if (previewEnabled()) {
        cv::namedWindow(kWindowName, cv::WINDOW_AUTOSIZE);
    }

    while (!stopRequested_.load(std::memory_order_acquire)) {
        if (!capture.read(frame) || frame.empty()) {
            break;
        }

        preprocessFrame(frame, hsv);

        const cv::Mat fruitMask = buildFruitMask(hsv);
        const cv::Mat leafMask = buildLeafMask(hsv);

        const std::vector<FrameDetection> fruitDetections = detectFruits(fruitMask);
        const std::vector<LeafTarget> leafDetections = detectLeaves(hsv, leafMask);

        drawFruitDetections(frame, fruitDetections);
        drawLeafDetections(frame, leafDetections);

        CameraDetections detections = buildPublishedDetections(fruitDetections, leafDetections);

        const bool abnormalLeafDetected = hasAbnormalLeaf(leafDetections);
        const bool shouldSaveImage = !fruitDetections.empty() || abnormalLeafDetected;
        const bool hasAnyDetection = !detections.fruits.empty() || !detections.leaves.empty();

        const auto now = std::chrono::steady_clock::now();
        if (shouldSaveImage && shouldSaveNow(now, lastSaveTime)) {
            const std::string imagePath =
                buildFileName(savePath_, fruitDetections, leafDetections);

            if (cv::imwrite(imagePath, frame)) {
                detections.savedImagePath = imagePath;
                lastSaveTime = now;
            }
        }

        if (hasAnyDetection) {
            publishDetections(detections);
        } else {
            clearLatestDetections();
        }

        updateLatestFrame(frame);
        notifyFrameReady();

        if (previewEnabled()) {
            cv::imshow(kWindowName, frame);
            if (cv::waitKey(1) == 27) {
                break;
            }
        } else {
            cv::waitKey(1);
        }
    }

    capture.release();

    if (previewEnabled()) {
        cv::destroyWindow(kWindowName);
    }

    clearLatestDetections();
}