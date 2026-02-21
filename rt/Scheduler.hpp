#pragma once
#include "motion/MotionController.hpp"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>

struct MotionTask {
    Motion motion;
    int speed; // 0..100
    std::chrono::milliseconds duration;
};

class Scheduler {
public:
    explicit Scheduler(MotionController& mc);
    ~Scheduler();

    void start();
    void stop();

    // event: push task then wake worker thread
    void enqueue(const MotionTask& task);
    void clear();

private:
    void workerLoop();

    MotionController& mc_;
    std::thread worker_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<MotionTask> q_;
    std::atomic<bool> running_{false};
};