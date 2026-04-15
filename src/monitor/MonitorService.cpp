#include "monitor/MonitorService.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <gpiod.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <unistd.h>

namespace {
constexpr int kLedR = 20;
constexpr int kLedG = 21;
constexpr int kBuzzer = 17;

constexpr int kPcf8591Address = 0x48;
constexpr const char* kI2cDevice = "/dev/i2c-1";
constexpr const char* kGpioChip = "/dev/gpiochip0";

constexpr double kVref = 5.0;
constexpr double kR0 = 10000.0;
constexpr double kB = 3950.0;
constexpr double kT0 = 298.15;
constexpr double kRser = 10000.0;

[[noreturn]] void throwSys(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

void closeFd(int& fd) noexcept {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

void drainFdCounter(int fd) noexcept {
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

void setTimerOnce(int timerFd, int ms) {
    itimerspec its{};
    its.it_value.tv_sec = ms / 1000;
    its.it_value.tv_nsec = (ms % 1000) * 1000'000L;
    if (::timerfd_settime(timerFd, 0, &its, nullptr) < 0) {
        throwSys("timerfd_settime(oneshot)");
    }
}

void setTimerPeriodic(int timerFd, int periodMs) {
    itimerspec its{};
    its.it_value.tv_sec = periodMs / 1000;
    its.it_value.tv_nsec = (periodMs % 1000) * 1000'000L;
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

MonitorService::~MonitorService() {
    stop();
}

double MonitorService::currentTemperature() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_.currentTemperature;
}

int MonitorService::currentLightLevel() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_.currentLightLevel;
}

bool MonitorService::isDark() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_.currentIsDark;
}

int MonitorService::lightThreshold() const noexcept {
    return lightThreshold_.load(std::memory_order_acquire);
}

int MonitorService::lowLimit() const noexcept {
    return lowLimit_.load(std::memory_order_acquire);
}

int MonitorService::highLimit() const noexcept {
    return highLimit_.load(std::memory_order_acquire);
}

std::string MonitorService::currentStatus() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_.currentStatus;
}

void MonitorService::setLimits(int low, int high) {
    if (low >= high) {
        return;
    }
    lowLimit_.store(low, std::memory_order_release);
    highLimit_.store(high, std::memory_order_release);
}

int MonitorService::clampToByte(int value) noexcept {
    return std::clamp(value, 0, 255);
}

void MonitorService::setLightThreshold(int threshold) noexcept {
    lightThreshold_.store(clampToByte(threshold), std::memory_order_release);
}

void MonitorService::setStatusCallback(StatusCallback cb) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    state_.statusCallback = std::move(cb);
}

void MonitorService::setLightLevelCallback(LightLevelCallback cb) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    state_.lightLevelCallback = std::move(cb);
}

void MonitorService::setLightStateCallback(LightStateCallback cb) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    state_.lightStateCallback = std::move(cb);
}

void MonitorService::start() {
    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    stopRequested_.store(false, std::memory_order_release);
    callbackThread_ = std::thread(&MonitorService::callbackLoop, this);
    worker_ = std::thread([this]() {
        running_.store(true, std::memory_order_release);
        try {
            runLoop();
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[Monitor] Fatal: %s\n", e.what());
            updateStateAndQueueStatus(std::numeric_limits<double>::quiet_NaN(), "Monitor Fault");
        } catch (...) {
            std::fprintf(stderr, "[Monitor] Fatal: unknown exception\n");
            updateStateAndQueueStatus(std::numeric_limits<double>::quiet_NaN(), "Monitor Fault");
        }
        running_.store(false, std::memory_order_release);
        stopRequested_.store(true, std::memory_order_release);
        callbackCv_.notify_all();
    });
}

void MonitorService::stop() {
    const bool wasStarted = started_.exchange(false, std::memory_order_acq_rel);
    stopRequested_.store(true, std::memory_order_release);

    if (stopFd_ >= 0) {
        std::uint64_t one = 1;
        (void)::write(stopFd_, &one, sizeof(one));
    }

    callbackCv_.notify_all();

    if (worker_.joinable()) {
        worker_.join();
    }
    if (callbackThread_.joinable()) {
        callbackThread_.join();
    }

    if (!wasStarted) {
        closeResources();
    }
}

