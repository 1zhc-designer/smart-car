#include "monitor/MonitorService.hpp"
#include <cassert>
#include <iostream>
#include <string>

void testTemperatureLogic() {
    MonitorService monitor;

    monitor.setLimits(10, 30);
    assert(monitor.lowLimit() == 10);
    assert(monitor.highLimit() == 30);

    std::string status = monitor.currentStatus();
    std::cout << "Initial status: " << status << std::endl;
    
    std::cout << "testTemperatureLogic (Limits check) passed!" << std::endl;
}

int main() {
    testTemperatureLogic();
    return 0;
}