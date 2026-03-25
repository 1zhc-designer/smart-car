#include "rt/MotionCommandService.hpp"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

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
}

MotionCommandService::MotionCommandService(LocalDdsBus& bus, MotionController& controller)
    : bus_(bus), controller_(controller) {}

MotionCommandService::~MotionCommandService() {
    stop();
}

void MotionCommandService::start() {
    if (running_.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    epollFd_ = ::epoll_create1(0);
    if (epollFd_ < 0) {
        running_.store(false, std::memory_order_release);
        throwSys("epoll_create1");
    }

    timerFd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerFd_ < 0) {
        closeResources();
        running_.store(false, std::memory_order_release);
        throwSys("timerfd_create");
    }

    eventFd_ = ::eventfd(0, EFD_NONBLOCK);
    if (eventFd_ < 0) {
        closeResources();
        running_.store(false, std::memory_order_release);
        throwSys("eventfd");
    }

    epoll_event timerEvent{};
    timerEvent.events = EPOLLIN;
    timerEvent.data.fd = timerFd_;
    if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, timerFd_, &timerEvent) < 0) {
        closeResources();
        running_.store(false, std::memory_order_release);
        throwSys("epoll_ctl(timerFd)");
    }

    epoll_event commandEvent{};
    commandEvent.events = EPOLLIN;
    commandEvent.data.fd = eventFd_;
    if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, eventFd_, &commandEvent) < 0) {
        closeResources();
        running_.store(false, std::memory_order_release);
        throwSys("epoll_ctl(eventFd)");
    }

    subscription_ = bus_.subscribe<MotionCommandTopic>(
        [this](const MotionCommandTopic& topic) { onCommand(topic); });

    worker_ = std::thread(&MotionCommandService::workerLoop, this);
}

void MotionCommandService::stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    subscription_.reset();
    notifyWorker();

    if (worker_.joinable()) {
        worker_.join();
    }

    controller_.apply(Motion::Stop, 0);
    closeResources();
}

void MotionCommandService::onCommand(const MotionCommandTopic& topic) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingCommand_ = topic;
    }
    notifyWorker();
}

void MotionCommandService::notifyWorker() {
    if (eventFd_ < 0) {
        return;
    }
    const uint64_t one = 1;
    (void)::write(eventFd_, &one, sizeof(one));
}

void MotionCommandService::drainEventFd() const {
    uint64_t value = 0;
    while (true) {
        const ssize_t readSize = ::read(eventFd_, &value, sizeof(value));
        if (readSize == static_cast<ssize_t>(sizeof(value))) {
            continue;
        }
        if (readSize < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        break;
    }
}

void MotionCommandService::drainTimerFd() const {
    uint64_t expirations = 0;
    while (true) {
        const ssize_t readSize = ::read(timerFd_, &expirations, sizeof(expirations));
        if (readSize == static_cast<ssize_t>(sizeof(expirations))) {
            continue;
        }
        if (readSize < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        break;
    }
}

void MotionCommandService::armTimer(std::chrono::milliseconds duration) {
    itimerspec spec{};
    spec.it_value.tv_sec = static_cast<time_t>(duration.count() / 1000);
    spec.it_value.tv_nsec = static_cast<long>((duration.count() % 1000) * 1000'000L);
    if (::timerfd_settime(timerFd_, 0, &spec, nullptr) < 0) {
        throwSys("timerfd_settime");
    }
}

void MotionCommandService::disarmTimer() {
    itimerspec spec{};
    if (::timerfd_settime(timerFd_, 0, &spec, nullptr) < 0) {
        throwSys("timerfd_settime(disarm)");
    }
}

void MotionCommandService::applyCommand(const MotionCommandTopic& topic) {
    activeCommand_ = topic;
    controller_.apply(topic.motion, topic.speed);

    if (topic.motion == Motion::Stop || topic.duration.count() <= 0) {
        activeCommandArmed_ = false;
        disarmTimer();
        controller_.apply(Motion::Stop, 0);
        return;
    }

    activeCommandArmed_ = true;
    armTimer(topic.duration);
}

void MotionCommandService::workerLoop() {
    constexpr int kMaxEvents = 4;
    epoll_event events[kMaxEvents];

    while (running_.load(std::memory_order_acquire)) {
        const int ready = ::epoll_wait(epollFd_, events, kMaxEvents, -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        for (int index = 0; index < ready; ++index) {
            if (events[index].data.fd == eventFd_) {
                drainEventFd();

                std::optional<MotionCommandTopic> command;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    command = pendingCommand_;
                    pendingCommand_.reset();
                }

                if (command) {
                    applyCommand(*command);
                }
            } else if (events[index].data.fd == timerFd_) {
                drainTimerFd();
                if (!running_.load(std::memory_order_acquire)) {
                    break;
                }
                if (activeCommandArmed_) {
                    activeCommandArmed_ = false;
                    controller_.apply(Motion::Stop, 0);
                }
            }
        }
    }

    controller_.apply(Motion::Stop, 0);
}

void MotionCommandService::closeResources() {
    closeFd(timerFd_);
    closeFd(eventFd_);
    closeFd(epollFd_);
}
