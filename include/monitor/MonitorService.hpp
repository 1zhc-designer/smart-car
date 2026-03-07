#pragma once

#include <atomic>
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

private:
    void runLoop();

private:
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> running_{false};
    std::thread worker_;
};