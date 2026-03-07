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
#include <stdexcept>
#include <string>

namespace {

// ===== Important: smartcar already uses wiringPiSetup() (wPi numbering).
// So monitor MUST use the same numbering scheme to avoid breaking motor/IR.
// BCM17/20/21 mapping to wiringPi (wPi):
//   BCM17 -> wPi 0
//   BCM20 -> wPi 28
//   BCM21 -> wPi 29
static constexpr int LED_R  = 28; // BCM20 (Pin 38)
static constexpr int LED_G  = 29; // BCM21 (Pin 40)
static constexpr int BUZZER = 0;  // BCM17 (Pin 36)

static constexpr int PCF_BASE = 120;

// PCF channels (same as original)
static constexpr int AIN_Y   = PCF_BASE + 0;
static constexpr int AIN_X   = PCF_BASE + 1;
static constexpr int AIN_SW  = PCF_BASE + 2;
static constexpr int AIN_NTC = PCF_BASE + 3;

using uchar = unsigned char;

inline void throwSys(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

inline void setTimerOnce(int timer_fd, int ms) {
    itimerspec its{};
    its.it_value.tv_sec  = ms / 1000;
    its.it_value.tv_nsec = (ms % 1000) * 1000'000L;
    if (::timerfd_settime(timer_fd, 0, &its, nullptr) < 0) {
        throwSys("timerfd_settime");
    }
}

inline void timerfdDrain(int timer_fd) {
    uint64_t expirations;
    (void)::read(timer_fd, &expirations, sizeof(expirations));
}

inline void eventfdNotify(int event_fd) {
    uint64_t one = 1;
    (void)::write(event_fd, &one, sizeof(one));
}

inline void eventfdDrain(int event_fd) {
    uint64_t v;
    while (true) {
        ssize_t r = ::read(event_fd, &v, sizeof(v));
        if (r == static_cast<ssize_t>(sizeof(v))) continue;
        if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        break;
    }
}

// ===== Buzzer (LOW active, same logic as original)
inline void buzzerInit() {
    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, HIGH); // OFF
}
inline void buzzerOn()  { digitalWrite(BUZZER, LOW);  }
inline void buzzerOff() { digitalWrite(BUZZER, HIGH); }

// ===== LED (comment kept same meaning as original code)
// Original comment says LOW=ON for common-anode style, but it actually sets
// states using HIGH/LOW exactly as your original monitor.cpp did.
// We keep identical calls: setLED(HIGH,HIGH), setLED(HIGH,LOW), setLED(LOW,HIGH).
inline void ledInit() {
    pinMode(LED_R, OUTPUT);
    pinMode(LED_G, OUTPUT);
}
inline void setLED(int r_state, int g_state) {
    digitalWrite(LED_R, r_state);
    digitalWrite(LED_G, g_state);
}

// ===== Joystick (same thresholds and “press disabled” behaviour)
inline uchar readJoystick() {
    uchar js = 0;
    uchar x  = (uchar)analogRead(AIN_X);
    uchar y  = (uchar)analogRead(AIN_Y);
    uchar sw = (uchar)analogRead(AIN_SW);

    if (x >= 250) js = 2;
    if (x <= 5)   js = 1;
    if (y >= 250) js = 4;
    if (y <= 5)   js = 3;

    // press disabled (same as your original)
    // if (sw <= 5)  js = 5;

    if ((int)x - 127 < 30 && (int)x - 127 > -30 &&
        (int)y - 127 < 30 && (int)y - 127 > -30 &&
        sw > 127)
    {
        js = 0;
    }
    return js;
}

// ===== NTC (same formula)
inline double readNTC() {
    const double Vref = 5.0;
    const double R0   = 10000.0;
    const double B    = 3950.0;
    const double T0   = 298.15;
    const double Rser = 10000.0;

    unsigned char adc = (unsigned char)analogRead(AIN_NTC);

    double Vr = Vref * adc / 255.0;
    if (Vr <= 0.000001) Vr = 0.000001;
    if (Vr >= Vref - 0.000001) Vr = Vref - 0.000001;

    double Rt = Rser * Vr / (Vref - Vr);
    double Tk = 1.0 / ((log(Rt / R0) / B) + (1.0 / T0));

    return Tk - 273.15;
}

} // namespace

MonitorService::~MonitorService() {
    stop();
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
    if (worker_.joinable()) worker_.join();
}

