#include <cstdint>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <iostream>

#include <wiringPi.h>
#include <softPwm.h>

// ========================== IMotorDriver ==========================
class IMotorDriver {
public:
    virtual ~IMotorDriver() = default;

    // speed: 0..100
    virtual void setLeft(int speed, bool forward) = 0;
    virtual void setRight(int speed, bool forward) = 0;
    virtual void stopAll() = 0;
};

// ====================== WiringPiMotorDriver =======================
class WiringPiMotorDriver final : public IMotorDriver {
public:
    struct Pins {
        int PWMA, AIN1, AIN2;
        int PWMB, BIN1, BIN2;
    };

    explicit WiringPiMotorDriver(Pins pins, int pwmRange = 100)
        : pins_(pins), pwmRange_(pwmRange)
    {
        if (wiringPiSetup() == -1) {
            throw std::runtime_error("wiringPiSetup failed");
        }

        pinMode(pins_.PWMA, OUTPUT);
        pinMode(pins_.AIN1, OUTPUT);
        pinMode(pins_.AIN2, OUTPUT);

        pinMode(pins_.PWMB, OUTPUT);
        pinMode(pins_.BIN1, OUTPUT);
        pinMode(pins_.BIN2, OUTPUT);

        if (softPwmCreate(pins_.PWMA, 0, pwmRange_) != 0) {
            throw std::runtime_error("softPwmCreate PWMA failed");
        }
        if (softPwmCreate(pins_.PWMB, 0, pwmRange_) != 0) {
            throw std::runtime_error("softPwmCreate PWMB failed");
        }

        stopAll();
    }

    ~WiringPiMotorDriver() override {
        stopAll();
    }

    void setLeft(int speed, bool forward) override {
        setOneSide_(pins_.PWMA, pins_.AIN1, pins_.AIN2, speed, forward);
    }

    void setRight(int speed, bool forward) override {
        setOneSide_(pins_.PWMB, pins_.BIN1, pins_.BIN2, speed, forward);
    }

    void stopAll() override {
        digitalWrite(pins_.AIN1, LOW);
        digitalWrite(pins_.AIN2, LOW);
        softPwmWrite(pins_.PWMA, 0);

        digitalWrite(pins_.BIN1, LOW);
        digitalWrite(pins_.BIN2, LOW);
        softPwmWrite(pins_.PWMB, 0);
    }

    WiringPiMotorDriver(const WiringPiMotorDriver&) = delete;
    WiringPiMotorDriver& operator=(const WiringPiMotorDriver&) = delete;

private:
    Pins pins_;
    int pwmRange_;

    static int clampSpeed_(int s) {
        return std::clamp(s, 0, 100);
    }

    void setOneSide_(int pwmPin, int in1, int in2, int speed, bool forward) {
        speed = clampSpeed_(speed);
        digitalWrite(in1, forward ? HIGH : LOW);
        digitalWrite(in2, forward ? LOW : HIGH);
        softPwmWrite(pwmPin, speed);
    }
};

// ========================= LineFollowerDO3 =========================
class LineFollowerDO3 {
public:
    struct Params {
        // 木质地板循白线：低速稳态保守参数
        int baseSpeed      = 28;
        int maxSpeed       = 50;
        int minSpeed       = 0;

        int softDelta      = 6;
        int hardDelta      = 14;
        int junctionSpeed  = 22;
        int pivotSpeed     = 24;

        int filterWindow   = 7;

        int lostHoldMs     = 220;
        int lostPivotMs    = 1200;
        int lostStopMs     = 3500;

        int reacquireMs    = 180;
    };

    // activeLow:
    // true  -> 检测到目标线时 DO = LOW
    // false -> 检测到目标线时 DO = HIGH
    //
    // 这里“目标线”已经是白线
    explicit LineFollowerDO3(IMotorDriver& driver, bool activeLow = true)
    : driver_(driver), activeLow_(activeLow), p_()
{
    normalizeParams_();
    resetFilter_();
}

LineFollowerDO3(IMotorDriver& driver, bool activeLow, const Params& params)
    : driver_(driver), activeLow_(activeLow), p_(params)
{
    normalizeParams_();
    resetFilter_();
}

