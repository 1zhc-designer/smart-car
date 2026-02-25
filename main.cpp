#include "motor/WiringPiMotorDriver.hpp"
#include "motion/MotionController.hpp"
#include "rt/Scheduler.hpp"

// sensors
#include "sensors/DHT11.hpp"
#include "sensors/DS18B20.hpp"
#include "sensors/RainSensor.hpp"

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

int main() {
    try {
        // wiringPi 初始化（建议在 main 显式初始化一次）
        if (wiringPiSetup() == -1) {
            std::cerr << "Fatal: wiringPiSetup failed\n";
            return 1;
        }

        // ===== Motor + Motion + Scheduler =====
        WiringPiMotorDriver::Pins pins{
            .PWMA = 1, .AIN1 = 3, .AIN2 = 2,
            .PWMB = 4, .BIN1 = 6, .BIN2 = 5
        };

        WiringPiMotorDriver driver(pins, 100);
        MotionController motion(driver);
        Scheduler sched(motion);

        // ===== Sensors =====
        // DHT11：wiringPi pin 0（如果你原先 DHT11 用的就是 0）
        DHT11 dht11(0);

        // DS18B20：默认自动发现第一个 28- 开头设备
        DS18B20 ds18b20;

        // 雨滴：PCF8591 base=120 addr=0x48 AIN0，DO pin=0（按你图里示例）
        // 注意：如果你的 DO 和 DHT11 共用 pin 0 会冲突！建议把雨滴 DO 换一个 wiringPi pin。
        // 这里我按“示例默认”写，实际项目请确保不冲突。
        RainSensor rain(120, 0x48, 0, 0);
        if (!rain.init()) {
            std::cerr << "Warning: RainSensor init failed (I2C/PCF8591 not ready?)\n";
        }

        // ===== Start scheduler =====
        sched.start();

        // enqueue tasks (event-driven)
        sched.enqueue({Motion::Up,    50, std::chrono::milliseconds(2000)});
        sched.enqueue({Motion::Down,  50, std::chrono::milliseconds(2000)});
        sched.enqueue({Motion::Left,  50, std::chrono::milliseconds(2000)});
        sched.enqueue({Motion::Right, 50, std::chrono::milliseconds(2000)});
        sched.enqueue({Motion::Stop,   0, std::chrono::milliseconds(2000)});

        // ===== Sensor polling thread =====
        std::atomic<bool> running{true};
        std::thread sensorThread([&]() {
            int lastRainDigital = -1;

            while (running.load()) {
                // --- DS18B20 ---
                if (auto t = ds18b20.readTemperatureC(); t) {
                    std::cout << "[DS18B20] Temp: " << *t << " °C\n";
                } else {
                    std::cout << "[DS18B20] Read failed\n";
                }

                // --- DHT11 ---
                if (dht11.read()) {
                    std::cout << "[DHT11] Humidity: "
                              << dht11.getHumidityInt() << "." << dht11.getHumidityDec() << " %  "
                              << "Temp: "
                              << dht11.getTempInt() << "." << dht11.getTempDec() << " °C ("
                              << dht11.getTempF() << " °F)\n";
                } else {
                    std::cout << "[DHT11] Read failed\n";
                }

                // --- Rain sensor ---
                if (auto r = rain.read(); r) {
                    std::cout << "[Rain] Analog(AIN" << rain.analogChannel() << "): "
                              << r->analog << "  DO: " << r->digital << "\n";

                    // 状态变化才提示
                    if (r->digital != lastRainDigital) {
                        std::cout << (r->raining ? "[Rain] Raining!!\n" : "[Rain] Not Raining\n");
                        lastRainDigital = r->digital;
                    }
                } else {
                    std::cout << "[Rain] Read skipped/failed\n";
                }

                std::cout << "-----------------------------\n";
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });

        std::cout << "Tasks submitted. Press Enter to exit...\n";
        std::cin.get();

        // shutdown
        running = false;
        if (sensorThread.joinable()) sensorThread.join();

        sched.stop();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}