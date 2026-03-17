#include "rt/Scheduler.hpp"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

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

uint64_t toMicros(std::chrono::steady_clock::duration d) {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(d).count());
}

void setTimerOnce(int timerFd, std::chrono::milliseconds d) {
    itimerspec its{};
    its.it_value.tv_sec = static_cast<time_t>(d.count() / 1000);
    its.it_value.tv_nsec = static_cast<long>((d.count() % 1000) * 1000'000L);
    if (::timerfd_settime(timerFd, 0, &its, nullptr) < 0) {
        throwSys("timerfd_settime");
    }
}

void disarmTimer(int timerFd) {
    itimerspec its{};
    if (::timerfd_settime(timerFd, 0, &its, nullptr) < 0) {
        throwSys("timerfd_settime(disarm)");
    }
}

void notifyEvent(int eventFd) {
    uint64_t one = 1;
    (void)::write(eventFd, &one, sizeof(one));
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

void drainTimerfd(int timerFd) {
    uint64_t expirations = 0;
    (void)::read(timerFd, &expirations, sizeof(expirations));
}
} // namespace

void Scheduler::LatencyStats::add(uint64_t us) {
    samplesUs[writeIdx] = us;
    writeIdx = (writeIdx + 1) % kLatencyBufSize;
    ++count;
    minUs = std::min(minUs, us);
    maxUs = std::max(maxUs, us);
    sumUs += static_cast<long double>(us);
}

void Scheduler::LatencyStats::snapshot(std::array<uint64_t, kLatencyBufSize>& out,
                                       std::size_t& valid,
                                       std::size_t& totalCount,
                                       uint64_t& minOut,
                                       uint64_t& maxOut,
                                       long double& sumOut) const {
    out = samplesUs;
    totalCount = count;
    valid = std::min<std::size_t>(count, kLatencyBufSize);
    minOut = (count == 0) ? 0 : minUs;
    maxOut = (count == 0) ? 0 : maxUs;
    sumOut = sumUs;
}

Scheduler::Scheduler(MotionController& mc) : mc_(mc) {}

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::start() {
    if (running_) {
        return;
    }
    running_ = true;

    epollFd_ = ::epoll_create1(0);
    if (epollFd_ < 0) {
        throwSys("epoll_create1");
    }

    timerFd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerFd_ < 0) {
        throwSys("timerfd_create");
    }

    eventFd_ = ::eventfd(0, EFD_NONBLOCK);
    if (eventFd_ < 0) {
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
        ev.data.fd = eventFd_;
        if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, eventFd_, &ev) < 0) {
            throwSys("epoll_ctl(eventFd)");
        }
    }

    worker_ = std::thread(&Scheduler::workerLoop, this);
}

void Scheduler::stop() {
    if (!running_) {
        return;
    }
    running_ = false;

    if (eventFd_ >= 0) {
        notifyEvent(eventFd_);
    }

    if (worker_.joinable()) {
        worker_.join();
    }

    mc_.apply(Motion::Stop, 0);
    closeFd(timerFd_);
    closeFd(eventFd_);
    closeFd(epollFd_);
    printLatencySummary();
}

void Scheduler::enqueue(const MotionTask& task) {
    QueuedTask qt{task, Clock::now()};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(qt);
    }
    if (eventFd_ >= 0) {
        notifyEvent(eventFd_);
    }
}

void Scheduler::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
}

void Scheduler::replaceNow(const MotionTask& task) {
    QueuedTask qt{task, Clock::now()};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
        queue_.push_front(qt);
    }
    if (eventFd_ >= 0) {
        notifyEvent(eventFd_);
    }
}

void Scheduler::recordLatencyUs(uint64_t us) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    latency_.add(us);
}

void Scheduler::startNextTask() {
    QueuedTask next{};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return;
        }
        next = queue_.front();
        queue_.pop_front();
    }

    current_ = next.task;
    taskActive_ = true;

    const auto now = Clock::now();
    recordLatencyUs(toMicros(now - next.enqueueTs));

    mc_.apply(current_.motion, current_.speed);
    setTimerOnce(timerFd_, current_.duration);
}

void Scheduler::workerLoop() {
    constexpr int kMaxEvents = 4;
    epoll_event events[kMaxEvents];

    startNextTask();

    while (running_) {
        const int n = ::epoll_wait(epollFd_, events, kMaxEvents, -1);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        for (int i = 0; i < n; ++i) {
            const int fd = events[i].data.fd;

            if (fd == eventFd_) {
                drainEventfd(eventFd_);
                if (taskActive_) {
                    disarmTimer(timerFd_);
                    taskActive_ = false;
                }
                startNextTask();
            } else if (fd == timerFd_) {
                drainTimerfd(timerFd_);
                if (taskActive_) {
                    mc_.apply(Motion::Stop, 0);
                    taskActive_ = false;
                    startNextTask();
                }
            }
        }
    }

    mc_.apply(Motion::Stop, 0);
}

void Scheduler::printLatencySummary() {
    std::array<uint64_t, kLatencyBufSize> buffer{};
    std::size_t valid = 0;
    std::size_t totalCount = 0;
    uint64_t minUs = 0;
    uint64_t maxUs = 0;
    long double sumUs = 0.0L;

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        latency_.snapshot(buffer, valid, totalCount, minUs, maxUs, sumUs);
    }

    if (totalCount == 0 || valid == 0) {
        std::cout << "[Scheduler Latency] no samples collected\n";
        return;
    }

    std::vector<uint64_t> sorted(buffer.begin(), buffer.begin() + valid);
    std::sort(sorted.begin(), sorted.end());

    const auto percentile = [&](double p) -> uint64_t {
        const std::size_t idx = static_cast<std::size_t>(p * (sorted.size() - 1));
        return sorted[idx];
    };

    const long double mean = sumUs / static_cast<long double>(totalCount);
    std::cout << "[Scheduler Latency us] samples=" << totalCount
              << " min=" << minUs
              << " p50=" << percentile(0.50)
              << " p90=" << percentile(0.90)
              << " p99=" << percentile(0.99)
              << " max=" << maxUs
              << " mean=" << static_cast<double>(mean)
              << '\n';
}
