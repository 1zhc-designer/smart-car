#include "monitor/MonitorService.hpp"

#include <gpiod.h>

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
constexpr int kLedR = 20;
constexpr int kLedG = 21;
constexpr int kBuzzer = 17;
constexpr int kPcf8591Address = 0x48;
constexpr const char* kI2cDevice = "/dev/i2c-1";
constexpr const char* kGpioChip = "/dev/gpiochip0";
constexpr int kSamplePeriodMs = 200;
constexpr double kAdcMax = 255.0;
constexpr double kVref = 3.3;
constexpr double kSeriesResistorOhms = 10'000.0;
constexpr double kThermistorNominalOhms = 10'000.0;
constexpr double kThermistorNominalCelsius = 25.0;
constexpr double kThermistorBeta = 3950.0;

[[noreturn]] void throwSys(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

void closeFd(int& fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

void addFdToEpoll(int epollFd, int fd) {
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (::epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        throwSys("epoll_ctl(EPOLL_CTL_ADD)");
    }
}

void setPeriodicTimer(int timerFd, int periodMs) {
    itimerspec its{};
    its.it_value.tv_sec = periodMs / 1000;
    its.it_value.tv_nsec = (periodMs % 1000) * 1'000'000L;
    its.it_interval = its.it_value;
    if (::timerfd_settime(timerFd, 0, &its, nullptr) < 0) {
        throwSys("timerfd_settime");
    }
}

void drainTimerfd(int timerFd) {
    uint64_t expirations = 0;
    while (::read(timerFd, &expirations, sizeof(expirations)) == static_cast<ssize_t>(sizeof(expirations))) {
    }
}

void drainEventfd(int eventFd) {
    uint64_t value = 0;
    while (::read(eventFd, &value, sizeof(value)) == static_cast<ssize_t>(sizeof(value))) {
    }
}

int clampToByte(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return value;
}
} // namespace

MonitorService::~MonitorService() {
    stop();
}

double MonitorService::currentTemperature() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return currentTemperature_;
}

int MonitorService::currentLightLevel() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return currentLightLevel_;
}

bool MonitorService::isDark() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return currentIsDark_;
}

int MonitorService::lowLimit() const noexcept {
    return lowLimit_.load();
}

int MonitorService::highLimit() const noexcept {
    return highLimit_.load();
}

int MonitorService::lightThreshold() const noexcept {
    return lightThreshold_.load();
}

std::string MonitorService::currentStatus() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return currentStatus_;
}

void MonitorService::setLimits(int low, int high) {
    if (low >= high) {
        return;
    }
    lowLimit_.store(low);
    highLimit_.store(high);
}

void MonitorService::setLightThreshold(int threshold) noexcept {
    lightThreshold_.store(clampToByte(threshold));
}

void MonitorService::setStatusCallback(StatusCallback cb) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    statusCallback_ = std::move(cb);
}

void MonitorService::setLightLevelCallback(LightLevelCallback cb) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    lightLevelCallback_ = std::move(cb);
}

void MonitorService::setLightStateCallback(LightStateCallback cb) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    lightStateCallback_ = std::move(cb);
}

void MonitorService::start() {
    if (running_.load()) {
        return;
    }

    stopRequested_.store(false);
    worker_ = std::thread([this]() {
        running_.store(true);
        try {
            runLoop();
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[Monitor] Fatal: %s\n", e.what());
            closeResources();
        } catch (...) {
            std::fprintf(stderr, "[Monitor] Fatal: unknown exception\n");
            closeResources();
        }
        running_.store(false);
    });
}

void MonitorService::stop() {
    stopRequested_.store(true);

    if (stopFd_ >= 0) {
        const uint64_t one = 1;
        (void)::write(stopFd_, &one, sizeof(one));
    }

    if (worker_.joinable()) {
        worker_.join();
    }
}

void MonitorService::ensurePollerReady() {
    if (epollFd_ >= 0 && timerFd_ >= 0 && stopFd_ >= 0) {
        return;
    }

    epollFd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epollFd_ < 0) {
        throwSys("epoll_create1");
    }

    timerFd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (timerFd_ < 0) {
        throwSys("timerfd_create");
    }

    stopFd_ = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (stopFd_ < 0) {
        throwSys("eventfd");
    }

    addFdToEpoll(epollFd_, timerFd_);
    addFdToEpoll(epollFd_, stopFd_);
    setPeriodicTimer(timerFd_, kSamplePeriodMs);
}

void MonitorService::ensureGpioReady() {
    if (gpioRequest_ != nullptr) {
        return;
    }

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
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    const std::array<unsigned int, 3> offsets = {
        static_cast<unsigned int>(kLedR),
        static_cast<unsigned int>(kLedG),
        static_cast<unsigned int>(kBuzzer)};

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
}

void MonitorService::ensureI2cReady() {
    if (i2cFd_ >= 0) {
        return;
    }

    i2cFd_ = ::open(kI2cDevice, O_RDWR | O_CLOEXEC);
    if (i2cFd_ < 0) {
        throw std::runtime_error("Failed to open /dev/i2c-1 for PCF8591");
    }
    if (::ioctl(i2cFd_, I2C_SLAVE, kPcf8591Address) < 0) {
        throw std::runtime_error("Failed to select PCF8591 I2C address");
    }
}

