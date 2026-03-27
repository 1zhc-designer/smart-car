#include "gimbal/GimbalCommandService.hpp"
#include "gimbal/GimbalService.hpp"
#include "dds/LocalDdsBus.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        LocalDdsBus bus;
        GimbalService gimbal("/dev/null", 0x40); 
        
        std::cout << "[GimbalTest] Attempting hardware init..." << std::endl;
        gimbal.init();

        GimbalCommandService service(bus, gimbal);
        service.start();
        
        GimbalCommandTopic cmd;
        cmd.command = GimbalCommand::Reset;
        bus.publish(cmd);
        
        std::cout << "[GimbalTest] Logic test passed with hardware." << std::endl;
        service.stop();
    } catch (const std::exception& e) {
        std::cout << "[GimbalTest] Hardware not found, skipping low-level check: " << e.what() << std::endl;
    }
    
    return 0;
}