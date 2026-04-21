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
constexpr int kLeafDetectEveryNFrames = 3;
constexpr std::size_t kMaxSaveQueueDepth = 8;

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
        case LeafHealthStatus::Normal: return "Normal";
        case LeafHealthStatus::Suspicious: return "Suspicious";
        case LeafHealthStatus::Abnormal: return "Abnormal";
        default: return "Unknown";
    }
}

cv::Scalar statusColor(const LeafHealthStatus status) {
    switch (status) {
        case LeafHealthStatus::Normal: return cv::Scalar(0, 255, 0);
        case LeafHealthStatus::Suspicious: return cv::Scalar(0, 255, 255);
        case LeafHealthStatus::Abnormal: return cv::Scalar(0, 0, 255);
        default: return cv::Scalar(255, 255, 255);
    }
}

void configureCamera(cv::VideoCapture& capture) {
    capture.set(cv::CAP_PROP_FRAME_WIDTH, kFrameWidth);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, kFrameHeight);
    capture.set(cv::CAP_PROP_FPS, kFrameFps);
    capture.set(cv::CAP_PROP_BUFFERSIZE, 1);
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
    cv::findContours(
        fruitMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<FrameDetection> detections;
    detections.reserve(contours.size());

    for (const auto& contour : contours) {
        if (!isValidFruitContour(contour)) {
            continue;
        }

        const cv::Rect rect = cv::boundingRect(contour);
        detections.push_back(
            {rect, rect.x + rect.width / 2, rect.y + rect.height / 2});
    }

    return detections;
}

LeafColorStats analyzeLeafColors(const cv::Mat& hsvRoi, const cv::Mat& maskRoi) {
    LeafColorStats stats{};

    int total = 0;
    int yellow = 0;
    int white = 0;
    int black = 0;
    int brown = 0;

    for (int y = 0; y < hsvRoi.rows; ++y) {
        const auto* hsvRow = hsvRoi.ptr<cv::Vec3b>(y);
        const auto* maskRow = maskRoi.ptr<uchar>(y);

        for (int x = 0; x < hsvRoi.cols; ++x) {
            if (maskRow[x] == 0) {
                continue;
            }

            ++total;
            const cv::Vec3b& hsv = hsvRow[x];

            if (hsv[2] < 55) {
                ++black;
            } else if (hsv[1] < 35 && hsv[2] > 160) {
                ++white;
            } else if (hsv[0] >= 15 && hsv[0] <= 40 && hsv[1] >= 45 && hsv[2] >= 80) {
                ++yellow;
            } else if (hsv[0] >= 5 && hsv[0] <= 22 &&
                       hsv[1] >= 50 && hsv[2] >= 40 && hsv[2] <= 180) {
                ++brown;
            }
        }
    }

    if (total > 0) {
        stats.yellowRatio = static_cast<double>(yellow) / static_cast<double>(total);
        stats.whiteRatio = static_cast<double>(white) / static_cast<double>(total);
        stats.blackRatio = static_cast<double>(black) / static_cast<double>(total);
        stats.brownRatio = static_cast<double>(brown) / static_cast<double>(total);
        stats.abnormalRatio =
            stats.yellowRatio + stats.whiteRatio + stats.blackRatio + stats.brownRatio;

        if (stats.abnormalRatio >= 0.20 ||
            stats.blackRatio >= 0.08 ||
            stats.brownRatio >= 0.10) {
            stats.status = LeafHealthStatus::Abnormal;
        } else if (stats.abnormalRatio >= 0.08 ||
                   stats.yellowRatio >= 0.05 ||
                   stats.whiteRatio >= 0.05) {
            stats.status = LeafHealthStatus::Suspicious;
        }
    }

    return stats;
}

