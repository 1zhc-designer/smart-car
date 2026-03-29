#include "monitor/MonitorService.hpp"

#include <algorithm>
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
#include <limits>
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

[[noreturn]] void throwSys(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

void closeFd(int& fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

void setTimerOnce(int timerFd, int ms) {
    itimerspec its{};
    its.it_value.tv_sec = ms / 1000;
    its.it_value.tv_nsec = (ms % 1000) * 1000'000L;
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
    while (true) {
        const ssize_t r = ::read(eventFd, &value, sizeof(value));
        if (r == static_cast<ssize_t>(sizeof(value))) {
            continue;
        }
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        break;
    }
}
} // namespace

MonitorService::~MonitorService() {
    stop();
}

double MonitorService::currentTemperature() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return currentTemperature_;
}

int MonitorService::lowLimit() const noexcept {
    return lowLimit_.load();
}

int MonitorService::highLimit() const noexcept {
    return highLimit_.load();
}

std::string MonitorService::currentStatus() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return currentStatus_;
}

void MonitorService::setLimits(int low, int high) {
    if (low >= high) {
        return;
    }
    lowLimit_.store(low, std::memory_order_release);
    highLimit_.store(high, std::memory_order_release);
}

void MonitorService::setStatusCallback(StatusCallback cb) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    statusCallback_ = std::move(cb);
}

void MonitorService::start() {
    if (running_) {
        return;
    }

    stopRequested_ = false;
    worker_ = std::thread([this]() {
        running_ = true;
        try {
            runLoop();
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[Monitor] Fatal: %s\n", e.what());
        } catch (...) {
            std::fprintf(stderr, "[Monitor] Fatal: unknown exception\n");
        }
        running_ = false;
    });
}

