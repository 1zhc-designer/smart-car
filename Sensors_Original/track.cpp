#include <gpiod.hpp>

#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

// ============================================================================
// 1. Motor Driver Interface and Implementation
// ============================================================================

class IMotorDriver {
public:
    virtual ~IMotorDriver() = default;
    virtual void setLeft(int speed, bool forward) = 0;
    virtual void setRight(int speed, bool forward) = 0;
    virtual void stopAll() = 0;
};

class MotorDriverGpiod final : public IMotorDriver {
public:
    struct Pins {
        unsigned leftPwm;
        unsigned leftIn1;
        unsigned leftIn2;
        unsigned rightPwm;
        unsigned rightIn1;
        unsigned rightIn2;
    };

    explicit MotorDriverGpiod(const std::filesystem::path& chipPath,
                              Pins pins,
                              int pwmPeriodUs = 2000)
        : chip_(chipPath),
          pins_(pins),
          pwmPeriodUs_(pwmPeriodUs),
          dutyLeft_(0),
          dutyRight_(0),
          phaseUs_(0) {
        requestLines_();
        stopAll();
    }

    ~MotorDriverGpiod() override {
        stopAll();
    }

    void setLeft(int speed, bool forward) override {
        speed = clamp_(speed);
        dutyLeft_ = speed;
        setDirection_(true, forward);
    }

    void setRight(int speed, bool forward) override {
        speed = clamp_(speed);
        dutyRight_ = speed;
        setDirection_(false, forward);
    }

    void stopAll() override {
        dutyLeft_ = 0;
        dutyRight_ = 0;
        request_.set_value(pins_.leftIn1, gpiod::line::value::INACTIVE);
        request_.set_value(pins_.leftIn2, gpiod::line::value::INACTIVE);
        request_.set_value(pins_.rightIn1, gpiod::line::value::INACTIVE);
        request_.set_value(pins_.rightIn2, gpiod::line::value::INACTIVE);
        request_.set_value(pins_.leftPwm, gpiod::line::value::INACTIVE);
        request_.set_value(pins_.rightPwm, gpiod::line::value::INACTIVE);
    }

    void tickPwm(int tickUs) {
        phaseUs_ += tickUs;
        if (phaseUs_ >= pwmPeriodUs_) {
            phaseUs_ %= pwmPeriodUs_;
        }

        const int thresholdLeft = dutyLeft_ * pwmPeriodUs_ / 100;
        const int thresholdRight = dutyRight_ * pwmPeriodUs_ / 100;

        request_.set_value(
            pins_.leftPwm,
            phaseUs_ < thresholdLeft ? gpiod::line::value::ACTIVE
                                     : gpiod::line::value::INACTIVE);

        request_.set_value(
            pins_.rightPwm,
            phaseUs_ < thresholdRight ? gpiod::line::value::ACTIVE
                                      : gpiod::line::value::INACTIVE);
    }

private:
    gpiod::chip chip_;
    Pins pins_;
    int pwmPeriodUs_;
    int dutyLeft_;
    int dutyRight_;
    int phaseUs_;
    gpiod::line_request request_;

    static int clamp_(int v) {
        return v < 0 ? 0 : (v > 100 ? 100 : v);
    }

    void requestLines_() {
        gpiod::line_settings out;
        out.set_direction(gpiod::line::direction::OUTPUT)
           .set_output_value(gpiod::line::value::INACTIVE);

        auto builder = chip_.prepare_request();
        builder.set_consumer("line-follow-car")
               .add_line_settings(
                   {pins_.leftPwm, pins_.leftIn1, pins_.leftIn2,
                    pins_.rightPwm, pins_.rightIn1, pins_.rightIn2},
                   out);

        request_ = builder.do_request();
    }

    void setDirection_(bool left, bool forward) {
        const auto active = gpiod::line::value::ACTIVE;
        const auto inactive = gpiod::line::value::INACTIVE;

        const unsigned in1 = left ? pins_.leftIn1 : pins_.rightIn1;
        const unsigned in2 = left ? pins_.leftIn2 : pins_.rightIn2;

        request_.set_value(in1, forward ? active : inactive);
        request_.set_value(in2, forward ? inactive : active);
    }
};

// ============================================================================
// 2. Line Follower Definitions
// ============================================================================

struct LineState {
    bool left{};
    bool center{};
    bool right{};
};

struct ObstacleState {
    bool left{};
    bool right{};
    bool any() const { return left || right; }
};