    void begin() {
        // 传感器：BCM 13/19/26 -> wiringPi 23/24/25
        pinMode(kPinL, INPUT);
        pinMode(kPinC, INPUT);
        pinMode(kPinR, INPUT);

        resetFilter_();
        lastSeenMs_ = millis();
        lastBias_ = 0;
        driver_.stopAll();
    }

    void update() {
        sampleAndFilter_();

        const int L = filtL_;
        const int C = filtC_;
        const int R = filtR_;
        const int code = (L << 2) | (C << 1) | R; // LCR

        const unsigned now = millis();
        const bool any = (code != 0);
        const bool all = (code == 0b111);

        if (any) {
            lastSeenMs_ = now;
            if (code == 0b100 || code == 0b110) {
                lastBias_ = -1; // 白线偏左
            } else if (code == 0b001 || code == 0b011) {
                lastBias_ = +1; // 白线偏右
            }
        }

        // 111: 大面积都检测到白色，减速直行
        if (all) {
            drive_(p_.junctionSpeed, p_.junctionSpeed);
            return;
        }

        // 000: 丢线
        if (!any) {
            handleLost_(now);
            return;
        }

        const bool reacquire = (now - lastSeenMs_ <= static_cast<unsigned>(p_.reacquireMs));
        const int base = p_.baseSpeed;
        const int soft = reacquire ? std::max(1, p_.softDelta / 2) : p_.softDelta;
        const int hard = reacquire ? std::max(2, p_.hardDelta / 2) : p_.hardDelta;

        int leftSpeed = base;
        int rightSpeed = base;

        switch (code) {
            case 0b010: // 中间在白线上
                leftSpeed = base;
                rightSpeed = base;
                break;

            case 0b110: // 左+中检测到白线 -> 向左微调
                leftSpeed = base - soft;
                rightSpeed = base + soft;
                break;

            case 0b011: // 中+右检测到白线 -> 向右微调
                leftSpeed = base + soft;
                rightSpeed = base - soft;
                break;

            case 0b100: // 仅左检测到白线 -> 强向左纠偏
                leftSpeed = base - hard;
                rightSpeed = base + hard;
                break;

            case 0b001: // 仅右检测到白线 -> 强向右纠偏
                leftSpeed = base + hard;
                rightSpeed = base - hard;
                break;

            case 0b101: // 左右检测到白线，中间未检测到
                if (lastBias_ < 0) {
                    leftSpeed = base - soft;
                    rightSpeed = base + soft;
                } else if (lastBias_ > 0) {
                    leftSpeed = base + soft;
                    rightSpeed = base - soft;
                } else {
                    leftSpeed = base;
                    rightSpeed = base;
                }
                break;

            default:
                leftSpeed = base;
                rightSpeed = base;
                break;
        }

        drive_(leftSpeed, rightSpeed);
    }

    void stop() {
        driver_.stopAll();
    }

private:
    IMotorDriver& driver_;
    bool activeLow_;
    Params p_;

    // 传感器 wiringPi 编号
    static constexpr int kPinL = 23; // BCM13
    static constexpr int kPinC = 24; // BCM19
    static constexpr int kPinR = 25; // BCM26

    // 多数表决滤波
    static constexpr int kMaxWin = 9;
    int win_ = 7;
    int idx_ = 0;
    int cnt_ = 0;
    std::array<uint8_t, kMaxWin> bufL_{};
    std::array<uint8_t, kMaxWin> bufC_{};
    std::array<uint8_t, kMaxWin> bufR_{};

    int filtL_ = 0;
    int filtC_ = 0;
    int filtR_ = 0;

    // 状态
    int lastBias_ = 0;        // -1 左，+1 右
    unsigned lastSeenMs_ = 0;

