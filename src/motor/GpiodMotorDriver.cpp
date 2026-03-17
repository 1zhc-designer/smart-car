#include "motor/GpiodMotorDriver.hpp"

#include <gpiod.h>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
[[noreturn]] void throwSys(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

void closeFd(int& fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
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

void notifyEvent(int eventFd) {
    uint64_t one = 1;
    (void)::write(eventFd, &one, sizeof(one));
}

void setPeriodicTimer(int timerFd, int periodUs) {
    itimerspec its{};
    its.it_interval.tv_sec = periodUs / 1000000;
    its.it_interval.tv_nsec = (periodUs % 1000000) * 1000L;
    its.it_value = its.it_interval;
    if (::timerfd_settime(timerFd, 0, &its, nullptr) < 0) {
        throwSys("timerfd_settime");
    }
}

void drainTimerfd(int timerFd) {
    uint64_t expirations = 0;
    (void)::read(timerFd, &expirations, sizeof(expirations));
}
} // namespace

int GpiodMotorDriver::clampSpeed(int speed) {
    return std::clamp(speed, 0, 100);
}

GpiodMotorDriver::GpiodMotorDriver(Pins pins,
                                   int pwmRange,
                                   int pwmFrequencyHz,
                                   const char* chipPath)
    : pins_(pins), pwmRange_(pwmRange), pwmFrequencyHz_(pwmFrequencyHz) {
    configureOutputs(chipPath);

    pwmTimerFd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (pwmTimerFd_ < 0) {
        throwSys("timerfd_create");
    }

    pwmEventFd_ = ::eventfd(0, EFD_NONBLOCK);
    if (pwmEventFd_ < 0) {
        throwSys("eventfd");
    }

    pwmEpollFd_ = ::epoll_create1(0);
    if (pwmEpollFd_ < 0) {
        throwSys("epoll_create1");
    }

    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = pwmTimerFd_;
        if (::epoll_ctl(pwmEpollFd_, EPOLL_CTL_ADD, pwmTimerFd_, &ev) < 0) {
            throwSys("epoll_ctl(timer)");
        }
    }
    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = pwmEventFd_;
        if (::epoll_ctl(pwmEpollFd_, EPOLL_CTL_ADD, pwmEventFd_, &ev) < 0) {
            throwSys("epoll_ctl(event)");
        }
    }

    setPeriodicTimer(pwmTimerFd_, 1000000 / std::max(1, pwmFrequencyHz_ * pwmRange_));

    pwmRunning_ = true;
    pwmThread_ = std::thread(&GpiodMotorDriver::pwmLoop, this);
    stopAll();
}

GpiodMotorDriver::~GpiodMotorDriver() {
    stopAll();
    stopPwmThread();

    if (request_) {
        gpiod_line_request_release(request_);
        request_ = nullptr;
    }
    if (chip_) {
        gpiod_chip_close(chip_);
        chip_ = nullptr;
    }
}