void MonitorService::runLoop() {
    // Important: DO NOT call wiringPiSetupGpio() here.
    // smartcar uses wiringPiSetup() already; we must stay consistent.
    // Calling wiringPiSetup() again is safe (it will just return quickly).
    if (wiringPiSetup() == -1) {
        throw std::runtime_error("wiringPiSetup failed (monitor)");
    }

    pcf8591Setup(PCF_BASE, 0x48);

    ledInit();
    buzzerInit();

    std::printf("System running...\n");
    std::printf("LED: High=RED, Low=YELLOW, Normal=GREEN\n");
    std::printf("Joystick press disabled (no exit)\n");

    uchar low_limit  = 26;
    uchar high_limit = 30;

    // Event-driven timing (A1 style): epoll + timerfd + eventfd(stop)
    const int epoll_fd = ::epoll_create1(0);
    if (epoll_fd < 0) throwSys("epoll_create1");

    const int timer_fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timer_fd < 0) throwSys("timerfd_create");

    const int stop_fd = ::eventfd(0, EFD_NONBLOCK);
    if (stop_fd < 0) throwSys("eventfd");

    // register fds
    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = timer_fd;
        if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &ev) < 0) throwSys("epoll_ctl(timer_fd)");
    }
    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = stop_fd;
        if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, stop_fd, &ev) < 0) throwSys("epoll_ctl(stop_fd)");
    }

    enum class Phase { Sample, BeepOn, BeepOff, Gap };
    enum class Alarm { None, Low, High };

    Phase phase = Phase::Sample;
    Alarm alarm = Alarm::None;

    int beep_ms = 0;
    int beeps_left = 0;

    // Kick off immediately
    setTimerOnce(timer_fd, 1);

    while (!stopRequested_.load()) {
        // allow stop() to wake us quickly
        if (stopRequested_.load()) break;

        epoll_event events[4];
        int n = ::epoll_wait(epoll_fd, events, 4, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < n; ++i) {
            const int fd = events[i].data.fd;

            if (fd == stop_fd) {
                eventfdDrain(stop_fd);
                continue;
            }

            if (fd != timer_fd) continue;
            timerfdDrain(timer_fd);

            if (stopRequested_.load()) break;

            switch (phase) {
                case Phase::Sample: {
                    // emulate original "flag:" block (incl. invalid limit handling)
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
                            std::printf("Lower limit must be less than upper limit\n");
                            continue; // same as goto flag
                        }
                        break;
                    }

                    const double temp = readNTC();

                    std::printf("Temp: %.2f°C  Low:%d  High:%d\n",
                                temp, low_limit, high_limit);

                    if (temp < low_limit) {
                        alarm = Alarm::Low;
                        setLED(HIGH, HIGH); // Low -> Yellow (R+G) same as original
                        beeps_left = 3;
                        beep_ms = 400;

                        buzzerOn();
                        phase = Phase::BeepOn;
                        setTimerOnce(timer_fd, beep_ms);
                    } else if (temp >= high_limit) {
                        alarm = Alarm::High;
                        setLED(HIGH, LOW); // High -> Red same as original
                        beeps_left = 3;
                        beep_ms = 80;

                        buzzerOn();
                        phase = Phase::BeepOn;
                        setTimerOnce(timer_fd, beep_ms);
                    } else {
                        alarm = Alarm::None;
                        setLED(LOW, HIGH); // Normal -> Green same as original
                        buzzerOff();

                        phase = Phase::Gap;
                        setTimerOnce(timer_fd, 200); // same as original delay(200)
                    }
                } break;

                case Phase::BeepOn: {
                    // buzzer ON finished -> OFF for same duration
                    buzzerOff();
                    phase = Phase::BeepOff;
                    setTimerOnce(timer_fd, beep_ms);
                } break;

                case Phase::BeepOff: {
                    // one beep cycle done (ON+OFF)
                    --beeps_left;
                    if (beeps_left > 0) {
                        buzzerOn();
                        phase = Phase::BeepOn;
                        setTimerOnce(timer_fd, beep_ms);
                    } else {
                        // end of 3 beeps -> emulate original trailing delay(200)
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

    // failsafe shutdown
    buzzerOff();
    setLED(HIGH, HIGH); // leave in a visible state (optional, safe)

    ::close(timer_fd);
    ::close(stop_fd);
    ::close(epoll_fd);
}