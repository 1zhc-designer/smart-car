#include "autonomy/AutoTrackService.hpp"
#include <chrono>
#include <thread>
#include <iostream>

namespace {
constexpr bool kObstacleActiveLow = true;
}

AutoTrackService::AutoTrackService(LocalDdsBus& bus, GimbalService& gimbal, const char* gpioChipPath)
    : bus_(bus), gimbal_(gimbal), gpioChipPath_(gpioChipPath) {}

AutoTrackService::~AutoTrackService() { stop(); }

void AutoTrackService::start() {
    if (running_.exchange(true, std::memory_order_acq_rel)) return;
    
    try {
        chip_.emplace(gpioChipPath_);

        sensorReq_ = chip_->prepare_request().set_consumer("auto-track-sensors")
            .add_line_settings(sensorOffsets_, gpiod::line_settings()
                .set_direction(gpiod::line::direction::INPUT)
                .set_edge_detection(gpiod::line::edge::BOTH)
                .set_bias(gpiod::line::bias::PULL_UP)).do_request();

        obstacleReq_ = chip_->prepare_request().set_consumer("auto-track-obstacles")
            .add_line_settings(obstacleOffsets_, gpiod::line_settings()
                .set_direction(gpiod::line::direction::INPUT)
                .set_edge_detection(gpiod::line::edge::BOTH)
                .set_bias(gpiod::line::bias::PULL_UP)).do_request();

        detectionSub_ = bus_.subscribe<ObjectDetectedTopic>([this](const ObjectDetectedTopic& msg) { onObjectDetected(msg); });

        const auto initialObstacleValues = obstacleReq_->get_values(obstacleOffsets_);
        bool initialObstacle = obstacleTriggered(initialObstacleValues[0]) || obstacleTriggered(initialObstacleValues[1]);
        obstacleActive_.store(initialObstacle, std::memory_order_release);
        lastObstacleState_ = initialObstacle;

        const auto initialLineValues = sensorReq_->get_values(sensorOffsets_);
        handleLineState(initialLineValues[0] == gpiod::line::value::ACTIVE, 
                        initialLineValues[1] == gpiod::line::value::ACTIVE);

        sensorThread_ = std::thread(&AutoTrackService::sensorThreadFunc, this);
        obstacleThread_ = std::thread(&AutoTrackService::obstacleThreadFunc, this);
        gimbalThread_ = std::thread(&AutoTrackService::gimbalThreadFunc, this);
        
        std::cout << "[AutoTrack] Service started and initial state applied." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[AutoTrack] Start failed: " << e.what() << std::endl;
        running_.store(false, std::memory_order_release);
        throw;
    }
}

void AutoTrackService::stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) return;
    publishStop("auto-track-stop");
    detectionSub_.reset();
    if (sensorThread_.joinable()) sensorThread_.join();
    if (obstacleThread_.joinable()) obstacleThread_.join();
    if (gimbalThread_.joinable()) gimbalThread_.join();
    try { gimbal_.setPanPosition(kSweepCenter); } catch (...) {}
    sensorReq_.reset(); obstacleReq_.reset(); chip_.reset();
}

void AutoTrackService::onObjectDetected(const ObjectDetectedTopic& msg) {
    if (!msg.detected) return;
    auto now = std::chrono::steady_clock::now();
    if (isHandlingObject_.load(std::memory_order_acquire) || (now - lastActionEndTime_ < kCooldownDuration)) return;

    std::thread([this]() {
        isHandlingObject_.store(true, std::memory_order_release);
        publishStop("object-detected-lock");
        
        std::this_thread::sleep_for(kStopWaitDuration);
        bus_.publish(CameraTriggerTopic{3, 500, "auto-track-logic"});
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        
        lastActionEndTime_ = std::chrono::steady_clock::now();
        isHandlingObject_.store(false, std::memory_order_release);

        if (sensorReq_) {
            auto values = sensorReq_->get_values(sensorOffsets_);
            handleLineState(values[0] == gpiod::line::value::ACTIVE, values[1] == gpiod::line::value::ACTIVE);
        }
    }).detach();
}

void AutoTrackService::handleLineState(bool leftActive, bool rightActive) {
    if (obstacleActive_.load(std::memory_order_acquire) || isHandlingObject_.load(std::memory_order_acquire)) {
        publishStop("auto-hold-safety");
        return;
    }

    if (!leftActive && !rightActive) {
        publishMotion(Motion::Up, kBaseSpeed, "auto-track");
    } else if (leftActive && !rightActive) {
        publishMotion(Motion::Left, kOuterSpinSpeed, "auto-track");
    } else if (!leftActive && rightActive) {
        publishMotion(Motion::Right, kOuterSpinSpeed, "auto-track");
    } else {
        publishStop("auto-track-lost");
    }
}

void AutoTrackService::sensorThreadFunc() {
    gpiod::edge_event_buffer buffer(8);
    while (running_.load(std::memory_order_acquire)) {
        if (!sensorReq_ || !sensorReq_->wait_edge_events(std::chrono::milliseconds(100))) continue;
        sensorReq_->read_edge_events(buffer);
        const auto values = sensorReq_->get_values(sensorOffsets_);
        handleLineState(values[0] == gpiod::line::value::ACTIVE, values[1] == gpiod::line::value::ACTIVE);
    }
}

void AutoTrackService::obstacleThreadFunc() {
    gpiod::edge_event_buffer buffer(8);
    while (running_.load(std::memory_order_acquire)) {
        if (!obstacleReq_ || !obstacleReq_->wait_edge_events(std::chrono::milliseconds(100))) continue;
        obstacleReq_->read_edge_events(buffer);
        const auto values = obstacleReq_->get_values(obstacleOffsets_);
        const bool currentActive = (obstacleTriggered(values[0]) || obstacleTriggered(values[1]));

        obstacleActive_.store(currentActive, std::memory_order_release);

        if (currentActive && !lastObstacleState_) {
            publishStop("auto-obstacle-detected");
            try { gimbal_.setPanPosition(kSweepCenter); } catch (...) {}
        } 
        else if (!currentActive && lastObstacleState_) {
            const auto lineValues = sensorReq_->get_values(sensorOffsets_);
            handleLineState(lineValues[0] == gpiod::line::value::ACTIVE, lineValues[1] == gpiod::line::value::ACTIVE);
        }
        lastObstacleState_ = currentActive;
    }
}

void AutoTrackService::gimbalThreadFunc() {
    int currentPos = kSweepMin;
    int step = kSweepStep;
    while (running_.load(std::memory_order_acquire)) {
        if (obstacleActive_.load(std::memory_order_acquire) || isHandlingObject_.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        try {
            currentPos += step;
            if (currentPos >= kSweepMax || currentPos <= kSweepMin) step = -step;
            gimbal_.setPanPosition(currentPos);
        } catch (...) {}

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void AutoTrackService::publishMotion(Motion motion, int speed, const char* source) {
    bus_.publish(MotionCommandTopic{motion, speed, std::chrono::hours(24), source});
}

void AutoTrackService::publishStop(const char* source) {
    bus_.publish(MotionCommandTopic{Motion::Stop, 0, std::chrono::milliseconds(10), source});
}

bool AutoTrackService::obstacleTriggered(gpiod::line::value value) {
    return kObstacleActiveLow ? (value != gpiod::line::value::ACTIVE) : (value == gpiod::line::value::ACTIVE);
}