class LineFollowerGpiod {
public:
    using LineCallback = std::function<void(const LineState&)>;
    using ObstacleCallback = std::function<void(const ObstacleState&)>;
    using StopCallback = std::function<void()>;

    struct Config {
        std::string chipPath = "/dev/gpiochip0";

        unsigned lineLeft = 13;
        unsigned lineCenter = 19;
        unsigned lineRight = 26;

        unsigned obstacleLeft = 16;
        unsigned obstacleRight = 12;

        unsigned buzzer = 17;

        bool whiteLineActiveHigh = true;
        bool obstacleActiveLow = true;
        bool buzzerActiveHigh = true;

        int baseSpeed = 22;
        int softDelta = 5;
        int hardDelta = 12;

        std::chrono::microseconds debounce{3000};
        std::chrono::microseconds pwmTick{500};
    };

    LineFollowerGpiod(IMotorDriver& motor, Config cfg);
    ~LineFollowerGpiod();

    void start();
    void stop();

    void setLineCallback(LineCallback cb);
    void setObstacleCallback(ObstacleCallback cb);
    void setStopCallback(StopCallback cb);

private:
    IMotorDriver& motor_;
    Config cfg_;

    gpiod::chip chip_;
    gpiod::line_request lineReq_;
    gpiod::line_request obstacleReq_;
    gpiod::line_request buzzerReq_;

    std::thread lineThread_;
    std::thread obstacleThread_;
    std::thread pwmThread_;
    std::thread buzzerThread_;

    std::atomic<bool> running_{false};
    std::atomic<bool> obstacleActive_{false};
    std::atomic<bool> buzzerAlarm_{false};

    std::mutex stateMutex_;
    LineState lineState_{};
    ObstacleState obstacleState_{};
    int lastBias_ = 0;

    LineCallback lineCb_;
    ObstacleCallback obstacleCb_;
    StopCallback stopCb_;

    void requestInputs_();
    void requestBuzzer_();

    void lineLoop_();
    void obstacleLoop_();
    void pwmLoop_();
    void buzzerLoop_();

    void refreshLineState_();
    void refreshObstacleState_();

    bool lineOn_(gpiod::line::value v) const;
    bool obstacleOn_(gpiod::line::value v) const;

    void handleLineControl_();
    void handleObstacleControl_();

    void setBuzzer_(bool on);
    void hardStop_();
};

// ============================================================================
// 3. Line Follower Implementation
// ============================================================================

static gpiod::line_settings make_input_settings(bool activeLow,
                                                std::chrono::microseconds debounce) {
    gpiod::line_settings s;
    s.set_direction(gpiod::line::direction::INPUT)
     .set_edge_detection(gpiod::line::edge::BOTH)
     .set_active_low(activeLow)
     .set_debounce_period(debounce);
    return s;
}

LineFollowerGpiod::LineFollowerGpiod(IMotorDriver& motor, Config cfg)
    : motor_(motor), cfg_(std::move(cfg)), chip_(cfg_.chipPath) {
    requestInputs_();
    requestBuzzer_();
    setBuzzer_(false);
    refreshLineState_();
    refreshObstacleState_();
}

LineFollowerGpiod::~LineFollowerGpiod() {
    stop();
}

void LineFollowerGpiod::start() {
    if (running_.exchange(true)) return;

    lineThread_ = std::thread(&LineFollowerGpiod::lineLoop_, this);
    obstacleThread_ = std::thread(&LineFollowerGpiod::obstacleLoop_, this);
    pwmThread_ = std::thread(&LineFollowerGpiod::pwmLoop_, this);
    buzzerThread_ = std::thread(&LineFollowerGpiod::buzzerLoop_, this);
}

void LineFollowerGpiod::stop() {
    if (!running_.exchange(false)) return;

    try { lineReq_.release(); } catch (...) {}
    try { obstacleReq_.release(); } catch (...) {}
    try { buzzerReq_.release(); } catch (...) {}

    if (lineThread_.joinable()) lineThread_.join();
    if (obstacleThread_.joinable()) obstacleThread_.join();
    if (pwmThread_.joinable()) pwmThread_.join();
    if (buzzerThread_.joinable()) buzzerThread_.join();

    hardStop_();
}

void LineFollowerGpiod::setLineCallback(LineCallback cb) { lineCb_ = std::move(cb); }
void LineFollowerGpiod::setObstacleCallback(ObstacleCallback cb) { obstacleCb_ = std::move(cb); }
void LineFollowerGpiod::setStopCallback(StopCallback cb) { stopCb_ = std::move(cb); }

