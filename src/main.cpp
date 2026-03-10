#include "motor/WiringPiMotorDriver.hpp"
#include "motion/MotionController.hpp"
#include "rt/Scheduler.hpp"
#include "ir/IrRemote.hpp"
#include "monitor/MonitorService.hpp"
#include "monitor/CameraService.hpp"
#include "gimbal/GimbalService.hpp"

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

        GimbalService gimbal;
        gimbal.init();

        sched.start();

        IrRemote remote(sched, gimbal);
        remote.start();

        MonitorService monitor;
        monitor.start();

        CameraService camera(0, "./captures");
        camera.start();

        std::cout << "IR motion + IR gimbal + Temperature monitor + Camera monitor enabled.\n";
        std::cout << "Press Enter to exit the whole program...\n";
        std::cin.get();

        camera.stop();
        monitor.stop();
        remote.stop();
        sched.stop();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
}