std::vector<LeafTarget> detectLeaves(const cv::Mat& hsv, const cv::Mat& leafMask) {
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(
        leafMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<LeafTarget> leaves;
    leaves.reserve(contours.size());

    for (const auto& contour : contours) {
        if (!isValidLeafContour(contour)) {
            continue;
        }

        const cv::Rect bounds = cv::boundingRect(contour);

        cv::Mat roiMask = cv::Mat::zeros(bounds.size(), CV_8UC1);
        std::vector<cv::Point> shifted;
        shifted.reserve(contour.size());

        for (const auto& p : contour) {
            shifted.push_back({p.x - bounds.x, p.y - bounds.y});
        }

        cv::fillPoly(
            roiMask, std::vector<std::vector<cv::Point>>{shifted}, cv::Scalar(255));

        const auto stats = analyzeLeafColors(hsv(bounds), roiMask);

        leaves.push_back({
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

    return leaves;
}

void drawFruitDetections(cv::Mat& frame, const std::vector<FruitTarget>& fruits) {
    for (std::size_t i = 0; i < fruits.size(); ++i) {
        cv::rectangle(frame, fruits[i].bounds, cv::Scalar(0, 255, 0), 2);
        cv::putText(frame,
                    "Fruit " + std::to_string(i + 1),
                    {fruits[i].bounds.x, std::max(20, fruits[i].bounds.y - 10)},
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.5,
                    cv::Scalar(0, 255, 0),
                    2);
    }
}

void drawLeafDetections(cv::Mat& frame, const std::vector<LeafTarget>& leaves) {
    for (std::size_t i = 0; i < leaves.size(); ++i) {
        const cv::Scalar color = statusColor(leaves[i].status);
        cv::rectangle(frame, leaves[i].bounds, color, 2);
        cv::putText(frame,
                    "Leaf " + std::to_string(i + 1) + " " + toStatusText(leaves[i].status),
                    {leaves[i].bounds.x, std::max(20, leaves[i].bounds.y - 10)},
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.5,
                    color,
                    2);
    }
}

std::vector<FruitTarget> convertFruitTargets(
    const std::vector<FrameDetection>& detections) {
    std::vector<FruitTarget> fruits;
    fruits.reserve(detections.size());

    for (const auto& d : detections) {
        fruits.push_back({d.rect, d.targetX, d.targetY});
    }

    return fruits;
}

}  // namespace

CameraService::CameraService(LocalDdsBus& bus,
                             int cameraIndex,
                             const std::string& savePath,
                             bool showPreviewWindow)
    : bus_(bus),
      cameraIndex_(cameraIndex),
      savePath_(savePath),
      showPreviewWindow_(showPreviewWindow) {}

CameraService::~CameraService() {
    stop();
}

void CameraService::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }

    stopRequested_.store(false);

    triggerSub_ = bus_.subscribe<CameraTriggerTopic>(
        [this](const CameraTriggerTopic& topic) {
            executeBurst(topic.count, topic.intervalMs);
        });

    saveWorker_ = std::thread(&CameraService::saveLoop, this);
    processWorker_ = std::thread(&CameraService::processLoop, this);
    captureWorker_ = std::thread(&CameraService::captureLoop, this);

    if (previewEnabled()) {
        previewWorker_ = std::thread(&CameraService::previewLoop, this);
    }
}

void CameraService::stop() {
    stopRequested_.store(true);
    triggerSub_.reset();

    rawFrameCv_.notify_all();
    saveCv_.notify_all();
    previewCv_.notify_all();

    if (captureWorker_.joinable()) {
        captureWorker_.join();
    }
    if (processWorker_.joinable()) {
        processWorker_.join();
    }
    if (saveWorker_.joinable()) {
        saveWorker_.join();
    }
    if (previewWorker_.joinable()) {
        previewWorker_.join();
    }

    running_.store(false);
}

bool CameraService::isRunning() const noexcept {
    return running_.load();
}

void CameraService::setPreviewEnabled(bool enabled) {
    const bool old = showPreviewWindow_.exchange(enabled);

    if (!old && enabled && running_.load() && !previewWorker_.joinable()) {
        previewWorker_ = std::thread(&CameraService::previewLoop, this);
    }

    if (!enabled) {
        previewCv_.notify_all();
    }
}

bool CameraService::previewEnabled() const noexcept {
    return showPreviewWindow_.load();
}

cv::Mat CameraService::latestFrame() const {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return latestFrame_.clone();
}

std::optional<CameraDetections> CameraService::latestDetections() const {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    return latestDetections_;
}

void CameraService::setDetectionCallback(DetectionCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    detectionCallback_ = std::move(callback);
}

void CameraService::setFrameCallback(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    frameCallback_ = std::move(callback);
}

void CameraService::captureLoop() {
    cv::VideoCapture capture(cameraIndex_);
    if (!capture.isOpened()) {
        return;
    }

    configureCamera(capture);

    cv::Mat frame;
    while (!stopRequested_.load()) {
        if (!capture.read(frame) || frame.empty()) {
            break;
        }

        {
            std::lock_guard<std::mutex> lock(rawFrameMutex_);
            rawFrame_ = frame.clone();
            rawFrameReady_ = true;
        }
        rawFrameCv_.notify_one();
    }
}

void CameraService::processLoop() {
    ensureDirectoryExists(savePath_);

    cv::Mat frame;
    cv::Mat hsv;

    auto lastSaveTime = std::chrono::steady_clock::now();
    std::vector<LeafTarget> cachedLeaves;
    std::uint64_t frameCounter = 0;

    while (!stopRequested_.load()) {
        {
            std::unique_lock<std::mutex> lock(rawFrameMutex_);
            rawFrameCv_.wait(lock, [this] {
                return stopRequested_.load() || rawFrameReady_;
            });

            if (stopRequested_.load() && !rawFrameReady_) {
                break;
            }

            frame = rawFrame_.clone();
            rawFrameReady_ = false;
        }

        ++frameCounter;

        preprocessFrame(frame, hsv);

        const auto fruitDetections = detectFruits(buildFruitMask(hsv));
        const auto fruits = convertFruitTargets(fruitDetections);

        if ((frameCounter % kLeafDetectEveryNFrames) == 0U) {
            cachedLeaves = detectLeaves(hsv, buildLeafMask(hsv));
        }

        CameraDetections detections;
        detections.fruits = fruits;
        detections.leaves = cachedLeaves;

        drawFruitDetections(frame, detections.fruits);
        drawLeafDetections(frame, detections.leaves);

        const bool hasFruitTarget = !detections.fruits.empty();
        if (hasFruitTarget) {
            ObjectDetectedTopic det;
            det.detected = true;
            det.objectType = "fruit";
            bus_.publish(det);
        }

        if (hasFruitTarget &&
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - lastSaveTime).count() >=
                kSaveIntervalSeconds) {
            detections.savedImagePath =
                savePath_ + "/AutoCapture_" +
                std::to_string(
                    std::chrono::system_clock::now().time_since_epoch().count()) +
                ".jpg";
            enqueueSave(frame.clone(), detections.savedImagePath);
            lastSaveTime = std::chrono::steady_clock::now();
        }

        processBurstIfDue(frame);

        publishDetections(detections);
        updateLatestFrame(frame);
        updatePreviewFrame(frame);
        notifyFrameReady();
    }

    clearLatestDetections();
}

