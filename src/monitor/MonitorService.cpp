#include "monitor/MonitorService.hpp"

#include <wiringPi.h>
#include <pcf8591.h>
#include <math.h>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

// smartcar uses wiringPiSetup() / wiringPi numbering.
// BCM20 -> wPi 28
// BCM21 -> wPi 29
// BCM17 -> wPi 0
static constexpr int LED_R  = 28;
static constexpr int LED_G  = 29;
static constexpr int BUZZER = 0;

static constexpr int PCF_BASE = 120;

// Keep the same PCF channel mapping as your original good monitor.cpp
static constexpr int AIN_Y   = PCF_BASE + 0;
static constexpr int AIN_X   = PCF_BASE + 1;
static constexpr int AIN_SW  = PCF_BASE + 2;
static constexpr int AIN_NTC = PCF_BASE + 3;

using uchar = unsigned char;

[[noreturn]] void throwSys(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

void setTimerOnce(int timer_fd, int ms) {
    itimerspec its{};
    its.it_value.tv_sec  = ms / 1000;
    its.it_value.tv_nsec = (ms % 1000) * 1000'000L;
    if (::timerfd_settime(timer_fd, 0, &its, nullptr) < 0) {
        throwSys("timerfd_settime");
    }
}

void timerfdDrain(int timer_fd) {
    uint64_t expirations;
    while (::read(timer_fd, &expirations, sizeof(expirations)) == sizeof(expirations)) {}
}

void eventfdDrain(int event_fd) {
    uint64_t v;
    while (true) {
        ssize_t r = ::read(event_fd, &v, sizeof(v));
        if (r == static_cast<ssize_t>(sizeof(v))) continue;
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        break;
    }
}

// Restore the original good-version buzzer logic: LOW active, direct digitalWrite
inline void buzzerInit() {
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, HIGH);
}

inline void buzzerOn() {
    digitalWrite(BUZZER, LOW);
}

inline void buzzerOff() {
    digitalWrite(BUZZER, HIGH);
}

inline void ledInit() {
    pinMode(LED_R, OUTPUT);
    pinMode(LED_G, OUTPUT);
}

inline void setLED(int r_state, int g_state) {
    digitalWrite(LED_R, r_state);
    digitalWrite(LED_G, g_state);
}

inline uchar readJoystick() {
    uchar js = 0;
    uchar x  = static_cast<uchar>(analogRead(AIN_X));
    uchar y  = static_cast<uchar>(analogRead(AIN_Y));
    uchar sw = static_cast<uchar>(analogRead(AIN_SW));

    if (x >= 250) js = 2;
    if (x <= 5)   js = 1;
    if (y >= 250) js = 4;
    if (y <= 5)   js = 3;

    if ((int)x - 127 < 30 && (int)x - 127 > -30 &&
        (int)y - 127 < 30 && (int)y - 127 > -30 &&
        sw > 127) {
        js = 0;
    }

    return js;
}

inline double readNTC() {
    const double Vref = 5.0;
    const double R0   = 10000.0;
    const double B    = 3950.0;
    const double T0   = 298.15;
    const double Rser = 10000.0;

    unsigned char adc = static_cast<unsigned char>(analogRead(AIN_NTC));

    double Vr = Vref * adc / 255.0;
    if (Vr <= 0.000001) Vr = 0.000001;
    if (Vr >= Vref - 0.000001) Vr = Vref - 0.000001;

    double Rt = Rser * Vr / (Vref - Vr);
    double Tk = 1.0 / ((std::log(Rt / R0) / B) + (1.0 / T0));

    return Tk - 273.15;
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
    lowLimit_.store(low);
    highLimit_.store(high);
}

void MonitorService::start() {
    if (running_.load()) return;

    stopRequested_.store(false);
    worker_ = std::thread([this]() {
        running_.store(true);
        try {
            runLoop();
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[Monitor] Fatal: %s\n", e.what());
        } catch (...) {
            std::fprintf(stderr, "[Monitor] Fatal: unknown exception\n");
        }
        running_.store(false);
    });
}

void MonitorService::stop() {
    stopRequested_.store(true);

    if (stopFd_ >= 0) {
        uint64_t one = 1;
        ::write(stopFd_, &one, sizeof(one));
    }

    if (worker_.joinable()) {
        worker_.join();
    }
}

