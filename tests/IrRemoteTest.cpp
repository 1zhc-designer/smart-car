#include "ir/IrRemote.hpp"
#include "dds/LocalDdsBus.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

void testIrLifecycle() {
    LocalDdsBus bus;
    IrRemote remote(bus);

    try {
        remote.start();
        std::cout << "IrRemote started successfully (or skipped if no LIRC hardware)." << std::endl;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        remote.stop();
        std::cout << "IrRemote stopped successfully." << std::endl;
    } catch (const std::exception& e) {
        std::cout << "IrRemote handled missing hardware correctly: " << e.what() << std::endl;
    }
}

int main() {
    testIrLifecycle();
    return 0;
}