void CameraService::saveLoop() {
    while (true) {
        SaveRequest req;

        {
            std::unique_lock<std::mutex> lock(saveMutex_);
            saveCv_.wait(lock, [this] {
                return stopRequested_.load() || !saveQueue_.empty();
            });

            if (saveQueue_.empty() && stopRequested_.load()) {
                break;
            }

            if (saveQueue_.empty()) {
                continue;
            }

            req = std::move(saveQueue_.front());
            saveQueue_.pop_front();
        }

        if (!req.frame.empty()) {
            cv::imwrite(req.imagePath, req.frame);
        }
    }
}

void CameraService::previewLoop() {
    while (!stopRequested_.load()) {
        cv::Mat frameToShow;

        {
            std::unique_lock<std::mutex> lock(previewMutex_);
            previewCv_.wait(lock, [this] {
                return stopRequested_.load() ||
                       (!showPreviewWindow_.load() ? false : previewFrameReady_);
            });

            if (stopRequested_.load()) {
                break;
            }

            if (!showPreviewWindow_.load()) {
                continue;
            }

            frameToShow = previewFrame_.clone();
            previewFrameReady_ = false;
        }

        if (!frameToShow.empty()) {
            cv::imshow(kWindowName, frameToShow);
            if (cv::waitKey(1) == 27) {
                stopRequested_.store(true);
                rawFrameCv_.notify_all();
                saveCv_.notify_all();
                previewCv_.notify_all();
                break;
            }
        }
    }

    cv::destroyWindow(kWindowName);
}

void CameraService::enqueueSave(cv::Mat frame, std::string imagePath) {
    std::lock_guard<std::mutex> lock(saveMutex_);

    if (saveQueue_.size() >= kMaxSaveQueueDepth) {
        saveQueue_.pop_front();
    }

    saveQueue_.push_back({std::move(frame), std::move(imagePath)});
    saveCv_.notify_one();
}

void CameraService::executeBurst(int count, int intervalMs) {
    if (count <= 0 || intervalMs < 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(burstMutex_);
    burstState_.active = true;
    burstState_.remainingShots = count;
    burstState_.intervalMs = intervalMs;
    burstState_.nextShotTime = std::chrono::steady_clock::now();
}

void CameraService::processBurstIfDue(const cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(burstMutex_);

    if (!burstState_.active || burstState_.remainingShots <= 0) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (now < burstState_.nextShotTime) {
        return;
    }

    const std::string path =
        savePath_ + "/Burst_" +
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
        "_" + std::to_string(burstState_.remainingShots) + ".jpg";

    enqueueSave(frame.clone(), path);

    --burstState_.remainingShots;
    if (burstState_.remainingShots <= 0) {
        burstState_.active = false;
        return;
    }

    burstState_.nextShotTime =
        now + std::chrono::milliseconds(burstState_.intervalMs);
}

void CameraService::updateLatestFrame(const cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(frameMutex_);
    latestFrame_ = frame.clone();
}

void CameraService::updatePreviewFrame(const cv::Mat& frame) {
    if (!showPreviewWindow_.load()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(previewMutex_);
        previewFrame_ = frame.clone();
        previewFrameReady_ = true;
    }
    previewCv_.notify_one();
}

void CameraService::publishDetections(const CameraDetections& detections) {
    DetectionCallback cb;
    {
        std::lock_guard<std::mutex> lock(detectionMutex_);
        latestDetections_ = detections;
    }
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        cb = detectionCallback_;
    }
    if (cb) {
        cb(detections);
    }
}

void CameraService::clearLatestDetections() {
    std::lock_guard<std::mutex> lock(detectionMutex_);
    latestDetections_.reset();
}

void CameraService::notifyFrameReady() {
    FrameCallback cb;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        cb = frameCallback_;
    }
    if (cb) {
        cb();
    }
}

void CameraService::ensureDirectoryExists(const std::string& path) const {
    struct stat s {};
    if (stat(path.c_str(), &s) != 0) {
        mkdir(path.c_str(), 0777);
    }
}