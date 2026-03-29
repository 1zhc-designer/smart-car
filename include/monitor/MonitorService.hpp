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
 * GPIO outputs use libgpiod.
 * PCF8591 ADC is read directly through /dev/i2c-1.
 *
 * GUI is the only path allowed to update temperature limits.
 * Joystick-based threshold adjustment is intentionally disabled.
 */
class MonitorService {
public:
    using StatusCallback = std::function<void(double, const std::string&)>;

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

private:
    void runLoop();
    void ensureGpioReady();
    void ensureI2cReady();
    void closeResources();

    unsigned char readPcf8591Channel(int channel);
    bool tryReadNtcTemperature(double& temp);

    void writePhysicalLevel(int offset, bool high);
    void setLedRed(bool on);
    void setLedGreen(bool on);
    void setBuzzer(bool on);

    void updateUiState(double temp, const std::string& status);

    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> running_{false};
    std::thread worker_;

    std::atomic<int> lowLimit_{16};
    std::atomic<int> highLimit_{30};

    mutable std::mutex stateMutex_;
    double currentTemperature_{0.0};
    double lastValidTemperature_{0.0};
    bool hasValidTemperature_{false};
    std::string currentStatus_{"Initializing"};
    StatusCallback statusCallback_;

    int stopFd_{-1};
    int epollFd_{-1};
    int timerFd_{-1};
    int i2cFd_{-1};

    gpiod_chip* chip_{nullptr};
    gpiod_line_request* gpioRequest_{nullptr};
};