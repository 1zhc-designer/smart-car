#include "ir/IrRemote.hpp"

#include <cerrno>
#include "gimbal/GimbalService.hpp"

#include <lirc/lirc_client.h>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {
const char* kKeyMotionForward = " KEY_CHANNEL ";
const char* kKeyMotionBackward = " KEY_VOLUMEUP ";
const char* kKeyMotionLeft = " KEY_PREVIOUS ";
const char* kKeyMotionRight = " KEY_PLAYPAUSE ";
const char* kKeyStop = " KEY_NEXT ";

bool contains(const char* src, const char* needle) {
    return src && needle && std::strstr(src, needle);
}

constexpr int kSpeedForward = 50;
constexpr int kSpeedTurn = 70;
const std::chrono::milliseconds kContinuous = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(24));
constexpr auto kStopDur = std::chrono::milliseconds(10);

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

enum class GimbalCommand {
    None,
    TiltUp,
    TiltDown,
    PanLeft,
    PanRight,
    Reset
};

bool decodeMotion(const char* code, Motion& motion, int& speed, std::chrono::milliseconds& duration) {
    motion = Motion::Stop;
    speed = 0;
    duration = kStopDur;

    if (contains(code, kKeyMotionForward)) {
        motion = Motion::Up;
        speed = kSpeedForward;
        duration = kContinuous;
        return true;
    }
    if (contains(code, kKeyMotionBackward)) {
        motion = Motion::Down;
        speed = kSpeedForward;
        duration = kContinuous;
        return true;
    }
    if (contains(code, kKeyMotionLeft)) {
        motion = Motion::Left;
        speed = kSpeedTurn;
        duration = kContinuous;
        return true;
    }
    if (contains(code, kKeyMotionRight)) {
        motion = Motion::Right;
        speed = kSpeedTurn;
        duration = kContinuous;
        return true;
    }
    if (contains(code, kKeyStop)) {
        motion = Motion::Stop;
        speed = 0;
        duration = kStopDur;
        return true;
    }
    return false;
}

GimbalCommand decodeGimbal(const char* code) {
    if (contains(code, " KEY_NUMERIC_2 ")) return GimbalCommand::TiltUp;
    if (contains(code, " KEY_NUMERIC_8 ")) return GimbalCommand::TiltDown;
    if (contains(code, " KEY_NUMERIC_4 ")) return GimbalCommand::PanLeft;
    if (contains(code, " KEY_NUMERIC_6 ")) return GimbalCommand::PanRight;
    if (contains(code, " KEY_NUMERIC_5 ")) return GimbalCommand::Reset;
    return GimbalCommand::None;
}
} // namespace

IrRemote::IrRemote(Scheduler& sched, GimbalService& gimbal) : sched_(sched), gimbal_(gimbal) {}

IrRemote::~IrRemote() {
    stop();
}

void IrRemote::setCommandCallback(CommandCallback cb) {
    commandCallback_ = std::move(cb);
}

void IrRemote::start() {
    if (running_) {
        return;
    }

    lircFd_ = lirc_init("ircontrol", 0);
    if (lircFd_ == -1) {
        throw std::runtime_error("lirc_init failed");
    }

    if (lirc_readconfig(nullptr, &config_, nullptr) != 0) {
        lirc_deinit();
        config_ = nullptr;
        throw std::runtime_error("lirc_readconfig failed");
    }

    stopFd_ = ::eventfd(0, EFD_NONBLOCK);
    if (stopFd_ < 0) {
        throw std::runtime_error("eventfd failed for IR remote");
    }

    epollFd_ = ::epoll_create1(0);
    if (epollFd_ < 0) {
        throw std::runtime_error("epoll_create1 failed for IR remote");
    }

    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = lircFd_;
        if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, lircFd_, &ev) < 0) {
            throw std::runtime_error("epoll_ctl add lircFd failed");
        }
    }
    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = stopFd_;
        if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, stopFd_, &ev) < 0) {
            throw std::runtime_error("epoll_ctl add stopFd failed");
        }
    }

    lastMotion_ = Motion::Stop;
    lastMotionTs_ = std::chrono::steady_clock::now();
    lastGimbalTs_ = std::chrono::steady_clock::now();

    running_ = true;
    thread_ = std::thread(&IrRemote::loop, this);
}

void IrRemote::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    if (stopFd_ >= 0) {
        uint64_t one = 1;
        (void)::write(stopFd_, &one, sizeof(one));
    }

    if (thread_.joinable()) {
        thread_.join();
    }

    if (epollFd_ >= 0) {
        ::close(epollFd_);
        epollFd_ = -1;
    }
    if (stopFd_ >= 0) {
        ::close(stopFd_);
        stopFd_ = -1;
    }
    if (config_) {
        lirc_freeconfig(config_);
        config_ = nullptr;
    }
    if (lircFd_ >= 0) {
        lirc_deinit();
        lircFd_ = -1;
    }

    sched_.replaceNow({Motion::Stop, 0, kStopDur});
}

void IrRemote::emitCommand(const std::string& text) {
    if (commandCallback_) {
        commandCallback_(text);
    }
}

void IrRemote::loop() {
    epoll_event events[4];

    while (running_) {
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
            if (fd != lircFd_) {
                continue;
            }

            char* code = nullptr;
            if (lirc_nextcode(&code) != 0) {
                continue;
            }
            if (!code) {
                continue;
            }
            handleCode(code);
            std::free(code);
        }
    }
}

void IrRemote::handleCode(const char* code) {
    const auto now = std::chrono::steady_clock::now();

    switch (decodeGimbal(code)) {
    case GimbalCommand::TiltUp:
        if ((now - lastGimbalTs_) >= kGimbalDebounce) {
            gimbal_.tiltUp();
            lastGimbalTs_ = now;
            emitCommand("gimbal_up");
        }
        return;
    case GimbalCommand::TiltDown:
        if ((now - lastGimbalTs_) >= kGimbalDebounce) {
            gimbal_.tiltDown();
            lastGimbalTs_ = now;
            emitCommand("gimbal_down");
        }
        return;
    case GimbalCommand::PanLeft:
        if ((now - lastGimbalTs_) >= kGimbalDebounce) {
            gimbal_.panLeft();
            lastGimbalTs_ = now;
            emitCommand("gimbal_left");
        }
        return;
    case GimbalCommand::PanRight:
        if ((now - lastGimbalTs_) >= kGimbalDebounce) {
            gimbal_.panRight();
            lastGimbalTs_ = now;
            emitCommand("gimbal_right");
        }
        return;
    case GimbalCommand::Reset:
        if ((now - lastGimbalTs_) >= kGimbalDebounce) {
            gimbal_.reset();
            lastGimbalTs_ = now;
            emitCommand("gimbal_reset");
        }
        return;
    case GimbalCommand::None:
        break;
    }

    Motion motion;
    int speed = 0;
    std::chrono::milliseconds duration{0};
    if (!decodeMotion(code, motion, speed, duration)) {
        return;
    }

    const bool sameMotion = (motion == lastMotion_);
    if ((now - lastMotionTs_) < kMotionDebounce && sameMotion) {
        return;
    }

    lastMotionTs_ = now;
    lastMotion_ = motion;
    sched_.replaceNow({motion, speed, duration});
    emitCommand("motion_command");
}