void LineFollowerGpiod::requestInputs_() {
    auto lineSettings = make_input_settings(false, cfg_.debounce);
    auto obsSettings = make_input_settings(false, cfg_.debounce);

    auto lineBuilder = chip_.prepare_request();
    lineBuilder.set_consumer("line-sensors")
               .add_line_settings({cfg_.lineLeft, cfg_.lineCenter, cfg_.lineRight},
                                  lineSettings);
    lineReq_ = lineBuilder.do_request();

    auto obsBuilder = chip_.prepare_request();
    obsBuilder.set_consumer("obstacle-sensors")
              .add_line_settings({cfg_.obstacleLeft, cfg_.obstacleRight}, obsSettings);
    obstacleReq_ = obsBuilder.do_request();
}

void LineFollowerGpiod::requestBuzzer_() {
    gpiod::line_settings out;
    out.set_direction(gpiod::line::direction::OUTPUT)
       .set_output_value(gpiod::line::value::INACTIVE);

    auto builder = chip_.prepare_request();
    builder.set_consumer("buzzer")
           .add_line_settings({cfg_.buzzer}, out);

    buzzerReq_ = builder.do_request();
}

bool LineFollowerGpiod::lineOn_(gpiod::line::value v) const {
    const bool active = (v == gpiod::line::value::ACTIVE);
    return cfg_.whiteLineActiveHigh ? active : !active;
}

bool LineFollowerGpiod::obstacleOn_(gpiod::line::value v) const {
    const bool active = (v == gpiod::line::value::ACTIVE);
    return cfg_.obstacleActiveLow ? !active : active;
}

void LineFollowerGpiod::refreshLineState_() {
    auto values = lineReq_.get_values({cfg_.lineLeft, cfg_.lineCenter, cfg_.lineRight});
    std::lock_guard<std::mutex> lock(stateMutex_);
    lineState_.left = lineOn_(values[0]);
    lineState_.center = lineOn_(values[1]);
    lineState_.right = lineOn_(values[2]);
}

void LineFollowerGpiod::refreshObstacleState_() {
    auto values = obstacleReq_.get_values({cfg_.obstacleLeft, cfg_.obstacleRight});
    std::lock_guard<std::mutex> lock(stateMutex_);
    obstacleState_.left = obstacleOn_(values[0]);
    obstacleState_.right = obstacleOn_(values[1]);
    obstacleActive_.store(obstacleState_.any(), std::memory_order_relaxed);
}

void LineFollowerGpiod::lineLoop_() {
    gpiod::edge_event_buffer buffer(16);
    while (running_.load()) {
        try {
            const bool ready = lineReq_.wait_edge_events(std::chrono::nanoseconds{-1});
            if (!running_.load()) break;
            if (!ready) continue;

            (void)lineReq_.read_edge_events(buffer);
            refreshLineState_();
            handleLineControl_();

            if (lineCb_) {
                std::lock_guard<std::mutex> lock(stateMutex_);
                lineCb_(lineState_);
            }
        } catch (...) {
            break;
        }
    }
}

void LineFollowerGpiod::obstacleLoop_() {
    gpiod::edge_event_buffer buffer(8);
    while (running_.load()) {
        try {
            const bool ready = obstacleReq_.wait_edge_events(std::chrono::nanoseconds{-1});
            if (!running_.load()) break;
            if (!ready) continue;

            (void)obstacleReq_.read_edge_events(buffer);
            refreshObstacleState_();
            handleObstacleControl_();

            if (obstacleCb_) {
                std::lock_guard<std::mutex> lock(stateMutex_);
                obstacleCb_(obstacleState_);
            }
        } catch (...) {
            break;
        }
    }
}

void LineFollowerGpiod::pwmLoop_() {
    auto* pwm = dynamic_cast<MotorDriverGpiod*>(&motor_);
    if (!pwm) {
        while (running_.load()) std::this_thread::sleep_for(cfg_.pwmTick);
        return;
    }

    while (running_.load()) {
        pwm->tickPwm(static_cast<int>(cfg_.pwmTick.count()));
        std::this_thread::sleep_for(cfg_.pwmTick);
    }
}