void MonitorService::stop() {
    stopRequested_ = true;

    if (stopFd_ >= 0) {
        uint64_t one = 1;
        (void)::write(stopFd_, &one, sizeof(one));
    }

    if (worker_.joinable()) {
        worker_.join();
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

    // Warm up channel 3 once so the first real sample is not stale.
    (void)readPcf8591Channel(3);
    ::usleep(2000);
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

void MonitorService::writePhysicalLevel(int offset, bool high) {
    if (!gpioRequest_) {
        return;
    }

    gpiod_line_request_set_value(
        gpioRequest_,
        offset,
        high ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE
    );
}

/*
   Keep the same behavior as your validated monitor logic:
   setLedRed(true) / setLedGreen(true) means "drive line high".
*/
void MonitorService::setLedRed(bool on) {
    writePhysicalLevel(kLedR, on);
}

void MonitorService::setLedGreen(bool on) {
    writePhysicalLevel(kLedG, on);
}

/*
   Buzzer is LOW-active:
   on  -> LOW
   off -> HIGH
*/
void MonitorService::setBuzzer(bool on) {
    writePhysicalLevel(kBuzzer, !on);
}

unsigned char MonitorService::readPcf8591Channel(int channel) {
    const unsigned char control = static_cast<unsigned char>(0x40 | (channel & 0x03));

    if (::write(i2cFd_, &control, 1) != 1) {
        throw std::runtime_error("PCF8591 control write failed");
    }

    ::usleep(2000);

    // First byte after channel switch is stale.
    unsigned char stale = 0;
    if (::read(i2cFd_, &stale, 1) != 1) {
        throw std::runtime_error("PCF8591 stale read failed");
    }

    int sum = 0;
    constexpr int kSamples = 4;

    for (int i = 0; i < kSamples; ++i) {
        unsigned char value = 0;
        if (::read(i2cFd_, &value, 1) != 1) {
            throw std::runtime_error("PCF8591 value read failed");
        }
        sum += static_cast<int>(value);
        ::usleep(1000);
    }

    return static_cast<unsigned char>(sum / kSamples);
}

bool MonitorService::tryReadNtcTemperature(double& temp) {
    constexpr double Vref = 5.0;
    constexpr double R0 = 10000.0;
    constexpr double B = 3950.0;
    constexpr double T0 = 298.15;
    constexpr double Rser = 10000.0;

    int validCount = 0;
    double sumTemp = 0.0;

    for (int i = 0; i < 5; ++i) {
        const unsigned char adc = readPcf8591Channel(3);

        double vr = Vref * static_cast<double>(adc) / 255.0;
        vr = std::clamp(vr, 0.000001, Vref - 0.000001);

        const double rt = Rser * vr / (Vref - vr);
        const double tk = 1.0 / ((std::log(rt / R0) / B) + (1.0 / T0));
        const double oneTemp = tk - 273.15;

        if (!std::isfinite(oneTemp) || oneTemp < -80.0 || oneTemp > 150.0) {
            ::usleep(1000);
            continue;
        }

        sumTemp += oneTemp;
        ++validCount;
        ::usleep(1000);
    }

    if (validCount == 0) {
        return false;
    }

    temp = sumTemp / static_cast<double>(validCount);
    lastValidTemperature_ = temp;
    hasValidTemperature_ = true;
    return true;
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

void MonitorService::runLoop() {
    ensureGpioReady();
    ensureI2cReady();

    epollFd_ = ::epoll_create1(0);
    if (epollFd_ < 0) {
        throwSys("epoll_create1");
    }

    timerFd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerFd_ < 0) {
        throwSys("timerfd_create");
    }

    stopFd_ = ::eventfd(0, EFD_NONBLOCK);
    if (stopFd_ < 0) {
        throwSys("eventfd");
    }

    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = timerFd_;
        if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, timerFd_, &ev) < 0) {
            throwSys("epoll_ctl(timerFd)");
        }
    }

    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = stopFd_;
        if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, stopFd_, &ev) < 0) {
            throwSys("epoll_ctl(stopFd)");
        }
    }

    enum class Phase {
        Sample,
        BeepOn,
        BeepOff,
        Gap
    };

    Phase phase = Phase::Sample;
    int beepMs = 0;
    int beepsLeft = 0;

    setTimerOnce(timerFd_, 1);

    while (!stopRequested_) {
        epoll_event events[4];
        const int n = ::epoll_wait(epollFd_, events, 4, -1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        for (int i = 0; i < n; ++i) {
            const int fd = events[i].data.fd;

            if (fd == stopFd_) {
                drainEventfd(stopFd_);
                continue;
            }

            if (fd != timerFd_) {
                continue;
            }

            drainTimerfd(timerFd_);

            if (stopRequested_) {
                break;
            }

            switch (phase) {
            case Phase::Sample: {
                const int low = lowLimit_.load(std::memory_order_acquire);
                const int high = highLimit_.load(std::memory_order_acquire);

                double temp = std::numeric_limits<double>::quiet_NaN();
                if (!tryReadNtcTemperature(temp)) {
                    if (hasValidTemperature_) {
                        temp = lastValidTemperature_;
                        setLedRed(false);
                        setLedGreen(true);
                        setBuzzer(false);
                        updateUiState(temp, "Sensor Read Retry");
                    } else {
                        setLedRed(false);
                        setLedGreen(false);
                        setBuzzer(false);
                        updateUiState(std::numeric_limits<double>::quiet_NaN(), "Sensor Invalid");
                    }

                    phase = Phase::Gap;
                    setTimerOnce(timerFd_, 200);
                    break;
                }

                if (temp < low) {
                    setLedRed(true);
                    setLedGreen(true);
                    updateUiState(temp, "Too Cold");

                    beepsLeft = 3;
                    beepMs = 400;
                    setBuzzer(true);
                    phase = Phase::BeepOn;
                    setTimerOnce(timerFd_, beepMs);
                } else if (temp >= high) {
                    setLedRed(true);
                    setLedGreen(false);
                    updateUiState(temp, "Too Hot");

                    beepsLeft = 3;
                    beepMs = 80;
                    setBuzzer(true);
                    phase = Phase::BeepOn;
                    setTimerOnce(timerFd_, beepMs);
                } else {
                    setLedRed(false);
                    setLedGreen(true);
                    setBuzzer(false);
                    updateUiState(temp, "Normal");

                    phase = Phase::Gap;
                    setTimerOnce(timerFd_, 200);
                }
            } break;

            case Phase::BeepOn:
                setBuzzer(false);
                phase = Phase::BeepOff;
                setTimerOnce(timerFd_, beepMs);
                break;

            case Phase::BeepOff:
                --beepsLeft;
                if (beepsLeft > 0) {
                    setBuzzer(true);
                    phase = Phase::BeepOn;
                    setTimerOnce(timerFd_, beepMs);
                } else {
                    setBuzzer(false);
                    phase = Phase::Gap;
                    setTimerOnce(timerFd_, 200);
                }
                break;

            case Phase::Gap:
                phase = Phase::Sample;
                setTimerOnce(timerFd_, 1);
                break;
            }
        }
    }

    setBuzzer(false);
    setLedRed(false);
    setLedGreen(false);
    closeResources();
}