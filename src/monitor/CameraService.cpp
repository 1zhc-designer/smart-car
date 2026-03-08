#include "monitor/CameraService.hpp"

#include <sys/stat.h>

#include <stdexcept>
#include <vector>

namespace {
constexpr int kFrameWidth = 640;
constexpr int kFrameHeight = 480;
constexpr int kFrameFps = 30;
constexpr double kMinContourArea = 2000.0;
constexpr int kSaveIntervalSeconds = 2;
const char* kWindowName = "Real-time Monitor";
}

CameraService::CameraService(int cameraIndex, const std::string& savePath)
    : cameraIndex_(cameraIndex), savePath_(savePath) {}

CameraService::~CameraService() {
    stop();
}

bool CameraService::isRunning() const noexcept {
    return running_.load();
}

void CameraService::ensureDirectory(const std::string& path) {
    struct stat info {};
    if (::stat(path.c_str(), &info) != 0) {
        ::mkdir(path.c_str(), 0777);
    }
}

void CameraService::start() {
    if (running_.load()) return;

    stopRequested_.store(false);
    worker_ = std::thread([this]() {
        running_.store(true);
        try {
            runLoop();
        } catch (...) {
        }
        running_.store(false);
    });
}

void CameraService::stop() {
    stopRequested_.store(true);
    if (worker_.joinable()) {
        worker_.join();
    }
}

void CameraService::runLoop() {
    cv::VideoCapture cap(cameraIndex_);
    if (!cap.isOpened()) {
        throw std::runtime_error("Cannot open camera");
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, kFrameWidth);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, kFrameHeight);
    cap.set(cv::CAP_PROP_FPS, kFrameFps);

    ensureDirectory(savePath_);

    cv::Mat frame;
    cv::Mat hsv;
    cv::Mat mask1;
    cv::Mat mask2;
    cv::Mat mask;

    auto lastSaveTime = std::chrono::steady_clock::now();

    cv::namedWindow(kWindowName, cv::WINDOW_AUTOSIZE);

    while (!stopRequested_.load()) {
        cap >> frame;
        if (frame.empty()) {
            break;
        }

        cv::GaussianBlur(frame, frame, cv::Size(5, 5), 0);
        cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

        cv::inRange(hsv, cv::Scalar(0, 120, 70), cv::Scalar(10, 255, 255), mask1);
        cv::inRange(hsv, cv::Scalar(170, 120, 70), cv::Scalar(180, 255, 255), mask2);
        mask = mask1 | mask2;

        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
        cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        bool foundInThisFrame = false;
        int targetX = 0;
        int targetY = 0;

        for (const auto& contour : contours) {
            const double area = cv::contourArea(contour);
            if (area > kMinContourArea) {
                const cv::Rect rect = cv::boundingRect(contour);
                targetX = rect.x + rect.width / 2;
                targetY = rect.y + rect.height / 2;

                cv::rectangle(frame, rect, cv::Scalar(0, 255, 0), 2);

                const std::string label =
                    "Red Fruit: [" + std::to_string(targetX) + "," + std::to_string(targetY) + "]";
                cv::putText(frame,
                            label,
                            cv::Point(rect.x, std::max(20, rect.y - 10)),
                            cv::FONT_HERSHEY_SIMPLEX,
                            0.5,
                            cv::Scalar(0, 255, 0),
                            2);

                foundInThisFrame = true;
                break;
            }
        }

        if (foundInThisFrame) {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::seconds>(now - lastSaveTime).count();

            if (elapsed >= kSaveIntervalSeconds) {
                const std::string fileName =
                    savePath_ + "/Fruit_X" + std::to_string(targetX) +
                    "_Y" + std::to_string(targetY) + ".jpg";

                if (cv::imwrite(fileName, frame)) {
                    lastSaveTime = now;
                }
            }
        }

        cv::imshow(kWindowName, frame);

        const char key = static_cast<char>(cv::waitKey(10));
        if (key == 27) {
            break;
        }
    }

    cap.release();
    cv::destroyWindow(kWindowName);
}