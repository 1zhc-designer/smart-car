#include "motor/WiringPiMotorDriver.hpp"
#include "motion/MotionController.hpp"
#include "rt/Scheduler.hpp"
#include "ir/IrRemote.hpp"
#include "monitor/MonitorService.hpp"
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

        // Start scheduler thread (unchanged behaviour)
        sched.start();

        // Start IR remote thread (unchanged behaviour)
        IrRemote remote(sched);
        remote.start();

        // Start monitor service in its own event-driven thread (no blocking main)
        MonitorService monitor;
        monitor.start();

        std::cout << "IR control + Temperature monitor enabled. Press Enter to exit...\n";
        std::cin.get();

        monitor.stop();   // graceful exit
        remote.stop();
        sched.stop();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}