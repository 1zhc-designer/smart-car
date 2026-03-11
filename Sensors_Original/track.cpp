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
        int baseSpeed;
        int maxSpeed;
        int minSpeed;

        int softDelta;
        int hardDelta;
        int junctionSpeed;
        int pivotSpeed;

        int filterWindow;

        int lostSearchMs;
        int reacquireMs;

        Params()
            : baseSpeed(22),
              maxSpeed(40),
              minSpeed(0),
              softDelta(5),
              hardDelta(12),
              junctionSpeed(18),
              pivotSpeed(20),
              filterWindow(7),
              lostSearchMs(3000),
              reacquireMs(220) {}
    };

    // activeLow:
    // true  -> 白线时 DO = LOW
    // false -> 白线时 DO = HIGH
    explicit LineFollowerDO3(IMotorDriver& driver, bool activeLow = false)
        : driver_(driver), activeLow_(activeLow), p_(),
          win_(7), idx_(0), cnt_(0),
          filtL_(0), filtC_(0), filtR_(0),
          lastBias_(0), lastSeenMs_(0),
          buzzerActiveHigh_(true),
          obstacleActiveLow_(true),
          buzzerState_(false),
          lastBeepToggleMs_(0)
    {
        normalizeParams_();
        resetFilter_();
    }

    LineFollowerDO3(IMotorDriver& driver, bool activeLow, const Params& params)
        : driver_(driver), activeLow_(activeLow), p_(params),
          win_(7), idx_(0), cnt_(0),
          filtL_(0), filtC_(0), filtR_(0),
          lastBias_(0), lastSeenMs_(0),
          buzzerActiveHigh_(true),
          obstacleActiveLow_(true),
          buzzerState_(false),
          lastBeepToggleMs_(0)
    {
        normalizeParams_();
        resetFilter_();
    }

    void begin() {
        // 循迹：BCM 13/19/26 -> wiringPi 23/24/25
        pinMode(kPinL, INPUT);
        pinMode(kPinC, INPUT);
        pinMode(kPinR, INPUT);

        // 避障：BCM 16/12 -> wiringPi 27/26
        pinMode(kObstacleLeftPin, INPUT);
        pinMode(kObstacleRightPin, INPUT);

        // 蜂鸣器：BCM 17 -> wiringPi 0
        pinMode(kBuzzerPin, OUTPUT);
        setBuzzer_(false);

        resetFilter_();
        lastSeenMs_ = millis();
        lastBias_ = 0;
        lastBeepToggleMs_ = millis();

        driver_.stopAll();
    }

    void update() {
        const unsigned now = millis();

        // ---------- 避障优先 ----------
        if (hasObstacle_()) {
            driver_.stopAll();
            beepAlert_(now);

            std::cout << "[OBSTACLE] "
                      << "Left=" << obstacleDetected_(kObstacleLeftPin)
                      << " Right=" << obstacleDetected_(kObstacleRightPin)
                      << std::endl;
            return;
        } else {
            setBuzzer_(false);
        }

        // ---------- 循迹采样 ----------
        sampleAndFilter_();

        const int L = filtL_;
        const int C = filtC_;
        const int R = filtR_;
        const int code = (L << 2) | (C << 1) | R;

        // ---------- 调试输出 ----------
        std::cout << "L=" << L
                  << " C=" << C
                  << " R=" << R
                  << " code=" << code
                  << std::endl;

        const bool any = (code != 0);
        const bool all = (code == 0b111);

        if (any) {
            lastSeenMs_ = now;
            if (code == 0b100 || code == 0b110) {
                lastBias_ = -1;
            } else if (code == 0b001 || code == 0b011) {
                lastBias_ = +1;
            }
        }

        // 三路都检测到白线：减速直行
        if (all) {
            drive_(p_.junctionSpeed, p_.junctionSpeed);
            return;
        }

        // 丢线
        if (!any) {
            handleLost_(now);
            return;
        }

        const bool reacquire =
            (now - lastSeenMs_ <= static_cast<unsigned>(p_.reacquireMs));

        const int base = p_.baseSpeed;
        const int soft = reacquire ? std::max(1, p_.softDelta / 2) : p_.softDelta;
        const int hard = reacquire ? std::max(2, p_.hardDelta / 2) : p_.hardDelta;

        int leftSpeed = base;
        int rightSpeed = base;

        switch (code) {
            case 0b010: // 中间在线
                leftSpeed = base;
                rightSpeed = base;
                break;

            case 0b110: // 左+中
                leftSpeed = base - soft;
                rightSpeed = base + soft;
                break;

            case 0b011: // 中+右
                leftSpeed = base + soft;
                rightSpeed = base - soft;
                break;

            case 0b100: // 仅左
                leftSpeed = base - hard;
                rightSpeed = base + hard;
                break;

            case 0b001: // 仅右
                leftSpeed = base + hard;
                rightSpeed = base - hard;
                break;

            case 0b101: // 左右在线，中间不在线
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
        setBuzzer_(false);
        driver_.stopAll();
    }

private:
    IMotorDriver& driver_;
    bool activeLow_;
    Params p_;

    // 循迹：BCM13/19/26 -> wPi 23/24/25
    static const int kPinL = 23;
    static const int kPinC = 24;
    static const int kPinR = 25;

    // 蜂鸣器/避障：BCM17/16/12 -> wPi 0/27/26
    static const int kBuzzerPin = 0;
    static const int kObstacleLeftPin = 27;
    static const int kObstacleRightPin = 26;

    enum { kMaxWin = 9 };

    int win_;
    int idx_;
    int cnt_;
    std::array<uint8_t, kMaxWin> bufL_;
    std::array<uint8_t, kMaxWin> bufC_;
    std::array<uint8_t, kMaxWin> bufR_;

    int filtL_;
    int filtC_;
    int filtR_;

    int lastBias_;
    unsigned lastSeenMs_;

    bool buzzerActiveHigh_;
    bool obstacleActiveLow_;
    bool buzzerState_;
    unsigned lastBeepToggleMs_;

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

        p_.filterWindow = std::clamp(
            p_.filterWindow, 3, static_cast<int>(kMaxWin)
        );
        if ((p_.filterWindow % 2) == 0) {
            p_.filterWindow += 1;
        }
        win_ = p_.filterWindow;

        p_.lostSearchMs = std::max(0, p_.lostSearchMs);
        p_.reacquireMs  = std::max(0, p_.reacquireMs);
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

        int sumL = 0;
        int sumC = 0;
        int sumR = 0;

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

    bool obstacleDetected_(int pin) const {
        const int value = digitalRead(pin);
        return obstacleActiveLow_ ? (value == LOW) : (value == HIGH);
    }

    bool hasObstacle_() const {
        return obstacleDetected_(kObstacleLeftPin) ||
               obstacleDetected_(kObstacleRightPin);
    }

    void setBuzzer_(bool on) {
        buzzerState_ = on;
        if (buzzerActiveHigh_) {
            digitalWrite(kBuzzerPin, on ? HIGH : LOW);
        } else {
            digitalWrite(kBuzzerPin, on ? LOW : HIGH);
        }
    }

    void beepAlert_(unsigned now) {
        if (now - lastBeepToggleMs_ >= 150) {
            lastBeepToggleMs_ = now;
            setBuzzer_(!buzzerState_);
        }
    }

    void handleLost_(unsigned now) {
        const unsigned lostFor = now - lastSeenMs_;

        if (lostFor <= static_cast<unsigned>(p_.lostSearchMs)) {
            if (lastBias_ >= 0) {
                drive_(p_.pivotSpeed, std::max(p_.pivotSpeed - 8, 0));
            } else {
                drive_(std::max(p_.pivotSpeed - 8, 0), p_.pivotSpeed);
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

        // 你的车当前接线下，false 才是前进
        driver_.setLeft(leftSpeed, false);
        driver_.setRight(rightSpeed, false);
    }
};

// ============================== main ==============================
int main() {
    try {
        // 电机：BCM 18/27/22 -> wPi 1/2/3
        //      BCM 23/24/25 -> wPi 4/5/6
        WiringPiMotorDriver::Pins motorPins = {
            1, 2, 3,
            4, 5, 6
        };

        WiringPiMotorDriver motor(motorPins);

        // 当前默认按“白线 DO = HIGH”处理，所以用 false
        // 若你实测白线时 DO = LOW，请改成 true
        LineFollowerDO3 follower(motor, false);
        follower.begin();

        std::cout << "White line follower + IR obstacle avoidance started." << std::endl;
        std::cout << "Debug output enabled. Lost line: search 3 seconds, then stop." << std::endl;

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