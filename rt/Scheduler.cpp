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
inline void throwSys(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

inline void closeFd(int& fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

inline uint64_t toMicros(std::chrono::steady_clock::duration d) {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(d).count()
    );
}

inline void setTimerOnce(int timer_fd, std::chrono::milliseconds d) {
    itimerspec its{};
    its.it_value.tv_sec = static_cast<time_t>(d.count() / 1000);
    its.it_value.tv_nsec = static_cast<long>((d.count() % 1000) * 1000'000L);
    if (::timerfd_settime(timer_fd, 0, &its, nullptr) < 0) {
        throwSys("timerfd_settime");
    }
}

inline void disarmTimer(int timer_fd) {
    itimerspec its{};
    if (::timerfd_settime(timer_fd, 0, &its, nullptr) < 0) {
        throwSys("timerfd_settime(disarm)");
    }
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

inline void timerfdDrain(int timer_fd) {
    uint64_t expirations;
    (void)::read(timer_fd, &expirations, sizeof(expirations));
}
} // namespace

void Scheduler::LatencyStats::add(uint64_t us) {
    samples_us[write_idx] = us;
    write_idx = (write_idx + 1) % kLatencyBufSize;
    count++;

    min_us = std::min(min_us, us);
    max_us = std::max(max_us, us);
    sum_us += static_cast<long double>(us);
}

void Scheduler::LatencyStats::snapshot(std::array<uint64_t, kLatencyBufSize>& out,
                                      std::size_t& valid,
                                      std::size_t& total_count,
                                      uint64_t& min_out,
                                      uint64_t& max_out,
                                      long double& sum_out) const {
    out = samples_us;
    total_count = count;
    valid = std::min<std::size_t>(count, kLatencyBufSize);
    min_out = (count == 0) ? 0 : min_us;
    max_out = (count == 0) ? 0 : max_us;
    sum_out = sum_us;
}

Scheduler::Scheduler(MotionController& mc) : mc_(mc) {}

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::start() {
    if (running_) return;
    running_ = true;

    epoll_fd_ = ::epoll_create1(0);
    if (epoll_fd_ < 0) throwSys("epoll_create1");

    timer_fd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timer_fd_ < 0) throwSys("timerfd_create");

    event_fd_ = ::eventfd(0, EFD_NONBLOCK);
    if (event_fd_ < 0) throwSys("eventfd");

    // Add timer_fd_ to epoll
    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = timer_fd_;
        if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, timer_fd_, &ev) < 0) {
            throwSys("epoll_ctl(add timer_fd)");
        }
    }

    // Add event_fd_ to epoll
    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = event_fd_;
        if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, event_fd_, &ev) < 0) {
            throwSys("epoll_ctl(add event_fd)");
        }
    }

    worker_ = std::thread(&Scheduler::workerLoop, this);
}

void Scheduler::stop() {
    if (!running_) return;
    running_ = false;

    // Wake the worker so epoll_wait can return and exit.
    if (event_fd_ >= 0) eventfdNotify(event_fd_);

    if (worker_.joinable()) worker_.join();

    // Safety stop on exit.
    mc_.apply(Motion::Stop, 0);

    closeFd(timer_fd_);
    closeFd(event_fd_);
    closeFd(epoll_fd_);

    printLatencySummary();
}

void Scheduler::enqueue(const MotionTask& task) {
    QueuedTask qt{task, Clock::now()};
    {
        std::lock_guard<std::mutex> lk(mtx_);
        q_.push_back(qt);
    }
    if (event_fd_ >= 0) eventfdNotify(event_fd_);
}

void Scheduler::clear() {
    std::lock_guard<std::mutex> lk(mtx_);
    q_.clear();
}

void Scheduler::replaceNow(const MotionTask& task) {
    QueuedTask qt{task, Clock::now()};
    {
        std::lock_guard<std::mutex> lk(mtx_);
        q_.clear();
        q_.push_front(qt);
    }
    // Notify worker to preempt immediately.
    if (event_fd_ >= 0) eventfdNotify(event_fd_);
}

void Scheduler::recordLatencyUs(uint64_t us) {
    std::lock_guard<std::mutex> lk(stats_mtx_);
    latency_.add(us);
}

void Scheduler::startNextTask() {
    QueuedTask next{};
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (q_.empty()) return;
        next = q_.front();
        q_.pop_front();
    }

    current_ = next.task;
    task_active_ = true;

    // Measure enqueue -> actual apply latency
    const auto now = Clock::now();
    const uint64_t lat_us = toMicros(now - next.enqueue_ts);
    recordLatencyUs(lat_us);

    mc_.apply(current_.motion, current_.speed);
    setTimerOnce(timer_fd_, current_.duration);
}

void Scheduler::workerLoop() {
    constexpr int kMaxEvents = 4;
    epoll_event events[kMaxEvents];

    // If tasks were queued before start(), kick off immediately.
    startNextTask();

    while (running_) {
        int n = ::epoll_wait(epoll_fd_, events, kMaxEvents, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < n; ++i) {
            const int fd = events[i].data.fd;

            if (fd == event_fd_) {
                // A new task arrived (or stop requested). Preempt immediately.
                eventfdDrain(event_fd_);

                // If a task is currently active, cancel its timer so it does not "win later".
                if (task_active_) {
                    disarmTimer(timer_fd_);
                    task_active_ = false;
                }

                // Start the newest queued task (if any).
                startNextTask();

            } else if (fd == timer_fd_) {
                // Current task duration elapsed.
                timerfdDrain(timer_fd_);
                if (task_active_) {
                    mc_.apply(Motion::Stop, 0);
                    task_active_ = false;
                    startNextTask();
                }
            }
        }
    }

    mc_.apply(Motion::Stop, 0);
}

void Scheduler::printLatencySummary() {
    std::array<uint64_t, kLatencyBufSize> buf{};
    std::size_t valid = 0;
    std::size_t total_count = 0;
    uint64_t min_us = 0;
    uint64_t max_us = 0;
    long double sum_us = 0.0L;

    {
        std::lock_guard<std::mutex> lk(stats_mtx_);
        latency_.snapshot(buf, valid, total_count, min_us, max_us, sum_us);
    }

    if (total_count == 0 || valid == 0) {
        std::cout << "[Scheduler Latency] no samples collected\n";
        return;
    }

    std::vector<uint64_t> sorted(buf.begin(), buf.begin() + valid);
    std::sort(sorted.begin(), sorted.end());

    auto pct = [&](double p) -> uint64_t {
        const std::size_t idx = static_cast<std::size_t>(p * (sorted.size() - 1));
        return sorted[idx];
    };

    const long double mean = sum_us / static_cast<long double>(total_count);

    std::cout << "[Scheduler Latency us] samples=" << total_count
              << " min=" << min_us
              << " p50=" << pct(0.50)
              << " p90=" << pct(0.90)
              << " p99=" << pct(0.99)
              << " max=" << max_us
              << " mean=" << static_cast<double>(mean)
              << "\n";
}