void MonitorService::ensureGpioReady() {
    chip_ = gpiod_chip_open(kGpioChip);
    if (!chip_) {
        throw std::runtime_error("Failed to open gpiochip for monitor outputs");
    }

    gpiod_line_settings* settings = gpiod_line_settings_new();
    gpiod_line_config* lineConfig = gpiod_line_config_new();
    gpiod_request_config* requestConfig = gpiod_request_config_new();

    if (!settings || !lineConfig || !requestConfig) {
        gpiod_line_settings_free(settings);
        gpiod_line_config_free(lineConfig);
        gpiod_request_config_free(requestConfig);
        throw std::runtime_error("Failed to allocate libgpiod monitor objects");
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);

    const std::array<unsigned int, 3> offsets = {
        static_cast<unsigned int>(kLedR),
        static_cast<unsigned int>(kLedG),
        static_cast<unsigned int>(kBuzzer)
    };

    if (gpiod_line_config_add_line_settings(lineConfig, offsets.data(), offsets.size(), settings) < 0) {
        gpiod_line_settings_free(settings);
        gpiod_line_config_free(lineConfig);
        gpiod_request_config_free(requestConfig);
        throw std::runtime_error("Failed to configure monitor GPIO lines");
    }

    gpiod_request_config_set_consumer(requestConfig, "smartcar-monitor");
    gpioRequest_ = gpiod_chip_request_lines(chip_, requestConfig, lineConfig);

    gpiod_line_settings_free(settings);
    gpiod_line_config_free(lineConfig);
    gpiod_request_config_free(requestConfig);

    if (!gpioRequest_) {
        throw std::runtime_error("Failed to request monitor GPIO lines");
    }

    setLedRed(false);
    setLedGreen(false);
    setBuzzer(false);
}

void MonitorService::ensureI2cReady() {
    i2cFd_ = ::open(kI2cDevice, O_RDWR);
    if (i2cFd_ < 0) {
        throw std::runtime_error("Failed to open /dev/i2c-1 for PCF8591");
    }

    if (::ioctl(i2cFd_, I2C_SLAVE, kPcf8591Address) < 0) {
        throw std::runtime_error("Failed to select PCF8591 I2C address");
    }

    // Warm up both used channels once. No active waiting is used.
    (void)readPcf8591Channel(kLightSensorChannel);
    (void)readPcf8591Channel(kTempSensorChannel);
}

void MonitorService::ensureEventLoopReady() {
    epollFd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epollFd_ < 0) {
        throwSys("epoll_create1");
    }

    stopFd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (stopFd_ < 0) {
        throwSys("eventfd");
    }

    sampleTimerFd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (sampleTimerFd_ < 0) {
        throwSys("timerfd_create(sample)");
    }

    alarmTimerFd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (alarmTimerFd_ < 0) {
        throwSys("timerfd_create(alarm)");
    }

    for (const int fd : {stopFd_, sampleTimerFd_, alarmTimerFd_}) {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = fd;
        if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
            throwSys("epoll_ctl");
        }
    }
}

void MonitorService::closeResources() noexcept {
    if (gpioRequest_) {
        gpiod_line_request_release(gpioRequest_);
        gpioRequest_ = nullptr;
    }
    if (chip_) {
        gpiod_chip_close(chip_);
        chip_ = nullptr;
    }

    closeFd(i2cFd_);
    closeFd(sampleTimerFd_);
    closeFd(alarmTimerFd_);
    closeFd(stopFd_);
    closeFd(epollFd_);
}

void MonitorService::writePhysicalLevel(int offset, bool high) noexcept {
    if (!gpioRequest_) {
        return;
    }

    const int rc = gpiod_line_request_set_value(
        gpioRequest_,
        offset,
        high ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE
    );

    if (rc < 0) {
        std::fprintf(stderr, "[Monitor] GPIO write failed on line %d: %s\n", offset, std::strerror(errno));
    }
}

void MonitorService::setLedRed(bool on) noexcept {
    writePhysicalLevel(kLedR, on);
}

void MonitorService::setLedGreen(bool on) noexcept {
    writePhysicalLevel(kLedG, on);
}

void MonitorService::setBuzzer(bool on) noexcept {
    writePhysicalLevel(kBuzzer, !on);
}

std::uint8_t MonitorService::readPcf8591Channel(int channel) {
    const std::uint8_t control = static_cast<std::uint8_t>(0x40 | (channel & 0x03));
    if (::write(i2cFd_, &control, 1) != 1) {
        throw std::runtime_error("PCF8591 control write failed");
    }

    // First value after channel selection is stale on PCF8591.
    std::uint8_t discard = 0;
    if (::read(i2cFd_, &discard, 1) != 1) {
        throw std::runtime_error("PCF8591 stale read failed");
    }

    int sum = 0;
    constexpr int kSamples = 2;
    for (int i = 0; i < kSamples; ++i) {
        std::uint8_t value = 0;
        if (::read(i2cFd_, &value, 1) != 1) {
            throw std::runtime_error("PCF8591 value read failed");
        }
        sum += static_cast<int>(value);
    }

    return static_cast<std::uint8_t>(sum / kSamples);
}

