#include "ir/IrRemote.hpp"

#include "gimbal/GimbalService.hpp"

#include <lirc/lirc_client.h>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <chrono>

namespace {
static const char* keymap[21] = {
    " KEY_CHANNELDOWN ",
    " KEY_CHANNEL ",
    " KEY_CHANNELUP ",
    " KEY_PREVIOUS ",
    " KEY_NEXT ",
    " KEY_PLAYPAUSE ",
    " KEY_VOLUMEDOWN ",
    " KEY_VOLUMEUP ",
    " KEY_EQUAL ",
    " KEY_NUMERIC_0 ",
    " BTN_0 ",
    " BTN_1 ",
    " KEY_NUMERIC_1 ",
    " KEY_NUMERIC_2 ",
    " KEY_NUMERIC_3 ",
    " KEY_NUMERIC_4 ",
    " KEY_NUMERIC_5 ",
    " KEY_NUMERIC_6 ",
    " KEY_NUMERIC_7 ",
    " KEY_NUMERIC_8 ",
    " KEY_NUMERIC_9 "
};

inline bool contains(const char* s, const char* sub) {
    return s && sub && std::strstr(s, sub);
}

static constexpr int kSpeedForward = 50;
static constexpr int kSpeedTurn = 70;

static const std::chrono::milliseconds kContinuous =
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(24));

static constexpr auto kStopDur = std::chrono::milliseconds(10);

inline bool decodeMotion(const char* code, Motion& m, int& speed, std::chrono::milliseconds& dur) {
    m = Motion::Stop;
    speed = 0;
    dur = kStopDur;

    if (contains(code, keymap[1])) {
        m = Motion::Up;
        speed = kSpeedForward;
        dur = kContinuous;
        return true;
    }
    if (contains(code, keymap[7])) {
        m = Motion::Down;
        speed = kSpeedForward;
        dur = kContinuous;
        return true;
    }
    if (contains(code, keymap[3])) {
        m = Motion::Left;
        speed = kSpeedTurn;
        dur = kContinuous;
        return true;
    }
    if (contains(code, keymap[5])) {
        m = Motion::Right;
        speed = kSpeedTurn;
        dur = kContinuous;
        return true;
    }
    if (contains(code, keymap[4])) {
        m = Motion::Stop;
        speed = 0;
        dur = kStopDur;
        return true;
    }
    return false;
}

enum class GimbalCommand {
    None,
    TiltUp,
    TiltDown,
    PanLeft,
    PanRight,
    Reset
};

inline GimbalCommand decodeGimbal(const char* code) {
    if (contains(code, " KEY_NUMERIC_2 ")) return GimbalCommand::TiltUp;
    if (contains(code, " KEY_NUMERIC_8 ")) return GimbalCommand::TiltDown;
    if (contains(code, " KEY_NUMERIC_4 ")) return GimbalCommand::PanLeft;
    if (contains(code, " KEY_NUMERIC_6 ")) return GimbalCommand::PanRight;
    if (contains(code, " KEY_NUMERIC_5 ")) return GimbalCommand::Reset;
    return GimbalCommand::None;
}
}  // namespace

IrRemote::IrRemote(Scheduler& sched, GimbalService& gimbal)
    : sched_(sched), gimbal_(gimbal) {}

IrRemote::~IrRemote() {
    stop();
}

void IrRemote::start() {
    if (running_) return;

    if (lirc_init("ircontrol", 1) == -1) {
        throw std::runtime_error("lirc_init failed (is lircd running and permissions correct?)");
    }

    if (lirc_readconfig(nullptr, &config_, nullptr) != 0) {
        lirc_deinit();
        config_ = nullptr;
        throw std::runtime_error("lirc_readconfig failed");
    }

    last_motion_ = Motion::Stop;
    last_motion_ts_ = std::chrono::steady_clock::now();
    last_gimbal_ts_ = std::chrono::steady_clock::now();

    running_ = true;
    th_ = std::thread(&IrRemote::loop, this);
}

void IrRemote::stop() {
    if (!running_) return;

    running_ = false;

    if (th_.joinable()) th_.join();

    if (config_) {
        lirc_freeconfig(config_);
        config_ = nullptr;
    }
    lirc_deinit();

    sched_.replaceNow({Motion::Stop, 0, kStopDur});
}

void IrRemote::loop() {
    while (running_) {
        char* code = nullptr;

        if (lirc_nextcode(&code) != 0) {
            break;
        }

        if (!running_) {
            if (code) std::free(code);
            break;
        }

        if (code == nullptr) {
            continue;
        }

        handleCode(code);
        std::free(code);
    }
}

void IrRemote::handleCode(const char* code) {
    const auto now = std::chrono::steady_clock::now();

    switch (decodeGimbal(code)) {
        case GimbalCommand::TiltUp:
            if ((now - last_gimbal_ts_) >= kGimbalDebounce) {
                gimbal_.tiltUp();
                last_gimbal_ts_ = now;
            }
            return;
        case GimbalCommand::TiltDown:
            if ((now - last_gimbal_ts_) >= kGimbalDebounce) {
                gimbal_.tiltDown();
                last_gimbal_ts_ = now;
            }
            return;
        case GimbalCommand::PanLeft:
            if ((now - last_gimbal_ts_) >= kGimbalDebounce) {
                gimbal_.panLeft();
                last_gimbal_ts_ = now;
            }
            return;
        case GimbalCommand::PanRight:
            if ((now - last_gimbal_ts_) >= kGimbalDebounce) {
                gimbal_.panRight();
                last_gimbal_ts_ = now;
            }
            return;
        case GimbalCommand::Reset:
            if ((now - last_gimbal_ts_) >= kGimbalDebounce) {
                gimbal_.reset();
                last_gimbal_ts_ = now;
            }
            return;
        case GimbalCommand::None:
            break;
    }

    Motion m;
    int speed;
    std::chrono::milliseconds dur;

    if (!decodeMotion(code, m, speed, dur)) return;

    const bool same_motion = (m == last_motion_);
    if ((now - last_motion_ts_) < kMotionDebounce) {
        if (same_motion) return;
    }

    last_motion_ts_ = now;
    last_motion_ = m;

    sched_.replaceNow({m, speed, dur});
}