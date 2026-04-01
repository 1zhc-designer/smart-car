#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

/**
 * @brief Temperature, light and alarm monitor service.
 *
 * The service reads PCF8591 ADC channels through @c /dev/i2c-1 and drives LEDs
 * and buzzer through libgpiod v2. The worker thread blocks in @c epoll_wait()
 * and is woken by blocking I/O sources such as @c timerfd and @c eventfd.
 *
 * Existing temperature/status APIs are preserved. Photoresistor related support
 * is added through light-level sampling and optional C++ callbacks.
 */
class MonitorService {
public:
    /**
     * @brief Callback used for consolidated status updates.
     *
     * The callback receives the latest temperature and a status string, for
     * example @c NORMAL|L=123.
     */
    using StatusCallback = std::function<void(double, const std::string&)>;

    /**
     * @brief Callback invoked when a new light level sample is available.
     *
     * @param level Latest raw PCF8591 ADC value in the range 0..255.
     */
    using LightLevelCallback = std::function<void(int level)>;

    /**
     * @brief Callback invoked when the light/dark state changes.
     *
     * @param level Latest raw PCF8591 ADC value.
     * @param dark  True when the raw value is less than or equal to the current
     *              light threshold.
     */
    using LightStateCallback = std::function<void(int level, bool dark)>;

    MonitorService() = default;
    ~MonitorService();

    MonitorService(const MonitorService&) = delete;
    MonitorService& operator=(const MonitorService&) = delete;

    /** @brief Start the worker thread. Safe to call repeatedly. */
    void start();

    /** @brief Stop the worker thread and release resources. Safe to call repeatedly. */
    void stop();

    /** @brief Return whether the worker thread is currently active. */
    bool isRunning() const noexcept { return running_.load(); }

    /** @brief Return the latest sampled NTC temperature in Celsius. */
    double currentTemperature() const;

    /** @brief Return the currently configured low temperature threshold. */
    int lowLimit() const noexcept;

    /** @brief Return the currently configured high temperature threshold. */
    int highLimit() const noexcept;

    /** @brief Return the latest composed status string. */
    std::string currentStatus() const;

    /**
     * @brief Update the allowed temperature range.
     *
     * Invalid ranges where @p low is not less than @p high are ignored.
     */
    void setLimits(int low, int high);

    /** @brief Register or replace the consolidated status callback. */
    void setStatusCallback(StatusCallback cb);

    /** @brief Return the latest sampled photoresistor raw value. */
    int currentLightLevel() const;

    /** @brief Return whether the latest light sample is considered dark. */
    bool isDark() const;

    /** @brief Return the raw ADC threshold used to classify dark/light state. */
    int lightThreshold() const noexcept;

    /**
     * @brief Set the photoresistor threshold.
     *
     * Values are clamped to the PCF8591 raw range 0..255.
     */
    void setLightThreshold(int threshold) noexcept;

    /** @brief Register or replace the raw light-level callback. */
    void setLightLevelCallback(LightLevelCallback cb);

    /** @brief Register or replace the light state change callback. */
    void setLightStateCallback(LightStateCallback cb);

private:
    /** @brief Main epoll-based worker loop. */
    void runLoop();

    /** @brief Create or validate the GPIO request used for outputs. */
    void ensureGpioReady();

    /** @brief Open and configure the I2C device used by the PCF8591. */
    void ensureI2cReady();

    /** @brief Create epoll, timerfd and eventfd instances and register them. */
    void ensurePollerReady();

    /** @brief Release all owned file descriptors and GPIO resources. */
    void closeResources();

    /** @brief Read one PCF8591 ADC channel. */
    unsigned char readPcf8591Channel(int channel);

    /** @brief Read the joystick ADC channel if present. */
    unsigned char readJoystick();

    /** @brief Convert the NTC ADC channel reading into Celsius. */
    double readNtcTemperature();

    /** @brief Read and store the current photoresistor ADC value. */
    int readLightLevel();

    /** @brief Drive a GPIO output line to the requested active state. */
    void writeGpioValue(int offset, bool active);

    /** @brief Update cached state and invoke the consolidated callback. */
    void updateUiState(double temp, const std::string& status);

    /** @brief Publish light level and light/dark state callbacks. */
    void publishLightSample(int level);

    static constexpr int kLightSensorChannel = 0;
    static constexpr int kNtcChannel = 1;
    static constexpr int kJoystickChannel = 2;

    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> running_{false};
    std::thread worker_;

    std::atomic<int> lowLimit_{16};
    std::atomic<int> highLimit_{30};
    std::atomic<int> lightThreshold_{128};

    mutable std::mutex stateMutex_;
    double currentTemperature_{0.0};
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
    struct gpiod_chip* chip_{nullptr};
    struct gpiod_line_request* gpioRequest_{nullptr};
};
