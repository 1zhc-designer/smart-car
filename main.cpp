#include "motor/WiringPiMotorDriver.hpp"
#include "motion/MotionController.hpp"
#include "rt/Scheduler.hpp"
#include <iostream>

int main() {
    try {
        WiringPiMotorDriver::Pins pins {
            .PWMA = 1, .AIN1 = 3, .AIN2 = 2,
            .PWMB = 4, .BIN1 = 6, .BIN2 = 5
        };

        WiringPiMotorDriver driver(pins, 100);
        MotionController motion(driver);
        Scheduler sched(motion);

        sched.start();

        // enqueue tasks (event-driven)
        sched.enqueue({Motion::Up,    50, std::chrono::milliseconds(2000)});
        sched.enqueue({Motion::Down,  50, std::chrono::milliseconds(2000)});
        sched.enqueue({Motion::Left,  50, std::chrono::milliseconds(2000)});
        sched.enqueue({Motion::Right, 50, std::chrono::milliseconds(2000)});
        sched.enqueue({Motion::Stop,   0, std::chrono::milliseconds(2000)});

        std::cout << "Tasks submitted. Press Enter to exit...\n";
        std::cin.get();

        sched.stop();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}