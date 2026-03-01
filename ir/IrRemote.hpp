#pragma once

#include "rt/Scheduler.hpp"
#include <atomic>
#include <chrono>
#include <thread>

struct lirc_config; // Forward declaration from lirc

class IrRemote final {
public:
    explicit IrRemote(Scheduler& sched);
    ~IrRemote();

    IrRemote(const IrRemote&) = delete;
    IrRemote& operator=(const IrRemote&) = delete;

    void start();
    void stop();

private:
    void loop();
    void handleCode(const char* code);

    Scheduler& sched_;
    std::atomic<bool> running_{false};
    std::thread th_;

    lirc_config* config_{nullptr};

    // Debounce state
    Motion last_motion_{Motion::Stop};
    std::chrono::steady_clock::time_point last_ts_{};

    // Small debounce for better responsiveness
    static constexpr auto kDebounce = std::chrono::milliseconds(30);
};