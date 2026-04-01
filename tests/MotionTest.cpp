#include "motion/MotionController.hpp"
#include "MockMotorDriver.hpp"
#include <cassert>
#include <iostream>

void testForwardMovement() {
    MockMotorDriver mock;
    MotionController controller(mock);

    controller.apply(Motion::Up, 50);

    assert(mock.lastCall.leftSpeed == 50);
    assert(mock.lastCall.leftForward == true);
    assert(mock.lastCall.rightSpeed == 50);
    assert(mock.lastCall.rightForward == true);
    
    std::cout << "testForwardMovement passed!" << std::endl;
}

int main() {
    testForwardMovement();
    return 0;
}