#include "autonomy/AutoTrackService.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

namespace {
constexpr bool kObstacleActiveLow = true;

[[noreturn]] void throwSys(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

void closeFd(int& fd) noexcept {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

void drainCounterFd(int fd) noexcept {
    std::uint64_t value = 0;
    while (true) {
        const ssize_t r = ::read(fd, &value, sizeof(value));
        if (r == static_cast<ssize_t>(sizeof(value))) {
            continue;
        }
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        break;
    }
}

void setTimerOnce(int timerFd, std::chrono::milliseconds delay) {
    itimerspec its{};
    its.it_value.tv_sec = static_cast<time_t>(delay.count() / 1000);
    its.it_value.tv_nsec = static_cast<long>((delay.count() % 1000) * 1000'000LL);
    if (::timerfd_settime(timerFd, 0, &its, nullptr) < 0) {
        throwSys("timerfd_settime(oneshot)");
    }
}

void setTimerPeriodic(int timerFd, std::chrono::milliseconds period) {
    itimerspec its{};
    its.it_value.tv_sec = static_cast<time_t>(period.count() / 1000);
    its.it_value.tv_nsec = static_cast<long>((period.count() % 1000) * 1000'000LL);
    its.it_interval = its.it_value;
    if (::timerfd_settime(timerFd, 0, &its, nullptr) < 0) {
        throwSys("timerfd_settime(periodic)");
    }
}

void disarmTimer(int timerFd) noexcept {
    itimerspec its{};
    (void)::timerfd_settime(timerFd, 0, &its, nullptr);
}
} // namespace

AutoTrackService::AutoTrackService(LocalDdsBus& bus, GimbalService& gimbal, const char* gpioChipPath)
    : bus_(bus), gimbal_(gimbal), gpioChipPath_(gpioChipPath) {}

AutoTrackService::~AutoTrackService() {
    stop();
}

void AutoTrackService::start() {
    if (running_.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

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

        ensureTimersReady();

        detectionSub_ = bus_.subscribe<ObjectDetectedTopic>([this](const ObjectDetectedTopic& msg) {
            if (msg.detected) {
                queueObjectDetectedEvent();
            }
        });

        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            const auto obstacleValues = obstacleReq_->get_values(obstacleOffsets_);
            obstacleActive_ = obstacleTriggered(obstacleValues[0]) || obstacleTriggered(obstacleValues[1]);

            const auto lineValues = sensorReq_->get_values(sensorOffsets_);
            leftActive_ = (lineValues[0] == gpiod::line::value::ACTIVE);
            rightActive_ = (lineValues[1] == gpiod::line::value::ACTIVE);
            currentPan_ = kSweepCenter;
            currentStep_ = kSweepStep;
            lockedPan_ = kSweepCenter;
            objectPanLocked_ = false;
            objectStage_ = ObjectStage::Idle;
            isHandlingObject_ = false;
            cooldownUntil_ = std::chrono::steady_clock::time_point{};
            lastMotion_ = Motion::Stop;
            lastSpeed_ = 0;
        }

        objectEventQueued_.store(false, std::memory_order_release);

        controlThread_ = std::thread(&AutoTrackService::controlThreadFunc, this);
        sensorThread_ = std::thread(&AutoTrackService::sensorThreadFunc, this);
        obstacleThread_ = std::thread(&AutoTrackService::obstacleThreadFunc, this);

        recomputeMotion();
        armGimbalTimerPeriodic(kGimbalPeriod);
        std::cout << "[AutoTrack] Service started." << std::endl;
    } catch (...) {
        running_.store(false, std::memory_order_release);
        detectionSub_.reset();
        closeKernelObjects();
        sensorReq_.reset();
        obstacleReq_.reset();
        chip_.reset();
        throw;
    }
}

void AutoTrackService::stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    detectionSub_.reset();
    objectEventQueued_.store(false, std::memory_order_release);

    if (stopFd_ >= 0) {
        const std::uint64_t one = 1;
        (void)::write(stopFd_, &one, sizeof(one));
    }

    if (controlThread_.joinable()) {
        controlThread_.join();
    }
    if (sensorThread_.joinable()) {
        sensorThread_.join();
    }
    if (obstacleThread_.joinable()) {
        obstacleThread_.join();
    }

    forceStopStateAndPublish("auto-track-stop");
    setGimbalCenter();

    closeKernelObjects();
    sensorReq_.reset();
    obstacleReq_.reset();
    chip_.reset();

    {
        std::lock_guard<std::mutex> queueLock(queueMutex_);
        eventQueue_.clear();
    }

    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        isHandlingObject_ = false;
        objectPanLocked_ = false;
        objectStage_ = ObjectStage::Idle;
        currentPan_ = kSweepCenter;
        lockedPan_ = kSweepCenter;
    }
}

void AutoTrackService::ensureTimersReady() {
    epollFd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epollFd_ < 0) {
        throwSys("epoll_create1");
    }