int MonitorService::readLightLevel() noexcept {
    try {
        return static_cast<int>(readPcf8591Channel(kLightSensorChannel));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Monitor] Light read failed: %s\n", e.what());
        return -1;
    }
}

bool MonitorService::tryReadNtcTemperature(double& temp) noexcept {
    try {
        const std::uint8_t adc = readPcf8591Channel(kTempSensorChannel);
        double vr = kVref * static_cast<double>(adc) / 255.0;
        vr = std::clamp(vr, 0.000001, kVref - 0.000001);

        const double rt = kRser * vr / (kVref - vr);
        const double tk = 1.0 / ((std::log(rt / kR0) / kB) + (1.0 / kT0));
        const double celsius = tk - 273.15;

        if (!std::isfinite(celsius) || celsius < -80.0 || celsius > 150.0) {
            return false;
        }

        temp = celsius;
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Monitor] Temperature read failed: %s\n", e.what());
        return false;
    }
}

void MonitorService::applyNormalOutputs() noexcept {
    setLedRed(false);
    setLedGreen(true);
    setBuzzer(false);
}

void MonitorService::applySensorInvalidOutputs() noexcept {
    setLedRed(false);
    setLedGreen(false);
    setBuzzer(false);
}

void MonitorService::armAlarmTimerOnce(int ms) {
    setTimerOnce(alarmTimerFd_, ms);
}

void MonitorService::armSampleTimerPeriodic(int periodMs) {
    setTimerPeriodic(sampleTimerFd_, periodMs);
}

void MonitorService::startAlarmBurst(AlarmMode mode) noexcept {
    alarmMode_ = mode;
    alarmPhase_ = AlarmPhase::On;
    beepsRemaining_ = 3;
    beepMs_ = (mode == AlarmMode::TooCold) ? 400 : 80;
    setBuzzer(true);
    try {
        armAlarmTimerOnce(beepMs_);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[Monitor] Alarm timer arm failed: %s\n", e.what());
        setBuzzer(false);
        alarmMode_ = AlarmMode::None;
        alarmPhase_ = AlarmPhase::Idle;
    }
}

void MonitorService::queueNotification(Notification notification) {
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (callbackQueue_.size() >= kMaxQueuedNotifications) {
            callbackQueue_.pop_front();
        }
        callbackQueue_.push_back(std::move(notification));
    }
    callbackCv_.notify_one();
}

void MonitorService::updateStateAndQueueStatus(double temp, std::string status) {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        state_.currentTemperature = temp;
        state_.currentStatus = status;
        if (std::isfinite(temp)) {
            state_.lastValidTemperature = temp;
            state_.hasValidTemperature = true;
        }
    }

    Notification n;
    n.type = NotifyType::Status;
    n.temperature = temp;
    n.status = std::move(status);
    queueNotification(std::move(n));
}

void MonitorService::updateStateAndQueueLight(int level) {
    bool changed = false;
    bool dark = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        state_.currentLightLevel = level;
        dark = level <= lightThreshold_.load(std::memory_order_acquire);
        changed = (dark != state_.currentIsDark);
        state_.currentIsDark = dark;
    }

    Notification raw;
    raw.type = NotifyType::LightLevel;
    raw.lightLevel = level;
    queueNotification(std::move(raw));

    if (changed) {
        Notification stateChange;
        stateChange.type = NotifyType::LightState;
        stateChange.lightLevel = level;
        stateChange.dark = dark;
        queueNotification(std::move(stateChange));
    }
}

