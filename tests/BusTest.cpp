#include "dds/LocalDdsBus.hpp"
#include "dds/VehicleTopics.hpp"
#include <iostream>
#include <chrono>
#include <cassert>

int main() {
    LocalDdsBus bus;
    bool received = false;
    long long latencyUs = 0;

    auto sub = bus.subscribe<MotionCommandTopic>([&](const MotionCommandTopic& topic) {
        auto end = std::chrono::high_resolution_clock::now();
        received = true;
        std::cout << "[Latency] Received motion command from: " << topic.source << std::endl;
    });

    auto start = std::chrono::high_resolution_clock::now();

    bus.publish(MotionCommandTopic{Motion::Up, 50, std::chrono::milliseconds(100), "unit_test"});

    auto end = std::chrono::high_resolution_clock::now();
    latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    assert(received == true);

    std::cout << "[Quantitative Assessment] Bus delivery latency: " << latencyUs << " us" << std::endl;
    std::cout << "BusTest passed successfully!" << std::endl;

    return 0;
}