void LineFollowerGpiod::buzzerLoop_() {
    bool state = false;
    while (running_.load()) {
        if (!buzzerAlarm_.load(std::memory_order_relaxed)) {
            setBuzzer_(false);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }

        state = !state;
        setBuzzer_(state);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
    setBuzzer_(false);
}

void LineFollowerGpiod::setBuzzer_(bool on) {
    const bool physicalOn = cfg_.buzzerActiveHigh ? on : !on;
    buzzerReq_.set_value(cfg_.buzzer,
                         physicalOn ? gpiod::line::value::ACTIVE
                                    : gpiod::line::value::INACTIVE);
}

void LineFollowerGpiod::hardStop_() {
    buzzerAlarm_.store(false, std::memory_order_relaxed);
    motor_.stopAll();
    if (stopCb_) stopCb_();
}

void LineFollowerGpiod::handleObstacleControl_() {
    if (obstacleActive_.load(std::memory_order_relaxed)) {
        buzzerAlarm_.store(true, std::memory_order_relaxed);
        hardStop_();
    } else {
        buzzerAlarm_.store(false, std::memory_order_relaxed);
    }
}

void LineFollowerGpiod::handleLineControl_() {
    if (obstacleActive_.load(std::memory_order_relaxed)) return;

    LineState s;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        s = lineState_;
    }

    const int code = (s.left ? 4 : 0) | (s.center ? 2 : 0) | (s.right ? 1 : 0);

    if (code != 0) {
        if (code == 0b100 || code == 0b110) lastBias_ = -1;
        if (code == 0b001 || code == 0b011) lastBias_ = +1;
    }

    int left = cfg_.baseSpeed;
    int right = cfg_.baseSpeed;

    switch (code) {
        case 0b010: break;
        case 0b110: left -= cfg_.softDelta; right += cfg_.softDelta; break;
        case 0b011: left += cfg_.softDelta; right -= cfg_.softDelta; break;
        case 0b100: left -= cfg_.hardDelta; right += cfg_.hardDelta; break;
        case 0b001: left += cfg_.hardDelta; right -= cfg_.hardDelta; break;
        case 0b101: 
            if (lastBias_ < 0) { left -= cfg_.softDelta; right += cfg_.softDelta; }
            else if (lastBias_ > 0) { left += cfg_.softDelta; right -= cfg_.softDelta; }
            break;
        case 0b111: left = cfg_.baseSpeed / 2; right = cfg_.baseSpeed / 2; break;
        case 0b000: 
            // 只要三个传感器都没检测到线，立刻切断动力并停止逻辑
            hardStop_();
            return; 
        default: break;
    }

    left = std::clamp(left, 0, 100);
    right = std::clamp(right, 0, 100);

    motor_.setLeft(left, false);
    motor_.setRight(right, false);
}

// ============================================================================
// 4. Main Entry Point
// ============================================================================

namespace {
std::unique_ptr<LineFollowerGpiod> g_controller;
std::unique_ptr<MotorDriverGpiod> g_motor;

void onSignal(int) {
    if (g_controller) g_controller->stop();
    if (g_motor) g_motor->stopAll();
    std::_Exit(0);
}
}

int main() {
    try {
        std::signal(SIGINT, onSignal);
        std::signal(SIGTERM, onSignal);

        MotorDriverGpiod::Pins motorPins{
            .leftPwm = 18,
            .leftIn1 = 27,
            .leftIn2 = 22,
            .rightPwm = 23,
            .rightIn1 = 24,
            .rightIn2 = 25
        };

        g_motor = std::make_unique<MotorDriverGpiod>("/dev/gpiochip0", motorPins);

        LineFollowerGpiod::Config cfg;
        cfg.chipPath = "/dev/gpiochip0";
        cfg.lineLeft = 13;
        cfg.lineCenter = 19;
        cfg.lineRight = 26;
        cfg.obstacleLeft = 16;
        cfg.obstacleRight = 12;
        cfg.buzzer = 17;

        cfg.whiteLineActiveHigh = true;   
        cfg.obstacleActiveLow = true;     
        cfg.buzzerActiveHigh = true;      

        g_controller = std::make_unique<LineFollowerGpiod>(*g_motor, cfg);

        g_controller->setLineCallback([](const LineState& s) {
            std::cout << "LINE  L=" << s.left
                      << " C=" << s.center
                      << " R=" << s.right << '\n';
        });

        g_controller->setObstacleCallback([](const ObstacleState& s) {
            std::cout << "OBS   L=" << s.left
                      << " R=" << s.right
                      << " any=" << s.any() << '\n';
        });

        g_controller->setStopCallback([]() {
            std::cout << "STOP issued\n";
        });

        g_controller->start();

        std::cout << "libgpiod v2 line follower started. Ctrl+C to exit.\n";

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n';
        return 1;
    }
}