void GpiodMotorDriver::configureOutputs(const char* chipPath) {
    chip_ = gpiod_chip_open(chipPath);
    if (!chip_) {
        throw std::runtime_error("Failed to open gpiochip");
    }

    gpiod_line_settings* settings = gpiod_line_settings_new();
    gpiod_line_config* lineConfig = gpiod_line_config_new();
    gpiod_request_config* requestConfig = gpiod_request_config_new();
    if (!settings || !lineConfig || !requestConfig) {
        gpiod_line_settings_free(settings);
        gpiod_line_config_free(lineConfig);
        gpiod_request_config_free(requestConfig);
        throw std::runtime_error("Failed to allocate libgpiod objects");
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    const std::array<unsigned int, 6> offsets = {
        static_cast<unsigned int>(pins_.PWMA),
        static_cast<unsigned int>(pins_.AIN1),
        static_cast<unsigned int>(pins_.AIN2),
        static_cast<unsigned int>(pins_.PWMB),
        static_cast<unsigned int>(pins_.BIN1),
        static_cast<unsigned int>(pins_.BIN2)};

    if (gpiod_line_config_add_line_settings(lineConfig, offsets.data(), offsets.size(), settings) < 0) {
        gpiod_line_settings_free(settings);
        gpiod_line_config_free(lineConfig);
        gpiod_request_config_free(requestConfig);
        throw std::runtime_error("Failed to configure GPIO lines");
    }

    gpiod_request_config_set_consumer(requestConfig, "smartcar-motor");
    request_ = gpiod_chip_request_lines(chip_, requestConfig, lineConfig);

    gpiod_line_settings_free(settings);
    gpiod_line_config_free(lineConfig);
    gpiod_request_config_free(requestConfig);

    if (!request_) {
        throw std::runtime_error("Failed to request motor GPIO lines");
    }
}

void GpiodMotorDriver::notifyPwmWorker() {
    if (pwmEventFd_ >= 0) {
        notifyEvent(pwmEventFd_);
    }
}

void GpiodMotorDriver::stopPwmThread() {
    if (!pwmRunning_) {
        return;
    }
    pwmRunning_ = false;
    notifyPwmWorker();
    if (pwmThread_.joinable()) {
        pwmThread_.join();
    }
    closeFd(pwmTimerFd_);
    closeFd(pwmEventFd_);
    closeFd(pwmEpollFd_);
}

void GpiodMotorDriver::setDirectionUnsafe(bool left, bool forward) {
    const int in1 = left ? pins_.AIN1 : pins_.BIN1;
    const int in2 = left ? pins_.AIN2 : pins_.BIN2;
    gpiod_line_request_set_value(request_, in1, forward ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
    gpiod_line_request_set_value(request_, in2, forward ? GPIOD_LINE_VALUE_INACTIVE : GPIOD_LINE_VALUE_ACTIVE);
}

void GpiodMotorDriver::setPwmLevelUnsafe(bool left, bool high) {
    const int pwmPin = left ? pins_.PWMA : pins_.PWMB;
    gpiod_line_request_set_value(request_, pwmPin, high ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
}

void GpiodMotorDriver::setLeft(int speed, bool forward) {
    std::lock_guard<std::mutex> lock(mutex_);
    left_.speed = clampSpeed(speed);
    left_.forward = forward;
    setDirectionUnsafe(true, forward);
    notifyPwmWorker();
}

void GpiodMotorDriver::setRight(int speed, bool forward) {
    std::lock_guard<std::mutex> lock(mutex_);
    right_.speed = clampSpeed(speed);
    right_.forward = forward;
    setDirectionUnsafe(false, forward);
    notifyPwmWorker();
}

void GpiodMotorDriver::stopAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    left_.speed = 0;
    right_.speed = 0;
    gpiod_line_request_set_value(request_, pins_.AIN1, GPIOD_LINE_VALUE_INACTIVE);
    gpiod_line_request_set_value(request_, pins_.AIN2, GPIOD_LINE_VALUE_INACTIVE);
    gpiod_line_request_set_value(request_, pins_.BIN1, GPIOD_LINE_VALUE_INACTIVE);
    gpiod_line_request_set_value(request_, pins_.BIN2, GPIOD_LINE_VALUE_INACTIVE);
    setPwmLevelUnsafe(true, false);
    setPwmLevelUnsafe(false, false);
    notifyPwmWorker();
}

void GpiodMotorDriver::pwmLoop() {
    int phase = 0;
    constexpr int kMaxEvents = 4;
    epoll_event events[kMaxEvents];

    while (pwmRunning_) {
        const int n = ::epoll_wait(pwmEpollFd_, events, kMaxEvents, -1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        for (int i = 0; i < n; ++i) {
            const int fd = events[i].data.fd;
            if (fd == pwmEventFd_) {
                drainEventfd(pwmEventFd_);
            } else if (fd == pwmTimerFd_) {
                drainTimerfd(pwmTimerFd_);
            }
        }

        MotorState left;
        MotorState right;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            left = left_;
            right = right_;
        }

        const bool leftHigh = left.speed > 0 && phase < left.speed;
        const bool rightHigh = right.speed > 0 && phase < right.speed;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            setPwmLevelUnsafe(true, leftHigh);
            setPwmLevelUnsafe(false, rightHigh);
        }

        phase = (phase + 1) % std::max(1, pwmRange_);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    setPwmLevelUnsafe(true, false);
    setPwmLevelUnsafe(false, false);
}
