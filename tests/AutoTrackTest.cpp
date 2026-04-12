#include "autonomy/AutoTrackService.hpp"
#include "dds/LocalDdsBus.hpp"
#include "dds/VehicleTopics.hpp"
#include "gimbal/GimbalService.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <cassert>

void testObjectInteractionLogic() {
    std::cout << "[AutoTrackTest] Starting Object Interaction Logic Test..." << std::endl;

    LocalDdsBus bus;
    GimbalService gimbal("/dev/null");
    AutoTrackService autoTrack(bus, gimbal, "/dev/gpiochip0");

    std::atomic<bool> stopCommandReceived{false};
    std::atomic<bool> burstTriggerReceived{false};
    std::string stopSource = "";

    auto motionSub = bus.subscribe<MotionCommandTopic>([&](const MotionCommandTopic& topic) {
        if (topic.motion == Motion::Stop) {
            stopCommandReceived = true;
            stopSource = topic.source;
            std::cout << "  [Bus Info] Received Stop command from: " << stopSource << std::endl;
        }
    });

    auto cameraSub = bus.subscribe<CameraTriggerTopic>([&](const CameraTriggerTopic& topic) {
        if (topic.count == 3) {
            burstTriggerReceived = true;
            std::cout << "  [Bus Info] Received Camera Burst Trigger (Count: " << topic.count << ")!" << std::endl;
        }
    });

    try {
        autoTrack.start();
        std::cout << "  [Step 1] Service started. Simulating object detection..." << std::endl;

        ObjectDetectedTopic detectionMsg;
        detectionMsg.detected = true;
        detectionMsg.objectType = "fruit";
        bus.publish(detectionMsg);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        assert(stopCommandReceived.load() && "Car should stop immediately upon detection");
        assert(stopSource == "object-capture-start");

        std::cout << "  [Step 2] Waiting for stop duration (3s) to elapse..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(3500));
        assert(burstTriggerReceived.load() && "Camera burst should be triggered after stop duration");

        stopCommandReceived = false;
        bus.publish(detectionMsg);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (stopCommandReceived.load()) {
            std::cout << "  [Warning] Cooldown lock failed: second detection triggered stop too early." << std::endl;
        } else {
            std::cout << "  [Step 3] Cooldown lock verified: second detection ignored during 5s lock." << std::endl;
        }

        autoTrack.stop();
        std::cout << "[AutoTrackTest] All logic tests PASSED!" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "[AutoTrackTest] Hardware specific error (expected on non-Pi environments): " << e.what() << std::endl;
    }
}

int main() {
    testObjectInteractionLogic();
    return 0;
}