#include "motor/WiringPiMotorDriver.hpp"
#include "motion/MotionController.hpp"
#include "rt/Scheduler.hpp"
#include "ir/IrRemote.hpp"
#include <iostream>

int main() {
    try {
        WiringPiMotorDriver::Pins pins{
            .PWMA = 1, .AIN1 = 3, .AIN2 = 2,
            .PWMB = 4, .BIN1 = 6, .BIN2 = 5
        };

        WiringPiMotorDriver driver(pins, 100);
        MotionController motion(driver);
        Scheduler sched(motion);

        // Start the scheduler worker thread
        sched.start();

        // Start IR remote in its own thread (LIRC reads are blocking)
        IrRemote remote(sched);
        remote.start();

        std::cout << "IR control enabled. Press Enter to exit...\n";
        std::cin.get();

        remote.stop();
        sched.stop();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}