void MonitorService::runLoop() {
    if (wiringPiSetup() == -1) {
        throw std::runtime_error("wiringPiSetup failed (monitor)");
    }

    pcf8591Setup(PCF_BASE, 0x48);

    ledInit();
    buzzerInit();

    std::printf("System running...\n");
    std::printf("LED: High=RED, Low=YELLOW, Normal=GREEN\n");
    std::printf("Joystick press disabled (no exit)\n");

    const int epoll_fd = ::epoll_create1(0);
    if (epoll_fd < 0) throwSys("epoll_create1");

    const int timer_fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timer_fd < 0) throwSys("timerfd_create");

    stopFd_ = ::eventfd(0, EFD_NONBLOCK);
    if (stopFd_ < 0) throwSys("eventfd");

    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = timer_fd;
        if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &ev) < 0) {
            throwSys("epoll_ctl(timer_fd)");
        }
    }
    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = stopFd_;
        if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, stopFd_, &ev) < 0) {
            throwSys("epoll_ctl(stop_fd)");
        }
    }

    enum class Phase { Sample, BeepOn, BeepOff, Gap };

    Phase phase = Phase::Sample;
    int beep_ms = 0;
    int beeps_left = 0;

    setTimerOnce(timer_fd, 1);

    while (!stopRequested_.load()) {
        epoll_event events[4];
        int n = ::epoll_wait(epoll_fd, events, 4, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < n; ++i) {
            const int fd = events[i].data.fd;

            if (fd == stopFd_) {
                eventfdDrain(stopFd_);
                continue;
            }

            if (fd != timer_fd) continue;
            timerfdDrain(timer_fd);

            if (stopRequested_.load()) break;

            switch (phase) {
                case Phase::Sample: {
                    int low_limit = lowLimit_.load();
                    int high_limit = highLimit_.load();

                    while (true) {
                        const uchar joy = readJoystick();
                        switch (joy) {
                            case 1: --low_limit;  break;
                            case 2: ++low_limit;  break;
                            case 3: ++high_limit; break;
                            case 4: --high_limit; break;
                            default: break;
                        }

                        if (low_limit >= high_limit) {
                            low_limit = lowLimit_.load();
                            high_limit = highLimit_.load();
                            continue;
                        }
                        break;
                    }

                    lowLimit_.store(low_limit);
                    highLimit_.store(high_limit);

                    const double temp = readNTC();

                    {
                        std::lock_guard<std::mutex> lock(stateMutex_);
                        currentTemperature_ = temp;
                    }

                    if (temp < low_limit) {
                        setLED(HIGH, HIGH);
                        beeps_left = 3;
                        beep_ms = 400;

                        {
                            std::lock_guard<std::mutex> lock(stateMutex_);
                            currentStatus_ = "Too Cold";
                        }

                        buzzerOn();
                        phase = Phase::BeepOn;
                        setTimerOnce(timer_fd, beep_ms);
                    } else if (temp >= high_limit) {
                        setLED(HIGH, LOW);
                        beeps_left = 3;
                        beep_ms = 80;

                        {
                            std::lock_guard<std::mutex> lock(stateMutex_);
                            currentStatus_ = "Too Hot";
                        }

                        buzzerOn();
                        phase = Phase::BeepOn;
                        setTimerOnce(timer_fd, beep_ms);
                    } else {
                        setLED(LOW, HIGH);
                        buzzerOff();

                        {
                            std::lock_guard<std::mutex> lock(stateMutex_);
                            currentStatus_ = "Normal";
                        }

                        phase = Phase::Gap;
                        setTimerOnce(timer_fd, 200);
                    }
                } break;

                case Phase::BeepOn: {
                    buzzerOff();
                    phase = Phase::BeepOff;
                    setTimerOnce(timer_fd, beep_ms);
                } break;

                case Phase::BeepOff: {
                    --beeps_left;
                    if (beeps_left > 0) {
                        buzzerOn();
                        phase = Phase::BeepOn;
                        setTimerOnce(timer_fd, beep_ms);
                    } else {
                        buzzerOff();
                        phase = Phase::Gap;
                        setTimerOnce(timer_fd, 200);
                    }
                } break;

                case Phase::Gap: {
                    phase = Phase::Sample;
                    setTimerOnce(timer_fd, 1);
                } break;
            }
        }
    }

    buzzerOff();
    setLED(HIGH, HIGH);

    if (timer_fd >= 0) ::close(timer_fd);
    if (stopFd_ >= 0) {
        ::close(stopFd_);
        stopFd_ = -1;
    }
    if (epoll_fd >= 0) ::close(epoll_fd);
}