    stopFd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (stopFd_ < 0) {
        throwSys("eventfd(stop)");
    }

    queueFd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (queueFd_ < 0) {
        throwSys("eventfd(queue)");
    }

    gimbalTimerFd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (gimbalTimerFd_ < 0) {
        throwSys("timerfd_create(gimbal)");
    }

    objectTimerFd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (objectTimerFd_ < 0) {
        throwSys("timerfd_create(object)");
    }

    for (const int fd : {stopFd_, queueFd_, gimbalTimerFd_, objectTimerFd_}) {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = fd;
        if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
            throwSys("epoll_ctl");
        }
    }
}

void AutoTrackService::closeKernelObjects() noexcept {
    disarmObjectTimer();
    disarmGimbalTimer();
    closeFd(objectTimerFd_);
    closeFd(gimbalTimerFd_);
    closeFd(queueFd_);
    closeFd(stopFd_);
    closeFd(epollFd_);
}

void AutoTrackService::queueLineEvent(bool leftActive, bool rightActive) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (!eventQueue_.empty() && eventQueue_.back().type == EventType::LineChanged) {
            eventQueue_.back().leftActive = leftActive;
            eventQueue_.back().rightActive = rightActive;
        } else {
            if (eventQueue_.size() >= kMaxQueuedEvents) {
                eventQueue_.pop_front();
            }
            eventQueue_.push_back(Event{EventType::LineChanged, leftActive, rightActive, false});
        }
    }
    if (queueFd_ >= 0) {
        const std::uint64_t one = 1;
        (void)::write(queueFd_, &one, sizeof(one));
    }
}

void AutoTrackService::queueObstacleEvent(bool obstacleActive) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (!eventQueue_.empty() && eventQueue_.back().type == EventType::ObstacleChanged) {
            eventQueue_.back().obstacleActive = obstacleActive;
        } else {
            if (eventQueue_.size() >= kMaxQueuedEvents) {
                eventQueue_.pop_front();
            }
            eventQueue_.push_back(Event{EventType::ObstacleChanged, false, false, obstacleActive});
        }
    }
    if (queueFd_ >= 0) {
        const std::uint64_t one = 1;
        (void)::write(queueFd_, &one, sizeof(one));
    }
}

void AutoTrackService::queueObjectDetectedEvent() {
    if (objectEventQueued_.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (eventQueue_.size() >= kMaxQueuedEvents) {
            eventQueue_.pop_front();
        }
        eventQueue_.push_back(Event{EventType::ObjectDetected, false, false, false});
    }
    if (queueFd_ >= 0) {
        const std::uint64_t one = 1;
        (void)::write(queueFd_, &one, sizeof(one));
    }
}

void AutoTrackService::processQueuedEvents() {
    std::deque<Event> local;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        local.swap(eventQueue_);
    }

    for (const Event& event : local) {
        switch (event.type) {
        case EventType::LineChanged:
            handleLineEvent(event.leftActive, event.rightActive);
            break;
        case EventType::ObstacleChanged:
            handleObstacleEvent(event.obstacleActive);
            break;
        case EventType::ObjectDetected:
            handleObjectDetectedEvent();
            break;
        }
    }
}

void AutoTrackService::handleLineEvent(bool leftActive, bool rightActive) {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        leftActive_ = leftActive;
        rightActive_ = rightActive;
    }
    recomputeMotion();
}

