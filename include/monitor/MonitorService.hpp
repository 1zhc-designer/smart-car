#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class MonitorService {
public:
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

private:
    void runLoop();

private:
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> running_{false};
    std::thread worker_;

    std::atomic<int> lowLimit_{16};
    std::atomic<int> highLimit_{30};

    mutable std::mutex stateMutex_;
    double currentTemperature_{0.0};
    std::string currentStatus_{"Initializing"};

    int stopFd_{-1};
};