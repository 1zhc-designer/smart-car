#pragma once

#include "dds/LocalDdsBus.hpp"
#include "dds/VehicleTopics.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

struct lirc_config;

/**
 * @brief Infrared remote controller using blocking epoll + LIRC.
 *
 * The IR service no longer knows about a global scheduler. It only publishes
 * typed topics to the DDS-style bus.
 */
class IrRemote final {
public:
    using CommandCallback = std::function<void(const std::string&)>;

    explicit IrRemote(LocalDdsBus& bus);
    ~IrRemote();

    IrRemote(const IrRemote&) = delete;
    IrRemote& operator=(const IrRemote&) = delete;

    void start();
    void stop();
    void setCommandCallback(CommandCallback callback);

private:
    void loop();
    void handleCode(const char* code);
    void emitCommand(const std::string& text);

    LocalDdsBus& bus_;
    std::atomic<bool> running_{false};
    std::thread thread_{};

    lirc_config* config_{nullptr};
    int lircFd_{-1};
    int epollFd_{-1};
    int stopFd_{-1};

    Motion lastMotion_{Motion::Stop};
    std::chrono::steady_clock::time_point lastMotionTs_{};
    std::chrono::steady_clock::time_point lastGimbalTs_{};
    CommandCallback commandCallback_{};

    static constexpr int kSpeedForward = 50;
    static constexpr int kSpeedTurn = 70;
    static constexpr auto kContinuous = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(24));
    static constexpr auto kStopDuration = std::chrono::milliseconds(10);
    static constexpr auto kMotionDebounce = std::chrono::milliseconds(30);
    static constexpr auto kGimbalDebounce = std::chrono::milliseconds(250);
};