    void normalizeParams_() {
        p_.baseSpeed     = std::clamp(p_.baseSpeed, 0, 100);
        p_.maxSpeed      = std::clamp(p_.maxSpeed, 0, 100);
        p_.minSpeed      = std::clamp(p_.minSpeed, 0, 100);
        p_.junctionSpeed = std::clamp(p_.junctionSpeed, 0, 100);
        p_.pivotSpeed    = std::clamp(p_.pivotSpeed, 0, 100);
        p_.softDelta     = std::clamp(p_.softDelta, 0, 100);
        p_.hardDelta     = std::clamp(p_.hardDelta, 0, 100);

        if (p_.minSpeed > p_.maxSpeed) {
            std::swap(p_.minSpeed, p_.maxSpeed);
        }

        p_.filterWindow = std::clamp(p_.filterWindow, 3, kMaxWin);
        if ((p_.filterWindow % 2) == 0) {
            p_.filterWindow += 1;
        }
        win_ = p_.filterWindow;

        p_.lostHoldMs  = std::max(0, p_.lostHoldMs);
        p_.lostPivotMs = std::max(p_.lostHoldMs, p_.lostPivotMs);
        p_.lostStopMs  = std::max(p_.lostPivotMs, p_.lostStopMs);
        p_.reacquireMs = std::max(0, p_.reacquireMs);
    }

    int readOnLine_(int pin) const {
        const int value = digitalRead(pin);
        const bool onLine = activeLow_ ? (value == LOW) : (value == HIGH);
        return onLine ? 1 : 0;
    }

    void resetFilter_() {
        idx_ = 0;
        cnt_ = 0;
        bufL_.fill(0);
        bufC_.fill(0);
        bufR_.fill(0);
        filtL_ = 0;
        filtC_ = 0;
        filtR_ = 0;
    }

    void sampleAndFilter_() {
        bufL_[idx_] = static_cast<uint8_t>(readOnLine_(kPinL));
        bufC_[idx_] = static_cast<uint8_t>(readOnLine_(kPinC));
        bufR_[idx_] = static_cast<uint8_t>(readOnLine_(kPinR));

        idx_ = (idx_ + 1) % win_;
        if (cnt_ < win_) {
            ++cnt_;
        }

        int sumL = 0, sumC = 0, sumR = 0;
        for (int i = 0; i < cnt_; ++i) {
            sumL += bufL_[i];
            sumC += bufC_[i];
            sumR += bufR_[i];
        }

        const int half = (cnt_ / 2) + 1;
        filtL_ = (sumL >= half) ? 1 : 0;
        filtC_ = (sumC >= half) ? 1 : 0;
        filtR_ = (sumR >= half) ? 1 : 0;
    }

    void handleLost_(unsigned now) {
        const unsigned lostFor = now - lastSeenMs_;

        if (lostFor <= static_cast<unsigned>(p_.lostHoldMs)) {
            if (lastBias_ >= 0) {
                drive_(p_.pivotSpeed, std::max(p_.pivotSpeed - 10, 0));
            } else {
                drive_(std::max(p_.pivotSpeed - 10, 0), p_.pivotSpeed);
            }
            return;
        }

        if (lostFor <= static_cast<unsigned>(p_.lostPivotMs)) {
            if (lastBias_ >= 0) {
                drive_(p_.pivotSpeed, 0);
            } else {
                drive_(0, p_.pivotSpeed);
            }
            return;
        }

        driver_.stopAll();
    }

    void drive_(int leftSpeed, int rightSpeed) {
        leftSpeed = std::clamp(leftSpeed, p_.minSpeed, p_.maxSpeed);
        rightSpeed = std::clamp(rightSpeed, p_.minSpeed, p_.maxSpeed);

        leftSpeed = std::clamp(leftSpeed, 0, 100);
        rightSpeed = std::clamp(rightSpeed, 0, 100);

        driver_.setLeft(leftSpeed, true);
        driver_.setRight(rightSpeed, true);
    }
};

// ============================== main ==============================
int main() {
    try {
        // 电机：
        // 左：BCM 18/27/22 -> wPi 1/2/3
        // 右：BCM 23/24/25 -> wPi 4/5/6
        WiringPiMotorDriver::Pins motorPins{
            1, 2, 3,
            4, 5, 6
        };

        WiringPiMotorDriver motor(motorPins);

        // 木质地板循白线：
        // 默认按“白线 DO = LOW”处理，所以 activeLow = true
        // 如果你实测白线时 DO = HIGH，把 true 改成 false
        LineFollowerDO3 follower(motor, true);
        follower.begin();

        std::cout << "White line follower started. Press Ctrl+C to exit.\n";

        while (true) {
            follower.update();
            delay(12);
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}