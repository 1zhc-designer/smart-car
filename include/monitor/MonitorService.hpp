#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

struct gpiod_chip;
struct gpiod_line_request;

/**
 * @brief Realtime monitor service for temperature, light sensing and alarm output.
 *
 * Design goals for A1-level realtime behaviour:
 * - event driven userspace design based on epoll + timerfd + eventfd
 * - no active waiting in the realtime path
 * - sensor sampling and alarm timing are driven by kernel timers
 * - GPIO and I2C stay inside the worker thread to avoid shared-device races
 * - UI callbacks are dispatched from a separate thread so slow GUI code cannot
 *   block sensor sampling or alarm timing
 * - public state is encapsulated behind safe getters / setters / callbacks
 */
class MonitorService {
public:
    /** @brief Callback used for consolidated temperature/status updates. */
    using StatusCallback = std::function<void(double, const std::string&)>;

    /** @brief Callback invoked whenever a new photoresistor sample is available. */
    using LightLevelCallback = std::function<void(int level)>;

    /** @brief Callback invoked when the derived light/dark state changes. */
    using LightStateCallback = std::function<void(int level, bool dark)>;

    MonitorService() = default;
    ~MonitorService();

    MonitorService(const MonitorService&) = delete;
    MonitorService& operator=(const MonitorService&) = delete;

    /** @brief Start the worker and callback dispatch threads. */
    void start();

    /** @brief Stop the service and release all OS / device resources. */
    void stop();

    /** @brief Return true when the service is currently active. */
    bool isRunning() const noexcept { return running_.load(std::memory_order_acquire); }

    /** @brief Get the latest valid temperature published to clients. */
    double currentTemperature() const;

    /** @brief Get the current low alarm limit in degree Celsius. */
    int lowLimit() const noexcept;

    /** @brief Get the current high alarm limit in degree Celsius. */
    int highLimit() const noexcept;

    /** @brief Get the latest textual monitor status. */
    std::string currentStatus() const;

    /**
     * @brief Update alarm limits.
     *
     * Invalid updates are ignored to keep the service in a safe state.
     */
    void setLimits(int low, int high);

    /** @brief Register or replace the status callback. */
    void setStatusCallback(StatusCallback cb);

    /** @brief Return the latest sampled photoresistor raw value. */
    int currentLightLevel() const;

    /** @brief Return whether the latest sampled light level is considered dark. */
    bool isDark() const;

    /** @brief Return the raw ADC threshold used to classify dark/light state. */
    int lightThreshold() const noexcept;

    /**
     * @brief Set the light/dark threshold used by isDark().
     *
     * Values are clamped to the valid PCF8591 range 0..255.
     */
    void setLightThreshold(int threshold) noexcept;

    /** @brief Register or replace the raw light-level callback. */
    void setLightLevelCallback(LightLevelCallback cb);

    /** @brief Register or replace the light state change callback. */
    void setLightStateCallback(LightStateCallback cb);

private:
    static constexpr int kLightSensorChannel = 0;
    static constexpr int kTempSensorChannel = 3;

    /**
     * @brief Periodic sampling interval.
     *
     * 100 ms keeps UI and monitoring responsive while giving generous timing
     * headroom for simple I2C transactions on Raspberry Pi Linux userspace.
     */
    static constexpr int kSamplePeriodMs = 100;

    /** @brief Gap after a completed alarm burst before the next decision cycle. */
    static constexpr int kAlarmRecoveryGapMs = 100;

    /** @brief Maximum number of queued callback notifications before oldest drop. */
    static constexpr std::size_t kMaxQueuedNotifications = 64;

    struct SharedState {
        double currentTemperature{0.0};
        double lastValidTemperature{0.0};
        bool hasValidTemperature{false};
        int currentLightLevel{0};
        bool currentIsDark{false};
        std::string currentStatus{"Initializing"};
        StatusCallback statusCallback;
        LightLevelCallback lightLevelCallback;
        LightStateCallback lightStateCallback;
    };

    enum class NotifyType {
        Status,
        LightLevel,
        LightState,
    };

    struct Notification {
        NotifyType type;
        double temperature{0.0};
        std::string status;
        int lightLevel{0};
        bool dark{false};
    };

    enum class AlarmMode {
        None,
        TooCold,
        TooHot,
    };

    enum class AlarmPhase {
        Idle,
        On,
        Off,
        RecoveryGap,
    };

    void runLoop();
    void callbackLoop();

    void ensureGpioReady();
    void ensureI2cReady();
    void ensureEventLoopReady();
    void closeResources() noexcept;

    void processSampleEvent();
    void processAlarmTimerEvent();

    std::uint8_t readPcf8591Channel(int channel);
    bool tryReadNtcTemperature(double& temp) noexcept;
    int readLightLevel() noexcept;

    void setLedRed(bool on) noexcept;
    void setLedGreen(bool on) noexcept;
    void setBuzzer(bool on) noexcept;
    void writePhysicalLevel(int offset, bool high) noexcept;

    void applyNormalOutputs() noexcept;
    void applySensorInvalidOutputs() noexcept;
    void startAlarmBurst(AlarmMode mode) noexcept;
    void armAlarmTimerOnce(int ms);
    void armSampleTimerPeriodic(int periodMs);

    void updateStateAndQueueStatus(double temp, std::string status);
    void updateStateAndQueueLight(int level);
    void queueNotification(Notification notification);

    static int clampToByte(int value) noexcept;

    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> started_{false};

    std::thread worker_;
    std::thread callbackThread_;

    std::atomic<int> lowLimit_{16};
    std::atomic<int> highLimit_{30};
    std::atomic<int> lightThreshold_{128};

    mutable std::mutex stateMutex_;
    SharedState state_;

    std::mutex callbackMutex_;
    std::condition_variable callbackCv_;
    std::deque<Notification> callbackQueue_;

    AlarmMode alarmMode_{AlarmMode::None};
    AlarmPhase alarmPhase_{AlarmPhase::Idle};
    int beepMs_{0};
    int beepsRemaining_{0};

    int stopFd_{-1};
    int epollFd_{-1};
    int sampleTimerFd_{-1};
    int alarmTimerFd_{-1};
    int i2cFd_{-1};

    gpiod_chip* chip_{nullptr};
    gpiod_line_request* gpioRequest_{nullptr};
};
