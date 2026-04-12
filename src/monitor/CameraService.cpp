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
    cv::Mat lowerRedMask, upperRedMask, fruitMask;
    cv::inRange(hsv, cv::Scalar(0, 160, 100), cv::Scalar(10, 255, 255), lowerRedMask);
    cv::inRange(hsv, cv::Scalar(170, 160, 100), cv::Scalar(180, 255, 255), upperRedMask);
    fruitMask = lowerRedMask | upperRedMask;
    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
    cv::morphologyEx(fruitMask, fruitMask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(fruitMask, fruitMask, cv::MORPH_CLOSE, kernel);
    return fruitMask;
}

cv::Mat buildLeafMask(const cv::Mat& hsv) {
    cv::Mat leafMask;
    cv::inRange(hsv, cv::Scalar(25, 35, 35), cv::Scalar(95, 255, 255), leafMask);
    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(leafMask, leafMask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(leafMask, leafMask, cv::MORPH_CLOSE, kernel);
    return leafMask;
}

bool isValidFruitContour(const std::vector<cv::Point>& contour) {
    const double area = cv::contourArea(contour);
    if (area < kMinFruitContourArea) return false;
    const cv::Rect rect = cv::boundingRect(contour);
    if (rect.height == 0) return false;
    const float aspectRatio = static_cast<float>(rect.width) / static_cast<float>(rect.height);
    return aspectRatio >= kMinAspectRatio && aspectRatio <= kMaxAspectRatio;
}

bool isValidLeafContour(const std::vector<cv::Point>& contour) {
    const double area = cv::contourArea(contour);
    if (area < kMinLeafContourArea) return false;
    const cv::Rect rect = cv::boundingRect(contour);
    return rect.width >= kMinLeafWidth && rect.height >= kMinLeafHeight;
}

std::vector<FrameDetection> detectFruits(const cv::Mat& fruitMask) {
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(fruitMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    std::vector<FrameDetection> detections;
    for (const auto& contour : contours) {
        if (!isValidFruitContour(contour)) continue;
        const cv::Rect rect = cv::boundingRect(contour);
        detections.push_back({rect, rect.x + rect.width / 2, rect.y + rect.height / 2});
    }
    return detections;
}

LeafColorStats analyzeLeafColors(const cv::Mat& hsvRoi, const cv::Mat& maskRoi) {
    LeafColorStats stats{};
    int total = 0, yellow = 0, white = 0, black = 0, brown = 0;
    for (int y = 0; y < hsvRoi.rows; ++y) {
        for (int x = 0; x < hsvRoi.cols; ++x) {
            if (maskRoi.at<uchar>(y, x) == 0) continue;
            ++total;
            const auto& hsv = hsvRoi.at<cv::Vec3b>(y, x);
            if (hsv[2] < 55) ++black;
            else if (hsv[1] < 35 && hsv[2] > 160) ++white;
            else if (hsv[0] >= 15 && hsv[0] <= 40 && hsv[1] >= 45 && hsv[2] >= 80) ++yellow;
            else if (hsv[0] >= 5 && hsv[0] <= 22 && hsv[1] >= 50 && hsv[2] >= 40 && hsv[2] <= 180) ++brown;
        }
    }
    if (total > 0) {
        stats.yellowRatio = (double)yellow / total;
        stats.whiteRatio = (double)white / total;
        stats.blackRatio = (double)black / total;
        stats.brownRatio = (double)brown / total;
        stats.abnormalRatio = stats.yellowRatio + stats.whiteRatio + stats.blackRatio + stats.brownRatio;
        if (stats.abnormalRatio >= 0.20 || stats.blackRatio >= 0.08 || stats.brownRatio >= 0.10) stats.status = LeafHealthStatus::Abnormal;
        else if (stats.abnormalRatio >= 0.08 || stats.yellowRatio >= 0.05 || stats.whiteRatio >= 0.05) stats.status = LeafHealthStatus::Suspicious;
    }
    return stats;
}

std::vector<LeafTarget> detectLeaves(const cv::Mat& hsv, const cv::Mat& leafMask) {
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(leafMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    std::vector<LeafTarget> leaves;
    for (const auto& contour : contours) {
        if (!isValidLeafContour(contour)) continue;
        const cv::Rect bounds = cv::boundingRect(contour);
        cv::Mat roiMask = cv::Mat::zeros(bounds.size(), CV_8UC1);
        std::vector<cv::Point> shifted;
        for(auto& p : contour) shifted.push_back({p.x - bounds.x, p.y - bounds.y});
        cv::fillPoly(roiMask, std::vector<std::vector<cv::Point>>{shifted}, cv::Scalar(255));
        auto stats = analyzeLeafColors(hsv(bounds), roiMask);
        leaves.push_back({bounds, bounds.x + bounds.width/2, bounds.y + bounds.height/2, stats.yellowRatio, stats.whiteRatio, stats.blackRatio, stats.brownRatio, stats.abnormalRatio, stats.status});
    }
    return leaves;
}

void drawFruitDetections(cv::Mat& frame, const std::vector<FrameDetection>& fruits) {
    for (size_t i = 0; i < fruits.size(); ++i) {
        cv::rectangle(frame, fruits[i].rect, cv::Scalar(0, 255, 0), 2);
        cv::putText(frame, "Fruit " + std::to_string(i + 1), {fruits[i].rect.x, std::max(20, fruits[i].rect.y - 10)}, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
    }
}

void drawLeafDetections(cv::Mat& frame, const std::vector<LeafTarget>& leaves) {
    for (size_t i = 0; i < leaves.size(); ++i) {
        cv::Scalar color = statusColor(leaves[i].status);
        cv::rectangle(frame, leaves[i].bounds, color, 2);
        cv::putText(frame, "Leaf " + std::to_string(i + 1) + " " + toStatusText(leaves[i].status), {leaves[i].bounds.x, std::max(20, leaves[i].bounds.y - 10)}, cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 2);
    }
}

bool hasAbnormalLeaf(const std::vector<LeafTarget>& leaves) {
    return std::any_of(leaves.begin(), leaves.end(), [](const LeafTarget& l) { return l.status != LeafHealthStatus::Normal; });
}

} // namespace

CameraService::CameraService(LocalDdsBus& bus, int cameraIndex, const std::string& savePath, bool showPreviewWindow)
    : bus_(bus), cameraIndex_(cameraIndex), savePath_(savePath), showPreviewWindow_(showPreviewWindow) {}

CameraService::~CameraService() { stop(); }

void CameraService::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;
    stopRequested_.store(false);

    triggerSub_ = bus_.subscribe<CameraTriggerTopic>([this](const CameraTriggerTopic& topic) {
        executeBurst(topic.count, topic.intervalMs);
    });

    saveWorker_ = std::thread(&CameraService::saveLoop, this);
    processWorker_ = std::thread(&CameraService::processLoop, this);
    captureWorker_ = std::thread(&CameraService::captureLoop, this);
}

void CameraService::stop() {
    stopRequested_.store(true);
    triggerSub_.reset();
    rawFrameCv_.notify_all();
    saveCv_.notify_all();
    if (captureWorker_.joinable()) captureWorker_.join();
    if (processWorker_.joinable()) processWorker_.join();
    if (saveWorker_.joinable()) saveWorker_.join();
    running_.store(false);
}

void CameraService::executeBurst(int count, int intervalMs) {
    std::thread([this, count, intervalMs]() {
        for (int i = 0; i < count; ++i) {
            cv::Mat frame = latestFrame();
            if (!frame.empty()) {
                auto now = std::chrono::system_clock::now().time_since_epoch().count();
                std::string path = savePath_ + "/Burst_" + std::to_string(now) + "_" + std::to_string(i) + ".jpg";
                enqueueSave(frame, path);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    }).detach();
}

void CameraService::processLoop() {
    ensureDirectoryExists(savePath_);
    cv::Mat frame, hsv;
    auto lastSaveTime = std::chrono::steady_clock::now();

    while (!stopRequested_.load()) {
        {
            std::unique_lock<std::mutex> lock(rawFrameMutex_);
            rawFrameCv_.wait(lock, [this] { return stopRequested_.load() || rawFrameReady_; });
            if (stopRequested_ && !rawFrameReady_) break;
            frame = rawFrame_.clone();
            rawFrameReady_ = false;
        }

        preprocessFrame(frame, hsv);
        auto fruitDetections = detectFruits(buildFruitMask(hsv));
        auto leafDetections = detectLeaves(hsv, buildLeafMask(hsv));

        drawFruitDetections(frame, fruitDetections);
        drawLeafDetections(frame, leafDetections);

        bool hasTarget = !fruitDetections.empty() || hasAbnormalLeaf(leafDetections);
        if (hasTarget) {
            ObjectDetectedTopic det;
            det.detected = true;
            det.objectType = fruitDetections.empty() ? "leaf" : "fruit";
            bus_.publish(det);
        }

        if (hasTarget && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - lastSaveTime).count() >= kSaveIntervalSeconds) {
            enqueueSave(frame.clone(), savePath_ + "/AutoCapture_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".jpg");
            lastSaveTime = std::chrono::steady_clock::now();
        }

        updateLatestFrame(frame);
        notifyFrameReady();
        
        if (previewEnabled()) {
            cv::imshow(kWindowName, frame);
            if (cv::waitKey(1) == 27) stopRequested_.store(true);
        }
    }
}

void CameraService::captureLoop() {
    cv::VideoCapture capture(cameraIndex_);
    if (!capture.isOpened()) return;
    configureCamera(capture);
    cv::Mat frame;
    while (!stopRequested_) {
        if (!capture.read(frame) || frame.empty()) break;
        {
            std::lock_guard<std::mutex> lock(rawFrameMutex_);
            rawFrame_ = frame.clone();
            rawFrameReady_ = true;
        }
        rawFrameCv_.notify_one();
    }
}

void CameraService::saveLoop() {
    while (true) {
        SaveRequest req;
        {
            std::unique_lock<std::mutex> lock(saveMutex_);
            saveCv_.wait(lock, [this]{ return stopRequested_ || pendingSave_.has_value(); });
            if (!pendingSave_.has_value() && stopRequested_) break;
            req = std::move(*pendingSave_);
            pendingSave_.reset();
        }
        if (!req.frame.empty()) cv::imwrite(req.imagePath, req.frame);
    }
}

void CameraService::enqueueSave(cv::Mat frame, std::string path) {
    std::lock_guard<std::mutex> lock(saveMutex_);
    pendingSave_ = {std::move(frame), std::move(path)};
    saveCv_.notify_one();
}

void CameraService::updateLatestFrame(const cv::Mat& frame) {
    std::lock_guard<std::mutex> lock(frameMutex_);
    latestFrame_ = frame.clone();
}

bool CameraService::isRunning() const noexcept { return running_.load(); }
void CameraService::setPreviewEnabled(bool e) { showPreviewWindow_.store(e); }
bool CameraService::previewEnabled() const noexcept { return showPreviewWindow_.load(); }
cv::Mat CameraService::latestFrame() const { std::lock_guard<std::mutex> lock(frameMutex_); return latestFrame_.clone(); }
void CameraService::setDetectionCallback(DetectionCallback cb) { std::lock_guard<std::mutex> lock(callbackMutex_); detectionCallback_ = std::move(cb); }
void CameraService::setFrameCallback(FrameCallback cb) { std::lock_guard<std::mutex> lock(callbackMutex_); frameCallback_ = std::move(cb); }
void CameraService::ensureDirectoryExists(const std::string& p) const { struct stat s; if (stat(p.c_str(), &s) != 0) mkdir(p.c_str(), 0777); }
void CameraService::notifyFrameReady() { FrameCallback cb; { std::lock_guard<std::mutex> lock(callbackMutex_); cb = frameCallback_; } if (cb) cb(); }