void MonitorService::processSampleEvent() {
    const int low = lowLimit_.load(std::memory_order_acquire);
    const int high = highLimit_.load(std::memory_order_acquire);

    const int light = readLightLevel();
    if (light >= 0) {
        updateStateAndQueueLight(light);
    }

    double temp = std::numeric_limits<double>::quiet_NaN();
    if (!tryReadNtcTemperature(temp)) {
        bool hasLastValid = false;
        double lastValid = std::numeric_limits<double>::quiet_NaN();
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            hasLastValid = state_.hasValidTemperature;
            lastValid = state_.lastValidTemperature;
        }

        applySensorInvalidOutputs();
        alarmMode_ = AlarmMode::None;
        alarmPhase_ = AlarmPhase::Idle;
        disarmTimer(alarmTimerFd_);

        if (hasLastValid) {
            updateStateAndQueueStatus(lastValid, "Sensor Read Retry");
        } else {
            updateStateAndQueueStatus(std::numeric_limits<double>::quiet_NaN(), "Sensor Invalid");
        }
        return;
    }

    if (temp < low) {
        setLedRed(true);
        setLedGreen(true);
        updateStateAndQueueStatus(temp, "Too Cold");
        if (alarmMode_ != AlarmMode::TooCold || alarmPhase_ == AlarmPhase::Idle) {
            startAlarmBurst(AlarmMode::TooCold);
        }
        return;
    }

    if (temp >= high) {
        setLedRed(true);
        setLedGreen(false);
        updateStateAndQueueStatus(temp, "Too Hot");
        if (alarmMode_ != AlarmMode::TooHot || alarmPhase_ == AlarmPhase::Idle) {
            startAlarmBurst(AlarmMode::TooHot);
        }
        return;
    }

    alarmMode_ = AlarmMode::None;
    alarmPhase_ = AlarmPhase::Idle;
    disarmTimer(alarmTimerFd_);
    applyNormalOutputs();
    updateStateAndQueueStatus(temp, "Normal");
}

void MonitorService::processAlarmTimerEvent() {
    switch (alarmPhase_) {
    case AlarmPhase::Idle:
        setBuzzer(false);
        break;

    case AlarmPhase::On:
        setBuzzer(false);
        alarmPhase_ = AlarmPhase::Off;
        armAlarmTimerOnce(beepMs_);
        break;

    case AlarmPhase::Off:
        --beepsRemaining_;
        if (beepsRemaining_ > 0) {
            setBuzzer(true);
            alarmPhase_ = AlarmPhase::On;
            armAlarmTimerOnce(beepMs_);
        } else {
            setBuzzer(false);
            alarmPhase_ = AlarmPhase::RecoveryGap;
            armAlarmTimerOnce(kAlarmRecoveryGapMs);
        }
        break;

    case AlarmPhase::RecoveryGap:
        alarmPhase_ = AlarmPhase::Idle;
        alarmMode_ = AlarmMode::None;
        setBuzzer(false);
        disarmTimer(alarmTimerFd_);
        break;
    }
}

void MonitorService::runLoop() {
    ensureGpioReady();
    ensureI2cReady();
    ensureEventLoopReady();

    armSampleTimerPeriodic(kSamplePeriodMs);
    disarmTimer(alarmTimerFd_);

    while (!stopRequested_.load(std::memory_order_acquire)) {
        epoll_event events[8];
        const int n = ::epoll_wait(epollFd_, events, 8, -1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throwSys("epoll_wait");
        }

        for (int i = 0; i < n; ++i) {
            const int fd = events[i].data.fd;

            if (fd == stopFd_) {
                drainFdCounter(stopFd_);
                continue;
            }

            if (fd == sampleTimerFd_) {
                drainFdCounter(sampleTimerFd_);
                processSampleEvent();
                continue;
            }

            if (fd == alarmTimerFd_) {
                drainFdCounter(alarmTimerFd_);
                processAlarmTimerEvent();
                continue;
            }
        }
    }

    disarmTimer(sampleTimerFd_);
    disarmTimer(alarmTimerFd_);
    setBuzzer(false);
    setLedRed(false);
    setLedGreen(false);
    closeResources();
}

void MonitorService::callbackLoop() {
    while (true) {
        Notification notification;
        {
            std::unique_lock<std::mutex> lock(callbackMutex_);
            callbackCv_.wait(lock, [this]() {
                return stopRequested_.load(std::memory_order_acquire) || !callbackQueue_.empty();
            });

            if (callbackQueue_.empty()) {
                if (stopRequested_.load(std::memory_order_acquire)) {
                    break;
                }
                continue;
            }

            notification = std::move(callbackQueue_.front());
            callbackQueue_.pop_front();
        }

        StatusCallback statusCb;
        LightLevelCallback lightLevelCb;
        LightStateCallback lightStateCb;
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            statusCb = state_.statusCallback;
            lightLevelCb = state_.lightLevelCallback;
            lightStateCb = state_.lightStateCallback;
        }

        try {
            switch (notification.type) {
            case NotifyType::Status:
                if (statusCb) {
                    statusCb(notification.temperature, notification.status);
                }
                break;
            case NotifyType::LightLevel:
                if (lightLevelCb) {
                    lightLevelCb(notification.lightLevel);
                }
                break;
            case NotifyType::LightState:
                if (lightStateCb) {
                    lightStateCb(notification.lightLevel, notification.dark);
                }
                break;
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[Monitor] Callback exception: %s\n", e.what());
        } catch (...) {
            std::fprintf(stderr, "[Monitor] Callback exception: unknown\n");
        }
    }
}
