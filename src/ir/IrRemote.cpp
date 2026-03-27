#include "ir/IrRemote.hpp"

#include <lirc/lirc_client.h>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
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

void drainEventfd(int eventFd) {
    uint64_t value = 0;
    while (true) {
        const ssize_t readSize = ::read(eventFd, &value, sizeof(value));
        if (readSize == static_cast<ssize_t>(sizeof(value))) {
            continue;
        }
        if (readSize < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        break;
    }
}

constexpr int kSpeedForward = 50;
constexpr int kSpeedTurn = 70;
constexpr auto kContinuous = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(24));
constexpr auto kStopDuration = std::chrono::milliseconds(10);

bool decodeMotion(const char* code, MotionCommandTopic& topic) {
    topic.motion = Motion::Stop;
    topic.speed = 0;
    topic.duration = kStopDuration;
    topic.source = "ir";

    if (contains(code, kKeyMotionForward)) {
        topic.motion = Motion::Up;
        topic.speed = kSpeedForward;
        topic.duration = kContinuous;
        return true;
    }
    if (contains(code, kKeyMotionBackward)) {
        topic.motion = Motion::Down;
        topic.speed = kSpeedForward;
        topic.duration = kContinuous;
        return true;
    }
    if (contains(code, kKeyMotionLeft)) {
        topic.motion = Motion::Left;
        topic.speed = kSpeedTurn;
        topic.duration = kContinuous;
        return true;
    }
    if (contains(code, kKeyMotionRight)) {
        topic.motion = Motion::Right;
        topic.speed = kSpeedTurn;
        topic.duration = kContinuous;
        return true;
    }
    if (contains(code, kKeyStop)) {
        topic.motion = Motion::Stop;
        topic.speed = 0;
        topic.duration = kStopDuration;
        return true;
    }
    return false;
}

bool decodeGimbal(const char* code, GimbalCommandTopic& topic) {
    topic.source = "ir";
    if (contains(code, " KEY_NUMERIC_2 ")) {
        topic.command = GimbalCommand::TiltUp;
        return true;
    }
    if (contains(code, " KEY_NUMERIC_8 ")) {
        topic.command = GimbalCommand::TiltDown;
        return true;
    }
    if (contains(code, " KEY_NUMERIC_4 ")) {
        topic.command = GimbalCommand::PanLeft;
        return true;
    }
    if (contains(code, " KEY_NUMERIC_6 ")) {
        topic.command = GimbalCommand::PanRight;
        return true;
    }
    if (contains(code, " KEY_NUMERIC_5 ")) {
        topic.command = GimbalCommand::Reset;
        return true;
    }
    return false;
}
} // namespace

IrRemote::IrRemote(LocalDdsBus& bus) : bus_(bus) {}

IrRemote::~IrRemote() {
    stop();
}

void IrRemote::setCommandCallback(CommandCallback callback) {
    commandCallback_ = std::move(callback);
}

void IrRemote::start() {
    if (running_.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    lircFd_ = lirc_init("ircontrol", 0);
    if (lircFd_ == -1) {
        running_.store(false, std::memory_order_release);
        throw std::runtime_error("lirc_init failed");
    }

    if (lirc_readconfig(nullptr, &config_, nullptr) != 0) {
        lirc_deinit();
        config_ = nullptr;
        lircFd_ = -1;
        running_.store(false, std::memory_order_release);
        throw std::runtime_error("lirc_readconfig failed");
    }

    stopFd_ = ::eventfd(0, EFD_NONBLOCK);
    if (stopFd_ < 0) {
        stop();
        throw std::runtime_error("eventfd failed for IR remote");
    }

    epollFd_ = ::epoll_create1(0);
    if (epollFd_ < 0) {
        stop();
        throw std::runtime_error("epoll_create1 failed for IR remote");
    }

    epoll_event irEvent{};
    irEvent.events = EPOLLIN;
    irEvent.data.fd = lircFd_;
    if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, lircFd_, &irEvent) < 0) {
        stop();
        throw std::runtime_error("epoll_ctl add lircFd failed");
    }

    epoll_event stopEvent{};
    stopEvent.events = EPOLLIN;
    stopEvent.data.fd = stopFd_;
    if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, stopFd_, &stopEvent) < 0) {
        stop();
        throw std::runtime_error("epoll_ctl add stopFd failed");
    }

    lastMotion_ = Motion::Stop;
    lastMotionTs_ = std::chrono::steady_clock::now();
    lastGimbalTs_ = std::chrono::steady_clock::now();

    thread_ = std::thread(&IrRemote::loop, this);
}

void IrRemote::stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    if (stopFd_ >= 0) {
        const uint64_t one = 1;
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

    bus_.publish(MotionCommandTopic{Motion::Stop, 0, kStopDuration, "ir-stop"});
}

void IrRemote::emitCommand(const std::string& text) {
    if (commandCallback_) {
        commandCallback_(text);
    }
}

void IrRemote::loop() {
    epoll_event events[4];

    while (running_.load(std::memory_order_acquire)) {
        const int ready = ::epoll_wait(epollFd_, events, 4, -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        for (int index = 0; index < ready; ++index) {
            const int fd = events[index].data.fd;
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

    GimbalCommandTopic gimbalTopic{};
    if (decodeGimbal(code, gimbalTopic)) {
        if ((now - lastGimbalTs_) >= kGimbalDebounce) {
            lastGimbalTs_ = now;
            bus_.publish(gimbalTopic);
            emitCommand("gimbal_command");
        }
        return;
    }

    MotionCommandTopic motionTopic{};
    if (!decodeMotion(code, motionTopic)) {
        return;
    }

    const bool sameMotion = (motionTopic.motion == lastMotion_);
    if ((now - lastMotionTs_) < kMotionDebounce && sameMotion) {
        return;
    }

    lastMotionTs_ = now;
    lastMotion_ = motionTopic.motion;
    bus_.publish(motionTopic);
    emitCommand("motion_command");
}
