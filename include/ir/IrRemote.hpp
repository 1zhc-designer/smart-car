#pragma once

#include "rt/Scheduler.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

struct lirc_config;
class GimbalService;

/**
 * @brief Infrared remote controller using blocking epoll + LIRC.
 */
class IrRemote final {
public:
    using CommandCallback = std::function<void(const std::string&)>;

    IrRemote(Scheduler& sched, GimbalService& gimbal);
    ~IrRemote();

    IrRemote(const IrRemote&) = delete;
    IrRemote& operator=(const IrRemote&) = delete;

    void start();
    void stop();
    void setCommandCallback(CommandCallback cb);

private:
    void loop();
    void handleCode(const char* code);
    void emitCommand(const std::string& text);

    Scheduler& sched_;
    GimbalService& gimbal_;

    std::atomic<bool> running_{false};
    std::thread thread_;

    lirc_config* config_{nullptr};
    int lircFd_{-1};
    int epollFd_{-1};
    int stopFd_{-1};

    Motion lastMotion_{Motion::Stop};
    std::chrono::steady_clock::time_point lastMotionTs_{};
    std::chrono::steady_clock::time_point lastGimbalTs_{};
    CommandCallback commandCallback_;

    static constexpr auto kMotionDebounce = std::chrono::milliseconds(30);
    static constexpr auto kGimbalDebounce = std::chrono::milliseconds(250);
};
