#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

struct gpiod_chip;
struct gpiod_line_request;

/**
 * @brief Temperature and alarm monitor using i2c-dev and libgpiod v2.
 *
 * GPIO outputs use libgpiod. PCF8591 ADC is read directly through
 * @c /dev/i2c-1.
 *
 * The monitor keeps the original temperature/alarm behavior from the main
 * branch and additionally samples the photoresistor on PCF8591 AIN0 so the GUI
 * can display the current light intensity.
 *
 * GUI is the only path allowed to update temperature limits. Joystick-based
 * threshold adjustment is intentionally disabled.
 */
class MonitorService {
public:
    /**
     * @brief Callback used for consolidated temperature/status updates.
     */
    using StatusCallback = std::function<void(double, const std::string&)>;

    /**
     * @brief Callback invoked whenever a new photoresistor sample is available.
     *
     * @param level Latest raw PCF8591 AIN0 value in the range 0..255.
     */
    using LightLevelCallback = std::function<void(int level)>;

    /**
     * @brief Callback invoked when the derived light/dark state changes.
     *
     * @param level Latest raw PCF8591 AIN0 value.
     * @param dark  True when @p level is less than or equal to the configured
     *              light threshold.
     */
    using LightStateCallback = std::function<void(int level, bool dark)>;

    MonitorService() = default;
    ~MonitorService();

    MonitorService(const MonitorService&) = delete;
    MonitorService& operator=(const MonitorService&) = delete;

    void start();
    void stop();
    bool isRunning() const noexcept { return running_.load(); }

    double currentTemperature() const;
    int lowLimit() const noexcept;
    int highLimit() const noexcept;
    std::string currentStatus() const;

    void setLimits(int low, int high);
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
    void runLoop();
    void ensureGpioReady();
    void ensureI2cReady();
    void closeResources();

    unsigned char readPcf8591Channel(int channel);
    bool tryReadNtcTemperature(double& temp);
    int readLightLevel();

    void writePhysicalLevel(int offset, bool high);
    void setLedRed(bool on);
    void setLedGreen(bool on);
    void setBuzzer(bool on);

    void updateUiState(double temp, const std::string& status);
    void publishLightSample(int level);

    static constexpr int kLightSensorChannel = 0;

    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> running_{false};
    std::thread worker_;

    std::atomic<int> lowLimit_{16};
    std::atomic<int> highLimit_{30};
    std::atomic<int> lightThreshold_{128};

    mutable std::mutex stateMutex_;
    double currentTemperature_{0.0};
    double lastValidTemperature_{0.0};
    bool hasValidTemperature_{false};
    int currentLightLevel_{0};
    bool currentIsDark_{false};
    std::string currentStatus_{"Initializing"};
    StatusCallback statusCallback_;
    LightLevelCallback lightLevelCallback_;
    LightStateCallback lightStateCallback_;

    int stopFd_{-1};
    int epollFd_{-1};
    int timerFd_{-1};
    int i2cFd_{-1};

    gpiod_chip* chip_{nullptr};
    gpiod_line_request* gpioRequest_{nullptr};
};
