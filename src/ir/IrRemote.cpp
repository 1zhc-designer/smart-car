#include "ir/IrRemote.hpp"

#include <lirc/lirc_client.h>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <chrono>

namespace {
// Original key strings (kept identical for strstr matching)
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

// Tunables (same as your original intent)
static constexpr int kSpeedForward = 50;
static constexpr int kSpeedTurn    = 70;

// Long duration is fine now because Scheduler is preemptive.
// It will be interrupted immediately by replaceNow() when a new command arrives.
static const std::chrono::milliseconds kContinuous =
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(24));

static constexpr auto kStopDur = std::chrono::milliseconds(10);

// Mapping matches your original C: [1]=up, [7]=down, [3]=left, [5]=right, [4]=stop
inline bool decodeMotion(const char* code, Motion& m, int& speed, std::chrono::milliseconds& dur) {
    m = Motion::Stop;
    speed = 0;
    dur = kStopDur;

    if (contains(code, keymap[1])) {         // Up
        m = Motion::Up;
        speed = kSpeedForward;
        dur = kContinuous;
        return true;
    }
    if (contains(code, keymap[7])) {         // Down
        m = Motion::Down;
        speed = kSpeedForward;
        dur = kContinuous;
        return true;
    }
    if (contains(code, keymap[3])) {         // Left
        m = Motion::Left;
        speed = kSpeedTurn;
        dur = kContinuous;
        return true;
    }
    if (contains(code, keymap[5])) {         // Right
        m = Motion::Right;
        speed = kSpeedTurn;
        dur = kContinuous;
        return true;
    }
    if (contains(code, keymap[4])) {         // Stop
        m = Motion::Stop;
        speed = 0;
        dur = kStopDur;
        return true;
    }
    return false;
}
} // namespace

IrRemote::IrRemote(Scheduler& sched) : sched_(sched) {}

IrRemote::~IrRemote() {
    stop();
}

void IrRemote::start() {
    if (running_) return;

    // Initialize LIRC and open the client socket
    if (lirc_init("ircontrol", 1) == -1) {
        throw std::runtime_error("lirc_init failed (is lircd running and permissions correct?)");
    }

    // Load LIRC config (default locations like /etc/lirc/lircrc or ~/.lircrc)
    if (lirc_readconfig(nullptr, &config_, nullptr) != 0) {
        lirc_deinit();
        config_ = nullptr;
        throw std::runtime_error("lirc_readconfig failed");
    }

    last_motion_ = Motion::Stop;
    last_ts_ = std::chrono::steady_clock::now();

    // Run the blocking read loop in a dedicated thread (does not block Scheduler)
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

    // Failsafe stop on shutdown
    sched_.replaceNow({Motion::Stop, 0, kStopDur});
}

void IrRemote::loop() {
    // lirc_nextcode() blocks; keeping it in this thread preserves scheduler responsiveness
    while (running_) {
        char* code = nullptr;

        if (lirc_nextcode(&code) != 0) {
            // If lircd disconnects or an error occurs, exit cleanly
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
    Motion m;
    int speed;
    std::chrono::milliseconds dur;

    // Ignore unrecognized keys
    if (!decodeMotion(code, m, speed, dur)) return;

    const auto now = std::chrono::steady_clock::now();
    const bool same_motion = (m == last_motion_);

    // Debounce repeated identical codes; always allow motion changes immediately.
    if ((now - last_ts_) < kDebounce) {
        if (same_motion) return;
    }
    last_ts_ = now;
    last_motion_ = m;

    // Preempt immediately: new command replaces the currently running command.
    sched_.replaceNow({m, speed, dur});
}