void AutoTrackService::handleObstacleEvent(bool obstacleActive) {
    bool risingEdge = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        risingEdge = obstacleActive && !obstacleActive_;
        obstacleActive_ = obstacleActive;
        if (obstacleActive_) {
            objectPanLocked_ = false;
        }
    }

    if (risingEdge) {
        forceStopStateAndPublish("auto-obstacle-detected");
        setGimbalCenter();
    }

    recomputeMotion();
}

void AutoTrackService::handleObjectDetectedEvent() {
    objectEventQueued_.store(false, std::memory_order_release);

    const auto now = std::chrono::steady_clock::now();
    bool shouldStart = false;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (!obstacleActive_ &&
            !isHandlingObject_ &&
            objectStage_ == ObjectStage::Idle &&
            now >= cooldownUntil_) {
            isHandlingObject_ = true;
            objectStage_ = ObjectStage::WaitingBeforeTrigger;
            objectPanLocked_ = true;
            lockedPan_ = currentPan_;
            shouldStart = true;
        }
    }

    if (!shouldStart) {
        return;
    }

    forceStopStateAndPublish("object-detected-lock");
    armObjectTimerOnce(kStopWaitDuration);
}

void AutoTrackService::handleObjectTimerEvent() {
    ObjectStage nextStage = ObjectStage::Idle;
    bool fireCamera = false;
    bool finishSequence = false;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        nextStage = objectStage_;
        if (objectStage_ == ObjectStage::WaitingBeforeTrigger) {
            objectStage_ = ObjectStage::WaitingAfterTrigger;
            nextStage = objectStage_;
            fireCamera = true;
        } else if (objectStage_ == ObjectStage::WaitingAfterTrigger) {
            objectStage_ = ObjectStage::Idle;
            isHandlingObject_ = false;
            objectPanLocked_ = false;
            currentPan_ = lockedPan_;
            cooldownUntil_ = std::chrono::steady_clock::now() + kCooldownDuration;
            nextStage = objectStage_;
            finishSequence = true;
        }
    }

    if (fireCamera) {
        bus_.publish(CameraTriggerTopic{3, 500, "auto-track-logic"});
        armObjectTimerOnce(kCaptureSettleDuration);
        return;
    }

    if (finishSequence || nextStage == ObjectStage::Idle) {
        disarmObjectTimer();
        recomputeMotion();
    }
}

void AutoTrackService::handleGimbalTimerEvent() {
    int pan = kSweepCenter;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (obstacleActive_) {
            currentPan_ = kSweepCenter;
            currentStep_ = kSweepStep;
            pan = currentPan_;
        } else if (isHandlingObject_ && objectPanLocked_) {
            currentPan_ = lockedPan_;
            pan = currentPan_;
        } else {
            currentPan_ += currentStep_;
            if (currentPan_ >= kSweepMax) {
                currentPan_ = kSweepMax;
                currentStep_ = -kSweepStep;
            } else if (currentPan_ <= kSweepMin) {
                currentPan_ = kSweepMin;
                currentStep_ = kSweepStep;
            }
            pan = currentPan_;
        }
    }

    try {
        gimbal_.setPanPosition(pan);
    } catch (...) {
    }
}

void AutoTrackService::recomputeMotion() {
    Motion desiredMotion = Motion::Stop;
    int desiredSpeed = 0;
    const char* source = "auto-track-lost";

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (obstacleActive_ || isHandlingObject_) {
            desiredMotion = Motion::Stop;
            desiredSpeed = 0;
            source = "auto-hold-safety";
        } else if (!leftActive_ && !rightActive_) {
            desiredMotion = Motion::Up;
            desiredSpeed = kBaseSpeed;
            source = "auto-track";
        } else if (leftActive_ && !rightActive_) {
            desiredMotion = Motion::Left;
            desiredSpeed = kOuterSpinSpeed;
            source = "auto-track";
        } else if (!leftActive_ && rightActive_) {
            desiredMotion = Motion::Right;
            desiredSpeed = kOuterSpinSpeed;
            source = "auto-track";
        }

        if (desiredMotion == lastMotion_ && desiredSpeed == lastSpeed_) {
            return;
        }
        lastMotion_ = desiredMotion;
        lastSpeed_ = desiredSpeed;
    }

    if (desiredMotion == Motion::Stop) {
        publishStop(source);
    } else {
        publishMotion(desiredMotion, desiredSpeed, source);
    }
}