void MonitorService::closeResources() {
    closeFd(timerFd_);
    closeFd(stopFd_);
    closeFd(epollFd_);
    closeFd(i2cFd_);

    if (gpioRequest_) {
        gpiod_line_request_release(gpioRequest_);
        gpioRequest_ = nullptr;
    }
    if (chip_) {
        gpiod_chip_close(chip_);
        chip_ = nullptr;
    }
}

void MonitorService::writeGpioValue(int offset, bool active) {
    if (!gpioRequest_) {
        return;
    }

    (void)gpiod_line_request_set_value(
        gpioRequest_,
        static_cast<unsigned int>(offset),
        active ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
}

unsigned char MonitorService::readPcf8591Channel(int channel) {
    if (i2cFd_ < 0) {
        return 0;
    }

    if (::ioctl(i2cFd_, I2C_SLAVE, kPcf8591Address) < 0) {
        return 0;
    }

    const unsigned char control = static_cast<unsigned char>(0x40 | (channel & 0x03));
    if (::write(i2cFd_, &control, 1) != 1) {
        return 0;
    }

    unsigned char dummy = 0;
    if (::read(i2cFd_, &dummy, 1) != 1) {
        return 0;
    }

    unsigned char value = 0;
    if (::read(i2cFd_, &value, 1) != 1) {
        return 0;
    }

    return value;
}

unsigned char MonitorService::readJoystick() {
    return readPcf8591Channel(kJoystickChannel);
}

double MonitorService::readNtcTemperature() {
    const double raw = static_cast<double>(readPcf8591Channel(kNtcChannel));
    if (raw <= 0.0) {
        return currentTemperature();
    }
    if (raw >= kAdcMax) {
        return 0.0;
    }

    const double voltage = (raw / kAdcMax) * kVref;
    if (voltage <= 0.0 || voltage >= kVref) {
        return currentTemperature();
    }

    const double resistance = (kSeriesResistorOhms * voltage) / (kVref - voltage);
    if (resistance <= 0.0) {
        return currentTemperature();
    }

    const double steinhart =
        1.0 / ((std::log(resistance / kThermistorNominalOhms) / kThermistorBeta) +
               (1.0 / (kThermistorNominalCelsius + 273.15))) -
        273.15;
    return std::isfinite(steinhart) ? steinhart : currentTemperature();
}

int MonitorService::readLightLevel() {
    const int level = static_cast<int>(readPcf8591Channel(kLightSensorChannel));
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        currentLightLevel_ = level;
    }
    return level;
}

void MonitorService::updateUiState(double temp, const std::string& status) {
    StatusCallback cb;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        currentTemperature_ = temp;
        currentStatus_ = status;
        cb = statusCallback_;
    }
    if (cb) {
        cb(temp, status);
    }
}

void MonitorService::publishLightSample(int level) {
    LightLevelCallback lightLevelCb;
    LightStateCallback lightStateCb;
    bool dark = false;
    bool stateChanged = false;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        lightLevelCb = lightLevelCallback_;
        lightStateCb = lightStateCallback_;
        dark = level <= lightThreshold_.load();
        stateChanged = (dark != currentIsDark_);
        currentIsDark_ = dark;
    }

    if (lightLevelCb) {
        lightLevelCb(level);
    }
    if (stateChanged && lightStateCb) {
        lightStateCb(level, dark);
    }
}

void MonitorService::runLoop() {
    ensurePollerReady();
    ensureI2cReady();
    ensureGpioReady();

    epoll_event events[4]{};

    while (!stopRequested_.load()) {
        const int n = ::epoll_wait(epollFd_, events, 4, -1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throwSys("epoll_wait");
        }

        for (int i = 0; i < n; ++i) {
            const int fd = events[i].data.fd;
            if (fd == stopFd_) {
                drainEventfd(stopFd_);
                stopRequested_.store(true);
                break;
            }

            if (fd != timerFd_) {
                continue;
            }

            drainTimerfd(timerFd_);

            const double temp = readNtcTemperature();
            const int light = readLightLevel();
            publishLightSample(light);

            std::string status;
            const int low = lowLimit_.load();
            const int high = highLimit_.load();
            if (temp < low) {
                status = "LOW";
                writeGpioValue(kLedR, true);
                writeGpioValue(kLedG, false);
                writeGpioValue(kBuzzer, true);
            } else if (temp > high) {
                status = "HIGH";
                writeGpioValue(kLedR, true);
                writeGpioValue(kLedG, false);
                writeGpioValue(kBuzzer, true);
            } else {
                status = "NORMAL";
                writeGpioValue(kLedR, false);
                writeGpioValue(kLedG, true);
                writeGpioValue(kBuzzer, false);
            }

            status += "|L=" + std::to_string(light);
            status += isDark() ? "|DARK" : "|BRIGHT";
            updateUiState(temp, status);
        }
    }

    writeGpioValue(kLedR, false);
    writeGpioValue(kLedG, false);
    writeGpioValue(kBuzzer, false);
    closeResources();
}
