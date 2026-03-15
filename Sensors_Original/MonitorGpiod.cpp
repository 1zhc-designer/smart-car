/**
 * @file MonitorGpiod.cpp
 * @brief Integrated Temperature Monitoring System.
 * * Hardware Requirements:
 * - Raspberry Pi (libgpiod v2 compatible)
 * - PCF8591 AD/DA Converter
 * - NTC Thermistor (Channel 3)
 * - Joystick (X: Channel 1, Y: Channel 0)
 * - Active Buzzer (BCM 17)
 * - Dual-color LED (BCM 20, 21)
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <functional>
#include <atomic>
#include <chrono>
#include <string>
#include <memory> // Required for std::unique_ptr

// Linux specific headers for I2C and GPIO v2
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <unistd.h>
#include <gpiod.hpp>

/** @brief PCF8591 I2C hardware address */
#define PCF8591_ADDR 0x48

/** @brief BCM GPIO Pin Definitions */
enum Pins {
    LED_RED = 20,
    LED_GRN = 21,
    BUZZER  = 17
};

/**
 * @class Controller
 * @brief Manages hardware resources using libgpiod v2 and I2C.
 */
class Controller {
private:
    int i2c_fd;
    ::gpiod::chip gpio_chip;
    /** * @brief Using unique_ptr to wrap line_request because it lacks a 
     * default constructor in libgpiod v2.
     */
    std::unique_ptr<::gpiod::line_request> out_req; 
    std::atomic<bool> keep_running{true};

public:
    /**
     * @brief Constructor utilizing Initializer List and Smart Pointers.
     * @param chip_path Path to the gpiochip (e.g., "/dev/gpiochip4").
     */
    explicit Controller(const std::string& chip_path = "/dev/gpiochip4") 
        : gpio_chip(chip_path) 
    {
        // 1. Initialize I2C Communication
        i2c_fd = open("/dev/i2c-1", O_RDWR);
        if (i2c_fd < 0 || ioctl(i2c_fd, I2C_SLAVE, PCF8591_ADDR) < 0) {
            throw std::runtime_error("Failed to initialize I2C bus or PCF8591");
        }

        // 2. Configure GPIO Output Lines
        auto request = gpio_chip.prepare_request()
            .set_consumer("monitor_app")
            .add_line_settings(LED_RED, ::gpiod::line_settings()
                .set_direction(::gpiod::line::direction::OUTPUT))
            .add_line_settings(LED_GRN, ::gpiod::line_settings()
                .set_direction(::gpiod::line::direction::OUTPUT))
            .add_line_settings(BUZZER, ::gpiod::line_settings()
                .set_direction(::gpiod::line::direction::OUTPUT))
            .do_request();
        
        // Move the request into the unique_ptr to manage its lifetime
        out_req = std::make_unique<::gpiod::line_request>(std::move(request));
    }

    /**
     * @brief Cleanup hardware resources.
     */
    ~Controller() {
        keep_running = false;
        if (i2c_fd >= 0) close(i2c_fd);
    }

    /**
     * @brief Reads 8-bit value from the specified ADC channel.
     */
    uint8_t read_adc(uint8_t chan) {
        uint8_t ctrl = 0x40 | (chan & 0x03);
        if (write(i2c_fd, &ctrl, 1) != 1) return 0;
        
        uint8_t data;
        read(i2c_fd, &data, 1); // Discard previous conversion result
        if (read(i2c_fd, &data, 1) != 1) return 0;
        return data;
    }

    /**
     * @brief Sets LED states.
     */
    void update_leds(bool r, bool g) {
        out_req->set_value(LED_RED, r ? ::gpiod::line::value::ACTIVE : ::gpiod::line::value::INACTIVE);
        out_req->set_value(LED_GRN, g ? ::gpiod::line::value::ACTIVE : ::gpiod::line::value::INACTIVE);
    }

    /**
     * @brief Triggers buzzer with blocking sleep for precise duration.
     */
    void alert_beep(int ms) {
        out_req->set_value(BUZZER, ::gpiod::line::value::ACTIVE);
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        out_req->set_value(BUZZER, ::gpiod::line::value::INACTIVE);
    }

    /**
     * @brief Background thread for Joystick input (Blocking IO simulation).
     * @param on_input Callback function for direction events.
     */
    void run_input_listener(std::function<void(int)> on_input) {
        std::thread([this, on_input]() {
            while (keep_running) {
                uint8_t x = read_adc(1);
                uint8_t y = read_adc(0);

                if (x >= 250) on_input(2);      // Right
                else if (x <= 5) on_input(1);   // Left
                else if (y >= 250) on_input(4); // Down
                else if (y <= 5) on_input(3);   // Up

                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }
        }).detach();
    }
};

/**
 * @brief Steinhart-Hart simplified calculation for NTC.
 */
double calculate_temp(uint8_t raw) {
    if (raw == 0) return 0.0;
    const double B = 3950.0;
    const double T0 = 298.15;
    const double R0 = 10000.0;
    
    double v = static_cast<double>(raw) * 5.0 / 255.0;
    if (v >= 4.95) return 99.0;
    
    double rt = 10000.0 * v / (5.0 - v);
    return (1.0 / (std::log(rt / R0) / B + (1.0 / T0))) - 273.15;
}

int main() {
    try {
        Controller hw("/dev/gpiochip4");
        std::atomic<int> low_limit{26};
        std::atomic<int> high_limit{30};

        std::cout << "--- System Initialized (libgpiod v2) ---" << std::endl;

        // Register Callback for Threshold Adjustments
        hw.run_input_listener([&](int direction) {
            if (direction == 1) low_limit--;
            if (direction == 2) low_limit++;
            if (direction == 3) high_limit++;
            if (direction == 4) high_limit--;
            
            std::cout << "[Config] Limits: " << low_limit << "C to " << high_limit << "C" << std::endl;
        });

        while (true) {
            double current_temp = calculate_temp(hw.read_adc(3));
            printf("Current Temp: %.2f C | Range: [%d - %d]\n", current_temp, (int)low_limit, (int)high_limit);

            if (current_temp < low_limit) {
                hw.update_leds(true, true); // Yellow
                hw.alert_beep(300);
            } else if (current_temp >= high_limit) {
                hw.update_leds(true, false); // Red
                hw.alert_beep(100);
            } else {
                hw.update_leds(false, true); // Green
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}