void AutoTrackService::controlThreadFunc() {
    while (running_.load(std::memory_order_acquire)) {
        epoll_event events[8];
        const int n = ::epoll_wait(epollFd_, events, 8, -1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "[AutoTrack] epoll_wait failed: " << std::strerror(errno) << std::endl;
            break;
        }

        for (int i = 0; i < n; ++i) {
            const int fd = events[i].data.fd;
            if (fd == stopFd_) {
                drainCounterFd(stopFd_);
                continue;
            }
            if (fd == queueFd_) {
                drainCounterFd(queueFd_);
                processQueuedEvents();
                continue;
            }
            if (fd == gimbalTimerFd_) {
                drainCounterFd(gimbalTimerFd_);
                handleGimbalTimerEvent();
                continue;
            }
            if (fd == objectTimerFd_) {
                drainCounterFd(objectTimerFd_);
                handleObjectTimerEvent();
                continue;
            }
        }
    }
}

void AutoTrackService::sensorThreadFunc() {
    gpiod::edge_event_buffer buffer(8);
    while (running_.load(std::memory_order_acquire)) {
        try {
            if (!sensorReq_ || !sensorReq_->wait_edge_events(kSensorWaitSlice)) {
                continue;
            }
            sensorReq_->read_edge_events(buffer);
            const auto values = sensorReq_->get_values(sensorOffsets_);
            queueLineEvent(values[0] == gpiod::line::value::ACTIVE,
                           values[1] == gpiod::line::value::ACTIVE);
        } catch (const std::exception& e) {
            std::cerr << "[AutoTrack] Sensor thread error: " << e.what() << std::endl;
        }
    }
}

void AutoTrackService::obstacleThreadFunc() {
    gpiod::edge_event_buffer buffer(8);
    while (running_.load(std::memory_order_acquire)) {
        try {
            if (!obstacleReq_ || !obstacleReq_->wait_edge_events(kObstacleWaitSlice)) {
                continue;
            }
            obstacleReq_->read_edge_events(buffer);
            const auto values = obstacleReq_->get_values(obstacleOffsets_);
            const bool active = obstacleTriggered(values[0]) || obstacleTriggered(values[1]);
            queueObstacleEvent(active);
        } catch (const std::exception& e) {
            std::cerr << "[AutoTrack] Obstacle thread error: " << e.what() << std::endl;
        }
    }
}

void AutoTrackService::publishMotion(Motion motion, int speed, const char* source) {
    bus_.publish(MotionCommandTopic{motion, speed, std::chrono::hours(24), source});
}

void AutoTrackService::publishStop(const char* source) {
    bus_.publish(MotionCommandTopic{Motion::Stop, 0, std::chrono::milliseconds(10), source});
}

void AutoTrackService::forceStopStateAndPublish(const char* source) {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        lastMotion_ = Motion::Stop;
        lastSpeed_ = 0;
    }
    publishStop(source);
}

void AutoTrackService::setGimbalCenter() noexcept {
    try {
        gimbal_.setPanPosition(kSweepCenter);
    } catch (...) {
    }
}

void AutoTrackService::armObjectTimerOnce(std::chrono::milliseconds delay) {
    setTimerOnce(objectTimerFd_, delay);
}

void AutoTrackService::armGimbalTimerPeriodic(std::chrono::milliseconds period) {
    setTimerPeriodic(gimbalTimerFd_, period);
}

void AutoTrackService::disarmObjectTimer() noexcept {
    if (objectTimerFd_ >= 0) {
        disarmTimer(objectTimerFd_);
    }
}

void AutoTrackService::disarmGimbalTimer() noexcept {
    if (gimbalTimerFd_ >= 0) {
        disarmTimer(gimbalTimerFd_);
    }
}

bool AutoTrackService::obstacleTriggered(gpiod::line::value value) {
    return kObstacleActiveLow ? (value != gpiod::line::value::ACTIVE)
                              : (value == gpiod::line::value::ACTIVE);
}