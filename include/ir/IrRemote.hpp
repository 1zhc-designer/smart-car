#pragma once

#include "rt/Scheduler.hpp"
#include <atomic>
#include <chrono>
#include <thread>

struct lirc_config;

class GimbalService;

class IrRemote final {
public:
    IrRemote(Scheduler& sched, GimbalService& gimbal);
    ~IrRemote();

    IrRemote(const IrRemote&) = delete;
    IrRemote& operator=(const IrRemote&) = delete;

    void start();
    void stop();

private:
    void loop();
    void handleCode(const char* code);

    Scheduler& sched_;
    GimbalService& gimbal_;

    std::atomic<bool> running_{false};
    std::thread th_;

    lirc_config* config_{nullptr};

    Motion last_motion_{Motion::Stop};
    std::chrono::steady_clock::time_point last_motion_ts_{};
    std::chrono::steady_clock::time_point last_gimbal_ts_{};

    static constexpr auto kMotionDebounce = std::chrono::milliseconds(30);
    static constexpr auto kGimbalDebounce = std::chrono::milliseconds(400);
};