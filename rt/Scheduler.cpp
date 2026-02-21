#include "rt/Scheduler.hpp"

Scheduler::Scheduler(MotionController& mc) : mc_(mc) {}

Scheduler::~Scheduler() { stop(); }

void Scheduler::start() {
    if (running_) return;
    running_ = true;
    worker_ = std::thread(&Scheduler::workerLoop, this);
}

void Scheduler::stop() {
    if (!running_) return;
    running_ = false;
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
    // failsafe stop
    mc_.apply(Motion::Stop, 0);
}

void Scheduler::enqueue(const MotionTask& task) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        q_.push_back(task);
    }
    cv_.notify_one(); // wake thread on new event
}

void Scheduler::clear() {
    std::lock_guard<std::mutex> lk(mtx_);
    q_.clear();
}

void Scheduler::workerLoop() {
    std::unique_lock<std::mutex> lk(mtx_);
    while (running_) {
        // wait until there is work or stop signal
        cv_.wait(lk, [&]{ return !running_ || !q_.empty(); });
        if (!running_) break;
        auto task = q_.front();
        q_.pop_front();

        // release lock while executing motion (so enqueue doesn't block)
        lk.unlock();

        mc_.apply(task.motion, task.speed);
        // timed wait (not busy-loop); in a bigger project you can replace with timerfd/epoll
        std::this_thread::sleep_for(task.duration);
        mc_.apply(Motion::Stop, 0);

        lk.lock();
    }
}